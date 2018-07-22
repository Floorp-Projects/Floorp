/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_recordreplay_ChunkAllocator_h
#define mozilla_recordreplay_ChunkAllocator_h

#include "SpinLock.h"

namespace mozilla {
namespace recordreplay {

// ChunkAllocator is a simple allocator class for creating objects which can be
// fetched by their integer id. Objects are stored as a linked list of arrays;
// like a linked list, existing entries can be accessed without taking or
// holding a lock, and using an array in each element mitigates the runtime
// cost of O(n) lookup.
//
// ChunkAllocator contents are never destroyed.
template <typename T>
class ChunkAllocator
{
  struct Chunk;
  typedef Atomic<Chunk*, SequentiallyConsistent, Behavior::DontPreserve> ChunkPointer;

  // A page sized block holding a next pointer and an array of as many things
  // as possible.
  struct Chunk
  {
    uint8_t mStorage[PageSize - sizeof(Chunk*)];
    ChunkPointer mNext;
    Chunk() : mStorage{}, mNext(nullptr) {}

    static size_t MaxThings() {
      return sizeof(mStorage) / sizeof(T);
    }

    T* GetThing(size_t i) {
      MOZ_RELEASE_ASSERT(i < MaxThings());
      return reinterpret_cast<T*>(&mStorage[i * sizeof(T)]);
    }
  };

  ChunkPointer mFirstChunk;
  Atomic<size_t, SequentiallyConsistent, Behavior::DontPreserve> mCapacity;
  SpinLock mLock;

  void EnsureChunk(ChunkPointer* aChunk) {
    if (!*aChunk) {
      *aChunk = new Chunk();
      mCapacity += Chunk::MaxThings();
    }
  }

  ChunkAllocator(const ChunkAllocator&) = delete;
  ChunkAllocator& operator=(const ChunkAllocator&) = delete;

public:
  // ChunkAllocators are allocated in static storage and should not have
  // constructors. Their memory will be initially zero.
  ChunkAllocator() = default;
  ~ChunkAllocator() = default;

  // Get an existing entry from the allocator.
  inline T* Get(size_t aId) {
    Chunk* chunk = mFirstChunk;
    while (aId >= Chunk::MaxThings()) {
      aId -= Chunk::MaxThings();
      chunk = chunk->mNext;
    }
    return chunk->GetThing(aId);
  }

  // Get an existing entry from the allocator, or null. This may return an
  // entry that has not been created yet.
  inline T* MaybeGet(size_t aId) {
    return (aId < mCapacity) ? Get(aId) : nullptr;
  }

  // Create a new entry with the specified ID. This must not be called on IDs
  // that have already been used with this allocator.
  inline T* Create(size_t aId) {
    if (aId < mCapacity) {
      T* res = Get(aId);
      return new(res) T();
    }

    AutoSpinLock lock(mLock);
    ChunkPointer* pchunk = &mFirstChunk;
    while (aId >= Chunk::MaxThings()) {
      aId -= Chunk::MaxThings();
      EnsureChunk(pchunk);
      Chunk* chunk = *pchunk;
      pchunk = &chunk->mNext;
    }
    EnsureChunk(pchunk);
    Chunk* chunk = *pchunk;
    T* res = chunk->GetThing(aId);
    return new(res) T();
  }
};

} // namespace recordreplay
} // namespace mozilla

#endif // mozilla_recordreplay_ChunkAllocator_h
