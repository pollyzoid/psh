#ifndef _JOBS_GUARD
#define _JOBS_GUARD

#include "input.h"

#include <stdio.h>
#include <stdbool.h>
#include <sys/types.h>

typedef struct process {
	struct process* next;

	char* argv[MAX_ARGC];

	pid_t pid;
	bool completed;
	bool stopped;

	/* used for redirecting in/out of files */
	char* in_file;
	char* out_file;

	int status;
} process;

typedef struct job {
	struct job* next;

	int id;
	parsed_line* line;

	pid_t pgid;
	int stdin, stdout, stderr;
	bool foreground;

	process* first_proc;
} job;

typedef struct jobs_state {
	job* first_job;
} jobs_state;

jobs_state* jobs_init(void);
void jobs_destroy(jobs_state* jobs);

void jobs_process(jobs_state* jobs, parsed_line* line);

void jobs_update(int pd);

#endif
