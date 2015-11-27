#include "shell.h"
#include "input.h"
#include "jobs.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

#include <linux/limits.h>

bool check_exit(parsed_line* line);
void ignore_signals(void);
bool check_interactive(shell_state* sh);
void make_foreground(shell_state* sh);
bool set_pgrp(shell_state* sh);

/*
*	Public functions
*/

shell_state* shell_init(void) {
	shell_state *sh = NULL;

	sh = malloc(sizeof(shell_state));
	sh->pid = getpid();
	sh->term = STDIN_FILENO;

	if (!check_interactive(sh))
		goto error;

	make_foreground(sh);
	ignore_signals();

	if (!set_pgrp(sh))
		goto error;

	/* grab terminal */
	tcsetpgrp(sh->term, sh->pgid);

	if (!(sh->input = input_init()))
		goto error;

	if (!(sh->jobs = jobs_init()))
		goto error;

	print_prompt();

	return sh;

error:
	shell_destroy(sh);
	return NULL;
}

void shell_destroy(shell_state *sh) {
	if (sh->input)
		input_destroy(sh->input);
	if (sh->jobs)
		jobs_destroy(sh->jobs);

	free(sh);
}

bool shell_cmdloop(shell_state *sh) {
	parsed_line *line;

	if ((line = input_process(sh->input)) != NULL) {
		if (line->cmdc == 0)
			return true;

		if (check_exit(line)) {
			free(line);
			return false;
		}

		jobs_process(sh->jobs, line);
		line = NULL;

		print_prompt();
	}

	return true;
}

/*
*	Private functions
*/

bool check_exit(parsed_line* line) {
	if (line->argv[0] && line->argv[0][0] &&
		(strcmp(line->argv[0][0], "exit") == 0)) {
		return true;
	}

	return false;
}

void print_prompt(void) {
	static char cwd[PATH_MAX];

	if (!getcwd(cwd, PATH_MAX)) {
		perror("psh:cwd");
		exit(EXIT_FAILURE);
	}
	printf("[%s] $ ", cwd);
	fflush(stdout);
}

bool check_interactive(shell_state* sh) {
	if (!isatty(sh->term)) {
		fprintf(stderr, "Could not make PSH interactive\n");
		return false;
	}

	return true;
}

void make_foreground(shell_state* sh) {
	/* send TTIN until we're fg */
	while (tcgetpgrp(sh->term) != (sh->pgid = getpgrp())) {
		kill(-sh->pgid, SIGTTIN);
	}
}

void ignore_signals(void) {
	/* don't exit on signals */
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);

	/* ignore background i/o */
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);

	/* child signal */
	signal(SIGCHLD, &jobs_update);
}

bool set_pgrp(shell_state* sh) {
	setpgid(sh->pid, sh->pid);
	sh->pgid = getpgrp();
	if (sh->pid != sh->pgid) {
		fprintf(stderr, "Could not put PSH in its own process group\n");
		return false;
	}

	return true;
}
