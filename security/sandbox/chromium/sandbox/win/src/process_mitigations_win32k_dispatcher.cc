// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/process_mitigations_win32k_dispatcher.h"
#include "sandbox/win/src/interception.h"
#include "sandbox/win/src/interceptors.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/process_mitigations_win32k_interception.h"

namespace sandbox {

ProcessMitigationsWin32KDispatcher::ProcessMitigationsWin32KDispatcher(
    PolicyBase* policy_base)
    : policy_base_(policy_base) {
}

bool ProcessMitigationsWin32KDispatcher::SetupService(
    InterceptionManager* manager, int service) {
  if (!(policy_base_->GetProcessMitigations() &
        sandbox::MITIGATION_WIN32K_DISABLE)) {
    return false;
  }

  switch (service) {
    case IPC_GDI_GDIDLLINITIALIZE_TAG: {
      if (!INTERCEPT_EAT(manager, L"gdi32.dll", GdiDllInitialize,
                         GDIINITIALIZE_ID, 12)) {
        return false;
      }
      return true;
    }

    case IPC_GDI_GETSTOCKOBJECT_TAG: {
      if (!INTERCEPT_EAT(manager, L"gdi32.dll", GetStockObject,
                         GETSTOCKOBJECT_ID, 8)) {
        return false;
      }
      return true;
    }

    case IPC_USER_REGISTERCLASSW_TAG: {
      if (!INTERCEPT_EAT(manager, L"user32.dll", RegisterClassW,
                         REGISTERCLASSW_ID, 8)) {
        return false;
      }
      return true;
    }

    default:
      break;
  }
  return false;
}

}  // namespace sandbox

