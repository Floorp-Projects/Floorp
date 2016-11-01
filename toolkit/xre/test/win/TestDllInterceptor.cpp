/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include "nsWindowsDllInterceptor.h"

using namespace mozilla;

struct payload {
  UINT64 a;
  UINT64 b;
  UINT64 c;

  bool operator==(const payload &other) const {
    return (a == other.a &&
            b == other.b &&
            c == other.c);
  }
};

extern "C" __declspec(dllexport) __declspec(noinline) payload rotatePayload(payload p) {
  UINT64 tmp = p.a;
  p.a = p.b;
  p.b = p.c;
  p.c = tmp;
  return p;
}

static bool patched_func_called = false;

static payload (*orig_rotatePayload)(payload);

static payload
patched_rotatePayload(payload p)
{
  patched_func_called = true;
  return orig_rotatePayload(p);
}

bool TestHook(const char *dll, const char *func)
{
  void *orig_func;
  bool successful = false;
  {
    WindowsDllInterceptor TestIntercept;
    TestIntercept.Init(dll);
    successful = TestIntercept.AddHook(func, 0, &orig_func);
  }

  if (successful) {
    printf("TEST-PASS | WindowsDllInterceptor | Could hook %s from %s\n", func, dll);
    return true;
  } else {
    printf("TEST-UNEXPECTED-FAIL | WindowsDllInterceptor | Failed to hook %s from %s\n", func, dll);
    return false;
  }
}

bool TestDetour(const char *dll, const char *func)
{
  void *orig_func;
  bool successful = false;
  {
    WindowsDllInterceptor TestIntercept;
    TestIntercept.Init(dll);
    successful = TestIntercept.AddDetour(func, 0, &orig_func);
  }

  if (successful) {
    printf("TEST-PASS | WindowsDllInterceptor | Could detour %s from %s\n", func, dll);
    return true;
  } else {
    printf("TEST-UNEXPECTED-FAIL | WindowsDllInterceptor | Failed to detour %s from %s\n", func, dll);
    return false;
  }
}

int main()
{
  payload initial = { 0x12345678, 0xfc4e9d31, 0x87654321 };
  payload p0, p1;
  ZeroMemory(&p0, sizeof(p0));
  ZeroMemory(&p1, sizeof(p1));

  p0 = rotatePayload(initial);

  {
    WindowsDllInterceptor ExeIntercept;
    ExeIntercept.Init("TestDllInterceptor.exe");
    if (ExeIntercept.AddHook("rotatePayload", reinterpret_cast<intptr_t>(patched_rotatePayload), (void**) &orig_rotatePayload)) {
      printf("TEST-PASS | WindowsDllInterceptor | Hook added\n");
    } else {
      printf("TEST-UNEXPECTED-FAIL | WindowsDllInterceptor | Failed to add hook\n");
      return 1;
    }

    p1 = rotatePayload(initial);

    if (patched_func_called) {
      printf("TEST-PASS | WindowsDllInterceptor | Hook called\n");
    } else {
      printf("TEST-UNEXPECTED-FAIL | WindowsDllInterceptor | Hook was not called\n");
      return 1;
    }

    if (p0 == p1) {
      printf("TEST-PASS | WindowsDllInterceptor | Hook works properly\n");
    } else {
      printf("TEST-UNEXPECTED-FAIL | WindowsDllInterceptor | Hook didn't return the right information\n");
      return 1;
    }
  }

  patched_func_called = false;
  ZeroMemory(&p1, sizeof(p1));

  p1 = rotatePayload(initial);

  if (!patched_func_called) {
    printf("TEST-PASS | WindowsDllInterceptor | Hook was not called after unregistration\n");
  } else {
    printf("TEST-UNEXPECTED-FAIL | WindowsDllInterceptor | Hook was still called after unregistration\n");
    return 1;
  }

  if (p0 == p1) {
    printf("TEST-PASS | WindowsDllInterceptor | Original function worked properly\n");
  } else {
    printf("TEST-UNEXPECTED-FAIL | WindowsDllInterceptor | Original function didn't return the right information\n");
    return 1;
  }

  if (TestHook("user32.dll", "GetWindowInfo") &&
#ifdef _WIN64
      TestHook("user32.dll", "SetWindowLongPtrA") &&
      TestHook("user32.dll", "SetWindowLongPtrW") &&
#else
      TestHook("user32.dll", "SetWindowLongA") &&
      TestHook("user32.dll", "SetWindowLongW") &&
#endif
      TestHook("user32.dll", "TrackPopupMenu") &&
#ifdef _M_IX86
      // We keep this test to hook complex code on x86. (Bug 850957)
      TestHook("ntdll.dll", "NtFlushBuffersFile") &&
#endif
      TestHook("ntdll.dll", "NtCreateFile") &&
      TestHook("ntdll.dll", "NtReadFile") &&
      TestHook("ntdll.dll", "NtReadFileScatter") &&
      TestHook("ntdll.dll", "NtWriteFile") &&
      TestHook("ntdll.dll", "NtWriteFileGather") &&
      TestHook("ntdll.dll", "NtQueryFullAttributesFile") &&
      // Bug 733892: toolkit/crashreporter/nsExceptionHandler.cpp
      TestHook("kernel32.dll", "SetUnhandledExceptionFilter") &&
#ifdef _M_IX86
      // Bug 670967: xpcom/base/AvailableMemoryTracker.cpp
      TestHook("kernel32.dll", "VirtualAlloc") &&
      TestHook("kernel32.dll", "MapViewOfFile") &&
      TestHook("gdi32.dll", "CreateDIBSection") &&
      TestHook("kernel32.dll", "CreateFileW") &&
#endif
      TestDetour("user32.dll", "CreateWindowExW") &&
      TestHook("user32.dll", "InSendMessageEx") &&
      TestHook("imm32.dll", "ImmGetContext") &&
      TestHook("imm32.dll", "ImmGetCompositionStringW") &&
      TestHook("imm32.dll", "ImmSetCandidateWindow") &&
#ifdef _M_X64
      TestHook("user32.dll", "GetKeyState") &&
#endif
      TestDetour("ntdll.dll", "LdrLoadDll")) {
    printf("TEST-PASS | WindowsDllInterceptor | all checks passed\n");
    return 0;
  }

  return 1;
}
