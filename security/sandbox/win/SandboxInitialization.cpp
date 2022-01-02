/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SandboxInitialization.h"

#include "base/memory/ref_counted.h"
#include "nsWindowsDllInterceptor.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/sandboxing/permissionsService.h"
#include "mozilla/WindowsProcessMitigations.h"

namespace mozilla {
namespace sandboxing {

typedef BOOL(WINAPI* CloseHandle_func)(HANDLE hObject);
static WindowsDllInterceptor::FuncHookType<CloseHandle_func> stub_CloseHandle;

typedef BOOL(WINAPI* DuplicateHandle_func)(
    HANDLE hSourceProcessHandle, HANDLE hSourceHandle,
    HANDLE hTargetProcessHandle, LPHANDLE lpTargetHandle, DWORD dwDesiredAccess,
    BOOL bInheritHandle, DWORD dwOptions);
static WindowsDllInterceptor::FuncHookType<DuplicateHandle_func>
    stub_DuplicateHandle;

static BOOL WINAPI patched_CloseHandle(HANDLE hObject) {
  // Check all handles being closed against the sandbox's tracked handles.
  base::win::OnHandleBeingClosed(hObject);
  return stub_CloseHandle(hObject);
}

static BOOL WINAPI patched_DuplicateHandle(
    HANDLE hSourceProcessHandle, HANDLE hSourceHandle,
    HANDLE hTargetProcessHandle, LPHANDLE lpTargetHandle, DWORD dwDesiredAccess,
    BOOL bInheritHandle, DWORD dwOptions) {
  // If closing a source handle from our process check it against the sandbox's
  // tracked handles.
  if ((dwOptions & DUPLICATE_CLOSE_SOURCE) &&
      (GetProcessId(hSourceProcessHandle) == ::GetCurrentProcessId())) {
    base::win::OnHandleBeingClosed(hSourceHandle);
  }

  return stub_DuplicateHandle(hSourceProcessHandle, hSourceHandle,
                              hTargetProcessHandle, lpTargetHandle,
                              dwDesiredAccess, bInheritHandle, dwOptions);
}

typedef BOOL(WINAPI* ApiSetQueryApiSetPresence_func)(PCUNICODE_STRING,
                                                     PBOOLEAN);
static WindowsDllInterceptor::FuncHookType<ApiSetQueryApiSetPresence_func>
    stub_ApiSetQueryApiSetPresence;

static const WCHAR gApiSetNtUserWindowStation[] =
    L"ext-ms-win-ntuser-windowstation-l1-1-0";

static BOOL WINAPI patched_ApiSetQueryApiSetPresence(
    PCUNICODE_STRING aNamespace, PBOOLEAN aPresent) {
  if (aNamespace && aPresent &&
      !wcsncmp(aNamespace->Buffer, gApiSetNtUserWindowStation,
               aNamespace->Length / sizeof(WCHAR))) {
    *aPresent = FALSE;
    return TRUE;
  }

  return stub_ApiSetQueryApiSetPresence(aNamespace, aPresent);
}

static WindowsDllInterceptor Kernel32Intercept;
static WindowsDllInterceptor gApiQueryIntercept;

static bool EnableHandleCloseMonitoring() {
  Kernel32Intercept.Init("kernel32.dll");
  bool hooked = stub_CloseHandle.Set(Kernel32Intercept, "CloseHandle",
                                     &patched_CloseHandle);
  if (!hooked) {
    return false;
  }

  hooked = stub_DuplicateHandle.Set(Kernel32Intercept, "DuplicateHandle",
                                    &patched_DuplicateHandle);
  if (!hooked) {
    return false;
  }

  return true;
}

/**
 * There is a bug in COM that causes its initialization to fail when user32.dll
 * is loaded but Win32k lockdown is enabled. COM uses ApiSetQueryApiSetPresence
 * to make this check. When we are under Win32k lockdown, we hook
 * ApiSetQueryApiSetPresence and force it to tell the caller that the DLL of
 * interest is not present.
 */
static void EnableApiQueryInterception() {
  if (!IsWin32kLockedDown()) {
    return;
  }

  gApiQueryIntercept.Init(L"Api-ms-win-core-apiquery-l1-1-0.dll");
  DebugOnly<bool> hookSetOk = stub_ApiSetQueryApiSetPresence.Set(
      gApiQueryIntercept, "ApiSetQueryApiSetPresence",
      &patched_ApiSetQueryApiSetPresence);
  MOZ_ASSERT(hookSetOk);
}

static bool ShouldDisableHandleVerifier() {
#if defined(_X86_) && (defined(EARLY_BETA_OR_EARLIER) || defined(DEBUG))
  // Chromium only has the verifier enabled for 32-bit and our close monitoring
  // hooks cause debug assertions for 64-bit anyway.
  // For x86 keep the verifier enabled by default only for Nightly or debug.
  return false;
#else
  return !getenv("MOZ_ENABLE_HANDLE_VERIFIER");
#endif
}

static void InitializeHandleVerifier() {
  // Disable the handle verifier if we don't want it or can't enable the close
  // monitoring hooks.
  if (ShouldDisableHandleVerifier() || !EnableHandleCloseMonitoring()) {
    base::win::DisableHandleVerifier();
  }
}

static sandbox::TargetServices* InitializeTargetServices() {
  // This might disable the verifier, so we want to do it before it is used.
  InitializeHandleVerifier();

  EnableApiQueryInterception();

  sandbox::TargetServices* targetServices =
      sandbox::SandboxFactory::GetTargetServices();
  if (!targetServices) {
    return nullptr;
  }

  if (targetServices->Init() != sandbox::SBOX_ALL_OK) {
    return nullptr;
  }

  return targetServices;
}

sandbox::TargetServices* GetInitializedTargetServices() {
  static sandbox::TargetServices* sInitializedTargetServices =
      InitializeTargetServices();

  return sInitializedTargetServices;
}

void LowerSandbox() { GetInitializedTargetServices()->LowerToken(); }

static sandbox::BrokerServices* InitializeBrokerServices() {
  // This might disable the verifier, so we want to do it before it is used.
  InitializeHandleVerifier();

  sandbox::BrokerServices* brokerServices =
      sandbox::SandboxFactory::GetBrokerServices();
  if (!brokerServices) {
    return nullptr;
  }

  if (brokerServices->Init() != sandbox::SBOX_ALL_OK) {
    return nullptr;
  }

  // Comment below copied from Chromium code.
  // Precreate the desktop and window station used by the renderers.
  // IMPORTANT: This piece of code needs to run as early as possible in the
  // process because it will initialize the sandbox broker, which requires
  // the process to swap its window station. During this time all the UI
  // will be broken. This has to run before threads and windows are created.
  scoped_refptr<sandbox::TargetPolicy> policy = brokerServices->CreatePolicy();
  policy->CreateAlternateDesktop(true);

  return brokerServices;
}

sandbox::BrokerServices* GetInitializedBrokerServices() {
  static sandbox::BrokerServices* sInitializedBrokerServices =
      InitializeBrokerServices();

  return sInitializedBrokerServices;
}

PermissionsService* GetPermissionsService() {
  return PermissionsService::GetInstance();
}

}  // namespace sandboxing
}  // namespace mozilla
