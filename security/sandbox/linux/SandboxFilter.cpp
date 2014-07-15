/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SandboxFilter.h"
#include "SandboxAssembler.h"

#include "linux_seccomp.h"
#include "linux_syscalls.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/NullPtr.h"

#include <errno.h>
#include <unistd.h>
#include <linux/net.h>

namespace mozilla {

class SandboxFilterImpl : public SandboxAssembler
{
  void Build();
public:
  SandboxFilterImpl() {
    Build();
    Finish();
  }
};

SandboxFilter::SandboxFilter(const sock_fprog** aStored, bool aVerbose)
  : mStored(aStored)
{
  MOZ_ASSERT(*mStored == nullptr);
  std::vector<struct sock_filter> filterVec;
  {
    SandboxFilterImpl impl;
    impl.Compile(&filterVec, aVerbose);
  }
  mProg = new sock_fprog;
  mProg->len = filterVec.size();
  mProg->filter = mFilter = new sock_filter[mProg->len];
  for (size_t i = 0; i < mProg->len; ++i) {
    mFilter[i] = filterVec[i];
  }
  *mStored = mProg;
}

SandboxFilter::~SandboxFilter()
{
  *mStored = nullptr;
  delete[] mFilter;
  delete mProg;
}

void
SandboxFilterImpl::Build() {
#define SYSCALL_EXISTS(name) (defined(__NR_##name))

#define SYSCALL(name) (Condition(__NR_##name))
#if defined(__arm__) && (defined(__thumb__) || defined(__ARM_EABI__))
#define ARM_SYSCALL(name) (Condition(__ARM_NR_##name))
#endif

#define SYSCALL_WITH_ARG(name, arg, values...) ({ \
  uint32_t argValues[] = { values };              \
  Condition(__NR_##name, arg, argValues);         \
})

  // Some architectures went through a transition from 32-bit to
  // 64-bit off_t and had to version all the syscalls that referenced
  // it; others (newer and/or 64-bit ones) didn't.  Adjust the
  // conditional as needed.
#if SYSCALL_EXISTS(stat64)
#define SYSCALL_LARGEFILE(plain, versioned) SYSCALL(versioned)
#else
#define SYSCALL_LARGEFILE(plain, versioned) SYSCALL(plain)
#endif

  // i386 multiplexes all the socket-related interfaces into a single
  // syscall.
#if SYSCALL_EXISTS(socketcall)
#define SOCKETCALL(name, NAME) SYSCALL_WITH_ARG(socketcall, 0, SYS_##NAME)
#else
#define SOCKETCALL(name, NAME) SYSCALL(name)
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

  Allow(SYSCALL(futex));
  Allow(SOCKETCALL(recvmsg, RECVMSG));
  Allow(SOCKETCALL(sendmsg, SENDMSG));

  // mmap2 is a little different from most off_t users, because it's
  // passed in a register (so it's a problem for even a "new" 32-bit
  // arch) -- and the workaround, mmap2, passes a page offset instead.
#if SYSCALL_EXISTS(mmap2)
  Allow(SYSCALL(mmap2));
#else
  Allow(SYSCALL(mmap));
#endif

  /* B2G specific high-frequency syscalls */
#ifdef MOZ_WIDGET_GONK
  Allow(SYSCALL(clock_gettime));
  Allow(SYSCALL(epoll_wait));
  Allow(SYSCALL(gettimeofday));
#endif
  Allow(SYSCALL(read));
  Allow(SYSCALL(write));
  // 32-bit lseek is used, at least on Android, to implement ANSI fseek.
#if SYSCALL_EXISTS(_llseek)
  Allow(SYSCALL(_llseek));
#endif
  Allow(SYSCALL(lseek));
  // Android also uses 32-bit ftruncate.
  Allow(SYSCALL(ftruncate));
#if SYSCALL_EXISTS(ftruncate64)
  Allow(SYSCALL(ftruncate64));
#endif

  /* ioctl() is for GL. Remove when GL proxy is implemented.
   * Additionally ioctl() might be a place where we want to have
   * argument filtering */
  Allow(SYSCALL(ioctl));
  Allow(SYSCALL(close));
  Allow(SYSCALL(munmap));
  Allow(SYSCALL(mprotect));
  Allow(SYSCALL(writev));
  Allow(SYSCALL(clone));
  Allow(SYSCALL(brk));
#if SYSCALL_EXISTS(set_thread_area)
  Allow(SYSCALL(set_thread_area));
#endif

  Allow(SYSCALL(getpid));
  Allow(SYSCALL(gettid));
  Allow(SYSCALL(getrusage));
  Allow(SYSCALL(times));
  Allow(SYSCALL(madvise));
  Allow(SYSCALL(dup));
  Allow(SYSCALL(nanosleep));
  Allow(SYSCALL(poll));
  // select()'s arguments used to be passed by pointer as a struct.
#if SYSCALL_EXISTS(_newselect)
  Allow(SYSCALL(_newselect));
#else
  Allow(SYSCALL(select));
#endif
  // Some archs used to have 16-bit uid/gid instead of 32-bit.
#if SYSCALL_EXISTS(getuid32)
  Allow(SYSCALL(getuid32));
  Allow(SYSCALL(geteuid32));
#else
  Allow(SYSCALL(getuid));
  Allow(SYSCALL(geteuid));
#endif
  // Some newer archs (e.g., x64 and x32) have only rt_sigreturn, but
  // ARM has and uses both syscalls -- rt_sigreturn for SA_SIGINFO
  // handlers and classic sigreturn otherwise.
#if SYSCALL_EXISTS(sigreturn)
  Allow(SYSCALL(sigreturn));
#endif
  Allow(SYSCALL(rt_sigreturn));
  Allow(SYSCALL_LARGEFILE(fcntl, fcntl64));

  /* Must remove all of the following in the future, when no longer used */
  /* open() is for some legacy APIs such as font loading. */
  /* See bug 906996 for removing unlink(). */
  Allow(SYSCALL_LARGEFILE(fstat, fstat64));
  Allow(SYSCALL_LARGEFILE(stat, stat64));
  Allow(SYSCALL_LARGEFILE(lstat, lstat64));
  Allow(SOCKETCALL(socketpair, SOCKETPAIR));
  Deny(EACCES, SOCKETCALL(socket, SOCKET));
  Allow(SYSCALL(open));
  Allow(SYSCALL(readlink)); /* Workaround for bug 964455 */
  Allow(SYSCALL(prctl));
  Allow(SYSCALL(access));
  Allow(SYSCALL(unlink));
  Allow(SYSCALL(fsync));
  Allow(SYSCALL(msync));

  /* Should remove all of the following in the future, if possible */
  Allow(SYSCALL(getpriority));
  Allow(SYSCALL(sched_get_priority_min));
  Allow(SYSCALL(sched_get_priority_max));
  Allow(SYSCALL(setpriority));
  // rt_sigprocmask is passed the sigset_t size.  On older archs,
  // sigprocmask is a compatibility shim that assumes the pre-RT size.
#if SYSCALL_EXISTS(sigprocmask)
  Allow(SYSCALL(sigprocmask));
#endif
  Allow(SYSCALL(rt_sigprocmask));

  // Used by profiler.  Also used for raise(), which causes problems
  // with Android KitKat abort(); see bug 1004832.
  Allow(SYSCALL_WITH_ARG(tgkill, 0, uint32_t(getpid())));

  /* B2G specific low-frequency syscalls */
#ifdef MOZ_WIDGET_GONK
  Allow(SOCKETCALL(sendto, SENDTO));
  Allow(SOCKETCALL(recvfrom, RECVFROM));
  Allow(SYSCALL_LARGEFILE(getdents, getdents64));
  Allow(SYSCALL(epoll_ctl));
  Allow(SYSCALL(sched_yield));
  Allow(SYSCALL(sched_getscheduler));
  Allow(SYSCALL(sched_setscheduler));
  Allow(SYSCALL(sigaltstack));
#endif

  /* Always last and always OK calls */
  /* Architecture-specific very infrequently used syscalls */
#if SYSCALL_EXISTS(sigaction)
  Allow(SYSCALL(sigaction));
#endif
  Allow(SYSCALL(rt_sigaction));
#ifdef ARM_SYSCALL
  Allow(ARM_SYSCALL(breakpoint));
  Allow(ARM_SYSCALL(cacheflush));
  Allow(ARM_SYSCALL(usr26));
  Allow(ARM_SYSCALL(usr32));
  Allow(ARM_SYSCALL(set_tls));
#endif

  /* restart_syscall is called internally, generally when debugging */
  Allow(SYSCALL(restart_syscall));

  /* linux desktop is not as performance critical as B2G */
  /* we can place desktop syscalls at the end */
#ifndef MOZ_WIDGET_GONK
  Allow(SYSCALL(stat));
  Allow(SYSCALL(getdents));
  Allow(SYSCALL(lstat));
  Allow(SYSCALL(mmap));
  Allow(SYSCALL(openat));
  Allow(SYSCALL(fcntl));
  Allow(SYSCALL(fstat));
  Allow(SYSCALL(readlink));
  Allow(SYSCALL(getsockname));
  Allow(SYSCALL(getuid));
  Allow(SYSCALL(geteuid));
  Allow(SYSCALL(mkdir));
  Allow(SYSCALL(getcwd));
  Allow(SYSCALL(readahead));
  Allow(SYSCALL(pread64));
  Allow(SYSCALL(statfs));
  Allow(SYSCALL(pipe));
  Allow(SYSCALL(getrlimit));
  Allow(SYSCALL(shutdown));
  Allow(SYSCALL(getpeername));
  Allow(SYSCALL(eventfd2));
  Allow(SYSCALL(clock_getres));
  Allow(SYSCALL(sysinfo));
  Allow(SYSCALL(getresuid));
  Allow(SYSCALL(umask));
  Allow(SYSCALL(getresgid));
  Allow(SYSCALL(poll));
  Allow(SYSCALL(getegid));
  Allow(SYSCALL(inotify_init1));
  Allow(SYSCALL(wait4));
  Allow(SYSCALL(shmctl));
  Allow(SYSCALL(set_robust_list));
  Allow(SYSCALL(rmdir));
  Allow(SYSCALL(recvfrom));
  Allow(SYSCALL(shmdt));
  Allow(SYSCALL(pipe2));
  Allow(SYSCALL(setsockopt));
  Allow(SYSCALL(shmat));
  Allow(SYSCALL(set_tid_address));
  Allow(SYSCALL(inotify_add_watch));
  Allow(SYSCALL(rt_sigprocmask));
  Allow(SYSCALL(shmget));
  Allow(SYSCALL(getgid));
  Allow(SYSCALL(utime));
  Allow(SYSCALL(arch_prctl));
  Allow(SYSCALL(sched_getaffinity));
  /* We should remove all of the following in the future (possibly even more) */
  Allow(SYSCALL(socket));
  Allow(SYSCALL(chmod));
  Allow(SYSCALL(execve));
  Allow(SYSCALL(rename));
  Allow(SYSCALL(symlink));
  Allow(SYSCALL(connect));
  Allow(SYSCALL(quotactl));
  Allow(SYSCALL(kill));
  Allow(SYSCALL(sendto));
#endif

  /* nsSystemInfo uses uname (and we cache an instance, so */
  /* the info remains present even if we block the syscall) */
  Allow(SYSCALL(uname));
  Allow(SYSCALL(exit_group));
  Allow(SYSCALL(exit));
}

}
