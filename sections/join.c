#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>

void *helper(void *arg) {
    printf("HELPER\n");
    return NULL;
}

void main() {
    pthread_t thread;
    pthread_create(&thread, NULL, &helper, NULL);
    pthread_join(thread, NULL);
    printf("MAIN\n");
    exit(0);
}
