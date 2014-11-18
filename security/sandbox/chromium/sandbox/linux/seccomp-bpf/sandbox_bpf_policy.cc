// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf/sandbox_bpf_policy.h"

#include <errno.h>

#include "sandbox/linux/seccomp-bpf/errorcode.h"

namespace sandbox {

ErrorCode SandboxBPFPolicy::InvalidSyscall(SandboxBPF* sandbox_compiler) const {
  return ErrorCode(ENOSYS);
}

}  // namespace sandbox
