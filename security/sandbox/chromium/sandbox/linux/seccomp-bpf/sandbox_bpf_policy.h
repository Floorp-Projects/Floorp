// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SECCOMP_BPF_SANDBOX_BPF_POLICY_H_
#define SANDBOX_LINUX_SECCOMP_BPF_SANDBOX_BPF_POLICY_H_

#include "base/basictypes.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {

class ErrorCode;
class SandboxBPF;

// This is the interface to implement to define a BPF sandbox policy.
class SANDBOX_EXPORT SandboxBPFPolicy {
 public:
  SandboxBPFPolicy() {}
  virtual ~SandboxBPFPolicy() {}

  // The EvaluateSyscall method is called with the system call number. It can
  // decide to allow the system call unconditionally by returning ERR_ALLOWED;
  // it can deny the system call unconditionally by returning an appropriate
  // "errno" value; or it can request inspection of system call argument(s) by
  // returning a suitable ErrorCode.
  // Will only be called for valid system call numbers.
  virtual ErrorCode EvaluateSyscall(SandboxBPF* sandbox_compiler,
                                    int system_call_number) const = 0;

  // The InvalidSyscall method specifies the behavior used for invalid
  // system calls.  The default implementation is to return ENOSYS.
  virtual ErrorCode InvalidSyscall(SandboxBPF* sandbox_compiler) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(SandboxBPFPolicy);
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SECCOMP_BPF_SANDBOX_BPF_POLICY_H_
