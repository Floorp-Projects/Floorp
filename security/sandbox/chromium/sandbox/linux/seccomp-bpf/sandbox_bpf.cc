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
#include <string.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/posix/eintr_wrapper.h"
#include "sandbox/linux/seccomp-bpf/codegen.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf_policy.h"
#include "sandbox/linux/seccomp-bpf/syscall.h"
#include "sandbox/linux/seccomp-bpf/syscall_iterator.h"
#include "sandbox/linux/seccomp-bpf/verifier.h"

namespace sandbox {

namespace {

const int kExpectedExitCode = 100;

int popcount(uint32_t x) {
  return __builtin_popcount(x);
}

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
class ProbePolicy : public SandboxBPFPolicy {
 public:
  ProbePolicy() {}
  virtual ErrorCode EvaluateSyscall(SandboxBPF*, int sysnum) const OVERRIDE {
    switch (sysnum) {
      case __NR_getpid:
        // Return EPERM so that we can check that the filter actually ran.
        return ErrorCode(EPERM);
      case __NR_exit_group:
        // Allow exit() with a non-default return code.
        return ErrorCode(ErrorCode::ERR_ALLOWED);
      default:
        // Make everything else fail in an easily recognizable way.
        return ErrorCode(EINVAL);
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

class AllowAllPolicy : public SandboxBPFPolicy {
 public:
  AllowAllPolicy() {}
  virtual ErrorCode EvaluateSyscall(SandboxBPF*, int sysnum) const OVERRIDE {
    DCHECK(SandboxBPF::IsValidSyscallNumber(sysnum));
    return ErrorCode(ErrorCode::ERR_ALLOWED);
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

bool IsDenied(const ErrorCode& code) {
  return (code.err() & SECCOMP_RET_ACTION) == SECCOMP_RET_TRAP ||
         (code.err() >= (SECCOMP_RET_ERRNO + ErrorCode::ERR_MIN_ERRNO) &&
          code.err() <= (SECCOMP_RET_ERRNO + ErrorCode::ERR_MAX_ERRNO));
}

// Function that can be passed as a callback function to CodeGen::Traverse().
// Checks whether the "insn" returns an UnsafeTrap() ErrorCode. If so, it
// sets the "bool" variable pointed to by "aux".
void CheckForUnsafeErrorCodes(Instruction* insn, void* aux) {
  bool* is_unsafe = static_cast<bool*>(aux);
  if (!*is_unsafe) {
    if (BPF_CLASS(insn->code) == BPF_RET && insn->k > SECCOMP_RET_TRAP &&
        insn->k - SECCOMP_RET_TRAP <= SECCOMP_RET_DATA) {
      const ErrorCode& err =
          Trap::ErrorCodeFromTrapId(insn->k & SECCOMP_RET_DATA);
      if (err.error_type() != ErrorCode::ET_INVALID && !err.safe()) {
        *is_unsafe = true;
      }
    }
  }
}

// A Trap() handler that returns an "errno" value. The value is encoded
// in the "aux" parameter.
intptr_t ReturnErrno(const struct arch_seccomp_data&, void* aux) {
  // TrapFnc functions report error by following the native kernel convention
  // of returning an exit code in the range of -1..-4096. They do not try to
  // set errno themselves. The glibc wrapper that triggered the SIGSYS will
  // ultimately do so for us.
  int err = reinterpret_cast<intptr_t>(aux) & SECCOMP_RET_DATA;
  return -err;
}

// Function that can be passed as a callback function to CodeGen::Traverse().
// Checks whether the "insn" returns an errno value from a BPF filter. If so,
// it rewrites the instruction to instead call a Trap() handler that does
// the same thing. "aux" is ignored.
void RedirectToUserspace(Instruction* insn, void* aux) {
  // When inside an UnsafeTrap() callback, we want to allow all system calls.
  // This means, we must conditionally disable the sandbox -- and that's not
  // something that kernel-side BPF filters can do, as they cannot inspect
  // any state other than the syscall arguments.
  // But if we redirect all error handlers to user-space, then we can easily
  // make this decision.
  // The performance penalty for this extra round-trip to user-space is not
  // actually that bad, as we only ever pay it for denied system calls; and a
  // typical program has very few of these.
  SandboxBPF* sandbox = static_cast<SandboxBPF*>(aux);
  if (BPF_CLASS(insn->code) == BPF_RET &&
      (insn->k & SECCOMP_RET_ACTION) == SECCOMP_RET_ERRNO) {
    insn->k = sandbox->Trap(ReturnErrno,
        reinterpret_cast<void*>(insn->k & SECCOMP_RET_DATA)).err();
  }
}

// This wraps an existing policy and changes its behavior to match the changes
// made by RedirectToUserspace(). This is part of the framework that allows BPF
// evaluation in userland.
// TODO(markus): document the code inside better.
class RedirectToUserSpacePolicyWrapper : public SandboxBPFPolicy {
 public:
  explicit RedirectToUserSpacePolicyWrapper(
      const SandboxBPFPolicy* wrapped_policy)
      : wrapped_policy_(wrapped_policy) {
    DCHECK(wrapped_policy_);
  }

  virtual ErrorCode EvaluateSyscall(SandboxBPF* sandbox_compiler,
                                    int system_call_number) const OVERRIDE {
    ErrorCode err =
        wrapped_policy_->EvaluateSyscall(sandbox_compiler, system_call_number);
    if ((err.err() & SECCOMP_RET_ACTION) == SECCOMP_RET_ERRNO) {
      return ReturnErrnoViaTrap(sandbox_compiler, err.err() & SECCOMP_RET_DATA);
    }
    return err;
  }

  virtual ErrorCode InvalidSyscall(
      SandboxBPF* sandbox_compiler) const OVERRIDE {
    return ReturnErrnoViaTrap(sandbox_compiler, ENOSYS);
  }

 private:
  ErrorCode ReturnErrnoViaTrap(SandboxBPF* sandbox_compiler, int err) const {
    return sandbox_compiler->Trap(ReturnErrno, reinterpret_cast<void*>(err));
  }

  const SandboxBPFPolicy* wrapped_policy_;
  DISALLOW_COPY_AND_ASSIGN(RedirectToUserSpacePolicyWrapper);
};

intptr_t BPFFailure(const struct arch_seccomp_data&, void* aux) {
  SANDBOX_DIE(static_cast<char*>(aux));
}

}  // namespace

SandboxBPF::SandboxBPF()
    : quiet_(false),
      proc_fd_(-1),
      conds_(new Conds),
      sandbox_has_started_(false) {}

SandboxBPF::~SandboxBPF() {
  // It is generally unsafe to call any memory allocator operations or to even
  // call arbitrary destructors after having installed a new policy. We just
  // have no way to tell whether this policy would allow the system calls that
  // the constructors can trigger.
  // So, we normally destroy all of our complex state prior to starting the
  // sandbox. But this won't happen, if the Sandbox object was created and
  // never actually used to set up a sandbox. So, just in case, we are
  // destroying any remaining state.
  // The "if ()" statements are technically superfluous. But let's be explicit
  // that we really don't want to run any code, when we already destroyed
  // objects before setting up the sandbox.
  if (conds_) {
    delete conds_;
  }
}

bool SandboxBPF::IsValidSyscallNumber(int sysnum) {
  return SyscallIterator::IsValid(sysnum);
}

bool SandboxBPF::RunFunctionInPolicy(void (*code_in_sandbox)(),
                                     scoped_ptr<SandboxBPFPolicy> policy) {
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
                             scoped_ptr<SandboxBPFPolicy>(new ProbePolicy())) &&
         RunFunctionInPolicy(
             TryVsyscallProcess,
             scoped_ptr<SandboxBPFPolicy>(new AllowAllPolicy()));
}

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

void SandboxBPF::set_proc_fd(int proc_fd) { proc_fd_ = proc_fd; }

bool SandboxBPF::StartSandbox(SandboxThreadState thread_state) {
  CHECK(thread_state == PROCESS_SINGLE_THREADED ||
        thread_state == PROCESS_MULTI_THREADED);

  if (status_ == STATUS_UNSUPPORTED || status_ == STATUS_UNAVAILABLE) {
    SANDBOX_DIE(
        "Trying to start sandbox, even though it is known to be "
        "unavailable");
    return false;
  } else if (sandbox_has_started_ || !conds_) {
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

  if (thread_state == PROCESS_SINGLE_THREADED && !IsSingleThreaded(proc_fd_)) {
    SANDBOX_DIE("Cannot start sandbox, if process is already multi-threaded");
    return false;
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
  InstallFilter(thread_state);

  // We are now inside the sandbox.
  status_ = STATUS_ENABLED;

  return true;
}

void SandboxBPF::PolicySanityChecks(SandboxBPFPolicy* policy) {
  if (!IsDenied(policy->InvalidSyscall(this))) {
    SANDBOX_DIE("Policies should deny invalid system calls.");
  }
  return;
}

// Don't take a scoped_ptr here, polymorphism make their use awkward.
void SandboxBPF::SetSandboxPolicy(SandboxBPFPolicy* policy) {
  DCHECK(!policy_);
  if (sandbox_has_started_ || !conds_) {
    SANDBOX_DIE("Cannot change policy after sandbox has started");
  }
  PolicySanityChecks(policy);
  policy_.reset(policy);
}

void SandboxBPF::InstallFilter(SandboxThreadState thread_state) {
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
  Program* program = AssembleFilter(false /* force_verification */);

  struct sock_filter bpf[program->size()];
  const struct sock_fprog prog = {static_cast<unsigned short>(program->size()),
                                  bpf};
  memcpy(bpf, &(*program)[0], sizeof(bpf));
  delete program;

  // Make an attempt to release memory that is no longer needed here, rather
  // than in the destructor. Try to avoid as much as possible to presume of
  // what will be possible to do in the new (sandboxed) execution environment.
  delete conds_;
  conds_ = NULL;
  policy_.reset();

  // Install BPF filter program
  if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
    SANDBOX_DIE(quiet_ ? NULL : "Kernel refuses to enable no-new-privs");
  } else {
    if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog)) {
      SANDBOX_DIE(quiet_ ? NULL : "Kernel refuses to turn on BPF filters");
    }
  }

  // TODO(rsesek): Always try to engage the sandbox with the
  // PROCESS_MULTI_THREADED path first, and if that fails, assert that the
  // process IsSingleThreaded() or SANDBOX_DIE.

  if (thread_state == PROCESS_MULTI_THREADED) {
    // TODO(rsesek): Move these to a more reasonable place once the kernel
    // patch has landed upstream and these values are formalized.
    #define PR_SECCOMP_EXT 41
    #define SECCOMP_EXT_ACT 1
    #define SECCOMP_EXT_ACT_TSYNC 1
    if (prctl(PR_SECCOMP_EXT, SECCOMP_EXT_ACT, SECCOMP_EXT_ACT_TSYNC, 0, 0)) {
      SANDBOX_DIE(quiet_ ? NULL : "Kernel refuses to synchronize threadgroup "
                                  "BPF filters.");
    }
  }

  sandbox_has_started_ = true;
}

SandboxBPF::Program* SandboxBPF::AssembleFilter(bool force_verification) {
#if !defined(NDEBUG)
  force_verification = true;
#endif

  // Verify that the user pushed a policy.
  DCHECK(policy_);

  // Assemble the BPF filter program.
  CodeGen* gen = new CodeGen();
  if (!gen) {
    SANDBOX_DIE("Out of memory");
  }

  // If the architecture doesn't match SECCOMP_ARCH, disallow the
  // system call.
  Instruction* tail;
  Instruction* head = gen->MakeInstruction(
      BPF_LD + BPF_W + BPF_ABS,
      SECCOMP_ARCH_IDX,
      tail = gen->MakeInstruction(
          BPF_JMP + BPF_JEQ + BPF_K,
          SECCOMP_ARCH,
          NULL,
          gen->MakeInstruction(
              BPF_RET + BPF_K,
              Kill("Invalid audit architecture in BPF filter"))));

  bool has_unsafe_traps = false;
  {
    // Evaluate all possible system calls and group their ErrorCodes into
    // ranges of identical codes.
    Ranges ranges;
    FindRanges(&ranges);

    // Compile the system call ranges to an optimized BPF jumptable
    Instruction* jumptable =
        AssembleJumpTable(gen, ranges.begin(), ranges.end());

    // If there is at least one UnsafeTrap() in our program, the entire sandbox
    // is unsafe. We need to modify the program so that all non-
    // SECCOMP_RET_ALLOW ErrorCodes are handled in user-space. This will then
    // allow us to temporarily disable sandboxing rules inside of callbacks to
    // UnsafeTrap().
    gen->Traverse(jumptable, CheckForUnsafeErrorCodes, &has_unsafe_traps);

    // Grab the system call number, so that we can implement jump tables.
    Instruction* load_nr =
        gen->MakeInstruction(BPF_LD + BPF_W + BPF_ABS, SECCOMP_NR_IDX);

    // If our BPF program has unsafe jumps, enable support for them. This
    // test happens very early in the BPF filter program. Even before we
    // consider looking at system call numbers.
    // As support for unsafe jumps essentially defeats all the security
    // measures that the sandbox provides, we print a big warning message --
    // and of course, we make sure to only ever enable this feature if it
    // is actually requested by the sandbox policy.
    if (has_unsafe_traps) {
      if (Syscall::Call(-1) == -1 && errno == ENOSYS) {
        SANDBOX_DIE(
            "Support for UnsafeTrap() has not yet been ported to this "
            "architecture");
      }

      if (!policy_->EvaluateSyscall(this, __NR_rt_sigprocmask)
               .Equals(ErrorCode(ErrorCode::ERR_ALLOWED)) ||
          !policy_->EvaluateSyscall(this, __NR_rt_sigreturn)
               .Equals(ErrorCode(ErrorCode::ERR_ALLOWED))
#if defined(__NR_sigprocmask)
          ||
          !policy_->EvaluateSyscall(this, __NR_sigprocmask)
               .Equals(ErrorCode(ErrorCode::ERR_ALLOWED))
#endif
#if defined(__NR_sigreturn)
          ||
          !policy_->EvaluateSyscall(this, __NR_sigreturn)
               .Equals(ErrorCode(ErrorCode::ERR_ALLOWED))
#endif
          ) {
        SANDBOX_DIE(
            "Invalid seccomp policy; if using UnsafeTrap(), you must "
            "unconditionally allow sigreturn() and sigprocmask()");
      }

      if (!Trap::EnableUnsafeTrapsInSigSysHandler()) {
        // We should never be able to get here, as UnsafeTrap() should never
        // actually return a valid ErrorCode object unless the user set the
        // CHROME_SANDBOX_DEBUGGING environment variable; and therefore,
        // "has_unsafe_traps" would always be false. But better double-check
        // than enabling dangerous code.
        SANDBOX_DIE("We'd rather die than enable unsafe traps");
      }
      gen->Traverse(jumptable, RedirectToUserspace, this);

      // Allow system calls, if they originate from our magic return address
      // (which we can query by calling Syscall::Call(-1)).
      uintptr_t syscall_entry_point = static_cast<uintptr_t>(Syscall::Call(-1));
      uint32_t low = static_cast<uint32_t>(syscall_entry_point);
#if __SIZEOF_POINTER__ > 4
      uint32_t hi = static_cast<uint32_t>(syscall_entry_point >> 32);
#endif

      // BPF cannot do native 64bit comparisons. On 64bit architectures, we
      // have to compare both 32bit halves of the instruction pointer. If they
      // match what we expect, we return ERR_ALLOWED. If either or both don't
      // match, we continue evalutating the rest of the sandbox policy.
      Instruction* escape_hatch = gen->MakeInstruction(
          BPF_LD + BPF_W + BPF_ABS,
          SECCOMP_IP_LSB_IDX,
          gen->MakeInstruction(
              BPF_JMP + BPF_JEQ + BPF_K,
              low,
#if __SIZEOF_POINTER__ > 4
              gen->MakeInstruction(
                  BPF_LD + BPF_W + BPF_ABS,
                  SECCOMP_IP_MSB_IDX,
                  gen->MakeInstruction(
                      BPF_JMP + BPF_JEQ + BPF_K,
                      hi,
#endif
                      gen->MakeInstruction(BPF_RET + BPF_K,
                                           ErrorCode(ErrorCode::ERR_ALLOWED)),
#if __SIZEOF_POINTER__ > 4
                      load_nr)),
#endif
              load_nr));
      gen->JoinInstructions(tail, escape_hatch);
    } else {
      gen->JoinInstructions(tail, load_nr);
    }
    tail = load_nr;

// On Intel architectures, verify that system call numbers are in the
// expected number range. The older i386 and x86-64 APIs clear bit 30
// on all system calls. The newer x32 API always sets bit 30.
#if defined(__i386__) || defined(__x86_64__)
    Instruction* invalidX32 = gen->MakeInstruction(
        BPF_RET + BPF_K, Kill("Illegal mixing of system call ABIs").err_);
    Instruction* checkX32 =
#if defined(__x86_64__) && defined(__ILP32__)
        gen->MakeInstruction(
            BPF_JMP + BPF_JSET + BPF_K, 0x40000000, 0, invalidX32);
#else
        gen->MakeInstruction(
            BPF_JMP + BPF_JSET + BPF_K, 0x40000000, invalidX32, 0);
#endif
    gen->JoinInstructions(tail, checkX32);
    tail = checkX32;
#endif

    // Append jump table to our pre-amble
    gen->JoinInstructions(tail, jumptable);
  }

  // Turn the DAG into a vector of instructions.
  Program* program = new Program();
  gen->Compile(head, program);
  delete gen;

  // Make sure compilation resulted in BPF program that executes
  // correctly. Otherwise, there is an internal error in our BPF compiler.
  // There is really nothing the caller can do until the bug is fixed.
  if (force_verification) {
    // Verification is expensive. We only perform this step, if we are
    // compiled in debug mode, or if the caller explicitly requested
    // verification.
    VerifyProgram(*program, has_unsafe_traps);
  }

  return program;
}

void SandboxBPF::VerifyProgram(const Program& program, bool has_unsafe_traps) {
  // If we previously rewrote the BPF program so that it calls user-space
  // whenever we return an "errno" value from the filter, then we have to
  // wrap our system call evaluator to perform the same operation. Otherwise,
  // the verifier would also report a mismatch in return codes.
  scoped_ptr<const RedirectToUserSpacePolicyWrapper> redirected_policy(
      new RedirectToUserSpacePolicyWrapper(policy_.get()));

  const char* err = NULL;
  if (!Verifier::VerifyBPF(this,
                           program,
                           has_unsafe_traps ? *redirected_policy : *policy_,
                           &err)) {
    CodeGen::PrintProgram(program);
    SANDBOX_DIE(err);
  }
}

void SandboxBPF::FindRanges(Ranges* ranges) {
  // Please note that "struct seccomp_data" defines system calls as a signed
  // int32_t, but BPF instructions always operate on unsigned quantities. We
  // deal with this disparity by enumerating from MIN_SYSCALL to MAX_SYSCALL,
  // and then verifying that the rest of the number range (both positive and
  // negative) all return the same ErrorCode.
  const ErrorCode invalid_err = policy_->InvalidSyscall(this);
  uint32_t old_sysnum = 0;
  ErrorCode old_err = IsValidSyscallNumber(old_sysnum)
                          ? policy_->EvaluateSyscall(this, old_sysnum)
                          : invalid_err;

  for (SyscallIterator iter(false); !iter.Done();) {
    uint32_t sysnum = iter.Next();
    ErrorCode err =
        IsValidSyscallNumber(sysnum)
            ? policy_->EvaluateSyscall(this, static_cast<int>(sysnum))
            : invalid_err;
    if (!err.Equals(old_err) || iter.Done()) {
      ranges->push_back(Range(old_sysnum, sysnum - 1, old_err));
      old_sysnum = sysnum;
      old_err = err;
    }
  }
}

Instruction* SandboxBPF::AssembleJumpTable(CodeGen* gen,
                                           Ranges::const_iterator start,
                                           Ranges::const_iterator stop) {
  // We convert the list of system call ranges into jump table that performs
  // a binary search over the ranges.
  // As a sanity check, we need to have at least one distinct ranges for us
  // to be able to build a jump table.
  if (stop - start <= 0) {
    SANDBOX_DIE("Invalid set of system call ranges");
  } else if (stop - start == 1) {
    // If we have narrowed things down to a single range object, we can
    // return from the BPF filter program.
    return RetExpression(gen, start->err);
  }

  // Pick the range object that is located at the mid point of our list.
  // We compare our system call number against the lowest valid system call
  // number in this range object. If our number is lower, it is outside of
  // this range object. If it is greater or equal, it might be inside.
  Ranges::const_iterator mid = start + (stop - start) / 2;

  // Sub-divide the list of ranges and continue recursively.
  Instruction* jf = AssembleJumpTable(gen, start, mid);
  Instruction* jt = AssembleJumpTable(gen, mid, stop);
  return gen->MakeInstruction(BPF_JMP + BPF_JGE + BPF_K, mid->from, jt, jf);
}

Instruction* SandboxBPF::RetExpression(CodeGen* gen, const ErrorCode& err) {
  if (err.error_type_ == ErrorCode::ET_COND) {
    return CondExpression(gen, err);
  } else {
    return gen->MakeInstruction(BPF_RET + BPF_K, err);
  }
}

Instruction* SandboxBPF::CondExpression(CodeGen* gen, const ErrorCode& cond) {
  // We can only inspect the six system call arguments that are passed in
  // CPU registers.
  if (cond.argno_ < 0 || cond.argno_ >= 6) {
    SANDBOX_DIE(
        "Internal compiler error; invalid argument number "
        "encountered");
  }

  // BPF programs operate on 32bit entities. Load both halfs of the 64bit
  // system call argument and then generate suitable conditional statements.
  Instruction* msb_head = gen->MakeInstruction(
      BPF_LD + BPF_W + BPF_ABS, SECCOMP_ARG_MSB_IDX(cond.argno_));
  Instruction* msb_tail = msb_head;
  Instruction* lsb_head = gen->MakeInstruction(
      BPF_LD + BPF_W + BPF_ABS, SECCOMP_ARG_LSB_IDX(cond.argno_));
  Instruction* lsb_tail = lsb_head;

  // Emit a suitable comparison statement.
  switch (cond.op_) {
    case ErrorCode::OP_EQUAL:
      // Compare the least significant bits for equality
      lsb_tail = gen->MakeInstruction(BPF_JMP + BPF_JEQ + BPF_K,
                                      static_cast<uint32_t>(cond.value_),
                                      RetExpression(gen, *cond.passed_),
                                      RetExpression(gen, *cond.failed_));
      gen->JoinInstructions(lsb_head, lsb_tail);

      // If we are looking at a 64bit argument, we need to also compare the
      // most significant bits.
      if (cond.width_ == ErrorCode::TP_64BIT) {
        msb_tail =
            gen->MakeInstruction(BPF_JMP + BPF_JEQ + BPF_K,
                                 static_cast<uint32_t>(cond.value_ >> 32),
                                 lsb_head,
                                 RetExpression(gen, *cond.failed_));
        gen->JoinInstructions(msb_head, msb_tail);
      }
      break;
    case ErrorCode::OP_HAS_ALL_BITS:
      // Check the bits in the LSB half of the system call argument. Our
      // OP_HAS_ALL_BITS operator passes, iff all of the bits are set. This is
      // different from the kernel's BPF_JSET operation which passes, if any of
      // the bits are set.
      // Of course, if there is only a single set bit (or none at all), then
      // things get easier.
      {
        uint32_t lsb_bits = static_cast<uint32_t>(cond.value_);
        int lsb_bit_count = popcount(lsb_bits);
        if (lsb_bit_count == 0) {
          // No bits are set in the LSB half. The test will always pass.
          lsb_head = RetExpression(gen, *cond.passed_);
          lsb_tail = NULL;
        } else if (lsb_bit_count == 1) {
          // Exactly one bit is set in the LSB half. We can use the BPF_JSET
          // operator.
          lsb_tail = gen->MakeInstruction(BPF_JMP + BPF_JSET + BPF_K,
                                          lsb_bits,
                                          RetExpression(gen, *cond.passed_),
                                          RetExpression(gen, *cond.failed_));
          gen->JoinInstructions(lsb_head, lsb_tail);
        } else {
          // More than one bit is set in the LSB half. We need to combine
          // BPF_AND and BPF_JEQ to test whether all of these bits are in fact
          // set in the system call argument.
          gen->JoinInstructions(
              lsb_head,
              gen->MakeInstruction(BPF_ALU + BPF_AND + BPF_K,
                                   lsb_bits,
                                   lsb_tail = gen->MakeInstruction(
                                       BPF_JMP + BPF_JEQ + BPF_K,
                                       lsb_bits,
                                       RetExpression(gen, *cond.passed_),
                                       RetExpression(gen, *cond.failed_))));
        }
      }

      // If we are looking at a 64bit argument, we need to also check the bits
      // in the MSB half of the system call argument.
      if (cond.width_ == ErrorCode::TP_64BIT) {
        uint32_t msb_bits = static_cast<uint32_t>(cond.value_ >> 32);
        int msb_bit_count = popcount(msb_bits);
        if (msb_bit_count == 0) {
          // No bits are set in the MSB half. The test will always pass.
          msb_head = lsb_head;
        } else if (msb_bit_count == 1) {
          // Exactly one bit is set in the MSB half. We can use the BPF_JSET
          // operator.
          msb_tail = gen->MakeInstruction(BPF_JMP + BPF_JSET + BPF_K,
                                          msb_bits,
                                          lsb_head,
                                          RetExpression(gen, *cond.failed_));
          gen->JoinInstructions(msb_head, msb_tail);
        } else {
          // More than one bit is set in the MSB half. We need to combine
          // BPF_AND and BPF_JEQ to test whether all of these bits are in fact
          // set in the system call argument.
          gen->JoinInstructions(
              msb_head,
              gen->MakeInstruction(
                  BPF_ALU + BPF_AND + BPF_K,
                  msb_bits,
                  gen->MakeInstruction(BPF_JMP + BPF_JEQ + BPF_K,
                                       msb_bits,
                                       lsb_head,
                                       RetExpression(gen, *cond.failed_))));
        }
      }
      break;
    case ErrorCode::OP_HAS_ANY_BITS:
      // Check the bits in the LSB half of the system call argument. Our
      // OP_HAS_ANY_BITS operator passes, iff any of the bits are set. This maps
      // nicely to the kernel's BPF_JSET operation.
      {
        uint32_t lsb_bits = static_cast<uint32_t>(cond.value_);
        if (!lsb_bits) {
          // No bits are set in the LSB half. The test will always fail.
          lsb_head = RetExpression(gen, *cond.failed_);
          lsb_tail = NULL;
        } else {
          lsb_tail = gen->MakeInstruction(BPF_JMP + BPF_JSET + BPF_K,
                                          lsb_bits,
                                          RetExpression(gen, *cond.passed_),
                                          RetExpression(gen, *cond.failed_));
          gen->JoinInstructions(lsb_head, lsb_tail);
        }
      }

      // If we are looking at a 64bit argument, we need to also check the bits
      // in the MSB half of the system call argument.
      if (cond.width_ == ErrorCode::TP_64BIT) {
        uint32_t msb_bits = static_cast<uint32_t>(cond.value_ >> 32);
        if (!msb_bits) {
          // No bits are set in the MSB half. The test will always fail.
          msb_head = lsb_head;
        } else {
          msb_tail = gen->MakeInstruction(BPF_JMP + BPF_JSET + BPF_K,
                                          msb_bits,
                                          RetExpression(gen, *cond.passed_),
                                          lsb_head);
          gen->JoinInstructions(msb_head, msb_tail);
        }
      }
      break;
    default:
      // TODO(markus): Need to add support for OP_GREATER
      SANDBOX_DIE("Not implemented");
      break;
  }

  // Ensure that we never pass a 64bit value, when we only expect a 32bit
  // value. This is somewhat complicated by the fact that on 64bit systems,
  // callers could legitimately pass in a non-zero value in the MSB, iff the
  // LSB has been sign-extended into the MSB.
  if (cond.width_ == ErrorCode::TP_32BIT) {
    if (cond.value_ >> 32) {
      SANDBOX_DIE(
          "Invalid comparison of a 32bit system call argument "
          "against a 64bit constant; this test is always false.");
    }

    Instruction* invalid_64bit = RetExpression(gen, Unexpected64bitArgument());
#if __SIZEOF_POINTER__ > 4
    invalid_64bit = gen->MakeInstruction(
        BPF_JMP + BPF_JEQ + BPF_K,
        0xFFFFFFFF,
        gen->MakeInstruction(BPF_LD + BPF_W + BPF_ABS,
                             SECCOMP_ARG_LSB_IDX(cond.argno_),
                             gen->MakeInstruction(BPF_JMP + BPF_JGE + BPF_K,
                                                  0x80000000,
                                                  lsb_head,
                                                  invalid_64bit)),
        invalid_64bit);
#endif
    gen->JoinInstructions(
        msb_tail,
        gen->MakeInstruction(
            BPF_JMP + BPF_JEQ + BPF_K, 0, lsb_head, invalid_64bit));
  }

  return msb_head;
}

ErrorCode SandboxBPF::Unexpected64bitArgument() {
  return Kill("Unexpected 64bit argument detected");
}

ErrorCode SandboxBPF::Trap(Trap::TrapFnc fnc, const void* aux) {
  return Trap::MakeTrap(fnc, aux, true /* Safe Trap */);
}

ErrorCode SandboxBPF::UnsafeTrap(Trap::TrapFnc fnc, const void* aux) {
  return Trap::MakeTrap(fnc, aux, false /* Unsafe Trap */);
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

ErrorCode SandboxBPF::Cond(int argno,
                           ErrorCode::ArgType width,
                           ErrorCode::Operation op,
                           uint64_t value,
                           const ErrorCode& passed,
                           const ErrorCode& failed) {
  return ErrorCode(argno,
                   width,
                   op,
                   value,
                   &*conds_->insert(passed).first,
                   &*conds_->insert(failed).first);
}

ErrorCode SandboxBPF::Kill(const char* msg) {
  return Trap(BPFFailure, const_cast<char*>(msg));
}

SandboxBPF::SandboxStatus SandboxBPF::status_ = STATUS_UNKNOWN;

}  // namespace sandbox
