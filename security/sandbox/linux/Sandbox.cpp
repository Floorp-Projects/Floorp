/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Sandbox.h"

#include "LinuxCapabilities.h"
#include "LinuxSched.h"
#include "SandboxChroot.h"
#include "SandboxFilter.h"
#include "SandboxInternal.h"
#include "SandboxLogging.h"
#include "SandboxUtil.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/futex.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <unistd.h>

#include "mozilla/Atomics.h"
#include "mozilla/SandboxInfo.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/unused.h"
#include "sandbox/linux/seccomp-bpf/linux_seccomp.h"
#if defined(ANDROID)
#include "sandbox/linux/services/android_ucontext.h"
#endif
#include "sandbox/linux/services/linux_syscalls.h"

#ifdef MOZ_ASAN
// Copy libsanitizer declarations to avoid depending on ASAN headers.
// See also bug 1081242 comment #4.
extern "C" {
namespace __sanitizer {
// Win64 uses long long, but this is Linux.
typedef signed long sptr;
} // namespace __sanitizer

typedef struct {
  int coverage_sandboxed;
  __sanitizer::sptr coverage_fd;
  unsigned int coverage_max_block_size;
} __sanitizer_sandbox_arguments;

MOZ_IMPORT_API void
__sanitizer_sandbox_on_notify(__sanitizer_sandbox_arguments *args);
} // extern "C"
#endif // MOZ_ASAN

namespace mozilla {

#ifdef ANDROID
SandboxCrashFunc gSandboxCrashFunc;
#endif

#ifdef MOZ_GMP_SANDBOX
// For media plugins, we can start the sandbox before we dlopen the
// module, so we have to pre-open the file and simulate the sandboxed
// open().
static int gMediaPluginFileDesc = -1;
static const char *gMediaPluginFilePath;
#endif

static UniquePtr<SandboxChroot> gChrootHelper;

/**
 * This is the SIGSYS handler function. It is used to report to the user
 * which system call has been denied by Seccomp.
 * This function also makes the process exit as denying the system call
 * will otherwise generally lead to unexpected behavior from the process,
 * since we don't know if all functions will handle such denials gracefully.
 *
 * @see InstallSyscallReporter() function.
 */
static void
Reporter(int nr, siginfo_t *info, void *void_context)
{
  ucontext_t *ctx = static_cast<ucontext_t*>(void_context);
  unsigned long syscall_nr, args[6];
  pid_t pid = getpid();

  if (nr != SIGSYS) {
    return;
  }
  if (info->si_code != SYS_SECCOMP) {
    return;
  }
  if (!ctx) {
    return;
  }

  syscall_nr = SECCOMP_SYSCALL(ctx);
  args[0] = SECCOMP_PARM1(ctx);
  args[1] = SECCOMP_PARM2(ctx);
  args[2] = SECCOMP_PARM3(ctx);
  args[3] = SECCOMP_PARM4(ctx);
  args[4] = SECCOMP_PARM5(ctx);
  args[5] = SECCOMP_PARM6(ctx);

#if defined(ANDROID) && ANDROID_VERSION < 16
  // Bug 1093893: Translate tkill to tgkill for pthread_kill; fixed in
  // bionic commit 10c8ce59a (in JB and up; API level 16 = Android 4.1).
  if (syscall_nr == __NR_tkill) {
    intptr_t ret = syscall(__NR_tgkill, getpid(), args[0], args[1]);
    if (ret < 0) {
      ret = -errno;
    }
    SECCOMP_RESULT(ctx) = ret;
    return;
  }
#endif

#ifdef MOZ_GMP_SANDBOX
  if (syscall_nr == __NR_open && gMediaPluginFilePath) {
    const char *path = reinterpret_cast<const char*>(args[0]);
    int flags = int(args[1]);

    if ((flags & O_ACCMODE) != O_RDONLY) {
      SANDBOX_LOG_ERROR("non-read-only open of file %s attempted (flags=0%o)",
                        path, flags);
    } else if (strcmp(path, gMediaPluginFilePath) != 0) {
      SANDBOX_LOG_ERROR("attempt to open file %s which is not the media plugin"
                        " %s", path, gMediaPluginFilePath);
    } else if (gMediaPluginFileDesc == -1) {
      SANDBOX_LOG_ERROR("multiple opens of media plugin file unimplemented");
    } else {
      SECCOMP_RESULT(ctx) = gMediaPluginFileDesc;
      gMediaPluginFileDesc = -1;
      return;
    }
  }
#endif

  SANDBOX_LOG_ERROR("seccomp sandbox violation: pid %d, syscall %lu,"
                    " args %lu %lu %lu %lu %lu %lu.  Killing process.",
                    pid, syscall_nr,
                    args[0], args[1], args[2], args[3], args[4], args[5]);

  // Bug 1017393: record syscall number somewhere useful.
  info->si_addr = reinterpret_cast<void*>(syscall_nr);

  gSandboxCrashFunc(nr, info, void_context);
  _exit(127);
}

/**
 * The reporter is called when the process receives a SIGSYS signal.
 * The signal is sent by the kernel when Seccomp encounter a system call
 * that has not been allowed.
 * We register an action for that signal (calling the Reporter function).
 *
 * This function should not be used in production and thus generally be
 * called from debug code. In production, the process is directly killed.
 * For this reason, the function is ifdef'd, as there is no reason to
 * compile it while unused.
 *
 * @return 0 on success, -1 on failure.
 * @see Reporter() function.
 */
static int
InstallSyscallReporter(void)
{
  struct sigaction act;
  sigset_t mask;
  memset(&act, 0, sizeof(act));
  sigemptyset(&mask);
  sigaddset(&mask, SIGSYS);

  act.sa_sigaction = &Reporter;
  act.sa_flags = SA_SIGINFO | SA_NODEFER;
  if (sigaction(SIGSYS, &act, nullptr) < 0) {
    return -1;
  }
  if (sigemptyset(&mask) ||
    sigaddset(&mask, SIGSYS) ||
    sigprocmask(SIG_UNBLOCK, &mask, nullptr)) {
      return -1;
  }
  return 0;
}

/**
 * This function installs the syscall filter, a.k.a. seccomp.
 * PR_SET_NO_NEW_PRIVS ensures that it is impossible to grant more
 * syscalls to the process beyond this point (even after fork()).
 * SECCOMP_MODE_FILTER is the "bpf" mode of seccomp which allows
 * to pass a bpf program (in our case, it contains a syscall
 * whitelist).
 *
 * Reports failure by crashing.
 *
 * @see sock_fprog (the seccomp_prog).
 */
static void
InstallSyscallFilter(const sock_fprog *prog)
{
  if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
    SANDBOX_LOG_ERROR("prctl(PR_SET_NO_NEW_PRIVS) failed: %s", strerror(errno));
    MOZ_CRASH("prctl(PR_SET_NO_NEW_PRIVS)");
  }

  if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, (unsigned long)prog, 0, 0)) {
    SANDBOX_LOG_ERROR("prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER) failed: %s",
                      strerror(errno));
    MOZ_CRASH("prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER)");
  }
}

// Use signals for permissions that need to be set per-thread.
// The communication channel from the signal handler back to the main thread.
static mozilla::Atomic<int> sSetSandboxDone;
// Pass the filter itself through a global.
static const sock_fprog *sSetSandboxFilter;

// We have to dynamically allocate the signal number; see bug 1038900.
// This function returns the first realtime signal currently set to
// default handling (i.e., not in use), or 0 if none could be found.
//
// WARNING: if this function or anything similar to it (including in
// external libraries) is used on multiple threads concurrently, there
// will be a race condition.
static int
FindFreeSignalNumber()
{
  for (int signum = SIGRTMIN; signum <= SIGRTMAX; ++signum) {
    struct sigaction sa;

    if (sigaction(signum, nullptr, &sa) == 0 &&
        (sa.sa_flags & SA_SIGINFO) == 0 &&
        sa.sa_handler == SIG_DFL) {
      return signum;
    }
  }
  return 0;
}

// Returns true if sandboxing was enabled, or false if sandboxing
// already was enabled.  Crashes if sandboxing could not be enabled.
static bool
SetThreadSandbox()
{
  if (prctl(PR_GET_SECCOMP, 0, 0, 0, 0) == 0) {
    InstallSyscallFilter(sSetSandboxFilter);
    return true;
  }
  return false;
}

static void
SetThreadSandboxHandler(int signum)
{
  // The non-zero number sent back to the main thread indicates
  // whether action was taken.
  if (SetThreadSandbox()) {
    sSetSandboxDone = 2;
  } else {
    sSetSandboxDone = 1;
  }
  // Wake up the main thread.  See the FUTEX_WAIT call, below, for an
  // explanation.
  syscall(__NR_futex, reinterpret_cast<int*>(&sSetSandboxDone),
          FUTEX_WAKE, 1);
}

static void
BroadcastSetThreadSandbox(SandboxType aType)
{
  int signum;
  pid_t pid, tid, myTid;
  DIR *taskdp;
  struct dirent *de;
  SandboxFilter filter(&sSetSandboxFilter, aType,
                       SandboxInfo::Get().Test(SandboxInfo::kVerbose));

  static_assert(sizeof(mozilla::Atomic<int>) == sizeof(int),
                "mozilla::Atomic<int> isn't represented by an int");
  pid = getpid();
  myTid = syscall(__NR_gettid);
  taskdp = opendir("/proc/self/task");
  if (taskdp == nullptr) {
    SANDBOX_LOG_ERROR("opendir /proc/self/task: %s\n", strerror(errno));
    MOZ_CRASH();
  }

  if (gChrootHelper) {
    gChrootHelper->Invoke();
    gChrootHelper = nullptr;
  }

  signum = FindFreeSignalNumber();
  if (signum == 0) {
    SANDBOX_LOG_ERROR("No available signal numbers!");
    MOZ_CRASH();
  }
  void (*oldHandler)(int);
  oldHandler = signal(signum, SetThreadSandboxHandler);
  if (oldHandler != SIG_DFL) {
    // See the comment on FindFreeSignalNumber about race conditions.
    SANDBOX_LOG_ERROR("signal %d in use by handler %p!\n", signum, oldHandler);
    MOZ_CRASH();
  }

  // In case this races with a not-yet-deprivileged thread cloning
  // itself, repeat iterating over all threads until we find none
  // that are still privileged.
  bool sandboxProgress;
  do {
    sandboxProgress = false;
    // For each thread...
    while ((de = readdir(taskdp))) {
      char *endptr;
      tid = strtol(de->d_name, &endptr, 10);
      if (*endptr != '\0' || tid <= 0) {
        // Not a task ID.
        continue;
      }
      if (tid == myTid) {
        // Drop this thread's privileges last, below, so we can
        // continue to signal other threads.
        continue;
      }
      // Reset the futex cell and signal.
      sSetSandboxDone = 0;
      if (syscall(__NR_tgkill, pid, tid, signum) != 0) {
        if (errno == ESRCH) {
          SANDBOX_LOG_ERROR("Thread %d unexpectedly exited.", tid);
          // Rescan threads, in case it forked before exiting.
          sandboxProgress = true;
          continue;
        }
        SANDBOX_LOG_ERROR("tgkill(%d,%d): %s\n", pid, tid, strerror(errno));
        MOZ_CRASH();
      }
      // It's unlikely, but if the thread somehow manages to exit
      // after receiving the signal but before entering the signal
      // handler, we need to avoid blocking forever.
      //
      // Using futex directly lets the signal handler send the wakeup
      // from an async signal handler (pthread mutex/condvar calls
      // aren't allowed), and to use a relative timeout that isn't
      // affected by changes to the system clock (not possible with
      // POSIX semaphores).
      //
      // If a thread doesn't respond within a reasonable amount of
      // time, but still exists, we crash -- the alternative is either
      // blocking forever or silently losing security, and it
      // shouldn't actually happen.
      static const int crashDelay = 10; // seconds
      struct timespec timeLimit;
      clock_gettime(CLOCK_MONOTONIC, &timeLimit);
      timeLimit.tv_sec += crashDelay;
      while (true) {
        static const struct timespec futexTimeout = { 0, 10*1000*1000 }; // 10ms
        // Atomically: if sSetSandboxDone == 0, then sleep.
        if (syscall(__NR_futex, reinterpret_cast<int*>(&sSetSandboxDone),
                  FUTEX_WAIT, 0, &futexTimeout) != 0) {
          if (errno != EWOULDBLOCK && errno != ETIMEDOUT && errno != EINTR) {
            SANDBOX_LOG_ERROR("FUTEX_WAIT: %s\n", strerror(errno));
            MOZ_CRASH();
          }
        }
        // Did the handler finish?
        if (sSetSandboxDone > 0) {
          if (sSetSandboxDone == 2) {
            sandboxProgress = true;
          }
          break;
        }
        // Has the thread ceased to exist?
        if (syscall(__NR_tgkill, pid, tid, 0) != 0) {
          if (errno == ESRCH) {
            SANDBOX_LOG_ERROR("Thread %d unexpectedly exited.", tid);
          }
          // Rescan threads, in case it forked before exiting.
          // Also, if it somehow failed in a way that wasn't ESRCH,
          // and still exists, that will be handled on the next pass.
          sandboxProgress = true;
          break;
        }
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec > timeLimit.tv_nsec ||
            (now.tv_sec == timeLimit.tv_nsec &&
             now.tv_nsec > timeLimit.tv_nsec)) {
          SANDBOX_LOG_ERROR("Thread %d unresponsive for %d seconds."
                            "  Killing process.",
                            tid, crashDelay);
          MOZ_CRASH();
        }
      }
    }
    rewinddir(taskdp);
  } while (sandboxProgress);
  oldHandler = signal(signum, SIG_DFL);
  if (oldHandler != SetThreadSandboxHandler) {
    // See the comment on FindFreeSignalNumber about race conditions.
    SANDBOX_LOG_ERROR("handler for signal %d was changed to %p!",
                      signum, oldHandler);
    MOZ_CRASH();
  }
  unused << closedir(taskdp);
  // And now, deprivilege the main thread:
  SetThreadSandbox();
}

// Common code for sandbox startup.
static void
SetCurrentProcessSandbox(SandboxType aType)
{
  MOZ_ASSERT(gSandboxCrashFunc);

  if (InstallSyscallReporter()) {
    SANDBOX_LOG_ERROR("install_syscall_reporter() failed\n");
  }

#ifdef MOZ_ASAN
  __sanitizer_sandbox_arguments asanArgs;
  asanArgs.coverage_sandboxed = 1;
  asanArgs.coverage_fd = -1;
  asanArgs.coverage_max_block_size = 0;
  __sanitizer_sandbox_on_notify(&asanArgs);
#endif

  BroadcastSetThreadSandbox(aType);
}

void
SandboxEarlyInit(GeckoProcessType aType, bool aIsNuwa)
{
  MOZ_RELEASE_ASSERT(IsSingleThreaded());

  // Which kinds of resource isolation (of those that need to be set
  // up at this point) can be used by this process?
  bool canChroot = false;
  bool canUnshareNet = false;
  bool canUnshareIPC = false;

  switch (aType) {
  case GeckoProcessType_Default:
    MOZ_ASSERT(false, "SandboxEarlyInit in parent process");
    return;
#ifdef MOZ_GMP_SANDBOX
  case GeckoProcessType_GMPlugin:
    canUnshareNet = true;
    canUnshareIPC = true;
    canChroot = true;
    break;
#endif
    // In the future, content processes will be able to use some of
    // these.
  default:
    // Other cases intentionally left blank.
    break;
  }

  // If there's nothing to do, then we're done.
  if (!canChroot && !canUnshareNet && !canUnshareIPC) {
    return;
  }

  // If capabilities can't be gained, then nothing can be done.
  const SandboxInfo info = SandboxInfo::Get();
  if (!info.Test(SandboxInfo::kHasUserNamespaces)) {
    return;
  }

  // The failure cases for the various unshares, and setting up the
  // chroot helper, don't strictly need to be fatal -- but they also
  // shouldn't fail on any reasonable system, so let's take the small
  // risk of breakage over the small risk of quietly providing less
  // security than we expect.  (Unlike in SandboxInfo, this is in the
  // child process, so crashing here isn't as severe a response to the
  // unexpected.)
  if (!UnshareUserNamespace()) {
    SANDBOX_LOG_ERROR("unshare(CLONE_NEWUSER): %s", strerror(errno));
    // If CanCreateUserNamespace (SandboxInfo.cpp) returns true, then
    // the unshare shouldn't have failed.
    MOZ_CRASH("unshare(CLONE_NEWUSER)");
  }
  // No early returns after this point!  We need to drop the
  // capabilities that were gained by unsharing the user namesapce.

  if (canUnshareIPC && syscall(__NR_unshare, CLONE_NEWIPC) != 0) {
    SANDBOX_LOG_ERROR("unshare(CLONE_NEWIPC): %s", strerror(errno));
    MOZ_CRASH("unshare(CLONE_NEWIPC)");
  }

  if (canUnshareNet && syscall(__NR_unshare, CLONE_NEWNET) != 0) {
    SANDBOX_LOG_ERROR("unshare(CLONE_NEWNET): %s", strerror(errno));
    MOZ_CRASH("unshare(CLONE_NEWNET)");
  }

  if (canChroot) {
    gChrootHelper = MakeUnique<SandboxChroot>();
    if (!gChrootHelper->Prepare()) {
      SANDBOX_LOG_ERROR("failed to set up chroot helper");
      MOZ_CRASH("SandboxChroot::Prepare");
    }
  }

  if (!LinuxCapabilities().SetCurrent()) {
    SANDBOX_LOG_ERROR("dropping capabilities: %s", strerror(errno));
    MOZ_CRASH("can't drop capabilities");
  }
}

#ifdef MOZ_CONTENT_SANDBOX
/**
 * Starts the seccomp sandbox for a content process.  Should be called
 * only once, and before any potentially harmful content is loaded.
 *
 * Will normally make the process exit on failure.
*/
void
SetContentProcessSandbox()
{
  if (!SandboxInfo::Get().Test(SandboxInfo::kEnabledForContent)) {
    return;
  }

  SetCurrentProcessSandbox(kSandboxContentProcess);
}
#endif // MOZ_CONTENT_SANDBOX

#ifdef MOZ_GMP_SANDBOX
/**
 * Starts the seccomp sandbox for a media plugin process.  Should be
 * called only once, and before any potentially harmful content is
 * loaded -- including the plugin itself, if it's considered untrusted.
 *
 * The file indicated by aFilePath, if non-null, can be open()ed once
 * read-only after the sandbox starts; it should be the .so file
 * implementing the not-yet-loaded plugin.
 *
 * Will normally make the process exit on failure.
*/
void
SetMediaPluginSandbox(const char *aFilePath)
{
  if (!SandboxInfo::Get().Test(SandboxInfo::kEnabledForMedia)) {
    return;
  }

  if (aFilePath) {
    gMediaPluginFilePath = strdup(aFilePath);
    gMediaPluginFileDesc = open(aFilePath, O_RDONLY | O_CLOEXEC);
    if (gMediaPluginFileDesc == -1) {
      SANDBOX_LOG_ERROR("failed to open plugin file %s: %s",
                        aFilePath, strerror(errno));
      MOZ_CRASH();
    }
  }
  // Finally, start the sandbox.
  SetCurrentProcessSandbox(kSandboxMediaPlugin);
}
#endif // MOZ_GMP_SANDBOX

} // namespace mozilla
