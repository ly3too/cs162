#include "shell.h"

#ifdef DEBUG
# define DEBUG_PRINT(x) printf x
#else
# define DEBUG_PRINT(x) NULL
#endif

/* Process group id for the shell */
pid_t shell_pgid;

/* line number to show */
int line_num = 0;

struct running_job {
	pid_t gid;
	pid_t *pidv;
	size_t pidc;
	int *statusv;
	char *cmd;
	bool bg;
	bool changed;
	running_job_t * next_job;
};

running_job_t *job_list = NULL;

int main(unused int argc, unused char *argv[])
{
	init_shell();

	static char line[4096];
	running_job_t *changed_job;

	/* Please only print shell prompts when standard input is not a tty */
	if (shell_is_interactive) {
		fprintf(stdout, "%s\n", getenv("PWD"));
		fprintf(stdout, "%d: ", line_num);
	}

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
						running_job_t *this_job = (running_job_t *)malloc(sizeof(running_job_t));
						running_job_init(this_job);
						running_job_set_cmd(this_job, job_get_cmd_line(job_p));

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
								signal(SIGTERM, SIG_DFL);

								/* wait for parent to initialize*/
								//kill(getpid(), SIGTSTP);

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
									if (setpgid(pid, pid) < 0) {
										perror("set pgid");
										exit(EXIT_FAILURE);
									}

									running_job_set_gid(this_job, pid);

								} else {
									/* set gid to pid of first process */
									if (setpgid(pid, running_job_get_gid(this_job)) < 0) {
										perror("set pgid");
										exit(EXIT_FAILURE);
									}
								}

								running_job_add_pid(this_job, pid);

							} //end parent process

						} // end process n

						job_list_apend(&job_list, this_job);

						if(!job_run_background(job_p))
						{
							wait_for_job(&job_list);
							DEBUG_PRINT(("debug: get job [%d] %s \n", job_list_get_index(&job_list, this_job), running_job_get_cmd(this_job)));
						}
						else {
							running_job_set_bg(this_job, true);
							printf("[%d] gid:%d \t%s\n", job_list_get_index(&job_list, this_job), running_job_get_gid(this_job), running_job_get_cmd(this_job));
						}

					} // end job n

				}

				job_vector_clean_up(job_vect);
			} //end execut non built-in
		} //end tokens lenght > 0

		wait_for_job(&job_list);
		/* display status chaned job and delete done job */
		while((changed_job = job_list_get_changed_job(&job_list))) {
			if(running_job_get_bg(changed_job)) {
				printf("[%d]", job_list_get_index(&job_list, changed_job));
				running_job_print(changed_job, stdout);
			}
			running_job_set_changed(changed_job, false);
			if(running_job_finished(changed_job)) {
				DEBUG_PRINT(("debug: job [%s] finished \n", running_job_get_cmd(changed_job)));
				job_list_remove(&job_list, changed_job);
			}
		}

		fflush(stdout);
		fflush(stderr);

		if (shell_is_interactive) {
			/* Please only print shell prompts when standard input is not a tty */
			char pwd[50];
			getcwd(pwd,50);
			fprintf(stdout, "\n%s\n", pwd);
			fprintf(stdout, "%d: ", ++line_num);
            fflush(stdout);
        }

		/* Clean up memory */
		tokens_destroy(tokens);
	}
	clear_jobs();
	return 0;
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
	//signal(SIGCHLD, sigchld_handle);
	signal(SIGTTIN, sigttin_handle);
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTERM, sigterm_handle);
}

/* Looks up the built-in command, if it exists. */
int lookup(char cmd[])
{
	for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
		if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0))
			return i;
	return -1;
}

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
	clear_jobs();
	job_list_destroy(&job_list);
	exit(0);
}

void sigint_handle(int sig)
{
	printf("^C\n");
	fprintf(stdout, "%d: ", ++line_num);
	fflush(stdout);
  /* send signal to all foreground process */
}

/* quit handler */
void sigquit_handle(int sig)
{
	printf("^\\\n");
	fprintf(stdout, "%d: ", ++line_num);
	fflush(stdout);
}


/* stop handler */
void sigtstp_handle(int sig)
{
	printf("^z\n");
	fprintf(stdout, "%d: ", ++line_num);
	fflush(stdout);
}

/* stop handler */
void sigttin_handle(int sig)
{
	printf("received SIGTTIN \n");
	fflush(stdout);
}

/* stop handler */
void sigttou_handle(int sig)
{
	printf("received SIGTTOUT \n");
	fflush(stdout);
}

void clear_jobs()
{
	running_job_t *job_p = job_list;
	while(job_p) {
		kill(-running_job_get_gid(job_p), SIGTERM);
		kill(-running_job_get_gid(job_p), SIGCONT);
		job_p = job_p -> next_job;
	}
	DEBUG_PRINT(("cleared !\n"));
}

void sigterm_handle(int sig)
{
	clear_jobs();
	SIG_DFL(SIGTERM);
}

void running_job_init(running_job_t * job) {
	if (job) {
		job -> pidv = NULL;
		job -> pidc = 0;
		job -> statusv = NULL;
		job -> cmd = NULL;
		job -> bg = false;
		job -> changed = false;
		job -> next_job = NULL;
	}
}

void running_job_set_gid (running_job_t * job, pid_t gid)
{
	job -> gid = gid;
}

pid_t running_job_get_gid (running_job_t * job)
{
	return job -> gid;
}

void running_job_add_pid (running_job_t * job, pid_t pid)
{
	job -> pidv = realloc(job -> pidv, sizeof(pid_t) * (job -> pidc + 1));
	job -> statusv = realloc(job -> statusv, sizeof(int) * (job -> pidc + 1));
	job -> pidv[job -> pidc] = pid;
	job -> statusv[job -> pidc] = -1;
	job -> pidc += 1;
}

int running_job_set_status (running_job_t * job, pid_t pid, int status)
{
	size_t n = 0;
	while (n < job->pidc && job->pidv[n] != pid) {
		++ n;
	}
	if(n < job->pidc) {
		job -> statusv[n] = status;
		return EXIT_SUCCESS;
	}
	return EXIT_FAILURE;
}

int running_job_get_status (running_job_t *job, pid_t pid, int *status)
{
	size_t n = 0;
	while (n < job->pidc && job->pidv[n] != pid) {
		++ n;
	}
	if(n < job->pidc) {
		*status = job -> statusv[n];
		return EXIT_SUCCESS;
	}
	return EXIT_FAILURE;
}

int running_job_get_status_n (running_job_t *job, size_t n, int *status)
{
	if(n < job->pidc) {
		*status = job -> statusv[n];
		return EXIT_SUCCESS;
	}
	return EXIT_FAILURE;
}

int running_job_set_cmd (running_job_t * job, char * cmd)
{
	if (job) {
		job -> cmd = strdup(cmd);
		return EXIT_SUCCESS;
	}
	return EXIT_FAILURE;
}

void running_job_set_bg (running_job_t * job, bool bg)
{
	if (job) {
		job -> bg = bg;
	}
}

bool running_job_get_bg (running_job_t * job)
{
	if(job)
		return job->bg;
	return false;
}

bool running_job_finished (running_job_t * job)
{
	size_t n = 0;
	int status;
	while (n < job->pidc) {
		status = job->statusv[n];
		if(!(WIFEXITED(status) || WIFSIGNALED(status) || WCOREDUMP(status)))
			return false;
		++n;
	}
	return true;
}

char *running_job_get_cmd(running_job_t * job)
{
	return (job -> cmd);
}

void running_job_print (running_job_t * job, FILE *fd)
{
	int status;
	running_job_get_status_n(job, 0, &status);
	fprintf(fd, " gid:%d \t%s \t%s\n", running_job_get_gid(job), status_to_string(status), running_job_get_cmd(job));
}

void job_list_apend(running_job_t **job_list, running_job_t *job)
{
	running_job_t **this_job = job_list;
	while (*this_job) {
		this_job = & ((*this_job) -> next_job);
	}
	*this_job = job;
}

bool job_list_is_empty(running_job_t **job_list)
{
	return *job_list == NULL;
}

int job_list_remove(running_job_t **job_list, running_job_t *job)
{
	running_job_t *this_job = *job_list;
	running_job_t *last_job = this_job;
	if(*job_list == job) {
		*job_list = (*job_list) -> next_job;
		free(this_job);
		return EXIT_SUCCESS;
	}
	while (this_job && this_job != job) {
		last_job = this_job;
		this_job = this_job -> next_job;
	}
	if (this_job) {
		last_job -> next_job = this_job -> next_job;
		free(this_job);
		return EXIT_SUCCESS;
	}
	return EXIT_FAILURE;
}

void job_list_destroy (running_job_t **job_list_p)
{
	running_job_t *this_job_p = *job_list_p;
	running_job_t *next_job_p;

	while(this_job_p) {
		next_job_p = this_job_p -> next_job;
		free(this_job_p);
		this_job_p = next_job_p;
	}
	*job_list_p = NULL;
}

running_job_t *job_list_get_fg_job(running_job_t **job_list_p)
{
	while (*job_list_p) {
		if(!((*job_list_p)->bg))
			return *job_list_p;
		job_list_p = &((*job_list_p)->next_job);
	}
	return NULL;
}

running_job_t *job_list_get_recent(running_job_t **job_list_p)
{
	return *job_list_p;
}

int job_list_get_index(running_job_t **job_list_p, running_job_t *job_p)
{
	int n = 0;
	while (*job_list_p) {
		if((*job_list_p) == job_p)
			return n;
		job_list_p = &((*job_list_p)->next_job);
		++n;
	}
	return -1;
}

int job_list_update_recent(running_job_t **job_list_p, running_job_t *job_p)
{
	running_job_t *this_job = *job_list_p;
	running_job_t *last_job = this_job;
	if(job_list == job_p) {
		return EXIT_SUCCESS;
	}
	while (this_job && this_job != job_p) {
		last_job = this_job;
		this_job = this_job -> next_job;
	}
	if (this_job) {
		last_job -> next_job = this_job -> next_job;
		this_job -> next_job = *job_list_p;
		*job_list_p = this_job;
		return EXIT_SUCCESS;
	}
	return EXIT_FAILURE;
}

char * status_to_string(int status)
{
	static char buf[50]="\0";
	if(WIFEXITED(status)) {
		sprintf(buf,"exited with %d", WEXITSTATUS(status));
	}
	if(WIFSIGNALED(status)) {
		sprintf(buf,"terminated by signal %d", WTERMSIG(status));
	}
	if(WIFSTOPPED(status)) {
		sprintf(buf,"stoped by signal %d", WSTOPSIG(status));
	}
	if(WIFCONTINUED(status)) {
		sprintf(buf, "continued");
	}
	if(WCOREDUMP(status)) {
		sprintf(buf, "core dumped %d", status);
	}
	return buf;
}

void job_list_print(running_job_t **job_list, FILE *fd)
{
	int n = 0;
	if(*job_list == NULL)
		fprintf(fd, "no jobs! \n");
	while(*job_list) {
		fprintf(fd, "[%d] ", n++);
		running_job_print(*job_list, fd);
		job_list = &((*job_list)->next_job);
	}
}

running_job_t *job_list_get_job_by_gid(running_job_t **job_list_p, pid_t gid)
{
	while (*job_list_p) {
		if((*job_list_p)->gid == gid)
			return *job_list_p;
		job_list_p = &((*job_list_p)->next_job);
	}
	return NULL;
}

running_job_t *job_list_get_job_n(running_job_t **job_list_p, int n)
{
	running_job_t *job_p = *job_list_p;
	int i = 0;
	while(job_p) {
		if(i == n)
			return job_p;
		job_p = job_p -> next_job;
	}
	return NULL;
}

running_job_t *job_list_get_job_by_pid(running_job_t **job_list_p, pid_t pid)
{
	while (*job_list_p) {
		for(size_t n=0; n < (*job_list_p)->pidc; ++n )
		{
			if((*job_list_p)->pidv[n] == pid)
				return *job_list_p;
		}
		job_list_p = &((*job_list_p)->next_job);
	}
	return NULL;
}

running_job_t *job_list_get_changed_job(running_job_t **job_list_p)
{
	while (*job_list_p) {
		if((*job_list_p)->changed == true)
			return *job_list_p;
		job_list_p = &((*job_list_p)->next_job);
	}
	return NULL;
}

void running_job_set_changed(running_job_t *job_p, bool changed)
{
	job_p -> changed = changed;
}

running_job_t *job_status_handle(running_job_t **job_list_p, pid_t rpid, int status)
{
	running_job_t *this_job;

	if(rpid == -1) {
		#ifdef DEBUG
        perror("debug: waitpid");
		#endif
        return NULL;
    } else if(rpid == 0) {
		#ifdef DEBUG
        printf("debug: no child changed status\n");
		#endif
        return NULL;
    }

	this_job = job_list_get_job_by_pid(job_list_p, rpid);

	running_job_set_status(this_job, rpid, status);
	if(running_job_get_bg(this_job))
		job_list_update_recent(job_list_p, this_job);
	running_job_set_changed(this_job, true);

	#ifdef DEBUG
	if(this_job)
		printf("debug: get one running job %s \n", running_job_get_cmd(this_job));
	else
		printf("debug: error : untraced job, pid=%d\n", rpid);
	#endif

	if (this_job) {
		DEBUG_PRINT(("debug: child %d ", rpid));
		if(WIFEXITED(status)) {
			DEBUG_PRINT(("exited with %d \n", WEXITSTATUS(status)));
		} else if (WIFSIGNALED(status)) {
			DEBUG_PRINT(("terminated by signal %d\n", WTERMSIG(status)));
		} else if (WCOREDUMP(status)) {
			DEBUG_PRINT(("core dumped\n"));
		} else if (WIFSTOPPED(status)) {
			running_job_set_bg(this_job, true);
			DEBUG_PRINT(("stoped by signal %d \n", WSTOPSIG(status)));
		} else if (WIFCONTINUED(status)) {
			DEBUG_PRINT(("continued \n"));
		}
	}

	fflush(stdout);

	return this_job;
}

void sigchld_handle(int sig)
{
	DEBUG_PRINT(("received sigchld signal\n"));
	//wait_for_job(&job_list);
}

void wait_for_job(running_job_t **job_list_p)
{
	int status;
    pid_t rpid;
	running_job_t * changed_job = NULL;
	running_job_t * fg_job = NULL;

	DEBUG_PRINT(("debug: in wait_for_fg_job\n"));

	/* wait untill all foreground job finished */
	while ((fg_job = job_list_get_fg_job(job_list_p))) {

		DEBUG_PRINT(("debug: set fg to child group\n"));
		if(tcsetpgrp(STDIN_FILENO, running_job_get_gid(fg_job)) < 0) {
			perror("set foreground");
			exit(EXIT_FAILURE);
		}
		kill(-running_job_get_gid(fg_job), SIGCONT);

		rpid = waitpid(-1, &status, WCONTINUED | WUNTRACED);
		changed_job = job_status_handle(job_list_p, rpid, status);
		if(running_job_finished(fg_job))
			job_list_remove(job_list_p, fg_job);
	}

	if(tcgetpgrp(0) != getpgid(0)) {
		/* resume to foreground */
		if(tcsetpgrp(STDIN_FILENO, getpgid(0)) < 0) {
			perror("parent resume foreground");
			exit(EXIT_FAILURE);
		}
	}

	do {
		rpid = waitpid(-1, &status, WNOHANG| WCONTINUED | WUNTRACED);
		changed_job = job_status_handle(job_list_p, rpid, status);
	} while (changed_job);
}


int cmd_jobs(struct tokens *tokens)
{
	job_list_print(&job_list, stdout);
	return EXIT_SUCCESS;
}

int cmd_fg(struct tokens *tokens)
{
	running_job_t *fg_job;
	if(tokens_get_length(tokens) < 2) {
		fg_job = job_list_get_recent(&job_list);
		if(fg_job) {
			running_job_set_bg(fg_job, false);
			return EXIT_SUCCESS;
		}
		else {
			printf ("no job to run\n");
		}
		return EXIT_FAILURE;
	} else if(tokens_get_length(tokens) == 2){
		int n = atoi(tokens_get_token(tokens, 1));
		fg_job = job_list_get_job_n(&job_list, n);
	} else if(tokens_get_length(tokens) == 3) {
		int pid = atoi(tokens_get_token(tokens, 2));
		fg_job = job_list_get_job_by_pid(&job_list, pid);
	} else {
		printf("syntex error\n");
	}
	if(fg_job) {
		running_job_set_bg(fg_job, false);
		return EXIT_SUCCESS;
	}
	else {
		printf ("no such job\n");
	}
	return EXIT_FAILURE;
}

int cmd_bg(struct tokens *tokens)
{
	running_job_t *bg_job;
	if(tokens_get_length(tokens) < 2) {
		bg_job = job_list_get_recent(&job_list);
		if(bg_job) {
			kill(-running_job_get_gid(bg_job), SIGCONT);
			return EXIT_SUCCESS;
		}
		else {
			printf ("no job to run\n");
		}
		return EXIT_FAILURE;
	} else if(tokens_get_length(tokens) == 2){
		int n = atoi(tokens_get_token(tokens, 1));
		bg_job = job_list_get_job_n(&job_list, n);
	} else if(tokens_get_length(tokens) == 3) {
		int pid = atoi(tokens_get_token(tokens, 2));
		bg_job = job_list_get_job_by_pid(&job_list, pid);
	} else {
		printf("syntex error\n");
	}
	if(bg_job) {
		kill(-running_job_get_gid(bg_job), SIGCONT);
		return EXIT_SUCCESS;
	}
	else {
		printf ("no such job\n");
	}
	return EXIT_FAILURE;
}

int cmd_wait(struct tokens *tokens)
{
	running_job_t *job_p = job_list;
	pid_t rpid;
	int status;
	while(job_p) {
		for(size_t n=0; n<job_p->pidc; n++) {
			do {
				rpid = waitpid(job_p->pidv[n], &status, WCONTINUED | WUNTRACED);
				job_status_handle(&job_list, rpid, status);
			} while(!(WIFEXITED(status) || WIFSIGNALED(status) || WCOREDUMP(status) || WIFSTOPPED(status) || (rpid<=0)) );
		}
		job_p = job_p -> next_job;
	}
	return EXIT_SUCCESS;
}

int cmd_cd(struct tokens *tokens)
{
	if(chdir(tokens_get_token(tokens, 1)) < 0){
		perror("cd:");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
