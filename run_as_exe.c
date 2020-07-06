#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <err.h>
#include <sys/ptrace.h>
#include <sys/prctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>

#include <compel/infect.h>
#include <compel/log.h>
#include <compel/infect-rpc.h>
#include <compel/infect-util.h>
#include "compel/include/infect-priv.h"

#include "parasite.h"

struct bootstrap_args {
	size_t *sp;
	size_t *ip;
};

static void print_vmsg(unsigned int lvl, const char *fmt, va_list parms)
{
	printf("\tLC%u: ", lvl);
	vprintf(fmt, parms);
}

static void do_infection(int pid, int parent_sync_fd, int child_sync_fd)
{
	int state;
	struct parasite_ctl *ctl;
	struct infect_ctx *ictx;
	char buf[1];
	int status;

	compel_log_init(print_vmsg, COMPEL_LOG_WARN);

	read(child_sync_fd, buf, 1);
	close(child_sync_fd);

	state = compel_stop_task(pid);
	if (state < 0)
		err(1, "Can't stop task");

	if (ptrace(PTRACE_SETOPTIONS, pid, NULL, PTRACE_O_TRACEEXEC))
		err(1, "Can't trap execve");

	// Let parent execute execve(), and trap
	if (write(parent_sync_fd, buf, 1) != 1)
		err(1, "Can't sync with parent");
	ptrace(PTRACE_CONT, pid, NULL, NULL);
	if (waitpid(pid, &status, 0) != pid ||
	    (status >> 8) != (SIGTRAP | PTRACE_EVENT_EXEC << 8))
		errx(1, "Failed to trap execve() of parent");

	/*
	 * At this point, the parent has completed its execve, and is stopped.
	 * We load parasite.c into it, and load the target program.
	 */

	ctl = compel_prepare(pid);
	if (!ctl)
		err(1, "Can't prepare for infection");

	/* 
	 * Trapping once in the app seems necessary after execve().
	 * Otherwise, regs->ax is not set properly when running the next syscall
	 */
	long sret;
	int ret = compel_syscall(ctl, __NR(getpid, !compel_mode_native(ctl)), &sret,
				 0, 0, 0, 0, 0, 0);
	(void)ret;


	ictx = compel_infect_ctx(ctl);
	ictx->log_fd = STDERR_FILENO;
	parasite_setup_c_header(ctl);
	if (compel_infect(ctl, 1, sizeof(int)))
		err(1, "Can't infect victim");

	struct bootstrap_args *args = compel_parasite_args(ctl, struct bootstrap_args);
	args->sp = (void *)compel_get_leader_sp(ctl);
	args->ip = (void *)compel_get_leader_ip(ctl);

	if (compel_rpc_call(0, ctl))
		err(1, "Call to bootstrap ELF failed");
}

int main(int argc, char **argv)
{
	pid_t parent_pid = getpid();
	pid_t pid;
	char buf[1];
	int child_sync_fds[2];
	int parent_sync_fds[2];

	if (argc <= 2)
		errx(1, "Usage: %s <masquerade_exe_path> cmd...", argv[0]);

	if (pipe(child_sync_fds) < 0)
		err(1, "pipe()");
	if (pipe(parent_sync_fds) < 0)
		err(1, "pipe()");

	pid = fork();
	if (pid < 0)
		err(1, "fork() failed");

	if (pid == 0) {
		// child
		close(parent_sync_fds[0]);
		close(child_sync_fds[1]);
		do_infection(parent_pid, parent_sync_fds[1], child_sync_fds[0]);
		return 0;
	}

	if (prctl(PR_SET_PTRACER, pid) < 0)
		err(1, "prctl(PR_SET_PTRACER) failed");

	close(child_sync_fds[0]);
	close(child_sync_fds[1]);

	close(parent_sync_fds[1]);
	if (read(parent_sync_fds[0], buf, 1) != 1)
		err(1, "Our child died");
	close(parent_sync_fds[0]);

	// We are ptraced, now we execve() the exe_path
	// We setup the real program argv correctly. The parasite will use it.
	execvp(argv[1], &argv[2]);

	err(1, "execve() failed");
	return 1;
}
