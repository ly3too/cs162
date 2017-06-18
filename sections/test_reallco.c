#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

void printTenNumbers(int *arr)
{
	int i;

	printf("\n");
	for (i = 0; i < 10; i++) {
		printf("%d ", arr[i]);
	}
	printf("\n i == %d", i);
//    fflush(stdout);
	exit(0);
}
int main()
{
	int *arr, i;
    pid_t pid;

	arr = (int*)malloc(sizeof(int));
	arr[0] = 0;
	for (i = 1; i < 10; i++) {
		arr = (int*)realloc( arr, (i + 1) * sizeof(int));
		arr[i] = i;
		if (i == 7) {
			pid = fork();
			if (pid == 0) {
				printTenNumbers(arr);
			}
            else {
                wait(pid);
            }
		}
	}
	printTenNumbers(arr);
    if (pid == 0) {
        printf("child pocess exit\n");
    } else {
        printf("parent pocess exit\n");
    }
    exit(0);
}
