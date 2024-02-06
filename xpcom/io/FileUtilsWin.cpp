/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileUtilsWin.h"

#include <windows.h>
#include <psapi.h>

#include "base/process_util.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/Unused.h"
#include "nsWindowsHelpers.h"

namespace {

// Scoped type used by HandleToFilename
struct ScopedMappedViewTraits {
  typedef void* type;
  static void* empty() { return nullptr; }
  static void release(void* aPtr) {
    if (aPtr) {
      mozilla::Unused << UnmapViewOfFile(aPtr);
    }
  }
};
typedef mozilla::Scoped<ScopedMappedViewTraits> ScopedMappedView;

}  // namespace

namespace mozilla {

bool HandleToFilename(HANDLE aHandle, const LARGE_INTEGER& aOffset,
                      nsAString& aFilename) {
  AUTO_PROFILER_LABEL("HandletoFilename", OTHER);

  aFilename.Truncate();
  // This implementation is nice because it uses fully documented APIs that
  // are available on all Windows versions that we support.
  nsAutoHandle fileMapping(
      CreateFileMapping(aHandle, nullptr, PAGE_READONLY, 0, 1, nullptr));
  if (!fileMapping) {
    return false;
  }
  ScopedMappedView view(MapViewOfFile(fileMapping, FILE_MAP_READ,
                                      aOffset.HighPart, aOffset.LowPart, 1));
  if (!view) {
    return false;
  }
  nsAutoString mappedFilename;
  DWORD len = 0;
  SetLastError(ERROR_SUCCESS);
  do {
    mappedFilename.SetLength(mappedFilename.Length() + MAX_PATH);
    len = GetMappedFileNameW(GetCurrentProcess(), view, mappedFilename.get(),
                             mappedFilename.Length());
  } while (!len && GetLastError() == ERROR_INSUFFICIENT_BUFFER);
  if (!len) {
    return false;
  }
  mappedFilename.Truncate(len);
  return NtPathToDosPath(mappedFilename, aFilename);
}

template <class T>
struct RVAMap {
  RVAMap(HANDLE map, DWORD offset) {
    SYSTEM_INFO info;
    GetSystemInfo(&info);

    DWORD alignedOffset =
        (offset / info.dwAllocationGranularity) * info.dwAllocationGranularity;

    MOZ_ASSERT(offset - alignedOffset < info.dwAllocationGranularity, "Wtf");

    mRealView = ::MapViewOfFile(map, FILE_MAP_READ, 0, alignedOffset,
                                sizeof(T) + (offset - alignedOffset));

    mMappedView =
        mRealView
            ? reinterpret_cast<T*>((char*)mRealView + (offset - alignedOffset))
            : nullptr;
  }
  ~RVAMap() {
    if (mRealView) {
      ::UnmapViewOfFile(mRealView);
    }
  }
  operator const T*() const { return mMappedView; }
  const T* operator->() const { return mMappedView; }

 private:
  const T* mMappedView;
  void* mRealView;
};

uint32_t GetExecutableArchitecture(const wchar_t* aPath) {
  nsAutoHandle file(::CreateFileW(aPath, GENERIC_READ, FILE_SHARE_READ, nullptr,
                                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                                  nullptr));
  if (!file) {
    return base::PROCESS_ARCH_INVALID;
  }

  nsAutoHandle map(
      ::CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr));
  if (!map) {
    return base::PROCESS_ARCH_INVALID;
  }

  RVAMap<IMAGE_DOS_HEADER> peHeader(map, 0);
  if (!peHeader) {
    return base::PROCESS_ARCH_INVALID;
  }

  RVAMap<IMAGE_NT_HEADERS> ntHeader(map, peHeader->e_lfanew);
  if (!ntHeader) {
    return base::PROCESS_ARCH_INVALID;
  }

  switch (ntHeader->FileHeader.Machine) {
    case IMAGE_FILE_MACHINE_I386:
      return base::PROCESS_ARCH_I386;
    case IMAGE_FILE_MACHINE_AMD64:
      return base::PROCESS_ARCH_X86_64;
    case IMAGE_FILE_MACHINE_ARM64:
      return base::PROCESS_ARCH_ARM_64;
    case IMAGE_FILE_MACHINE_ARM:
    case IMAGE_FILE_MACHINE_ARMNT:
    case IMAGE_FILE_MACHINE_THUMB:
      return base::PROCESS_ARCH_ARM;
    default:
      return base::PROCESS_ARCH_INVALID;
  }
}

}  // namespace mozilla
