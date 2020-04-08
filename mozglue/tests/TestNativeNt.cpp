/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "mozilla/NativeNt.h"
#include "mozilla/ThreadLocal.h"
#include "mozilla/UniquePtr.h"

#include <stdio.h>
#include <windows.h>

const wchar_t kNormal[] = L"Foo.dll";
const wchar_t kHex12[] = L"Foo.ABCDEF012345.dll";
const wchar_t kHex15[] = L"ABCDEF012345678.dll";
const wchar_t kHex16[] = L"ABCDEF0123456789.dll";
const wchar_t kHex17[] = L"ABCDEF0123456789a.dll";
const wchar_t kHex24[] = L"ABCDEF0123456789cdabef98.dll";
const wchar_t kHex8[] = L"01234567.dll";
const wchar_t kNonHex12[] = L"Foo.ABCDEFG12345.dll";
const wchar_t kHex13[] = L"Foo.ABCDEF0123456.dll";
const wchar_t kHex11[] = L"Foo.ABCDEF01234.dll";
const wchar_t kPrefixedHex16[] = L"Pabcdef0123456789.dll";
const uint32_t kTlsDataValue = 1234;
static MOZ_THREAD_LOCAL(uint32_t) sTlsData;

const char kFailFmt[] =
    "TEST-FAILED | NativeNt | %s(%s) should have returned %s but did not\n";

#define RUN_TEST(fn, varName, expected)         \
  if (fn(varName) == !expected) {               \
    printf(kFailFmt, #fn, #varName, #expected); \
    return 1;                                   \
  }

#define EXPECT_FAIL(fn, varName) RUN_TEST(fn, varName, false)

#define EXPECT_SUCCESS(fn, varName) RUN_TEST(fn, varName, true)

using namespace mozilla;
using namespace mozilla::nt;

bool TestVirtualQuery(HANDLE aProcess, LPCVOID aAddress) {
  MEMORY_BASIC_INFORMATION info1 = {}, info2 = {};
  SIZE_T result1 = ::VirtualQueryEx(aProcess, aAddress, &info1, sizeof(info1)),
         result2 = mozilla::nt::VirtualQueryEx(aProcess, aAddress, &info2,
                                               sizeof(info2));
  if (result1 != result2) {
    printf("TEST-FAILED | NativeNt | The returned values mismatch\n");
    return false;
  }

  if (!result1) {
    // Both APIs failed.
    return true;
  }

  if (memcmp(&info1, &info2, result1) != 0) {
    printf("TEST-FAILED | NativeNt | The returned structures mismatch\n");
    return false;
  }

  return true;
}

LauncherResult<HMODULE> GetModuleHandleFromLeafName(const wchar_t* aName) {
  UNICODE_STRING name;
  ::RtlInitUnicodeString(&name, aName);
  return nt::GetModuleHandleFromLeafName(name);
}

// Need a non-inline function to bypass compiler optimization that the thread
// local storage pointer is cached in a register before accessing a thread-local
// variable.
MOZ_NEVER_INLINE PVOID SwapThreadLocalStoragePointer(PVOID aNewValue) {
  auto oldValue = RtlGetThreadLocalStoragePointer();
  RtlSetThreadLocalStoragePointerForTestingOnly(aNewValue);
  return oldValue;
}

int wmain(int argc, wchar_t* argv[]) {
  UNICODE_STRING normal;
  ::RtlInitUnicodeString(&normal, kNormal);

  UNICODE_STRING hex12;
  ::RtlInitUnicodeString(&hex12, kHex12);

  UNICODE_STRING hex16;
  ::RtlInitUnicodeString(&hex16, kHex16);

  UNICODE_STRING hex24;
  ::RtlInitUnicodeString(&hex24, kHex24);

  UNICODE_STRING hex8;
  ::RtlInitUnicodeString(&hex8, kHex8);

  UNICODE_STRING nonHex12;
  ::RtlInitUnicodeString(&nonHex12, kNonHex12);

  UNICODE_STRING hex13;
  ::RtlInitUnicodeString(&hex13, kHex13);

  UNICODE_STRING hex11;
  ::RtlInitUnicodeString(&hex11, kHex11);

  UNICODE_STRING hex15;
  ::RtlInitUnicodeString(&hex15, kHex15);

  UNICODE_STRING hex17;
  ::RtlInitUnicodeString(&hex17, kHex17);

  UNICODE_STRING prefixedHex16;
  ::RtlInitUnicodeString(&prefixedHex16, kPrefixedHex16);

  EXPECT_FAIL(Contains12DigitHexString, normal);
  EXPECT_SUCCESS(Contains12DigitHexString, hex12);
  EXPECT_FAIL(Contains12DigitHexString, hex13);
  EXPECT_FAIL(Contains12DigitHexString, hex11);
  EXPECT_FAIL(Contains12DigitHexString, hex16);
  EXPECT_FAIL(Contains12DigitHexString, nonHex12);

  EXPECT_FAIL(IsFileNameAtLeast16HexDigits, normal);
  EXPECT_FAIL(IsFileNameAtLeast16HexDigits, hex12);
  EXPECT_SUCCESS(IsFileNameAtLeast16HexDigits, hex24);
  EXPECT_SUCCESS(IsFileNameAtLeast16HexDigits, hex16);
  EXPECT_SUCCESS(IsFileNameAtLeast16HexDigits, hex17);
  EXPECT_FAIL(IsFileNameAtLeast16HexDigits, hex8);
  EXPECT_FAIL(IsFileNameAtLeast16HexDigits, hex15);
  EXPECT_FAIL(IsFileNameAtLeast16HexDigits, prefixedHex16);

  if (RtlGetProcessHeap() != ::GetProcessHeap()) {
    printf("TEST-FAILED | NativeNt | RtlGetProcessHeap() is broken\n");
    return 1;
  }

#ifdef HAVE_SEH_EXCEPTIONS
  PVOID origTlsHead = nullptr;
  bool isExceptionThrown = false;
  // Touch sTlsData.get() several times to prevent the call to sTlsData.set()
  // from being optimized out in PGO build.
  printf("sTlsData#1 = %08x\n", sTlsData.get());
  MOZ_SEH_TRY {
    // Need to call SwapThreadLocalStoragePointer inside __try to make sure
    // accessing sTlsData is caught by SEH.  This is due to clang's design.
    // https://bugs.llvm.org/show_bug.cgi?id=44174.
    origTlsHead = SwapThreadLocalStoragePointer(nullptr);
    sTlsData.set(~kTlsDataValue);
  }
  MOZ_SEH_EXCEPT(GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION
                     ? EXCEPTION_EXECUTE_HANDLER
                     : EXCEPTION_CONTINUE_SEARCH) {
    isExceptionThrown = true;
  }
  SwapThreadLocalStoragePointer(origTlsHead);
  printf("sTlsData#2 = %08x\n", sTlsData.get());
  sTlsData.set(kTlsDataValue);
  printf("sTlsData#3 = %08x\n", sTlsData.get());
  if (!isExceptionThrown || sTlsData.get() != kTlsDataValue) {
    printf(
        "TEST-FAILED | NativeNt | RtlGetThreadLocalStoragePointer() is "
        "broken\n");
    return 1;
  }
#endif

  if (RtlGetCurrentThreadId() != ::GetCurrentThreadId()) {
    printf("TEST-FAILED | NativeNt | RtlGetCurrentThreadId() is broken\n");
    return 1;
  }

  const wchar_t kKernel32[] = L"kernel32.dll";
  DWORD verInfoSize = ::GetFileVersionInfoSizeW(kKernel32, nullptr);
  if (!verInfoSize) {
    printf(
        "TEST-FAILED | NativeNt | Call to GetFileVersionInfoSizeW failed with "
        "code %lu\n",
        ::GetLastError());
    return 1;
  }

  auto verInfoBuf = MakeUnique<char[]>(verInfoSize);

  if (!::GetFileVersionInfoW(kKernel32, 0, verInfoSize, verInfoBuf.get())) {
    printf(
        "TEST-FAILED | NativeNt | Call to GetFileVersionInfoW failed with code "
        "%lu\n",
        ::GetLastError());
    return 1;
  }

  UINT len;
  VS_FIXEDFILEINFO* fixedFileInfo = nullptr;
  if (!::VerQueryValueW(verInfoBuf.get(), L"\\", (LPVOID*)&fixedFileInfo,
                        &len)) {
    printf(
        "TEST-FAILED | NativeNt | Call to VerQueryValueW failed with code "
        "%lu\n",
        ::GetLastError());
    return 1;
  }

  const uint64_t expectedVersion =
      (static_cast<uint64_t>(fixedFileInfo->dwFileVersionMS) << 32) |
      static_cast<uint64_t>(fixedFileInfo->dwFileVersionLS);

  PEHeaders k32headers(::GetModuleHandleW(kKernel32));
  if (!k32headers) {
    printf(
        "TEST-FAILED | NativeNt | Failed parsing kernel32.dll's PE headers\n");
    return 1;
  }

  uint64_t version;
  if (!k32headers.GetVersionInfo(version)) {
    printf(
        "TEST-FAILED | NativeNt | Unable to obtain version information from "
        "kernel32.dll\n");
    return 1;
  }

  if (version != expectedVersion) {
    printf(
        "TEST-FAILED | NativeNt | kernel32.dll's detected version "
        "(0x%016llX) does not match expected version (0x%016llX)\n",
        version, expectedVersion);
    return 1;
  }

  Maybe<Span<IMAGE_THUNK_DATA>> iatThunks =
      k32headers.GetIATThunksForModule("kernel32.dll");
  if (iatThunks) {
    printf(
        "TEST-FAILED | NativeNt | Detected the IAT thunk for kernel32 "
        "in kernel32.dll\n");
    return 1;
  }

  PEHeaders ntdllheaders(::GetModuleHandleW(L"ntdll.dll"));

  auto ntdllBoundaries = ntdllheaders.GetBounds();
  if (!ntdllBoundaries) {
    printf(
        "TEST-FAILED | NativeNt | "
        "Unable to obtain the boundaries of ntdll.dll\n");
    return 1;
  }

  iatThunks =
      k32headers.GetIATThunksForModule("ntdll.dll", ntdllBoundaries.ptr());
  if (!iatThunks) {
    printf(
        "TEST-FAILED | NativeNt | Unable to find the IAT thunk for "
        "ntdll.dll in kernel32.dll\n");
    return 1;
  }

  // To test the Ex version of API, we purposely get a real handle
  // instead of a pseudo handle.
  nsAutoHandle process(
      ::OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, GetCurrentProcessId()));
  if (!process) {
    printf("TEST-FAILED | NativeNt | OpenProcess() failed - %08lx\n",
           ::GetLastError());
    return 1;
  }

  // Test Null page, Heap, Mapped image, and Invalid handle
  if (!TestVirtualQuery(process, nullptr) || !TestVirtualQuery(process, argv) ||
      !TestVirtualQuery(process, kNormal) ||
      !TestVirtualQuery(nullptr, kNormal)) {
    return 1;
  }

  auto moduleResult = GetModuleHandleFromLeafName(kKernel32);
  if (moduleResult.isErr() ||
      moduleResult.inspect() != k32headers.template RVAToPtr<HMODULE>(0)) {
    printf(
        "TEST-FAILED | NativeNt | "
        "GetModuleHandleFromLeafName returns a wrong value.\n");
    return 1;
  }

  moduleResult = GetModuleHandleFromLeafName(L"invalid");
  if (moduleResult.isOk()) {
    printf(
        "TEST-FAILED | NativeNt | "
        "GetModuleHandleFromLeafName unexpectedly returns a value.\n");
    return 1;
  }

  printf("TEST-PASS | NativeNt | All tests ran successfully\n");
  return 0;
}
