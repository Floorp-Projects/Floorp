// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SECCOMP_BPF_SANDBOX_BPF_H__
#define SANDBOX_LINUX_SECCOMP_BPF_SANDBOX_BPF_H__

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "sandbox/linux/seccomp-bpf/codegen.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {
struct arch_seccomp_data;
namespace bpf_dsl {
class Policy;
}

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

  // Determines if the kernel has support for the seccomp() system call to
  // synchronize BPF filters across a thread group.
  static SandboxStatus SupportsSeccompThreadFilterSynchronization();

  // The sandbox needs to be able to access files in "/proc/self". If this
  // directory is not accessible when "startSandbox()" gets called, the caller
  // can provide an already opened file descriptor by calling "set_proc_fd()".
  // The sandbox becomes the new owner of this file descriptor and will
  // eventually close it when "StartSandbox()" executes.
  void set_proc_fd(int proc_fd);

  // Set the BPF policy as |policy|. Ownership of |policy| is transfered here
  // to the sandbox object.
  void SetSandboxPolicy(bpf_dsl::Policy* policy);

  // UnsafeTraps require some syscalls to always be allowed.
  // This helper function returns true for these calls.
  static bool IsRequiredForUnsafeTrap(int sysno);

  // From within an UnsafeTrap() it is often useful to be able to execute
  // the system call that triggered the trap. The ForwardSyscall() method
  // makes this easy. It is more efficient than calling glibc's syscall()
  // function, as it avoid the extra round-trip to the signal handler. And
  // it automatically does the correct thing to report kernel-style error
  // conditions, rather than setting errno. See the comments for TrapFnc for
  // details. In other words, the return value from ForwardSyscall() is
  // directly suitable as a return value for a trap handler.
  static intptr_t ForwardSyscall(const struct arch_seccomp_data& args);

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
  scoped_ptr<CodeGen::Program> AssembleFilter(bool force_verification);

 private:
  // Get a file descriptor pointing to "/proc", if currently available.
  int proc_fd() { return proc_fd_; }

  // Creates a subprocess and runs "code_in_sandbox" inside of the specified
  // policy. The caller has to make sure that "this" has not yet been
  // initialized with any other policies.
  bool RunFunctionInPolicy(void (*code_in_sandbox)(),
                           scoped_ptr<bpf_dsl::Policy> policy);

  // Performs a couple of sanity checks to verify that the kernel supports the
  // features that we need for successful sandboxing.
  // The caller has to make sure that "this" has not yet been initialized with
  // any other policies.
  bool KernelSupportSeccompBPF();

  // Assembles and installs a filter based on the policy that has previously
  // been configured with SetSandboxPolicy().
  void InstallFilter(bool must_sync_threads);

  // Verify the correctness of a compiled program by comparing it against the
  // current policy. This function should only ever be called by unit tests and
  // by the sandbox internals. It should not be used by production code.
  void VerifyProgram(const CodeGen::Program& program);

  static SandboxStatus status_;

  bool quiet_;
  int proc_fd_;
  bool sandbox_has_started_;
  scoped_ptr<bpf_dsl::Policy> policy_;

  DISALLOW_COPY_AND_ASSIGN(SandboxBPF);
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SECCOMP_BPF_SANDBOX_BPF_H__
