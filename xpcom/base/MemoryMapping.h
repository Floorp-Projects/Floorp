/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_MemoryMapping_h
#define mozilla_MemoryMapping_h

#include "mozilla/EnumSet.h"
#include "nsString.h"
#include "nsTArray.h"

/**
 * MemoryMapping is a helper class which describes an entry in the Linux
 * /proc/<pid>/smaps file. See procfs(5) for details on the entry format.
 *
 * The GetMemoryMappings() function returns an array of such entries, sorted by
 * start address, one for each entry in the current process's address space.
 */

namespace mozilla {

enum class VMFlag : uint8_t
{
  Readable,      // rd  - readable
  Writable,      // wr  - writable
  Executable,    // ex  - executable
  Shared,        // sh  - shared
  MayRead,       // mr  - may read
  MayWrite,      // mw  - may write
  MayExecute,    // me  - may execute
  MayShare,      // ms  - may share
  GrowsDown,     // gd  - stack segment grows down
  PurePFN,       // pf  - pure PFN range
  DisabledWrite, // dw  - disabled write to the mapped file
  Locked,        // lo  - pages are locked in memory
  IO,            // io  - memory mapped I/O area
  Sequential,    // sr  - sequential read advise provided
  Random,        // rr  - random read advise provided
  NoFork,        // dc  - do not copy area on fork
  NoExpand,      // de  - do not expand area on remapping
  Accountable,   // ac  - area is accountable
  NotReserved,   // nr  - swap space is not reserved for the area
  HugeTLB,       // ht  - area uses huge tlb pages
  NonLinear,     // nl  - non-linear mapping
  ArchSpecific,  // ar  - architecture specific flag
  NoCore,        // dd  - do not include area into core dump
  SoftDirty,     // sd  - soft-dirty flag
  MixedMap,      // mm  - mixed map area
  HugePage,      // hg  - huge page advise flag
  NoHugePage,    // nh  - no-huge page advise flag
  Mergeable,     // mg  - mergeable advise flag
};

using VMFlagSet = EnumSet<VMFlag>;

class MemoryMapping final
{
public:
  enum class Perm : uint8_t
  {
    Read,
    Write,
    Execute,
    Shared,
    Private,
  };

  using PermSet = EnumSet<Perm>;

  MemoryMapping(uintptr_t aStart, uintptr_t aEnd,
                PermSet aPerms, size_t aOffset,
                const char* aName)
    : mStart(aStart)
    , mEnd(aEnd)
    , mOffset(aOffset)
    , mName(aName)
    , mPerms(aPerms)
  {}

  const nsCString& Name() const { return mName; }

  uintptr_t Start() const { return mStart; }
  uintptr_t End() const { return mEnd; }

  bool Includes(const void* aPtr) const
  {
    auto ptr = uintptr_t(aPtr);
    return ptr >= mStart && ptr < mEnd;
  }

  PermSet Perms() const { return mPerms; }
  VMFlagSet VMFlags() const { return mFlags; }

  // For file mappings, the offset in the mapped file which corresponds to the
  // start of the mapped region.
  size_t Offset() const { return mOffset; }

  size_t AnonHugePages() const { return mAnonHugePages; }
  size_t Anonymous() const { return mAnonymous; }
  size_t KernelPageSize() const { return mKernelPageSize; }
  size_t LazyFree() const { return mLazyFree; }
  size_t Locked() const { return mLocked; }
  size_t MMUPageSize() const { return mMMUPageSize; }
  size_t Private_Clean() const { return mPrivate_Clean; }
  size_t Private_Dirty() const { return mPrivate_Dirty; }
  size_t Private_Hugetlb() const { return mPrivate_Hugetlb; }
  size_t Pss() const { return mPss; }
  size_t Referenced() const { return mReferenced; }
  size_t Rss() const { return mRss; }
  size_t Shared_Clean() const { return mShared_Clean; }
  size_t Shared_Dirty() const { return mShared_Dirty; }
  size_t Shared_Hugetlb() const { return mShared_Hugetlb; }
  size_t ShmemPmdMapped() const { return mShmemPmdMapped; }
  size_t Size() const { return mSize; }
  size_t Swap() const { return mSwap; }
  size_t SwapPss() const { return mSwapPss; }

  // Dumps a string representation of the entry, similar to its format in the
  // smaps file, to the given string. Mainly useful for debugging.
  void Dump(nsACString& aOut) const;

  // These comparison operators are used for binary searching sorted arrays of
  // MemoryMapping entries to find the one which contains a given pointer.
  bool operator==(const void* aPtr) const { return Includes(aPtr); }
  bool operator<(const void* aPtr) const { return mStart < uintptr_t(aPtr); }

private:
  friend nsresult GetMemoryMappings(nsTArray<MemoryMapping>& aMappings);

  uintptr_t mStart = 0;
  uintptr_t mEnd = 0;

  size_t mOffset = 0;

  nsCString mName;

  // Members for size fields in the smaps file. Please keep these in sync with
  // the sFields array.
  size_t mAnonHugePages = 0;
  size_t mAnonymous = 0;
  size_t mKernelPageSize = 0;
  size_t mLazyFree = 0;
  size_t mLocked = 0;
  size_t mMMUPageSize = 0;
  size_t mPrivate_Clean = 0;
  size_t mPrivate_Dirty = 0;
  size_t mPrivate_Hugetlb = 0;
  size_t mPss = 0;
  size_t mReferenced = 0;
  size_t mRss = 0;
  size_t mShared_Clean = 0;
  size_t mShared_Dirty = 0;
  size_t mShared_Hugetlb = 0;
  size_t mShmemPmdMapped = 0;
  size_t mSize = 0;
  size_t mSwap = 0;
  size_t mSwapPss = 0;

  PermSet mPerms{};
  VMFlagSet mFlags{};

  // Contains the name and offset of one of the above size_t fields, for use in
  // parsing in dumping. The below helpers contain a list of the fields, and map
  // Field entries to the appropriate member in a class instance.
  struct Field
  {
    const char* mName;
    size_t mOffset;
  };

  static const Field sFields[20];

  size_t& ValueForField(const Field& aField)
  {
    char* fieldPtr = reinterpret_cast<char*>(this) + aField.mOffset;
    return reinterpret_cast<size_t*>(fieldPtr)[0];
  }
  size_t ValueForField(const Field& aField) const
  {
    return const_cast<MemoryMapping*>(this)->ValueForField(aField);
  }
};

nsresult GetMemoryMappings(nsTArray<MemoryMapping>& aMappings);

} // namespace mozilla

#endif // mozilla_MemoryMapping_h
