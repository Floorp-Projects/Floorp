// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"

// Some headers on Android are missing cdefs: crbug.com/172337.
// (We can't use OS_ANDROID here since build_config.h is not included).
#if defined(ANDROID)
#include <sys/cdefs.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <linux/filter.h>
#include <signal.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/posix/eintr_wrapper.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/bpf_dsl/dump_bpf.h"
#include "sandbox/linux/bpf_dsl/policy.h"
#include "sandbox/linux/bpf_dsl/policy_compiler.h"
#include "sandbox/linux/seccomp-bpf/codegen.h"
#include "sandbox/linux/seccomp-bpf/die.h"
#include "sandbox/linux/seccomp-bpf/errorcode.h"
#include "sandbox/linux/seccomp-bpf/linux_seccomp.h"
#include "sandbox/linux/seccomp-bpf/syscall.h"
#include "sandbox/linux/seccomp-bpf/syscall_iterator.h"
#include "sandbox/linux/seccomp-bpf/trap.h"
#include "sandbox/linux/seccomp-bpf/verifier.h"
#include "sandbox/linux/services/linux_syscalls.h"

using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::Error;
using sandbox::bpf_dsl::ResultExpr;

namespace sandbox {

namespace {

const int kExpectedExitCode = 100;

#if !defined(NDEBUG)
void WriteFailedStderrSetupMessage(int out_fd) {
  const char* error_string = strerror(errno);
  static const char msg[] =
      "You have reproduced a puzzling issue.\n"
      "Please, report to crbug.com/152530!\n"
      "Failed to set up stderr: ";
  if (HANDLE_EINTR(write(out_fd, msg, sizeof(msg) - 1)) > 0 && error_string &&
      HANDLE_EINTR(write(out_fd, error_string, strlen(error_string))) > 0 &&
      HANDLE_EINTR(write(out_fd, "\n", 1))) {
  }
}
#endif  // !defined(NDEBUG)

// We define a really simple sandbox policy. It is just good enough for us
// to tell that the sandbox has actually been activated.
class ProbePolicy : public bpf_dsl::Policy {
 public:
  ProbePolicy() {}
  virtual ~ProbePolicy() {}

  virtual ResultExpr EvaluateSyscall(int sysnum) const override {
    switch (sysnum) {
      case __NR_getpid:
        // Return EPERM so that we can check that the filter actually ran.
        return Error(EPERM);
      case __NR_exit_group:
        // Allow exit() with a non-default return code.
        return Allow();
      default:
        // Make everything else fail in an easily recognizable way.
        return Error(EINVAL);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ProbePolicy);
};

void ProbeProcess(void) {
  if (syscall(__NR_getpid) < 0 && errno == EPERM) {
    syscall(__NR_exit_group, static_cast<intptr_t>(kExpectedExitCode));
  }
}

class AllowAllPolicy : public bpf_dsl::Policy {
 public:
  AllowAllPolicy() {}
  virtual ~AllowAllPolicy() {}

  virtual ResultExpr EvaluateSyscall(int sysnum) const override {
    DCHECK(SandboxBPF::IsValidSyscallNumber(sysnum));
    return Allow();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AllowAllPolicy);
};

void TryVsyscallProcess(void) {
  time_t current_time;
  // time() is implemented as a vsyscall. With an older glibc, with
  // vsyscall=emulate and some versions of the seccomp BPF patch
  // we may get SIGKILL-ed. Detect this!
  if (time(&current_time) != static_cast<time_t>(-1)) {
    syscall(__NR_exit_group, static_cast<intptr_t>(kExpectedExitCode));
  }
}

bool IsSingleThreaded(int proc_fd) {
  if (proc_fd < 0) {
    // Cannot determine whether program is single-threaded. Hope for
    // the best...
    return true;
  }

  struct stat sb;
  int task = -1;
  if ((task = openat(proc_fd, "self/task", O_RDONLY | O_DIRECTORY)) < 0 ||
      fstat(task, &sb) != 0 || sb.st_nlink != 3 || IGNORE_EINTR(close(task))) {
    if (task >= 0) {
      if (IGNORE_EINTR(close(task))) {
      }
    }
    return false;
  }
  return true;
}

}  // namespace

SandboxBPF::SandboxBPF()
    : quiet_(false), proc_fd_(-1), sandbox_has_started_(false), policy_() {
}

SandboxBPF::~SandboxBPF() {
}

bool SandboxBPF::IsValidSyscallNumber(int sysnum) {
  return SyscallSet::IsValid(sysnum);
}

bool SandboxBPF::RunFunctionInPolicy(void (*code_in_sandbox)(),
                                     scoped_ptr<bpf_dsl::Policy> policy) {
  // Block all signals before forking a child process. This prevents an
  // attacker from manipulating our test by sending us an unexpected signal.
  sigset_t old_mask, new_mask;
  if (sigfillset(&new_mask) || sigprocmask(SIG_BLOCK, &new_mask, &old_mask)) {
    SANDBOX_DIE("sigprocmask() failed");
  }
  int fds[2];
  if (pipe2(fds, O_NONBLOCK | O_CLOEXEC)) {
    SANDBOX_DIE("pipe() failed");
  }

  if (fds[0] <= 2 || fds[1] <= 2) {
    SANDBOX_DIE("Process started without standard file descriptors");
  }

  // This code is using fork() and should only ever run single-threaded.
  // Most of the code below is "async-signal-safe" and only minor changes
  // would be needed to support threads.
  DCHECK(IsSingleThreaded(proc_fd_));
  pid_t pid = fork();
  if (pid < 0) {
    // Die if we cannot fork(). We would probably fail a little later
    // anyway, as the machine is likely very close to running out of
    // memory.
    // But what we don't want to do is return "false", as a crafty
    // attacker might cause fork() to fail at will and could trick us
    // into running without a sandbox.
    sigprocmask(SIG_SETMASK, &old_mask, NULL);  // OK, if it fails
    SANDBOX_DIE("fork() failed unexpectedly");
  }

  // In the child process
  if (!pid) {
    // Test a very simple sandbox policy to verify that we can
    // successfully turn on sandboxing.
    Die::EnableSimpleExit();

    errno = 0;
    if (IGNORE_EINTR(close(fds[0]))) {
      // This call to close() has been failing in strange ways. See
      // crbug.com/152530. So we only fail in debug mode now.
#if !defined(NDEBUG)
      WriteFailedStderrSetupMessage(fds[1]);
      SANDBOX_DIE(NULL);
#endif
    }
    if (HANDLE_EINTR(dup2(fds[1], 2)) != 2) {
      // Stderr could very well be a file descriptor to .xsession-errors, or
      // another file, which could be backed by a file system that could cause
      // dup2 to fail while trying to close stderr. It's important that we do
      // not fail on trying to close stderr.
      // If dup2 fails here, we will continue normally, this means that our
      // parent won't cause a fatal failure if something writes to stderr in
      // this child.
#if !defined(NDEBUG)
      // In DEBUG builds, we still want to get a report.
      WriteFailedStderrSetupMessage(fds[1]);
      SANDBOX_DIE(NULL);
#endif
    }
    if (IGNORE_EINTR(close(fds[1]))) {
      // This call to close() has been failing in strange ways. See
      // crbug.com/152530. So we only fail in debug mode now.
#if !defined(NDEBUG)
      WriteFailedStderrSetupMessage(fds[1]);
      SANDBOX_DIE(NULL);
#endif
    }

    SetSandboxPolicy(policy.release());
    if (!StartSandbox(PROCESS_SINGLE_THREADED)) {
      SANDBOX_DIE(NULL);
    }

    // Run our code in the sandbox.
    code_in_sandbox();

    // code_in_sandbox() is not supposed to return here.
    SANDBOX_DIE(NULL);
  }

  // In the parent process.
  if (IGNORE_EINTR(close(fds[1]))) {
    SANDBOX_DIE("close() failed");
  }
  if (sigprocmask(SIG_SETMASK, &old_mask, NULL)) {
    SANDBOX_DIE("sigprocmask() failed");
  }
  int status;
  if (HANDLE_EINTR(waitpid(pid, &status, 0)) != pid) {
    SANDBOX_DIE("waitpid() failed unexpectedly");
  }
  bool rc = WIFEXITED(status) && WEXITSTATUS(status) == kExpectedExitCode;

  // If we fail to support sandboxing, there might be an additional
  // error message. If so, this was an entirely unexpected and fatal
  // failure. We should report the failure and somebody must fix
  // things. This is probably a security-critical bug in the sandboxing
  // code.
  if (!rc) {
    char buf[4096];
    ssize_t len = HANDLE_EINTR(read(fds[0], buf, sizeof(buf) - 1));
    if (len > 0) {
      while (len > 1 && buf[len - 1] == '\n') {
        --len;
      }
      buf[len] = '\000';
      SANDBOX_DIE(buf);
    }
  }
  if (IGNORE_EINTR(close(fds[0]))) {
    SANDBOX_DIE("close() failed");
  }

  return rc;
}

bool SandboxBPF::KernelSupportSeccompBPF() {
  return RunFunctionInPolicy(ProbeProcess,
                             scoped_ptr<bpf_dsl::Policy>(new ProbePolicy())) &&
         RunFunctionInPolicy(TryVsyscallProcess,
                             scoped_ptr<bpf_dsl::Policy>(new AllowAllPolicy()));
}

// static
SandboxBPF::SandboxStatus SandboxBPF::SupportsSeccompSandbox(int proc_fd) {
  // It the sandbox is currently active, we clearly must have support for
  // sandboxing.
  if (status_ == STATUS_ENABLED) {
    return status_;
  }

  // Even if the sandbox was previously available, something might have
  // changed in our run-time environment. Check one more time.
  if (status_ == STATUS_AVAILABLE) {
    if (!IsSingleThreaded(proc_fd)) {
      status_ = STATUS_UNAVAILABLE;
    }
    return status_;
  }

  if (status_ == STATUS_UNAVAILABLE && IsSingleThreaded(proc_fd)) {
    // All state transitions resulting in STATUS_UNAVAILABLE are immediately
    // preceded by STATUS_AVAILABLE. Furthermore, these transitions all
    // happen, if and only if they are triggered by the process being multi-
    // threaded.
    // In other words, if a single-threaded process is currently in the
    // STATUS_UNAVAILABLE state, it is safe to assume that sandboxing is
    // actually available.
    status_ = STATUS_AVAILABLE;
    return status_;
  }

  // If we have not previously checked for availability of the sandbox or if
  // we otherwise don't believe to have a good cached value, we have to
  // perform a thorough check now.
  if (status_ == STATUS_UNKNOWN) {
    // We create our own private copy of a "Sandbox" object. This ensures that
    // the object does not have any policies configured, that might interfere
    // with the tests done by "KernelSupportSeccompBPF()".
    SandboxBPF sandbox;

    // By setting "quiet_ = true" we suppress messages for expected and benign
    // failures (e.g. if the current kernel lacks support for BPF filters).
    sandbox.quiet_ = true;
    sandbox.set_proc_fd(proc_fd);
    status_ = sandbox.KernelSupportSeccompBPF() ? STATUS_AVAILABLE
                                                : STATUS_UNSUPPORTED;

    // As we are performing our tests from a child process, the run-time
    // environment that is visible to the sandbox is always guaranteed to be
    // single-threaded. Let's check here whether the caller is single-
    // threaded. Otherwise, we mark the sandbox as temporarily unavailable.
    if (status_ == STATUS_AVAILABLE && !IsSingleThreaded(proc_fd)) {
      status_ = STATUS_UNAVAILABLE;
    }
  }
  return status_;
}

// static
SandboxBPF::SandboxStatus
SandboxBPF::SupportsSeccompThreadFilterSynchronization() {
  // Applying NO_NEW_PRIVS, a BPF filter, and synchronizing the filter across
  // the thread group are all handled atomically by this syscall.
  const int rv = syscall(
      __NR_seccomp, SECCOMP_SET_MODE_FILTER, SECCOMP_FILTER_FLAG_TSYNC, NULL);

  if (rv == -1 && errno == EFAULT) {
    return STATUS_AVAILABLE;
  } else {
    // TODO(jln): turn these into DCHECK after 417888 is considered fixed.
    CHECK_EQ(-1, rv);
    CHECK(ENOSYS == errno || EINVAL == errno);
    return STATUS_UNSUPPORTED;
  }
}

void SandboxBPF::set_proc_fd(int proc_fd) { proc_fd_ = proc_fd; }

bool SandboxBPF::StartSandbox(SandboxThreadState thread_state) {
  CHECK(thread_state == PROCESS_SINGLE_THREADED ||
        thread_state == PROCESS_MULTI_THREADED);

  if (status_ == STATUS_UNSUPPORTED || status_ == STATUS_UNAVAILABLE) {
    SANDBOX_DIE(
        "Trying to start sandbox, even though it is known to be "
        "unavailable");
    return false;
  } else if (sandbox_has_started_) {
    SANDBOX_DIE(
        "Cannot repeatedly start sandbox. Create a separate Sandbox "
        "object instead.");
    return false;
  }
  if (proc_fd_ < 0) {
    proc_fd_ = open("/proc", O_RDONLY | O_DIRECTORY);
  }
  if (proc_fd_ < 0) {
    // For now, continue in degraded mode, if we can't access /proc.
    // In the future, we might want to tighten this requirement.
  }

  bool supports_tsync =
      SupportsSeccompThreadFilterSynchronization() == STATUS_AVAILABLE;

  if (thread_state == PROCESS_SINGLE_THREADED) {
    if (!IsSingleThreaded(proc_fd_)) {
      SANDBOX_DIE("Cannot start sandbox; process is already multi-threaded");
      return false;
    }
  } else if (thread_state == PROCESS_MULTI_THREADED) {
    if (IsSingleThreaded(proc_fd_)) {
      SANDBOX_DIE("Cannot start sandbox; "
                  "process may be single-threaded when reported as not");
      return false;
    }
    if (!supports_tsync) {
      SANDBOX_DIE("Cannot start sandbox; kernel does not support synchronizing "
                  "filters for a threadgroup");
      return false;
    }
  }

  // We no longer need access to any files in /proc. We want to do this
  // before installing the filters, just in case that our policy denies
  // close().
  if (proc_fd_ >= 0) {
    if (IGNORE_EINTR(close(proc_fd_))) {
      SANDBOX_DIE("Failed to close file descriptor for /proc");
      return false;
    }
    proc_fd_ = -1;
  }

  // Install the filters.
  InstallFilter(supports_tsync || thread_state == PROCESS_MULTI_THREADED);

  // We are now inside the sandbox.
  status_ = STATUS_ENABLED;

  return true;
}

// Don't take a scoped_ptr here, polymorphism make their use awkward.
void SandboxBPF::SetSandboxPolicy(bpf_dsl::Policy* policy) {
  DCHECK(!policy_);
  if (sandbox_has_started_) {
    SANDBOX_DIE("Cannot change policy after sandbox has started");
  }
  policy_.reset(policy);
}

void SandboxBPF::InstallFilter(bool must_sync_threads) {
  // We want to be very careful in not imposing any requirements on the
  // policies that are set with SetSandboxPolicy(). This means, as soon as
  // the sandbox is active, we shouldn't be relying on libraries that could
  // be making system calls. This, for example, means we should avoid
  // using the heap and we should avoid using STL functions.
  // Temporarily copy the contents of the "program" vector into a
  // stack-allocated array; and then explicitly destroy that object.
  // This makes sure we don't ex- or implicitly call new/delete after we
  // installed the BPF filter program in the kernel. Depending on the
  // system memory allocator that is in effect, these operators can result
  // in system calls to things like munmap() or brk().
  CodeGen::Program* program = AssembleFilter(false).release();

  struct sock_filter bpf[program->size()];
  const struct sock_fprog prog = {static_cast<unsigned short>(program->size()),
                                  bpf};
  memcpy(bpf, &(*program)[0], sizeof(bpf));
  delete program;

  // Make an attempt to release memory that is no longer needed here, rather
  // than in the destructor. Try to avoid as much as possible to presume of
  // what will be possible to do in the new (sandboxed) execution environment.
  policy_.reset();

  if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
    SANDBOX_DIE(quiet_ ? NULL : "Kernel refuses to enable no-new-privs");
  }

  // Install BPF filter program. If the thread state indicates multi-threading
  // support, then the kernel hass the seccomp system call. Otherwise, fall
  // back on prctl, which requires the process to be single-threaded.
  if (must_sync_threads) {
    int rv = syscall(__NR_seccomp, SECCOMP_SET_MODE_FILTER,
        SECCOMP_FILTER_FLAG_TSYNC, reinterpret_cast<const char*>(&prog));
    if (rv) {
      SANDBOX_DIE(quiet_ ? NULL :
          "Kernel refuses to turn on and synchronize threads for BPF filters");
    }
  } else {
    if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog)) {
      SANDBOX_DIE(quiet_ ? NULL : "Kernel refuses to turn on BPF filters");
    }
  }

  sandbox_has_started_ = true;
}

scoped_ptr<CodeGen::Program> SandboxBPF::AssembleFilter(
    bool force_verification) {
#if !defined(NDEBUG)
  force_verification = true;
#endif

  bpf_dsl::PolicyCompiler compiler(policy_.get(), Trap::Registry());
  scoped_ptr<CodeGen::Program> program = compiler.Compile();

  // Make sure compilation resulted in BPF program that executes
  // correctly. Otherwise, there is an internal error in our BPF compiler.
  // There is really nothing the caller can do until the bug is fixed.
  if (force_verification) {
    // Verification is expensive. We only perform this step, if we are
    // compiled in debug mode, or if the caller explicitly requested
    // verification.

    const char* err = NULL;
    if (!Verifier::VerifyBPF(&compiler, *program, *policy_, &err)) {
      bpf_dsl::DumpBPF::PrintProgram(*program);
      SANDBOX_DIE(err);
    }
  }

  return program.Pass();
}

bool SandboxBPF::IsRequiredForUnsafeTrap(int sysno) {
  return bpf_dsl::PolicyCompiler::IsRequiredForUnsafeTrap(sysno);
}

intptr_t SandboxBPF::ForwardSyscall(const struct arch_seccomp_data& args) {
  return Syscall::Call(args.nr,
                       static_cast<intptr_t>(args.args[0]),
                       static_cast<intptr_t>(args.args[1]),
                       static_cast<intptr_t>(args.args[2]),
                       static_cast<intptr_t>(args.args[3]),
                       static_cast<intptr_t>(args.args[4]),
                       static_cast<intptr_t>(args.args[5]));
}

SandboxBPF::SandboxStatus SandboxBPF::status_ = STATUS_UNKNOWN;

}  // namespace sandbox
