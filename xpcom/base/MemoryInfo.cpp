/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/MemoryInfo.h"

#include "mozilla/DebugOnly.h"

#include <WinBase.h>

namespace mozilla {

/* static */ MemoryInfo
MemoryInfo::Get(const void* aPtr, size_t aSize)
{
  MemoryInfo result;

  result.mStart = uintptr_t(aPtr);
  const char* ptr = reinterpret_cast<const char*>(aPtr);
  const char* end = ptr + aSize;
  DebugOnly<void*> base = nullptr;
  while (ptr < end) {
    MEMORY_BASIC_INFORMATION basicInfo;
    if (!VirtualQuery(ptr, &basicInfo, sizeof(basicInfo))) {
      break;
    }

    MOZ_ASSERT_IF(base, base == basicInfo.AllocationBase);
    base = basicInfo.AllocationBase;

    size_t regionSize = std::min(size_t(basicInfo.RegionSize),
                                 size_t(end - ptr));

    if (basicInfo.State == MEM_COMMIT) {
      result.mCommitted += regionSize;
    } else if (basicInfo.State == MEM_RESERVE) {
      result.mReserved += regionSize;
    } else if (basicInfo.State == MEM_FREE) {
      result.mFree += regionSize;
    } else {
      MOZ_ASSERT_UNREACHABLE("Unexpected region state");
    }
    result.mSize += regionSize;
    ptr += regionSize;

    if (result.mType.isEmpty()) {
      if (basicInfo.Type & MEM_IMAGE) {
        result.mType += PageType::Image;
      }
      if (basicInfo.Type & MEM_MAPPED) {
        result.mType += PageType::Mapped;
      }
      if (basicInfo.Type & MEM_PRIVATE) {
        result.mType += PageType::Private;
      }

      // The first 8 bits of AllocationProtect are an enum. The remaining bits
      // are flags.
      switch (basicInfo.AllocationProtect & 0xff) {
        case PAGE_EXECUTE_WRITECOPY:
          result.mPerms += Perm::CopyOnWrite;
          MOZ_FALLTHROUGH;
        case PAGE_EXECUTE_READWRITE:
          result.mPerms += Perm::Write;
          MOZ_FALLTHROUGH;
        case PAGE_EXECUTE_READ:
          result.mPerms += Perm::Read;
          MOZ_FALLTHROUGH;
        case PAGE_EXECUTE:
          result.mPerms += Perm::Execute;
          break;

        case PAGE_WRITECOPY:
          result.mPerms += Perm::CopyOnWrite;
          MOZ_FALLTHROUGH;
        case PAGE_READWRITE:
          result.mPerms += Perm::Write;
          MOZ_FALLTHROUGH;
        case PAGE_READONLY:
          result.mPerms += Perm::Read;
          break;

        default:
          break;
      }

      if (basicInfo.AllocationProtect & PAGE_GUARD) {
        result.mPerms += Perm::Guard;
      }
      if (basicInfo.AllocationProtect & PAGE_NOCACHE) {
        result.mPerms += Perm::NoCache;
      }
      if (basicInfo.AllocationProtect & PAGE_WRITECOMBINE) {
        result.mPerms += Perm::WriteCombine;
      }
    }
  }

  result.mEnd = uintptr_t(ptr);
  return result;
}

} // namespace mozilla
