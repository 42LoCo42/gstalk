#pragma once

#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200112L

#include <assert.h>
#include <err.h>

extern int    ARGC;
extern char** ARGV;
extern char*  APP_NAME;

#define shift() ((void) (ARGC > 0 || usage()), ARGC--, (ARGV++)[0])
#define store() (ARGC = argc, ARGV = argv, APP_NAME = shift())

#define die(...) err(1, __VA_ARGS__)

#define Array(t)                                                               \
	typedef struct {                                                           \
		t*     ptr;                                                            \
		size_t len;                                                            \
		size_t cap;                                                            \
	}

#define ArrayAdd(arr, x)                                                       \
	do {                                                                       \
		if((arr).len >= (arr).cap)                                             \
			(arr).ptr = reallocarray(                                          \
				(arr).ptr, ((arr).cap = (arr).cap ? (arr).cap << 1 : 16),      \
				sizeof((arr).ptr[0])                                           \
			);                                                                 \
                                                                               \
		(arr).ptr[(arr).len++] = (x);                                          \
	} while(0)

#if __INCLUDE_LEVEL__ == 0 /////////////////////////////////////////////////////

int    ARGC;
char** ARGV;
char*  APP_NAME;

#endif
