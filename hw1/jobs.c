#include "jobs.h"

/*process*/
struct process {
	char * cmd;
	char ** argv;
	size_t argc;
	bool use_pipe;
	int input;
	int output;
};

static void process_argv_init(process_t *proc)
{
	proc->argc = 0;
	proc->argv = NULL;
}

static void process_init(process_t *proc)
{
	proc -> cmd = NULL;
	process_argv_init(proc);
	proc -> use_pipe = false;
	proc -> input = STDIN_FILENO;
	proc -> output = STDOUT_FILENO;
}

static void process_set_cmd(process_t *proc, char *cmd)
{
	proc -> cmd = cmd;
}

static void process_push_arg(process_t *proc, char * arg)
{
	proc->argc += 1;
	proc->argv = (char**) realloc(proc->argv, sizeof(char*)*(proc->argc+1));
	proc->argv[proc->argc-1] = strdup(arg);
	proc->argv[proc->argc] = NULL;
}

static void process_set_input(process_t *proc, int fd)
{
	if (proc -> input != STDIN_FILENO)
		close(proc -> input);
	proc -> input = fd;
}

static void process_set_output(process_t *proc, int fd)
{
	if (proc -> output != STDOUT_FILENO)
		close(proc -> output);
	proc -> output = fd;
}

static void process_clean_up(process_t *proc)
{
	free (proc -> cmd);
	for (int i=0; i < proc->argc; i++) {
		free(proc -> argv[i]);
	}
	free(proc -> argv);
	free(proc);
}

char** process_get_argv(process_t *proc)
{
	return proc -> argv;
}

char* process_get_cmd(process_t *proc)
{
	return proc -> cmd;
}

int process_get_input(process_t *proc)
{
	return proc -> input;
}

int process_get_output(process_t *proc)
{
	return proc -> output;
}

bool process_use_pipe(process_t *proc)
{
	return proc -> use_pipe;
}

static void process_set_use_pipe(process_t *proc)
{
	proc -> use_pipe = true;
}

struct job {
	process_t **processv;
	size_t size;
	char *cmd_line;
	pid_t gid;
	bool bg;
};

static void job_process_vecotr_push (job_t *job_p, process_t *proc)
{
	job_p -> processv = (process_t **) realloc(job_p -> processv, (job_p->size+1)*sizeof(process_t*));
	job_p -> processv[job_p->size] = proc;
	job_p -> size ++;
}

static void job_process_vecotr_init(job_t *job_p)
{
	job_p -> size = 0;
	job_p -> processv = NULL;
}

static void job_init(job_t *job_p)
{
	job_process_vecotr_init(job_p);
	job_p -> gid = getpgid(0);
	job_p -> bg = false;
	job_p -> cmd_line = NULL;
}

static void job_append_arg(job_t *job_p, const char *arg)
{
	if (job_p -> cmd_line == NULL)
		job_p -> cmd_line = strdup(arg);
	else {
		job_p -> cmd_line = (char *)realloc(job_p -> cmd_line, (strlen(job_p -> cmd_line)+strlen(arg)+2)*sizeof(char));
		strcat(job_p -> cmd_line," ");
		strcat(job_p -> cmd_line, arg);
	}
}

size_t job_get_process_size(job_t *job_p)
{
	return job_p -> size;
}

process_t * job_get_process(job_t *job_p, size_t n)
{
	return job_p -> processv[n];
}

static void job_set_bg_true(job_t *job_p)
{
	job_p -> bg = true;
}

static void job_clean_up(job_t *job_p)
{
	free (job_p -> cmd_line);
	for(size_t i=0; i < job_p->size; i++) {
		process_clean_up (job_p -> processv[i]);
	}
	free (job_p -> processv);
	free (job_p);
}

char *job_get_cmd_line(job_t *job_p)
{
	return job_p -> cmd_line;
}

void job_set_gid(job_t *job_p, pid_t gid)
{
	job_p -> gid = gid;
}

pid_t job_get_gid(job_t *job_p)
{
	return job_p -> gid;
}

bool job_run_background(job_t *job_p)
{
	return job_p -> bg;
}


struct job_vector {
	job_t **jobv;
	size_t size;
};

static void job_vector_push(job_vector_t *job_vect, job_t *job)
{
	job_vect->jobv = (job_t ** )realloc (job_vect->jobv, (job_vect -> size + 1) * sizeof(job_t*));
	job_vect->jobv[job_vect -> size] = job;
	job_vect -> size ++;
}

static void job_vector_init(job_vector_t *job_vect)
{
	job_vect -> size = 0;
	job_vect -> jobv = NULL;
}

void job_vector_clean_up(job_vector_t *job_vect)
{
	if (!job_vect)
		return;
	for (int i=0; i < job_vect -> size; i++) {
		job_clean_up (job_vect -> jobv[i]);
	}
	if(job_vect -> jobv)
		free(job_vect -> jobv);
	free(job_vect);
}

size_t job_vector_get_length (job_vector_t *job_vect)
{
	return job_vect -> size;
}

job_t * job_vector_get_job(job_vector_t *job_vect, size_t n)
{
	return job_vect->jobv[n];
}

enum mode_e {
	MODE_NORMAL = 0,
	MODE_PIPE,
	MODE_IN_FILE,
	MODE_OUT_FILE_NEW,
	MODE_OUT_FILE_APEND,
	MODE_START_CMD,
	MODE_SYN_ERROR,
	MODE_CMD_ERROR
};

#ifdef DEBUG_GET_JOB_VECTOR
	static char* mode_argv[] = {"MODE_NORMAL", "MODE_PIPE", "MODE_IN_FILE",
	 "MODE_OUT_FILE_NEW", "MODE_OUT_FILE_APEND", "MODE_START_CMD", "MODE_SYN_ERROR", "MODE_CMD_ERROR"};
#endif

static void syn_error(char *tkn);

job_vector_t *get_job_vector (struct tokens *tkn)
{
	size_t size = tokens_get_length(tkn);
	enum mode_e mode = MODE_START_CMD;
	char * cmd;
	process_t *cur_process;
	job_t *cur_job = NULL;
	char *cur_wd;

	job_vector_t *job_vect = (job_vector_t *)malloc(sizeof(job_vect));
	job_vector_init(job_vect);

	#ifdef DEBUG
		printf("init_ok\n");
	#endif

	for (size_t i=0; i<size; ++i) {
		cur_wd = tokens_get_token(tkn, i);
		int pipefd[2];

		if (strcmp(cur_wd, "|") == 0) {
			/* creat pipe */
			if (mode == MODE_NORMAL) {
				/* ok to creat */
				mode = MODE_PIPE;
				//cur_process -> use_pipe = true;
				job_append_arg(cur_job, cur_wd);
				if(pipe(pipefd) != 0 ) {
					perror("creat pipe");
					exit(EXIT_FAILURE);
				}
				process_set_output(cur_process, pipefd[1]);
				process_set_use_pipe(cur_process);

			} else {
				mode = MODE_SYN_ERROR;
			}

		} else if (strcmp(cur_wd, "<") == 0) {
			/* specify input as a file */
			if (mode == MODE_NORMAL) {
				mode = MODE_IN_FILE;
				job_append_arg(cur_job, cur_wd);
			} else {
				mode = MODE_SYN_ERROR;
			}

		} else if (strcmp(cur_wd, ">") == 0) {
			/* output to file */
			if (mode == MODE_NORMAL) {
				mode = MODE_OUT_FILE_NEW;
				job_append_arg(cur_job, cur_wd);
			} else {
				mode = MODE_SYN_ERROR;
			}

		} else if (strcmp(cur_wd, ">>") == 0)  {
			/* apend to file */
			if (mode == MODE_NORMAL) {
				mode = MODE_OUT_FILE_APEND;
				job_append_arg(cur_job, cur_wd);
			} else {
				mode = MODE_SYN_ERROR;
			}

		} else if (strcmp(cur_wd, "&") == 0) {
			/* run in background, append this job to vector and creat new job*/
			if (mode == MODE_NORMAL) {
				mode = MODE_START_CMD;
				job_set_bg_true(cur_job);
				job_append_arg(cur_job, cur_wd);

			} else {
				mode = MODE_SYN_ERROR;
			}

		} else if (strcmp(cur_wd, "&&") == 0) {
			/* append this job to vector and creat new job*/
			if (mode == MODE_NORMAL) {
				mode = MODE_START_CMD;
				//job_append_arg(cur_job, cur_wd);

			} else {
				mode = MODE_SYN_ERROR;
			}

		} else {
			/* this is an argv or cmd */
			if (mode == MODE_START_CMD || mode == MODE_PIPE) {
				/* check exist of cmd*/
				int status = get_cmd(cur_wd, &cmd);
				if (status == -1) {
					fprintf(stderr, "-shell: %s: command not found\n", cur_wd);
					mode = MODE_CMD_ERROR;
				} else if (status == 1) {
					fprintf(stderr, "-shell: %s: is a directory\n", cur_wd);
					mode = MODE_CMD_ERROR;
				} else if (status == 0 && cmd != NULL) {
					/* valid cmd */

					/* creat a new process */
					cur_process = (process_t *) malloc (sizeof(process_t));
					process_init(cur_process);
					process_push_arg(cur_process, cur_wd);
					process_set_cmd(cur_process, cmd);

					/*creat a new job and add to jov_vect if at start*/
					if(mode == MODE_START_CMD) {
						cur_job = (job_t *) malloc (sizeof(job_t));
						job_init(cur_job);
						job_vector_push(job_vect, cur_job);
					} else {
						process_set_input(cur_process, pipefd[0]);
					}

					job_append_arg(cur_job, cur_wd);
					job_process_vecotr_push(cur_job, cur_process);

				} else {
					fprintf(stderr,"unknown get cmd error\n");
					mode = MODE_SYN_ERROR;
				}

			}

			int fd;
			switch (mode) {
				case MODE_PIPE :
					mode = MODE_NORMAL;
				break;

				case MODE_START_CMD:
					mode = MODE_NORMAL;
				break;

				case MODE_IN_FILE :
					/*cur_wd is the INPUT file name*/
					fd = open(cur_wd, O_RDONLY);
					if(fd < 0) {
						perror(cur_wd);
						mode = MODE_CMD_ERROR;
					}
					process_set_input(cur_process, fd);
					mode = MODE_NORMAL;
				break;

				case MODE_OUT_FILE_NEW:
					/* cur_wd is the output file name */
					fd = open(cur_wd, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
					if(fd < 0) {
						perror(cur_wd);
						mode = MODE_CMD_ERROR;
					}
					process_set_output(cur_process, fd);
					mode = MODE_NORMAL;
				break;

				case MODE_OUT_FILE_APEND:
				/* cur_wd is the output append file*/
					fd = open(cur_wd, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
					if(fd < 0) {
						perror(cur_wd);
						mode = MODE_CMD_ERROR;
					}
					process_set_output(cur_process, fd);
					mode = MODE_NORMAL;
				break;

				case MODE_NORMAL:
					/* this is an argument */
					process_push_arg(cur_process, cur_wd);
					job_append_arg(cur_job, cur_wd);
				break;

				default :
				break;
			}

		}

		#ifdef DEBUG_GET_JOB_VECTOR
			printf("cur_wd = %s , mode = %s \n", cur_wd, mode_argv[mode]);
			if (cur_job)
				printf ("cur_job = %s \n", cur_job -> cmd_line);
			else printf ("no cur_job created yet \n");
		#endif

		if(mode == MODE_SYN_ERROR || mode == MODE_CMD_ERROR)
			break;
	}

	/* check at the end of cmd_line */
	if(mode != MODE_NORMAL && mode != MODE_START_CMD) {
		if(mode == MODE_SYN_ERROR || mode != MODE_CMD_ERROR)
			syn_error(cur_wd);
		job_vector_clean_up(job_vect);
		return NULL;
	}

	/* debug display jobs */
	#ifdef DEBUG_GET_JOB_VECTOR
	for(int i=0; i<job_vect->size; i++) {
		printf ("get job [%d]: %s \n", i, job_vect->jobv[i]->cmd_line);
		for(int j=0; j<job_vect->jobv[i]->size; j++) {
			printf ("\t process[%d]: cmd = %s, argv = ", j, job_vect->jobv[i]->processv[j]->cmd);
			for(int k=0; k<job_vect->jobv[i]->processv[j]->argc; k++) {
				printf ("%s  ", job_vect->jobv[i]->processv[j]->argv[k]);
			}
			if(job_vect->jobv[i]->processv[j] -> use_pipe) {
				printf("|");
			}
			printf("\n");
		}
	}
	#endif

	return job_vect;
}

static void syn_error(char *tkn)
{
	fprintf(stderr, "-shell: syntax error near unexpected token '%s' \n", tkn);
}
