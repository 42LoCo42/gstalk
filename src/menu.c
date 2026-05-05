#pragma once

#include <pthread.h>
#include <sys/types.h>

void launch_menu(void);
void selNode(int offset, int step);

extern bool autoadd;
extern bool deaf;
extern bool mute;

extern ssize_t selected;

extern pthread_cond_t redisplay;

#if __INCLUDE_LEVEL__ == 0 /////////////////////////////////////////////////////

#define _DEFAULT_SOURCE
#include "pipewire.c"
#include "util.c"

#include <stdio.h>

bool autoadd;
bool deaf;
bool mute;

ssize_t selected = -1;

pthread_cond_t redisplay;

static void* menu_fn(void*) {
	pthread_mutex_t mutex = {0};
	pthread_mutex_init(&mutex, NULL);

	for(;;) {
		if(selected != -1) selNode(0, 1);

		printf("[H[J");
		printf("(a) autoadd: %s[m\n", autoadd ? "[1;32myes" : "[1;31mno");
		printf("(d) deaf:    %s[m\n", deaf ? "[1;32myes" : "[1;31mno");
		printf("(m) mute:    %s[m\n", mute ? "[1;32myes" : "[1;31mno");

		puts("\n========== Microphones ===========\n");

		ArrayLoop(microphones, {
			if(!it->playing) printf("[2;3m");

			printf(
				"[%s%s] %s",
				it->ignore          ? "[1;31m-[22;39m"
				: it->links.len > 0 ? "[1;32mx[22;39m"
									: " ",
				it->playing ? "" : "[2;3m", it->desc
			);

			printf("[m\n");
		});

		puts("\n========== Applications ==========\n");

		ArrayLoop(applications, {
			if(!it->playing) printf("[2;3m");
			if(i == (size_t) selected) printf("[7m");

			printf(
				"[%s%s] %s",
				it->ignore          ? "[1;31m-[22;39m"
				: it->links.len > 0 ? "[1;32mx[22;39m"
									: " ",
				it->playing ? "" : "[2;3m", it->desc
			);

			printf("[m\n");
		});

		pthread_cond_wait(&redisplay, &mutex);
	}

	return NULL;
}

void launch_menu(void) {
	static pthread_t menu_thread = {0};
	pthread_create(&menu_thread, NULL, menu_fn, NULL);
}

void selNode(int offset, int step) {
	ssize_t new = -1;

	for(ssize_t i = offset; i <= (ssize_t) applications.len; i++) {
		size_t j = mod(max(selected, 0) + i * step, (ssize_t) applications.len);
		if(!applications.ptr[j].ignore) {
			new = j;
			break;
		}
	}

	selected = new;
}

#endif
