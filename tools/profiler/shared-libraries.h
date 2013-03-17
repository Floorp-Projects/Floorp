/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SHARED_LIBRARIES_H_
#define SHARED_LIBRARIES_H_

#ifndef MOZ_ENABLE_PROFILER_SPS
#error This header does not have a useful implementation on your platform!
#endif

#include <algorithm>
#include <vector>
#include <string>
#include <stdlib.h>
#include <mozilla/StandardInteger.h>
#include <nsID.h>

class SharedLibrary {
public:

  SharedLibrary(uintptr_t aStart,
                uintptr_t aEnd,
                uintptr_t aOffset,
                const std::string& aBreakpadId,
                const std::string& aName)
    : mStart(aStart)
    , mEnd(aEnd)
    , mOffset(aOffset)
    , mBreakpadId(aBreakpadId)
    , mName(aName)
  {}

  SharedLibrary(const SharedLibrary& aEntry)
    : mStart(aEntry.mStart)
    , mEnd(aEntry.mEnd)
    , mOffset(aEntry.mOffset)
    , mBreakpadId(aEntry.mBreakpadId)
    , mName(aEntry.mName)
  {}

  SharedLibrary& operator=(const SharedLibrary& aEntry)
  {
    // Gracefully handle self assignment
    if (this == &aEntry) return *this;

    mStart = aEntry.mStart;
    mEnd = aEntry.mEnd;
    mOffset = aEntry.mOffset;
    mBreakpadId = aEntry.mBreakpadId;
    mName = aEntry.mName;
    return *this;
  }

  bool operator==(const SharedLibrary& other) const
  {
    return (mStart == other.mStart) &&
           (mEnd == other.mEnd) &&
           (mOffset == other.mOffset) &&
           (mName == other.mName) &&
           (mBreakpadId == other.mBreakpadId);
  }

  uintptr_t GetStart() const { return mStart; }
  uintptr_t GetEnd() const { return mEnd; }
  uintptr_t GetOffset() const { return mOffset; }
  const std::string &GetBreakpadId() const { return mBreakpadId; }
  const std::string &GetName() const { return mName; }

private:
  SharedLibrary() {}

  uintptr_t mStart;
  uintptr_t mEnd;
  uintptr_t mOffset;
  std::string mBreakpadId;
  std::string mName;
};

static bool
CompareAddresses(const SharedLibrary& first, const SharedLibrary& second)
{
  return first.GetStart() < second.GetStart();
}

class SharedLibraryInfo {
public:
  static SharedLibraryInfo GetInfoForSelf();
  SharedLibraryInfo() {}

  void AddSharedLibrary(SharedLibrary entry)
  {
    mEntries.push_back(entry);
  }

  const SharedLibrary& GetEntry(size_t i) const
  {
    return mEntries[i];
  }

  // Removes items in the range [first, last)
  // i.e. element at the "last" index is not removed
  void RemoveEntries(size_t first, size_t last)
  {
    mEntries.erase(mEntries.begin() + first, mEntries.begin() + last);
  }

  bool Contains(const SharedLibrary& searchItem) const
  {
    return (mEntries.end() !=
              std::find(mEntries.begin(), mEntries.end(), searchItem));
  }

  size_t GetSize() const
  {
    return mEntries.size();
  }

  void SortByAddress()
  {
    std::sort(mEntries.begin(), mEntries.end(), CompareAddresses);
  }

  void Clear()
  {
    mEntries.clear();
  }

private:
  std::vector<SharedLibrary> mEntries;
};

#endif
