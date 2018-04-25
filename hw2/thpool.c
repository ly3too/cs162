/* ********************************
 * Author:      ly3too

 * Description:
 ********************************/
#include "thpool.h"
#include <semaphore.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static volatile int keepalive; // enable main loop
static volatile int num_threads; // number of worker threads
pthread_t *p_threads;  // record threads' ID in pool
sem_t has_jobs, job_slots; // producer consumer model
sem_t finished_jobque; // jobs in jobque all done;

typedef struct job{
    struct job *next;
    void (*func) (void *arg);
    void * arg;
} job_t;

typedef struct {
    pthread_mutex_t lock;
    job_t * head;
    job_t * tail;
    int len;
} jobque_t;

jobque_t jobque;

void free_job(job_t * job) {
    DEBUG_FUNC(printf, ("freeing job!\n"));
    free(job -> arg);
    free(job);
}

int jobque_init(jobque_t* p_que) {
    DEBUG_FUNC(printf, ("enter jobque init!\n"));
    if (pthread_mutex_init(&(p_que -> lock), NULL)) {
        perror("mutex initialize failed.");
        return -1;
    }
    pthread_mutex_lock(&(p_que -> lock));
    p_que -> head = NULL;
    p_que -> tail = NULL;
    p_que -> len = 0;
    pthread_mutex_unlock(&(p_que -> lock));
    return 0;
}

int jobque_push(jobque_t* p_que, job_t *p_job) {
    DEBUG_FUNC(printf, ("enter jobque push!\n"));
    pthread_mutex_lock(&(p_que -> lock));
        if (p_que -> len == 0) {
            p_que -> head = p_que -> tail = p_job;
        } else {
            (p_que -> tail) -> next = p_job;
            p_job -> next = NULL;
            p_que -> tail = p_job;
        }
        ++ (p_que -> len);
    pthread_mutex_unlock(&(p_que -> lock));
    return 0;
}

job_t *jobque_pop(jobque_t* p_que) {
    DEBUG_FUNC(printf, ("enter jobque_pop!\n"));
    job_t * new_job = NULL;
    pthread_mutex_lock(&(p_que -> lock));

    if (p_que -> len > 0) {
        new_job = p_que -> head;
        p_que -> head = new_job -> next;
        new_job -> next = NULL;
        -- p_que -> len;
    }

    pthread_mutex_unlock(&(p_que -> lock));
    return new_job;
}

int jobque_size(jobque_t* p_que) {
    int size = 0;
    pthread_mutex_lock(&(p_que -> lock));
    size = p_que -> len;
    pthread_mutex_unlock(&(p_que -> lock));
    return size;
}

void jobque_destroy(jobque_t* p_que) {
    pthread_mutex_lock(&(p_que -> lock));

    while (p_que -> head) {
        job_t *job = p_que -> head;
        p_que -> head = p_que -> head -> next;
        free_job(job);
    }
    p_que -> len = 0;

    pthread_mutex_unlock(&(p_que -> lock));
    pthread_mutex_destroy(&(p_que -> lock));
}

/* main worker threads */
void* worker (void *arg) {
    DEBUG_FUNC(printf, ("enter worker: %lu.\n", pthread_self()));
    while (keepalive) {
        sem_wait(&has_jobs);
        job_t *new_job = jobque_pop(&jobque);
        DEBUG_FUNC(printf, ("thread %lu gets a job .\n", pthread_self()));
        if (new_job != NULL) {
            sem_post(&job_slots);
            if (new_job -> func)
                new_job->func(new_job->arg);
            free_job(new_job);
            DEBUG_FUNC(printf, ("thread %lu finished a job .\n", pthread_self()));
        } else {
            sem_post(&finished_jobque);
            DEBUG_FUNC(printf, ("thread %lu did nothing.\n", pthread_self()));
        }
    }

    DEBUG_FUNC(printf, ("thread %lu exit.\n", pthread_self()));
    return NULL;
}

/* add job to jobque, arg will be freed after task finshed */
int thpool_add_job(void (*func) (void *arg), void *arg) {
    DEBUG_FUNC(printf, ("enter thpool add job\n"));
    sem_wait(&job_slots);
    job_t *new_job = (job_t *)malloc(sizeof(job_t));
    if (new_job == NULL) {
        perror("cannot creat job");
        return -1;
    }

    new_job -> func = func;
    new_job -> arg = arg;
    new_job -> next = NULL;
    if (jobque_push(&jobque, new_job)){
        perror("cannot add new job");
        free_job(new_job);
        return -1;
    }
    sem_post(&has_jobs);

    return 0;
}


/* return -1 if anny error */
int thpool_init(int num_th, int que_size) {
    if (que_size <= 0 || num_th <= 0) {
        perror("que size and number of threads should be positive value");
        return -1;
    }
    DEBUG_FUNC(printf, ("enter thpool_init!\n"));

    keepalive = 1;
    num_threads = num_th;
    sem_init(&has_jobs, 0, 0);
    sem_init(&finished_jobque, 0, 0);
    sem_init(&job_slots, 0, que_size);
    p_threads = (pthread_t *)malloc(num_th * sizeof(pthread_t));
    if (p_threads == NULL) {
        perror("cannot creat threads id array");
        return -1;
    }

    if (jobque_init(&jobque) < 0) {
        perror("jobque init failed");
        jobque_destroy(&jobque);
        return -1;
    }
    DEBUG_FUNC(printf, ("jobque init ok!\n"));

    /*creating process*/
    for (int i=0; i<num_th; ++i) {
        if (pthread_create(&(p_threads[i]), NULL, &worker, NULL) != 0) {
            perror("creating pthread failed");
            return -1;
        }
    }

    DEBUG_FUNC(printf, ("thread pool init ok!\n"));
    return 0;
}

/* wait jobs to be finished in jobque and free thread pool */
void thpool_destroy() {
    DEBUG_FUNC(printf, ("enter thpool_destroy!\n"));
    // DEBUG_FUNC(sleep, (10));
    void* retval;
    sem_post(&has_jobs);
    DEBUG_FUNC(printf, ("waiting for jobques to be cleaned!\n"));
    sem_wait(&finished_jobque);
    DEBUG_FUNC(printf, ("all jobs finished!\n"));

    keepalive = 0;
    // wake up all threads again to let them exit main loop
    for (int i=0; i<num_threads; ++i) {
        sem_post(&has_jobs);
    }
    for (int i=0; i<num_threads; ++i) {
        pthread_join(p_threads[i], &retval);
    }
    DEBUG_FUNC(printf, ("all process joined!\n"));

    jobque_destroy(&jobque);
    free(p_threads);

    sem_destroy(&has_jobs);
    sem_destroy(&job_slots);
    sem_destroy(&finished_jobque);
}
