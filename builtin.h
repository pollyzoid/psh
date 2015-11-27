#ifndef _BIN_GUARD
#define _BIN_GUARD

/* builtin stuff */

typedef void(builtin_func)(int argc, char* argv[]);

typedef struct builtin {
	char const* name;
	builtin_func* func;
} builtin;

builtin const* builtin_get(char const* name);

#endif
