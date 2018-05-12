#include <stdio.h>
#include "mm_alloc.h"

int main() {
	void *ptr = mm_malloc(100);
	sprintf(ptr, "write to location %lx \n", (long)ptr);
	printf("%s\n", (char *)ptr);
	ptr = mm_realloc(ptr, 50);
	sprintf(ptr, "write to location %lx \n", (long)ptr);
	printf("%s\n", (char *)ptr);
	ptr = mm_realloc(ptr, 100);
	sprintf(ptr, "write to location %lx \n", (long)ptr);
	printf("%s\n", (char *)ptr);
	void *ptr2 = mm_malloc(200);
	sprintf(ptr2, "write to ptr2 @ %lx \n", (long)ptr2);
	printf("%s\n", (char *)ptr2);
	printf("freeing %lx\n", (long)ptr);
	mm_free(ptr);
	printf("freeing %lx\n", (long)ptr2);
	mm_free(ptr2);
}
