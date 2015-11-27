#ifndef _INPUT_GUARD
#define _INPUT_GUARD

#include <stdbool.h>
#include <termios.h>

/* POSIX minimum */
#define BUFFER_MAX_LENGTH 4096
#define MAX_COMMANDS 16
#define MAX_ARGC 16

typedef struct parsed_line {
	char buffer[BUFFER_MAX_LENGTH];
	char *argv[MAX_COMMANDS][MAX_ARGC];

	int cmdc;
	int argc[MAX_COMMANDS];
} parsed_line;

typedef struct history_line {
	struct history_line* prev;
	struct history_line* next;

	char buffer[BUFFER_MAX_LENGTH];
} history_line;

typedef struct input_state {
	struct termios attr;
	struct termios attr_old;

	int cursor;

	history_line* history_first;
	history_line* history_current;
} input_state;

input_state* input_init(void);
void input_destroy(input_state* input);

parsed_line* input_process(input_state* input);

void input_restore(void);

#endif
