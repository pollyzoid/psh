#ifndef _SHELL_GUARD
#define _SHELL_GUARD

#include <stdbool.h>
#include <sys/types.h>

/* to get rid of those unused parameters */
#define UNUSED(x) (void)(x)

struct input_state;
struct jobs_state;

typedef struct shell_state {
	struct input_state *input;
	struct jobs_state *jobs;

	pid_t pid;
	pid_t pgid;

	int term;
	int pad;
} shell_state;

extern shell_state *psh;

shell_state* shell_init(void);
void shell_destroy(shell_state* sh);

void print_prompt(void);

bool shell_cmdloop(shell_state* sh);

#endif
