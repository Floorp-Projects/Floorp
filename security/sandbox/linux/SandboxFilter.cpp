/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SandboxFilter.h"

#include "linux_seccomp.h"
#include "linux_syscalls.h"

#include "mozilla/ArrayUtils.h"

#include <errno.h>

namespace mozilla {

#define SYSCALL_EXISTS(name) defined(__NR_##name)

static struct sock_filter seccomp_filter[] = {
  VALIDATE_ARCHITECTURE,
  EXAMINE_SYSCALL,

  // Some architectures went through a transition from 32-bit to
  // 64-bit off_t and had to version all the syscalls that referenced
  // it; others (newer and/or 64-bit ones) didn't.  Adjust the
  // conditional as needed.
#if SYSCALL_EXISTS(stat64)
#define ALLOW_SYSCALL_LARGEFILE(plain, versioned) ALLOW_SYSCALL(versioned)
#else
#define ALLOW_SYSCALL_LARGEFILE(plain, versioned) ALLOW_SYSCALL(plain)
#endif

  /* Most used system calls should be at the top of the whitelist
   * for performance reasons. The whitelist BPF filter exits after
   * processing any ALLOW_SYSCALL macro.
   *
   * How are those syscalls found?
   * 1) via strace -p <child pid> or/and
   * 2) with MOZ_CONTENT_SANDBOX_REPORTER set, the child will report which system call
   *    has been denied by seccomp-bpf, just before exiting, via NSPR.
   * System call number to name mapping is found in:
   * bionic/libc/kernel/arch-arm/asm/unistd.h
   * or your libc's unistd.h/kernel headers.
   *
   * Current list order has been optimized through manual guess-work.
   * It could be further optimized by analyzing the output of:
   * 'strace -c -p <child pid>' for most used web apps.
   */

  ALLOW_SYSCALL(futex),
  // FIXME, bug 920372: i386 multiplexes all the socket-related
  // interfaces into a single syscall.  We should check the selector.
#if SYSCALL_EXISTS(socketcall)
  ALLOW_SYSCALL(socketcall),
#else
  ALLOW_SYSCALL(recvmsg),
  ALLOW_SYSCALL(sendmsg),
#endif

  // mmap2 is a little different from most off_t users, because it's
  // passed in a register (so it's a problem for even a "new" 32-bit
  // arch) -- and the workaround, mmap2, passes a page offset instead.
#if SYSCALL_EXISTS(mmap2)
  ALLOW_SYSCALL(mmap2),
#else
  ALLOW_SYSCALL(mmap),
#endif

  /* B2G specific high-frequency syscalls */
#ifdef MOZ_WIDGET_GONK
  ALLOW_SYSCALL(clock_gettime),
  ALLOW_SYSCALL(epoll_wait),
  ALLOW_SYSCALL(gettimeofday),
#endif
  ALLOW_SYSCALL(read),
  ALLOW_SYSCALL(write),
  // 32-bit lseek is used, at least on Android, to implement ANSI fseek.
#if SYSCALL_EXISTS(_llseek)
  ALLOW_SYSCALL(_llseek),
#endif
  ALLOW_SYSCALL(lseek),

  /* ioctl() is for GL. Remove when GL proxy is implemented.
   * Additionally ioctl() might be a place where we want to have
   * argument filtering */
  ALLOW_SYSCALL(ioctl),
  ALLOW_SYSCALL(close),
  ALLOW_SYSCALL(munmap),
  ALLOW_SYSCALL(mprotect),
  ALLOW_SYSCALL(writev),
  ALLOW_SYSCALL(clone),
  ALLOW_SYSCALL(brk),

  ALLOW_SYSCALL(getpid),
  ALLOW_SYSCALL(gettid),
  ALLOW_SYSCALL(getrusage),
  ALLOW_SYSCALL(madvise),
  ALLOW_SYSCALL(dup),
  ALLOW_SYSCALL(nanosleep),
  ALLOW_SYSCALL(poll),
  // select()'s arguments used to be passed by pointer as a struct.
#if SYSCALL_EXISTS(_newselect)
  ALLOW_SYSCALL(_newselect),
#else
  ALLOW_SYSCALL(select),
#endif
  // Some archs used to have 16-bit uid/gid instead of 32-bit.
#if SYSCALL_EXISTS(getuid32)
  ALLOW_SYSCALL(getuid32),
  ALLOW_SYSCALL(geteuid32),
#else
  ALLOW_SYSCALL(getuid),
  ALLOW_SYSCALL(geteuid),
#endif
  // Some newer archs (e.g., x64 and x32) have only rt_sigreturn, but
  // ARM has and uses both syscalls -- rt_sigreturn for SA_SIGINFO
  // handlers and classic sigreturn otherwise.
#if SYSCALL_EXISTS(sigreturn)
  ALLOW_SYSCALL(sigreturn),
#endif
  ALLOW_SYSCALL(rt_sigreturn),
  ALLOW_SYSCALL_LARGEFILE(fcntl, fcntl64),

  /* Must remove all of the following in the future, when no longer used */
  /* open() is for some legacy APIs such as font loading. */
  /* See bug 906996 for removing unlink(). */
  ALLOW_SYSCALL_LARGEFILE(fstat, fstat64),
  ALLOW_SYSCALL_LARGEFILE(stat, stat64),
  ALLOW_SYSCALL_LARGEFILE(lstat, lstat64),
  // FIXME, bug 920372: see above.
#if !SYSCALL_EXISTS(socketcall)
  ALLOW_SYSCALL(socketpair),
  DENY_SYSCALL(socket, EACCES),
#endif
  ALLOW_SYSCALL(open),
  ALLOW_SYSCALL(readlink), /* Workaround for bug 964455 */
  ALLOW_SYSCALL(prctl),
  ALLOW_SYSCALL(access),
  ALLOW_SYSCALL(unlink),
  ALLOW_SYSCALL(fsync),
  ALLOW_SYSCALL(msync),

  /* Should remove all of the following in the future, if possible */
  ALLOW_SYSCALL(getpriority),
  ALLOW_SYSCALL(sched_get_priority_min),
  ALLOW_SYSCALL(sched_get_priority_max),
  ALLOW_SYSCALL(setpriority),
  // rt_sigprocmask is passed the sigset_t size.  On older archs,
  // sigprocmask is a compatibility shim that assumes the pre-RT size.
#if SYSCALL_EXISTS(sigprocmask)
  ALLOW_SYSCALL(sigprocmask),
#endif
  ALLOW_SYSCALL(rt_sigprocmask),

  /* System calls used by the profiler */
#ifdef MOZ_PROFILING
  ALLOW_SYSCALL(tgkill),
#endif

  /* B2G specific low-frequency syscalls */
#ifdef MOZ_WIDGET_GONK
#if !SYSCALL_EXISTS(socketcall)
  ALLOW_SYSCALL(sendto),
  ALLOW_SYSCALL(recvfrom),
#endif
  ALLOW_SYSCALL_LARGEFILE(getdents, getdents64),
  ALLOW_SYSCALL(epoll_ctl),
  ALLOW_SYSCALL(sched_yield),
  ALLOW_SYSCALL(sched_getscheduler),
  ALLOW_SYSCALL(sched_setscheduler),
#endif

  /* Always last and always OK calls */
  /* Architecture-specific very infrequently used syscalls */
#if SYSCALL_EXISTS(sigaction)
  ALLOW_SYSCALL(sigaction),
#endif
  ALLOW_SYSCALL(rt_sigaction),
#ifdef ALLOW_ARM_SYSCALL
  ALLOW_ARM_SYSCALL(breakpoint),
  ALLOW_ARM_SYSCALL(cacheflush),
  ALLOW_ARM_SYSCALL(usr26),
  ALLOW_ARM_SYSCALL(usr32),
  ALLOW_ARM_SYSCALL(set_tls),
#endif

  /* restart_syscall is called internally, generally when debugging */
  ALLOW_SYSCALL(restart_syscall),

  /* linux desktop is not as performance critical as B2G */
  /* we can place desktop syscalls at the end */
#ifndef MOZ_WIDGET_GONK
  ALLOW_SYSCALL(stat),
  ALLOW_SYSCALL(getdents),
  ALLOW_SYSCALL(lstat),
  ALLOW_SYSCALL(mmap),
  ALLOW_SYSCALL(openat),
  ALLOW_SYSCALL(fcntl),
  ALLOW_SYSCALL(fstat),
  ALLOW_SYSCALL(readlink),
  ALLOW_SYSCALL(getsockname),
  ALLOW_SYSCALL(getuid),
  ALLOW_SYSCALL(geteuid),
  ALLOW_SYSCALL(mkdir),
  ALLOW_SYSCALL(getcwd),
  ALLOW_SYSCALL(readahead),
  ALLOW_SYSCALL(pread64),
  ALLOW_SYSCALL(statfs),
  ALLOW_SYSCALL(pipe),
  ALLOW_SYSCALL(ftruncate),
  ALLOW_SYSCALL(getrlimit),
  ALLOW_SYSCALL(shutdown),
  ALLOW_SYSCALL(getpeername),
  ALLOW_SYSCALL(eventfd2),
  ALLOW_SYSCALL(clock_getres),
  ALLOW_SYSCALL(sysinfo),
  ALLOW_SYSCALL(getresuid),
  ALLOW_SYSCALL(umask),
  ALLOW_SYSCALL(getresgid),
  ALLOW_SYSCALL(poll),
  ALLOW_SYSCALL(getegid),
  ALLOW_SYSCALL(inotify_init1),
  ALLOW_SYSCALL(wait4),
  ALLOW_SYSCALL(shmctl),
  ALLOW_SYSCALL(set_robust_list),
  ALLOW_SYSCALL(rmdir),
  ALLOW_SYSCALL(recvfrom),
  ALLOW_SYSCALL(shmdt),
  ALLOW_SYSCALL(pipe2),
  ALLOW_SYSCALL(setsockopt),
  ALLOW_SYSCALL(shmat),
  ALLOW_SYSCALL(set_tid_address),
  ALLOW_SYSCALL(inotify_add_watch),
  ALLOW_SYSCALL(rt_sigprocmask),
  ALLOW_SYSCALL(shmget),
  ALLOW_SYSCALL(getgid),
  ALLOW_SYSCALL(utime),
  ALLOW_SYSCALL(arch_prctl),
  ALLOW_SYSCALL(sched_getaffinity),
  /* We should remove all of the following in the future (possibly even more) */
  ALLOW_SYSCALL(socket),
  ALLOW_SYSCALL(chmod),
  ALLOW_SYSCALL(execve),
  ALLOW_SYSCALL(rename),
  ALLOW_SYSCALL(symlink),
  ALLOW_SYSCALL(connect),
  ALLOW_SYSCALL(quotactl),
  ALLOW_SYSCALL(kill),
  ALLOW_SYSCALL(sendto),
#endif

  /* nsSystemInfo uses uname (and we cache an instance, so */
  /* the info remains present even if we block the syscall) */
  ALLOW_SYSCALL(uname),
  ALLOW_SYSCALL(exit_group),
  ALLOW_SYSCALL(exit),

#ifdef MOZ_CONTENT_SANDBOX_REPORTER
  TRAP_PROCESS,
#else
  KILL_PROCESS,
#endif
};

static struct sock_fprog seccomp_prog = {
  (unsigned short)MOZ_ARRAY_LENGTH(seccomp_filter),
  seccomp_filter,
};

const sock_fprog*
GetSandboxFilter()
{
  return &seccomp_prog;
}

}
