#include "gstreamer.c"
#include "pulseaudio.c"
#include "util.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char transmitter_src[] = {
#embed "transmitter.txt"
	, 0
};

static const char receiver_src[] = {
#embed "receiver.txt"
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

static void setup_rx(void) {
	char* port = shift();
	snprintf(pipeline_src, sizeof(pipeline_src), receiver_src, port);
}

static void setup_tx(void) {
	char* host = shift();
	char* port = shift();
	char* id   = NULL;

	if(ARGC > 0) {
		id = shift();
	} else {
		print_audio_sources();
		printf("ID: ");

		size_t n = 0;
		if(getline(&id, &n, stdin) == -1) exit(0);
		id[strcspn(id, "\n")] = 0;
	}

	snprintf(
		pipeline_src, sizeof(pipeline_src), transmitter_src, id, host, port
	);
}

int main(int argc, char** argv) {
	store();

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
