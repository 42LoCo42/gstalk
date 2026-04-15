#pragma once

#define _DEFAULT_SOURCE
#include "util.c"

#include <stdint.h>

typedef struct {
	uint32_t id;
	uint32_t ix;
} PWPort;

Array(PWPort);

ArrayN(struct pw_proxy*, PWLinks);

typedef struct {
	uint32_t id;

	char*  name;
	size_t name_len;

	char*  desc;
	size_t desc_len;

	bool playing;
	bool ignore;

	PWPorts ports;
	PWLinks links;

	struct spa_hook* listener;
} PWNode;

Array(PWNode);

extern PWNodes pwNodes;

typedef struct {
	struct pw_thread_loop* thread_loop;
	struct pw_loop*        loop;
	struct pw_context*     context;
	struct pw_core*        core;
	struct pw_registry*    registry;

	struct {
		uint32_t id;
		PWPorts  ports;
	} nullSink;
} Data;

extern Data data;

void launch_pipewire(const char* ignore_pat);

void mkLink(PWNode* node);
void unLink(PWNode* node);

#if __INCLUDE_LEVEL__ == 0 /////////////////////////////////////////////////////

#include "menu.c"

#include <assert.h>
#include <pipewire/pipewire.h>
#include <regex.h>

#define printf(...)

bool launched = false;

pthread_barrier_t nullSinkBarrier = {0};

PWNodes pwNodes = {0};
Data    data    = {0};

static regex_t* ignore_rgx = NULL;

static void removeNode(uint32_t id) {
	ArrayLoop(pwNodes, {
		if(it->id == id) {
			spa_hook_remove(it->listener);

			free(it->name);
			free(it->desc);
			free(it->ports.ptr);
			free(it->listener);

			memmove(it, it + 1, sizeof(*it) * (--pwNodes.len - i));
			break;
		}
	});

	pthread_cond_signal(&redisplay);
}

static int nodeSorter(const void* a, const void* b) {
	return ((PWNode*) a)->id - ((PWNode*) b)->id;
}

static void on_bound_id(void*, uint32_t id) {
	data.nullSink.id = id;
	printf("sink %u\n", id);
}

static void on_node_info(void*, const struct pw_node_info* info) {
	ArrayFind(pwNodes, node, it->id == info->id);
	assert(node && "received node info for unknown node!");

	printf("info for node %u\n", node->id);
	const char* media_name = spa_dict_lookup(info->props, PW_KEY_MEDIA_NAME);
	if(media_name) {
		size_t detail_len = strlen(media_name);
		size_t desc_len = node->name_len + detail_len + 4; // 3 = " []" and null

		if(node->desc_len < desc_len) {
			node->desc = realloc(node->desc, desc_len);
		}

		node->desc_len = desc_len;
		snprintf(node->desc, desc_len, "%s [%s]", node->name, media_name);

		node->ignore =
			ignore_rgx && regexec(ignore_rgx, node->desc, 0, NULL, 0) == 0;
	}

	switch(info->state) {
	case PW_NODE_STATE_ERROR:
	case PW_NODE_STATE_CREATING:
	case PW_NODE_STATE_SUSPENDED:
	case PW_NODE_STATE_IDLE:
		node->playing = false;
		break;

	case PW_NODE_STATE_RUNNING:
		node->playing = true;
		break;
	}

	pthread_cond_signal(&redisplay);
}

static void on_registry_event(
	void*, uint32_t id, uint32_t, const char* type, uint32_t version,
	const struct spa_dict* props
) {
	const char* mediaClass = NULL;
	int         monitor    = 1;

	if(strcmp(type, PW_TYPE_INTERFACE_Node) == 0 && id != data.nullSink.id &&
	   (mediaClass = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS)) &&
	   ((monitor = strcmp(mediaClass, "Audio/Sink")) &
	    strcmp(mediaClass, "Audio/Source") &
	    strcmp(mediaClass, "Stream/Output/Audio")) == 0) {
		ArrayFind(pwNodes, node, it->id == id);
		assert(!node && "existing node re-added???");

		const char* name = spa_dict_lookup(props, PW_KEY_NODE_DESCRIPTION);
		if(!name) name = spa_dict_lookup(props, PW_KEY_NODE_NAME);
		assert(name && "node must have a name!");

		if(strstr(name, "gstalk") != NULL) return;

		PWNode new = {
			.id       = id,
			.listener = malloc(sizeof(*new.listener)),
		};

		int name_len = asprintf(
			&new.name, "%s%s", monitor == 0 ? "Monitor of " : "", name
		);
		assert(name_len != -1 && "buy more RAM");
		new.name_len = name_len;

		int desc_len = asprintf(&new.desc, "%s", new.name);
		assert(desc_len != -1 && "buy more RAM");
		new.desc_len = desc_len;

		new.ignore =
			ignore_rgx&& regexec(ignore_rgx, new.desc, 0, NULL, 0) == 0;

		printf("node %u\n", id);

		ArrayAdd(pwNodes, new);
		node = &ArrayLast(pwNodes);

		static struct pw_node_events node_events = {
			.version = PW_VERSION_NODE_EVENTS,
			.info    = on_node_info,
		};

		struct pw_node* nodeRef =
			pw_registry_bind(data.registry, id, type, version, 0);
		pw_node_add_listener(nodeRef, node->listener, &node_events, NULL);

		qsort(pwNodes.ptr, pwNodes.len, sizeof(pwNodes.ptr[0]), nodeSorter);
	} else if(strcmp(type, PW_TYPE_INTERFACE_Port) == 0) {
		const char* node_id_str = spa_dict_lookup(props, PW_KEY_NODE_ID);
		assert(node_id_str && "port must have a node ID!");

		uint32_t node_id = atoi(node_id_str);

		const char* direction = spa_dict_lookup(props, PW_KEY_PORT_DIRECTION);
		assert(direction && "port must have a direction!");

		const char* port_id = spa_dict_lookup(props, PW_KEY_PORT_ID);
		assert(port_id && "port must have a port.id!");

		PWPort port = {
			.id = id,
			.ix = atoi(port_id),
		};

		if(node_id == data.nullSink.id && strcmp(direction, "in") == 0) {
			ArrayAdd(data.nullSink.ports, port);
			printf("port sink %u.%u for %u\n", port.id, port.ix, node_id);
			pthread_barrier_wait(&nullSinkBarrier);
		} else if(strcmp(direction, "out") == 0) {
			ArrayFind(pwNodes, node, it->id == node_id);
			if(node) {
				ArrayAdd(node->ports, port);
				if(launched && autoadd && !node->ignore && node->ports.len == 2)
					mkLink(node);
				printf("port node %u.%u for %u\n", port.id, port.ix, node_id);
			}
		}
	}

	pthread_cond_signal(&redisplay);
}

static void on_registry_remove_event(void*, uint32_t id) {
	removeNode(id);
}

void launch_pipewire(const char* ignore_pat) {
	if(ignore_pat != NULL) {
		ignore_rgx = malloc(sizeof(*ignore_rgx));
		int err    = regcomp(ignore_rgx, ignore_pat, REG_EXTENDED | REG_NOSUB);
		if(err != 0) {
			size_t len = regerror(err, ignore_rgx, NULL, 0);
			char*  buf = malloc(len);
			regerror(err, ignore_rgx, buf, len);
			errx(1, "failed to parse ignore: %s", buf);
		}
	}

	pthread_barrier_init(&nullSinkBarrier, NULL, 2);

	pw_init(NULL, NULL);
	data.thread_loop = pw_thread_loop_new("pipewire", NULL);
	data.loop        = pw_thread_loop_get_loop(data.thread_loop);
	data.context     = pw_context_new(data.loop, NULL, 0);
	data.core        = pw_context_connect(data.context, NULL, 0);

	////////// setup null sink //////////

	struct pw_properties* sink_props = pw_properties_new(
		PW_KEY_NODE_NAME, "gstalk",                     //
		PW_KEY_NODE_DESCRIPTION, "gstalk",              //
		PW_KEY_FACTORY_NAME, "support.null-audio-sink", //
		NULL
	);

	struct pw_proxy* sink = pw_core_create_object(
		data.core, "adapter", PW_TYPE_INTERFACE_Node, PW_VERSION_NODE,
		&sink_props->dict, 0
	);

	pw_properties_free(sink_props);

	static struct spa_hook sink_listener = {0};

	static struct pw_proxy_events sink_events = {
		.version = PW_VERSION_PROXY_EVENTS,
		.bound   = on_bound_id,
	};

	pw_proxy_add_listener(sink, &sink_listener, &sink_events, NULL);

	////////// setup registry listener //////////

	data.registry = pw_core_get_registry(data.core, PW_VERSION_REGISTRY, 0);

	static struct spa_hook registry_listener = {0};

	static struct pw_registry_events registry_events = {
		.version       = PW_VERSION_REGISTRY_EVENTS,
		.global        = on_registry_event,
		.global_remove = on_registry_remove_event,
	};

	pw_registry_add_listener(
		data.registry, &registry_listener, &registry_events, NULL
	);

	pw_thread_loop_start(data.thread_loop);

	for(size_t i = 0; i < 2; i++) {
		pthread_barrier_wait(&nullSinkBarrier);
	}

	launched = true;
	printf("launched!\n");
}

void mkLink(PWNode* node) {
	pw_thread_loop_lock(data.thread_loop);

	ArrayLoopN(node->ports, src, {
		ArrayFind(data.nullSink.ports, dst, it->ix == src->ix);
		assert(dst && "sink must have a matching port");

		struct pw_properties* props = pw_properties_new_string("");
		pw_properties_setf(props, PW_KEY_LINK_OUTPUT_PORT, "%u", src->id);
		pw_properties_setf(props, PW_KEY_LINK_INPUT_PORT, "%u", dst->id);

		struct pw_proxy* link = pw_core_create_object(
			data.core, "link-factory", PW_TYPE_INTERFACE_Link, PW_VERSION_LINK,
			&props->dict, 0
		);

		pw_properties_free(props);
		ArrayAdd(node->links, link);
	});

	pw_thread_loop_unlock(data.thread_loop);
}

void unLink(PWNode* node) {
	pw_thread_loop_lock(data.thread_loop);

	ArrayLoop(node->links, { pw_core_destroy(data.core, *it); });
	ArrayFree(node->links);

	pw_thread_loop_unlock(data.thread_loop);
}

#endif
