// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/process_mitigations_win32k_policy.h"

namespace sandbox {

bool ProcessMitigationsWin32KLockdownPolicy::GenerateRules(
    const wchar_t* name,
    TargetPolicy::Semantics semantics,
    LowLevelPolicy* policy) {
  PolicyRule rule(FAKE_SUCCESS);
  if (!policy->AddRule(IPC_GDI_GDIDLLINITIALIZE_TAG, &rule))
    return false;
  if (!policy->AddRule(IPC_GDI_GETSTOCKOBJECT_TAG, &rule))
    return false;
  if (!policy->AddRule(IPC_USER_REGISTERCLASSW_TAG, &rule))
    return false;
  return true;
}

}  // namespace sandbox

