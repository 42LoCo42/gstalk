#pragma once

#define _DEFAULT_SOURCE
#include "main.c"
#include "util.c"

#include <pipewire/pipewire.h>

Array(uint32_t, IDs);

typedef struct {
	uint32_t id;
	char*    name;
	char*    detail;
	bool     playing;
	IDs      ports;

	struct spa_hook* listener;
} PWNode;

Array(PWNode);

extern PWNodes pwNodes;

void launch_pipewire(void);

#if __INCLUDE_LEVEL__ == 0 /////////////////////////////////////////////////////

#include <assert.h>

PWNodes pwNodes = {0};

static int nodeSorter(const void* a, const void* b) {
	return ((PWNode*) a)->id - ((PWNode*) b)->id;
}

static void on_node_info(void*, const struct pw_node_info* info) {
	ArrayFind(pwNodes, node, it->id == info->id);

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

	pthread_barrier_wait(&barrier);
}

static void on_registry_event(
	void* data, uint32_t id, uint32_t, const char* type, uint32_t version,
	const struct spa_dict* props
) {
	struct pw_registry* registry = data;

	const char* media_class = NULL;
	int         is_app      = 0;

	if(strcmp(type, PW_TYPE_INTERFACE_Node) == 0 &&
	   (media_class = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS)) &&
	   (strcmp(media_class, "Audio/Source") &
	    strcmp(media_class, "Audio/Sink") &
	    (is_app = strcmp(media_class, "Stream/Output/Audio"))) == 0) {
		ArrayFind(pwNodes, node, it->id == id);
		assert(!node && "existing node re-added???");

		const char* name = spa_dict_lookup(props, PW_KEY_NODE_DESCRIPTION);
		if(!name) name = spa_dict_lookup(props, PW_KEY_NODE_NAME);
		assert(name && "node must have a name!");

		PWNode new = {
			.id       = id,
			.name     = strdup(name),
			.listener = malloc(sizeof(*new.listener)),
		};

		ArrayAdd(pwNodes, new);
		node = &ArrayLast(pwNodes);

		static struct pw_node_events node_events = {
			.version = PW_VERSION_NODE_EVENTS,
			.info    = on_node_info,
		};

		struct pw_node* nodeRef =
			pw_registry_bind(registry, id, type, version, 0);
		pw_node_add_listener(nodeRef, node->listener, &node_events, NULL);

		qsort(pwNodes.ptr, pwNodes.len, sizeof(pwNodes.ptr[0]), nodeSorter);

	} else if(strcmp(type, PW_TYPE_INTERFACE_Port) == 0) {
		const char* direction = spa_dict_lookup(props, PW_KEY_PORT_DIRECTION);
		assert(direction && "port must have a direction!");
		if(strcmp(direction, "out") != 0) return;

		const char* node_id_str = spa_dict_lookup(props, PW_KEY_NODE_ID);
		assert(node_id_str && "port must have a node ID!");

		uint32_t node_id = atoi(node_id_str);
		ArrayFind(pwNodes, node, it->id == node_id);
		if(node) { ArrayAdd(node->ports, id); }
	}
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

	pthread_barrier_wait(&barrier);
}

void launch_pipewire(void) {
	pw_init(NULL, NULL);
	struct pw_thread_loop* thread_loop = pw_thread_loop_new("pipewire", NULL);
	struct pw_loop*        loop        = pw_thread_loop_get_loop(thread_loop);
	struct pw_context*     context     = pw_context_new(loop, NULL, 0);
	struct pw_core*        core        = pw_context_connect(context, NULL, 0);

	struct pw_registry* registry =
		pw_core_get_registry(core, PW_VERSION_REGISTRY, 0);

	static struct spa_hook registry_listener = {0};

	static struct pw_registry_events registry_events = {
		.version       = PW_VERSION_REGISTRY_EVENTS,
		.global        = on_registry_event,
		.global_remove = on_registry_remove_event,
	};

	pw_registry_add_listener(
		registry, &registry_listener, &registry_events, registry
	);

	pw_thread_loop_start(thread_loop);
}

#endif
