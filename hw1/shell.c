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

void sigint_handle(int sig);
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
				pid_t pid;
				int status;

                //form argv
                const int token_length = tokens_get_length(tokens);
                char ** argv = (char** )malloc(sizeof(char *) * (token_length + 1));
                argv[token_length] = NULL;
                for (int i = 0; i < token_length; i++) {
                    argv[i] = tokens_get_token(tokens, i);
                }

				pid = fork();
				if (pid < 0) {
					perror("fork");
					exit(EXIT_FAILURE);
				}

				if (pid == 0) {
					/* exec a program*/
					run_prog(argv);

				} else {
					//wait for exit of program and continute
					waitpid(pid, &status, 0);

		    #ifdef DEBUG
					fprintf(stdout, "program exited with: %d.\n", status);
		    #endif
				}
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
    //if (shell_is_interactive)
        /* Please only print shell prompts when standard input is not a tty */
        //fprintf(stdout, "%d: ",++line_num);
    //fflush(stdout);
}

/* quit handler */
void sigquit_prog(int sig)
{
	printf("^\\\n");
}

/* stop handler */
void sigtstp_prog(int sig)
{
	;
}


/* run program with argvs */
int run_prog(char *const argv[])
{
	char * cmd = NULL;
	struct stat sb; // check file status

	if (access(argv[0], X_OK) != -1) {
		/* file excutable*/
		cmd = argv[0];
	} else {
		/*find it in path*/

		/* try to get full path */
		char * path = getenv("PATH");
    #ifdef DEBUG
		printf("PATH = %s\n", path);
    #endif
		char ** pvec = get_path_vector(path);
    #ifdef DEBUG
		for (int i = 0; pvec[i] != NULL; ++i)
			printf("pvec[%d] = %s\n", i, pvec[i]);
    #endif
		cmd = get_fpath(pvec, argv[0], X_OK);
    #ifdef DEBUG
		printf("cmd = %s\n", cmd);
    #endif

		if (cmd == NULL) {
			/* file doesn't exist; */
			destroy_path_vector(pvec);
			fprintf(stderr, "-shell: %s: command not found \n", argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	//running program
    #ifdef DEBUG
	printf("runing program: %s, with argv: ", cmd);
	for (int i = 0; i < token_length; i++) {
		fprintf(stdout, "%s ", argv[i]);
	}
	printf("\n");
    #endif

	//check if is a regular file
	if (stat(cmd, &sb) == -1) {
		perror("stat error:");
		exit(EXIT_FAILURE);
	}
	if (S_ISREG(sb.st_mode)) {
		if (execv(cmd, argv) < 0) {
            perror(cmd);
			exit(EXIT_FAILURE);
		}
	} else {
		if (S_ISDIR(sb.st_mode))
			fprintf(stderr, "-shell: %s: is a directory\n", cmd);
		else
			fprintf(stderr, "-shell: %s: not excutable\n", cmd);
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}


typedef struct cmds {
	char ** argv;
	FILE input;
	FILE output;
	bool bkg;
} cmds_t;

int parse (struct tokens *tkn, cmds_t cmdv)
{
	int n = tokens_get_length(tkn); // token size

	for (int i = 0; i < n; i++) {

	}
}
