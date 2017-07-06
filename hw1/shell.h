#pragma once

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <sys/stat.h>

#include "tokenizer.h"
#include "get_full_path.h"
#include "jobs.h"

/* Convenience macro to silence compiler warnings about unused function parameters. */
#define unused __attribute__((unused))

/* Whether the shell is connected to an actual terminal or not. */
bool shell_is_interactive;

/* File descriptor for the shell input */
int shell_terminal;

/* Terminal mode settings for the shell */
struct termios shell_tmodes;

int cmd_exit(struct tokens *tokens);
int cmd_help(struct tokens *tokens);
int cmd_jobs(struct tokens *tokens);
int cmd_fg(struct tokens *tokens);
int cmd_bg(struct tokens *tokens);
int cmd_wait(struct tokens *tokens);
int cmd_cd(struct tokens *tokens);

void clear_jobs();

/* signal handlers */
void sigint_handle(int sig);
void sigquit_handle(int sig);
void sigtstp_handle(int sig);
void sigttin_handle(int sig);
void sigttou_handle(int sig);
void sigterm_handle(int sig);

/* Built-in command functions take token array (see parse.h) and return int */
typedef int cmd_fun_t (struct tokens *tokens);

/* Built-in command struct and lookup table */
typedef struct fun_desc {
	cmd_fun_t *fun;
	char *cmd;
	char *doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {
	{ cmd_help, "?",    "show this help menu"	  },
	{ cmd_exit, "exit", "exit the command shell"	  },
	{ cmd_jobs, "jobs", "display current jobs"	  },
	{ cmd_fg, "fg", "foregorund a background job \n\tusage:\tfg [-gid] gid \n\t  or\tfg job_number"	  },
	{ cmd_bg, "bg", "Resume a background job"	  },
	{ cmd_wait, "wait", "wait for all background jobs"	  },
    { cmd_cd, "cd", "wait for all background jobs"	  },
};

void init_shell();
int lookup(char cmd[]);

typedef struct running_job running_job_t;
void running_job_init(running_job_t * job);
void running_job_set_gid (running_job_t * job, pid_t gid);
pid_t running_job_get_gid (running_job_t * job);
void running_job_add_pid (running_job_t * job, pid_t pid);
int running_job_set_status (running_job_t * job, pid_t pid, int status);
int running_job_get_status (running_job_t *job, pid_t pid, int *status);
int running_job_set_cmd (running_job_t * job, char * cmd);
void running_job_set_bg (running_job_t * job, bool bg);
bool running_job_get_bg (running_job_t * job);
bool running_job_finished (running_job_t * job);
void running_job_set_changed(running_job_t *job_p, bool changed);
void running_job_print (running_job_t * job, FILE *fd);
char *running_job_get_cmd(running_job_t * job);
void wait_for_job(running_job_t **job_list_p);


void job_list_apend(running_job_t **job_list, running_job_t *job);
bool job_list_is_empty(running_job_t **job_list);
int job_list_remove(running_job_t **job_list, running_job_t *job);
void job_list_destroy (running_job_t **job_list_p);
running_job_t *job_list_get_fg_job(running_job_t **job_list_p);
running_job_t *job_list_get_recent(running_job_t **job_list_p);
void job_list_print(running_job_t **job_list, FILE *fd);
running_job_t *job_list_get_job_by_gid(running_job_t **job_list, pid_t gid);
running_job_t *job_status_handle(running_job_t **job_list_p, pid_t rpid, int status);
running_job_t *job_list_get_changed_job(running_job_t **job_list_p);
int job_list_get_index(running_job_t **job_list_p, running_job_t *job_p);
int job_list_update_recent(running_job_t **job_list_p, running_job_t *job_p);

void sigchld_handle(int sig);

char * status_to_string(int status);
