/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "linux_seccomp.h"
#include "linux_syscalls.h"

/* This is the actual seccomp whitelist.
 * This is used for B2G content-processes.
 */

/* Frequently used syscalls specific to arm */
#if defined(__arm__)
#define SECCOMP_WHITELIST_ADD_ARM_HIGH \
  ALLOW_SYSCALL(msgget), \
  ALLOW_SYSCALL(recv), \
  ALLOW_SYSCALL(mmap2),
#else
#define SECCOMP_WHITELIST_ADD_ARM_HIGH
#endif

/* Frequently used syscalls specific to i386 */
#if defined(__i386__)
#define SECCOMP_WHITELIST_ADD_i386_HIGH \
  ALLOW_SYSCALL(ipc), \
  ALLOW_SYSCALL(mmap2),
#else
#define SECCOMP_WHITELIST_ADD_i386_HIGH
#endif

/* Frequently used syscalls specific to x86_64 */
#if defined(__x86_64__)
#define SECCOMP_WHITELIST_ADD_x86_64_HIGH \
  ALLOW_SYSCALL(msgget),
#else
#define SECCOMP_WHITELIST_ADD_x86_64_HIGH
#endif

/* Infrequently used syscalls specific to arm */
#if defined(__arm__)
#define SECCOMP_WHITELIST_ADD_ARM_LOW \
  ALLOW_SYSCALL(_llseek), \
  ALLOW_SYSCALL(getuid32), \
  ALLOW_SYSCALL(sigreturn), \
  ALLOW_SYSCALL(fcntl64),
#else
#define SECCOMP_WHITELIST_ADD_ARM_LOW
#endif

/* Infrequently used syscalls specific to i386 */
#if defined(__i386__)
#define SECCOMP_WHITELIST_ADD_i386_LOW \
  ALLOW_SYSCALL(_llseek), \
  ALLOW_SYSCALL(getuid32), \
  ALLOW_SYSCALL(sigreturn), \
  ALLOW_SYSCALL(fcntl64),
#else
#define SECCOMP_WHITELIST_ADD_i386_LOW
#endif

#if defined(__arm__)
#define SECCOMP_WHITELIST_ADD_ARM_LAST \
  ALLOW_ARM_SYSCALL(breakpoint), \
  ALLOW_ARM_SYSCALL(cacheflush), \
  ALLOW_ARM_SYSCALL(usr26), \
  ALLOW_ARM_SYSCALL(usr32), \
  ALLOW_ARM_SYSCALL(set_tls),
#else
#define SECCOMP_WHITELIST_ADD_ARM_LAST
#endif

/* Syscalls specific to arm that should eventually be removed */
#if defined(__arm__)
#define SECCOMP_WHITELIST_REMOVE_ARM \
  ALLOW_SYSCALL(fstat64), \
  ALLOW_SYSCALL(stat64), \
  ALLOW_SYSCALL(sigprocmask),
#else
#define SECCOMP_WHITELIST_REMOVE_ARM
#endif

/* Syscalls specific to i386 that should eventually be removed */
#if defined(__arm__)
#define SECCOMP_WHITELIST_REMOVE_i386 \
  ALLOW_SYSCALL(fstat64), \
  ALLOW_SYSCALL(stat64), \
  ALLOW_SYSCALL(sigprocmask),
#else
#define SECCOMP_WHITELIST_REMOVE_i386
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
  SECCOMP_WHITELIST_ADD_ARM_HIGH \
  SECCOMP_WHITELIST_ADD_i386_HIGH \
  SECCOMP_WHITELIST_ADD_x86_64_HIGH \
  ALLOW_SYSCALL(gettimeofday), \
  ALLOW_SYSCALL(read), \
  ALLOW_SYSCALL(write), \
  ALLOW_SYSCALL(lseek), \
  /* ioctl() is for GL. Remove when GL proxy is implemented.
   * Additionally ioctl() might be a place where we want to have
   * argument filtering */ \
  ALLOW_SYSCALL(ioctl), \
  ALLOW_SYSCALL(close), \
  ALLOW_SYSCALL(munmap), \
  ALLOW_SYSCALL(mprotect), \
  ALLOW_SYSCALL(writev), \
  ALLOW_SYSCALL(clone), \
  ALLOW_SYSCALL(brk), \
  ALLOW_SYSCALL(clock_gettime), \
  ALLOW_SYSCALL(getpid), \
  ALLOW_SYSCALL(gettid), \
  ALLOW_SYSCALL(getrusage), \
  ALLOW_SYSCALL(madvise), \
  ALLOW_SYSCALL(rt_sigreturn), \
  ALLOW_SYSCALL(epoll_wait), \
  ALLOW_SYSCALL(futex), \
  ALLOW_SYSCALL(dup), \
  ALLOW_SYSCALL(nanosleep), \
  SECCOMP_WHITELIST_ADD_ARM_LOW \
  SECCOMP_WHITELIST_ADD_i386_LOW \
  /* Must remove all of the following in the future, when no longer used */ \
  /* open() is for some legacy APIs such as font loading. */ \
  /* See bug 906996 for removing unlink(). */ \
  SECCOMP_WHITELIST_REMOVE_ARM \
  SECCOMP_WHITELIST_REMOVE_i386 \
  ALLOW_SYSCALL(open), \
  ALLOW_SYSCALL(prctl), \
  ALLOW_SYSCALL(access), \
  ALLOW_SYSCALL(getdents64), \
  ALLOW_SYSCALL(unlink), \
  /* Should remove all of the following in the future, if possible */ \
  ALLOW_SYSCALL(getpriority), \
  ALLOW_SYSCALL(setpriority), \
  ALLOW_SYSCALL(sched_setscheduler), \
  /* Always last and always OK calls */ \
  SECCOMP_WHITELIST_ADD_ARM_LAST \
  /* restart_syscall is called internally, generally when debugging */ \
  ALLOW_SYSCALL(restart_syscall), \
  ALLOW_SYSCALL(exit_group), \
  ALLOW_SYSCALL(exit)

