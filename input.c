#include "input.h"

#include "shell.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <linux/limits.h>

void set_attr(input_state* sh);
void reset_term(input_state* in, bool out);
bool read_input(input_state* in, char* buf);
parsed_line* parse_input(history_line* line);

void history_load(input_state* input);
void history_save(history_line* first);
history_line* history_add(input_state* input);
void history_destroy(history_line* first);
int history_travel(input_state* input, char* buf, int cursor,
	int pos, bool forw);

/*
*	Public functions
*/

input_state* input_init(void) {
	input_state *input;

	input = malloc(sizeof(input_state));
	input->history_current =
		input->history_first = NULL;
	input->cursor = 0;

	history_load(input);
	history_add(input);

	set_attr(input);
	reset_term(input, false);

	return input;
}

parsed_line* input_process(input_state* input) {
	parsed_line* line;

	/* set term into unbuffered mode */
	reset_term(input, false);

	/* terminal i/o */
	if (!read_input(input, input->history_current->buffer)) {
		return NULL;
	}
	/* back to buffered */
	reset_term(input, true);
	putc('\n', stdout);

	if (strlen(input->history_current->buffer) == 0) {
		print_prompt();
		return NULL;
	}

	/* parse line */
	line = parse_input(input->history_current);

	history_add(input);
	input->cursor = 0;

	return line;
}

void input_destroy(input_state* input) {
	history_save(input->history_first);
	reset_term(input, true);
	history_destroy(input->history_first);
	free(input);
}

void input_restore(void) {
	fputs("\n", stdout);
	print_prompt();
	if (strlen(psh->input->history_current->buffer) != 0) {
		reset_term(psh->input, false);
		fputs(psh->input->history_current->buffer, stdout);
	}
	fflush(stdout);
}

/*
*	Private functions
*/

parsed_line* parse_input(history_line* line) {
	/* strtok_r state */
	char *tok_pipe, *tok_cmd;
	char *pipe_buf, *cmd_buf;

	parsed_line *parse = malloc(sizeof(parsed_line));
	memset(parse->buffer, 0, sizeof(parse->buffer));
	memset(parse->argc, 0, sizeof(parse->argc));
	memset(parse->argv, 0, sizeof(parse->argv));
	memcpy(parse->buffer, line->buffer, strlen(line->buffer) + 1);

	parse->cmdc = 0;

	/* split by pipes */
	pipe_buf = strtok_r(parse->buffer, "|", &tok_pipe);
	while (pipe_buf != NULL) {
		cmd_buf = strtok_r(pipe_buf, " \t\n", &tok_cmd);

		/* then by spaces to get argv */
		while (cmd_buf != NULL) {
			if (parse->argc[parse->cmdc] < MAX_ARGC) {
				parse->argv[parse->cmdc][parse->argc[parse->cmdc]++] = cmd_buf;
			}
			else
			{
				fprintf(stderr, "Too many args\n");
				break;
			}

			cmd_buf = strtok_r(NULL, " \t\n", &tok_cmd);
		}

		++parse->cmdc;
		if (parse->cmdc >= MAX_COMMANDS) {
			fprintf(stderr, "Too many commands in pipeline\n");
			break;
		}
		parse->argc[parse->cmdc] = 0;
		pipe_buf = strtok_r(NULL, "|", &tok_pipe);
	}

	return parse;
}

void history_load(input_state* input) {
	FILE *fp;
	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	history_line* hist;

	char path[PATH_MAX] = {'\0'};
	strcat(path, getenv("HOME"));
	strcat(path, "/.phistory");

	fp = fopen(path, "r");
	if (!fp)
		return;

	while ((read = getline(&line, &len, fp)) != -1) {
		if (line[read - 1] == '\n') {
			line[read - 1] = '\0';
			--read;
		}
		hist = history_add(input);
		memcpy(hist->buffer, line, read);
	}

	fclose(fp);
	free(line);
}

void history_save(history_line* first) {
	history_line *hist = first;
	FILE *fp;

	char path[PATH_MAX] = {'\0'};
	strcat(path, getenv("HOME"));
	strcat(path, "/.phistory");

	fp = fopen(path, "w");
	if (!fp) {
		perror("psh: save");
		return;
	}

	/* don't save the WIP history line */
	while (hist && hist->next) {
		fprintf(fp, "%s\n", hist->buffer);
		hist = hist->next;
	}
	fclose(fp);
}

history_line* history_add(input_state* input) {
	history_line *last;
	history_line *hist = malloc(sizeof(history_line));
	memset(hist->buffer, 0, sizeof(hist->buffer));
	hist->prev = NULL;
	hist->next = NULL;

	if (!input->history_first) {
		input->history_first = hist;
		input->history_current = hist;
	} else {
		/* existing history, append to list */
		last = input->history_current;
		while (last && last->next) {
			last = last->next;
		}

		last->next = hist;
		hist->prev = last;
		input->history_current = hist;
	}

	return hist;
}

void history_destroy(history_line *first) {
	history_line *hist = first;
	history_line *next;

	while (hist) {
		next = hist->next;
		free(hist);
		hist = next;
	}
}

int history_travel(input_state* input, char* buf, int cursor,
	int pos, bool forw) {

	history_line *hist;
	int i;
	int len = strlen(buf);
	int newlen;

	hist = input->history_current;
	for (i = 0; i < (forw ? pos - 1 : pos + 1); ++i) {
		if (!hist)
			return 0;
		hist = hist->prev;
	}

	if (!hist || hist == input->history_current)
		return 0;

	newlen = strlen(hist->buffer);

	/* no editing history, sorry! */
	memcpy(buf, hist->buffer, BUFFER_MAX_LENGTH - 1);

	/* move all the way to right */
	for (i = cursor; i < len; ++i) {
		fputs("\033[C", stdout);
	}

	/* "backspace" old line */
	for(; i > 0; --i) {
		/* I have no idea why, but just '\b' or "\b" won't work */
		fputs("\b \b", stdout);
	}

	/* write new line */
	for (; i <= newlen; ++i){
		putc((buf)[i], stdout);
	}

	fflush(stdout);

	return newlen;
}

void set_attr(input_state* in) {
	/* save the original attrs */
	tcgetattr(0, &in->attr_old);

	/* copy to new attrs and change stuff */
	memcpy(&in->attr, &in->attr_old, sizeof(in->attr_old));
	in->attr.c_cc[VMIN] = 0;
	in->attr.c_cc[VTIME] = 10;
	in->attr.c_lflag &= ~(unsigned int)(ECHO | ICANON);
}

/* change between canonical and non-canon mode */
void reset_term(input_state* in, bool out) {
	if (!out)
		tcsetattr(0, TCSANOW, &in->attr);
	else
		tcsetattr(0, TCSANOW, &in->attr_old);
}

void backspace(char* buf, int* cursor, int* len) {
	int i;

	if (*cursor <= 0)
		return;

	/* move back one */
	putc('\b', stdout);

	/* copy memory over the erased char */
	memmove(&buf[*cursor - 1], &buf[*cursor], *len - *cursor + 1);

	/* write over the old line */
	for (i = *cursor - 1; i < *len; ++i) {
		putc(buf[i], stdout);
	}

	/* erase extra character, left behind by memmove */
	fputs(" \b", stdout);

	while(i-- > *cursor)
		putc('\b', stdout);

	--*cursor;
	--*len;
	fflush(stdout);
}

#define ESC 27
#define DEL 127

bool read_input(input_state* inp, char* buf) {
	char c = 0;
	int len, cursor;
	int i;
	int hislen;
	int hispos = 0;

	cursor = inp->cursor;
	len = strlen(buf);

	while (true) {
		if (!read(0, &c, 1)) {
			inp->cursor = cursor;
			return false;
		}

		switch(c) {
		case '\n':
			return true;
		case '\b':
		case DEL:
			backspace(buf, &cursor, &len);
			break;
		case ESC: /* escape char */
			if (!read(0, &c, 1)) {
				inp->cursor = cursor;
				return false;
			}

			if (c == '[') {
				if (!read(0, &c, 1)) {
					inp->cursor = cursor;
					return false;
				}
				switch(c) {
				case 'A': /* up */
					hislen = history_travel(inp, buf, cursor, hispos, false);
					if (hislen) {
						++hispos;
						cursor = len = hislen;
					}
					break;
				case 'B': /* down */
					hislen = history_travel(inp, buf, cursor, hispos, true);
					if (hislen) {
						--hispos;
						cursor = len = hislen;
					}
					break;
				case 'C': /* right */
					if (cursor < len) {
						++cursor;
						fputs("\033[C", stdout);
						fflush(stdout);
					}
					break;
				case 'D': /* left */
					if (cursor > 0) {
						--cursor;
						fputs("\033[D", stdout);
						fflush(stdout);
					}
					break;
				case '1': /* Ctrl-A */
					break;
				case '4': /* Ctrl-E */
					break;
				}
			}
			break;
		default:
			if (!isprint(c)) /* skip non-printables */
				break;

			/* leave room for null terminator */
			if (len >= (BUFFER_MAX_LENGTH - 1))
				break;

			if (cursor == len) { /* EOL, append */
				buf[cursor] = c;
			} else { /* insert */
				memmove(&buf[cursor + 1], &buf[cursor], len - cursor);

				buf[cursor] = c;

				/* write the changed rest of the line */
				for (i = cursor; i <= len; ++i)
					putc(buf[i], stdout);
				for (; i > cursor; --i) /* move cursor back to original pos */
					fputs("\033[D", stdout);
			}

			++cursor;
			++len;

			putc(c, stdout);
			fflush(stdout);
			break;
		}
	}
}
