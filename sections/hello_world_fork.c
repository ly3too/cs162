#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <conio.h>
#include <sys/wait.h>

void sigint_handle(int sig);

int main()
{
	pid_t pid = fork();
	int status;

	printf("Hello World: %d\n", pid);

	if (pid == 0) {
		//setpgid(0, 0);
		printf("child pgid = %d\n", getpgid(0));

		for (int i=0; i<1000; i++) {
			printf("%d: I am in child process\n",i);
			//sleep(1);
			putchar(getch());
			fflush(stdout);
		}
	} else if (pid == -1) {
		printf("no child process creaded\n");
	} else {
		signal(SIGINT, sigint_handle);
		printf("parent pgid = %d\n", getpgid(0));
        //wait (&status);
		for (int i=0; i<1000; i++) {
			printf("%d I am in parent process, has a child process %d\n",i, pid);
			putchar(getch());
			fflush(stdout);
		}
	}
}

void sigint_handle(int sig)
{
	printf("^c exit\n");
    fflush(stdout);
	kill(0, SIGTERM);
}
