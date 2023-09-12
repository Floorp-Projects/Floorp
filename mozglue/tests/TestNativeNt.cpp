/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "nscore.h"
#include "mozilla/NativeNt.h"
#include "mozilla/ThreadLocal.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WindowsEnumProcessModules.h"

#include <limits>
#include <stdio.h>
#include <windows.h>
#include <strsafe.h>

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

// Need non-inline functions to bypass compiler optimization that the thread
// local storage pointer is cached in a register before accessing a thread-local
// variable. See bug 1803322 for a motivating example.
MOZ_NEVER_INLINE uint32_t getTlsData() { return sTlsData.get(); }
MOZ_NEVER_INLINE void setTlsData(uint32_t x) { sTlsData.set(x); }

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

// This class copies the self executable file to the %temp%\<outer>\<inner>
// folder.  The length of its path is longer than MAX_PATH.
class LongNameModule {
  wchar_t mOuterDirBuffer[MAX_PATH];
  wchar_t mInnerDirBuffer[MAX_PATH * 2];
  wchar_t mTargetFileBuffer[MAX_PATH * 2];

  const wchar_t* mOuterDir;
  const wchar_t* mInnerDir;
  const wchar_t* mTargetFile;

 public:
  explicit LongNameModule(const wchar_t* aNewLeafNameAfterCopy)
      : mOuterDir(nullptr), mInnerDir(nullptr), mTargetFile(nullptr) {
    const wchar_t kFolderName160Chars[] =
        L"0123456789ABCDEF0123456789ABCDEF"
        L"0123456789ABCDEF0123456789ABCDEF"
        L"0123456789ABCDEF0123456789ABCDEF"
        L"0123456789ABCDEF0123456789ABCDEF"
        L"0123456789ABCDEF0123456789ABCDEF";
    UniquePtr<wchar_t[]> thisExe = GetFullBinaryPath();
    if (!thisExe) {
      return;
    }

    // If the buffer is too small, GetTempPathW returns the required
    // length including a null character, while on a successful case
    // it returns the number of copied characters which does not include
    // a null character.  This means len == MAX_PATH should never happen
    // and len > MAX_PATH means GetTempPathW failed.
    wchar_t tempDir[MAX_PATH];
    DWORD len = ::GetTempPathW(MAX_PATH, tempDir);
    if (!len || len >= MAX_PATH) {
      return;
    }

    if (FAILED(::StringCbPrintfW(mOuterDirBuffer, sizeof(mOuterDirBuffer),
                                 L"\\\\?\\%s%s", tempDir,
                                 kFolderName160Chars)) ||
        !::CreateDirectoryW(mOuterDirBuffer, nullptr)) {
      return;
    }
    mOuterDir = mOuterDirBuffer;

    if (FAILED(::StringCbPrintfW(mInnerDirBuffer, sizeof(mInnerDirBuffer),
                                 L"\\\\?\\%s%s\\%s", tempDir,
                                 kFolderName160Chars, kFolderName160Chars)) ||
        !::CreateDirectoryW(mInnerDirBuffer, nullptr)) {
      return;
    }
    mInnerDir = mInnerDirBuffer;

    if (FAILED(::StringCbPrintfW(mTargetFileBuffer, sizeof(mTargetFileBuffer),
                                 L"\\\\?\\%s%s\\%s\\%s", tempDir,
                                 kFolderName160Chars, kFolderName160Chars,
                                 aNewLeafNameAfterCopy)) ||
        !::CopyFileW(thisExe.get(), mTargetFileBuffer,
                     /*bFailIfExists*/ TRUE)) {
      return;
    }
    mTargetFile = mTargetFileBuffer;
  }

  ~LongNameModule() {
    if (mTargetFile) {
      ::DeleteFileW(mTargetFile);
    }
    if (mInnerDir) {
      ::RemoveDirectoryW(mInnerDir);
    }
    if (mOuterDir) {
      ::RemoveDirectoryW(mOuterDir);
    }
  }

  operator const wchar_t*() const { return mTargetFile; }
};

// Make sure module info retrieved from nt::PEHeaders is the same as one
// retrieved from GetModuleInformation API.
bool CompareModuleInfo(HMODULE aModuleForApi, HMODULE aModuleForPEHeader) {
  MODULEINFO moduleInfo;
  if (!::GetModuleInformation(::GetCurrentProcess(), aModuleForApi, &moduleInfo,
                              sizeof(moduleInfo))) {
    printf("TEST-FAILED | NativeNt | GetModuleInformation failed - %08lx\n",
           ::GetLastError());
    return false;
  }

  PEHeaders headers(aModuleForPEHeader);
  if (!headers) {
    printf("TEST-FAILED | NativeNt | Failed to instantiate PEHeaders\n");
    return false;
  }

  Maybe<Range<const uint8_t>> bounds = headers.GetBounds();
  if (!bounds) {
    printf("TEST-FAILED | NativeNt | PEHeaders::GetBounds failed\n");
    return false;
  }

  if (bounds->length() != moduleInfo.SizeOfImage) {
    printf("TEST-FAILED | NativeNt | SizeOfImage does not match\n");
    return false;
  }

  // GetModuleInformation sets EntryPoint to 0 for executables
  // except the running self.
  static const HMODULE sSelf = ::GetModuleHandleW(nullptr);
  if (aModuleForApi != sSelf &&
      !(headers.GetFileCharacteristics() & IMAGE_FILE_DLL)) {
    if (moduleInfo.EntryPoint) {
      printf(
          "TEST-FAIL | NativeNt | "
          "GetModuleInformation returned a non-zero entrypoint "
          "for an executable\n");
      return false;
    }

    // Cannot verify PEHeaders::GetEntryPoint.
    return true;
  }

  // For a module whose entrypoint is 0 (e.g. ntdll.dll or win32u.dll),
  // MODULEINFO::EntryPoint is set to 0, while PEHeaders::GetEntryPoint
  // returns the imagebase (RVA=0).
  intptr_t rvaEntryPoint =
      moduleInfo.EntryPoint
          ? reinterpret_cast<uintptr_t>(moduleInfo.EntryPoint) -
                reinterpret_cast<uintptr_t>(moduleInfo.lpBaseOfDll)
          : 0;
  if (rvaEntryPoint < 0) {
    printf("TEST-FAILED | NativeNt | MODULEINFO is invalid\n");
    return false;
  }

  if (headers.RVAToPtr<FARPROC>(rvaEntryPoint) != headers.GetEntryPoint()) {
    printf("TEST-FAILED | NativeNt | Entrypoint does not match\n");
    return false;
  }

  return true;
}

bool TestModuleInfo() {
  UNICODE_STRING newLeafName;
  ::RtlInitUnicodeString(&newLeafName,
                         L"\u672D\u5E4C\u5473\u564C.\u30E9\u30FC\u30E1\u30F3");

  LongNameModule longNameModule(newLeafName.Buffer);
  if (!longNameModule) {
    printf(
        "TEST-FAILED | NativeNt | "
        "Failed to copy the executable to a long directory path\n");
    return 1;
  }

  {
    nsModuleHandle module(::LoadLibraryW(longNameModule));

    bool detectedTarget = false;
    bool passedAllModules = true;
    auto moduleCallback = [&](const wchar_t* aModulePath, HMODULE aModule) {
      UNICODE_STRING modulePath, moduleName;
      ::RtlInitUnicodeString(&modulePath, aModulePath);
      GetLeafName(&moduleName, &modulePath);
      if (::RtlEqualUnicodeString(&moduleName, &newLeafName,
                                  /*aCaseInsensitive*/ TRUE)) {
        detectedTarget = true;
      }

      if (!CompareModuleInfo(aModule, aModule)) {
        passedAllModules = false;
      }
    };

    if (!mozilla::EnumerateProcessModules(moduleCallback)) {
      printf("TEST-FAILED | NativeNt | EnumerateProcessModules failed\n");
      return false;
    }

    if (!detectedTarget) {
      printf(
          "TEST-FAILED | NativeNt | "
          "EnumerateProcessModules missed the target file\n");
      return false;
    }

    if (!passedAllModules) {
      return false;
    }
  }

  return true;
}

// Make sure PEHeaders works for a module loaded with LOAD_LIBRARY_AS_DATAFILE
// as well as a module loaded normally.
bool TestModuleLoadedAsData() {
  const wchar_t kNewLeafName[] = L"\u03BC\u0061\u9EBA.txt";

  LongNameModule longNameModule(kNewLeafName);
  if (!longNameModule) {
    printf(
        "TEST-FAILED | NativeNt | "
        "Failed to copy the executable to a long directory path\n");
    return 1;
  }

  const wchar_t* kManualLoadModules[] = {
      L"mshtml.dll",
      L"shell32.dll",
      longNameModule,
  };

  for (const auto moduleName : kManualLoadModules) {
    // Must load a module as data first,
    nsModuleHandle moduleAsData(::LoadLibraryExW(
        moduleName, nullptr,
        LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE));

    // then load a module normally to map it on a different address.
    nsModuleHandle module(::LoadLibraryW(moduleName));

    if (!CompareModuleInfo(module.get(), moduleAsData.get())) {
      return false;
    }

    PEHeaders peAsData(moduleAsData.get());
    PEHeaders pe(module.get());
    if (!peAsData || !pe) {
      printf("TEST-FAIL | NativeNt | Failed to load the module\n");
      return false;
    }

    if (peAsData.RVAToPtr<HMODULE>(0) == pe.RVAToPtr<HMODULE>(0)) {
      printf(
          "TEST-FAIL | NativeNt | "
          "The module should have been mapped onto two different places\n");
      return false;
    }

    const auto* pdb1 = peAsData.GetPdbInfo();
    const auto* pdb2 = pe.GetPdbInfo();
    if (pdb1 && pdb2) {
      if (pdb1->pdbSignature != pdb2->pdbSignature ||
          pdb1->pdbAge != pdb2->pdbAge ||
          strcmp(pdb1->pdbFileName, pdb2->pdbFileName)) {
        printf(
            "TEST-FAIL | NativeNt | "
            "PDB info from the same module did not match.\n");
        return false;
      }
    } else if (pdb1 || pdb2) {
      printf(
          "TEST-FAIL | NativeNt | Failed to get PDB info from the module.\n");
      return false;
    }

    uint64_t version1, version2;
    bool result1 = peAsData.GetVersionInfo(version1);
    bool result2 = pe.GetVersionInfo(version2);
    if (result1 && result2) {
      if (version1 != version2) {
        printf("TEST-FAIL | NativeNt | Version mismatch\n");
        return false;
      }
    } else if (result1 || result2) {
      printf(
          "TEST-FAIL | NativeNt | Failed to get PDB info from the module.\n");
      return false;
    }
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

#if defined(_M_X64)
bool TestCheckStack() {
  auto stackBase = reinterpret_cast<uint8_t*>(RtlGetThreadStackBase());
  auto stackLimit = reinterpret_cast<uint8_t*>(RtlGetThreadStackLimit());
  uint8_t* stackPointer = nullptr;
  asm volatile("mov %%rsp, %0;" : "=r"(stackPointer));
  if (!(stackLimit < stackBase && stackLimit <= stackPointer &&
        stackPointer < stackBase)) {
    printf("TEST-FAIL | NativeNt | Stack addresses are not coherent.\n");
    return false;
  }
  uintptr_t committedBytes = stackPointer - stackLimit;
  const uint32_t maxExtraCommittedBytes = 0x10000;
  if ((committedBytes + maxExtraCommittedBytes) >
      std::numeric_limits<uint32_t>::max()) {
    printf(
        "TEST-FAIL | NativeNt | The stack limit is too high to perform the "
        "test.\n");
    return false;
  }
  for (uint32_t extraSize = 0; extraSize < maxExtraCommittedBytes;
       ++extraSize) {
    CheckStack(static_cast<uint32_t>(committedBytes) + extraSize);
    auto expectedNewLimit = stackLimit - ((extraSize + 0xFFF) & ~0xFFF);
    if (expectedNewLimit != RtlGetThreadStackLimit()) {
      printf(
          "TEST-FAIL | NativeNt | CheckStack did not grow the stack "
          "correctly (expected: %p, got: %p).\n",
          expectedNewLimit, RtlGetThreadStackLimit());
      return false;
    }
  }
  return true;
}
#endif  // _M_X64

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
  printf("sTlsData#1 = %08x\n", getTlsData());
  MOZ_SEH_TRY {
    // Need to call SwapThreadLocalStoragePointer inside __try to make sure
    // accessing sTlsData is caught by SEH.  This is due to clang's design.
    // https://bugs.llvm.org/show_bug.cgi?id=44174.
    origTlsHead = SwapThreadLocalStoragePointer(nullptr);
    setTlsData(~kTlsDataValue);
  }
  MOZ_SEH_EXCEPT(GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION
                     ? EXCEPTION_EXECUTE_HANDLER
                     : EXCEPTION_CONTINUE_SEARCH) {
    isExceptionThrown = true;
  }
  SwapThreadLocalStoragePointer(origTlsHead);
  printf("sTlsData#2 = %08x\n", getTlsData());
  setTlsData(kTlsDataValue);
  printf("sTlsData#3 = %08x\n", getTlsData());
  if (!isExceptionThrown || getTlsData() != kTlsDataValue) {
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

  const mozilla::nt::CodeViewRecord70* debugInfo = k32headers.GetPdbInfo();
  if (!debugInfo) {
    printf(
        "TEST-FAILED | NativeNt | Unable to obtain debug information from "
        "kernel32.dll\n");
    return 1;
  }

#ifndef WIN32  // failure on windows10x32
  if (stricmp(debugInfo->pdbFileName, "kernel32.pdb")) {
    printf(
        "TEST-FAILED | NativeNt | Unexpected PDB filename "
        "in kernel32.dll: %s\n",
        debugInfo->pdbFileName);
    return 1;
  }
#endif

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

  if (!TestModuleInfo()) {
    return 1;
  }

  if (!TestModuleLoadedAsData()) {
    return 1;
  }

#if defined(_M_X64)
  if (!TestCheckStack()) {
    return 1;
  }
#endif  // _M_X64

  printf("TEST-PASS | NativeNt | All tests ran successfully\n");
  return 0;
}
