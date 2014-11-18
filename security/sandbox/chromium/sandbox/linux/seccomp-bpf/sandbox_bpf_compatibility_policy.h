// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SECCOMP_BPF_SANDBOX_BPF_COMPATIBILITY_POLICY_H_
#define SANDBOX_LINUX_SECCOMP_BPF_SANDBOX_BPF_COMPATIBILITY_POLICY_H_

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/macros.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf_policy.h"

namespace sandbox {

// This class allows compatibility with the old, deprecated
// policies that were designed for SetSandboxPolicyDeprecated().
template <class AuxType>
class CompatibilityPolicy : public SandboxBPFPolicy {
 public:
  typedef ErrorCode (*SyscallEvaluator)(SandboxBPF* sandbox_compiler,
                                        int system_call_number,
                                        AuxType* aux);
  CompatibilityPolicy(SyscallEvaluator syscall_evaluator, AuxType* aux)
      : syscall_evaluator_(syscall_evaluator), aux_(aux) {}

  virtual ~CompatibilityPolicy() {}

  virtual ErrorCode EvaluateSyscall(SandboxBPF* sandbox_compiler,
                                    int system_call_number) const OVERRIDE {
    DCHECK(SandboxBPF::IsValidSyscallNumber(system_call_number));
    return syscall_evaluator_(sandbox_compiler, system_call_number, aux_);
  }

 private:
  SyscallEvaluator syscall_evaluator_;
  AuxType* aux_;
  DISALLOW_COPY_AND_ASSIGN(CompatibilityPolicy);
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SECCOMP_BPF_SANDBOX_BPF_COMPATIBILITY_POLICY_H_
