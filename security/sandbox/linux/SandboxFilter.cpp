/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SandboxFilter.h"
#include "SandboxFilterUtil.h"

#include "SandboxBrokerClient.h"
#include "SandboxInfo.h"
#include "SandboxInternal.h"
#include "SandboxLogging.h"
#ifdef MOZ_GMP_SANDBOX
#include "SandboxOpenedFiles.h"
#endif
#include "mozilla/PodOperations.h"
#include "mozilla/TemplateLib.h"
#include "mozilla/UniquePtr.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/ioctl.h>
#include <linux/ipc.h>
#include <linux/net.h>
#include <linux/prctl.h>
#include <linux/sched.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>
#include <vector>
#include <algorithm>

#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/system_headers/linux_seccomp.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"

using namespace sandbox::bpf_dsl;
#define CASES SANDBOX_BPF_DSL_CASES

// Fill in defines in case of old headers.
// (Warning: these are wrong on PA-RISC.)
#ifndef MADV_HUGEPAGE
#define MADV_HUGEPAGE 14
#endif
#ifndef MADV_NOHUGEPAGE
#define MADV_NOHUGEPAGE 15
#endif
#ifndef MADV_DONTDUMP
#define MADV_DONTDUMP 16
#endif

// Added in Linux 4.5; see bug 1303813.
#ifndef MADV_FREE
#define MADV_FREE 8
#endif

#ifndef PR_SET_PTRACER
#define PR_SET_PTRACER 0x59616d61
#endif

// The headers define O_LARGEFILE as 0 on x86_64, but we need the
// actual value because it shows up in file flags.
#define O_LARGEFILE_REAL 00100000

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

  // Convert Unix-style "return -1 and set errno" APIs back into the
  // Linux ABI "return -err" style.
  static intptr_t ConvertError(long rv) {
    return rv < 0 ? -errno : rv;
  }

  template<typename... Args>
  static intptr_t DoSyscall(long nr, Args... args) {
    static_assert(tl::And<(sizeof(Args) <= sizeof(void*))...>::value,
                  "each syscall arg is at most one word");
    return ConvertError(syscall(nr, args...));
  }

private:
  // Bug 1093893: Translate tkill to tgkill for pthread_kill; fixed in
  // bionic commit 10c8ce59a (in JB and up; API level 16 = Android 4.1).
  // Bug 1376653: musl also needs this, and security-wise it's harmless.
  static intptr_t TKillCompatTrap(ArgsRef aArgs, void *aux)
  {
    auto tid = static_cast<pid_t>(aArgs.args[0]);
    auto sig = static_cast<int>(aArgs.args[1]);
    return DoSyscall(__NR_tgkill, getpid(), tid, sig);
  }

  static intptr_t SetNoNewPrivsTrap(ArgsRef& aArgs, void* aux) {
    if (gSetSandboxFilter == nullptr) {
      // Called after BroadcastSetThreadSandbox finished, therefore
      // not our doing and not expected.
      return BlockedSyscallTrap(aArgs, nullptr);
    }
    // Signal that the filter is already in place.
    return -ETXTBSY;
  }

public:
  ResultExpr InvalidSyscall() const override {
    return Trap(BlockedSyscallTrap, nullptr);
  }

  virtual ResultExpr ClonePolicy(ResultExpr failPolicy) const {
    // Allow use for simple thread creation (pthread_create) only.

    // WARNING: s390 and cris pass the flags in the second arg -- see
    // CLONE_BACKWARDS2 in arch/Kconfig in the kernel source -- but we
    // don't support seccomp-bpf on those archs yet.
    Arg<int> flags(0);

    // The exact flags used can vary.  CLONE_DETACHED is used by musl
    // and by old versions of Android (<= JB 4.2), but it's been
    // ignored by the kernel since the beginning of the Git history.
    //
    // If we ever need to support Android <= KK 4.4 again, SETTLS
    // and the *TID flags will need to be made optional.
    static const int flags_required = CLONE_VM | CLONE_FS | CLONE_FILES |
      CLONE_SIGHAND | CLONE_THREAD | CLONE_SYSVSEM | CLONE_SETTLS |
      CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID;
    static const int flags_optional = CLONE_DETACHED;

    return If((flags & ~flags_optional) == flags_required, Allow())
      .Else(failPolicy);
  }

  virtual ResultExpr PrctlPolicy() const {
    // Note: this will probably need PR_SET_VMA if/when it's used on
    // Android without being overridden by an allow-all policy, and
    // the constant will need to be defined locally.
    Arg<int> op(0);
    return Switch(op)
      .CASES((PR_GET_SECCOMP, // BroadcastSetThreadSandbox, etc.
              PR_SET_NAME,    // Thread creation
              PR_SET_DUMPABLE, // Crash reporting
              PR_SET_PTRACER), // Debug-mode crash handling
             Allow())
      .Default(InvalidSyscall());
  }

  Maybe<ResultExpr> EvaluateSocketCall(int aCall) const override {
    switch (aCall) {
    case SYS_RECVMSG:
    case SYS_SENDMSG:
      return Some(Allow());
    default:
      return Nothing();
    }
  }

  ResultExpr EvaluateSyscall(int sysno) const override {
    switch (sysno) {
      // Timekeeping
    case __NR_clock_gettime: {
      Arg<clockid_t> clk_id(0);
      return If(clk_id == CLOCK_MONOTONIC, Allow())
#ifdef CLOCK_MONOTONIC_COARSE
        // Used by SandboxReporter, among other things.
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
    case __NR_epoll_create1:
    case __NR_epoll_create:
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

      // Polyfill with tgkill; see above.
    case __NR_tkill:
      return Trap(TKillCompatTrap, nullptr);

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
    case __NR_prctl: {
      if (SandboxInfo::Get().Test(SandboxInfo::kHasSeccompTSync)) {
        return PrctlPolicy();
      }

      Arg<int> option(0);
      return If(option == PR_SET_NO_NEW_PRIVS,
                Trap(SetNoNewPrivsTrap, nullptr))
        .Else(PrctlPolicy());
    }

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
      return If(fd == STDERR_FILENO, Error(ENOTTY))
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
private:
  SandboxBrokerClient* mBroker;
  std::vector<int> mSyscallWhitelist;

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
    auto buf = reinterpret_cast<statstruct*>(aArgs.args[1]);
    return broker->Stat(path, buf);
  }

  static intptr_t LStatTrap(ArgsRef aArgs, void* aux) {
    auto broker = static_cast<SandboxBrokerClient*>(aux);
    auto path = reinterpret_cast<const char*>(aArgs.args[0]);
    auto buf = reinterpret_cast<statstruct*>(aArgs.args[1]);
    return broker->LStat(path, buf);
  }

  static intptr_t StatAtTrap(ArgsRef aArgs, void* aux) {
    auto broker = static_cast<SandboxBrokerClient*>(aux);
    auto fd = static_cast<int>(aArgs.args[0]);
    auto path = reinterpret_cast<const char*>(aArgs.args[1]);
    auto buf = reinterpret_cast<statstruct*>(aArgs.args[2]);
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

  static intptr_t ChmodTrap(ArgsRef aArgs, void* aux) {
    auto broker = static_cast<SandboxBrokerClient*>(aux);
    auto path = reinterpret_cast<const char*>(aArgs.args[0]);
    auto mode = static_cast<mode_t>(aArgs.args[1]);
    return broker->Chmod(path, mode);
  }

  static intptr_t LinkTrap(ArgsRef aArgs, void *aux) {
    auto broker = static_cast<SandboxBrokerClient*>(aux);
    auto path = reinterpret_cast<const char*>(aArgs.args[0]);
    auto path2 = reinterpret_cast<const char*>(aArgs.args[1]);
    return broker->Link(path, path2);
  }

  static intptr_t SymlinkTrap(ArgsRef aArgs, void *aux) {
    auto broker = static_cast<SandboxBrokerClient*>(aux);
    auto path = reinterpret_cast<const char*>(aArgs.args[0]);
    auto path2 = reinterpret_cast<const char*>(aArgs.args[1]);
    return broker->Symlink(path, path2);
  }

  static intptr_t RenameTrap(ArgsRef aArgs, void *aux) {
    auto broker = static_cast<SandboxBrokerClient*>(aux);
    auto path = reinterpret_cast<const char*>(aArgs.args[0]);
    auto path2 = reinterpret_cast<const char*>(aArgs.args[1]);
    return broker->Rename(path, path2);
  }

  static intptr_t MkdirTrap(ArgsRef aArgs, void* aux) {
    auto broker = static_cast<SandboxBrokerClient*>(aux);
    auto path = reinterpret_cast<const char*>(aArgs.args[0]);
    auto mode = static_cast<mode_t>(aArgs.args[1]);
    return broker->Mkdir(path, mode);
  }

  static intptr_t RmdirTrap(ArgsRef aArgs, void* aux) {
    auto broker = static_cast<SandboxBrokerClient*>(aux);
    auto path = reinterpret_cast<const char*>(aArgs.args[0]);
    return broker->Rmdir(path);
  }

  static intptr_t UnlinkTrap(ArgsRef aArgs, void* aux) {
    auto broker = static_cast<SandboxBrokerClient*>(aux);
    auto path = reinterpret_cast<const char*>(aArgs.args[0]);
    return broker->Unlink(path);
  }

  static intptr_t ReadlinkTrap(ArgsRef aArgs, void* aux) {
    auto broker = static_cast<SandboxBrokerClient*>(aux);
    auto path = reinterpret_cast<const char*>(aArgs.args[0]);
    auto buf = reinterpret_cast<char*>(aArgs.args[1]);
    auto size = static_cast<size_t>(aArgs.args[2]);
    return broker->Readlink(path, buf, size);
  }

  static intptr_t ReadlinkAtTrap(ArgsRef aArgs, void* aux) {
    auto broker = static_cast<SandboxBrokerClient*>(aux);
    auto fd = static_cast<int>(aArgs.args[0]);
    auto path = reinterpret_cast<const char*>(aArgs.args[1]);
    auto buf = reinterpret_cast<char*>(aArgs.args[2]);
    auto size = static_cast<size_t>(aArgs.args[3]);
    if (fd != AT_FDCWD && path[0] != '/') {
      SANDBOX_LOG_ERROR("unsupported fd-relative readlinkat(%d, %s, %p, %u)",
                        fd, path, buf, size);
      return BlockedSyscallTrap(aArgs, nullptr);
    }
    return broker->Readlink(path, buf, size);
  }

  static intptr_t GetPPidTrap(ArgsRef aArgs, void* aux) {
    // In a pid namespace, getppid() will return 0. We will return 0 instead
    // of the real parent pid to see what breaks when we introduce the
    // pid namespace (Bug 1151624).
    return 0;
  }

  static intptr_t SocketpairDatagramTrap(ArgsRef aArgs, void* aux) {
    auto fds = reinterpret_cast<int*>(aArgs.args[3]);
    // Return sequential packet sockets instead of the expected
    // datagram sockets; see bug 1355274 for details.
    return ConvertError(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds));
  }

  static intptr_t StatFsTrap(ArgsRef aArgs, void* aux) {
    // Warning: the kernel interface is not the C interface.  The
    // structs are different (<asm/statfs.h> vs. <sys/statfs.h>), and
    // the statfs64 version takes an additional size parameter.
    auto path = reinterpret_cast<const char*>(aArgs.args[0]);
    int fd = open(path, O_RDONLY | O_LARGEFILE);
    if (fd < 0) {
      return -errno;
    }

    intptr_t rv;
    switch (aArgs.nr) {
    case __NR_statfs: {
      auto buf = reinterpret_cast<void*>(aArgs.args[1]);
      rv = DoSyscall(__NR_fstatfs, fd, buf);
      break;
    }
#ifdef __NR_statfs64
    case __NR_statfs64: {
      auto sz = static_cast<size_t>(aArgs.args[1]);
      auto buf = reinterpret_cast<void*>(aArgs.args[2]);
      rv = DoSyscall(__NR_fstatfs64, fd, sz, buf);
      break;
    }
#endif
    default:
      MOZ_ASSERT(false);
      rv = -ENOSYS;
    }

    close(fd);
    return rv;
  }

public:
  explicit ContentSandboxPolicy(SandboxBrokerClient* aBroker,
                                const std::vector<int>& aSyscallWhitelist)
    : mBroker(aBroker),
      mSyscallWhitelist(aSyscallWhitelist) {}
  ~ContentSandboxPolicy() override = default;
  Maybe<ResultExpr> EvaluateSocketCall(int aCall) const override {
    switch(aCall) {
    case SYS_RECVFROM:
    case SYS_SENDTO:
    case SYS_SENDMMSG: // libresolv via libasyncns; see bug 1355274
      return Some(Allow());

    case SYS_SOCKETPAIR: {
      // See bug 1066750.
      if (!kSocketCallHasArgs) {
        // We can't filter the args if the platform passes them by pointer.
        return Some(Allow());
      }
      Arg<int> domain(0), type(1);
      return Some(If(domain == AF_UNIX,
                     Switch(type & ~SOCK_CLOEXEC)
                     .Case(SOCK_STREAM, Allow())
                     .Case(SOCK_SEQPACKET, Allow())
                     .Case(SOCK_DGRAM, Trap(SocketpairDatagramTrap, nullptr))
                     .Default(InvalidSyscall()))
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
  Maybe<ResultExpr> EvaluateIpcCall(int aCall) const override {
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

#ifdef MOZ_PULSEAUDIO
  ResultExpr PrctlPolicy() const override {
    Arg<int> op(0);
    return If(op == PR_GET_NAME, Allow())
      .Else(SandboxPolicyCommon::PrctlPolicy());
  }
#endif

  ResultExpr EvaluateSyscall(int sysno) const override {
    // Straight allow for anything that got overriden via prefs
    if (std::find(mSyscallWhitelist.begin(), mSyscallWhitelist.end(), sysno)
        != mSyscallWhitelist.end()) {
      if (SandboxInfo::Get().Test(SandboxInfo::kVerbose)) {
        SANDBOX_LOG_ERROR("Allowing syscall nr %d via whitelist", sysno);
      }
      return Allow();
    }
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
      case __NR_chmod:
        return Trap(ChmodTrap, mBroker);
      case __NR_link:
        return Trap(LinkTrap, mBroker);
      case __NR_mkdir:
        return Trap(MkdirTrap, mBroker);
      case __NR_symlink:
        return Trap(SymlinkTrap, mBroker);
      case __NR_rename:
        return Trap(RenameTrap, mBroker);
      case __NR_rmdir:
        return Trap(RmdirTrap, mBroker);
      case __NR_unlink:
        return Trap(UnlinkTrap, mBroker);
      case __NR_readlink:
        return Trap(ReadlinkTrap, mBroker);
      case __NR_readlinkat:
        return Trap(ReadlinkAtTrap, mBroker);
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
      case __NR_chmod:
      case __NR_link:
      case __NR_mkdir:
      case __NR_symlink:
      case __NR_rename:
      case __NR_rmdir:
      case __NR_unlink:
      case __NR_readlink:
      case __NR_readlinkat:
        return Allow();
      }
    }

    switch (sysno) {
#ifdef DESKTOP
    case __NR_getppid:
      return Trap(GetPPidTrap, nullptr);

    CASES_FOR_statfs:
      return Trap(StatFsTrap, nullptr);

      // GTK's theme parsing tries to getcwd() while sandboxed, but
      // only during Talos runs.
    case __NR_getcwd:
      return Error(ENOENT);

    CASES_FOR_fstatfs: // fontconfig, pulseaudio, GIO (see also statfs)
    CASES_FOR_fchown: // pulseaudio
    case __NR_fchmod: // pulseaudio
    case __NR_flock: // graphics
      return Allow();

      // Bug 1354731: proprietary GL drivers try to mknod() their devices
    case __NR_mknod: {
      Arg<mode_t> mode(1);
      return If((mode & S_IFMT) == S_IFCHR, Error(EPERM))
        .Else(InvalidSyscall());
    }

      // For ORBit called by GConf (on some systems) to get proxy
      // settings.  Can remove when bug 1325242 happens in some form.
    case __NR_utime:
      return Error(EPERM);
#endif

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

#ifdef MOZ_ALSA
    case __NR_ioctl:
      return Allow();
#else
    case __NR_ioctl: {
      static const unsigned long kTypeMask = _IOC_TYPEMASK << _IOC_TYPESHIFT;
      static const unsigned long kTtyIoctls = TIOCSTI & kTypeMask;
      // On some older architectures (but not x86 or ARM), ioctls are
      // assigned type fields differently, and the TIOC/TC/FIO group
      // isn't all the same type.  If/when we support those archs,
      // this would need to be revised (but really this should be a
      // default-deny policy; see below).
      static_assert(kTtyIoctls == (TCSETA & kTypeMask) &&
                    kTtyIoctls == (FIOASYNC & kTypeMask),
                    "tty-related ioctls use the same type");

      Arg<unsigned long> request(1);
      auto shifted_type = request & kTypeMask;

      // Rust's stdlib seems to use FIOCLEX instead of equivalent fcntls.
      return If(request == FIOCLEX, Allow())
        // Rust's stdlib also uses FIONBIO instead of equivalent fcntls.
        .ElseIf(request == FIONBIO, Allow())
        // ffmpeg, and anything else that calls isatty(), will be told
        // that nothing is a typewriter:
        .ElseIf(request == TCGETS, Error(ENOTTY))
        // Allow anything that isn't a tty ioctl, for now; bug 1302711
        // will cover changing this to a default-deny policy.
        .ElseIf(shifted_type != kTtyIoctls, Allow())
        .Else(SandboxPolicyCommon::EvaluateSyscall(sysno));
    }
#endif // !MOZ_ALSA

    CASES_FOR_fcntl: {
      Arg<int> cmd(1);
      Arg<int> flags(2);
      // Typical use of F_SETFL is to modify the flags returned by
      // F_GETFL and write them back, including some flags that
      // F_SETFL ignores.  This is a default-deny policy in case any
      // new SETFL-able flags are added.  (In particular we want to
      // forbid O_ASYNC; see bug 1328896, but also see bug 1408438.)
      static const int ignored_flags = O_ACCMODE | O_LARGEFILE_REAL | O_CLOEXEC;
      static const int allowed_flags = ignored_flags | O_APPEND | O_NONBLOCK;
      return Switch(cmd)
        // Close-on-exec is meaningless when execve isn't allowed, but
        // NSPR reads the bit and asserts that it has the expected value.
        .Case(F_GETFD, Allow())
        .Case(F_SETFD,
              If((flags & ~FD_CLOEXEC) == 0, Allow())
              .Else(InvalidSyscall()))
        .Case(F_GETFL, Allow())
        .Case(F_SETFL,
              If((flags & ~allowed_flags) == 0, Allow())
              .Else(InvalidSyscall()))
        .Case(F_DUPFD_CLOEXEC, Allow())
        // Nvidia GL and fontconfig (newer versions) use fcntl file locking.
        .Case(F_SETLK, Allow())
#ifdef F_SETLK64
        .Case(F_SETLK64, Allow())
#endif
        // Pulseaudio uses F_SETLKW, as does fontconfig.
        .Case(F_SETLKW, Allow())
#ifdef F_SETLKW64
        .Case(F_SETLKW64, Allow())
#endif
        .Default(SandboxPolicyCommon::EvaluateSyscall(sysno));
    }

    case __NR_mprotect:
    case __NR_brk:
    case __NR_madvise:
      // libc's realloc uses mremap (Bug 1286119); wasm does too (bug 1342385).
    case __NR_mremap:
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

    case __NR_prlimit64: {
      // Allow only the getrlimit() use case.  (glibc seems to use
      // only pid 0 to indicate the current process; pid == getpid()
      // is equivalent and could also be allowed if needed.)
      Arg<pid_t> pid(0);
      // This is really a const struct ::rlimit*, but Arg<> doesn't
      // work with pointers, only integer types.
      Arg<uintptr_t> new_limit(2);
      return If(AllOf(pid == 0, new_limit == 0), Allow())
        .Else(InvalidSyscall());
    }

      // PulseAudio calls umask, even though it's unsafe in
      // multithreaded applications.  But, allowing it here doesn't
      // really do anything one way or the other, now that file
      // accesses are brokered to another process.
    case __NR_umask:
      return Allow();

    case __NR_kill: {
      Arg<int> sig(1);
      // PulseAudio uses kill(pid, 0) to check if purported owners of
      // shared memory files are still alive; see bug 1397753 for more
      // details.
      return If(sig == 0, Error(EPERM))
        .Else(InvalidSyscall());
    }

    case __NR_wait4:
#ifdef __NR_waitpid
    case __NR_waitpid:
#endif
      // NSPR will start a thread to wait for child processes even if
      // fork() fails; see bug 227246 and bug 1299581.
      return Error(ECHILD);

    case __NR_eventfd2:
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

#ifdef __NR_fadvise64_64
    case __NR_fadvise64_64:
      return Allow();
#endif

    case __NR_fallocate:
      return Allow();

    case __NR_get_mempolicy:
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

#ifdef MOZ_JPROF
    case __NR_setitimer:
      return Allow();
#endif // MOZ_JPROF

    default:
      return SandboxPolicyCommon::EvaluateSyscall(sysno);
    }
  }
};

UniquePtr<sandbox::bpf_dsl::Policy>
GetContentSandboxPolicy(SandboxBrokerClient* aMaybeBroker,
                        const std::vector<int>& aSyscallWhitelist)
{
  return MakeUnique<ContentSandboxPolicy>(aMaybeBroker, aSyscallWhitelist);
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
    const auto files = static_cast<const SandboxOpenedFiles*>(aux);
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
      return -EROFS;
    }
    int fd = files->GetDesc(path);
    if (fd < 0) {
      // SandboxOpenedFile::GetDesc already logged about this, if appropriate.
      return -ENOENT;
    }
    return fd;
  }

  static intptr_t SchedTrap(ArgsRef aArgs, void* aux)
  {
    const pid_t tid = syscall(__NR_gettid);
    if (aArgs.args[0] == static_cast<uint64_t>(tid)) {
      return DoSyscall(aArgs.nr,
                       0,
                       static_cast<uintptr_t>(aArgs.args[1]),
                       static_cast<uintptr_t>(aArgs.args[2]),
                       static_cast<uintptr_t>(aArgs.args[3]),
                       static_cast<uintptr_t>(aArgs.args[4]),
                       static_cast<uintptr_t>(aArgs.args[5]));
    }
    SANDBOX_LOG_ERROR("unsupported tid in SchedTrap");
    return BlockedSyscallTrap(aArgs, nullptr);
  }

  static intptr_t UnameTrap(const sandbox::arch_seccomp_data& aArgs,
                            void* aux)
  {
    const auto buf = reinterpret_cast<struct utsname*>(aArgs.args[0]);
    PodZero(buf);
    // The real uname() increases fingerprinting risk for no benefit.
    // This is close enough.
    strcpy(buf->sysname, "Linux");
    strcpy(buf->version, "3");
    return 0;
  };

  static intptr_t FcntlTrap(const sandbox::arch_seccomp_data& aArgs,
                            void* aux)
  {
    const auto cmd = static_cast<int>(aArgs.args[1]);
    switch (cmd) {
      // This process can't exec, so the actual close-on-exec flag
      // doesn't matter; have it always read as true and ignore writes.
    case F_GETFD:
      return O_CLOEXEC;
    case F_SETFD:
      return 0;
    default:
      return -ENOSYS;
    }
  }

  const SandboxOpenedFiles* mFiles;
public:
  explicit GMPSandboxPolicy(const SandboxOpenedFiles* aFiles)
  : mFiles(aFiles)
  { }

  ~GMPSandboxPolicy() override = default;

  ResultExpr EvaluateSyscall(int sysno) const override {
    switch (sysno) {
      // Simulate opening the plugin file.
#ifdef __NR_open
    case __NR_open:
#endif
    case __NR_openat:
      return Trap(OpenTrap, mFiles);

      // ipc::Shmem
    case __NR_mprotect:
      return Allow();
    case __NR_madvise: {
      Arg<int> advice(2);
      return If(advice == MADV_DONTNEED, Allow())
        .ElseIf(advice == MADV_FREE, Allow())
        .ElseIf(advice == MADV_HUGEPAGE, Allow())
        .ElseIf(advice == MADV_NOHUGEPAGE, Allow())
#ifdef MOZ_ASAN
        .ElseIf(advice == MADV_DONTDUMP, Allow())
#endif
        .Else(InvalidSyscall());
    }
    case __NR_brk:
    CASES_FOR_geteuid:
      return Allow();
    case __NR_sched_get_priority_min:
    case __NR_sched_get_priority_max:
      return Allow();
    case __NR_sched_getparam:
    case __NR_sched_getscheduler:
    case __NR_sched_setscheduler: {
      Arg<pid_t> pid(0);
      return If(pid == 0, Allow())
        .Else(Trap(SchedTrap, nullptr));
    }

    // For clock(3) on older glibcs; bug 1304220.
    case __NR_times:
      return Allow();

    // Bug 1372428
    case __NR_uname:
      return Trap(UnameTrap, nullptr);
    CASES_FOR_fcntl:
      return Trap(FcntlTrap, nullptr);

    case __NR_dup:
      return Allow();

    default:
      return SandboxPolicyCommon::EvaluateSyscall(sysno);
    }
  }
};

UniquePtr<sandbox::bpf_dsl::Policy>
GetMediaSandboxPolicy(const SandboxOpenedFiles* aFiles)
{
  return UniquePtr<sandbox::bpf_dsl::Policy>(new GMPSandboxPolicy(aFiles));
}

#endif // MOZ_GMP_SANDBOX

} // namespace mozilla
