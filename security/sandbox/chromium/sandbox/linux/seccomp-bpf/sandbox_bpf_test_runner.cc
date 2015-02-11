// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf/sandbox_bpf_test_runner.h"

#include <fcntl.h>
#include <linux/filter.h>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "sandbox/linux/bpf_dsl/policy.h"
#include "sandbox/linux/seccomp-bpf/die.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/tests/unit_tests.h"

namespace sandbox {

SandboxBPFTestRunner::SandboxBPFTestRunner(
    BPFTesterDelegate* bpf_tester_delegate)
    : bpf_tester_delegate_(bpf_tester_delegate) {
}

SandboxBPFTestRunner::~SandboxBPFTestRunner() {
}

void SandboxBPFTestRunner::Run() {
  DCHECK(bpf_tester_delegate_);
  sandbox::Die::EnableSimpleExit();

  scoped_ptr<bpf_dsl::Policy> policy =
      bpf_tester_delegate_->GetSandboxBPFPolicy();

  if (sandbox::SandboxBPF::SupportsSeccompSandbox(-1) ==
      sandbox::SandboxBPF::STATUS_AVAILABLE) {
    // Ensure the the sandbox is actually available at this time
    int proc_fd;
    SANDBOX_ASSERT((proc_fd = open("/proc", O_RDONLY | O_DIRECTORY)) >= 0);
    SANDBOX_ASSERT(sandbox::SandboxBPF::SupportsSeccompSandbox(proc_fd) ==
                   sandbox::SandboxBPF::STATUS_AVAILABLE);

    // Initialize and then start the sandbox with our custom policy
    sandbox::SandboxBPF sandbox;
    sandbox.set_proc_fd(proc_fd);
    sandbox.SetSandboxPolicy(policy.release());
    SANDBOX_ASSERT(
        sandbox.StartSandbox(sandbox::SandboxBPF::PROCESS_SINGLE_THREADED));

    // Run the actual test.
    bpf_tester_delegate_->RunTestFunction();
  } else {
    printf("This BPF test is not fully running in this configuration!\n");
    // Android and Valgrind are the only configurations where we accept not
    // having kernel BPF support.
    if (!IsAndroid() && !IsRunningOnValgrind()) {
      const bool seccomp_bpf_is_supported = false;
      SANDBOX_ASSERT(seccomp_bpf_is_supported);
    }
    // Call the compiler and verify the policy. That's the least we can do,
    // if we don't have kernel support.
    sandbox::SandboxBPF sandbox;
    sandbox.SetSandboxPolicy(policy.release());
    sandbox.AssembleFilter(true /* force_verification */);
    sandbox::UnitTests::IgnoreThisTest();
  }
}

bool SandboxBPFTestRunner::ShouldCheckForLeaks() const {
  // LSAN requires being able to use ptrace() and other system calls that could
  // be denied.
  return false;
}

}  // namespace sandbox
