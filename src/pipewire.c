#pragma once

#define _DEFAULT_SOURCE
#include "util.c"

#include <stdint.h>

typedef struct {
	uint32_t id;
	uint32_t ix;
} PWPort;

Array(PWPort);

typedef struct {
	struct pw_proxy* proxy;
	/* struct spa_hook* listener; */
} PWLink;

Array(PWLink);

typedef enum {
	NT_INVALID,
	NT_SOURCE,
	NT_SINK,
	NT_APP,
} NodeClass;

const char* ShowNodeClass(NodeClass type);

typedef struct {
	uint32_t id;
	NodeClass class;
	char* name;
	char* detail;
	bool  playing;

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

	int seq;
} Data;

extern Data data;

void launch_pipewire(void);

void mkLink(PWNode* node);
void unLink(PWNode* node);

#if __INCLUDE_LEVEL__ == 0 /////////////////////////////////////////////////////

#include "menu.c"

#include <assert.h>
#include <pipewire/pipewire.h>

#define printf(...)

pthread_barrier_t nullSinkBarrier = {0};

PWNodes pwNodes = {0};
Data    data    = {0};

const char* ShowNodeClass(NodeClass type) {
	switch(type) {
	case NT_INVALID:
		assert(false && "invalid node class!");
	case NT_SOURCE:
		return "src";
	case NT_SINK:
		return "snk";
	case NT_APP:
		return "app";
		break;
	}
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

	const char* media_name = spa_dict_lookup(info->props, PW_KEY_MEDIA_NAME);
	if(media_name) {
		free(node->detail);
		node->detail = strdup(media_name);
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
	const char* media_class = NULL;

	if(strcmp(type, PW_TYPE_INTERFACE_Node) == 0 && id != data.nullSink.id &&
	   (media_class = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS))) {
		NodeClass class = NT_INVALID;

		if(strcmp(media_class, "Audio/Source") == 0) {
			class = NT_SOURCE;
		} else if(strcmp(media_class, "Audio/Sink") == 0) {
			class = NT_SINK;
		} else if(strcmp(media_class, "Stream/Output/Audio") == 0) {
			class = NT_APP;
		}

		if(class == NT_INVALID) return;

		ArrayFind(pwNodes, node, it->id == id);
		assert(!node && "existing node re-added???");

		const char* name = spa_dict_lookup(props, PW_KEY_NODE_DESCRIPTION);
		if(!name) name = spa_dict_lookup(props, PW_KEY_NODE_NAME);
		assert(name && "node must have a name!");

		PWNode new = {
			.id       = id,
			.class    = class,
			.name     = strdup(name),
			.listener = malloc(sizeof(*new.listener)),
		};
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
				printf("port node %u.%u for %u\n", port.id, port.ix, node_id);
			}
		}
	}

	pthread_cond_signal(&redisplay);
}

static void on_registry_remove_event(void*, uint32_t id) {
	ArrayLoop(pwNodes, {
		if(it->id == id) {
			free(it->name);
			free(it->detail);
			free(it->ports.ptr);
			free(it->listener);

			memmove(it, it + 1, sizeof(*it) * (--pwNodes.len - i));
			break;
		}
	});

	pthread_cond_signal(&redisplay);
}

void launch_pipewire(void) {
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
}

void mkLink(PWNode* node) {
	pw_thread_loop_lock(data.thread_loop);

	ArrayLoopN(node->ports, src, {
		ArrayFind(data.nullSink.ports, dst, it->ix == src->ix);
		assert(dst && "sink must have a matching port");

		PWLink link = {0};

		struct pw_properties* props = pw_properties_new_string("");
		pw_properties_setf(props, PW_KEY_LINK_OUTPUT_PORT, "%u", src->id);
		pw_properties_setf(props, PW_KEY_LINK_INPUT_PORT, "%u", dst->id);

		link.proxy = pw_core_create_object(
			data.core, "link-factory", PW_TYPE_INTERFACE_Link, PW_VERSION_LINK,
			&props->dict, 0
		);

		pw_properties_clear(props);
		ArrayAdd(node->links, link);
	});

	pw_thread_loop_unlock(data.thread_loop);
}

void unLink(PWNode* node) {
	pw_thread_loop_lock(data.thread_loop);

	ArrayLoop(node->links, { pw_core_destroy(data.core, it->proxy); });
	ArrayFree(node->links);

	pw_thread_loop_unlock(data.thread_loop);
}

#endif
