#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <sys/mman.h>
#include <malloc.h>
#include <signal.h>
#include <sys/ucontext.h>
#include <stdint.h>

void *new_pos;
long pagesize;
void *addr;

static void
handler(int sig, siginfo_t *si, void *context)
{
    printf("Got SIGSEGV at address: 0x%lx\n", (long) si->si_addr);
	//ucontext_t *uc = (ucontext_t *)context;
    //uc->uc_mcontext.gregs[REG_RIP] = addr;
	if (mprotect(new_pos, pagesize, PROT_READ|PROT_WRITE) == -1) {
		perror("mprotect");
		exit(EXIT_FAILURE);
	}

}

int main () {
	pagesize = sysconf(_SC_PAGE_SIZE);
	struct sigaction sa;
	addr = &&skip_to;

   	sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = handler;
    if (sigaction(SIGSEGV, &sa, NULL) == -1){
		perror("sigaction");
		exit(EXIT_FAILURE);
	}

	if (pagesize == -1) {
		perror("get pagesize");
		exit(EXIT_FAILURE);
	}
	#define NEXT_ALIGN(start) ((long)(start+pagesize+1)&(~(pagesize-1)))

	void *start = sbrk(0);
	if(brk((void*)NEXT_ALIGN(start))) {
		perror("brk");
		exit(EXIT_FAILURE);
	}
	new_pos = sbrk(0);
	if(brk(new_pos+pagesize)) {
		perror("brk");
		exit(EXIT_FAILURE);
	}

	printf("pagesize = 0x%lx, start = 0x%lx, new_pos = 0x%lx, alignment = 0x%lx \n", pagesize, (long)start, (long)new_pos, ((long)(start+pagesize+1))&(~(pagesize-1)));

	if (mprotect(new_pos, pagesize, PROT_READ) == -1) {
		perror("mprotect");
		exit(EXIT_FAILURE);
	}

	printf("*new_pos = %d, try to write to it(100)\n", *((int *)new_pos));
	*((int *)new_pos) = 100;

skip_to:
	printf("*new_pos = %d, try to write to it again \n", *((int *)new_pos));
	if (mprotect(new_pos, pagesize, PROT_READ|PROT_WRITE) == -1) {
		perror("mprotect");
		exit(EXIT_FAILURE);
	}
	*((int *)new_pos) = 100;
	printf("*new_pos = %d \n", *((int *)new_pos));
}
