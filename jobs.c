#include "jobs.h"

#include "shell.h"
#include "input.h"
#include "builtin.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

job* create_job(parsed_line* line, bool foreground, char* redir[][2]);
void destroy_job(job* j);

void launch_job(job* j);
void launch_process(process* p, pid_t pgid, int in,
	int out, bool foreground) __attribute__ ((noreturn));

void job_foreground(job* j);
void job_background(job* j);

void job_wait(job* j);

bool job_stopped(job* j);
bool job_done(job* j);

void job_report(job* j);

/*
*	Public functions
*/

jobs_state* jobs_init(void) {
	jobs_state *jobs = malloc(sizeof(jobs_state));
	jobs->first_job = NULL;

	return jobs;
}

void jobs_destroy(jobs_state* jobs) {
	while (jobs->first_job) {
		destroy_job(jobs->first_job);
	}

	free(jobs);
}

/*
*	Creates a job struct from given parsed_line
*/
void jobs_process(jobs_state* jobs, parsed_line* line) {
	job *j, *last;
	int i, k, argc;
	char* argv[MAX_ARGC];
	char* redir[MAX_COMMANDS][2] = {{NULL}};
	bool foreground = true;

	/* this should really be in parse_input, since redirection is
	 * so similar to to piping, but oh well */
	for (i = 0; i < line->cmdc; ++i) {
		memcpy(argv, line->argv[i], sizeof(char*) * MAX_ARGC);

		argc = line->argc[i];

		/* we can skip the command name and last argument
		 * since there can't be a filename after the bracket */
		for (k = 1; k < argc - 1; ++k) {
			if (argv[k][0] == '<') { /* stdin */
				redir[i][0] = argv[k+1];
				line->argv[i][k] = NULL;
				if (k < line->argc[i])
					line->argc[i] = k;
				/* skip one arg */
				++k;
			} else if (argv[k][0] == '>') { /* stdout */
				redir[i][1] = argv[k+1];
				line->argv[i][k] = NULL;
				if (k < line->argc[i])
					line->argc[i] = k;
				++k;
			}
		}
		/* to make the amp-checking a bit simpler */
		k = argc - 1;
	}

	/* check the last cmd's larg arg for amp */
	if (argv[k][0] == '&') {
		foreground = false;
		argv[k] = NULL;
	}

	/* TODO: a better way (a MUCH better way) */
	/* last argument of the last command */
	if (line->argv[line->cmdc-1][line->argc[line->cmdc-1] - 1][0] == '&') {
		foreground = false;

		/* remove ampersand (or whole last command if amp was the only arg) */
		line->argv[line->cmdc-1][line->argc[line->cmdc-1] - 1] = NULL;
		if (line->argc[line->cmdc-1] <= 1) {
			--line->cmdc;
		} else {
			--line->argc[line->cmdc-1];
		}
	}

	j = create_job(line, foreground, redir);
	if (!jobs->first_job) {
		jobs->first_job = j;
	} else {
		last = jobs->first_job;
		while (last && last->next) {
			last = last->next;
		}
		last->next = j;
	}

	launch_job(j);
}

/*
*	SIGCHLD handler, updates job and process status
*/
void jobs_update(int pd) {
	job *j;
	process *p;
	int status;
	pid_t pid = waitpid(WAIT_ANY, &status, WUNTRACED | WNOHANG);

	UNUSED(pd);

	if (pid <= 0) {
		return;
	}

	for (j = psh->jobs->first_job; j; j = j->next) {
		for (p = j->first_proc; p; p = p->next) {
			if (p->pid != pid)
				continue;

			p->status = status;
			if (WIFSTOPPED(status)) {
				p->stopped = true;
			} else {
				p->completed = true;
				if (WIFSIGNALED(status)) {
					fprintf(stderr, "%d: Terminated by signal %d.\n",
						pid, WTERMSIG(p->status));
				}
			}

			job_report(j);

			return;
		}
	}

	fprintf(stderr, "No child %d.\n", pid);
}

/*
*	Private functions
*/

void report(job* j, char const* status) {
	process *p;
	char *arg;
	int i;

	fprintf(stderr, "\n%d (%s):", j->pgid, status);
	for (p = j->first_proc; p; p = p->next) {
		i = 1;
		fprintf(stderr, "\n\t%d \t%s", p->pid, p->argv[0]);
		for (i = 1, arg = p->argv[i]; arg && arg[0]; arg = p->argv[++i]) {
			fprintf(stderr, " %s", arg);
		}
		fprintf(stderr, "\n");
	}
	fprintf(stderr, "\n");
}

/*
*	reports job status back after completion and then destroys the job
*	unless it's on foreground
*/
void job_report(job* j) {
	bool fg = j->foreground;
	if (j->foreground && job_done(j)) {
		return;
	}

	if (job_done(j)) {
		report(j, "done");
		destroy_job(j);
	} else if (job_stopped(j)) {
		report(j, "suspended");
	}

	if (!fg)
		input_restore();
}

/*
*	Have all the job's processes completed?
*/
bool job_done(job* j) {
	process *p;

	for (p = j->first_proc; p; p = p->next) {
		if (!p->completed)
			return false;
	}

	return true;
}

/*
*	Job suspended?
*/
bool job_stopped(job* j) {
	process *p;

	for (p = j->first_proc; p; p = p->next) {
		if (!p->stopped)
			return false;
	}

	return true;
}

void launch_builtin(builtin const* bin, char* argv[]) {
	int argc = 0;
	char *arg = argv[0];

	while (arg && arg[0]) {
		arg = argv[++argc];
	}

	bin->func(argc, argv);
}

/*
*	does the hard work of launching a job
*/
void launch_job(job* j) {
	builtin const *bin;
	process *p;
	pid_t pid;
	int fd[2], in, out;

	/* thanks, builtins */
	/* thuiltins */
	if ((bin = builtin_get(j->first_proc->argv[0]))) {
		if (j->foreground && !j->first_proc->next) {
			launch_builtin(bin, j->first_proc->argv);
			destroy_job(j);
		}
		return;
	}

	if (!j->foreground)
		printf("[%d]", j->id);

	/* start with non-pipe stdin */
	in = j->stdin;
	if (j->first_proc && j->first_proc->in_file) {
		in = open(j->first_proc->in_file, O_RDONLY);
		if (in < 0) {
			perror("psh: in");
			return;
		}
	}
	for (p = j->first_proc; p; p = p->next) {
		/* redirect output unless it's the last proc */
		if (p->next) {
			if (pipe(fd) < 0) {
				perror("PSH-pipe");
				return;
			}
			out = fd[STDOUT_FILENO];
		} else {
			out = j->stdout;
		}
		if (p->out_file) {
			/* we don't need stdin if we have a file in-between */
			if (p->next) {
				close(fd[STDIN_FILENO]);
				out = open(p->out_file, O_RDWR | O_CREAT | O_TRUNC);
			} else {
				out = open(p->out_file, O_WRONLY | O_CREAT | O_TRUNC);
			}

			if (out < 0) {
				perror("psh: out");
				return;
			}
		}

		pid = fork();

		if (pid == 0) { /* child proc */
			launch_process(p, j->pgid, in, out, j->foreground);
		} else if (pid < 0) { /* fork failed */
			perror("PSH-fork");
			return;
		} else { /* in parent */
			if (!j->foreground)
				printf(" %d", pid);
			p->pid = pid;
			/* if no group id for children yet, first child becomes leader */
			if (!j->pgid)
				j->pgid = pid;
			/* set child's group id */
			setpgid(pid, j->pgid);
		}

		if (in != j->stdin)
			close(in);

		/* next child's stdin */
		if (p->out_file && p->next) {
			/* reset fd position */
			if (lseek(out, 0, SEEK_SET) < 0)
				perror("psh: lseek");
			in = out;
		}
		else {
			if (out != j->stdout)
				close(out);
			in = fd[STDIN_FILENO];
		}
	}

	if (!j->foreground)
		printf("\n");

	if (j->foreground)
		job_foreground(j);
	else
		job_background(j);
}

/*
*	sets child's signals, pgid, dup2's stdio
*/
void launch_process(process* p, pid_t pgid, int in,
	int out, bool foreground) {

	pid_t pid = getpid();

	if (!pgid)
		pgid = pid;

	setpgid(pid, pgid);
	if (foreground)
		tcsetpgrp(psh->term, pgid);

	signal(SIGINT,  SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	signal(SIGTSTP, SIG_DFL);
	signal(SIGTTIN, SIG_DFL);
	signal(SIGTTOU, SIG_DFL);
	signal(SIGCHLD, &jobs_update);

	if (in != STDIN_FILENO) {
		dup2(in, STDIN_FILENO);
		close(in);
	}
	if (out != STDOUT_FILENO) {
		dup2(out, STDOUT_FILENO);
		close(out);
	}

	execvp(p->argv[0], p->argv);
	perror("psh: exec");
	exit(EXIT_FAILURE);
}

void job_foreground(job* j) {
	tcsetpgrp(psh->term, j->pgid);

	job_wait(j);

	tcsetpgrp(psh->term, psh->pgid);
}

void job_background(job* j) {
	UNUSED(j);
}

/* used for foreground job */
void job_wait(job* j) {
	do {}
	while (!job_done(j));

	destroy_job(j);
}

job* create_job(parsed_line* line, bool foreground, char* redir[][2]) {
	static int id = 1;
	process *p, *tmpp;
	job *j = malloc(sizeof(job));
	j->next = NULL;
	j->id = id++;
	j->line = line;
	j->pgid = 0;
	j->foreground = foreground;
	j->stdin = STDIN_FILENO;
	j->stdout = STDOUT_FILENO;
	j->stderr = STDERR_FILENO;
	int i;

	for (i = 0; i < line->cmdc; ++i) {
		p = malloc(sizeof(process));
		if (i == 0) {
			j->first_proc = p;
		}
		else {
			tmpp = j->first_proc;
			while (tmpp && tmpp->next)
				tmpp = tmpp->next;
			tmpp->next = p;
		}

		p->next = NULL;
		memcpy(p->argv, line->argv[i], sizeof(char*) * MAX_ARGC);
		p->pid = 0;
		p->completed = false;
		p->stopped = false;
		p->status = 0;
		p->in_file = NULL;
		p->out_file = NULL;

		/* check redirs */
		if (redir[i][0]) {
			p->in_file = strdup(redir[i][0]);
		}
		if (redir[i][1]) {
			p->out_file = strdup(redir[i][1]);
		}
	}

	return j;
}

void destroy_job(job* j) {
	process *p, *pnext;
	job *jnext;

	p = j->first_proc;
	while (p) {
		pnext = p->next;
		free(p->in_file);
		free(p->out_file);
		free(p);
		p = pnext;
	}

	if (psh->jobs->first_job == j) {
		psh->jobs->first_job = j->next;
	} else {
		jnext = psh->jobs->first_job;

		while(jnext) {
			if (jnext->next == j) {
				jnext->next = j->next;
				break;
			}

			jnext = jnext->next;
		}
	}

	free(j->line);
	free(j);
}
