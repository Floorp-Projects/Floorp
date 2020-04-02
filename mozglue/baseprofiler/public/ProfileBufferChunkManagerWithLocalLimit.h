/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProfileBufferChunkManagerWithLocalLimit_h
#define ProfileBufferChunkManagerWithLocalLimit_h

#include "BaseProfiler.h"
#include "mozilla/BaseProfilerDetail.h"
#include "mozilla/ProfileBufferChunkManager.h"

namespace mozilla {

// Manages the Chunks for this process in a thread-safe manner, with a maximum
// size per process.
//
// "Unreleased" chunks are not owned here, only "released" chunks can be
// destroyed or recycled when reaching the memory limit, so it is theoretically
// possible to break that limit, if:
// - The user of this class doesn't release their chunks, AND/OR
// - The limit is too small (e.g., smaller than 2 or 3 chunks, which should be
//   the usual number of unreleased chunks in flight).
// In this case, it just means that we will use more memory than allowed,
// potentially risking OOMs. Hopefully this shouldn't happen in real code,
// assuming that the user is doing the right thing and releasing chunks ASAP,
// and that the memory limit is reasonably large.
class ProfileBufferChunkManagerWithLocalLimit final
    : public ProfileBufferChunkManager {
 public:
  using Length = ProfileBufferChunk::Length;

  // MaxTotalBytes: Maximum number of bytes allocated in all local Chunks.
  // ChunkMinBufferBytes: Minimum number of user-available bytes in each Chunk.
  // Note that Chunks use a bit more memory for their header.
  explicit ProfileBufferChunkManagerWithLocalLimit(size_t aMaxTotalBytes,
                                                   Length aChunkMinBufferBytes)
      : mMaxTotalBytes(aMaxTotalBytes),
        mChunkMinBufferBytes(aChunkMinBufferBytes) {}

  MOZ_MUST_USE size_t MaxTotalSize() const final {
    // `mMaxTotalBytes` is `const` so there is no need to lock the mutex.
    return mMaxTotalBytes;
  }

  MOZ_MUST_USE UniquePtr<ProfileBufferChunk> GetChunk() final {
    AUTO_PROFILER_STATS(Local_GetChunk);
    baseprofiler::detail::BaseProfilerAutoLock lock(mMutex);
    return GetChunk(lock);
  }

  void RequestChunk(std::function<void(UniquePtr<ProfileBufferChunk>)>&&
                        aChunkReceiver) final {
    AUTO_PROFILER_STATS(Local_RequestChunk);
    baseprofiler::detail::BaseProfilerAutoLock lock(mMutex);
    if (mChunkReceiver) {
      // We already have a chunk receiver, meaning a request is pending.
      return;
    }
    // Store the chunk receiver. This indicates that a request is pending, and
    // it will be handled in the next `FulfillChunkRequests()` call.
    mChunkReceiver = std::move(aChunkReceiver);
  }

  void FulfillChunkRequests() final {
    AUTO_PROFILER_STATS(Local_FulfillChunkRequests);
    std::function<void(UniquePtr<ProfileBufferChunk>)> chunkReceiver;
    UniquePtr<ProfileBufferChunk> chunk;
    {
      baseprofiler::detail::BaseProfilerAutoLock lock(mMutex);
      if (!mChunkReceiver) {
        // No receiver means no pending request, we're done.
        return;
      }
      // Otherwise there is a request, extract the receiver to call below.
      std::swap(chunkReceiver, mChunkReceiver);
      MOZ_ASSERT(!mChunkReceiver, "mChunkReceiver should have been emptied");
      // And allocate the requested chunk. This may fail, it's fine, we're
      // letting the receiver know about it.
      AUTO_PROFILER_STATS(Local_FulfillChunkRequests_GetChunk);
      chunk = GetChunk(lock);
    }
    // Invoke callback outside of lock, so that it can use other chunk manager
    // functions if needed.
    // Note that this means there could be a race, where another request happens
    // now and even gets fulfilled before this one is! It should be rare, and
    // shouldn't be a problem anyway, the user will still get their requested
    // chunks, new/recycled chunks look the same so their order doesn't matter.
    MOZ_ASSERT(!!chunkReceiver, "chunkReceiver shouldn't be empty here");
    std::move(chunkReceiver)(std::move(chunk));
  }

  void ReleaseChunks(UniquePtr<ProfileBufferChunk> aChunks) final {
    baseprofiler::detail::BaseProfilerAutoLock lock(mMutex);
    MOZ_ASSERT(mUser, "Not registered yet");
    // Compute the size of all provided chunks.
    size_t bytes = 0;
    for (const ProfileBufferChunk* chunk = aChunks.get(); chunk;
         chunk = chunk->GetNext()) {
      bytes += chunk->ChunkBytes();
    }
    // Transfer the chunks size from the unreleased bucket to the released one.
    mUnreleasedBytes -= bytes;
    if (!mReleasedChunks) {
      // No other released chunks at the moment, we're starting the list.
      MOZ_ASSERT(mReleasedBytes == 0);
      mReleasedBytes = bytes;
      mReleasedChunks = std::move(aChunks);
    } else {
      // Add to the end of the released chunks list (oldest first, most recent
      // last.)
      mReleasedBytes += bytes;
      mReleasedChunks->SetLast(std::move(aChunks));
    }
  }

  void SetChunkDestroyedCallback(
      std::function<void(const ProfileBufferChunk&)>&& aChunkDestroyedCallback)
      final {
    baseprofiler::detail::BaseProfilerAutoLock lock(mMutex);
    MOZ_ASSERT(mUser, "Not registered yet");
    mChunkDestroyedCallback = std::move(aChunkDestroyedCallback);
  }

  MOZ_MUST_USE UniquePtr<ProfileBufferChunk> GetExtantReleasedChunks() final {
    baseprofiler::detail::BaseProfilerAutoLock lock(mMutex);
    MOZ_ASSERT(mUser, "Not registered yet");
    mReleasedBytes = 0;
    return std::move(mReleasedChunks);
  }

  void ForgetUnreleasedChunks() final {
    baseprofiler::detail::BaseProfilerAutoLock lock(mMutex);
    MOZ_ASSERT(mUser, "Not registered yet");
    mUnreleasedBytes = 0;
  }

  MOZ_MUST_USE size_t
  SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const final {
    baseprofiler::detail::BaseProfilerAutoLock lock(mMutex);
    return SizeOfExcludingThis(aMallocSizeOf, lock);
  }

  MOZ_MUST_USE size_t
  SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const final {
    baseprofiler::detail::BaseProfilerAutoLock lock(mMutex);
    MOZ_ASSERT(mUser, "Not registered yet");
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf, lock);
  }

 protected:
  const ProfileBufferChunk* PeekExtantReleasedChunksAndLock() final {
    mMutex.Lock();
    MOZ_ASSERT(mUser, "Not registered yet");
    return mReleasedChunks.get();
  }
  void UnlockAfterPeekExtantReleasedChunks() final { mMutex.Unlock(); }

 private:
  MOZ_MUST_USE UniquePtr<ProfileBufferChunk> GetChunk(
      const baseprofiler::detail::BaseProfilerAutoLock&) {
    MOZ_ASSERT(mUser, "Not registered yet");
    UniquePtr<ProfileBufferChunk> chunk;
    // After this function, the total memory consumption will be the sum of:
    // - Bytes from released (i.e., full) chunks,
    // - Bytes from unreleased (still in use) chunks,
    // - Bytes from the chunk we want to create/recycle. (Note that we don't
    //   count the extra bytes of chunk header, and of extra allocation ability,
    //   for the new chunk, as it's assumed to be negligible compared to the
    //   total memory limit.)
    // If this total is higher than the local limit, we'll want to destroy
    // the oldest released chunks until we're under the limit; if any, we'll
    // recycle one of them to avoid a deallocation followed by an allocation.
    while (mReleasedBytes + mUnreleasedBytes + mChunkMinBufferBytes >=
               mMaxTotalBytes &&
           mReleasedBytes != 0) {
      MOZ_ASSERT(!!mReleasedChunks);
      // We have reached the local limit, extract the oldest released chunk,
      // which is the first one at `mReleasedChunks`.
      UniquePtr<ProfileBufferChunk> oldest =
          std::exchange(mReleasedChunks, mReleasedChunks->ReleaseNext());
      // Subtract its size from the "released" number of bytes.
      mReleasedBytes -= oldest->ChunkBytes();
      if (mChunkDestroyedCallback) {
        // Inform the user that we're going to destroy or recycle this chunk.
        mChunkDestroyedCallback(*oldest);
      }
      // Try to recycle at least one big-enough chunk. (All chunks should have
      // the same size, but it's a cheap test and may allow future adjustments
      // based on actual data rate.)
      if (!chunk && oldest->BufferBytes() >= mChunkMinBufferBytes) {
        // Recycle this chunk.
        chunk = std::move(oldest);
        chunk->MarkRecycled();
      }
    }

    if (!chunk) {
      // No recycling -> Create a chunk now. (This could still fail.)
      chunk = ProfileBufferChunk::Create(mChunkMinBufferBytes);
    }

    if (chunk) {
      // We do have a chunk (recycled or new), record its size as "unreleased".
      mUnreleasedBytes += chunk->ChunkBytes();
    }

    return chunk;
  }

  MOZ_MUST_USE size_t
  SizeOfExcludingThis(MallocSizeOf aMallocSizeOf,
                      const baseprofiler::detail::BaseProfilerAutoLock&) const {
    MOZ_ASSERT(mUser, "Not registered yet");
    // Note: Missing size of std::function external resources (if any).
    return mReleasedChunks ? mReleasedChunks->SizeOfIncludingThis(aMallocSizeOf)
                           : 0;
  }

  // Maxumum number of bytes that should be used by all unreleased and released
  // chunks. Note that only released chunks can be destroyed here, so it is the
  // responsibility of the user to properly release their chunks when possible.
  const size_t mMaxTotalBytes;

  // Minimum number of bytes that new chunks should be able to store.
  // Used when calling `ProfileBufferChunk::Create()`.
  const Length mChunkMinBufferBytes;

  // Mutex guarding the following members.
  mutable baseprofiler::detail::BaseProfilerMutex mMutex;

  // Number of bytes currently held in chunks that have been given away (through
  // `GetChunk` or `RequestChunk`) and not released yet.
  size_t mUnreleasedBytes = 0;

  // Number of bytes currently held in chunks that have been released and stored
  // in `mReleasedChunks` below.
  size_t mReleasedBytes = 0;

  // List of all released chunks. The oldest one should be at the start of the
  // list, and may be destroyed or recycled when the memory limit is reached.
  UniquePtr<ProfileBufferChunk> mReleasedChunks;

  // Optional callback used to notify the user when a chunk is about to be
  // destroyed or recycled. (The data content is always destroyed, but the chunk
  // container may be reused.)
  std::function<void(const ProfileBufferChunk&)> mChunkDestroyedCallback;

  // Callback set from `RequestChunk()`, until it is serviced in
  // `FulfillChunkRequests()`. There can only be one request in flight.
  std::function<void(UniquePtr<ProfileBufferChunk>)> mChunkReceiver;
};

}  // namespace mozilla

#endif  // ProfileBufferChunkManagerWithLocalLimit_h
