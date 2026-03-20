#pragma once

void launch_input(void);

#if __INCLUDE_LEVEL__ == 0 /////////////////////////////////////////////////////

#include "menu.c"
#include "pipewire.c"
#include "util.c"

#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static struct termios term = {0};

static void cleanup(void) {
	tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

void launch_input(void) {
	if(tcgetattr(STDIN_FILENO, &term) < 0) die("tcgetattr");
	atexit(cleanup);

	struct termios raw = term;
	raw.c_lflag &= ~(ECHO | ICANON);
	if(tcsetattr(STDIN_FILENO, TCSANOW, &raw) < 0) die("tcsetattr");

	bool running = true;
	char buf[32] = {0};

	while(running) {
		ssize_t nodes = pwNodes.len;

		ssize_t l = read(STDIN_FILENO, buf, sizeof(buf));
		if(l < 0) die("read input");

		switch(l) {
		case 1:
			switch(buf[0]) {
			case 'a':
				autoadd ^= 1;
				break;
			case 'm':
				muted ^= 1;
				break;
			case 'q':
				running = false;
				break;

			case ' ':
			case '\n':
				PWNode* node = &pwNodes.ptr[selected];
				if(node->links.len == 0) {
					mkLink(node);
				} else {
					unLink(node);
				}
				break;
			}
			break;

		case 3:
			if(strncmp(buf, "[", 2) != 0) break;
			switch(buf[2]) {
			case 'A':
				selected = mod(selected - 1, nodes);
				break;
			case 'B':
				selected = mod(selected + 1, nodes);
				break;
			}
			break;
		}

		pthread_cond_signal(&redisplay);
	}
}

#endif
