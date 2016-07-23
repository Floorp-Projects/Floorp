/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SandboxFilter.h"
#include "SandboxFilterUtil.h"

#include "SandboxBrokerClient.h"
#include "SandboxInternal.h"
#include "SandboxLogging.h"

#include "mozilla/UniquePtr.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/ipc.h>
#include <linux/net.h>
#include <linux/prctl.h>
#include <linux/sched.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/seccomp-bpf/linux_seccomp.h"
#include "sandbox/linux/services/linux_syscalls.h"

using namespace sandbox::bpf_dsl;
#define CASES SANDBOX_BPF_DSL_CASES

// Fill in defines in case of old headers.
// (Warning: these are wrong on PA-RISC.)
#ifndef MADV_NOHUGEPAGE
#define MADV_NOHUGEPAGE 15
#endif
#ifndef MADV_DONTDUMP
#define MADV_DONTDUMP 16
#endif

// To avoid visual confusion between "ifdef ANDROID" and "ifndef ANDROID":
#ifndef ANDROID
#define DESKTOP
#endif

// This file defines the seccomp-bpf system call filter policies.
// See also SandboxFilterUtil.h, for the CASES_FOR_* macros and
// SandboxFilterBase::Evaluate{Socket,Ipc}Call.
//
// One important difference from how Chromium bpf_dsl filters are
// normally interpreted: returning -ENOSYS from a Trap() handler
// indicates an unexpected system call; SigSysHandler() in Sandbox.cpp
// will detect this, request a crash dump, and terminate the process.
// This does not apply to using Error(ENOSYS) in the policy, so that
// can be used if returning an actual ENOSYS is needed.

namespace mozilla {

// This class whitelists everything used by the sandbox itself, by the
// core IPC code, by the crash reporter, or other core code.
class SandboxPolicyCommon : public SandboxPolicyBase
{
protected:
  typedef const sandbox::arch_seccomp_data& ArgsRef;

  static intptr_t BlockedSyscallTrap(ArgsRef aArgs, void *aux) {
    MOZ_ASSERT(!aux);
    return -ENOSYS;
  }

private:
#if defined(ANDROID) && ANDROID_VERSION < 16
  // Bug 1093893: Translate tkill to tgkill for pthread_kill; fixed in
  // bionic commit 10c8ce59a (in JB and up; API level 16 = Android 4.1).
  static intptr_t TKillCompatTrap(const sandbox::arch_seccomp_data& aArgs,
                                  void *aux)
  {
    return syscall(__NR_tgkill, getpid(), aArgs.args[0], aArgs.args[1]);
  }
#endif

public:
  virtual ResultExpr InvalidSyscall() const override {
    return Trap(BlockedSyscallTrap, nullptr);
  }

  virtual ResultExpr ClonePolicy(ResultExpr failPolicy) const {
    // Allow use for simple thread creation (pthread_create) only.

    // WARNING: s390 and cris pass the flags in the second arg -- see
    // CLONE_BACKWARDS2 in arch/Kconfig in the kernel source -- but we
    // don't support seccomp-bpf on those archs yet.
    Arg<int> flags(0);

    // The glibc source hasn't changed the thread creation clone flags
    // since 2004, so this *should* be safe to hard-code.  Bionic's
    // value has changed a few times, and has converged on the same one
    // as glibc; allow any of them.
    static const int flags_common = CLONE_VM | CLONE_FS | CLONE_FILES |
      CLONE_SIGHAND | CLONE_THREAD | CLONE_SYSVSEM;
    static const int flags_modern = flags_common | CLONE_SETTLS |
      CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID;

    // Can't use CASES here because its decltype magic infers const
    // int instead of regular int and bizarre voluminous errors issue
    // forth from the depths of the standard library implementation.
    return Switch(flags)
#ifdef ANDROID
      .Case(flags_common | CLONE_DETACHED, Allow()) // <= JB 4.2
      .Case(flags_common, Allow()) // JB 4.3 or KK 4.4
#endif
      .Case(flags_modern, Allow()) // Android L or glibc
      .Default(failPolicy);
  }

  virtual ResultExpr PrctlPolicy() const {
    // Note: this will probably need PR_SET_VMA if/when it's used on
    // Android without being overridden by an allow-all policy, and
    // the constant will need to be defined locally.
    Arg<int> op(0);
    return Switch(op)
      .CASES((PR_GET_SECCOMP, // BroadcastSetThreadSandbox, etc.
              PR_SET_NAME,    // Thread creation
              PR_SET_DUMPABLE), // Crash reporting
             Allow())
      .Default(InvalidSyscall());
  }

  virtual Maybe<ResultExpr> EvaluateSocketCall(int aCall) const override {
    switch (aCall) {
    case SYS_RECVMSG:
    case SYS_SENDMSG:
      return Some(Allow());
    default:
      return Nothing();
    }
  }

  virtual ResultExpr EvaluateSyscall(int sysno) const override {
    switch (sysno) {
      // Timekeeping
    case __NR_clock_gettime: {
      Arg<clockid_t> clk_id(0);
      return If(clk_id == CLOCK_MONOTONIC, Allow())
#ifdef CLOCK_MONOTONIC_COARSE
        .ElseIf(clk_id == CLOCK_MONOTONIC_COARSE, Allow())
#endif
        .ElseIf(clk_id == CLOCK_PROCESS_CPUTIME_ID, Allow())
        .ElseIf(clk_id == CLOCK_REALTIME, Allow())
#ifdef CLOCK_REALTIME_COARSE
        .ElseIf(clk_id == CLOCK_REALTIME_COARSE, Allow())
#endif
        .ElseIf(clk_id == CLOCK_THREAD_CPUTIME_ID, Allow())
        .Else(InvalidSyscall());
    }
    case __NR_gettimeofday:
#ifdef __NR_time
    case __NR_time:
#endif
    case __NR_nanosleep:
      return Allow();

      // Thread synchronization
    case __NR_futex:
      // FIXME: This could be more restrictive....
      return Allow();

      // Asynchronous I/O
    case __NR_epoll_wait:
    case __NR_epoll_pwait:
    case __NR_epoll_ctl:
    case __NR_ppoll:
    case __NR_poll:
      return Allow();

      // Used when requesting a crash dump.
    case __NR_pipe:
      return Allow();

      // Metadata of opened files
    CASES_FOR_fstat:
      return Allow();

      // Simple I/O
    case __NR_write:
    case __NR_read:
    case __NR_readv:
    case __NR_writev: // see SandboxLogging.cpp
    CASES_FOR_lseek:
      return Allow();

      // Memory mapping
    CASES_FOR_mmap:
    case __NR_munmap:
      return Allow();

      // Signal handling
#if defined(ANDROID) || defined(MOZ_ASAN)
    case __NR_sigaltstack:
#endif
    CASES_FOR_sigreturn:
    CASES_FOR_sigprocmask:
    CASES_FOR_sigaction:
      return Allow();

      // Send signals within the process (raise(), profiling, etc.)
    case __NR_tgkill: {
      Arg<pid_t> tgid(0);
      return If(tgid == getpid(), Allow())
        .Else(InvalidSyscall());
    }

#if defined(ANDROID) && ANDROID_VERSION < 16
      // Polyfill with tgkill; see above.
    case __NR_tkill:
      return Trap(TKillCompatTrap, nullptr);
#endif

      // Yield
    case __NR_sched_yield:
      return Allow();

      // Thread creation.
    case __NR_clone:
      return ClonePolicy(InvalidSyscall());

      // More thread creation.
#ifdef __NR_set_robust_list
    case __NR_set_robust_list:
      return Allow();
#endif
#ifdef ANDROID
    case __NR_set_tid_address:
      return Allow();
#endif

      // prctl
    case __NR_prctl:
      return PrctlPolicy();

      // NSPR can call this when creating a thread, but it will accept a
      // polite "no".
    case __NR_getpriority:
      // But if thread creation races with sandbox startup, that call
      // could succeed, and then we get one of these:
    case __NR_setpriority:
      return Error(EACCES);

      // Stack bounds are obtained via pthread_getattr_np, which calls
      // this but doesn't actually need it:
    case __NR_sched_getaffinity:
      return Error(ENOSYS);

      // Read own pid/tid.
    case __NR_getpid:
    case __NR_gettid:
      return Allow();

      // Discard capabilities
    case __NR_close:
      return Allow();

      // Machine-dependent stuff
#ifdef __arm__
    case __ARM_NR_breakpoint:
    case __ARM_NR_cacheflush:
    case __ARM_NR_usr26: // FIXME: do we actually need this?
    case __ARM_NR_usr32:
    case __ARM_NR_set_tls:
      return Allow();
#endif

      // Needed when being debugged:
    case __NR_restart_syscall:
      return Allow();

      // Terminate threads or the process
    case __NR_exit:
    case __NR_exit_group:
      return Allow();

#ifdef MOZ_ASAN
      // ASAN's error reporter wants to know if stderr is a tty.
    case __NR_ioctl: {
      Arg<int> fd(0);
      return If(fd == STDERR_FILENO, Allow())
        .Else(InvalidSyscall());
    }

      // ...and before compiler-rt r209773, it will call readlink on
      // /proc/self/exe and use the cached value only if that fails:
    case __NR_readlink:
    case __NR_readlinkat:
      return Error(ENOENT);

      // ...and if it found an external symbolizer, it will try to run it:
      // (See also bug 1081242 comment #7.)
    CASES_FOR_stat:
      return Error(ENOENT);
#endif

    default:
      return SandboxPolicyBase::EvaluateSyscall(sysno);
    }
  }
};

// The process-type-specific syscall rules start here:

#ifdef MOZ_CONTENT_SANDBOX
// The seccomp-bpf filter for content processes is not a true sandbox
// on its own; its purpose is attack surface reduction and syscall
// interception in support of a semantic sandboxing layer.  On B2G
// this is the Android process permission model; on desktop,
// namespaces and chroot() will be used.
class ContentSandboxPolicy : public SandboxPolicyCommon {
  SandboxBrokerClient* mBroker;

  // Trap handlers for filesystem brokering.
  // (The amount of code duplication here could be improved....)
#ifdef __NR_open
  static intptr_t OpenTrap(ArgsRef aArgs, void* aux) {
    auto broker = static_cast<SandboxBrokerClient*>(aux);
    auto path = reinterpret_cast<const char*>(aArgs.args[0]);
    auto flags = static_cast<int>(aArgs.args[1]);
    return broker->Open(path, flags);
  }
#endif

  static intptr_t OpenAtTrap(ArgsRef aArgs, void* aux) {
    auto broker = static_cast<SandboxBrokerClient*>(aux);
    auto fd = static_cast<int>(aArgs.args[0]);
    auto path = reinterpret_cast<const char*>(aArgs.args[1]);
    auto flags = static_cast<int>(aArgs.args[2]);
    if (fd != AT_FDCWD && path[0] != '/') {
      SANDBOX_LOG_ERROR("unsupported fd-relative openat(%d, \"%s\", 0%o)",
                        fd, path, flags);
      return BlockedSyscallTrap(aArgs, nullptr);
    }
    return broker->Open(path, flags);
  }

#ifdef __NR_access
  static intptr_t AccessTrap(ArgsRef aArgs, void* aux) {
    auto broker = static_cast<SandboxBrokerClient*>(aux);
    auto path = reinterpret_cast<const char*>(aArgs.args[0]);
    auto mode = static_cast<int>(aArgs.args[1]);
    return broker->Access(path, mode);
  }
#endif

  static intptr_t AccessAtTrap(ArgsRef aArgs, void* aux) {
    auto broker = static_cast<SandboxBrokerClient*>(aux);
    auto fd = static_cast<int>(aArgs.args[0]);
    auto path = reinterpret_cast<const char*>(aArgs.args[1]);
    auto mode = static_cast<int>(aArgs.args[2]);
    // Linux's faccessat syscall has no "flags" argument.  Attempting
    // to handle the flags != 0 case is left to userspace; this is
    // impossible to do correctly in all cases, but that's not our
    // problem.
    if (fd != AT_FDCWD && path[0] != '/') {
      SANDBOX_LOG_ERROR("unsupported fd-relative faccessat(%d, \"%s\", %d)",
                        fd, path, mode);
      return BlockedSyscallTrap(aArgs, nullptr);
    }
    return broker->Access(path, mode);
  }

  static intptr_t StatTrap(ArgsRef aArgs, void* aux) {
    auto broker = static_cast<SandboxBrokerClient*>(aux);
    auto path = reinterpret_cast<const char*>(aArgs.args[0]);
    auto buf = reinterpret_cast<struct stat*>(aArgs.args[1]);
    return broker->Stat(path, buf);
  }

  static intptr_t LStatTrap(ArgsRef aArgs, void* aux) {
    auto broker = static_cast<SandboxBrokerClient*>(aux);
    auto path = reinterpret_cast<const char*>(aArgs.args[0]);
    auto buf = reinterpret_cast<struct stat*>(aArgs.args[1]);
    return broker->LStat(path, buf);
  }

  static intptr_t StatAtTrap(ArgsRef aArgs, void* aux) {
    auto broker = static_cast<SandboxBrokerClient*>(aux);
    auto fd = static_cast<int>(aArgs.args[0]);
    auto path = reinterpret_cast<const char*>(aArgs.args[1]);
    auto buf = reinterpret_cast<struct stat*>(aArgs.args[2]);
    auto flags = static_cast<int>(aArgs.args[3]);
    if (fd != AT_FDCWD && path[0] != '/') {
      SANDBOX_LOG_ERROR("unsupported fd-relative fstatat(%d, \"%s\", %p, %d)",
                        fd, path, buf, flags);
      return BlockedSyscallTrap(aArgs, nullptr);
    }
    if ((flags & ~AT_SYMLINK_NOFOLLOW) != 0) {
      SANDBOX_LOG_ERROR("unsupported flags %d in fstatat(%d, \"%s\", %p, %d)",
                        (flags & ~AT_SYMLINK_NOFOLLOW), fd, path, buf, flags);
      return BlockedSyscallTrap(aArgs, nullptr);
    }
    return (flags & AT_SYMLINK_NOFOLLOW) == 0
      ? broker->Stat(path, buf)
      : broker->LStat(path, buf);
  }

  static intptr_t GetPPidTrap(ArgsRef aArgs, void* aux) {
    // In a pid namespace, getppid() will return 0. We will return 0 instead
    // of the real parent pid to see what breaks when we introduce the
    // pid namespace (Bug 1151624).
    return 0;
  }

public:
  explicit ContentSandboxPolicy(SandboxBrokerClient* aBroker):mBroker(aBroker) { }
  virtual ~ContentSandboxPolicy() { }
  virtual ResultExpr PrctlPolicy() const override {
    // Ideally this should be restricted to a whitelist, but content
    // uses enough things that it's not trivial to determine it.
    return Allow();
  }
  virtual Maybe<ResultExpr> EvaluateSocketCall(int aCall) const override {
    switch(aCall) {
    case SYS_RECVFROM:
    case SYS_SENDTO:
      return Some(Allow());

    case SYS_SOCKETPAIR: {
      // See bug 1066750.
      if (!kSocketCallHasArgs) {
        // We can't filter the args if the platform passes them by pointer.
        return Some(Allow());
      }
      Arg<int> domain(0), type(1);
      return Some(If(domain == AF_UNIX &&
                     (type == SOCK_STREAM || type == SOCK_SEQPACKET), Allow())
                  .Else(InvalidSyscall()));
    }

#ifdef ANDROID
    case SYS_SOCKET:
      return Some(Error(EACCES));
#else // #ifdef DESKTOP
    case SYS_RECV:
    case SYS_SEND:
    case SYS_SOCKET: // DANGEROUS
    case SYS_CONNECT: // DANGEROUS
    case SYS_ACCEPT:
    case SYS_ACCEPT4:
    case SYS_BIND:
    case SYS_LISTEN:
    case SYS_GETSOCKOPT:
    case SYS_SETSOCKOPT:
    case SYS_GETSOCKNAME:
    case SYS_GETPEERNAME:
    case SYS_SHUTDOWN:
      return Some(Allow());
#endif
    default:
      return SandboxPolicyCommon::EvaluateSocketCall(aCall);
    }
  }

#ifdef DESKTOP
  virtual Maybe<ResultExpr> EvaluateIpcCall(int aCall) const override {
    switch(aCall) {
      // These are a problem: SysV shared memory follows the Unix
      // "same uid policy" and can't be restricted/brokered like file
      // access.  But the graphics layer might not be using them
      // anymore; this needs to be studied.
    case SHMGET:
    case SHMCTL:
    case SHMAT:
    case SHMDT:
    case SEMGET:
    case SEMCTL:
    case SEMOP:
    case MSGGET:
      return Some(Allow());
    default:
      return SandboxPolicyCommon::EvaluateIpcCall(aCall);
    }
  }
#endif

  virtual ResultExpr EvaluateSyscall(int sysno) const override {
    if (mBroker) {
      // Have broker; route the appropriate syscalls to it.
      switch (sysno) {
      case __NR_open:
        return Trap(OpenTrap, mBroker);
      case __NR_openat:
        return Trap(OpenAtTrap, mBroker);
      case __NR_access:
        return Trap(AccessTrap, mBroker);
      case __NR_faccessat:
        return Trap(AccessAtTrap, mBroker);
      CASES_FOR_stat:
        return Trap(StatTrap, mBroker);
      CASES_FOR_lstat:
        return Trap(LStatTrap, mBroker);
      CASES_FOR_fstatat:
        return Trap(StatAtTrap, mBroker);
      }
    } else {
      // No broker; allow the syscalls directly.  )-:
      switch(sysno) {
      case __NR_open:
      case __NR_openat:
      case __NR_access:
      case __NR_faccessat:
      CASES_FOR_stat:
      CASES_FOR_lstat:
      CASES_FOR_fstatat:
        return Allow();
      }
    }

    switch (sysno) {
#ifdef DESKTOP
    case __NR_getppid:
      return Trap(GetPPidTrap, nullptr);

      // Filesystem syscalls that need more work to determine who's
      // using them, if they need to be, and what we intend to about it.
    case __NR_mkdir:
    case __NR_rmdir:
    case __NR_getcwd:
    CASES_FOR_statfs:
    CASES_FOR_fstatfs:
    case __NR_chmod:
    case __NR_rename:
    case __NR_symlink:
    case __NR_quotactl:
    case __NR_utimes:
    case __NR_link:
    case __NR_unlink:
    CASES_FOR_fchown:
    case __NR_fchmod:
#endif
      return Allow();

    case __NR_readlink:
    case __NR_readlinkat:
      // Workaround for bug 964455:
      return Error(EINVAL);

    CASES_FOR_select:
    case __NR_pselect6:
      return Allow();

    CASES_FOR_getdents:
    CASES_FOR_ftruncate:
    case __NR_writev:
    case __NR_pread64:
#ifdef DESKTOP
    case __NR_pwrite64:
    case __NR_readahead:
#endif
      return Allow();

    case __NR_ioctl:
      // ioctl() is for GL. Remove when GL proxy is implemented.
      // Additionally ioctl() might be a place where we want to have
      // argument filtering
      return Allow();

    CASES_FOR_fcntl:
      // Some fcntls have significant side effects like sending
      // arbitrary signals, and there's probably nontrivial kernel
      // attack surface; this should be locked down more if possible.
      return Allow();

    case __NR_mprotect:
    case __NR_brk:
    case __NR_madvise:
#if !defined(MOZ_MEMORY)
      // libc's realloc uses mremap (Bug 1286119).
    case __NR_mremap:
#endif
      return Allow();

    case __NR_sigaltstack:
      return Allow();

#ifdef __NR_set_thread_area
    case __NR_set_thread_area:
      return Allow();
#endif

    case __NR_getrusage:
    case __NR_times:
      return Allow();

    case __NR_dup:
      return Allow();

    CASES_FOR_getuid:
    CASES_FOR_getgid:
    CASES_FOR_geteuid:
    CASES_FOR_getegid:
      return Allow();

    case __NR_fsync:
    case __NR_msync:
      return Allow();

    case __NR_getpriority:
    case __NR_setpriority:
    case __NR_sched_get_priority_min:
    case __NR_sched_get_priority_max:
    case __NR_sched_getscheduler:
    case __NR_sched_setscheduler:
    case __NR_sched_getparam:
    case __NR_sched_setparam:
#ifdef DESKTOP
    case __NR_sched_getaffinity:
#endif
      return Allow();

#ifdef DESKTOP
    case __NR_pipe2:
      return Allow();

    CASES_FOR_getrlimit:
    case __NR_clock_getres:
    CASES_FOR_getresuid:
    CASES_FOR_getresgid:
      return Allow();

    case __NR_umask:
    case __NR_kill:
    case __NR_wait4:
#ifdef __NR_arch_prctl
    case __NR_arch_prctl:
#endif
      return Allow();

    case __NR_eventfd2:
    case __NR_inotify_init1:
    case __NR_inotify_add_watch:
    case __NR_inotify_rm_watch:
      return Allow();

#ifdef __NR_memfd_create
    case __NR_memfd_create:
      return Allow();
#endif

#ifdef __NR_rt_tgsigqueueinfo
      // Only allow to send signals within the process.
    case __NR_rt_tgsigqueueinfo: {
      Arg<pid_t> tgid(0);
      return If(tgid == getpid(), Allow())
        .Else(InvalidSyscall());
    }
#endif

    case __NR_mlock:
    case __NR_munlock:
      return Allow();

      // We can't usefully allow fork+exec, even on a temporary basis;
      // the child would inherit the seccomp-bpf policy and almost
      // certainly die from an unexpected SIGSYS.  We also can't have
      // fork() crash, currently, because there are too many system
      // libraries/plugins that try to run commands.  But they can
      // usually do something reasonable on error.
    case __NR_clone:
      return ClonePolicy(Error(EPERM));

#ifdef __NR_fadvise64
    case __NR_fadvise64:
      return Allow();
#endif

    case __NR_fallocate:
      return Allow();

#endif // DESKTOP

#ifdef __NR_getrandom
    case __NR_getrandom:
      return Allow();
#endif

      // nsSystemInfo uses uname (and we cache an instance, so
      // the info remains present even if we block the syscall)
    case __NR_uname:
#ifdef DESKTOP
    case __NR_sysinfo:
#endif
      return Allow();

    default:
      return SandboxPolicyCommon::EvaluateSyscall(sysno);
    }
  }
};

UniquePtr<sandbox::bpf_dsl::Policy>
GetContentSandboxPolicy(SandboxBrokerClient* aMaybeBroker)
{
  return UniquePtr<sandbox::bpf_dsl::Policy>(new ContentSandboxPolicy(aMaybeBroker));
}
#endif // MOZ_CONTENT_SANDBOX


#ifdef MOZ_GMP_SANDBOX
// Unlike for content, the GeckoMediaPlugin seccomp-bpf policy needs
// to be an effective sandbox by itself, because we allow GMP on Linux
// systems where that's the only sandboxing mechanism we can use.
//
// Be especially careful about what this policy allows.
class GMPSandboxPolicy : public SandboxPolicyCommon {
  static intptr_t OpenTrap(const sandbox::arch_seccomp_data& aArgs,
                           void* aux)
  {
    auto plugin = static_cast<SandboxOpenedFile*>(aux);
    const char* path;
    int flags;

    switch (aArgs.nr) {
#ifdef __NR_open
    case __NR_open:
      path = reinterpret_cast<const char*>(aArgs.args[0]);
      flags = static_cast<int>(aArgs.args[1]);
      break;
#endif
    case __NR_openat:
      // The path has to be absolute to match the pre-opened file (see
      // assertion in ctor) so the dirfd argument is ignored.
      path = reinterpret_cast<const char*>(aArgs.args[1]);
      flags = static_cast<int>(aArgs.args[2]);
      break;
    default:
      MOZ_CRASH("unexpected syscall number");
    }

    if ((flags & O_ACCMODE) != O_RDONLY) {
      SANDBOX_LOG_ERROR("non-read-only open of file %s attempted (flags=0%o)",
                        path, flags);
      return -ENOSYS;
    }
    if (strcmp(path, plugin->mPath) != 0) {
      SANDBOX_LOG_ERROR("attempt to open file %s which is not the media plugin"
                        " %s", path, plugin->mPath);
      return -ENOSYS;
    }
    int fd = plugin->mFd.exchange(-1);
    if (fd < 0) {
      SANDBOX_LOG_ERROR("multiple opens of media plugin file unimplemented");
      return -ENOSYS;
    }
    return fd;
  }

  static intptr_t SchedTrap(const sandbox::arch_seccomp_data& aArgs,
                            void* aux)
  {
    const pid_t tid = syscall(__NR_gettid);
    if (aArgs.args[0] == static_cast<uint64_t>(tid)) {
      return syscall(aArgs.nr,
                     0,
                     aArgs.args[1],
                     aArgs.args[2],
                     aArgs.args[3],
                     aArgs.args[4],
                     aArgs.args[5]);
    }
    SANDBOX_LOG_ERROR("unsupported tid in SchedTrap");
    return BlockedSyscallTrap(aArgs, nullptr);
  }

  SandboxOpenedFile* mPlugin;
public:
  explicit GMPSandboxPolicy(SandboxOpenedFile* aPlugin)
  : mPlugin(aPlugin)
  {
    MOZ_ASSERT(aPlugin->mPath[0] == '/', "plugin path should be absolute");
  }

  virtual ~GMPSandboxPolicy() { }

  virtual ResultExpr EvaluateSyscall(int sysno) const override {
    switch (sysno) {
      // Simulate opening the plugin file.
#ifdef __NR_open
    case __NR_open:
#endif
    case __NR_openat:
      return Trap(OpenTrap, mPlugin);

      // ipc::Shmem
    case __NR_mprotect:
      return Allow();
    case __NR_madvise: {
      Arg<int> advice(2);
      return If(advice == MADV_DONTNEED, Allow())
#ifdef MOZ_ASAN
        .ElseIf(advice == MADV_NOHUGEPAGE, Allow())
        .ElseIf(advice == MADV_DONTDUMP, Allow())
#endif
        .Else(InvalidSyscall());
    }
    case __NR_brk:
    case __NR_geteuid:
      return Allow();
    case __NR_sched_getparam:
    case __NR_sched_getscheduler:
    case __NR_sched_get_priority_min:
    case __NR_sched_get_priority_max:
    case __NR_sched_setscheduler: {
      Arg<pid_t> pid(0);
      return If(pid == 0, Allow())
        .Else(Trap(SchedTrap, nullptr));
    }

    default:
      return SandboxPolicyCommon::EvaluateSyscall(sysno);
    }
  }
};

UniquePtr<sandbox::bpf_dsl::Policy>
GetMediaSandboxPolicy(SandboxOpenedFile* aPlugin)
{
  return UniquePtr<sandbox::bpf_dsl::Policy>(new GMPSandboxPolicy(aPlugin));
}

#endif // MOZ_GMP_SANDBOX

}
