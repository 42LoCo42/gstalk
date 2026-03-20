#pragma once

#define _POSIX_C_SOURCE 200809L
#include <pthread.h>

extern pthread_cond_t redisplay;

#if __INCLUDE_LEVEL__ == 0 /////////////////////////////////////////////////////

pthread_mutex_t mutex     = {0};
pthread_cond_t  redisplay = {0};

#include "gstreamer.c"
#include "pipewire.c"
#include "util.c"

#include <math.h>
#include <stdio.h>

static const char receiver_src[] = {
#embed "receiver.txt"
	, 0
};

static const char transmitter_src[] = {
#embed "transmitter.txt"
	, 0
};

static char pipeline_src[8192] = {0};

static int usage() {
	errx(
		1,
		""
		"usage: %s mode...\n"
		"  mode ls: lists all pulseaudio sink inputs\n"
		"  mode rx: <port>\n"
		"  mode tx: <host> <port> [ID]",
		APP_NAME
	);
}

static void print_audio_sources(void) {};

static void setup_rx(void) {
	char* port = shift();
	snprintf(pipeline_src, sizeof(pipeline_src), receiver_src, port);
}

static void setup_tx(void) {
	char* host = shift();
	char* port = shift();
	snprintf(pipeline_src, sizeof(pipeline_src), transmitter_src, host, port);
}

int main(int argc, char** argv) {
	store();

	pthread_mutex_init(&mutex, NULL);
	pthread_cond_init(&redisplay, NULL);
	launch_pipewire();

	for(;;) {
		printf("[H[J");

		uint32_t max_id = 0;
		ArrayLoop(pwNodes, {
			if(it->id > max_id) max_id = it->id;
		});

		int pad = floor(log10(max_id)) + 1;

		ArrayLoop(pwNodes, {
			if(!it->playing) printf("[2;3m");
			printf("node %*u: %s", pad, it->id, it->name);
			if(it->detail) printf(" [%s]", it->detail);
			puts("");

			ArrayLoopN(it->ports, port, {
				printf("  port %u.%u\n", port->id, port->ix);
			});

			printf("[m");
		});

		pthread_cond_wait(&redisplay, &mutex);
	}

	return 0;

	const char* mode = shift();
	if(strcmp(mode, "ls") == 0) {
		print_audio_sources();
		return 0;
	} else if(strcmp(mode, "rx") == 0) {
		setup_rx();
	} else if(strcmp(mode, "tx") == 0) {
		setup_tx();
	} else {
		usage();
	}

	printf("launching pipeline: %s\n", pipeline_src);
	launch_pipeline(pipeline_src);
}

#endif
