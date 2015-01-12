/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SandboxInfo.h"

#include <errno.h>
#include <stdlib.h>
#include <sys/prctl.h>

#include "sandbox/linux/seccomp-bpf/linux_seccomp.h"
#include "mozilla/Assertions.h"

namespace mozilla {

/* static */
const SandboxInfo SandboxInfo::sSingleton = SandboxInfo();

SandboxInfo::SandboxInfo() {
  int flags = 0;
  static_assert(sizeof(flags) >= sizeof(Flags), "enum Flags fits in an int");

  // Allow simulating the absence of seccomp-bpf support, for testing.
  if (!getenv("MOZ_FAKE_NO_SANDBOX")) {
    // Determine whether seccomp-bpf is supported by trying to
    // enable it with an invalid pointer for the filter.  This will
    // fail with EFAULT if supported and EINVAL if not, without
    // changing the process's state.
    if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, nullptr) != -1) {
      MOZ_CRASH("prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, nullptr)"
                " didn't fail");
    }
    if (errno == EFAULT) {
      flags |= kHasSeccompBPF;
    }
  }

#ifdef MOZ_CONTENT_SANDBOX
  if (!getenv("MOZ_DISABLE_CONTENT_SANDBOX")) {
    flags |= kEnabledForContent;
  }
#endif
#ifdef MOZ_GMP_SANDBOX
  if (!getenv("MOZ_DISABLE_GMP_SANDBOX")) {
    flags |= kEnabledForMedia;
  }
#endif
  if (getenv("MOZ_SANDBOX_VERBOSE")) {
    flags |= kVerbose;
  }

  mFlags = static_cast<Flags>(flags);
}

} // namespace mozilla
