// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SECCOMP_BPF_BPF_TESTS_H__
#define SANDBOX_LINUX_SECCOMP_BPF_BPF_TESTS_H__

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "build/build_config.h"
#include "sandbox/linux/tests/unit_tests.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"

namespace sandbox {

// A BPF_DEATH_TEST is just the same as a BPF_TEST, but it assumes that the
// test will fail with a particular known error condition. Use the DEATH_XXX()
// macros from unit_tests.h to specify the expected error condition.
// A BPF_DEATH_TEST is always disabled under ThreadSanitizer, see
// crbug.com/243968.
#define BPF_DEATH_TEST(test_case_name, test_name, death, policy, aux...) \
  void BPF_TEST_##test_name(sandbox::BPFTests<aux>::AuxType& BPF_AUX);   \
  TEST(test_case_name, DISABLE_ON_TSAN(test_name)) {                     \
    sandbox::BPFTests<aux>::TestArgs arg(BPF_TEST_##test_name, policy);  \
    sandbox::BPFTests<aux>::RunTestInProcess(                            \
        sandbox::BPFTests<aux>::TestWrapper, &arg, death);               \
  }                                                                      \
  void BPF_TEST_##test_name(sandbox::BPFTests<aux>::AuxType& BPF_AUX)

// BPF_TEST() is a special version of SANDBOX_TEST(). It turns into a no-op,
// if the host does not have kernel support for running BPF filters.
// Also, it takes advantage of the Die class to avoid calling LOG(FATAL), from
// inside our tests, as we don't need or even want all the error handling that
// LOG(FATAL) would do.
// BPF_TEST() takes a C++ data type as an optional fourth parameter. If
// present, this sets up a variable that can be accessed as "BPF_AUX". This
// variable will be passed as an argument to the "policy" function. Policies
// would typically use it as an argument to SandboxBPF::Trap(), if they want to
// communicate data between the BPF_TEST() and a Trap() function.
#define BPF_TEST(test_case_name, test_name, policy, aux...) \
  BPF_DEATH_TEST(test_case_name, test_name, DEATH_SUCCESS(), policy, aux)

// Assertions are handled exactly the same as with a normal SANDBOX_TEST()
#define BPF_ASSERT SANDBOX_ASSERT

// The "Aux" type is optional. We use an "empty" type by default, so that if
// the caller doesn't provide any type, all the BPF_AUX related data compiles
// to nothing.
template <class Aux = int[0]>
class BPFTests : public UnitTests {
 public:
  typedef Aux AuxType;

  class TestArgs {
   public:
    TestArgs(void (*t)(AuxType&), sandbox::SandboxBPF::EvaluateSyscall p)
        : test_(t), policy_(p), aux_() {}

    void (*test() const)(AuxType&) { return test_; }
    sandbox::SandboxBPF::EvaluateSyscall policy() const { return policy_; }

   private:
    friend class BPFTests;

    void (*test_)(AuxType&);
    sandbox::SandboxBPF::EvaluateSyscall policy_;
    AuxType aux_;
  };

  static void TestWrapper(void* void_arg) {
    TestArgs* arg = reinterpret_cast<TestArgs*>(void_arg);
    sandbox::Die::EnableSimpleExit();
    if (sandbox::SandboxBPF::SupportsSeccompSandbox(-1) ==
        sandbox::SandboxBPF::STATUS_AVAILABLE) {
      // Ensure the the sandbox is actually available at this time
      int proc_fd;
      BPF_ASSERT((proc_fd = open("/proc", O_RDONLY | O_DIRECTORY)) >= 0);
      BPF_ASSERT(sandbox::SandboxBPF::SupportsSeccompSandbox(proc_fd) ==
                 sandbox::SandboxBPF::STATUS_AVAILABLE);

      // Initialize and then start the sandbox with our custom policy
      sandbox::SandboxBPF sandbox;
      sandbox.set_proc_fd(proc_fd);
      sandbox.SetSandboxPolicyDeprecated(arg->policy(), &arg->aux_);
      BPF_ASSERT(sandbox.StartSandbox(
          sandbox::SandboxBPF::PROCESS_SINGLE_THREADED));

      arg->test()(arg->aux_);
    } else {
      printf("This BPF test is not fully running in this configuration!\n");
      // Android and Valgrind are the only configurations where we accept not
      // having kernel BPF support.
      if (!IsAndroid() && !IsRunningOnValgrind()) {
        const bool seccomp_bpf_is_supported = false;
        BPF_ASSERT(seccomp_bpf_is_supported);
      }
      // Call the compiler and verify the policy. That's the least we can do,
      // if we don't have kernel support.
      sandbox::SandboxBPF sandbox;
      sandbox.SetSandboxPolicyDeprecated(arg->policy(), &arg->aux_);
      sandbox::SandboxBPF::Program* program =
          sandbox.AssembleFilter(true /* force_verification */);
      delete program;
      sandbox::UnitTests::IgnoreThisTest();
    }
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(BPFTests);
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SECCOMP_BPF_BPF_TESTS_H__
