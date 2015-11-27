#include "builtin.h"

#include "shell.h"
#include "input.h"
#include "jobs.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

parsed_line* parse_input(history_line* line);

void builtin_cd(int argc, char* argv[]);
void builtin_history(int argc, char* argv[]);
void builtin_rerun(int argc, char* argv[]);

static const builtin builtins[] = {
	{"cd", builtin_cd},
	{"history", builtin_history},
	{"!", builtin_rerun},
	{NULL, NULL}
};

/* TODO: implement this with a hashmap */
builtin const* builtin_get(char const* name) {
	int i = 0;
	builtin const *ret = NULL;
	builtin const *bin;

	bin = &builtins[i];
	while(bin->name && bin->func) {
		if (strcmp(bin->name, name) == 0) {
			ret = bin;
		}
		bin = &builtins[++i];
	}

	return ret;
}

void builtin_cd(int argc, char* argv[]) {
	char *dir = getenv("HOME");

	if (argc > 1) {
		dir = argv[1];
	}

	if (chdir(dir) == -1) {
		perror("PSH");
	}
}

void builtin_history(int argc, char* argv[]) {
	UNUSED(argc);
	UNUSED(argv);
	int i = 0;
	history_line *hist = psh->input->history_first;

	while (hist && hist->next) {
		printf("%02d: %s\n", ++i, hist->buffer);
		hist = hist->next;
	}
}

void builtin_rerun(int argc, char* argv[]) {
	int i = 0;
	int in;
	parsed_line* line;
	history_line *hist = psh->input->history_first;

	if (argc <= 1) {
		fprintf(stderr, "not enough arguments\n");
		return;
	}

	in = atoi(argv[1]);

	while (hist && hist != psh->input->history_current->prev) {
		if (++i == in) {
			/* last minute hacks yaaay */
			/* please never actually do this */
			line = parse_input(hist);
			if (line->cmdc == 0)
				return;

			jobs_process(psh->jobs, line);
			return;
		}
		hist = hist->next;
	}
}
