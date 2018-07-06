/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <shlobj.h>
#include <stdio.h>
#include <commdlg.h>
#define SECURITY_WIN32
#include <security.h>
#include <wininet.h>
#include <schnlsp.h>

#include "mozilla/TypeTraits.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WindowsVersion.h"
#include "nsWindowsDllInterceptor.h"
#include "nsWindowsHelpers.h"

NTSTATUS NTAPI NtFlushBuffersFile(HANDLE, PIO_STATUS_BLOCK);
NTSTATUS NTAPI NtReadFile(HANDLE, HANDLE, PIO_APC_ROUTINE, PVOID,
                          PIO_STATUS_BLOCK, PVOID, ULONG,
                          PLARGE_INTEGER, PULONG);
NTSTATUS NTAPI NtReadFileScatter(HANDLE, HANDLE, PIO_APC_ROUTINE, PVOID,
                                 PIO_STATUS_BLOCK, PFILE_SEGMENT_ELEMENT, ULONG,
                                 PLARGE_INTEGER, PULONG);
NTSTATUS NTAPI NtWriteFile(HANDLE, HANDLE, PIO_APC_ROUTINE, PVOID,
                           PIO_STATUS_BLOCK, PVOID, ULONG,
                           PLARGE_INTEGER, PULONG);
NTSTATUS NTAPI NtWriteFileGather(HANDLE, HANDLE, PIO_APC_ROUTINE, PVOID,
                                 PIO_STATUS_BLOCK, PFILE_SEGMENT_ELEMENT, ULONG,
                                 PLARGE_INTEGER, PULONG);
NTSTATUS NTAPI NtQueryFullAttributesFile(POBJECT_ATTRIBUTES, PVOID);
NTSTATUS NTAPI LdrLoadDll(PWCHAR filePath, PULONG flags, PUNICODE_STRING moduleFileName, PHANDLE handle);
NTSTATUS NTAPI LdrUnloadDll(HMODULE);
// These pointers are disguised as PVOID to avoid pulling in obscure headers
PVOID NTAPI LdrResolveDelayLoadedAPI(PVOID, PVOID, PVOID, PVOID, PVOID, ULONG);
void CALLBACK ProcessCaretEvents(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD);
void __fastcall BaseThreadInitThunk(BOOL aIsInitialThread, void* aStartAddress, void* aThreadParam);

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

static WindowsDllInterceptor::FuncHookType<decltype(&rotatePayload)>
  orig_rotatePayload;

static payload
patched_rotatePayload(payload p)
{
  patched_func_called = true;
  return orig_rotatePayload(p);
}

// Invoke aFunc by taking aArg's contents and using them as aFunc's arguments
template <typename CallableT, typename... Args, typename ArgTuple = Tuple<Args...>, size_t... Indices>
decltype(auto) Apply(CallableT&& aFunc, ArgTuple&& aArgs, std::index_sequence<Indices...>)
{
  return std::forward<CallableT>(aFunc)(Get<Indices>(std::forward<ArgTuple>(aArgs))...);
}

template <typename CallableT>
bool TestFunction(CallableT aFunc);

#define DEFINE_TEST_FUNCTION(calling_convention) \
  template <typename R, typename... Args, typename... TestArgs> \
  bool TestFunction(WindowsDllInterceptor::FuncHookType<R (calling_convention *)(Args...)>&& aFunc, \
                    bool (* aPred)(R), TestArgs... aArgs) \
  { \
    using FuncHookType = WindowsDllInterceptor::FuncHookType<R (calling_convention *)(Args...)>; \
    using ArgTuple = Tuple<Args...>; \
    using Indices = std::index_sequence_for<Args...>; \
    ArgTuple fakeArgs{ std::forward<TestArgs>(aArgs)... }; \
    return aPred(Apply(std::forward<FuncHookType>(aFunc), std::forward<ArgTuple>(fakeArgs), Indices())); \
  } \
  \
  /* Specialization for functions returning void */ \
  template <typename... Args, typename PredicateT, typename... TestArgs> \
  bool TestFunction(WindowsDllInterceptor::FuncHookType<void (calling_convention *)(Args...)>&& aFunc, \
                    PredicateT&& aPred, TestArgs... aArgs) \
  { \
    using FuncHookType = WindowsDllInterceptor::FuncHookType<void (calling_convention *)(Args...)>; \
    using ArgTuple = Tuple<Args...>; \
    using Indices = std::index_sequence_for<Args...>; \
    ArgTuple fakeArgs{ std::forward<TestArgs>(aArgs)... }; \
    Apply(std::forward<FuncHookType>(aFunc), std::forward<ArgTuple>(fakeArgs), Indices()); \
    return true; \
  }

// C++11 allows empty arguments to macros. clang works just fine. MSVC does the
// right thing, but it also throws up warning C4003.
#if defined(_MSC_VER) && !defined(__clang__)
DEFINE_TEST_FUNCTION(__cdecl)
#else
DEFINE_TEST_FUNCTION()
#endif

#ifdef _M_IX86
DEFINE_TEST_FUNCTION(__stdcall)
DEFINE_TEST_FUNCTION(__fastcall)
#endif // _M_IX86

// Test the hooked function against the supplied predicate
template <typename OrigFuncT, typename PredicateT, typename... Args>
bool CheckHook(WindowsDllInterceptor::FuncHookType<OrigFuncT> &aOrigFunc,
               const char* aDllName, const char* aFuncName, PredicateT&& aPred,
               Args... aArgs)
{
  if (TestFunction(std::forward<WindowsDllInterceptor::FuncHookType<OrigFuncT>>(aOrigFunc), std::forward<PredicateT>(aPred), std::forward<Args>(aArgs)...)) {
    printf("TEST-PASS | WindowsDllInterceptor | "
           "Executed hooked function %s from %s\n", aFuncName, aDllName);
    return true;
  }
  printf("TEST-FAILED | WindowsDllInterceptor | "
         "Failed to execute hooked function %s from %s\n", aFuncName, aDllName);
  return false;
}

// Hook the function and optionally attempt calling it
template <typename OrigFuncT, size_t N, typename PredicateT, typename... Args>
bool TestHook(const char (&dll)[N], const char *func, PredicateT&& aPred, Args... aArgs)
{
  auto orig_func(mozilla::MakeUnique<WindowsDllInterceptor::FuncHookType<OrigFuncT>>());

  bool successful = false;
  {
    WindowsDllInterceptor TestIntercept;
    TestIntercept.Init(dll);
    successful = orig_func->Set(TestIntercept, func, nullptr);
  }

  if (successful) {
    printf("TEST-PASS | WindowsDllInterceptor | Could hook %s from %s\n", func, dll);
    if (!aPred) {
      printf("TEST-SKIPPED | WindowsDllInterceptor | "
             "Will not attempt to execute patched %s.\n", func);
      return true;
    }

    return CheckHook(*orig_func, dll, func, std::forward<PredicateT>(aPred), std::forward<Args>(aArgs)...);
  } else {
    printf("TEST-UNEXPECTED-FAIL | WindowsDllInterceptor | Failed to hook %s from %s\n", func, dll);
    return false;
  }
}

// Detour the function and optionally attempt calling it
template <typename OrigFuncT, size_t N, typename PredicateT>
bool TestDetour(const char (&dll)[N], const char *func, PredicateT&& aPred)
{
  auto orig_func(mozilla::MakeUnique<WindowsDllInterceptor::FuncHookType<OrigFuncT>>());

  bool successful = false;
  {
    WindowsDllInterceptor TestIntercept;
    TestIntercept.Init(dll);
    successful = orig_func->SetDetour(TestIntercept, func, nullptr);
  }

  if (successful) {
    printf("TEST-PASS | WindowsDllInterceptor | Could detour %s from %s\n", func, dll);
    if (!aPred) {
      printf("TEST-SKIPPED | WindowsDllInterceptor | "
             "Will not attempt to execute patched %s.\n", func);
      return true;
    }

    return CheckHook(*orig_func, dll, func, std::forward<PredicateT>(aPred));
  } else {
    printf("TEST-UNEXPECTED-FAIL | WindowsDllInterceptor | Failed to detour %s from %s\n", func, dll);
    return false;
  }
}

// If a function pointer's type returns void*, this template converts that type
// to return uintptr_t instead, for the purposes of predicates.
template <typename FuncT>
struct SubstituteForVoidPtr
{
  using Type = FuncT;
};

template <typename... Args>
struct SubstituteForVoidPtr<void* (*)(Args...)>
{
  using Type = uintptr_t (*)(Args...);
};

#ifdef _M_IX86
template <typename... Args>
struct SubstituteForVoidPtr<void* (__stdcall*)(Args...)>
{
  using Type = uintptr_t (__stdcall*)(Args...);
};

template <typename... Args>
struct SubstituteForVoidPtr<void* (__fastcall*)(Args...)>
{
  using Type = uintptr_t (__fastcall*)(Args...);
};
#endif // _M_IX86

// Determines the function's return type
template <typename FuncT>
struct ReturnType;

template <typename R, typename... Args>
struct ReturnType<R (*)(Args...)>
{
  using Type = R;
};

#ifdef _M_IX86
template <typename R, typename... Args>
struct ReturnType<R (__stdcall*)(Args...)>
{
  using Type = R;
};

template <typename R, typename... Args>
struct ReturnType<R (__fastcall*)(Args...)>
{
  using Type = R;
};
#endif // _M_IX86

// Predicates that may be supplied during tests
template <typename FuncT>
struct Predicates
{
  using ArgType = typename ReturnType<FuncT>::Type;

  template <ArgType CompVal>
  static bool Equals(ArgType aValue)
  {
    return CompVal == aValue;
  }

  template <ArgType CompVal>
  static bool NotEquals(ArgType aValue)
  {
    return CompVal != aValue;
  }

  template <ArgType CompVal>
  static bool Ignore(ArgType aValue)
  {
    return true;
  }
};

// Functions that return void should be ignored, so we specialize the
// Ignore predicate for that case. Use nullptr as the value to compare against.
template <typename... Args>
struct Predicates<void (*)(Args...)>
{
  template <nullptr_t DummyVal>
  static bool Ignore()
  {
    return true;
  }
};

#ifdef _M_IX86
template <typename... Args>
struct Predicates<void (__stdcall*)(Args...)>
{
  template <nullptr_t DummyVal>
  static bool Ignore()
  {
    return true;
  }
};

template <typename... Args>
struct Predicates<void (__fastcall*)(Args...)>
{
  template <nullptr_t DummyVal>
  static bool Ignore()
  {
    return true;
  }
};
#endif // _M_IX86

// The standard test. Hook |func|, and then try executing it with all zero
// arguments, using |pred| and |comp| to determine whether the call successfully
// executed. In general, you want set pred and comp such that they return true
// when the function is returning whatever value is expected with all-zero
// arguments.
//
// Note: When |func| returns void, you must supply |Ignore| and |nullptr| as the
// |pred| and |comp| arguments, respectively.
#define TEST_HOOK(dll, func, pred, comp) \
  TestHook<decltype(&func)>(#dll, #func, &Predicates<decltype(&func)>::pred<comp>)

// We need to special-case functions that return INVALID_HANDLE_VALUE
// (ie, CreateFile). Our template machinery for comparing values doesn't work
// with integer constants passed as pointers (well, it works on MSVC, but not
// clang, because that is not standard-compliant).
#define TEST_HOOK_FOR_INVALID_HANDLE_VALUE(dll, func) \
  TestHook<SubstituteForVoidPtr<decltype(&func)>::Type>(#dll, #func, &Predicates<SubstituteForVoidPtr<decltype(&func)>::Type>::Equals<uintptr_t(-1)>)

// This variant allows you to explicitly supply arguments to the hooked function
// during testing. You want to provide arguments that produce the conditions that
// induce the function to return a value that is accepted by your predicate.
#define TEST_HOOK_PARAMS(dll, func, pred, comp, ...) \
  TestHook<decltype(&func)>(#dll, #func, &Predicates<decltype(&func)>::pred<comp>, __VA_ARGS__)

// This is for cases when we want to hook |func|, but it is unsafe to attempt
// to execute the function in the context of a test.
#define TEST_HOOK_SKIP_EXEC(dll, func) \
  TestHook<decltype(&func)>(#dll, #func, reinterpret_cast<bool (*)(typename ReturnType<decltype(&func)>::Type)>(NULL))

// The following three variants are identical to the previous macros,
// however the forcibly use a Detour on 32-bit Windows. On 64-bit Windows,
// these macros are identical to their TEST_HOOK variants.
#define TEST_DETOUR(dll, func, pred, comp) \
  TestDetour<decltype(&func)>(#dll, #func, &Predicates<decltype(&func)>::pred<comp>)

#define TEST_DETOUR_PARAMS(dll, func, pred, comp, ...) \
  TestDetour<decltype(&func)>(#dll, #func, &Predicates<decltype(&func)>::pred<comp>, __VA_ARGS__)

#define TEST_DETOUR_SKIP_EXEC(dll, func) \
  TestDetour<decltype(&func)>(#dll, #func, reinterpret_cast<bool (*)(typename ReturnType<decltype(&func)>::Type)>(NULL))

template <typename OrigFuncT, size_t N, typename PredicateT, typename... Args>
bool MaybeTestHook(const bool cond, const char (&dll)[N], const char *func, PredicateT&& aPred, Args... aArgs)
{
  if (!cond) {
    printf("TEST-SKIPPED | WindowsDllInterceptor | Skipped hook test for %s from %s\n", func, dll);
    return true;
  }

  return TestHook<OrigFuncT>(dll, func, std::forward<PredicateT>(aPred), std::forward<Args>(aArgs)...);
}

// Like TEST_HOOK, but the test is only executed when cond is true.
#define MAYBE_TEST_HOOK(cond, dll, func, pred, comp) \
  MaybeTestHook<decltype(&func)>(cond, #dll, #func, &Predicates<decltype(&func)>::pred<comp>)

#define MAYBE_TEST_HOOK_PARAMS(cond, dll, func, pred, comp, ...) \
  MaybeTestHook<decltype(&func)>(cond, #dll, #func, &Predicates<decltype(&func)>::pred<comp>, __VA_ARGS__)

#define MAYBE_TEST_HOOK_SKIP_EXEC(cond, dll, func) \
  MaybeTestHook<decltype(&func)>(cond, #dll, #func, reinterpret_cast<bool (*)(typename ReturnType<decltype(&func)>::Type)>(NULL))

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

int main()
{
  LARGE_INTEGER start;
  QueryPerformanceCounter(&start);

  // We disable this part of the test because the code coverage instrumentation
  // injects code in rotatePayload in a way that WindowsDllInterceptor doesn't
  // understand.
#ifndef MOZ_CODE_COVERAGE
  payload initial = { 0x12345678, 0xfc4e9d31, 0x87654321 };
  payload p0, p1;
  ZeroMemory(&p0, sizeof(p0));
  ZeroMemory(&p1, sizeof(p1));

  p0 = rotatePayload(initial);

  {
    WindowsDllInterceptor ExeIntercept;
    ExeIntercept.Init("TestDllInterceptor.exe");
    if (orig_rotatePayload.Set(ExeIntercept, "rotatePayload", &patched_rotatePayload)) {
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
#endif

  if (TEST_HOOK(user32.dll, GetWindowInfo, Equals, FALSE) &&
#ifdef _WIN64
      TEST_HOOK(user32.dll, SetWindowLongPtrA, Equals, 0) &&
      TEST_HOOK(user32.dll, SetWindowLongPtrW, Equals, 0) &&
#else
      TEST_HOOK(user32.dll, SetWindowLongA, Equals, 0) &&
      TEST_HOOK(user32.dll, SetWindowLongW, Equals, 0) &&
#endif
      TEST_HOOK(user32.dll, TrackPopupMenu, Equals, FALSE) &&
#ifdef _M_IX86
      // We keep this test to hook complex code on x86. (Bug 850957)
      TEST_HOOK(ntdll.dll, NtFlushBuffersFile, NotEquals, 0) &&
#endif
      TEST_HOOK(ntdll.dll, NtCreateFile, NotEquals, 0) &&
      TEST_HOOK(ntdll.dll, NtReadFile, NotEquals, 0) &&
      TEST_HOOK(ntdll.dll, NtReadFileScatter, NotEquals, 0) &&
      TEST_HOOK(ntdll.dll, NtWriteFile, NotEquals, 0) &&
      TEST_HOOK(ntdll.dll, NtWriteFileGather, NotEquals, 0) &&
      TEST_HOOK(ntdll.dll, NtQueryFullAttributesFile, NotEquals, 0) &&
#ifndef MOZ_ASAN
      // Bug 733892: toolkit/crashreporter/nsExceptionHandler.cpp
      // This fails on ASan because the ASan runtime already hooked this function
      TEST_HOOK(kernel32.dll, SetUnhandledExceptionFilter, Ignore, nullptr) &&
#endif
#ifdef _M_IX86
      TEST_HOOK_FOR_INVALID_HANDLE_VALUE(kernel32.dll, CreateFileW) &&
#endif
      TEST_HOOK_FOR_INVALID_HANDLE_VALUE(kernel32.dll, CreateFileA) &&
      TEST_HOOK(kernelbase.dll, QueryDosDeviceW, Equals, 0) &&
      TEST_DETOUR(user32.dll, CreateWindowExW, Equals, nullptr) &&
      TEST_HOOK(user32.dll, InSendMessageEx, Equals, ISMEX_NOSEND) &&
      TEST_HOOK(imm32.dll, ImmGetContext, Equals, nullptr) &&
      TEST_HOOK(imm32.dll, ImmGetCompositionStringW, Ignore, 0) &&
      TEST_HOOK_SKIP_EXEC(imm32.dll, ImmSetCandidateWindow) &&
      TEST_HOOK(imm32.dll, ImmNotifyIME, Equals, 0) &&
      TEST_HOOK(comdlg32.dll, GetSaveFileNameW, Ignore, FALSE) &&
      TEST_HOOK(comdlg32.dll, GetOpenFileNameW, Ignore, FALSE) &&
#ifdef _M_X64
      TEST_HOOK(user32.dll, GetKeyState, Ignore, 0) &&    // see Bug 1316415
      TEST_HOOK(ntdll.dll, LdrUnloadDll, NotEquals, 0) &&
      MAYBE_TEST_HOOK_SKIP_EXEC(IsWin8OrLater(), ntdll.dll, LdrResolveDelayLoadedAPI) &&
      MAYBE_TEST_HOOK(!IsWin8OrLater(), kernel32.dll, RtlInstallFunctionTableCallback, Equals, FALSE) &&
      TEST_HOOK(comdlg32.dll, PrintDlgW, Ignore, 0) &&
#endif
      MAYBE_TEST_HOOK(ShouldTestTipTsf(), tiptsf.dll, ProcessCaretEvents, Ignore, nullptr) &&
#ifdef _M_IX86
      TEST_HOOK(user32.dll, SendMessageTimeoutW, Equals, 0) &&
#endif
      TEST_HOOK(user32.dll, SetCursorPos, NotEquals, FALSE) &&
      TEST_HOOK(kernel32.dll, TlsAlloc, NotEquals, TLS_OUT_OF_INDEXES) &&
      TEST_HOOK_PARAMS(kernel32.dll, TlsFree, Equals, FALSE, TLS_OUT_OF_INDEXES) &&
      TEST_HOOK(kernel32.dll, CloseHandle, Equals, FALSE) &&
      TEST_HOOK(kernel32.dll, DuplicateHandle, Equals, FALSE) &&

      TEST_HOOK(wininet.dll, InternetOpenA, NotEquals, nullptr) &&
      TEST_HOOK(wininet.dll, InternetCloseHandle, Equals, FALSE) &&
      TEST_HOOK(wininet.dll, InternetConnectA, Equals, nullptr) &&
      TEST_HOOK(wininet.dll, InternetQueryDataAvailable, Equals, FALSE) &&
      TEST_HOOK(wininet.dll, InternetReadFile, Equals, FALSE) &&
      TEST_HOOK(wininet.dll, InternetWriteFile, Equals, FALSE) &&
      TEST_HOOK(wininet.dll, InternetSetOptionA, Equals, FALSE) &&
      TEST_HOOK(wininet.dll, HttpAddRequestHeadersA, Equals, FALSE) &&
      TEST_HOOK(wininet.dll, HttpOpenRequestA, Equals, nullptr) &&
      TEST_HOOK(wininet.dll, HttpQueryInfoA, Equals, FALSE) &&
      TEST_HOOK(wininet.dll, HttpSendRequestA, Equals, FALSE) &&
      TEST_HOOK(wininet.dll, HttpSendRequestExA, Equals, FALSE) &&
      TEST_HOOK(wininet.dll, HttpEndRequestA, Equals, FALSE) &&
      TEST_HOOK(wininet.dll, InternetQueryOptionA, Equals, FALSE) &&

      TEST_HOOK(sspicli.dll, AcquireCredentialsHandleA, NotEquals, SEC_E_OK) &&
      TEST_HOOK(sspicli.dll, QueryCredentialsAttributesA, NotEquals, SEC_E_OK) &&
      TEST_HOOK(sspicli.dll, FreeCredentialsHandle, NotEquals, SEC_E_OK) &&

      TEST_DETOUR_SKIP_EXEC(kernel32.dll, BaseThreadInitThunk) &&
      TEST_DETOUR_SKIP_EXEC(ntdll.dll, LdrLoadDll)) {
    printf("TEST-PASS | WindowsDllInterceptor | all checks passed\n");

    LARGE_INTEGER end, freq;
    QueryPerformanceCounter(&end);

    QueryPerformanceFrequency(&freq);

    LARGE_INTEGER result;
    result.QuadPart = end.QuadPart - start.QuadPart;
    result.QuadPart *= 1000000;
    result.QuadPart /= freq.QuadPart;

    printf("Elapsed time: %lld microseconds\n", result.QuadPart);

    return 0;
  }

  return 1;
}
