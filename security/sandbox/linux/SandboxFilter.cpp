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
#include <linux/ipc.h>
#include <linux/net.h>
#include <linux/prctl.h>
#include <linux/sched.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

namespace mozilla {

class SandboxFilterImpl : public SandboxAssembler
{
public:
  virtual void Build() = 0;
  virtual ~SandboxFilterImpl() { }
};

// Some helper macros to make the code that builds the filter more
// readable, and to help deal with differences among architectures.

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

// i386 multiplexes all the SysV-IPC-related interfaces into a single
// syscall.
#if SYSCALL_EXISTS(ipc)
#define SYSVIPCCALL(name, NAME) SYSCALL_WITH_ARG(ipc, 0, NAME)
#else
#define SYSVIPCCALL(name, NAME) SYSCALL(name)
#endif

#ifdef MOZ_CONTENT_SANDBOX
class SandboxFilterImplContent : public SandboxFilterImpl {
protected:
  virtual void Build() MOZ_OVERRIDE;
};

void
SandboxFilterImplContent::Build() {
  /* Most used system calls should be at the top of the whitelist
   * for performance reasons. The whitelist BPF filter exits after
   * processing any ALLOW_SYSCALL macro.
   *
   * How are those syscalls found?
   * 1) via strace -p <child pid> or/and
   * 2) the child will report which system call has been denied by seccomp-bpf,
   *    just before exiting
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

  Allow(SYSCALL(clock_gettime));
  Allow(SYSCALL(epoll_wait));
  Allow(SYSCALL(gettimeofday));
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
  Allow(SYSCALL(getgid32));
  Allow(SYSCALL(getegid32));
#else
  Allow(SYSCALL(getuid));
  Allow(SYSCALL(geteuid));
  Allow(SYSCALL(getgid));
  Allow(SYSCALL(getegid));
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

#if defined(ANDROID) && !defined(MOZ_MEMORY)
  // Android's libc's realloc uses mremap.
  Allow(SYSCALL(mremap));
#endif

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

  Allow(SOCKETCALL(sendto, SENDTO));
  Allow(SOCKETCALL(recvfrom, RECVFROM));
  Allow(SYSCALL_LARGEFILE(getdents, getdents64));
  Allow(SYSCALL(epoll_ctl));
  Allow(SYSCALL(sched_yield));
  Allow(SYSCALL(sched_getscheduler));
  Allow(SYSCALL(sched_setscheduler));
  Allow(SYSCALL(sched_getparam));
  Allow(SYSCALL(sched_setparam));
  Allow(SYSCALL(sigaltstack));
  Allow(SYSCALL(pipe));

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

  /* linux desktop is not as performance critical as mobile */
  /* we can place desktop syscalls at the end */
#ifndef ANDROID
  Allow(SYSCALL(stat));
  Allow(SYSCALL(getdents));
  Allow(SYSCALL(lstat));
#if SYSCALL_EXISTS(mmap2)
  Allow(SYSCALL(mmap2));
#else
  Allow(SYSCALL(mmap));
#endif
  Allow(SYSCALL(openat));
  Allow(SYSCALL(fcntl));
  Allow(SYSCALL(fstat));
  Allow(SYSCALL(readlink));
  Allow(SOCKETCALL(getsockname, GETSOCKNAME));
  Allow(SYSCALL(getuid));
  Allow(SYSCALL(geteuid));
  Allow(SYSCALL(mkdir));
  Allow(SYSCALL(getcwd));
  Allow(SYSCALL(readahead));
  Allow(SYSCALL(pread64));
  Allow(SYSCALL(statfs));
#if SYSCALL_EXISTS(ugetrlimit)
  Allow(SYSCALL(ugetrlimit));
#else
  Allow(SYSCALL(getrlimit));
#endif
  Allow(SOCKETCALL(shutdown, SHUTDOWN));
  Allow(SOCKETCALL(getpeername, GETPEERNAME));
  Allow(SYSCALL(eventfd2));
  Allow(SYSCALL(clock_getres));
  Allow(SYSCALL(sysinfo));
  Allow(SYSCALL(getresuid));
  Allow(SYSCALL(umask));
  Allow(SYSCALL(getresgid));
  Allow(SYSCALL(poll));
  Allow(SYSCALL(inotify_init1));
  Allow(SYSCALL(wait4));
  Allow(SYSVIPCCALL(shmctl, SHMCTL));
  Allow(SYSCALL(set_robust_list));
  Allow(SYSCALL(rmdir));
  Allow(SOCKETCALL(recvfrom, RECVFROM));
  Allow(SYSVIPCCALL(shmdt, SHMDT));
  Allow(SYSCALL(pipe2));
  Allow(SOCKETCALL(setsockopt, SETSOCKOPT));
  Allow(SYSVIPCCALL(shmat, SHMAT));
  Allow(SYSCALL(set_tid_address));
  Allow(SYSCALL(inotify_add_watch));
  Allow(SYSCALL(rt_sigprocmask));
  Allow(SYSVIPCCALL(shmget, SHMGET));
#if SYSCALL_EXISTS(utimes)
  Allow(SYSCALL(utimes));
#else
  Allow(SYSCALL(utime));
#endif
#if SYSCALL_EXISTS(arch_prctl)
  Allow(SYSCALL(arch_prctl));
#endif
  Allow(SYSCALL(sched_getaffinity));
  /* We should remove all of the following in the future (possibly even more) */
  Allow(SOCKETCALL(socket, SOCKET));
  Allow(SYSCALL(chmod));
  Allow(SYSCALL(execve));
  Allow(SYSCALL(rename));
  Allow(SYSCALL(symlink));
  Allow(SOCKETCALL(connect, CONNECT));
  Allow(SYSCALL(quotactl));
  Allow(SYSCALL(kill));
  Allow(SOCKETCALL(sendto, SENDTO));
#endif

  /* nsSystemInfo uses uname (and we cache an instance, so */
  /* the info remains present even if we block the syscall) */
  Allow(SYSCALL(uname));
  Allow(SYSCALL(exit_group));
  Allow(SYSCALL(exit));
}
#endif // MOZ_CONTENT_SANDBOX

#ifdef MOZ_GMP_SANDBOX
class SandboxFilterImplGMP : public SandboxFilterImpl {
protected:
  virtual void Build() MOZ_OVERRIDE;
};

void SandboxFilterImplGMP::Build() {
  // As for content processes, check the most common syscalls first.

  Allow(SYSCALL_WITH_ARG(clock_gettime, 0, CLOCK_MONOTONIC, CLOCK_REALTIME));
  Allow(SYSCALL(futex));
  Allow(SYSCALL(gettimeofday));
  Allow(SYSCALL(poll));
  Allow(SYSCALL(write));
  Allow(SYSCALL(read));
  Allow(SYSCALL(epoll_wait));
  Allow(SOCKETCALL(recvmsg, RECVMSG));
  Allow(SOCKETCALL(sendmsg, SENDMSG));
  Allow(SYSCALL(time));

  // Nothing after this line is performance-critical.

#if SYSCALL_EXISTS(mmap2)
  Allow(SYSCALL(mmap2));
#else
  Allow(SYSCALL(mmap));
#endif
  Allow(SYSCALL_LARGEFILE(fstat, fstat64));
  Allow(SYSCALL(munmap));

  Allow(SYSCALL(getpid));
  Allow(SYSCALL(gettid));

  // The glibc source hasn't changed the thread creation clone flags
  // since 2004, so this *should* be safe to hard-code.  Bionic is
  // different, but MOZ_GMP_SANDBOX isn't supported there yet.
  //
  // At minimum we should require CLONE_THREAD, so that a single
  // SIGKILL from the parent will destroy all descendant tasks.  In
  // general, pinning down as much of the flags word as possible is a
  // good idea, because it exposes a lot of subtle (and probably not
  // well tested in all cases) kernel functionality.
  //
  // WARNING: s390 and cris pass the flags in a different arg -- see
  // CLONE_BACKWARDS2 in arch/Kconfig in the kernel source -- but we
  // don't support seccomp-bpf on those archs yet.
  static const int new_thread_flags = CLONE_VM | CLONE_FS | CLONE_FILES |
    CLONE_SIGHAND | CLONE_THREAD | CLONE_SYSVSEM | CLONE_SETTLS |
    CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID;
  Allow(SYSCALL_WITH_ARG(clone, 0, new_thread_flags));

  Allow(SYSCALL_WITH_ARG(prctl, 0, PR_GET_SECCOMP, PR_SET_NAME));

#if SYSCALL_EXISTS(set_robust_list)
  Allow(SYSCALL(set_robust_list));
#endif

  // NSPR can call this when creating a thread, but it will accept a
  // polite "no".
  Deny(EACCES, SYSCALL(getpriority));

  // Stack bounds are obtained via pthread_getattr_np, which calls
  // this but doesn't actually need it:
  Deny(ENOSYS, SYSCALL(sched_getaffinity));

#ifdef MOZ_ASAN
  Allow(SYSCALL(sigaltstack));
#endif

  Allow(SYSCALL(mprotect));
  Allow(SYSCALL_WITH_ARG(madvise, 2, MADV_DONTNEED));

#if SYSCALL_EXISTS(sigreturn)
  Allow(SYSCALL(sigreturn));
#endif
  Allow(SYSCALL(rt_sigreturn));

  Allow(SYSCALL(restart_syscall));
  Allow(SYSCALL(close));

  // "Sleeping for 300 seconds" in debug crashes; possibly other uses.
  Allow(SYSCALL(nanosleep));

  // For the crash reporter:
#if SYSCALL_EXISTS(sigprocmask)
  Allow(SYSCALL(sigprocmask));
#endif
  Allow(SYSCALL(rt_sigprocmask));
#if SYSCALL_EXISTS(sigaction)
  Allow(SYSCALL(sigaction));
#endif
  Allow(SYSCALL(rt_sigaction));
  Allow(SYSCALL(pipe));
  Allow(SYSCALL_WITH_ARG(tgkill, 0, uint32_t(getpid())));
  Allow(SYSCALL_WITH_ARG(prctl, 0, PR_SET_DUMPABLE));

  // Note for when GMP is supported on an ARM platform: Add whichever
  // of the ARM-specific syscalls are needed for this type of process.

  Allow(SYSCALL(epoll_ctl));
  Allow(SYSCALL(exit));
  Allow(SYSCALL(exit_group));
}
#endif // MOZ_GMP_SANDBOX

SandboxFilter::SandboxFilter(const sock_fprog** aStored, SandboxType aType,
                             bool aVerbose)
  : mStored(aStored)
{
  MOZ_ASSERT(*mStored == nullptr);
  std::vector<struct sock_filter> filterVec;
  SandboxFilterImpl *impl;

  switch (aType) {
  case kSandboxContentProcess:
#ifdef MOZ_CONTENT_SANDBOX
    impl = new SandboxFilterImplContent();
#else
    MOZ_CRASH("Content process sandboxing not supported in this build!");
#endif
    break;
  case kSandboxMediaPlugin:
#ifdef MOZ_GMP_SANDBOX
    impl = new SandboxFilterImplGMP();
#else
    MOZ_CRASH("Gecko Media Plugin process sandboxing not supported in this"
              " build!");
#endif
    break;
  default:
    MOZ_CRASH("Nonexistent sandbox type!");
  }
  impl->Build();
  impl->Finish();
  impl->Compile(&filterVec, aVerbose);
  delete impl;

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

}
