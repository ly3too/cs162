#include <stdio.h>
#include <sys/resource.h>

int main() {
    struct rlimit lim;
    getrlimit(RLIMIT_STACK,&lim);
    printf("stack size: %ld\n", lim.rlim_cur);
    //getrlimit(RLIMIT_NPROC ,&lim); //dosen't exist in my computer
    getrlimit(RLIMIT_CORE ,&lim);
    printf("process limit: %ld\n", lim.rlim_cur);
    getrlimit(RLIMIT_NOFILE ,&lim);
    printf("max file descriptors: %ld\n", lim.rlim_cur);
    return 0;
}
