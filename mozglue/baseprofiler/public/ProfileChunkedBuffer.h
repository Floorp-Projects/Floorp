/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProfileChunkedBuffer_h
#define ProfileChunkedBuffer_h

#include "mozilla/BaseProfilerDetail.h"
#include "mozilla/ProfileBufferChunkManager.h"
#include "mozilla/Unused.h"

#include <cstdio>
#include <utility>

namespace mozilla {

// Thread-safe buffer that can store blocks of different sizes during defined
// sessions, using Chunks (from a ChunkManager) as storage.
//
// Each *block* contains an *entry* and the entry size:
// [ entry_size | entry ] [ entry_size | entry ] ...
//
// *In-session* is a period of time during which `ProfileChunkedBuffer` allows
// reading and writing.
// *Out-of-session*, the `ProfileChunkedBuffer` object is still valid, but
// contains no data, and gracefully denies accesses.
//
// To write an entry, the buffer reserves a block of sufficient size (to contain
// user data of predetermined size), writes the entry size, and lets the caller
// fill the entry contents using a ProfileBufferEntryWriter. E.g.:
// ```
// ProfileChunkedBuffer cb(...);
// cb.ReserveAndPut([]() { return sizeof(123); },
//                  [&](ProfileBufferEntryWriter* aEW) {
//                    if (aEW) { aEW->WriteObject(123); }
//                  });
// ```
// Other `Put...` functions may be used as shortcuts for simple entries.
// The objects given to the caller's callbacks should only be used inside the
// callbacks and not stored elsewhere, because they keep their own references to
// chunk memory and therefore should not live longer.
// Different type of objects may be serialized into an entry, see
// `ProfileBufferEntryWriter::Serializer` for more information.
//
// When reading data, the buffer iterates over blocks (it knows how to read the
// entry size, and therefore move to the next block), and lets the caller read
// the entry inside of each block. E.g.:
// ```
// cb.ReadEach([](ProfileBufferEntryReader& aER) {
//   /* Use ProfileBufferEntryReader functions to read serialized objects. */
//   int n = aER.ReadObject<int>();
// });
// ```
// Different type of objects may be deserialized from an entry, see
// `ProfileBufferEntryReader::Deserializer` for more information.
//
// Writers may retrieve the block index corresponding to an entry
// (`ProfileBufferBlockIndex` is an opaque type preventing the user from easily
// modifying it). That index may later be used with `ReadAt` to get back to the
// entry in that particular block -- if it still exists.
class ProfileChunkedBuffer {
 public:
  using Byte = ProfileBufferChunk::Byte;
  using Length = ProfileBufferChunk::Length;

  enum class ThreadSafety { WithoutMutex, WithMutex };

  // Default constructor starts out-of-session (nothing to read or write).
  explicit ProfileChunkedBuffer(ThreadSafety aThreadSafety)
      : mMutex(aThreadSafety != ThreadSafety::WithoutMutex) {}

  // Start in-session with external chunk manager.
  ProfileChunkedBuffer(ThreadSafety aThreadSafety,
                       ProfileBufferChunkManager& aChunkManager)
      : mMutex(aThreadSafety != ThreadSafety::WithoutMutex) {
    SetChunkManager(aChunkManager);
  }

  // Start in-session with owned chunk manager.
  ProfileChunkedBuffer(ThreadSafety aThreadSafety,
                       UniquePtr<ProfileBufferChunkManager>&& aChunkManager)
      : mMutex(aThreadSafety != ThreadSafety::WithoutMutex) {
    SetChunkManager(std::move(aChunkManager));
  }

  ~ProfileChunkedBuffer() {
    // Do proper clean-up by resetting the chunk manager.
    ResetChunkManager();
  }

  // This cannot change during the lifetime of this buffer, so there's no need
  // to lock.
  [[nodiscard]] bool IsThreadSafe() const { return mMutex.IsActivated(); }

  [[nodiscard]] bool IsInSession() const {
    baseprofiler::detail::BaseProfilerMaybeAutoLock lock(mMutex);
    return !!mChunkManager;
  }

  // Stop using the current chunk manager.
  // If we own the current chunk manager, it will be destroyed.
  // This will always clear currently-held chunks, if any.
  void ResetChunkManager() {
    baseprofiler::detail::BaseProfilerMaybeAutoLock lock(mMutex);
    Unused << ResetChunkManager(lock);
  }

  // Set the current chunk manager.
  // The caller is responsible for keeping the chunk manager alive as along as
  // it's used here (until the next (Re)SetChunkManager, or
  // ~ProfileChunkedBuffer).
  void SetChunkManager(ProfileBufferChunkManager& aChunkManager) {
    baseprofiler::detail::BaseProfilerMaybeAutoLock lock(mMutex);
    Unused << ResetChunkManager(lock);
    SetChunkManager(aChunkManager, lock);
  }

  // Set the current chunk manager, and keep ownership of it.
  void SetChunkManager(UniquePtr<ProfileBufferChunkManager>&& aChunkManager) {
    baseprofiler::detail::BaseProfilerMaybeAutoLock lock(mMutex);
    Unused << ResetChunkManager(lock);
    mOwnedChunkManager = std::move(aChunkManager);
    if (mOwnedChunkManager) {
      SetChunkManager(*mOwnedChunkManager, lock);
    }
  }

  // Stop using the current chunk manager, and return it if owned here.
  [[nodiscard]] UniquePtr<ProfileBufferChunkManager> ExtractChunkManager() {
    baseprofiler::detail::BaseProfilerMaybeAutoLock lock(mMutex);
    return ResetChunkManager(lock);
  }

  void Clear() {
    baseprofiler::detail::BaseProfilerMaybeAutoLock lock(mMutex);
    if (MOZ_UNLIKELY(!mChunkManager)) {
      // Out-of-session.
      return;
    }

    Unused << mChunkManager->GetExtantReleasedChunks();

    mRangeStart = mRangeEnd = mNextChunkRangeStart;
    mPushedBlockCount = 0;
    mClearedBlockCount = 0;
    if (mCurrentChunk) {
      mCurrentChunk->MarkDone();
      mCurrentChunk->MarkRecycled();
      InitializeCurrentChunk(lock);
    }
  }

  // Buffer maximum length in bytes.
  Maybe<size_t> BufferLength() const {
    baseprofiler::detail::BaseProfilerMaybeAutoLock lock(mMutex);
    if (!mChunkManager) {
      return Nothing{};
    }
    return Some(mChunkManager->MaxTotalSize());
  }

  [[nodiscard]] size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
    baseprofiler::detail::BaseProfilerMaybeAutoLock lock(mMutex);
    return SizeOfExcludingThis(aMallocSizeOf, lock);
  }

  [[nodiscard]] size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
    baseprofiler::detail::BaseProfilerMaybeAutoLock lock(mMutex);
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf, lock);
  }

  // Snapshot of the buffer state.
  struct State {
    // Index to/before the first block.
    ProfileBufferIndex mRangeStart = 1;

    // Index past the last block. Equals mRangeStart if empty.
    ProfileBufferIndex mRangeEnd = 1;

    // Number of blocks that have been pushed into this buffer.
    uint64_t mPushedBlockCount = 0;

    // Number of blocks that have been removed from this buffer.
    // Note: Live entries = pushed - cleared.
    uint64_t mClearedBlockCount = 0;
  };

  // Get a snapshot of the current state.
  // When out-of-session, mFirstReadIndex==mNextWriteIndex, and
  // mPushedBlockCount==mClearedBlockCount==0.
  // Note that these may change right after this thread-safe call, so they
  // should only be used for statistical purposes.
  [[nodiscard]] State GetState() const {
    baseprofiler::detail::BaseProfilerMaybeAutoLock lock(mMutex);
    return {mRangeStart, mRangeEnd, mPushedBlockCount, mClearedBlockCount};
  }

  // Lock the buffer mutex and run the provided callback.
  // This can be useful when the caller needs to explicitly lock down this
  // buffer, but not do anything else with it.
  template <typename Callback>
  auto LockAndRun(Callback&& aCallback) const {
    baseprofiler::detail::BaseProfilerMaybeAutoLock lock(mMutex);
    return std::forward<Callback>(aCallback)();
  }

  // Get *all* chunks related to this buffer, including extant chunks in its
  // ChunkManager, and yet-unused new/recycled chunks.
  // We don't expect this buffer to be used again, though it's still possible
  // and will allocate the first buffer when needed.
  [[nodiscard]] UniquePtr<ProfileBufferChunk> GetAllChunks() {
    baseprofiler::detail::BaseProfilerMaybeAutoLock lock(mMutex);
    if (MOZ_UNLIKELY(!mChunkManager)) {
      // Out-of-session.
      return nullptr;
    }
    UniquePtr<ProfileBufferChunk> chunks =
        mChunkManager->GetExtantReleasedChunks();
    if (MOZ_LIKELY(!!mCurrentChunk)) {
      mCurrentChunk->MarkDone();
      chunks =
          ProfileBufferChunk::Join(std::move(chunks), std::move(mCurrentChunk));
    }
    chunks =
        ProfileBufferChunk::Join(std::move(chunks), std::move(mNextChunks));
    mChunkManager->ForgetUnreleasedChunks();
    mRangeStart = mRangeEnd = mNextChunkRangeStart;
    return chunks;
  }

#ifdef DEBUG
  void Dump(std::FILE* aFile = stdout) const {
    baseprofiler::detail::BaseProfilerMaybeAutoLock lock(mMutex);
    fprintf(aFile,
            "ProfileChunkedBuffer[%p] State: range %u-%u pushed=%u cleared=%u "
            "(live=%u)",
            this, unsigned(mRangeStart), unsigned(mRangeEnd),
            unsigned(mPushedBlockCount), unsigned(mClearedBlockCount),
            unsigned(mPushedBlockCount) - unsigned(mClearedBlockCount));
    if (MOZ_UNLIKELY(!mChunkManager)) {
      fprintf(aFile, " - Out-of-session\n");
      return;
    }
    fprintf(aFile, " - chunks:\n");
    bool hasChunks = false;
    mChunkManager->PeekExtantReleasedChunks(
        [&](const ProfileBufferChunk* aOldestChunk) {
          for (const ProfileBufferChunk* chunk = aOldestChunk; chunk;
               chunk = chunk->GetNext()) {
            fprintf(aFile, "R ");
            chunk->Dump(aFile);
            hasChunks = true;
          }
        });
    if (mCurrentChunk) {
      fprintf(aFile, "C ");
      mCurrentChunk->Dump(aFile);
      hasChunks = true;
    }
    for (const ProfileBufferChunk* chunk = mNextChunks.get(); chunk;
         chunk = chunk->GetNext()) {
      fprintf(aFile, "N ");
      chunk->Dump(aFile);
      hasChunks = true;
    }
    if (!hasChunks) {
      fprintf(aFile, " No chunks.\n");
    }
  }
#endif  // DEBUG

 private:
  [[nodiscard]] UniquePtr<ProfileBufferChunkManager> ResetChunkManager(
      const baseprofiler::detail::BaseProfilerMaybeAutoLock&) {
    UniquePtr<ProfileBufferChunkManager> chunkManager;
    if (mChunkManager) {
      mChunkManager->ForgetUnreleasedChunks();
#ifdef DEBUG
      mChunkManager->DeregisteredFrom(this);
#endif
      mChunkManager = nullptr;
      chunkManager = std::move(mOwnedChunkManager);
      if (mCurrentChunk) {
        mCurrentChunk->MarkDone();
        mCurrentChunk = nullptr;
      }
      mNextChunks = nullptr;
      mNextChunkRangeStart = mRangeEnd;
      mRangeStart = mRangeEnd;
      mPushedBlockCount = 0;
      mClearedBlockCount = 0;
    }
    return chunkManager;
  }

  void SetChunkManager(
      ProfileBufferChunkManager& aChunkManager,
      const baseprofiler::detail::BaseProfilerMaybeAutoLock& aLock) {
    MOZ_ASSERT(!mChunkManager);
    mChunkManager = &aChunkManager;
#ifdef DEBUG
    mChunkManager->RegisteredWith(this);
#endif

    mChunkManager->SetChunkDestroyedCallback(
        [this](const ProfileBufferChunk& aChunk) {
          for (;;) {
            ProfileBufferIndex rangeStart = mRangeStart;
            if (MOZ_LIKELY(rangeStart <= aChunk.RangeStart())) {
              if (MOZ_LIKELY(mRangeStart.compareExchange(
                      rangeStart,
                      aChunk.RangeStart() + aChunk.BufferBytes()))) {
                break;
              }
            }
          }
          mClearedBlockCount += aChunk.BlockCount();
        });

    // We start with one chunk right away.
    SetAndInitializeCurrentChunk(mChunkManager->GetChunk(), aLock);
  }

  [[nodiscard]] size_t SizeOfExcludingThis(
      MallocSizeOf aMallocSizeOf,
      const baseprofiler::detail::BaseProfilerMaybeAutoLock&) const {
    if (MOZ_UNLIKELY(!mChunkManager)) {
      // Out-of-session.
      return 0;
    }
    size_t size = mChunkManager->SizeOfIncludingThis(aMallocSizeOf);
    if (mCurrentChunk) {
      size += mCurrentChunk->SizeOfIncludingThis(aMallocSizeOf);
    }
    if (mNextChunks) {
      size += mNextChunks->SizeOfIncludingThis(aMallocSizeOf);
    }
    return size;
  }

  void InitializeCurrentChunk(
      const baseprofiler::detail::BaseProfilerMaybeAutoLock&) {
    MOZ_ASSERT(!!mCurrentChunk);
    mCurrentChunk->SetRangeStart(mNextChunkRangeStart);
    mNextChunkRangeStart += mCurrentChunk->BufferBytes();
    Unused << mCurrentChunk->ReserveInitialBlockAsTail(0);
  }

  void SetAndInitializeCurrentChunk(
      UniquePtr<ProfileBufferChunk>&& aChunk,
      const baseprofiler::detail::BaseProfilerMaybeAutoLock& aLock) {
    mCurrentChunk = std::move(aChunk);
    if (mCurrentChunk) {
      InitializeCurrentChunk(aLock);
    }
  }

  // Mutex guarding the following members.
  mutable baseprofiler::detail::BaseProfilerMaybeMutex mMutex;

  // Pointer to the current Chunk Manager (or null when out-of-session.)
  // It may be owned locally (see below) or externally.
  ProfileBufferChunkManager* mChunkManager = nullptr;

  // Only non-null when we own the current Chunk Manager.
  UniquePtr<ProfileBufferChunkManager> mOwnedChunkManager;

  UniquePtr<ProfileBufferChunk> mCurrentChunk;

  UniquePtr<ProfileBufferChunk> mNextChunks;

  // Range start of the next chunk to become current. Starting at 1 because
  // 0 is a reserved index similar to nullptr.
  ProfileBufferIndex mNextChunkRangeStart = 1;

  // Index to the first block.
  // Atomic because it may be increased when a Chunk is destroyed, and the
  // callback may be invoked from anywhere, including from inside one of our
  // locked section, so we cannot protect it with a mutex.
  Atomic<ProfileBufferIndex, MemoryOrdering::ReleaseAcquire> mRangeStart{1};

  // Index past the last block. Equals mRangeStart if empty.
  ProfileBufferIndex mRangeEnd = 1;

  // Number of blocks that have been pushed into this buffer.
  uint64_t mPushedBlockCount = 0;

  // Number of blocks that have been removed from this buffer.
  // Note: Live entries = pushed - cleared.
  // Atomic because it may be updated when a Chunk is destroyed, and the
  // callback may be invoked from anywhere, including from inside one of our
  // locked section, so we cannot protect it with a mutex.
  Atomic<uint64_t, MemoryOrdering::ReleaseAcquire> mClearedBlockCount{0};
};

}  // namespace mozilla

#endif  // ProfileChunkedBuffer_h
