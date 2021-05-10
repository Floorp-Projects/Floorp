/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/DynamicallyLinkedFunctionPtr.h"
#include "nsWindowsDllInterceptor.h"
#include "nsWindowsHelpers.h"

#include <shlwapi.h>

static int NormalImport() { return ::GetSystemMetrics(SM_CYCAPTION); }

static bool DelayLoadImport() {
  return !!::UrlIsW(L"http://example.com/", URLIS_FILEURL);
}

static mozilla::WindowsIATPatcher::FuncHookType<decltype(&::GetSystemMetrics)>
    gGetSystemMetricsHook;

static mozilla::WindowsIATPatcher::FuncHookType<decltype(&::MessageBoxA)>
    gMessageBoxAHook;

static mozilla::WindowsIATPatcher::FuncHookType<decltype(&::UrlIsW)> gUrlIsHook;

static bool gGetSystemMetricsHookCalled = false;

static int WINAPI GetSystemMetricsHook(int aIndex) {
  MOZ_DIAGNOSTIC_ASSERT(aIndex == SM_CYCAPTION);
  gGetSystemMetricsHookCalled = true;
  return 0;
}

static bool gUrlIsHookCalled = false;

static BOOL WINAPI UrlIsWHook(PCWSTR aUrl, URLIS aFlags) {
  gUrlIsHookCalled = true;
  return TRUE;
}

static HMODULE GetStrongReferenceToExeModule() {
  HMODULE result;
  if (!::GetModuleHandleExW(0, nullptr, &result)) {
    return nullptr;
  }

  return result;
}

#define PRINT_FAIL(msg) printf("TEST-UNEXPECTED-FAIL | IATPatcher | " msg "\n")

extern "C" int wmain(int argc, wchar_t* argv[]) {
  nsModuleHandle ourModule1(GetStrongReferenceToExeModule());
  if (!ourModule1) {
    PRINT_FAIL("Failed obtaining HMODULE for executable");
    return 1;
  }

  if (!gGetSystemMetricsHook.Set(ourModule1, "user32.dll", "GetSystemMetrics",
                                 &GetSystemMetricsHook)) {
    PRINT_FAIL("Failed setting GetSystemMetrics hook");
    return 1;
  }

  if (NormalImport() || !gGetSystemMetricsHookCalled) {
    PRINT_FAIL("GetSystemMetrics hook was not called");
    return 1;
  }

  static const mozilla::StaticDynamicallyLinkedFunctionPtr<
      decltype(&::GetSystemMetrics)>
      pRealGetSystemMetrics(L"user32.dll", "GetSystemMetrics");
  if (!pRealGetSystemMetrics) {
    PRINT_FAIL("Failed resolving real GetSystemMetrics pointer");
    return 1;
  }

  if (gGetSystemMetricsHook.GetStub() != pRealGetSystemMetrics) {
    PRINT_FAIL(
        "GetSystemMetrics hook stub pointer does not match real "
        "GetSystemMetrics pointer");
    return 1;
  }

  nsModuleHandle ourModule2(GetStrongReferenceToExeModule());
  if (!ourModule2) {
    PRINT_FAIL("Failed obtaining HMODULE for executable");
    return 1;
  }

  // This should fail becuase the test never calls, and thus never imports,
  // MessageBoxA
  if (gMessageBoxAHook.Set(ourModule2, "user32.dll", "MessageBoxA", nullptr)) {
    PRINT_FAIL("Setting MessageBoxA hook succeeded when it should have failed");
    return 1;
  }

  nsModuleHandle ourModule3(GetStrongReferenceToExeModule());
  if (!ourModule3) {
    PRINT_FAIL("Failed obtaining HMODULE for executable");
    return 1;
  }

  // These tests involve a delay-loaded import, which are not supported; we
  // expect these tests to FAIL.

  if (gUrlIsHook.Set(ourModule3, "shlwapi.dll", "UrlIsW", &UrlIsWHook)) {
    PRINT_FAIL("gUrlIsHook.Set should have failed");
    return 1;
  }

  if (DelayLoadImport() || gUrlIsHookCalled) {
    PRINT_FAIL("gUrlIsHook should not have been called");
    return 1;
  }

  printf("TEST-PASS | IATPatcher | All tests passed.\n");
  return 0;
}
