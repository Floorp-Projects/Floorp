/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "linux_seccomp.h"
#include "linux_syscalls.h"

/* This is the actual seccomp whitelist.
 * This is used for B2G content-processes.
 */

/* Architecture specific syscalls, required to function on ARM */
#ifdef ALLOW_ARM_SYSCALL
#define SECCOMP_WHITELIST_ADD \
  ALLOW_ARM_SYSCALL(breakpoint), \
  ALLOW_ARM_SYSCALL(cacheflush), \
  ALLOW_ARM_SYSCALL(usr26), \
  ALLOW_ARM_SYSCALL(usr32), \
  ALLOW_ARM_SYSCALL(set_tls),
#else
#define SECCOMP_WHITELIST_ADD
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
#define SECCOMP_WHITELIST \
  /* These are calls we're ok to allow */ \
  ALLOW_SYSCALL(recv), \
  ALLOW_SYSCALL(msgget), \
  ALLOW_SYSCALL(semget), \
  ALLOW_SYSCALL(read), \
  ALLOW_SYSCALL(write), \
  ALLOW_SYSCALL(brk), \
  /* ioctl() is for GL. Remove when GL proxy is implemented.
   * Additionally ioctl() might be a place where we want to have
   * argument filtering */ \
  ALLOW_SYSCALL(ioctl), \
  ALLOW_SYSCALL(writev), \
  ALLOW_SYSCALL(close), \
  ALLOW_SYSCALL(clone), \
  ALLOW_SYSCALL(clock_gettime), \
  ALLOW_SYSCALL(lseek), \
  ALLOW_SYSCALL(_llseek), \
  ALLOW_SYSCALL(gettimeofday), \
  ALLOW_SYSCALL(getpid), \
  ALLOW_SYSCALL(gettid), \
  ALLOW_SYSCALL(getrusage), \
  ALLOW_SYSCALL(madvise), \
  ALLOW_SYSCALL(rt_sigreturn), \
  ALLOW_SYSCALL(sigreturn), \
  ALLOW_SYSCALL(epoll_wait), \
  ALLOW_SYSCALL(futex), \
  ALLOW_SYSCALL(fcntl64), \
  ALLOW_SYSCALL(munmap), \
  ALLOW_SYSCALL(mmap2), \
  ALLOW_SYSCALL(mprotect), \
  /* Must remove all of the following in the future, when no longer used */ \
  /* open() is for some legacy APIs such as font loading. */ \
  ALLOW_SYSCALL(open), \
  ALLOW_SYSCALL(fstat64), \
  ALLOW_SYSCALL(stat64), \
  ALLOW_SYSCALL(prctl), \
  ALLOW_SYSCALL(access), \
  ALLOW_SYSCALL(getdents64), \
  /* Should remove all of the following in the future, if possible */ \
  ALLOW_SYSCALL(getpriority), \
  ALLOW_SYSCALL(setpriority), \
  ALLOW_SYSCALL(sigprocmask), \
  /* Always last and always OK calls */ \
  SECCOMP_WHITELIST_ADD \
  /* restart_syscall is called internally, generally when debugging */ \
  ALLOW_SYSCALL(restart_syscall), \
  ALLOW_SYSCALL(exit_group), \
  ALLOW_SYSCALL(exit)

