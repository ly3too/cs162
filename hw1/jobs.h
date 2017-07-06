#pragma once

#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <fcntl.h>
#include "tokenizer.h"
#include "get_full_path.h"

typedef struct process process_t;

typedef struct job job_t;

typedef struct job_vector job_vector_t;

job_vector_t *get_job_vector(struct tokens *tkn);

char** process_get_argv(process_t *proc);
char* process_get_cmd(process_t *proc);
int process_get_input(process_t *proc);
int process_get_output(process_t *proc);
bool process_use_pipe(process_t *proc);

size_t job_get_process_size(job_t *job_p);
process_t * job_get_process(job_t *job_p, size_t n);
char *job_get_cmd_line(job_t *job_p);
void job_set_gid(job_t *job_p, pid_t gid);
pid_t job_get_gid(job_t *job_p);
bool job_run_background(job_t *job_p);

void job_vector_clean_up(job_vector_t *job_vect);
size_t job_vector_get_length (job_vector_t *job_vect) ;
job_t * job_vector_get_job(job_vector_t *job_vect, size_t n);
