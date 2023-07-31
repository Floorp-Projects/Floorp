/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include <tlhelp32.h>

#include <stdio.h>

#include "mozilla/Assertions.h"
#include "mozilla/NativeNt.h"
#include "mozilla/Vector.h"

#include "nsWindowsDllInterceptor.h"

NTSTATUS NTAPI NtMapViewOfSection(
    HANDLE aSection, HANDLE aProcess, PVOID* aBaseAddress, ULONG_PTR aZeroBits,
    SIZE_T aCommitSize, PLARGE_INTEGER aSectionOffset, PSIZE_T aViewSize,
    SECTION_INHERIT aInheritDisposition, ULONG aAllocationType,
    ULONG aProtectionFlags);

using namespace mozilla;

static WindowsDllInterceptor NtdllIntercept;

static WindowsDllInterceptor::FuncHookType<decltype(&::NtMapViewOfSection)>
    stub_NtMapViewOfSection;

static constexpr auto kDllUsedForLoadLibraryTests = L"dbghelp.dll";

class MappedViewsInfoCollector {
 public:
  struct MappedViewInfo {
    PVOID BaseAddress;
    HANDLE aSection;
    PUBLIC_OBJECT_BASIC_INFORMATION obiSection;
    MEMORY_BASIC_INFORMATION mbiVirtualMemory;
  };

  MappedViewsInfoCollector() = default;
  ~MappedViewsInfoCollector() = default;

  const MappedViewInfo* begin() const { return mMappedViewsInfo.begin(); }
  const MappedViewInfo* end() const { return mMappedViewsInfo.end(); }

  const MappedViewInfo* GetInfo(PVOID aBaseAddress) const {
    for (const auto& mappedView : mMappedViewsInfo) {
      if (mappedView.BaseAddress == aBaseAddress) {
        return &mappedView;
      }
    }
    return nullptr;
  }

  void Add(PVOID aBaseAddress, HANDLE aSection) {
    auto existingMappedViewInfo = GetInfo(aBaseAddress);
    if (existingMappedViewInfo) {
      MOZ_RELEASE_ASSERT(existingMappedViewInfo->BaseAddress == aBaseAddress);
      MOZ_RELEASE_ASSERT(existingMappedViewInfo->aSection == aSection);
      return;
    }

    MappedViewInfo mappedViewInfo{aBaseAddress, aSection};
    MOZ_RELEASE_ASSERT(NT_SUCCESS(::NtQueryObject(
        aSection, ObjectBasicInformation, &mappedViewInfo.obiSection,
        sizeof(mappedViewInfo.obiSection), nullptr)));
    MOZ_RELEASE_ASSERT(NT_SUCCESS(::NtQueryVirtualMemory(
        ::GetCurrentProcess(), aBaseAddress, MemoryBasicInformation,
        &mappedViewInfo.mbiVirtualMemory,
        sizeof(mappedViewInfo.mbiVirtualMemory), nullptr)));
    MOZ_RELEASE_ASSERT(mMappedViewsInfo.append(std::move(mappedViewInfo)));
  }

  void Reset() { MOZ_RELEASE_ASSERT(mMappedViewsInfo.resize(0)); }

 private:
  Vector<MappedViewInfo> mMappedViewsInfo;
};

static bool sIsTestRunning = false;
static MappedViewsInfoCollector sMappedViewsInfoCollector;

NTSTATUS NTAPI patched_NtMapViewOfSection(
    HANDLE aSection, HANDLE aProcess, PVOID* aBaseAddress, ULONG_PTR aZeroBits,
    SIZE_T aCommitSize, PLARGE_INTEGER aSectionOffset, PSIZE_T aViewSize,
    SECTION_INHERIT aInheritDisposition, ULONG aAllocationType,
    ULONG aProtectionFlags) {
  NTSTATUS result = stub_NtMapViewOfSection(
      aSection, aProcess, aBaseAddress, aZeroBits, aCommitSize, aSectionOffset,
      aViewSize, aInheritDisposition, aAllocationType, aProtectionFlags);
  if (sIsTestRunning && NT_SUCCESS(result)) {
    MOZ_RELEASE_ASSERT(aBaseAddress);
    sMappedViewsInfoCollector.Add(*aBaseAddress, aSection);
  }
  return result;
}

bool InitializeTests() {
  NtdllIntercept.Init(L"ntdll.dll");

  bool success = stub_NtMapViewOfSection.Set(
      NtdllIntercept, "NtMapViewOfSection", patched_NtMapViewOfSection);
  if (!success) {
    printf(
        "TEST-UNEXPECTED-FAIL | TestDllBlocklistAssumptions | "
        "Failed to hook NtMapViewOfSection.\n");
    fflush(stdout);
    return false;
  }

  printf(
      "TEST-PASS | TestDllBlocklistAssumptions | "
      "Successfully hooked NtMapViewOfSection.\n");
  fflush(stdout);
  return true;
}

bool CheckMappedViewAssumptions(const char* aTestDescription, PVOID baseAddress,
                                bool aExpectExecutableSection,
                                bool aExpectImageTypeMemory) {
  auto mappedViewInfo = sMappedViewsInfoCollector.GetInfo(baseAddress);
  if (!mappedViewInfo) {
    printf(
        "TEST-UNEXPECTED-FAIL | TestDllBlocklistAssumptions | "
        "Failed to find mapped view information while testing %s.\n",
        aTestDescription);
    fflush(stdout);
    return false;
  }

  MOZ_RELEASE_ASSERT(mappedViewInfo->BaseAddress == baseAddress);

  if (aExpectExecutableSection !=
      bool(mappedViewInfo->obiSection.GrantedAccess & SECTION_MAP_EXECUTE)) {
    printf(
        "TEST-UNEXPECTED-FAIL | TestDllBlocklistAssumptions | "
        "Mismatch in assumptions regarding section executablity while testing "
        "%s.\n",
        aTestDescription);
    fflush(stdout);
    return false;
  }

  if (aExpectImageTypeMemory !=
      bool(mappedViewInfo->mbiVirtualMemory.Type & MEM_IMAGE)) {
    printf(
        "TEST-UNEXPECTED-FAIL | TestDllBlocklistAssumptions | "
        "Mismatch in assumptions regarding virtual memory type while testing "
        "%s.\n",
        aTestDescription);
    fflush(stdout);
    return false;
  }
  return true;
}

// Assumptions used to block normal DLL loads:
//  - section handle was granted SECTION_MAP_EXECUTE access;
//  - virtual memory is tagged as MEM_IMAGE type.
bool TestDllLoad() {
  sMappedViewsInfoCollector.Reset();

  auto module = ::LoadLibraryW(kDllUsedForLoadLibraryTests);
  if (!module) {
    printf(
        "TEST-UNEXPECTED-FAIL | TestDllBlocklistAssumptions | "
        "Call to LoadLibraryW failed with error %lu.\n",
        ::GetLastError());
    fflush(stdout);
    return false;
  }

  auto baseAddress = nt::PEHeaders::HModuleToBaseAddr<PVOID>(module);
  bool result =
      CheckMappedViewAssumptions("LoadLibraryW", baseAddress, true, true);
  FreeLibrary(module);

  if (result) {
    printf(
        "TEST-PASS | TestDllBlocklistAssumptions | "
        "DLL loading works as expected.\n");
    fflush(stdout);
  }
  return result;
}

// Assumptions used to avoid blocking DLL loads when loading as data file:
//  - section handle was *not* granted SECTION_MAP_EXECUTE access;
//  - virtual memory is *not* tagged as MEM_IMAGE type.
bool TestDllLoadAsDataFile() {
  sMappedViewsInfoCollector.Reset();

  auto module = ::LoadLibraryExW(kDllUsedForLoadLibraryTests, nullptr,
                                 LOAD_LIBRARY_AS_DATAFILE);
  if (!module) {
    printf(
        "TEST-UNEXPECTED-FAIL | TestDllBlocklistAssumptions | "
        "Call to LoadLibraryExW failed with error %lu.\n",
        ::GetLastError());
    fflush(stdout);
    return false;
  }

  auto baseAddress = nt::PEHeaders::HModuleToBaseAddr<PVOID>(module);
  bool result = CheckMappedViewAssumptions(
      "LoadLibraryExW(LOAD_LIBRARY_AS_DATAFILE)", baseAddress, false, false);
  FreeLibrary(module);

  if (result) {
    printf(
        "TEST-PASS | TestDllBlocklistAssumptions | "
        "DLL loading as data file works as expected.\n");
    fflush(stdout);
  }
  return result;
}

// Assumptions used to avoid blocking DLL loads when loading as image resource:
//  - section handle was *not* granted SECTION_MAP_EXECUTE access;
//  - virtual memory is tagged as MEM_IMAGE type, however.
bool TestDllLoadAsImageResource() {
  sMappedViewsInfoCollector.Reset();

  auto module = ::LoadLibraryExW(kDllUsedForLoadLibraryTests, nullptr,
                                 LOAD_LIBRARY_AS_IMAGE_RESOURCE);
  if (!module) {
    printf(
        "TEST-UNEXPECTED-FAIL | TestDllBlocklistAssumptions | "
        "Call to LoadLibraryExW failed with error %lu.\n",
        ::GetLastError());
    fflush(stdout);
    return false;
  }

  auto baseAddress = nt::PEHeaders::HModuleToBaseAddr<PVOID>(module);
  bool result = CheckMappedViewAssumptions(
      "LoadLibraryExW(LOAD_LIBRARY_AS_IMAGE_RESOURCE)", baseAddress, false,
      true);
  FreeLibrary(module);

  if (result) {
    printf(
        "TEST-PASS | TestDllBlocklistAssumptions | "
        "DLL loading as image resource works as expected.\n");
    fflush(stdout);
  }
  return result;
}

// Assumptions used to avoid crashing when using Thread32Next (bug 1733532):
//  - section handle was *not* granted SECTION_MAP_EXECUTE access;
//  - virtual memory is *not* tagged as MEM_IMAGE type.
bool TestThreadIteration() {
  sMappedViewsInfoCollector.Reset();

  HANDLE snapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  if (snapshot == INVALID_HANDLE_VALUE) {
    printf(
        "TEST-UNEXPECTED-FAIL | TestDllBlocklistAssumptions | "
        "Call toCreateToolhelp32Snapshot failed with error %lu.\n",
        ::GetLastError());
    fflush(stdout);
    return false;
  }

  THREADENTRY32 entry{};
  entry.dwSize = sizeof(entry);
  if (!::Thread32First(snapshot, &entry)) {
    printf(
        "TEST-UNEXPECTED-FAIL | TestDllBlocklistAssumptions | "
        "Call to Thread32First failed with error %lu.\n",
        ::GetLastError());
    fflush(stdout);
    return false;
  }

  while (::Thread32Next(snapshot, &entry)) {
  }
  auto error = GetLastError();
  if (error != ERROR_NO_MORE_FILES) {
    printf(
        "TEST-UNEXPECTED-FAIL | TestDllBlocklistAssumptions | "
        "Call to Thread32Next failed with unexpected error %lu.\n",
        error);
    fflush(stdout);
    return false;
  }

  uint32_t count = 0;
  for (const auto& mappedViewInfo : sMappedViewsInfoCollector) {
    if (!CheckMappedViewAssumptions("Thread32Next", mappedViewInfo.BaseAddress,
                                    false, false)) {
      return false;
    }
    ++count;
  }

  if (!count) {
    printf(
        "TEST-UNEXPECTED-FAIL | TestDllBlocklistAssumptions | "
        "Unexpectedly found no mappings after iterating threads with "
        "Thread32Next.\n");
    fflush(stdout);
    return false;
  }

  printf(
      "TEST-PASS | TestDllBlocklistAssumptions | "
      "Iterating threads with Thread32Next works as expected.\n");
  fflush(stdout);
  return true;
}

extern "C" int wmain(int argc, wchar_t* argv[]) {
  LARGE_INTEGER start;
  QueryPerformanceCounter(&start);

  sIsTestRunning = true;
  if (InitializeTests() && TestDllLoad() && TestDllLoadAsDataFile() &&
      TestDllLoadAsImageResource() && TestThreadIteration()) {
    sIsTestRunning = false;

    printf("TEST-PASS | TestDllBlocklistAssumptions | all checks passed\n");

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

  sIsTestRunning = false;
  return 1;
}
