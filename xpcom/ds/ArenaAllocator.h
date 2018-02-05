/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ArenaAllocator_h
#define mozilla_ArenaAllocator_h

#include <algorithm>
#include <cstdint>
#include <sstream>

#include "mozilla/Assertions.h"
#include "mozilla/fallible.h"
#include "mozilla/Likely.h"
#include "mozilla/MemoryChecking.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/OperatorNewExtensions.h"
#include "mozilla/Poison.h"
#include "mozilla/TemplateLib.h"
#include "nsDebug.h"

namespace mozilla {

/**
 * A very simple arena allocator based on NSPR's PLArena.
 *
 * The arena allocator only provides for allocation, all memory is retained
 * until the allocator is destroyed. It's useful for situations where a large
 * amount of small transient allocations are expected.
 *
 * Example usage:
 *
 * // Define an allocator that is page sized and returns allocations that are
 * // 8-byte aligned.
 * ArenaAllocator<4096, 8> a;
 * for (int i = 0; i < 1000; i++) {
 *   DoSomething(a.Allocate(i));
 * }
 */
template<size_t ArenaSize, size_t Alignment=1>
class ArenaAllocator
{
public:
  constexpr ArenaAllocator()
    : mHead()
    , mCurrent(nullptr)
  {
     static_assert(mozilla::tl::FloorLog2<Alignment>::value ==
                   mozilla::tl::CeilingLog2<Alignment>::value,
                  "ArenaAllocator alignment must be a power of two");
  }

  ArenaAllocator(const ArenaAllocator&) = delete;
  ArenaAllocator& operator=(const ArenaAllocator&) = delete;

  /**
   * Frees all internal arenas but does not call destructors for objects
   * allocated out of the arena.
   */
  ~ArenaAllocator()
  {
    Clear();
  }

  /**
   * Fallibly allocates a chunk of memory with the given size from the internal
   * arenas. If the allocation size is larger than the chosen arena a size an
   * entire arena is allocated and used.
   */
  MOZ_ALWAYS_INLINE void* Allocate(size_t aSize, const fallible_t&)
  {
    MOZ_RELEASE_ASSERT(aSize, "Allocation size must be non-zero");
    return InternalAllocate(AlignedSize(aSize));
  }

  void* Allocate(size_t aSize)
  {
    void* p = Allocate(aSize, fallible);
    if (MOZ_UNLIKELY(!p)) {
      NS_ABORT_OOM(std::max(aSize, ArenaSize));
    }

    return p;
  }

  /**
   * Frees all entries. The allocator can be reused after this is called.
   *
   * NB: This will not run destructors of any objects that were allocated from
   * the arena.
   */
  void Clear()
  {
    // Free all chunks.
    auto a = mHead.next;
    while (a) {
      auto tmp = a;
      a = a->next;
      free(tmp);
    }

    // Reset the list head.
    mHead.next = nullptr;
    mCurrent = nullptr;
  }

  /**
   * Adjusts the given size to the required alignment.
   */
  static constexpr size_t AlignedSize(size_t aSize)
  {
    return (aSize + (Alignment - 1)) & ~(Alignment - 1);
  }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
  {
    size_t s = 0;
    for (auto arena = mHead.next; arena; arena = arena->next) {
      s += aMallocSizeOf(arena);
    }

    return s;
  }


  void Check()
  {
    if (mCurrent) {
      mCurrent->canary.Check();
    }
  }

private:
  struct ArenaHeader
  {
    /**
     * The location in memory of the data portion of the arena.
     */
    uintptr_t offset;
    /**
     * The location in memory of the end of the data portion of the arena.
     */
    uintptr_t tail;
  };

  struct ArenaChunk
  {
    constexpr ArenaChunk() : header{0, 0}, next(nullptr) {}

    explicit ArenaChunk(size_t aSize)
      : header{AlignedSize(uintptr_t(this + 1)), uintptr_t(this) + aSize}
      , next(nullptr)
    {
    }

    CorruptionCanary canary;
    ArenaHeader header;
    ArenaChunk* next;

    /**
     * Allocates a chunk of memory out of the arena and advances the offset.
     */
    void* Allocate(size_t aSize)
    {
      MOZ_ASSERT(aSize <= Available());
      char* p = reinterpret_cast<char*>(header.offset);
      MOZ_RELEASE_ASSERT(p);
      header.offset += aSize;
      canary.Check();
      MOZ_MAKE_MEM_UNDEFINED(p, aSize);
      return p;
    }

    /**
     * Calculates the amount of space available for allocation in this chunk.
     */
    size_t Available() const {
      return header.tail - header.offset;
    }
  };

  /**
   * Allocates an arena chunk of the given size and initializes its header.
   */
  ArenaChunk* AllocateChunk(size_t aSize)
  {
    static const size_t kOffset = AlignedSize(sizeof(ArenaChunk));
    MOZ_ASSERT(kOffset < aSize);

    const size_t chunkSize = aSize + kOffset;
    void* p = malloc(chunkSize);
    if (!p) {
      return nullptr;
    }

    ArenaChunk* arena = new (KnownNotNull, p) ArenaChunk(chunkSize);
    MOZ_MAKE_MEM_NOACCESS((void*)arena->header.offset,
                          arena->header.tail - arena->header.offset);

    // Insert into the head of the list.
    arena->next = mHead.next;
    mHead.next = arena;

    // Only update |mCurrent| if this is a standard allocation, large
    // allocations will always end up full so there's no point in updating
    // |mCurrent| in that case.
    if (aSize == ArenaSize - kOffset) {
      mCurrent = arena;
    }

    return arena;
  }

  MOZ_ALWAYS_INLINE void* InternalAllocate(size_t aSize)
  {
    static_assert(ArenaSize > AlignedSize(sizeof(ArenaChunk)),
                  "Arena size must be greater than the header size");

    static const size_t kMaxArenaCapacity =
        ArenaSize - AlignedSize(sizeof(ArenaChunk));

    if (mCurrent && aSize <= mCurrent->Available()) {
      return mCurrent->Allocate(aSize);
    }

    ArenaChunk* arena = AllocateChunk(std::max(kMaxArenaCapacity, aSize));
    return arena ? arena->Allocate(aSize) : nullptr;
  }

  ArenaChunk mHead;
  ArenaChunk* mCurrent;
};

} // namespace mozilla

#endif // mozilla_ArenaAllocator_h
