/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_MemoryInfo_h
#define mozilla_MemoryInfo_h

#include "mozilla/EnumSet.h"
#include "nsTArray.h"

/**
 * MemoryInfo is a helper class which describes the attributes and sizes of a
 * particular region of VM memory on Windows. It roughtly corresponds to the
 * values in a MEMORY_BASIC_INFORMATION struct, summed over an entire region or
 * memory.
 */

namespace mozilla {

class MemoryInfo final
{
public:
  enum class Perm : uint8_t
  {
    Read,
    Write,
    Execute,
    CopyOnWrite,
    Guard,
    NoCache,
    WriteCombine,
  };
  enum class PageType : uint8_t
  {
    Image,
    Mapped,
    Private,
  };

  using PermSet = EnumSet<Perm>;
  using PageTypeSet = EnumSet<PageType>;

  MemoryInfo() = default;
  MOZ_IMPLICIT MemoryInfo(const MemoryInfo&) = default;

  uintptr_t Start() const { return mStart; }
  uintptr_t End() const { return mEnd; }

  PageTypeSet Type() const { return mType; }
  PermSet Perms() const { return mPerms; }

  size_t Reserved() const { return mReserved; }
  size_t Committed() const { return mCommitted; }
  size_t Free() const { return mFree; }
  size_t Size() const { return mSize; }

  // Returns a MemoryInfo object containing the sums of all region sizes,
  // divided into Reserved, Committed, and Free, depending on their State
  // properties.
  //
  // The entire range of aSize bytes starting at aPtr must correspond to a
  // single allocation. This restriction is enforced in debug builds.
  static MemoryInfo Get(const void* aPtr, size_t aSize);

private:
  uintptr_t mStart = 0;
  uintptr_t mEnd = 0;

  size_t mReserved = 0;
  size_t mCommitted = 0;
  size_t mFree = 0;
  size_t mSize = 0;

  PageTypeSet mType{};

  PermSet mPerms{};
};

} // namespace mozilla

#endif // mozilla_MemoryInfo_h
