/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SHARED_LIBRARIES_H_
#define SHARED_LIBRARIES_H_

#include "nsNativeCharsetUtils.h"
#include "nsString.h"
#include <nsID.h>

#include <algorithm>
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <vector>

namespace IPC {
class MessageReader;
class MessageWriter;
template <typename T>
struct ParamTraits;
}  // namespace IPC

class SharedLibrary {
 public:
  SharedLibrary(uintptr_t aStart, uintptr_t aEnd, uintptr_t aOffset,
                const nsCString& aBreakpadId, const nsCString& aCodeId,
                const nsString& aModuleName, const nsString& aModulePath,
                const nsString& aDebugName, const nsString& aDebugPath,
                const nsCString& aVersion, const char* aArch)
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
  const nsCString& GetBreakpadId() const { return mBreakpadId; }
  const nsCString& GetCodeId() const { return mCodeId; }
  const nsString& GetModuleName() const { return mModuleName; }
  const nsString& GetModulePath() const { return mModulePath; }
  const std::string GetNativeDebugPath() const {
    nsAutoCString debugPathStr;

    NS_CopyUnicodeToNative(mDebugPath, debugPathStr);

    return debugPathStr.get();
  }
  const nsString& GetDebugName() const { return mDebugName; }
  const nsString& GetDebugPath() const { return mDebugPath; }
  const nsCString& GetVersion() const { return mVersion; }
  const std::string& GetArch() const { return mArch; }
  size_t SizeOf() const {
    return sizeof *this + mBreakpadId.Length() + mCodeId.Length() +
           mModuleName.Length() * 2 + mModulePath.Length() * 2 +
           mDebugName.Length() * 2 + mDebugPath.Length() * 2 +
           mVersion.Length() + mArch.size();
  }

  SharedLibrary() : mStart{0}, mEnd{0}, mOffset{0} {}

 private:
  uintptr_t mStart;
  uintptr_t mEnd;
  uintptr_t mOffset;
  nsCString mBreakpadId;
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
  nsCString mCodeId;
  nsString mModuleName;
  nsString mModulePath;
  nsString mDebugName;
  nsString mDebugPath;
  nsCString mVersion;
  std::string mArch;

  friend struct IPC::ParamTraits<SharedLibrary>;
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

  void AddSharedLibrary(SharedLibrary entry) { mEntries.push_back(entry); }

  void AddAllSharedLibraries(const SharedLibraryInfo& sharedLibraryInfo) {
    mEntries.insert(mEntries.end(), sharedLibraryInfo.mEntries.begin(),
                    sharedLibraryInfo.mEntries.end());
  }

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

  // Remove duplicate entries from the vector.
  //
  // We purposefully don't use the operator== implementation of SharedLibrary
  // because it compares all the fields including mStart, mEnd and mOffset which
  // are not the same across different processes.
  void DeduplicateEntries() {
    static auto cmpSort = [](const SharedLibrary& a, const SharedLibrary& b) {
      return std::tie(a.GetModuleName(), a.GetBreakpadId()) <
             std::tie(b.GetModuleName(), b.GetBreakpadId());
    };
    static auto cmpEqual = [](const SharedLibrary& a, const SharedLibrary& b) {
      return std::tie(a.GetModuleName(), a.GetBreakpadId()) ==
             std::tie(b.GetModuleName(), b.GetBreakpadId());
    };
    // std::unique requires the vector to be sorted first. It can only remove
    // consecutive duplicate elements.
    std::sort(mEntries.begin(), mEntries.end(), cmpSort);
    // Remove the duplicates since it's sorted now.
    mEntries.erase(std::unique(mEntries.begin(), mEntries.end(), cmpEqual),
                   mEntries.end());
  }

  void Clear() { mEntries.clear(); }

  size_t SizeOf() const {
    size_t size = 0;

    for (const auto& item : mEntries) {
      size += item.SizeOf();
    }

    return size;
  }

 private:
  std::vector<SharedLibrary> mEntries;

  friend struct IPC::ParamTraits<SharedLibraryInfo>;
};

namespace IPC {
template <>
struct ParamTraits<SharedLibrary> {
  typedef SharedLibrary paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam);
  static bool Read(MessageReader* aReader, paramType* aResult);
};

template <>
struct ParamTraits<SharedLibraryInfo> {
  typedef SharedLibraryInfo paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam);
  static bool Read(MessageReader* aReader, paramType* aResult);
};
}  // namespace IPC

#endif
