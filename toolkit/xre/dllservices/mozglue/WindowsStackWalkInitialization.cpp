/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/WindowsStackWalkInitialization.h"

#include "nsWindowsDllInterceptor.h"
#include "mozilla/NativeNt.h"
#include "mozilla/StackWalk_windows.h"

namespace mozilla {

#if defined(_M_AMD64) || defined(_M_ARM64)
static WindowsDllInterceptor NtDllIntercept;

typedef NTSTATUS(NTAPI* LdrUnloadDll_func)(HMODULE module);
static WindowsDllInterceptor::FuncHookType<LdrUnloadDll_func> stub_LdrUnloadDll;

static NTSTATUS NTAPI patched_LdrUnloadDll(HMODULE module) {
  // Prevent the stack walker from suspending this thread when LdrUnloadDll
  // holds the RtlLookupFunctionEntry lock.
  AutoSuppressStackWalking suppress;
  return stub_LdrUnloadDll(module);
}

// These pointers are disguised as PVOID to avoid pulling in obscure headers
typedef PVOID(WINAPI* LdrResolveDelayLoadedAPI_func)(
    PVOID ParentModuleBase, PVOID DelayloadDescriptor, PVOID FailureDllHook,
    PVOID FailureSystemHook, PVOID ThunkAddress, ULONG Flags);
static WindowsDllInterceptor::FuncHookType<LdrResolveDelayLoadedAPI_func>
    stub_LdrResolveDelayLoadedAPI;

static PVOID WINAPI patched_LdrResolveDelayLoadedAPI(
    PVOID ParentModuleBase, PVOID DelayloadDescriptor, PVOID FailureDllHook,
    PVOID FailureSystemHook, PVOID ThunkAddress, ULONG Flags) {
  // Prevent the stack walker from suspending this thread when
  // LdrResolveDelayLoadAPI holds the RtlLookupFunctionEntry lock.
  AutoSuppressStackWalking suppress;
  return stub_LdrResolveDelayLoadedAPI(ParentModuleBase, DelayloadDescriptor,
                                       FailureDllHook, FailureSystemHook,
                                       ThunkAddress, Flags);
}

void WindowsStackWalkInitialization() {
  // This function could be called by both profilers, but we only want to run
  // it once.
  static bool ran = false;
  if (ran) {
    return;
  }
  ran = true;

  NtDllIntercept.Init("ntdll.dll");
  stub_LdrUnloadDll.Set(NtDllIntercept, "LdrUnloadDll", &patched_LdrUnloadDll);
  stub_LdrResolveDelayLoadedAPI.Set(NtDllIntercept, "LdrResolveDelayLoadedAPI",
                                    &patched_LdrResolveDelayLoadedAPI);
}
#endif  // _M_AMD64 || _M_ARM64

}  // namespace mozilla
