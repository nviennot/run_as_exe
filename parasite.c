/* We include dynlink.c to have access to map_library() */
#undef _GNU_SOURCE /* dynlink.c defines it */
 /* disable destructors */
#define __libc_exit_fini unused___libc_exit_fini
#include "musl/ldso/dynlink.c"

 /* Not sure why __must_check is not defined in infect-rpc.h */
#define __must_check
#include <infect-rpc.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

/*
 * These are musl symbols. They end up as COM symbols. But compel doesn't
 * support them. We declare them here with a value, forcing them to be
 * allocated in the .data section.
 */
size_t __sysinfo = 0;
struct __libc __libc = {};
int __malloc_replaced = 0;
size_t __hwcap = 0;
volatile int __eintr_valid_flag = 0;
volatile int __thread_list_lock = 0;
/* This one is WEAK UNDEFINED. It shouldn't be used */
const size_t _DYNAMIC[0] = {};

/*
 * Stubs for std compel plugin.
 */
int parasite_trap_cmd(int cmd, void *args) { return 0; }
void parasite_cleanup(void) { }

/* Adapted from musl */
static void decode_vec_ptr(size_t *v, size_t **a, size_t cnt)
{
	size_t i;
	for (i=0; i<cnt; i++) a[i] = NULL;
	for (; v[0]; v+=2) if (v[0]-1<cnt-1) {
		a[v[0]] = &v[1];
	}
}

struct bootstrap_args {
	size_t *sp;
	size_t *ip;
};

int parasite_daemon_cmd(int cmd, void *_args)
{
	struct bootstrap_args *args = _args;

	/*
	 * Pick up what the kernel has prepared for us.
	 * We do what an ELF loader would normally do
	 */
	int argc = *args->sp;
	char **argv = (void *)(args->sp+1);
	char **envp = argv+argc+1;
	size_t *auxv, *aux[AUX_CNT];
	int i;

	for (i=argc+1; argv[i]; i++);
	auxv = (void *)(argv+i+1);

	/* Rudimentary libc init */
	__progname = "parasite";
	__environ = envp;

	/* Initialize TLS (where errno is), taken from __dls2b() in musl/ldso/dynlink.c */
	libc.tls_size = sizeof builtin_tls;
	libc.tls_align = tls_align;
	if (__init_tp(__copy_tls((void *)builtin_tls)) < 0) {
		a_crash();
	}

	/* Load the target ELF */
	int fd = open(argv[0], O_RDONLY);
	if (fd < 0)
		errx(1, "Can't open %s", argv[0]);

	struct dso dso;
	Ehdr *ehdr = map_library(fd, &dso);
	close(fd);
	if (!ehdr)
		err(1, "Can't load ELF %s", argv[0]);

	/* Change the aux vector to point to the new ELF */
	decode_vec_ptr(auxv, aux, AUX_CNT);
	*aux[AT_BASE] = (size_t)dso.base;
	*aux[AT_PHDR] = (size_t)dso.phdr;
	*aux[AT_ENTRY] = (size_t)laddr(&dso, ehdr->e_entry);
	*aux[AT_PHENT] = dso.phentsize;
	*aux[AT_PHNUM] = ehdr->e_phnum;

	/* TODO unmap the old exe, release the compel socket and memfd */

	/* Continue execution to the standard /lib64/ld-linux-x86-64.so.2 loader */
	CRTJMP(args->ip, args->sp);
	return 0;
}
