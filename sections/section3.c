#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	pid_t pid;
    int status;
	int newfd;
	char *cmd[] = { "/bin/ls", "-al", "/", 0 };

    //put fork into if doesn't work!
    pid = fork();
	if (pid < 0) {
		perror("fork error\n");
		exit(1);
	}

    //printf ("pid = %d\n", pid);
    //fflush (stdout);

	if (pid == 0) {
		if (argc != 2) {
			fprintf(stderr, "usage: %s output_file\n", argv[0]);
			exit(1);
		}
		if ((newfd = open(argv[1], O_CREAT | O_TRUNC | O_WRONLY, 0644)) < 0) {
			perror(argv[1]); /* open failed */
			exit(1);
		}
		printf("writing output of the command %s to \"%s\"\n", cmd[0], argv[1]);
		dup2(newfd, 1);
		execvp(cmd[0], cmd);
		perror(cmd[0]); /* execvp failed */

        if(close(newfd))
            printf("close error\n");

		exit(0);
	}
	wait(&status);
	printf("all done\n");
	exit(1);
}
