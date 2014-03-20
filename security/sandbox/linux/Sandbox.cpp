/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Sandbox.h"

#include <unistd.h>
#include <stdio.h>
#include <sys/ptrace.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <signal.h>
#include <string.h>
#include <linux/futex.h>
#include <sys/time.h>
#include <dirent.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>

#include "mozilla/Atomics.h"
#include "mozilla/NullPtr.h"
#include "mozilla/unused.h"
#include "mozilla/dom/Exceptions.h"
#include "nsString.h"
#include "nsThreadUtils.h"

#ifdef MOZ_CRASHREPORTER
#include "nsExceptionHandler.h"
#endif

#if defined(ANDROID)
#include "android_ucontext.h"
#include <android/log.h>
#endif

#if defined(MOZ_CONTENT_SANDBOX)
#include "linux_seccomp.h"
#include "SandboxFilter.h"
#endif

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG 1
#endif
#include "prlog.h"
#include "prenv.h"

namespace mozilla {
#if defined(ANDROID)
#define LOG_ERROR(args...) __android_log_print(ANDROID_LOG_ERROR, "Sandbox", ## args)
#elif defined(PR_LOGGING)
static PRLogModuleInfo* gSeccompSandboxLog;
#define LOG_ERROR(args...) PR_LOG(gSeccompSandboxLog, PR_LOG_ERROR, (args))
#else
#define LOG_ERROR(args...)
#endif

/**
 * Log JS stack info in the same place as the sandbox violation
 * message.  Useful in case the responsible code is JS and all we have
 * are logs and a minidump with the C++ stacks (e.g., on TBPL).
 */
static void
SandboxLogJSStack(void)
{
  if (!NS_IsMainThread()) {
    // This might be a worker thread... or it might be a non-JS
    // thread, or a non-NSPR thread.  There's isn't a good API for
    // dealing with this, yet.
    return;
  }
  nsCOMPtr<nsIStackFrame> frame = dom::GetCurrentJSStack();
  for (int i = 0; frame != nullptr; ++i) {
    nsAutoCString fileName, funName;
    int32_t lineNumber;

    // Don't stop unwinding if an attribute can't be read.
    fileName.SetIsVoid(true);
    unused << frame->GetFilename(fileName);
    lineNumber = 0;
    unused << frame->GetLineNumber(&lineNumber);
    funName.SetIsVoid(true);
    unused << frame->GetName(funName);

    if (!funName.IsVoid() || !fileName.IsVoid()) {
      LOG_ERROR("JS frame %d: %s %s line %d", i,
                funName.IsVoid() ? "(anonymous)" : funName.get(),
                fileName.IsVoid() ? "(no file)" : fileName.get(),
                lineNumber);
    }

    nsCOMPtr<nsIStackFrame> nextFrame;
    nsresult rv = frame->GetCaller(getter_AddRefs(nextFrame));
    NS_ENSURE_SUCCESS_VOID(rv);
    frame = nextFrame;
  }
}

/**
 * This is the SIGSYS handler function. It is used to report to the user
 * which system call has been denied by Seccomp.
 * This function also makes the process exit as denying the system call
 * will otherwise generally lead to unexpected behavior from the process,
 * since we don't know if all functions will handle such denials gracefully.
 *
 * @see InstallSyscallReporter() function.
 */
#ifdef MOZ_CONTENT_SANDBOX_REPORTER
static void
Reporter(int nr, siginfo_t *info, void *void_context)
{
  ucontext_t *ctx = static_cast<ucontext_t*>(void_context);
  unsigned long syscall_nr, args[6];
  pid_t pid = getpid(), tid = syscall(__NR_gettid);

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

  LOG_ERROR("seccomp sandbox violation: pid %d, syscall %lu, args %lu %lu %lu"
            " %lu %lu %lu.  Killing process.", pid, syscall_nr,
            args[0], args[1], args[2], args[3], args[4], args[5]);

#ifdef MOZ_CRASHREPORTER
  bool dumped = CrashReporter::WriteMinidumpForSigInfo(nr, info, void_context);
  if (!dumped) {
    LOG_ERROR("Failed to write minidump");
  }
#endif

  // Do this last, in case it crashes or deadlocks.
  SandboxLogJSStack();

  // Try to reraise, so the parent sees that this process crashed.
  // (If tgkill is forbidden, then seccomp will raise SIGSYS, which
  // also accomplishes that goal.)
  signal(SIGSYS, SIG_DFL);
  syscall(__NR_tgkill, pid, tid, nr);
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
#endif

/**
 * This function installs the syscall filter, a.k.a. seccomp.
 * PR_SET_NO_NEW_PRIVS ensures that it is impossible to grant more
 * syscalls to the process beyond this point (even after fork()).
 * SECCOMP_MODE_FILTER is the "bpf" mode of seccomp which allows
 * to pass a bpf program (in our case, it contains a syscall
 * whitelist).
 *
 * @return 0 on success, 1 on failure.
 * @see sock_fprog (the seccomp_prog).
 */
static int
InstallSyscallFilter(void)
{
#ifdef MOZ_DMD
  char* e = PR_GetEnv("DMD");
  if (e && strcmp(e, "") != 0 && strcmp(e, "0") != 0) {
    LOG_ERROR("SANDBOX DISABLED FOR DMD!  See bug 956961.");
    // Must treat this as "failure" in order to prevent infinite loop;
    // cf. the PR_GET_SECCOMP check below.
    return 1;
  }
#endif
  if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
    return 1;
  }

  const sock_fprog *filter = GetSandboxFilter();

  if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, (unsigned long)filter, 0, 0)) {
    return 1;
  }
  return 0;
}

// Use signals for permissions that need to be set per-thread.
// The communication channel from the signal handler back to the main thread.
static mozilla::Atomic<int> sSetSandboxDone;
// about:memory has the first 3 RT signals.  (We should allocate
// signals centrally instead of hard-coding them like this.)
static const int sSetSandboxSignum = SIGRTMIN + 3;

static bool
SetThreadSandbox()
{
  bool didAnything = false;

  if (PR_GetEnv("MOZ_DISABLE_CONTENT_SANDBOX") == nullptr &&
      prctl(PR_GET_SECCOMP, 0, 0, 0, 0) == 0) {
    if (InstallSyscallFilter() == 0) {
      didAnything = true;
    }
    /*
     * Bug 880797: when all B2G devices are required to support
     * seccomp-bpf, this should exit/crash if InstallSyscallFilter
     * returns nonzero (ifdef MOZ_WIDGET_GONK).
     */
  }
  return didAnything;
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
BroadcastSetThreadSandbox()
{
  pid_t pid, tid;
  DIR *taskdp;
  struct dirent *de;

  static_assert(sizeof(mozilla::Atomic<int>) == sizeof(int),
                "mozilla::Atomic<int> isn't represented by an int");
  MOZ_ASSERT(NS_IsMainThread());
  pid = getpid();
  taskdp = opendir("/proc/self/task");
  if (taskdp == nullptr) {
    LOG_ERROR("opendir /proc/self/task: %s\n", strerror(errno));
    MOZ_CRASH();
  }
  if (signal(sSetSandboxSignum, SetThreadSandboxHandler) != SIG_DFL) {
    LOG_ERROR("signal %d in use!\n", sSetSandboxSignum);
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
      if (tid == pid) {
        // Drop the main thread's privileges last, below, so
        // we can continue to signal other threads.
        continue;
      }
      // Reset the futex cell and signal.
      sSetSandboxDone = 0;
      if (syscall(__NR_tgkill, pid, tid, sSetSandboxSignum) != 0) {
        if (errno == ESRCH) {
          LOG_ERROR("Thread %d unexpectedly exited.", tid);
          // Rescan threads, in case it forked before exiting.
          sandboxProgress = true;
          continue;
        }
        LOG_ERROR("tgkill(%d,%d): %s\n", pid, tid, strerror(errno));
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
            LOG_ERROR("FUTEX_WAIT: %s\n", strerror(errno));
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
            LOG_ERROR("Thread %d unexpectedly exited.", tid);
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
          LOG_ERROR("Thread %d unresponsive for %d seconds.  Killing process.",
                    tid, crashDelay);
          MOZ_CRASH();
        }
      }
    }
    rewinddir(taskdp);
  } while (sandboxProgress);
  unused << signal(sSetSandboxSignum, SIG_DFL);
  unused << closedir(taskdp);
  // And now, deprivilege the main thread:
  SetThreadSandbox();
}

/**
 * Starts the seccomp sandbox for this process and sets user/group-based privileges.
 * Should be called only once, and before any potentially harmful content is loaded.
 *
 * Should normally make the process exit on failure.
*/
void
SetCurrentProcessSandbox()
{
#if !defined(ANDROID) && defined(PR_LOGGING)
  if (!gSeccompSandboxLog) {
    gSeccompSandboxLog = PR_NewLogModule("SeccompSandbox");
  }
  PR_ASSERT(gSeccompSandboxLog);
#endif

#if defined(MOZ_CONTENT_SANDBOX_REPORTER)
  if (InstallSyscallReporter()) {
    LOG_ERROR("install_syscall_reporter() failed\n");
  }
#endif

  BroadcastSetThreadSandbox();
}

} // namespace mozilla

