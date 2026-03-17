#pragma once

#include <err.h>
#include <stdlib.h>

extern int    ARGC;
extern char** ARGV;
extern char*  APP_NAME;

#define shift() ((void) (ARGC > 0 || usage()), ARGC--, (ARGV++)[0])
#define store() (ARGC = argc, ARGV = argv, APP_NAME = shift())

#define die(...) err(1, __VA_ARGS__)

#define IF_TEST(...) __VA_ARGS__
#define IF_TEST1(...)
#define IF(COND, ...) IF_TEST##COND(__VA_ARGS__)

#define Array(t, ...)                                                          \
	typedef struct {                                                           \
		t*     ptr;                                                            \
		size_t len;                                                            \
		size_t cap;                                                            \
	} IF(__VA_OPT__(1), t##s) __VA_ARGS__

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

#define ArrayLoop(arr, body, ...)                                              \
	for(size_t i = 0; i < (arr).len; i++) {                                    \
		typeof((arr).ptr[0])* IF(__VA_OPT__(1), it) __VA_ARGS__ =              \
			&(arr).ptr[i];                                                     \
		body                                                                   \
	}

#define ArrayFind(arr, result, pred)                                           \
	typeof((arr).ptr[0])* result = NULL;                                       \
	ArrayLoop(arr, {                                                           \
		if(pred) {                                                             \
			result = it;                                                       \
			break;                                                             \
		}                                                                      \
	})

#define ArrayLast(arr) (arr).ptr[(arr).len - 1]

#if __INCLUDE_LEVEL__ == 0 /////////////////////////////////////////////////////

int    ARGC;
char** ARGV;
char*  APP_NAME;

#endif
