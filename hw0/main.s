	.file	"main.c"
	.def	__main;	.scl	2;	.type	32;	.endef
	.section .rdata,"dr"
.LC0:
	.ascii "stack size: %ld\12\0"
.LC1:
	.ascii "process limit: %ld\12\0"
.LC2:
	.ascii "max file descriptors: %ld\12\0"
	.text
	.globl	main
	.def	main;	.scl	2;	.type	32;	.endef
	.seh_proc	main
main:
	pushq	%rbx
	.seh_pushreg	%rbx
	subq	$48, %rsp
	.seh_stackalloc	48
	.seh_endprologue
	call	__main
	leaq	32(%rsp), %rbx
	movq	%rbx, %rdx
	movl	$3, %ecx
	call	getrlimit
	movq	32(%rsp), %rdx
	leaq	.LC0(%rip), %rcx
	call	printf
	movq	%rbx, %rdx
	movl	$4, %ecx
	call	getrlimit
	movq	32(%rsp), %rdx
	leaq	.LC1(%rip), %rcx
	call	printf
	movq	%rbx, %rdx
	movl	$5, %ecx
	call	getrlimit
	movq	32(%rsp), %rdx
	leaq	.LC2(%rip), %rcx
	call	printf
	movl	$0, %eax
	addq	$48, %rsp
	popq	%rbx
	ret
	.seh_endproc
	.ident	"GCC: (GNU) 5.4.0"
	.def	getrlimit;	.scl	2;	.type	32;	.endef
	.def	printf;	.scl	2;	.type	32;	.endef
