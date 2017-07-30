#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

void identify() {
    pid_t pid = getpid();
    printf("My pid is %d\n", pid);
    // return NULL;
}

void main() {
    pthread_t thread;
    pthread_create(&thread, NULL, (void *) &identify, NULL);
    identify();
}
