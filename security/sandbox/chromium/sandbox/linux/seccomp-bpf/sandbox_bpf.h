// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SECCOMP_BPF_SANDBOX_BPF_H__
#define SANDBOX_LINUX_SECCOMP_BPF_SANDBOX_BPF_H__

#include <stddef.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "sandbox/linux/sandbox_export.h"
#include "sandbox/linux/seccomp-bpf/die.h"
#include "sandbox/linux/seccomp-bpf/errorcode.h"
#include "sandbox/linux/seccomp-bpf/linux_seccomp.h"

namespace sandbox {

struct arch_seccomp_data {
  int nr;
  uint32_t arch;
  uint64_t instruction_pointer;
  uint64_t args[6];
};

struct arch_sigsys {
  void* ip;
  int nr;
  unsigned int arch;
};

class CodeGen;
class SandboxBPFPolicy;
class SandboxUnittestHelper;
struct Instruction;

class SANDBOX_EXPORT SandboxBPF {
 public:
  enum SandboxStatus {
    STATUS_UNKNOWN,      // Status prior to calling supportsSeccompSandbox()
    STATUS_UNSUPPORTED,  // The kernel does not appear to support sandboxing
    STATUS_UNAVAILABLE,  // Currently unavailable but might work again later
    STATUS_AVAILABLE,    // Sandboxing is available but not currently active
    STATUS_ENABLED       // The sandbox is now active
  };

  // Depending on the level of kernel support, seccomp-bpf may require the
  // process to be single-threaded in order to enable it. When calling
  // StartSandbox(), the program should indicate whether or not the sandbox
  // should try and engage with multi-thread support.
  enum SandboxThreadState {
    PROCESS_INVALID,
    PROCESS_SINGLE_THREADED,  // The program is currently single-threaded.
    // Note: PROCESS_MULTI_THREADED requires experimental kernel support that
    // has not been contributed to upstream Linux.
    PROCESS_MULTI_THREADED,   // The program may be multi-threaded.
  };

  // When calling setSandboxPolicy(), the caller can provide an arbitrary
  // pointer in |aux|. This pointer will then be forwarded to the sandbox
  // policy each time a call is made through an EvaluateSyscall function
  // pointer.  One common use case would be to pass the "aux" pointer as an
  // argument to Trap() functions.
  typedef ErrorCode (*EvaluateSyscall)(SandboxBPF* sandbox_compiler,
                                       int system_call_number,
                                       void* aux);
  typedef std::vector<std::pair<EvaluateSyscall, void*> > Evaluators;
  // A vector of BPF instructions that need to be installed as a filter
  // program in the kernel.
  typedef std::vector<struct sock_filter> Program;

  // Constructors and destructors.
  // NOTE: Setting a policy and starting the sandbox is a one-way operation.
  //       The kernel does not provide any option for unloading a loaded
  //       sandbox. Strictly speaking, that means we should disallow calling
  //       the destructor, if StartSandbox() has ever been called. In practice,
  //       this makes it needlessly complicated to operate on "Sandbox"
  //       objects. So, we instead opted to allow object destruction. But it
  //       should be noted that during its lifetime, the object probably made
  //       irreversible state changes to the runtime environment. These changes
  //       stay in effect even after the destructor has been run.
  SandboxBPF();
  ~SandboxBPF();

  // Checks whether a particular system call number is valid on the current
  // architecture. E.g. on ARM there's a non-contiguous range of private
  // system calls.
  static bool IsValidSyscallNumber(int sysnum);

  // There are a lot of reasons why the Seccomp sandbox might not be available.
  // This could be because the kernel does not support Seccomp mode, or it
  // could be because another sandbox is already active.
  // "proc_fd" should be a file descriptor for "/proc", or -1 if not
  // provided by the caller.
  static SandboxStatus SupportsSeccompSandbox(int proc_fd);

  // The sandbox needs to be able to access files in "/proc/self". If this
  // directory is not accessible when "startSandbox()" gets called, the caller
  // can provide an already opened file descriptor by calling "set_proc_fd()".
  // The sandbox becomes the new owner of this file descriptor and will
  // eventually close it when "StartSandbox()" executes.
  void set_proc_fd(int proc_fd);

  // The system call evaluator function is called with the system
  // call number. It can decide to allow the system call unconditionally
  // by returning ERR_ALLOWED; it can deny the system call unconditionally by
  // returning an appropriate "errno" value; or it can request inspection
  // of system call argument(s) by returning a suitable ErrorCode.
  // The "aux" parameter can be used to pass optional data to the system call
  // evaluator. There are different possible uses for this data, but one of the
  // use cases would be for the policy to then forward this pointer to a Trap()
  // handler. In this case, of course, the data that is pointed to must remain
  // valid for the entire time that Trap() handlers can be called; typically,
  // this would be the lifetime of the program.
  // DEPRECATED: use the policy interface below.
  void SetSandboxPolicyDeprecated(EvaluateSyscall syscallEvaluator, void* aux);

  // Set the BPF policy as |policy|. Ownership of |policy| is transfered here
  // to the sandbox object.
  void SetSandboxPolicy(SandboxBPFPolicy* policy);

  // We can use ErrorCode to request calling of a trap handler. This method
  // performs the required wrapping of the callback function into an
  // ErrorCode object.
  // The "aux" field can carry a pointer to arbitrary data. See EvaluateSyscall
  // for a description of how to pass data from SetSandboxPolicy() to a Trap()
  // handler.
  ErrorCode Trap(Trap::TrapFnc fnc, const void* aux);

  // Calls a user-space trap handler and disables all sandboxing for system
  // calls made from this trap handler.
  // This feature is available only if explicitly enabled by the user having
  // set the CHROME_SANDBOX_DEBUGGING environment variable.
  // Returns an ET_INVALID ErrorCode, if called when not enabled.
  // NOTE: This feature, by definition, disables all security features of
  //   the sandbox. It should never be used in production, but it can be
  //   very useful to diagnose code that is incompatible with the sandbox.
  //   If even a single system call returns "UnsafeTrap", the security of
  //   entire sandbox should be considered compromised.
  ErrorCode UnsafeTrap(Trap::TrapFnc fnc, const void* aux);

  // From within an UnsafeTrap() it is often useful to be able to execute
  // the system call that triggered the trap. The ForwardSyscall() method
  // makes this easy. It is more efficient than calling glibc's syscall()
  // function, as it avoid the extra round-trip to the signal handler. And
  // it automatically does the correct thing to report kernel-style error
  // conditions, rather than setting errno. See the comments for TrapFnc for
  // details. In other words, the return value from ForwardSyscall() is
  // directly suitable as a return value for a trap handler.
  static intptr_t ForwardSyscall(const struct arch_seccomp_data& args);

  // We can also use ErrorCode to request evaluation of a conditional
  // statement based on inspection of system call parameters.
  // This method wrap an ErrorCode object around the conditional statement.
  // Argument "argno" (1..6) will be compared to "value" using comparator
  // "op". If the condition is true "passed" will be returned, otherwise
  // "failed".
  // If "is32bit" is set, the argument must in the range of 0x0..(1u << 32 - 1)
  // If it is outside this range, the sandbox treats the system call just
  // the same as any other ABI violation (i.e. it aborts with an error
  // message).
  ErrorCode Cond(int argno,
                 ErrorCode::ArgType is_32bit,
                 ErrorCode::Operation op,
                 uint64_t value,
                 const ErrorCode& passed,
                 const ErrorCode& failed);

  // Kill the program and print an error message.
  ErrorCode Kill(const char* msg);

  // This is the main public entry point. It finds all system calls that
  // need rewriting, sets up the resources needed by the sandbox, and
  // enters Seccomp mode.
  // The calling process must specify its current SandboxThreadState, as a way
  // to tell the sandbox which type of kernel support it should engage.
  // It is possible to stack multiple sandboxes by creating separate "Sandbox"
  // objects and calling "StartSandbox()" on each of them. Please note, that
  // this requires special care, though, as newly stacked sandboxes can never
  // relax restrictions imposed by earlier sandboxes. Furthermore, installing
  // a new policy requires making system calls, that might already be
  // disallowed.
  // Finally, stacking does add more kernel overhead than having a single
  // combined policy. So, it should only be used if there are no alternatives.
  bool StartSandbox(SandboxThreadState thread_state) WARN_UNUSED_RESULT;

  // Assembles a BPF filter program from the current policy. After calling this
  // function, you must not call any other sandboxing function.
  // Typically, AssembleFilter() is only used by unit tests and by sandbox
  // internals. It should not be used by production code.
  // For performance reasons, we normally only run the assembled BPF program
  // through the verifier, iff the program was built in debug mode.
  // But by setting "force_verification", the caller can request that the
  // verifier is run unconditionally. This is useful for unittests.
  Program* AssembleFilter(bool force_verification);

  // Returns the fatal ErrorCode that is used to indicate that somebody
  // attempted to pass a 64bit value in a 32bit system call argument.
  // This method is primarily needed for testing purposes.
  ErrorCode Unexpected64bitArgument();

 private:
  friend class CodeGen;
  friend class SandboxUnittestHelper;
  friend class ErrorCode;

  struct Range {
    Range(uint32_t f, uint32_t t, const ErrorCode& e)
        : from(f), to(t), err(e) {}
    uint32_t from, to;
    ErrorCode err;
  };
  typedef std::vector<Range> Ranges;
  typedef std::map<uint32_t, ErrorCode> ErrMap;
  typedef std::set<ErrorCode, struct ErrorCode::LessThan> Conds;

  // Get a file descriptor pointing to "/proc", if currently available.
  int proc_fd() { return proc_fd_; }

  // Creates a subprocess and runs "code_in_sandbox" inside of the specified
  // policy. The caller has to make sure that "this" has not yet been
  // initialized with any other policies.
  bool RunFunctionInPolicy(void (*code_in_sandbox)(),
                           EvaluateSyscall syscall_evaluator,
                           void* aux);

  // Performs a couple of sanity checks to verify that the kernel supports the
  // features that we need for successful sandboxing.
  // The caller has to make sure that "this" has not yet been initialized with
  // any other policies.
  bool KernelSupportSeccompBPF();

  // Verify that the current policy passes some basic sanity checks.
  void PolicySanityChecks(SandboxBPFPolicy* policy);

  // Assembles and installs a filter based on the policy that has previously
  // been configured with SetSandboxPolicy().
  void InstallFilter(SandboxThreadState thread_state);

  // Verify the correctness of a compiled program by comparing it against the
  // current policy. This function should only ever be called by unit tests and
  // by the sandbox internals. It should not be used by production code.
  void VerifyProgram(const Program& program, bool has_unsafe_traps);

  // Finds all the ranges of system calls that need to be handled. Ranges are
  // sorted in ascending order of system call numbers. There are no gaps in the
  // ranges. System calls with identical ErrorCodes are coalesced into a single
  // range.
  void FindRanges(Ranges* ranges);

  // Returns a BPF program snippet that implements a jump table for the
  // given range of system call numbers. This function runs recursively.
  Instruction* AssembleJumpTable(CodeGen* gen,
                                 Ranges::const_iterator start,
                                 Ranges::const_iterator stop);

  // Returns a BPF program snippet that makes the BPF filter program exit
  // with the given ErrorCode "err". N.B. the ErrorCode may very well be a
  // conditional expression; if so, this function will recursively call
  // CondExpression() and possibly RetExpression() to build a complex set of
  // instructions.
  Instruction* RetExpression(CodeGen* gen, const ErrorCode& err);

  // Returns a BPF program that evaluates the conditional expression in
  // "cond" and returns the appropriate value from the BPF filter program.
  // This function recursively calls RetExpression(); it should only ever be
  // called from RetExpression().
  Instruction* CondExpression(CodeGen* gen, const ErrorCode& cond);

  static SandboxStatus status_;

  bool quiet_;
  int proc_fd_;
  scoped_ptr<const SandboxBPFPolicy> policy_;
  Conds* conds_;
  bool sandbox_has_started_;

  DISALLOW_COPY_AND_ASSIGN(SandboxBPF);
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SECCOMP_BPF_SANDBOX_BPF_H__
