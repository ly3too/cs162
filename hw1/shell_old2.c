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

/* Process group id for the shell */
pid_t shell_pgid;

/* line number to show */
int line_num = 0;

int cmd_exit(struct tokens *tokens);
int cmd_help(struct tokens *tokens);
int cmd_jobs(struct tokens *tokens){};
int cmd_fg(struct tokens *tokens){};
int cmd_bg(struct tokens *tokens){};
int cmd_wait(struct tokens *tokens){};

/* signal handlers */
void sigint_handle(int sig);
void sigquit_handle(int sig);
void sigtstp_handle(int sig);

int run_prog(char *const argv[]);

/* Built-in command functions take token array (see parse.h) and return int */
typedef int cmd_fun_t (struct tokens *tokens);

/* Built-in command struct and lookup table */
typedef struct fun_desc {
	cmd_fun_t *fun;
	char *cmd;
	char *doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {
	{ cmd_help, "?",    "show this help menu"	   },
	{ cmd_exit, "exit", "exit the command shell"	   },
	{ cmd_jobs, "jobs", "display current jobs"	   },
	{ cmd_fg, "fg", "foregorund a background job"	   },
	{ cmd_bg, "bg", "Resume a background job"	   },
	{ cmd_wait, "wait", "wait for all background jobs"	   },
};

/* Prints a helpful description for the given command */
int cmd_help(unused struct tokens *tokens)
{
	for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
		printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
	return 1;
}

/* Exits this shell */
int cmd_exit(unused struct tokens *tokens)
{
	exit(0);
}

/* Looks up the built-in command, if it exists. */
int lookup(char cmd[])
{
	for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
		if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0))
			return i;
	return -1;
}

/* Intialization procedures for this shell */
void init_shell()
{
	/* Our shell is connected to standard input. */
	shell_terminal = STDIN_FILENO;

	/* Check if we are running interactively */
	shell_is_interactive = isatty(shell_terminal);

	if (shell_is_interactive) {
		/* If the shell is not currently in the foreground, we must pause the shell until it becomes a
		 * foreground process. We use SIGTTIN to pause the shell. When the shell gets moved to the
		 * foreground, we'll receive a SIGCONT. */
		while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
			kill(-shell_pgid, SIGTTIN);

		/* Saves the shell's process id */
		shell_pgid = getpid();

		/* Take control of the terminal */
		tcsetpgrp(shell_terminal, shell_pgid);

		/* Save the current termios to a variable, so it can be restored later. */
		tcgetattr(shell_terminal, &shell_tmodes);
	}

	signal(SIGINT, sigint_handle);
	signal(SIGQUIT, sigquit_handle);
	signal(SIGTSTP, sigtstp_handle);
	signal(SIGCHLD, SIG_DFL);
	//signal(SIGTTIN, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);
}

int main(unused int argc, unused char *argv[])
{
	init_shell();

	static char line[4096];

	/* Please only print shell prompts when standard input is not a tty */
	if (shell_is_interactive)
		fprintf(stdout, "%d: ", line_num);

	while (fgets(line, 4096, stdin)) {
		/* Split our line into words. */
		struct tokens *tokens = tokenize(line);

		//print out tolkens
	#ifdef DEBUG
		size_t i;
		for (i = 0; i < tokens_get_length(tokens); i++) {
			fprintf(stdout, "%s\t", tokens_get_token(tokens, i));
		}
		if (i > 0)
			fprintf(stdout, "\n");
	#endif


		if (tokens_get_length(tokens) > 0) {
			/* Find which built-in function to run. */
			int fundex = lookup(tokens_get_token(tokens, 0));

			if (fundex >= 0) {
				cmd_table[fundex].fun(tokens);

			} else {
				/* REPLACE this to run commands as programs. */

        job_vector_t *job_vect = get_job_vector(tokens);
				if(job_vect != NULL) {
					/* no error, run jobs */

					for(int job_n=0; job_n < job_vector_get_length(job_vect); job_n++) {
						job_t * job_p = job_vector_get_job(job_vect, job_n);

						for(int proc_n=0; proc_n < job_get_process_size(job_p); proc_n++) {
							process_t *proc_p = job_get_process(job_p, proc_n);
							pid_t pid;

							pid = fork();
							if (pid < 0) {
								perror("fork");
								exit(EXIT_FAILURE);

							} else if (pid == 0) {
								/* child process */

								#ifdef DEBUG
									int stdin_cpy = dup(STDIN_FILENO);
								#endif

								dup2(process_get_input(proc_p), STDIN_FILENO);
								dup2(process_get_output(proc_p), STDOUT_FILENO);

								signal(SIGINT, SIG_DFL);
								signal(SIGQUIT, SIG_DFL);
								signal(SIGTSTP, SIG_DFL);
								signal(SIGTTIN, SIG_DFL);
								signal(SIGTTOU, SIG_DFL);

								/* wait for parent to initialize*/
								kill(getpid(), SIGTSTP);

								#ifdef DEBUG
								printf("mypid=%d, mygid=%d, tcgetpgrp=%d \n", getpid(), getpgid(0), tcgetpgrp(stdin_cpy));
								#endif

								if (execv(process_get_cmd(proc_p), process_get_argv(proc_p)) < 0) {
						      perror(process_get_cmd(proc_p));
									exit(EXIT_FAILURE);
								}

							} else {
								/* parent process */

								if(process_get_output(proc_p) != STDOUT_FILENO)
									close(process_get_output(proc_p));
								if(process_get_input(proc_p) != STDIN_FILENO)
									close(process_get_input(proc_p));

								if (proc_n == 0 ) {
									/* set gid = pid */
									job_set_gid(job_p, pid);
									if (setpgid(pid, pid) < 0) {
										perror("set pgid");
										exit(EXIT_FAILURE);
									}

								} else {
									/* set gid to pid of first process */
									if (setpgid(pid, job_get_gid(job_p)) < 0) {
										perror("set pgid");
										exit(EXIT_FAILURE);
									}
								}

							} //end parent process

						} // end process n

						int status;
						pid_t rpid;

						#ifdef DEBUG
						printf("I'm parent process, mypid=%d, mygid=%d, tcgetpgrp=%d \n", getpid(), getpgid(0), tcgetpgrp(STDIN_FILENO));
						#endif

						if(! job_run_background(job_p)) {
							if(tcsetpgrp(STDIN_FILENO, job_get_gid(job_p)) < 0) {
								perror("set foreground");
								exit(EXIT_FAILURE);
							}
						}

						/* weakup child process group */
						kill(-job_get_gid(job_p), SIGCONT);



						while(true && !job_run_background(job_p)) {

							rpid = waitpid(-1, &status, WCONTINUED | WUNTRACED);
							#ifdef DEBUG
								printf("groupid of child = %d\n", -job_get_gid(job_p));
							#endif

							if(rpid == -1) {
									perror("waitpid");
									//exit(EXIT_FAILURE);
									break;
							} else if(rpid == 0) {
									printf("no waited process changed status\n");
									break;
							} else {
									printf("child %d ", rpid);
									if(WIFEXITED(status)) {
											printf("exited with %d \n", WEXITSTATUS(status));
											//break;
									}
									if(WIFSIGNALED(status)) {
											printf("terminated by signal %d\n", WTERMSIG(status));
											//break;
									}
									if(WIFSTOPPED(status))
											printf("stoped by signal %d \n", WSTOPSIG(status));
											//break;
									if(WIFCONTINUED(status))
											printf("continued \n");
											//break;
									fflush(stdout);
							}
						}

						/* resume to foreground */
						if(!job_run_background(job_p) && tcsetpgrp(STDIN_FILENO, getpgid(0)) < 0) {
							perror("parent set foreground");
							exit(EXIT_FAILURE);
						}


					} // end job n

				}

				job_vector_clean_up(job_vect);
			}
		}

		fflush(stdout);
		fflush(stderr);

		if (shell_is_interactive) {
			/* Please only print shell prompts when standard input is not a tty */
			fprintf(stdout, "%d: ", ++line_num);
            fflush(stdout);
        }

		/* Clean up memory */
		tokens_destroy(tokens);
	}

	return 0;
}

void sigint_handle(int sig)
{
	printf("^C\n");
  /* send signal to all foreground process */
}

/* quit handler */
void sigquit_handle(int sig)
{
	printf("^\\\n");
}

/* stop handler */
void sigtstp_handle(int sig)
{
	printf("^z\n");
}
