/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BASE_PROFILER_SHARED_LIBRARIES_H_
#define BASE_PROFILER_SHARED_LIBRARIES_H_

#include "BaseProfiler.h"

#include <algorithm>
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <vector>

class SharedLibrary {
 public:
  SharedLibrary(uintptr_t aStart, uintptr_t aEnd, uintptr_t aOffset,
                const std::string& aBreakpadId, const std::string& aCodeId,
                const std::string& aModuleName, const std::string& aModulePath,
                const std::string& aDebugName, const std::string& aDebugPath,
                const std::string& aVersion, const char* aArch)
      : mStart(aStart),
        mEnd(aEnd),
        mOffset(aOffset),
        mBreakpadId(aBreakpadId),
        mCodeId(aCodeId),
        mModuleName(aModuleName),
        mModulePath(aModulePath),
        mDebugName(aDebugName),
        mDebugPath(aDebugPath),
        mVersion(aVersion),
        mArch(aArch) {}

  SharedLibrary(const SharedLibrary& aEntry)
      : mStart(aEntry.mStart),
        mEnd(aEntry.mEnd),
        mOffset(aEntry.mOffset),
        mBreakpadId(aEntry.mBreakpadId),
        mCodeId(aEntry.mCodeId),
        mModuleName(aEntry.mModuleName),
        mModulePath(aEntry.mModulePath),
        mDebugName(aEntry.mDebugName),
        mDebugPath(aEntry.mDebugPath),
        mVersion(aEntry.mVersion),
        mArch(aEntry.mArch) {}

  SharedLibrary& operator=(const SharedLibrary& aEntry) {
    // Gracefully handle self assignment
    if (this == &aEntry) return *this;

    mStart = aEntry.mStart;
    mEnd = aEntry.mEnd;
    mOffset = aEntry.mOffset;
    mBreakpadId = aEntry.mBreakpadId;
    mCodeId = aEntry.mCodeId;
    mModuleName = aEntry.mModuleName;
    mModulePath = aEntry.mModulePath;
    mDebugName = aEntry.mDebugName;
    mDebugPath = aEntry.mDebugPath;
    mVersion = aEntry.mVersion;
    mArch = aEntry.mArch;
    return *this;
  }

  bool operator==(const SharedLibrary& other) const {
    return (mStart == other.mStart) && (mEnd == other.mEnd) &&
           (mOffset == other.mOffset) && (mModuleName == other.mModuleName) &&
           (mModulePath == other.mModulePath) &&
           (mDebugName == other.mDebugName) &&
           (mDebugPath == other.mDebugPath) &&
           (mBreakpadId == other.mBreakpadId) && (mCodeId == other.mCodeId) &&
           (mVersion == other.mVersion) && (mArch == other.mArch);
  }

  uintptr_t GetStart() const { return mStart; }
  uintptr_t GetEnd() const { return mEnd; }
  uintptr_t GetOffset() const { return mOffset; }
  const std::string& GetBreakpadId() const { return mBreakpadId; }
  const std::string& GetCodeId() const { return mCodeId; }
  const std::string& GetModuleName() const { return mModuleName; }
  const std::string& GetModulePath() const { return mModulePath; }
  const std::string& GetDebugName() const { return mDebugName; }
  const std::string& GetDebugPath() const { return mDebugPath; }
  const std::string& GetVersion() const { return mVersion; }
  const std::string& GetArch() const { return mArch; }

 private:
  SharedLibrary() : mStart{0}, mEnd{0}, mOffset{0} {}

  uintptr_t mStart;
  uintptr_t mEnd;
  uintptr_t mOffset;
  std::string mBreakpadId;
  // A string carrying an identifier for a binary.
  //
  // All platforms have different formats:
  // - Windows: The code ID for a Windows PE file.
  //  It's the PE timestamp and PE image size.
  // - macOS: The code ID for a macOS / iOS binary (mach-O).
  //  It's the mach-O UUID without dashes and without the trailing 0 for the
  //  breakpad ID.
  // - Linux/Android: The code ID for a Linux ELF file.
  //  It's the complete build ID, as hex string.
  std::string mCodeId;
  std::string mModuleName;
  std::string mModulePath;
  std::string mDebugName;
  std::string mDebugPath;
  std::string mVersion;
  std::string mArch;
};

static bool CompareAddresses(const SharedLibrary& first,
                             const SharedLibrary& second) {
  return first.GetStart() < second.GetStart();
}

class SharedLibraryInfo {
 public:
#ifdef MOZ_GECKO_PROFILER
  static SharedLibraryInfo GetInfoForSelf();
#  ifdef XP_WIN
  static SharedLibraryInfo GetInfoFromPath(const wchar_t* aPath);
#  endif

  static void Initialize();
#else
  static SharedLibraryInfo GetInfoForSelf() { return SharedLibraryInfo(); }
#  ifdef XP_WIN
  static SharedLibraryInfo GetInfoFromPath(const wchar_t* aPath) {
    return SharedLibraryInfo();
  }
#  endif

  static void Initialize() {}
#endif

  SharedLibraryInfo() {}

  void AddSharedLibrary(SharedLibrary entry) { mEntries.push_back(entry); }

  const SharedLibrary& GetEntry(size_t i) const { return mEntries[i]; }

  SharedLibrary& GetMutableEntry(size_t i) { return mEntries[i]; }

  // Removes items in the range [first, last)
  // i.e. element at the "last" index is not removed
  void RemoveEntries(size_t first, size_t last) {
    mEntries.erase(mEntries.begin() + first, mEntries.begin() + last);
  }

  bool Contains(const SharedLibrary& searchItem) const {
    return (mEntries.end() !=
            std::find(mEntries.begin(), mEntries.end(), searchItem));
  }

  size_t GetSize() const { return mEntries.size(); }

  void SortByAddress() {
    std::sort(mEntries.begin(), mEntries.end(), CompareAddresses);
  }

  void Clear() { mEntries.clear(); }

 private:
#ifdef XP_WIN
  void AddSharedLibraryFromModuleInfo(const wchar_t* aModulePath,
                                      mozilla::Maybe<HMODULE> aModule);
#endif

  std::vector<SharedLibrary> mEntries;
};

#endif  // BASE_PROFILER_SHARED_LIBRARIES_H_
