// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SECCOMP_BPF_BPF_TESTER_COMPATIBILITY_DELEGATE_H_
#define SANDBOX_LINUX_SECCOMP_BPF_BPF_TESTER_COMPATIBILITY_DELEGATE_H_

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "base/memory/scoped_ptr.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf_compatibility_policy.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf_test_runner.h"
#include "sandbox/linux/tests/sandbox_test_runner.h"
#include "sandbox/linux/tests/unit_tests.h"

namespace sandbox {

// This templated class allows building a BPFTesterDelegate from a
// deprecated-style BPF policy (that is a SyscallEvaluator function pointer,
// instead of a SandboxBPFPolicy class), specified in |policy_function| and a
// function pointer to a test in |test_function|.
// This allows both the policy and the test function to take a pointer to an
// object of type "Aux" as a parameter. This is used to implement the BPF_TEST
// macro and should generally not be used directly.
template <class Aux>
class BPFTesterCompatibilityDelegate : public BPFTesterDelegate {
 public:
  typedef Aux AuxType;
  BPFTesterCompatibilityDelegate(
      void (*test_function)(AuxType*),
      typename CompatibilityPolicy<AuxType>::SyscallEvaluator policy_function)
      : aux_(),
        test_function_(test_function),
        policy_function_(policy_function) {}

  virtual ~BPFTesterCompatibilityDelegate() {}

  virtual scoped_ptr<SandboxBPFPolicy> GetSandboxBPFPolicy() OVERRIDE {
    // The current method is guaranteed to only run in the child process
    // running the test. In this process, the current object is guaranteed
    // to live forever. So it's ok to pass aux_pointer_for_policy_ to
    // the policy, which could in turn pass it to the kernel via Trap().
    return scoped_ptr<SandboxBPFPolicy>(
        new CompatibilityPolicy<AuxType>(policy_function_, &aux_));
  }

  virtual void RunTestFunction() OVERRIDE {
    // Run the actual test.
    // The current object is guaranteed to live forever in the child process
    // where this will run.
    test_function_(&aux_);
  }

 private:
  AuxType aux_;
  void (*test_function_)(AuxType*);
  typename CompatibilityPolicy<AuxType>::SyscallEvaluator policy_function_;
  DISALLOW_COPY_AND_ASSIGN(BPFTesterCompatibilityDelegate);
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SECCOMP_BPF_BPF_TESTER_COMPATIBILITY_DELEGATE_H_
