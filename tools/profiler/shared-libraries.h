/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Jeff Muizelaar <jmuizelaar@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef SHARED_LIBRARIES_H_
#define SHARED_LIBRARIES_H_

#ifndef MOZ_ENABLE_PROFILER_SPS
#error This header does not have a useful implementation on your platform!
#endif

#include <algorithm>
#include <vector>
#include <string.h>
#include <stdlib.h>
#include <mozilla/StandardInteger.h>
#include <nsID.h>

class SharedLibrary {
public:

  SharedLibrary(unsigned long aStart,
                unsigned long aEnd,
                unsigned long aOffset,
#ifdef XP_WIN
                nsID aPdbSignature,
                unsigned long aPdbAge,
#endif
                char *aName)
    : mStart(aStart)
    , mEnd(aEnd)
    , mOffset(aOffset)
#ifdef XP_WIN
    , mPdbSignature(aPdbSignature)
    , mPdbAge(aPdbAge)
#endif
    , mName(strdup(aName))
  {}

  SharedLibrary(const SharedLibrary& aEntry)
    : mStart(aEntry.mStart)
    , mEnd(aEntry.mEnd)
    , mOffset(aEntry.mOffset)
#ifdef XP_WIN
    , mPdbSignature(aEntry.mPdbSignature)
    , mPdbAge(aEntry.mPdbAge)
#endif
    , mName(strdup(aEntry.mName))
  {}

  SharedLibrary& operator=(const SharedLibrary& aEntry)
  {
    // Gracefully handle self assignment
    if (this == &aEntry) return *this;

    mStart = aEntry.mStart;
    mEnd = aEntry.mEnd;
    mOffset = aEntry.mOffset;
#ifdef XP_WIN
    mPdbSignature = aEntry.mPdbSignature;
    mPdbAge = aEntry.mPdbAge;
#endif
    if (mName)
      free(mName);
    mName = strdup(aEntry.mName);
    return *this;
  }

  bool operator==(const SharedLibrary& other) const
  {
    bool equal = ((mStart == other.mStart) &&
                  (mEnd == other.mEnd) &&
                  (mOffset == other.mOffset) &&
                  (mName && other.mName && (strcmp(mName, other.mName) == 0)));
#ifdef XP_WIN
    equal = equal &&
            (mPdbSignature.Equals(other.mPdbSignature)) &&
            (mPdbAge == other.mPdbAge);
#endif
    return equal;
  }

  ~SharedLibrary()
  {
    free(mName);
    mName = NULL;
  }

  uintptr_t GetStart() const { return mStart; }
  uintptr_t GetEnd() const { return mEnd; }
#ifdef XP_WIN
  nsID GetPdbSignature() const { return mPdbSignature; }
  uint32_t GetPdbAge() const { return mPdbAge; }
#endif
  char* GetName() const { return mName; }

private:
  explicit SharedLibrary() {}

  uintptr_t mStart;
  uintptr_t mEnd;
  uintptr_t mOffset;
#ifdef XP_WIN
  // Windows-specific PDB file identifiers
  nsID mPdbSignature;
  uint32_t mPdbAge;
#endif
  char *mName;
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

  SharedLibrary& GetEntry(size_t i)
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
