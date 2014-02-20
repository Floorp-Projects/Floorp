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

__declspec(noinline) bool AlwaysTrue(int, int, int, int, int, int) {
  // Dummy function that makes the caller recognizable by the detour patcher
  return true;
}

extern "C" __declspec(dllexport) __declspec(noinline) uint32_t SetBits(uint32_t x)
{
  if (AlwaysTrue(1, 2, 3, 4, 5, 6)) {
    return x | 0x11;
  }
  return 0;
}

static uint32_t (*orig_SetBits_early)(uint32_t);
static uint32_t patched_SetBits_early(uint32_t x)
{
  return orig_SetBits_early(x) | 0x2200;
}

static uint32_t (*orig_SetBits_late)(uint32_t);
static uint32_t patched_SetBits_late(uint32_t x)
{
  return orig_SetBits_late(x) | 0x330000;
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

bool TestSharedHook(const char *dll, const char *func)
{
  void *orig_func;
  bool successful = false;
  {
    WindowsDllInterceptor TestIntercept;
    TestIntercept.Init(dll);
    successful = TestIntercept.AddSharedHook(func, 0, &orig_func);
  }

  if (successful) {
    printf("TEST-PASS | WindowsDllInterceptor | Could hook (shared) %s from %s\n", func, dll);
    return true;
  } else {
    printf("TEST-UNEXPECTED-FAIL | WindowsDllInterceptor | Failed to hook (shared) %s from %s\n", func, dll);
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

#ifdef _M_IX86
  // The x64 detour patcher does understand the assembly code of SetBits.
  // We only need these shared hook tests on x86 anyway, because shared hooks
  // are the same as regular hooks on x64.

  // Despite the noinline annotation, the compiler may try to re-use the
  // return value of SetBits(0). Force it to call the function every time.
  uint32_t (*volatile SetBitsVolatile)(uint32_t) = SetBits;

  {
    WindowsDllInterceptor ExeInterceptEarly;
    ExeInterceptEarly.Init("TestDllInterceptor.exe");
    if (ExeInterceptEarly.AddSharedHook("SetBits", reinterpret_cast<intptr_t>(patched_SetBits_early), (void**) &orig_SetBits_early)) {
      printf("TEST-PASS | WindowsDllInterceptor | Early hook added\n");
    } else {
      printf("TEST-UNEXPECTED-FAIL | WindowsDllInterceptor | Failed to add early hook\n");
      return 1;
    }

    if (SetBitsVolatile(0) == 0x2211) {
      printf("TEST-PASS | WindowsDllInterceptor | Early hook was called\n");
    } else {
      printf("TEST-UNEXPECTED-FAIL | WindowsDllInterceptor | Early hook was not called\n");
      return 1;
    }

    {
      WindowsDllInterceptor ExeInterceptLate;
      ExeInterceptLate.Init("TestDllInterceptor.exe");
      if (ExeInterceptLate.AddHook("SetBits", reinterpret_cast<intptr_t>(patched_SetBits_late), (void**) &orig_SetBits_late)) {
        printf("TEST-PASS | WindowsDllInterceptor | Late hook added\n");
      } else {
        printf("TEST-UNEXPECTED-FAIL | WindowsDllInterceptor | Failed to add late hook\n");
        return 1;
      }

      if (SetBitsVolatile(0) == 0x332211) {
        printf("TEST-PASS | WindowsDllInterceptor | Late hook was called\n");
      } else {
        printf("TEST-UNEXPECTED-FAIL | WindowsDllInterceptor | Late hook was not called\n");
        return 1;
      }
    }

    if (SetBitsVolatile(0) == 0x2211) {
      printf("TEST-PASS | WindowsDllInterceptor | Late hook was unregistered\n");
    } else {
      printf("TEST-UNEXPECTED-FAIL | WindowsDllInterceptor | Late hook was not unregistered\n");
      return 1;
    }
  }

  if (SetBitsVolatile(0) == 0x11) {
    printf("TEST-PASS | WindowsDllInterceptor | Early hook was unregistered\n");
  } else {
    printf("TEST-UNEXPECTED-FAIL | WindowsDllInterceptor | Early hook was not unregistered\n");
    return 1;
  }
#endif

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
#endif
#ifdef _M_IX86 // Shared hooks are the same as regular hooks on x64
      TestSharedHook("ntdll.dll", "LdrLoadDll") &&
#endif
      TestHook("ntdll.dll", "LdrLoadDll")) {
    printf("TEST-PASS | WindowsDllInterceptor | all checks passed\n");
    return 0;
  }

  return 1;
}
