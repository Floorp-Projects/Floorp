/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <shlobj.h>
#include <stdio.h>
#include <commdlg.h>

#include "mozilla/WindowsVersion.h"
#include "nsWindowsDllInterceptor.h"
#include "nsWindowsHelpers.h"

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

typedef bool(*HookTestFunc)(void*);
bool CheckHook(HookTestFunc aHookTestFunc, void* aOrigFunc,
               const char* aDllName, const char* aFuncName)
{
  if (aHookTestFunc(aOrigFunc)) {
    printf("TEST-PASS | WindowsDllInterceptor | "
           "Executed hooked function %s from %s\n", aFuncName, aDllName);
    return true;
  }
  printf("TEST-FAILED | WindowsDllInterceptor | "
         "Failed to execute hooked function %s from %s\n", aFuncName, aDllName);
  return false;
}

bool TestHook(HookTestFunc funcTester, const char *dll, const char *func)
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
    return CheckHook(funcTester, orig_func, dll, func);
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

bool MaybeTestHook(const bool cond, HookTestFunc funcTester, const char* dll, const char* func)
{
  if (!cond) {
    printf("TEST-SKIPPED | WindowsDllInterceptor | Skipped hook test for %s from %s\n", func, dll);
    return true;
  }

  return TestHook(funcTester, dll, func);
}

bool ShouldTestTipTsf()
{
  if (!IsWin8OrLater()) {
    return false;
  }

  nsModuleHandle shell32(LoadLibraryW(L"shell32.dll"));
  if (!shell32) {
    return true;
  }

  auto pSHGetKnownFolderPath = reinterpret_cast<decltype(&SHGetKnownFolderPath)>(GetProcAddress(shell32, "SHGetKnownFolderPath"));
  if (!pSHGetKnownFolderPath) {
    return true;
  }

  PWSTR commonFilesPath = nullptr;
  if (FAILED(pSHGetKnownFolderPath(FOLDERID_ProgramFilesCommon, 0, nullptr,
                                   &commonFilesPath))) {
    return true;
  }

  wchar_t fullPath[MAX_PATH + 1] = {};
  wcscpy(fullPath, commonFilesPath);
  wcscat(fullPath, L"\\Microsoft Shared\\Ink\\tiptsf.dll");
  CoTaskMemFree(commonFilesPath);

  if (!LoadLibraryW(fullPath)) {
    return false;
  }

  // Leak the module so that it's loaded for the interceptor test
  return true;
}

// These test the patched function returned by the DLL
// interceptor.  They check that the patched assembler preamble does
// something sane.  The parameter is a pointer to the patched function.
bool TestGetWindowInfo(void* aFunc)
{
  auto patchedGetWindowInfo =
    reinterpret_cast<decltype(&GetWindowInfo)>(aFunc);
  return patchedGetWindowInfo(0, 0) == FALSE;
}

bool TestSetWindowLongPtr(void* aFunc)
{
  auto patchedSetWindowLongPtr =
    reinterpret_cast<decltype(&SetWindowLongPtr)>(aFunc);
  return patchedSetWindowLongPtr(0, 0, 0) == 0;
}

bool TestSetWindowLong(void* aFunc)
{
  auto patchedSetWindowLong =
    reinterpret_cast<decltype(&SetWindowLong)>(aFunc);
  return patchedSetWindowLong(0, 0, 0) == 0;
}

bool TestTrackPopupMenu(void* aFunc)
{
  auto patchedTrackPopupMenu =
    reinterpret_cast<decltype(&TrackPopupMenu)>(aFunc);
  return patchedTrackPopupMenu(0, 0, 0, 0, 0, 0, 0) == 0;
}

bool TestNtFlushBuffersFile(void* aFunc)
{
  typedef NTSTATUS(WINAPI *NtFlushBuffersFileType)(HANDLE, PIO_STATUS_BLOCK);
  auto patchedNtFlushBuffersFile =
    reinterpret_cast<NtFlushBuffersFileType>(aFunc);
  patchedNtFlushBuffersFile(0, 0);
  return true;
}

bool TestNtCreateFile(void* aFunc)
{
  auto patchedNtCreateFile =
    reinterpret_cast<decltype(&NtCreateFile)>(aFunc);
  return patchedNtCreateFile(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) != 0;
}

bool TestNtReadFile(void* aFunc)
{
  typedef NTSTATUS(WINAPI *NtReadFileType)(HANDLE, HANDLE, PIO_APC_ROUTINE, PVOID,
                                           PIO_STATUS_BLOCK, PVOID, ULONG,
                                           PLARGE_INTEGER, PULONG);
  auto patchedNtReadFile =
    reinterpret_cast<NtReadFileType>(aFunc);
  return patchedNtReadFile(0, 0, 0, 0, 0, 0, 0, 0, 0) != 0;
}

bool TestNtReadFileScatter(void* aFunc)
{
  typedef NTSTATUS(WINAPI *NtReadFileScatterType)(HANDLE, HANDLE, PIO_APC_ROUTINE, PVOID,
                                                  PIO_STATUS_BLOCK, PFILE_SEGMENT_ELEMENT, ULONG,
                                                  PLARGE_INTEGER, PULONG);
  auto patchedNtReadFileScatter =
    reinterpret_cast<NtReadFileScatterType>(aFunc);
  return patchedNtReadFileScatter(0, 0, 0, 0, 0, 0, 0, 0, 0) != 0;
}

bool TestNtWriteFile(void* aFunc)
{
  typedef NTSTATUS(WINAPI *NtWriteFileType)(HANDLE, HANDLE, PIO_APC_ROUTINE, PVOID,
                                            PIO_STATUS_BLOCK, PVOID, ULONG,
                                            PLARGE_INTEGER, PULONG);
  auto patchedNtWriteFile =
    reinterpret_cast<NtWriteFileType>(aFunc);
  return patchedNtWriteFile(0, 0, 0, 0, 0, 0, 0, 0, 0) != 0;
}

bool TestNtWriteFileGather(void* aFunc)
{
  typedef NTSTATUS(WINAPI *NtWriteFileGatherType)(HANDLE, HANDLE, PIO_APC_ROUTINE, PVOID,
                                                  PIO_STATUS_BLOCK, PFILE_SEGMENT_ELEMENT, ULONG,
                                                  PLARGE_INTEGER, PULONG);
  auto patchedNtWriteFileGather =
    reinterpret_cast<NtWriteFileGatherType>(aFunc);
  return patchedNtWriteFileGather(0, 0, 0, 0, 0, 0, 0, 0, 0) != 0;
}

bool TestNtQueryFullAttributesFile(void* aFunc)
{
  typedef NTSTATUS(WINAPI *NtQueryFullAttributesFileType)(POBJECT_ATTRIBUTES,
                                                          PVOID);
  auto patchedNtQueryFullAttributesFile =
    reinterpret_cast<NtQueryFullAttributesFileType>(aFunc);
  return patchedNtQueryFullAttributesFile(0, 0) != 0;
}

bool TestLdrUnloadDll(void* aFunc)
{
  typedef NTSTATUS (NTAPI *LdrUnloadDllType)(HMODULE);
  auto patchedLdrUnloadDll = reinterpret_cast<LdrUnloadDllType>(aFunc);
  return patchedLdrUnloadDll(0) != 0;
}

bool TestLdrResolveDelayLoadedAPI(void* aFunc)
{
  // These pointers are disguised as PVOID to avoid pulling in obscure headers
  typedef PVOID (WINAPI *LdrResolveDelayLoadedAPIType)(PVOID, PVOID, PVOID,
                                                       PVOID, PVOID, ULONG);
  auto patchedLdrResolveDelayLoadedAPI =
    reinterpret_cast<LdrResolveDelayLoadedAPIType>(aFunc);
  // No idea how to call this API. Flags==99 is just an arbitrary number that
  // doesn't crash when the other params are null.
  return patchedLdrResolveDelayLoadedAPI(0, 0, 0, 0, 0, 99) == 0;
}

#ifdef _M_AMD64
bool TestRtlInstallFunctionTableCallback(void* aFunc)
{
  auto patchedRtlInstallFunctionTableCallback =
    reinterpret_cast<decltype(RtlInstallFunctionTableCallback)*>(aFunc);

  return patchedRtlInstallFunctionTableCallback(0, 0, 0, 0, 0, 0) == FALSE;
}
#endif

bool TestSetUnhandledExceptionFilter(void* aFunc)
{
  auto patchedSetUnhandledExceptionFilter =
    reinterpret_cast<decltype(&SetUnhandledExceptionFilter)>(aFunc);
  // Retrieve the current filter as we set the new filter to null, then restore the current filter.
  LPTOP_LEVEL_EXCEPTION_FILTER current = patchedSetUnhandledExceptionFilter(0);
  patchedSetUnhandledExceptionFilter(current);
  return true;
}

bool TestVirtualAlloc(void* aFunc)
{
  auto patchedVirtualAlloc =
    reinterpret_cast<decltype(&VirtualAlloc)>(aFunc);
  return patchedVirtualAlloc(0, 0, 0, 0) == 0;
}

bool TestMapViewOfFile(void* aFunc)
{
  auto patchedMapViewOfFile =
    reinterpret_cast<decltype(&MapViewOfFile)>(aFunc);
  return patchedMapViewOfFile(0, 0, 0, 0, 0) == 0;
}

bool TestCreateDIBSection(void* aFunc)
{
  auto patchedCreateDIBSection =
    reinterpret_cast<decltype(&CreateDIBSection)>(aFunc);
  // MSDN is wrong here.  This does not return ERROR_INVALID_PARAMETER.  It
  // sets the value of GetLastError to ERROR_INVALID_PARAMETER.
  // CreateDIBSection returns 0 on error.
  return patchedCreateDIBSection(0, 0, 0, 0, 0, 0) == 0;
}

bool TestCreateFileW(void* aFunc)
{
  auto patchedCreateFileW =
    reinterpret_cast<decltype(&CreateFileW)>(aFunc);
  return patchedCreateFileW(0, 0, 0, 0, 0, 0, 0) == INVALID_HANDLE_VALUE;
}

bool TestCreateFileA(void* aFunc)
{
  auto patchedCreateFileA =
    reinterpret_cast<decltype(&CreateFileA)>(aFunc);
//  return patchedCreateFileA(0, 0, 0, 0, 0, 0, 0) == INVALID_HANDLE_VALUE;
  printf("TEST-SKIPPED | WindowsDllInterceptor | "
          "Will not attempt to execute patched CreateFileA -- patched method is known to fail.\n");
  return true;
}

bool TestInSendMessageEx(void* aFunc)
{
  auto patchedInSendMessageEx =
    reinterpret_cast<decltype(&InSendMessageEx)>(aFunc);
  patchedInSendMessageEx(0);
  return true;
}

bool TestImmGetContext(void* aFunc)
{
  auto patchedImmGetContext =
    reinterpret_cast<decltype(&ImmGetContext)>(aFunc);
  patchedImmGetContext(0);
  return true;
}

bool TestImmGetCompositionStringW(void* aFunc)
{
  auto patchedImmGetCompositionStringW =
    reinterpret_cast<decltype(&ImmGetCompositionStringW)>(aFunc);
  patchedImmGetCompositionStringW(0, 0, 0, 0);
  return true;
}

bool TestImmSetCandidateWindow(void* aFunc)
{
  auto patchedImmSetCandidateWindow =
    reinterpret_cast<decltype(&ImmSetCandidateWindow)>(aFunc);
//  return patchedImmSetCandidateWindow(0, 0) == 0;
  // ImmSetCandidateWindow crashes if given bad parameters.
  printf("TEST-SKIPPED | WindowsDllInterceptor | "
          "Will not attempt to execute patched ImmSetCandidateWindow.\n");
  return true;
}

bool TestImmNotifyIME(void* aFunc)
{
  auto patchedImmNotifyIME =
    reinterpret_cast<decltype(&ImmNotifyIME)>(aFunc);
  return patchedImmNotifyIME(0, 0, 0, 0) == 0;
}

bool TestGetSaveFileNameW(void* aFunc)
{
  auto patchedGetSaveFileNameWType =
    reinterpret_cast<decltype(&GetSaveFileNameW)>(aFunc);
  patchedGetSaveFileNameWType(0);
  return true;
}

bool TestGetOpenFileNameW(void* aFunc)
{
  auto patchedGetOpenFileNameWType =
    reinterpret_cast<decltype(&GetOpenFileNameW)>(aFunc);
  patchedGetOpenFileNameWType(0);
  return true;
}

bool TestGetKeyState(void* aFunc)
{
  auto patchedGetKeyState =
    reinterpret_cast<decltype(&GetKeyState)>(aFunc);
  patchedGetKeyState(0);
  return true;
}

bool TestSendMessageTimeoutW(void* aFunc)
{
  auto patchedSendMessageTimeoutW =
    reinterpret_cast<decltype(&SendMessageTimeoutW)>(aFunc);
  return patchedSendMessageTimeoutW(0, 0, 0, 0, 0, 0, 0) == 0;
}

bool TestProcessCaretEvents(void* aFunc)
{
  auto patchedProcessCaretEvents =
    reinterpret_cast<WINEVENTPROC>(aFunc);
  patchedProcessCaretEvents(0, 0, 0, 0, 0, 0, 0);
  return true;
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

  if (TestHook(TestGetWindowInfo, "user32.dll", "GetWindowInfo") &&
#ifdef _WIN64
      TestHook(TestSetWindowLongPtr, "user32.dll", "SetWindowLongPtrA") &&
      TestHook(TestSetWindowLongPtr, "user32.dll", "SetWindowLongPtrW") &&
#else
      TestHook(TestSetWindowLong, "user32.dll", "SetWindowLongA") &&
      TestHook(TestSetWindowLong, "user32.dll", "SetWindowLongW") &&
#endif
      TestHook(TestTrackPopupMenu, "user32.dll", "TrackPopupMenu") &&
#ifdef _M_IX86
      // We keep this test to hook complex code on x86. (Bug 850957)
      TestHook(TestNtFlushBuffersFile, "ntdll.dll", "NtFlushBuffersFile") &&
#endif
      TestHook(TestNtCreateFile, "ntdll.dll", "NtCreateFile") &&
      TestHook(TestNtReadFile, "ntdll.dll", "NtReadFile") &&
      TestHook(TestNtReadFileScatter, "ntdll.dll", "NtReadFileScatter") &&
      TestHook(TestNtWriteFile, "ntdll.dll", "NtWriteFile") &&
      TestHook(TestNtWriteFileGather, "ntdll.dll", "NtWriteFileGather") &&
      TestHook(TestNtQueryFullAttributesFile, "ntdll.dll", "NtQueryFullAttributesFile") &&
      // Bug 733892: toolkit/crashreporter/nsExceptionHandler.cpp
      TestHook(TestSetUnhandledExceptionFilter, "kernel32.dll", "SetUnhandledExceptionFilter") &&
#ifdef _M_IX86
      // Bug 670967: xpcom/base/AvailableMemoryTracker.cpp
      TestHook(TestVirtualAlloc, "kernel32.dll", "VirtualAlloc") &&
      TestHook(TestMapViewOfFile, "kernel32.dll", "MapViewOfFile") &&
      TestHook(TestCreateDIBSection, "gdi32.dll", "CreateDIBSection") &&
      TestHook(TestCreateFileW, "kernel32.dll", "CreateFileW") &&    // see Bug 1316415
#endif
      TestHook(TestCreateFileA, "kernel32.dll", "CreateFileA") &&
      TestDetour("user32.dll", "CreateWindowExW") &&
      TestHook(TestInSendMessageEx, "user32.dll", "InSendMessageEx") &&
      TestHook(TestImmGetContext, "imm32.dll", "ImmGetContext") &&
      // TestHook("imm32.dll", "ImmReleaseContext") &&    // see Bug 1316415
      TestHook(TestImmGetCompositionStringW, "imm32.dll", "ImmGetCompositionStringW") &&
      TestHook(TestImmSetCandidateWindow, "imm32.dll", "ImmSetCandidateWindow") &&
      TestHook(TestImmNotifyIME, "imm32.dll", "ImmNotifyIME") &&
      TestHook(TestGetSaveFileNameW, "comdlg32.dll", "GetSaveFileNameW") &&
      TestHook(TestGetOpenFileNameW, "comdlg32.dll", "GetOpenFileNameW") &&
#ifdef _M_X64
      TestHook(TestGetKeyState, "user32.dll", "GetKeyState") &&    // see Bug 1316415
      TestHook(TestLdrUnloadDll, "ntdll.dll", "LdrUnloadDll") &&
      MaybeTestHook(IsWin8OrLater(), TestLdrResolveDelayLoadedAPI, "ntdll.dll", "LdrResolveDelayLoadedAPI") &&
      MaybeTestHook(!IsWin8OrLater(), TestRtlInstallFunctionTableCallback, "kernel32.dll", "RtlInstallFunctionTableCallback") &&
#endif
      MaybeTestHook(ShouldTestTipTsf(), TestProcessCaretEvents, "tiptsf.dll", "ProcessCaretEvents") &&
#ifdef _M_IX86
      TestHook(TestSendMessageTimeoutW, "user32.dll", "SendMessageTimeoutW") &&
#endif
      TestDetour("ntdll.dll", "LdrLoadDll")) {
    printf("TEST-PASS | WindowsDllInterceptor | all checks passed\n");
    return 0;
  }

  return 1;
}
