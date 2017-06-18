#include <stdio.h>
#include <sys/types.h>

int main()
{
	pid_t pid = fork();
	int status;

	printf("Hello World: %d\n", pid);

	if (pid == 0) {
		printf("I am in child process\n");
	} else if (pid == -1) {
		printf("no child process creaded\n");
	} else {
        wait (&status);
		printf("I am in parent process, has a child process %d\n", pid);
	}
}
