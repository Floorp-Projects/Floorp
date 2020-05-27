/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DynamicallyLinkedFunctionPtr.h"
#include "mozilla/WindowsProcessMitigations.h"

#include <processthreadsapi.h>

#if (_WIN32_WINNT < 0x0602)
BOOL WINAPI GetProcessMitigationPolicy(
    HANDLE hProcess, PROCESS_MITIGATION_POLICY MitigationPolicy, PVOID lpBuffer,
    SIZE_T dwLength);
#endif  // (_WIN32_WINNT < 0x0602)

// MinGW does not have these definitions in winnt.h yet
#if defined(__MINGW32__)
constexpr PROCESS_MITIGATION_POLICY ProcessPayloadRestrictionPolicy =
    static_cast<PROCESS_MITIGATION_POLICY>(12);

typedef struct _PROCESS_MITIGATION_PAYLOAD_RESTRICTION_POLICY {
  union {
    DWORD Flags;
    struct {
      DWORD EnableExportAddressFilter : 1;
      DWORD AuditExportAddressFilter : 1;
      DWORD EnableExportAddressFilterPlus : 1;
      DWORD AuditExportAddressFilterPlus : 1;
      DWORD EnableImportAddressFilter : 1;
      DWORD AuditImportAddressFilter : 1;
      DWORD EnableRopStackPivot : 1;
      DWORD AuditRopStackPivot : 1;
      DWORD EnableRopCallerCheck : 1;
      DWORD AuditRopCallerCheck : 1;
      DWORD EnableRopSimExec : 1;
      DWORD AuditRopSimExec : 1;
      DWORD ReservedFlags : 20;
    } DUMMYSTRUCTNAME;
  } DUMMYUNIONNAME;
} PROCESS_MITIGATION_PAYLOAD_RESTRICTION_POLICY,
    *PPROCESS_MITIGATION_PAYLOAD_RESTRICTION_POLICY;
#endif  // defined(__MINGW32__)

namespace mozilla {

static decltype(&::GetProcessMitigationPolicy)
FetchGetProcessMitigationPolicyFunc() {
  static const StaticDynamicallyLinkedFunctionPtr<decltype(
      &::GetProcessMitigationPolicy)>
      pGetProcessMitigationPolicy(L"kernel32.dll",
                                  "GetProcessMitigationPolicy");
  return pGetProcessMitigationPolicy;
}

MFBT_API bool IsWin32kLockedDown() {
  auto pGetProcessMitigationPolicy = FetchGetProcessMitigationPolicyFunc();
  if (!pGetProcessMitigationPolicy) {
    return false;
  }

  PROCESS_MITIGATION_SYSTEM_CALL_DISABLE_POLICY polInfo;
  if (!pGetProcessMitigationPolicy(::GetCurrentProcess(),
                                   ProcessSystemCallDisablePolicy, &polInfo,
                                   sizeof(polInfo))) {
    return false;
  }

  return polInfo.DisallowWin32kSystemCalls;
}

MFBT_API bool IsDynamicCodeDisabled() {
  auto pGetProcessMitigationPolicy = FetchGetProcessMitigationPolicyFunc();
  if (!pGetProcessMitigationPolicy) {
    return false;
  }

  PROCESS_MITIGATION_DYNAMIC_CODE_POLICY polInfo;
  if (!pGetProcessMitigationPolicy(::GetCurrentProcess(),
                                   ProcessDynamicCodePolicy, &polInfo,
                                   sizeof(polInfo))) {
    return false;
  }

  return polInfo.ProhibitDynamicCode;
}

MFBT_API bool IsEafPlusEnabled() {
  auto pGetProcessMitigationPolicy = FetchGetProcessMitigationPolicyFunc();
  if (!pGetProcessMitigationPolicy) {
    return false;
  }

  PROCESS_MITIGATION_PAYLOAD_RESTRICTION_POLICY polInfo;
  if (!pGetProcessMitigationPolicy(::GetCurrentProcess(),
                                   ProcessPayloadRestrictionPolicy, &polInfo,
                                   sizeof(polInfo))) {
    return false;
  }

  return polInfo.EnableExportAddressFilterPlus;
}

}  // namespace mozilla
