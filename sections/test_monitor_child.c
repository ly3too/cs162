#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdlib.h>

void sigint_handle(int sig);
void sigchld_handle(int sig);
void sigtstp_handle (int sig);

pid_t pid;

int main()
{
	pid = fork();
	int status;

	printf("Hello World: %d\n", pid);

	if (pid == 0) {
		setpgid(0, 0);
		printf("child pgid = %d\n", getpgid(0));

		for (int i=0; i<15; i++) {
			printf("%d: I am in child process\n",i);
			sleep(2);
		}
	} else if (pid == -1) {
		printf("no child process creaded\n");
	} else {
		signal(SIGINT, sigint_handle);
        signal(SIGCHLD, sigchld_handle);
        signal(SIGTSTP, sigtstp_handle);
		printf("parent pgid = %d\n", getpgid(0));
		printf("I am in parent process, has a child process %d\n", pid);
        while(1)
            sleep(1);
	}
}

void sigint_handle(int sig)
{
	printf("^C\n");
    tcsetpgrp(STDIN_FILENO, pid);
    fflush(stdout);
}

void sigchld_handle(int sig)
{
    int status;
    pid_t rpid = waitpid(-1, &status, WNOHANG | WCONTINUED | WUNTRACED);
    if(rpid == -1) {
        perror("waitpid");
        exit(EXIT_FAILURE);
    } else if(rpid == 0) {
        printf("no child changed status\n");
        return;
    } else {
        printf("child %d ", rpid);
        if(WIFEXITED(status)) {
            printf("exited with %d \n", WEXITSTATUS(status));
            exit(EXIT_SUCCESS);
        }
        if(WIFSIGNALED(status)) {
            printf("terminated by signal %d\n", WTERMSIG(status));
            exit(EXIT_SUCCESS);
        }
        if(WIFSTOPPED(status))
            printf("stoped by signal %d \n", WSTOPSIG(status));
        if(WIFCONTINUED(status))
            printf("continued \n");
        fflush(stdout);
    }
}

void sigtstp_handle (int sig)
{
    kill(pid, SIGTSTP);
}
