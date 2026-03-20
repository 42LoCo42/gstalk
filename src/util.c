#pragma once

#include <err.h>
#include <stdlib.h>

extern int    ARGC;
extern char** ARGV;
extern char*  APP_NAME;

#define shift() ((void) (ARGC > 0 || usage()), ARGC--, (ARGV++)[0])
#define store() (ARGC = argc, ARGV = argv, APP_NAME = shift())

#define die(...) err(1, __VA_ARGS__)

#define ArrayN(t, n)                                                           \
	typedef struct {                                                           \
		t*     ptr;                                                            \
		size_t len;                                                            \
		size_t cap;                                                            \
	} n

#define Array(t) ArrayN(t, t##s)

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

#define ArrayLoop(arr, body) ArrayLoopN(arr, it, body)

#define ArrayLoopN(arr, it, body)                                              \
	for(size_t i = 0; i < (arr).len; i++) {                                    \
		typeof((arr).ptr[0])* it = &(arr).ptr[i];                              \
		body                                                                   \
	}

#define ArrayFindN(arr, result, it, pred)                                      \
	typeof((arr).ptr[0])* result = NULL;                                       \
	ArrayLoopN(arr, it, {                                                      \
		if(pred) {                                                             \
			result = it;                                                       \
			break;                                                             \
		}                                                                      \
	})

#define ArrayFind(arr, result, pred) ArrayFindN(arr, result, it, pred)

#define ArrayLast(arr) (arr).ptr[(arr).len - 1]

#define ArrayFree(arr)                                                         \
	do {                                                                       \
		free((arr).ptr);                                                       \
		(arr).ptr = NULL;                                                      \
		(arr).len = (arr).cap = 0;                                             \
	} while(0)

#if __INCLUDE_LEVEL__ == 0 /////////////////////////////////////////////////////

int    ARGC;
char** ARGV;
char*  APP_NAME;

#endif
