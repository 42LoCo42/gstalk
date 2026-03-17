#pragma once

void print_audio_sources(void);

#if __INCLUDE_LEVEL__ == 0 /////////////////////////////////////////////////////

#include "util.c"

#include <math.h>
#include <pthread.h>
#include <pulse/pulseaudio.h>
#include <stdio.h>
#include <string.h>

typedef struct {
	uint32_t index;
	char*    name;
} AudioSource;

Array(AudioSource) AudioSources;
static AudioSources      audioSources = {0};
static pthread_barrier_t barrier      = {0};

static void
pa_source_cb(pa_context*, const pa_source_info* i, int is_last, void*) {
	if(is_last) return;

	AudioSource source = {
		.index = i->index,
		.name  = strdup(i->description),
	};

	ArrayAdd(audioSources, source);
}

static void
pa_sink_input_cb(pa_context*, const pa_sink_input_info* i, int is_last, void*) {
	if(is_last) {
		pthread_barrier_wait(&barrier);
		return;
	}

	AudioSource source = {
		.index = i->index,
	};

	if(asprintf(
		   &source.name, "%s: %s", pa_proplist_gets(i->proplist, "node.name"),
		   i->name
	   ) == -1)
		die("asprintf audio source name");

	ArrayAdd(audioSources, source);
}

static void context_cb(pa_context* context, void*) {
	switch(pa_context_get_state(context)) {
	case PA_CONTEXT_READY:
		pthread_barrier_wait(&barrier);
		break;

	default:
	}
}

void print_audio_sources(void) {
	if(pthread_barrier_init(&barrier, NULL, 2) < 0) die("thread barrier");

	pa_threaded_mainloop* mainloop = pa_threaded_mainloop_new();
	if(mainloop == NULL) die("PA mainloop");

	pa_mainloop_api* mainloop_api = pa_threaded_mainloop_get_api(mainloop);
	if(mainloop_api == NULL) die("PA mainloop API");

	pa_context* context = pa_context_new(mainloop_api, NULL);
	if(context == NULL) die("PA context");

	pa_context_set_state_callback(context, context_cb, NULL);
	if(pa_context_connect(context, NULL, 0, NULL) < 0)
		die("PA context connect");

	if(pa_threaded_mainloop_start(mainloop) < 0) die("PA mainloop run");
	pthread_barrier_wait(&barrier);

	pa_context_get_source_info_list(context, pa_source_cb, NULL);
	pa_context_get_sink_input_info_list(context, pa_sink_input_cb, NULL);
	pthread_barrier_wait(&barrier);

	pa_context_disconnect(context);
	pa_context_unref(context);
	pa_threaded_mainloop_stop(mainloop);
	pa_threaded_mainloop_free(mainloop);

	uint32_t largest_index = 0;
	ArrayLoop(audioSources, {
		if(it.index > largest_index) largest_index = it.index;
	});

	ArrayLoop(audioSources, {
		printf("%*u: %s\n", (int) log10(largest_index) + 2, it.index, it.name);
	});
}

#endif
