#pragma once

#include <err.h>
#include <stdio.h>
#include <stdlib.h>

// defined by glibc
extern char* program_invocation_short_name;
#define APP_NAME program_invocation_short_name

// defined by meson
#ifndef VERSION
#error "no version defined!"
#endif

[[noreturn]] void usage(bool fail);

#define die(...) err(1, __VA_ARGS__)

#define max(a, b) ((a) > (b) ? (a) : (b))
#define mod(x, b) ((((x) % (b)) + (b)) % (b))

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

#define ArrayFindI(arr, result, pred)                                          \
	ArrayLoopN(arr, it, {                                                      \
		if(pred) {                                                             \
			result = it;                                                       \
			break;                                                             \
		}                                                                      \
	})

#define ArrayFind(arr, result, pred)                                           \
	typeof((arr).ptr[0])* result = NULL;                                       \
	ArrayFindI(arr, result, pred)

#define ArrayLast(arr) (arr).ptr[(arr).len - 1]

#define ArrayFree(arr)                                                         \
	do {                                                                       \
		free((arr).ptr);                                                       \
		(arr).ptr = NULL;                                                      \
		(arr).len = (arr).cap = 0;                                             \
	} while(0)

#if __INCLUDE_LEVEL__ == 0 /////////////////////////////////////////////////////

[[noreturn]] void usage(bool fail) {
	static const char USAGE[] = {
#embed "usage.txt"
		, 0
	};

	fprintf(fail ? stderr : stdout, USAGE, APP_NAME);
	exit(fail ? EXIT_FAILURE : EXIT_SUCCESS);
}

#endif
