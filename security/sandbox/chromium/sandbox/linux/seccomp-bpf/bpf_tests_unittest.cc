// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf/bpf_tests.h"

#include <errno.h>
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "build/build_config.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/services/linux_syscalls.h"
#include "sandbox/linux/tests/unit_tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

namespace {

class FourtyTwo {
 public:
  static const int kMagicValue = 42;
  FourtyTwo() : value_(kMagicValue) {}
  int value() { return value_; }

 private:
  int value_;
  DISALLOW_COPY_AND_ASSIGN(FourtyTwo);
};

ErrorCode EmptyPolicyTakesClass(SandboxBPF* sandbox,
                                int sysno,
                                FourtyTwo* fourty_two) {
  // |aux| should point to an instance of FourtyTwo.
  BPF_ASSERT(fourty_two);
  BPF_ASSERT(FourtyTwo::kMagicValue == fourty_two->value());
  if (!SandboxBPF::IsValidSyscallNumber(sysno)) {
    return ErrorCode(ENOSYS);
  } else {
    return ErrorCode(ErrorCode::ERR_ALLOWED);
  }
}

BPF_TEST(BPFTest,
         BPFAUXPointsToClass,
         EmptyPolicyTakesClass,
         FourtyTwo /* *BPF_AUX */) {
  // BPF_AUX should point to an instance of FourtyTwo.
  BPF_ASSERT(BPF_AUX);
  BPF_ASSERT(FourtyTwo::kMagicValue == BPF_AUX->value());
}

void DummyTestFunction(FourtyTwo *fourty_two) {
}

TEST(BPFTest, BPFTesterCompatibilityDelegateLeakTest) {
  // Don't do anything, simply gives dynamic tools an opportunity to detect
  // leaks.
  {
    BPFTesterCompatibilityDelegate<FourtyTwo> simple_delegate(
        DummyTestFunction, EmptyPolicyTakesClass);
  }
  {
    // Test polymorphism.
    scoped_ptr<BPFTesterDelegate> simple_delegate(
        new BPFTesterCompatibilityDelegate<FourtyTwo>(DummyTestFunction,
                                                      EmptyPolicyTakesClass));
  }
}

class EnosysPtracePolicy : public SandboxBPFPolicy {
 public:
  EnosysPtracePolicy() {
    my_pid_ = syscall(__NR_getpid);
  }
  virtual ~EnosysPtracePolicy() {
    // Policies should be able to bind with the process on which they are
    // created. They should never be created in a parent process.
    BPF_ASSERT_EQ(my_pid_, syscall(__NR_getpid));
  }

  virtual ErrorCode EvaluateSyscall(SandboxBPF* sandbox_compiler,
                                    int system_call_number) const OVERRIDE {
    if (!SandboxBPF::IsValidSyscallNumber(system_call_number)) {
      return ErrorCode(ENOSYS);
    } else if (system_call_number == __NR_ptrace) {
      // The EvaluateSyscall function should run in the process that created
      // the current object.
      BPF_ASSERT_EQ(my_pid_, syscall(__NR_getpid));
      return ErrorCode(ENOSYS);
    } else {
      return ErrorCode(ErrorCode::ERR_ALLOWED);
    }
  }

 private:
  pid_t my_pid_;
  DISALLOW_COPY_AND_ASSIGN(EnosysPtracePolicy);
};

class BasicBPFTesterDelegate : public BPFTesterDelegate {
 public:
  BasicBPFTesterDelegate() {}
  virtual ~BasicBPFTesterDelegate() {}

  virtual scoped_ptr<SandboxBPFPolicy> GetSandboxBPFPolicy() OVERRIDE {
    return scoped_ptr<SandboxBPFPolicy>(new EnosysPtracePolicy());
  }
  virtual void RunTestFunction() OVERRIDE {
    errno = 0;
    int ret = ptrace(PTRACE_TRACEME, -1, NULL, NULL);
    BPF_ASSERT(-1 == ret);
    BPF_ASSERT(ENOSYS == errno);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BasicBPFTesterDelegate);
};

// This is the most powerful and complex way to create a BPF test, but it
// requires a full class definition (BasicBPFTesterDelegate).
BPF_TEST_D(BPFTest, BPFTestWithDelegateClass, BasicBPFTesterDelegate);

// This is the simplest form of BPF tests.
BPF_TEST_C(BPFTest, BPFTestWithInlineTest, EnosysPtracePolicy) {
  errno = 0;
  int ret = ptrace(PTRACE_TRACEME, -1, NULL, NULL);
  BPF_ASSERT(-1 == ret);
  BPF_ASSERT(ENOSYS == errno);
}

}  // namespace

}  // namespace sandbox
