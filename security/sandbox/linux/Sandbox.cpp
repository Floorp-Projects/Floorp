/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Sandbox.h"

#include "LinuxSched.h"
#include "SandboxBrokerClient.h"
#include "SandboxChrootProto.h"
#include "SandboxFilter.h"
#include "SandboxInternal.h"
#include "SandboxLogging.h"
#ifdef MOZ_GMP_SANDBOX
#include "SandboxOpenedFiles.h"
#endif
#include "SandboxReporterClient.h"

#include <dirent.h>
#ifdef NIGHTLY_BUILD
#include "dlfcn.h"
#endif
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

#include "mozilla/Array.h"
#include "mozilla/Atomics.h"
#include "mozilla/Range.h"
#include "mozilla/SandboxInfo.h"
#include "mozilla/Span.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "prenv.h"
#include "base/posix/eintr_wrapper.h"
#include "sandbox/linux/bpf_dsl/codegen.h"
#include "sandbox/linux/bpf_dsl/dump_bpf.h"
#include "sandbox/linux/bpf_dsl/policy.h"
#include "sandbox/linux/bpf_dsl/policy_compiler.h"
#include "sandbox/linux/bpf_dsl/seccomp_macros.h"
#include "sandbox/linux/seccomp-bpf/trap.h"
#include "sandbox/linux/system_headers/linux_filter.h"
#include "sandbox/linux/system_headers/linux_seccomp.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"
#if defined(ANDROID)
#include "sandbox/linux/system_headers/linux_ucontext.h"
#endif

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

// Signal number used to enable seccomp on each thread.
mozilla::Atomic<int> gSeccompTsyncBroadcastSignum(0);

namespace mozilla {

static bool gSandboxCrashOnError = false;

// This is initialized by SandboxSetCrashFunc().
SandboxCrashFunc gSandboxCrashFunc;

static SandboxReporterClient* gSandboxReporterClient;
static void (*gChromiumSigSysHandler)(int, siginfo_t*, void*);

// Test whether a ucontext, interpreted as the state after a syscall,
// indicates the given error.  See also sandbox::Syscall::PutValueInUcontext.
static bool
ContextIsError(const ucontext_t *aContext, int aError)
{
  // Avoid integer promotion warnings.  (The unary addition makes
  // the decltype not evaluate to a reference type.)
  typedef decltype(+SECCOMP_RESULT(aContext)) reg_t;

#ifdef __mips__
  return SECCOMP_PARM4(aContext) != 0
    && SECCOMP_RESULT(aContext) == static_cast<reg_t>(aError);
#else
  return SECCOMP_RESULT(aContext) == static_cast<reg_t>(-aError);
#endif
}

/**
 * This is the SIGSYS handler function.  It delegates to the Chromium
 * TrapRegistry handler (see InstallSigSysHandler, below) and, if the
 * trap handler installed by the policy would fail with ENOSYS,
 * crashes the process.  This allows unintentional policy failures to
 * be reported as crash dumps and fixed.  It also logs information
 * about the failed system call.
 *
 * Note that this could be invoked in parallel on multiple threads and
 * that it could be in async signal context (e.g., intercepting an
 * open() called from an async signal handler).
 */
static void
SigSysHandler(int nr, siginfo_t *info, void *void_context)
{
  ucontext_t *ctx = static_cast<ucontext_t*>(void_context);
  // This shouldn't ever be null, but the Chromium handler checks for
  // that and refrains from crashing, so let's not crash release builds:
  MOZ_DIAGNOSTIC_ASSERT(ctx);
  if (!ctx) {
    return;
  }

  // Save a copy of the context before invoking the trap handler,
  // which will overwrite one or more registers with the return value.
  ucontext_t savedCtx = *ctx;

  gChromiumSigSysHandler(nr, info, ctx);
  if (!ContextIsError(ctx, ENOSYS)) {
    return;
  }

  SandboxReport report = gSandboxReporterClient->MakeReportAndSend(&savedCtx);

  // TODO, someday when this is enabled on MIPS: include the two extra
  // args in the error message.
  SANDBOX_LOG_ERROR("seccomp sandbox violation: pid %d, tid %d, syscall %d,"
                    " args %d %d %d %d %d %d.%s",
                    report.mPid, report.mTid, report.mSyscall,
                    report.mArgs[0], report.mArgs[1], report.mArgs[2],
                    report.mArgs[3], report.mArgs[4], report.mArgs[5],
                    gSandboxCrashOnError ? "  Killing process." : "");

  if (gSandboxCrashOnError) {
    // Bug 1017393: record syscall number somewhere useful.
    info->si_addr = reinterpret_cast<void*>(report.mSyscall);

    gSandboxCrashFunc(nr, info, &savedCtx);
    _exit(127);
  }
}

/**
 * This function installs the SIGSYS handler.  This is slightly
 * complicated because we want to use Chromium's handler to dispatch
 * to specific trap handlers defined in the policy, but we also need
 * the full original signal context to give to Breakpad for crash
 * dumps.  So we install Chromium's handler first, then retrieve its
 * address so our replacement can delegate to it.
 */
static void
InstallSigSysHandler(void)
{
  struct sigaction act;

  // Ensure that the Chromium handler is installed.
  Unused << sandbox::Trap::Registry();

  // If the signal handling state isn't as expected, crash now instead
  // of crashing later (and more confusingly) when SIGSYS happens.

  if (sigaction(SIGSYS, nullptr, &act) != 0) {
    MOZ_CRASH("Couldn't read old SIGSYS disposition");
  }
  if ((act.sa_flags & SA_SIGINFO) != SA_SIGINFO) {
    MOZ_CRASH("SIGSYS not already set to a siginfo handler?");
  }
  MOZ_RELEASE_ASSERT(act.sa_sigaction);
  gChromiumSigSysHandler = act.sa_sigaction;
  act.sa_sigaction = SigSysHandler;
  // Currently, SA_NODEFER should already be set by the Chromium code,
  // but it's harmless to ensure that it's set:
  MOZ_ASSERT(act.sa_flags & SA_NODEFER);
  act.sa_flags |= SA_NODEFER;
  if (sigaction(SIGSYS, &act, nullptr) < 0) {
    MOZ_CRASH("Couldn't change SIGSYS disposition");
  }
}

/**
 * This function installs the syscall filter, a.k.a. seccomp.  The
 * aUseTSync flag indicates whether this should apply to all threads
 * in the process -- which will fail if the kernel doesn't support
 * that -- or only the current thread.
 *
 * SECCOMP_MODE_FILTER is the "bpf" mode of seccomp which allows
 * to pass a bpf program (in our case, it contains a syscall
 * whitelist).
 *
 * PR_SET_NO_NEW_PRIVS ensures that it is impossible to grant more
 * syscalls to the process beyond this point (even after fork()), and
 * prevents gaining capabilities (e.g., by exec'ing a setuid root
 * program).  The kernel won't allow seccomp-bpf without doing this,
 * because otherwise it could be used for privilege escalation attacks.
 *
 * Returns false if the filter was already installed (see the
 * PR_SET_NO_NEW_PRIVS rule in SandboxFilter.cpp).  Crashes on any
 * other error condition.
 *
 * @see SandboxInfo
 * @see BroadcastSetThreadSandbox
 */
static bool MOZ_MUST_USE
InstallSyscallFilter(const sock_fprog *aProg, bool aUseTSync)
{
  if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
    if (!aUseTSync && errno == ETXTBSY) {
      return false;
    }
    SANDBOX_LOG_ERROR("prctl(PR_SET_NO_NEW_PRIVS) failed: %s", strerror(errno));
    MOZ_CRASH("prctl(PR_SET_NO_NEW_PRIVS)");
  }

  if (aUseTSync) {
    if (syscall(__NR_seccomp, SECCOMP_SET_MODE_FILTER,
                SECCOMP_FILTER_FLAG_TSYNC, aProg) != 0) {
      SANDBOX_LOG_ERROR("thread-synchronized seccomp failed: %s",
                        strerror(errno));
      MOZ_CRASH("prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER)");
    }
  } else {
    if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, (unsigned long)aProg, 0, 0)) {
      SANDBOX_LOG_ERROR("prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER) failed: %s",
                        strerror(errno));
      MOZ_CRASH("seccomp+tsync failed, but kernel supports tsync");
    }
  }
  return true;
}

// Use signals for permissions that need to be set per-thread.
// The communication channel from the signal handler back to the main thread.
static mozilla::Atomic<int> gSetSandboxDone;
// Pass the filter itself through a global.
const sock_fprog* gSetSandboxFilter;

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
  for (int signum = SIGRTMAX; signum >= SIGRTMIN; --signum) {
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
  return InstallSyscallFilter(gSetSandboxFilter, false);
}

static void
SetThreadSandboxHandler(int signum)
{
  // The non-zero number sent back to the main thread indicates
  // whether action was taken.
  if (SetThreadSandbox()) {
    gSetSandboxDone = 2;
  } else {
    gSetSandboxDone = 1;
  }
  // Wake up the main thread.  See the FUTEX_WAIT call, below, for an
  // explanation.
  syscall(__NR_futex, reinterpret_cast<int*>(&gSetSandboxDone),
          FUTEX_WAKE, 1);
}

static void
EnterChroot()
{
  if (!PR_GetEnv(kSandboxChrootEnvFlag)) {
    return;
  }
  char msg = kSandboxChrootRequest;
  ssize_t msg_len = HANDLE_EINTR(write(kSandboxChrootClientFd, &msg, 1));
  MOZ_RELEASE_ASSERT(msg_len == 1);
  msg_len = HANDLE_EINTR(read(kSandboxChrootClientFd, &msg, 1));
  MOZ_RELEASE_ASSERT(msg_len == 1);
  MOZ_RELEASE_ASSERT(msg == kSandboxChrootResponse);
  close(kSandboxChrootClientFd);
}

static void
BroadcastSetThreadSandbox(const sock_fprog* aFilter)
{
  pid_t pid, tid, myTid;
  DIR *taskdp;
  struct dirent *de;

  // This function does not own *aFilter, so this global needs to
  // always be zeroed before returning.
  gSetSandboxFilter = aFilter;

  static_assert(sizeof(mozilla::Atomic<int>) == sizeof(int),
                "mozilla::Atomic<int> isn't represented by an int");
  pid = getpid();
  myTid = syscall(__NR_gettid);
  taskdp = opendir("/proc/self/task");
  if (taskdp == nullptr) {
    SANDBOX_LOG_ERROR("opendir /proc/self/task: %s\n", strerror(errno));
    MOZ_CRASH();
  }

  // In case this races with a not-yet-deprivileged thread cloning
  // itself, repeat iterating over all threads until we find none
  // that are still privileged.
  bool sandboxProgress;
  const int tsyncSignum = gSeccompTsyncBroadcastSignum;
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

      MOZ_RELEASE_ASSERT(tsyncSignum != 0);

      // Reset the futex cell and signal.
      gSetSandboxDone = 0;
      if (syscall(__NR_tgkill, pid, tid, tsyncSignum) != 0) {
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
        // Atomically: if gSetSandboxDone == 0, then sleep.
        if (syscall(__NR_futex, reinterpret_cast<int*>(&gSetSandboxDone),
                  FUTEX_WAIT, 0, &futexTimeout) != 0) {
          if (errno != EWOULDBLOCK && errno != ETIMEDOUT && errno != EINTR) {
            SANDBOX_LOG_ERROR("FUTEX_WAIT: %s\n", strerror(errno));
            MOZ_CRASH();
          }
        }
        // Did the handler finish?
        if (gSetSandboxDone > 0) {
          if (gSetSandboxDone == 2) {
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
        if (now.tv_sec > timeLimit.tv_sec ||
            (now.tv_sec == timeLimit.tv_sec &&
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

  void (*oldHandler)(int);
  oldHandler = signal(tsyncSignum, SIG_DFL);
  if (oldHandler != SetThreadSandboxHandler) {
    // See the comment on FindFreeSignalNumber about race conditions.
    SANDBOX_LOG_ERROR("handler for signal %d was changed to %p!",
                      tsyncSignum, oldHandler);
    MOZ_CRASH();
  }
  gSeccompTsyncBroadcastSignum = 0;
  Unused << closedir(taskdp);
  // And now, deprivilege the main thread:
  SetThreadSandbox();
  gSetSandboxFilter = nullptr;
}

static void
ApplySandboxWithTSync(sock_fprog* aFilter)
{
  // At this point we're committed to using tsync, because we'd have
  // needed to allocate a signal and prevent it from being blocked on
  // other threads (see SandboxHooks.cpp), so there's no attempt to
  // fall back to the non-tsync path.
  if (!InstallSyscallFilter(aFilter, true)) {
    MOZ_CRASH();
  }
}

#ifdef NIGHTLY_BUILD
static bool
IsLibPresent(const char* aName)
{
  if (const auto handle = dlopen(aName, RTLD_LAZY | RTLD_NOLOAD)) {
    dlclose(handle);
    return true;
  }
  return false;
}

static const Array<const char*, 1> kLibsThatWillCrash {
  "libesets_pac.so",
};
#endif // NIGHTLY_BUILD

void
SandboxEarlyInit() {
  if (PR_GetEnv("MOZ_SANDBOXED") == nullptr) {
    return;
  }

  // Fix LD_PRELOAD for any child processes.  See bug 1434392 comment #10;
  // this can probably go away when audio remoting is mandatory.
  const char* oldPreload = PR_GetEnv("MOZ_ORIG_LD_PRELOAD");
  char* preloadEntry;
  // This string is "leaked" because the environment takes ownership.
  if (asprintf(&preloadEntry, "LD_PRELOAD=%s",
               oldPreload ? oldPreload : "") != -1) {
    PR_SetEnv(preloadEntry);
  }

  // If TSYNC is not supported, set up signal handler
  // used to enable seccomp on each thread.
  if (!SandboxInfo::Get().Test(SandboxInfo::kHasSeccompTSync)) {
    // The signal number has to be chosen early, so that the
    // interceptions in SandboxHooks.cpp can prevent it from being
    // masked.
    const int tsyncSignum = FindFreeSignalNumber();
    if (tsyncSignum == 0) {
      SANDBOX_LOG_ERROR("No available signal numbers!");
      MOZ_CRASH();
    }
    gSeccompTsyncBroadcastSignum = tsyncSignum;

    // ...and the signal handler also needs to be installed now, to
    // indicate to anything else looking for free signals that it's
    // claimed.
    void (*oldHandler)(int);
    oldHandler = signal(tsyncSignum, SetThreadSandboxHandler);
    if (oldHandler != SIG_DFL) {
      // See the comment on FindFreeSignalNumber about race conditions.
      SANDBOX_LOG_ERROR("signal %d in use by handler %p!\n",
                        tsyncSignum, oldHandler);
      MOZ_CRASH();
    }
  }
}

static void
SandboxLateInit() {
#ifdef NIGHTLY_BUILD
  gSandboxCrashOnError = true;
  for (const char* name : kLibsThatWillCrash) {
    if (IsLibPresent(name)) {
      gSandboxCrashOnError = false;
      break;
    }
  }
#endif

  if (const char* envVar = PR_GetEnv("MOZ_SANDBOX_CRASH_ON_ERROR")) {
    if (envVar[0]) {
      gSandboxCrashOnError = envVar[0] != '0';
    }
  }
}

// Common code for sandbox startup.
static void
SetCurrentProcessSandbox(UniquePtr<sandbox::bpf_dsl::Policy> aPolicy)
{
  MOZ_ASSERT(gSandboxCrashFunc);
  MOZ_RELEASE_ASSERT(gSandboxReporterClient != nullptr);
  SandboxLateInit();

  // Auto-collect child processes -- mainly the chroot helper if
  // present, but also anything setns()ed into the pid namespace (not
  // yet implemented).  This process won't be able to waitpid them
  // after the seccomp-bpf policy is applied.
  signal(SIGCHLD, SIG_IGN);

  // Note: PolicyCompiler borrows the policy and registry for its
  // lifetime, but does not take ownership of them.
  sandbox::bpf_dsl::PolicyCompiler compiler(aPolicy.get(),
                                            sandbox::Trap::Registry());
  sandbox::CodeGen::Program program = compiler.Compile();
  if (SandboxInfo::Get().Test(SandboxInfo::kVerbose)) {
    sandbox::bpf_dsl::DumpBPF::PrintProgram(program);
  }

  InstallSigSysHandler();

#ifdef MOZ_ASAN
  __sanitizer_sandbox_arguments asanArgs;
  asanArgs.coverage_sandboxed = 1;
  asanArgs.coverage_fd = -1;
  asanArgs.coverage_max_block_size = 0;
  __sanitizer_sandbox_on_notify(&asanArgs);
#endif

  // The syscall takes a C-style array, so copy the vector into one.
  size_t programLen = program.size();
  UniquePtr<sock_filter[]> flatProgram(new sock_filter[programLen]);
  for (auto i = program.begin(); i != program.end(); ++i) {
    flatProgram[i - program.begin()] = *i;
  }

  sock_fprog fprog;
  fprog.filter = flatProgram.get();
  fprog.len = static_cast<unsigned short>(programLen);
  MOZ_RELEASE_ASSERT(static_cast<size_t>(fprog.len) == programLen);

  const SandboxInfo info = SandboxInfo::Get();
  if (info.Test(SandboxInfo::kHasSeccompTSync)) {
    if (info.Test(SandboxInfo::kVerbose)) {
      SANDBOX_LOG_ERROR("using seccomp tsync");
    }
    ApplySandboxWithTSync(&fprog);
  } else {
    if (info.Test(SandboxInfo::kVerbose)) {
      SANDBOX_LOG_ERROR("no tsync support; using signal broadcast");
    }
    BroadcastSetThreadSandbox(&fprog);
  }

  // Now that all threads' filesystem accesses are being intercepted
  // (if a broker is used) it's safe to chroot the process:
  EnterChroot();
}

#ifdef MOZ_CONTENT_SANDBOX
/**
 * Starts the seccomp sandbox for a content process.  Should be called
 * only once, and before any potentially harmful content is loaded.
 *
 * Will normally make the process exit on failure.
*/
bool
SetContentProcessSandbox(ContentProcessSandboxParams&& aParams)
{
  int brokerFd = aParams.mBrokerFd;
  aParams.mBrokerFd = -1;

  if (!SandboxInfo::Get().Test(SandboxInfo::kEnabledForContent)) {
    if (brokerFd >= 0) {
      close(brokerFd);
    }
    return false;
  }

  auto procType = aParams.mFileProcess
    ? SandboxReport::ProcType::FILE
    : SandboxReport::ProcType::CONTENT;
  gSandboxReporterClient = new SandboxReporterClient(procType);

  // This needs to live until the process exits.
  static SandboxBrokerClient* sBroker;
  if (brokerFd >= 0) {
    sBroker = new SandboxBrokerClient(brokerFd);
  }

  SetCurrentProcessSandbox(GetContentSandboxPolicy(sBroker, std::move(aParams)));
  return true;
}
#endif // MOZ_CONTENT_SANDBOX

#ifdef MOZ_GMP_SANDBOX
/**
 * Starts the seccomp sandbox for a media plugin process.  Should be
 * called only once, and before any potentially harmful content is
 * loaded -- including the plugin itself, if it's considered untrusted.
 *
 * The file indicated by aFilePath, if non-null, can be open()ed
 * read-only, once, after the sandbox starts; it should be the .so
 * file implementing the not-yet-loaded plugin.
 *
 * Will normally make the process exit on failure.
*/
void
SetMediaPluginSandbox(const char *aFilePath)
{
  MOZ_RELEASE_ASSERT(aFilePath != nullptr);
  if (!SandboxInfo::Get().Test(SandboxInfo::kEnabledForMedia)) {
    return;
  }

  gSandboxReporterClient =
    new SandboxReporterClient(SandboxReport::ProcType::MEDIA_PLUGIN);

  SandboxOpenedFile plugin(aFilePath);
  if (!plugin.IsOpen()) {
    SANDBOX_LOG_ERROR("failed to open plugin file %s: %s",
                      aFilePath, strerror(errno));
    MOZ_CRASH();
  }

  auto files = new SandboxOpenedFiles();
  files->Add(std::move(plugin));
  files->Add("/dev/urandom", true);
  files->Add("/sys/devices/system/cpu/cpu0/tsc_freq_khz");
  files->Add("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq");
  files->Add("/proc/cpuinfo"); // Info also available via CPUID instruction.
#ifdef __i386__
  files->Add("/proc/self/auxv"); // Info also in process's address space.
#endif

  // Finally, start the sandbox.
  SetCurrentProcessSandbox(GetMediaSandboxPolicy(files));
}
#endif // MOZ_GMP_SANDBOX

} // namespace mozilla
