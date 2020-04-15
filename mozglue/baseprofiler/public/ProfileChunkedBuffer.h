/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProfileChunkedBuffer_h
#define ProfileChunkedBuffer_h

#include "mozilla/BaseProfilerDetail.h"
#include "mozilla/ProfileBufferChunkManager.h"
#include "mozilla/ProfileBufferEntrySerialization.h"
#include "mozilla/RefCounted.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Unused.h"

#include <cstdio>
#include <utility>

namespace mozilla {

namespace detail {

// Internal accessor pointing at a position inside a chunk.
// It can handle two groups of chunks (typically the extant chunks stored in
// the store manager, and the current chunk).
// The main operations are:
// - ReadEntrySize() to read an entry size, 0 means failure.
// - operator+=(Length) to skip a number of bytes.
// - EntryReader() creates an entry reader at the current position for a given
//   size (it may fail with an empty reader), and skips the entry.
// Note that there is no "past-the-end" position -- as soon as InChunkPointer
// reaches the end, it becomes effectively null.
class InChunkPointer {
 public:
  using Byte = ProfileBufferChunk::Byte;
  using Length = ProfileBufferChunk::Length;

  // Nullptr-like InChunkPointer, may be used as end iterator.
  InChunkPointer()
      : mChunk(nullptr), mNextChunkGroup(nullptr), mOffsetInChunk(0) {}

  // InChunkPointer over one or two chunk groups, will start at the first
  // block (if any).
  InChunkPointer(const ProfileBufferChunk* aChunk,
                 const ProfileBufferChunk* aNextChunkGroup,
                 ProfileBufferBlockIndex aBlockIndex = nullptr)
      : mChunk(aChunk), mNextChunkGroup(aNextChunkGroup) {
    if (mChunk) {
      mOffsetInChunk = mChunk->OffsetFirstBlock();
      Adjust();
    } else if (mNextChunkGroup) {
      mChunk = mNextChunkGroup;
      mNextChunkGroup = nullptr;
      mOffsetInChunk = mChunk->OffsetFirstBlock();
      Adjust();
    } else {
      mOffsetInChunk = 0;
    }

    // Try to advance to given position, don't worry about success.
    Unused << AdvanceToGlobalRangePosition(
        aBlockIndex.ConvertToProfileBufferIndex());
  }

  // Compute the current position in the global range.
  // 0 if null (including if we're reached the end).
  [[nodiscard]] ProfileBufferIndex GlobalRangePosition() const {
    if (!mChunk) {
      return 0;
    }
    return mChunk->RangeStart() + mOffsetInChunk;
  }

  // Move InChunkPointer forward to the block at or after the given global
  // range position.
  // 0 stays where it is (if valid already).
  [[nodiscard]] bool AdvanceToGlobalRangePosition(
      ProfileBufferIndex aPosition) {
    if (aPosition == 0) {
      // Special position '0', just stay where we are.
      // Success if this position is already valid.
      return !!mChunk;
    }
    for (;;) {
      ProfileBufferIndex currentPosition = GlobalRangePosition();
      if (currentPosition == 0) {
        // Pointer is null.
        return false;
      }
      if (aPosition <= currentPosition) {
        // At or past the requested position, stay where we are.
        return true;
      }
      if (aPosition < mChunk->RangeStart() + mChunk->OffsetPastLastBlock()) {
        // Target position is in this chunk's written space, move to it.
        for (;;) {
          // Skip the current block.
          mOffsetInChunk += ReadEntrySize();
          if (mOffsetInChunk >= mChunk->OffsetPastLastBlock()) {
            // Reached the end of the chunk, this can happen for the last
            // block, let's just continue to the next chunk.
            break;
          }
          if (aPosition <= mChunk->RangeStart() + mOffsetInChunk) {
            // We're at or after the position, return at this block position.
            return true;
          }
        }
      }
      // Position is after this chunk, try next chunk.
      GoToNextChunk();
      if (!mChunk) {
        return false;
      }
      // Skip whatever block tail there is, we don't allow pointing in the
      // middle of a block.
      mOffsetInChunk = mChunk->OffsetFirstBlock();
    }
  }

  [[nodiscard]] Byte ReadByte() {
    MOZ_ASSERT(!!mChunk);
    MOZ_ASSERT(mOffsetInChunk < mChunk->OffsetPastLastBlock());
    Byte byte = mChunk->ByteAt(mOffsetInChunk);
    if (MOZ_UNLIKELY(++mOffsetInChunk == mChunk->OffsetPastLastBlock())) {
      Adjust();
    }
    return byte;
  }

  // Read and skip a ULEB128-encoded size.
  // 0 means failure (0-byte entries are not allowed.)
  // Note that this doesn't guarantee that there are actually that many bytes
  // available to read! (EntryReader() below may gracefully fail.)
  [[nodiscard]] Length ReadEntrySize() {
    ULEB128Reader<Length> reader;
    if (!mChunk) {
      return 0;
    }
    for (;;) {
      const bool isComplete = reader.FeedByteIsComplete(ReadByte());
      if (MOZ_UNLIKELY(!mChunk)) {
        // End of chunks, so there's no actual entry after this anyway.
        return 0;
      }
      if (MOZ_LIKELY(isComplete)) {
        if (MOZ_UNLIKELY(reader.Value() > mChunk->BufferBytes())) {
          // Don't allow entries larger than a chunk.
          return 0;
        }
        return reader.Value();
      }
    }
  }

  InChunkPointer& operator+=(Length aLength) {
    MOZ_ASSERT(!!mChunk);
    mOffsetInChunk += aLength;
    Adjust();
    return *this;
  }

  [[nodiscard]] ProfileBufferEntryReader EntryReader(Length aLength) {
    if (!mChunk || aLength == 0) {
      return ProfileBufferEntryReader();
    }

    MOZ_ASSERT(mOffsetInChunk < mChunk->OffsetPastLastBlock());

    // We should be pointing at the entry, past the entry size.
    const ProfileBufferIndex entryIndex = GlobalRangePosition();
    // Verify that there's enough space before for the size (starting at index
    // 1 at least).
    MOZ_ASSERT(entryIndex >= 1u + ULEB128Size(aLength));

    const Length remaining = mChunk->OffsetPastLastBlock() - mOffsetInChunk;
    Span<const Byte> mem0 = mChunk->BufferSpan();
    mem0 = mem0.From(mOffsetInChunk);
    if (aLength <= remaining) {
      // Move to the end of this block, which could make this null if we have
      // reached the end of all buffers.
      *this += aLength;
      return ProfileBufferEntryReader(
          mem0.To(aLength),
          // Block starts before the entry size.
          ProfileBufferBlockIndex::CreateFromProfileBufferIndex(
              entryIndex - ULEB128Size(aLength)),
          // Block ends right after the entry (could be null for last entry).
          ProfileBufferBlockIndex::CreateFromProfileBufferIndex(
              GlobalRangePosition()));
    }

    // We need to go to the next chunk for the 2nd part of this block.
    GoToNextChunk();
    if (!mChunk) {
      return ProfileBufferEntryReader();
    }

    Span<const Byte> mem1 = mChunk->BufferSpan();
    const Length tail = aLength - remaining;
    MOZ_ASSERT(tail <= mChunk->BufferBytes());
    MOZ_ASSERT(tail == mChunk->OffsetFirstBlock());
    // We are in the correct chunk, move the offset to the end of the block.
    mOffsetInChunk = tail;
    // And adjust as needed, which could make this null if we have reached the
    // end of all buffers.
    Adjust();
    return ProfileBufferEntryReader(
        mem0, mem1.To(tail),
        // Block starts before the entry size.
        ProfileBufferBlockIndex::CreateFromProfileBufferIndex(
            entryIndex - ULEB128Size(aLength)),
        // Block ends right after the entry (could be null for last entry).
        ProfileBufferBlockIndex::CreateFromProfileBufferIndex(
            GlobalRangePosition()));
  }

  explicit operator bool() const { return !!mChunk; }

  [[nodiscard]] bool operator!() const { return !mChunk; }

  [[nodiscard]] bool operator==(const InChunkPointer& aOther) const {
    if (!*this || !aOther) {
      return !*this && !aOther;
    }
    return mChunk == aOther.mChunk && mOffsetInChunk == aOther.mOffsetInChunk;
  }

  [[nodiscard]] bool operator!=(const InChunkPointer& aOther) const {
    if (!*this || !aOther) {
      return !!*this || !!aOther;
    }
    return mChunk != aOther.mChunk || mOffsetInChunk != aOther.mOffsetInChunk;
  }

  [[nodiscard]] Byte operator*() const {
    MOZ_ASSERT(!!mChunk);
    MOZ_ASSERT(mOffsetInChunk < mChunk->OffsetPastLastBlock());
    return mChunk->ByteAt(mOffsetInChunk);
  }

  InChunkPointer& operator++() {
    MOZ_ASSERT(!!mChunk);
    MOZ_ASSERT(mOffsetInChunk < mChunk->OffsetPastLastBlock());
    if (MOZ_UNLIKELY(++mOffsetInChunk == mChunk->OffsetPastLastBlock())) {
      mOffsetInChunk = 0;
      GoToNextChunk();
      Adjust();
    }
    return *this;
  }

 private:
  void GoToNextChunk() {
    MOZ_ASSERT(!!mChunk);
    const ProfileBufferIndex expectedNextRangeStart =
        mChunk->RangeStart() + mChunk->BufferBytes();

    mChunk = mChunk->GetNext();
    if (!mChunk) {
      // Reached the end of the current chunk group, try the next one (which
      // may be null too, especially on the 2nd try).
      mChunk = mNextChunkGroup;
      mNextChunkGroup = nullptr;
    }

    if (mChunk && mChunk->RangeStart() == 0) {
      // Reached a chunk without a valid (non-null) range start, assume there
      // are only unused chunks from here on.
      mChunk = nullptr;
    }

    MOZ_ASSERT(!mChunk || mChunk->RangeStart() == expectedNextRangeStart,
               "We don't handle discontinuous buffers (yet)");
    // Non-DEBUG fallback: Stop reading past discontinuities.
    // (They should be rare, only happening on temporary OOMs.)
    // TODO: Handle discontinuities (by skipping over incomplete blocks).
    if (mChunk && mChunk->RangeStart() != expectedNextRangeStart) {
      mChunk = nullptr;
    }
  }

  // We want `InChunkPointer` to always point at a valid byte (or be null).
  // After some operations, `mOffsetInChunk` may point past the end of the
  // current `mChunk`, in which case we need to adjust our position to be inside
  // the appropriate chunk. E.g., if we're 10 bytes after the end of the current
  // chunk, we should end up at offset 10 in the next chunk.
  // Note that we may "fall off" the last chunk and make this `InChunkPointer`
  // effectively null.
  void Adjust() {
    while (mChunk && mOffsetInChunk >= mChunk->OffsetPastLastBlock()) {
      // TODO: Try to adjust offset between chunks relative to mRangeStart
      // differences. But we don't handle discontinuities yet.
      if (mOffsetInChunk < mChunk->BufferBytes()) {
        mOffsetInChunk -= mChunk->BufferBytes();
      } else {
        mOffsetInChunk -= mChunk->OffsetPastLastBlock();
      }
      GoToNextChunk();
    }
  }

  const ProfileBufferChunk* mChunk;
  const ProfileBufferChunk* mNextChunkGroup;
  Length mOffsetInChunk;
};

}  // namespace detail

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

  // Reserve a block that can hold an entry of the given `aCallbackEntryBytes()`
  // size, write the entry size (ULEB128-encoded), and invoke and return
  // `aCallback(ProfileBufferEntryWriter*)`.
  // Note: `aCallbackEntryBytes` is a callback instead of a simple value, to
  // delay this potentially-expensive computation until after we're checked that
  // we're in-session; use `Put(Length, Callback)` below if you know the size
  // already.
  template <typename CallbackEntryBytes, typename Callback>
  auto ReserveAndPut(CallbackEntryBytes&& aCallbackEntryBytes,
                     Callback&& aCallback)
      -> decltype(std::forward<Callback>(aCallback)(
          std::declval<ProfileBufferEntryWriter*>())) {
    baseprofiler::detail::BaseProfilerMaybeAutoLock lock(mMutex);

    // This can only be read in the 2nd lambda below after it has been written
    // by the first lambda.
    Length entryBytes;

    return ReserveAndPutRaw(
        [&]() {
          entryBytes = std::forward<CallbackEntryBytes>(aCallbackEntryBytes)();
          MOZ_ASSERT(entryBytes != 0, "Empty entries are not allowed");
          return ULEB128Size(entryBytes) + entryBytes;
        },
        [&](ProfileBufferEntryWriter* aEntryWriter) {
          if (aEntryWriter) {
            aEntryWriter->WriteULEB128(entryBytes);
            MOZ_ASSERT(aEntryWriter->RemainingBytes() == entryBytes);
          }
          return std::forward<Callback>(aCallback)(aEntryWriter);
        },
        lock);
  }

  template <typename Callback>
  auto Put(Length aEntryBytes, Callback&& aCallback) {
    return ReserveAndPut([aEntryBytes]() { return aEntryBytes; },
                         std::forward<Callback>(aCallback));
  }

  // Add a new entry copied from the given buffer, return block index.
  ProfileBufferBlockIndex PutFrom(const void* aSrc, Length aBytes) {
    return ReserveAndPut(
        [aBytes]() { return aBytes; },
        [aSrc, aBytes](ProfileBufferEntryWriter* aEntryWriter) {
          if (!aEntryWriter) {
            return ProfileBufferBlockIndex{};
          }
          aEntryWriter->WriteBytes(aSrc, aBytes);
          return aEntryWriter->CurrentBlockIndex();
        });
  }

  // Add a new single entry with *all* given object (using a Serializer for
  // each), return block index.
  template <typename... Ts>
  ProfileBufferBlockIndex PutObjects(const Ts&... aTs) {
    static_assert(sizeof...(Ts) > 0,
                  "PutObjects must be given at least one object.");
    return ReserveAndPut(
        [&]() { return ProfileBufferEntryWriter::SumBytes(aTs...); },
        [&](ProfileBufferEntryWriter* aEntryWriter) {
          if (!aEntryWriter) {
            return ProfileBufferBlockIndex{};
          }
          aEntryWriter->WriteObjects(aTs...);
          return aEntryWriter->CurrentBlockIndex();
        });
  }

  // Add a new entry copied from the given object, return block index.
  template <typename T>
  ProfileBufferBlockIndex PutObject(const T& aOb) {
    return PutObjects(aOb);
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
    Unused << HandleRequestedChunk_IsPending(lock);
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
    switch (mRequestedChunkHolder->GetState()) {
      case RequestedChunkRefCountedHolder::State::Unused:
        fprintf(aFile, " - No request pending.\n");
        break;
      case RequestedChunkRefCountedHolder::State::Requested:
        fprintf(aFile, " - Request pending.\n");
        break;
      case RequestedChunkRefCountedHolder::State::Fulfilled:
        fprintf(aFile, " - Request fulfilled.\n");
        break;
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
      mRequestedChunkHolder = nullptr;
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

    // We start with one chunk right away, and request a following one now
    // so it should be available before the current chunk is full.
    SetAndInitializeCurrentChunk(mChunkManager->GetChunk(), aLock);
    mRequestedChunkHolder = MakeRefPtr<RequestedChunkRefCountedHolder>();
    RequestChunk(aLock);
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

  void RequestChunk(
      const baseprofiler::detail::BaseProfilerMaybeAutoLock& aLock) {
    if (HandleRequestedChunk_IsPending(aLock)) {
      // There is already a pending request, don't start a new one.
      return;
    }

    // Ensure the `RequestedChunkHolder` knows we're starting a request.
    mRequestedChunkHolder->StartRequest();

    // Request a chunk, the callback carries a `RefPtr` of the
    // `RequestedChunkHolder`, so it's guaranteed to live until it's invoked,
    // even if this `ProfileChunkedBuffer` changes its `ChunkManager` or is
    // destroyed.
    mChunkManager->RequestChunk(
        [requestedChunkHolder = RefPtr<RequestedChunkRefCountedHolder>(
             mRequestedChunkHolder)](UniquePtr<ProfileBufferChunk> aChunk) {
          requestedChunkHolder->AddRequestedChunk(std::move(aChunk));
        });
  }

  [[nodiscard]] bool HandleRequestedChunk_IsPending(
      const baseprofiler::detail::BaseProfilerMaybeAutoLock& aLock) {
    MOZ_ASSERT(!!mChunkManager);
    MOZ_ASSERT(!!mRequestedChunkHolder);

    if (mRequestedChunkHolder->GetState() ==
        RequestedChunkRefCountedHolder::State::Unused) {
      return false;
    }

    // A request is either in-flight or fulfilled.
    Maybe<UniquePtr<ProfileBufferChunk>> maybeChunk =
        mRequestedChunkHolder->GetChunkIfFulfilled();
    if (maybeChunk.isNothing()) {
      // Request is still pending.
      return true;
    }

    // Since we extracted the provided chunk, the holder should now be unused.
    MOZ_ASSERT(mRequestedChunkHolder->GetState() ==
               RequestedChunkRefCountedHolder::State::Unused);

    // Request has been fulfilled.
    UniquePtr<ProfileBufferChunk>& chunk = *maybeChunk;
    if (chunk) {
      // Try to use as current chunk if needed.
      if (!mCurrentChunk) {
        SetAndInitializeCurrentChunk(std::move(chunk), aLock);
        // We've just received a chunk and made it current, request a next chunk
        // for later.
        MOZ_ASSERT(!mNextChunks);
        RequestChunk(aLock);
        return true;
      }

      if (!mNextChunks) {
        mNextChunks = std::move(chunk);
      } else {
        mNextChunks->InsertNext(std::move(chunk));
      }
    }

    return false;
  }

  // Get a pointer to the next chunk available
  [[nodiscard]] ProfileBufferChunk* GetOrCreateCurrentChunk(
      const baseprofiler::detail::BaseProfilerMaybeAutoLock& aLock) {
    ProfileBufferChunk* current = mCurrentChunk.get();
    if (MOZ_UNLIKELY(!current)) {
      // No current chunk ready.
      MOZ_ASSERT(!mNextChunks,
                 "There shouldn't be next chunks when there is no current one");
      // See if a request has recently been fulfilled, ignore pending status.
      Unused << HandleRequestedChunk_IsPending(aLock);
      current = mCurrentChunk.get();
      if (MOZ_UNLIKELY(!current)) {
        // There was no pending chunk, try to get one right now.
        // This may still fail, but we can't do anything else about it, the
        // caller must handle the nullptr case.
        // Attempt a request for later.
        SetAndInitializeCurrentChunk(mChunkManager->GetChunk(), aLock);
        current = mCurrentChunk.get();
      }
    }
    return current;
  }

  // Get a pointer to the next chunk available
  [[nodiscard]] ProfileBufferChunk* GetOrCreateNextChunk(
      const baseprofiler::detail::BaseProfilerMaybeAutoLock& aLock) {
    MOZ_ASSERT(!!mCurrentChunk,
               "Why ask for a next chunk when there isn't even a current one?");
    ProfileBufferChunk* next = mNextChunks.get();
    if (MOZ_UNLIKELY(!next)) {
      // No next chunk ready, see if a request has recently been fulfilled,
      // ignore pending status.
      Unused << HandleRequestedChunk_IsPending(aLock);
      next = mNextChunks.get();
      if (MOZ_UNLIKELY(!next)) {
        // There was no pending chunk, try to get one right now.
        mNextChunks = mChunkManager->GetChunk();
        next = mNextChunks.get();
        // This may still fail, but we can't do anything else about it, the
        // caller must handle the nullptr case.
        if (MOZ_UNLIKELY(!next)) {
          // Attempt a request for later.
          RequestChunk(aLock);
        }
      }
    }
    return next;
  }

  // Reserve a block of `aCallbackBlockBytes()` size, and invoke and return
  // `aCallback(ProfileBufferEntryWriter*)`. Note that this is the "raw" version
  // that doesn't write the entry size at the beginning of the block.
  // Note: `aCallbackBlockBytes` is a callback instead of a simple value, to
  // delay this potentially-expensive computation until after we're checked that
  // we're in-session; use `Put(Length, Callback)` below if you know the size
  // already.
  template <typename CallbackBlockBytes, typename Callback>
  auto ReserveAndPutRaw(CallbackBlockBytes&& aCallbackBlockBytes,
                        Callback&& aCallback,
                        baseprofiler::detail::BaseProfilerMaybeAutoLock& aLock,
                        uint64_t aBlockCount = 1) {
    // The current chunk will be filled if we need to write more than its
    // remaining space.
    bool currentChunkFilled = false;

    // If the current chunk gets filled, we may or may not initialize the next
    // chunk!
    bool nextChunkInitialized = false;

    // The entry writer that will point into one or two chunks to write
    // into, empty by default (failure).
    ProfileBufferEntryWriter entryWriter;

    // After we invoke the callback and return, we may need to handle the
    // current chunk being filled.
    auto handleFilledChunk = MakeScopeExit([&]() {
      // If the entry writer was not already empty, the callback *must* have
      // filled the full entry.
      MOZ_ASSERT(entryWriter.RemainingBytes() == 0);

      if (currentChunkFilled) {
        // Extract current (now filled) chunk.
        UniquePtr<ProfileBufferChunk> filled = std::move(mCurrentChunk);

        if (mNextChunks) {
          // Cycle to the next chunk.
          mCurrentChunk =
              std::exchange(mNextChunks, mNextChunks->ReleaseNext());

          // Make sure it is initialized (it is now the current chunk).
          if (!nextChunkInitialized) {
            InitializeCurrentChunk(aLock);
          }
        }

        // And finally mark filled chunk done and release it.
        filled->MarkDone();
        mChunkManager->ReleaseChunks(std::move(filled));

        // Request another chunk if needed.
        // In most cases, here we should have one current chunk and no next
        // chunk, so we want to do a request so there hopefully will be a next
        // chunk available when the current one gets filled.
        // But we also for a request if we don't even have a current chunk (if
        // it's too late, it's ok because the next `ReserveAndPutRaw` wil just
        // allocate one on the spot.)
        // And if we already have a next chunk, there's no need for more now.
        if (!mCurrentChunk || !mNextChunks) {
          RequestChunk(aLock);
        }
      }
    });

    if (MOZ_LIKELY(mChunkManager)) {
      // In-session.

      if (ProfileBufferChunk* current = GetOrCreateCurrentChunk(aLock);
          MOZ_LIKELY(current)) {
        const Length blockBytes =
            std::forward<CallbackBlockBytes>(aCallbackBlockBytes)();
        if (blockBytes <= current->RemainingBytes()) {
          // Block fits in current chunk with only one span.
          currentChunkFilled = blockBytes == current->RemainingBytes();
          const auto [mem0, blockIndex] = current->ReserveBlock(blockBytes);
          MOZ_ASSERT(mem0.LengthBytes() == blockBytes);
          entryWriter.Set(
              mem0, blockIndex,
              ProfileBufferBlockIndex::CreateFromProfileBufferIndex(
                  blockIndex.ConvertToProfileBufferIndex() + blockBytes));
        } else {
          // Block doesn't fit fully in current chunk, it needs to overflow into
          // the next one.
          // Make sure the next chunk is available (from a previous request),
          // otherwise create one on the spot.
          if (ProfileBufferChunk* next = GetOrCreateNextChunk(aLock);
              MOZ_LIKELY(next)) {
            // Here, we know we have a current and a next chunk.
            // Reserve head of block at the end of the current chunk.
            const auto [mem0, blockIndex] =
                current->ReserveBlock(current->RemainingBytes());
            MOZ_ASSERT(mem0.LengthBytes() < blockBytes);
            MOZ_ASSERT(current->RemainingBytes() == 0);
            // Set the next chunk range, and reserve the needed space for the
            // tail of the block.
            next->SetRangeStart(mNextChunkRangeStart);
            mNextChunkRangeStart += next->BufferBytes();
            const auto mem1 = next->ReserveInitialBlockAsTail(
                blockBytes - mem0.LengthBytes());
            MOZ_ASSERT(next->RemainingBytes() != 0);
            currentChunkFilled = true;
            nextChunkInitialized = true;
            // Block is split in two spans.
            entryWriter.Set(
                mem0, mem1, blockIndex,
                ProfileBufferBlockIndex::CreateFromProfileBufferIndex(
                    blockIndex.ConvertToProfileBufferIndex() + blockBytes));
          }
        }

        // Here, we either have an empty `entryWriter` (failure), or a non-empty
        // writer pointing at the start of the block.

        // Remember that following the final `return` below, `handleFilledChunk`
        // will take care of releasing the current chunk, and cycling to the
        // next one, if needed.

        if (MOZ_LIKELY(entryWriter.RemainingBytes() != 0)) {
          // `entryWriter` is not empty, record some stats and let the user
          // write their data in the entry.
          MOZ_ASSERT(entryWriter.RemainingBytes() == blockBytes);
          mRangeEnd += blockBytes;
          mPushedBlockCount += aBlockCount;
          return std::forward<Callback>(aCallback)(&entryWriter);
        }

        // If we're here, `entryWriter` was empty (probably because we couldn't
        // get a next chunk to write into), we just fall back to the `nullptr`
        // case below...

      }  // end of `if (current)`
    }    // end of `if (mChunkManager)`

    return std::forward<Callback>(aCallback)(nullptr);
  }

  // Reserve a block of `aBlockBytes` size, and invoke and return
  // `aCallback(ProfileBufferEntryWriter*)`. Note that this is the "raw" version
  // that doesn't write the entry size at the beginning of the block.
  template <typename Callback>
  auto ReserveAndPutRaw(Length aBlockBytes, Callback&& aCallback,
                        uint64_t aBlockCount) {
    baseprofiler::detail::BaseProfilerMaybeAutoLock lock(mMutex);
    return ReserveAndPutRaw([aBlockBytes]() { return aBlockBytes; },
                            std::forward<Callback>(aCallback), lock,
                            aBlockCount);
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

  // Class used to transfer requested chunks from a `ChunkManager` to a
  // `ProfileChunkedBuffer`.
  // It needs to be ref-counted because the request may be fulfilled
  // asynchronously, and either side may be destroyed during the request.
  // It cannot use the `ProfileChunkedBuffer` mutex, because that buffer and its
  // mutex could be destroyed during the request.
  class RequestedChunkRefCountedHolder
      : public external::AtomicRefCounted<RequestedChunkRefCountedHolder> {
   public:
    MOZ_DECLARE_REFCOUNTED_TYPENAME(RequestedChunkRefCountedHolder)

    enum class State { Unused, Requested, Fulfilled };

    // Get the current state. Note that it may change after the function
    // returns, so it should be used carefully, e.g., `ProfileChunkedBuffer` can
    // see if a request is pending or fulfilled, to avoid starting another
    // request.
    [[nodiscard]] State GetState() const {
      baseprofiler::detail::BaseProfilerAutoLock lock(mRequestMutex);
      return mState;
    }

    // Must be called by `ProfileChunkedBuffer` when it requests a chunk.
    // There cannot be more than one request in-flight.
    void StartRequest() {
      baseprofiler::detail::BaseProfilerAutoLock lock(mRequestMutex);
      MOZ_ASSERT(mState == State::Unused, "Already requested or fulfilled");
      mState = State::Requested;
    }

    // Must be called by the `ChunkManager` with a chunk.
    // If the `ChunkManager` cannot provide a chunk (because of memory limits,
    // or it gets destroyed), it must call this anyway with a nullptr.
    void AddRequestedChunk(UniquePtr<ProfileBufferChunk>&& aChunk) {
      baseprofiler::detail::BaseProfilerAutoLock lock(mRequestMutex);
      MOZ_ASSERT(mState == State::Requested);
      mState = State::Fulfilled;
      mRequestedChunk = std::move(aChunk);
    }

    // The `ProfileChunkedBuffer` can try to extract the provided chunk after a
    // request:
    // - Nothing -> Request is not fulfilled yet.
    // - Some(nullptr) -> The `ChunkManager` was not able to provide a chunk.
    // - Some(chunk) -> Requested chunk.
    [[nodiscard]] Maybe<UniquePtr<ProfileBufferChunk>> GetChunkIfFulfilled() {
      Maybe<UniquePtr<ProfileBufferChunk>> maybeChunk;
      baseprofiler::detail::BaseProfilerAutoLock lock(mRequestMutex);
      MOZ_ASSERT(mState == State::Requested || mState == State::Fulfilled);
      if (mState == State::Fulfilled) {
        mState = State::Unused;
        maybeChunk.emplace(std::move(mRequestedChunk));
      }
      return maybeChunk;
    }

   private:
    // Mutex guarding the following members.
    mutable baseprofiler::detail::BaseProfilerMutex mRequestMutex;
    State mState = State::Unused;
    UniquePtr<ProfileBufferChunk> mRequestedChunk;
  };

  // Requested-chunk holder, kept alive when in-session, but may also live
  // longer if a request is in-flight.
  RefPtr<RequestedChunkRefCountedHolder> mRequestedChunkHolder;

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
