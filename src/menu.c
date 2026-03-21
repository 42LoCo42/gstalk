#pragma once

#include <pthread.h>
#include <sys/types.h>

void launch_menu(void);

extern bool    autoadd;
extern bool    deaf;
extern bool    mute;
extern ssize_t selected;

extern pthread_cond_t redisplay;

#if __INCLUDE_LEVEL__ == 0 /////////////////////////////////////////////////////

#define _DEFAULT_SOURCE
#include "pipewire.c"
#include "util.c"

#include <stdio.h>

bool    autoadd;
bool    deaf;
bool    mute;
ssize_t selected;

pthread_cond_t redisplay;

static void* menu_fn(void*) {
	pthread_mutex_t mutex = {0};
	pthread_mutex_init(&mutex, NULL);

	for(;;) {
		printf("[H[J");
		printf("(a) autoadd: %s[m\n", autoadd ? "[1;32myes" : "[1;31mno");
		printf("(d) deaf:    %s[m\n", deaf ? "[1;32myes" : "[1;31mno");
		printf("(m) mute:    %s[m\n", mute ? "[1;32myes" : "[1;31mno");

		puts("\n========================================\n");

		ArrayLoop(pwNodes, {
			if(!it->playing) printf("[2;3m");
			if(i == (size_t) selected) printf("[7m");

			printf(
				"[%s] node %u: %s", it->links.len > 0 ? "x" : " ", it->id,
				it->name
			);

			if(it->detail) printf(" [%s]", it->detail);
			if(i == (size_t) selected) printf("[27m");

			puts("");

			ArrayLoopN(it->ports, port, {
				printf("  port %u.%u\n", port->id, port->ix);
			});

			printf("[m\n");
		});

		if(selected >= (ssize_t) pwNodes.len) {
			selected = 0;
			continue;
		}

		pthread_cond_wait(&redisplay, &mutex);
	}

	return NULL;
}

void launch_menu(void) {
	static pthread_t menu_thread = {0};
	pthread_create(&menu_thread, NULL, menu_fn, NULL);
}

#endif
