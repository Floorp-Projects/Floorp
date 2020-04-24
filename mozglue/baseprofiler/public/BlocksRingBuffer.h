/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BlocksRingBuffer_h
#define BlocksRingBuffer_h

#include "mozilla/BaseProfilerDetail.h"
#include "mozilla/ModuloBuffer.h"
#include "mozilla/ProfileBufferIndex.h"
#include "mozilla/ScopeExit.h"

#include <functional>
#include <string>
#include <tuple>
#include <utility>

namespace mozilla {

// Thread-safe Ring buffer that can store blocks of different sizes during
// defined sessions.
// Each *block* contains an *entry* and the entry size:
// [ entry_size | entry ] [ entry_size | entry ] ...
// *In-session* is a period of time during which `BlocksRingBuffer` allows
// reading and writing. *Out-of-session*, the `BlocksRingBuffer` object is
// still valid, but contains no data, and gracefully denies accesses.
//
// To write an entry, the buffer reserves a block of sufficient size (to contain
// user data of predetermined size), writes the entry size, and lets the caller
// fill the entry contents using ModuloBuffer::Iterator APIs and a few entry-
// specific APIs. E.g.:
// ```
// BlockRingsBuffer brb(PowerOfTwo<BlockRingsBuffer::Length>(1024));
// brb.ReserveAndPut([]() { return sizeof(123); },
//                   [&](ProfileBufferEntryWriter& aEW) {
//                     aEW.WriteObject(123);
//                   });
// ```
// Other `Put...` functions may be used as shortcuts for simple entries.
// The objects given to the caller's callbacks should only be used inside the
// callbacks and not stored elsewhere, because they keep their own references to
// the BlocksRingBuffer and therefore should not live longer.
// Different type of objects may be serialized into an entry, see `Serializer`
// for more information.
//
// When reading data, the buffer iterates over blocks (it knows how to read the
// entry size, and therefore move to the next block), and lets the caller read
// the entry inside of each block. E.g.:
// ```
// brb.Read([](BlocksRingBuffer::Reader aR) {}
//   for (ProfileBufferEntryReader aER : aR) {
//     /* Use ProfileBufferEntryReader functions to read serialized objects. */
//     int n = aER.ReadObject<int>();
//   }
// });
// ```
// Different type of objects may be deserialized from an entry, see
// `Deserializer` for more information.
//
// The caller may retrieve the `ProfileBufferBlockIndex` corresponding to an
// entry (`ProfileBufferBlockIndex` is an opaque type preventing the user from
// modifying it). That index may later be used to get back to that particular
// entry if it still exists.
class BlocksRingBuffer {
 public:
  // Using ModuloBuffer as underlying circular byte buffer.
  using Buffer = ModuloBuffer<uint32_t, ProfileBufferIndex>;
  using Byte = Buffer::Byte;

  // Length type for total buffer (as PowerOfTwo<Length>) and each entry.
  using Length = uint32_t;

  enum class ThreadSafety { WithoutMutex, WithMutex };

  // Default constructor starts out-of-session (nothing to read or write).
  explicit BlocksRingBuffer(ThreadSafety aThreadSafety)
      : mMutex(aThreadSafety != ThreadSafety::WithoutMutex) {}

  // Create a buffer of the given length.
  explicit BlocksRingBuffer(ThreadSafety aThreadSafety,
                            PowerOfTwo<Length> aLength)
      : mMutex(aThreadSafety != ThreadSafety::WithoutMutex),
        mMaybeUnderlyingBuffer(Some(UnderlyingBuffer(aLength))) {}

  // Take ownership of an existing buffer.
  BlocksRingBuffer(ThreadSafety aThreadSafety,
                   UniquePtr<Buffer::Byte[]> aExistingBuffer,
                   PowerOfTwo<Length> aLength)
      : mMutex(aThreadSafety != ThreadSafety::WithoutMutex),
        mMaybeUnderlyingBuffer(
            Some(UnderlyingBuffer(std::move(aExistingBuffer), aLength))) {}

  // Use an externally-owned buffer.
  BlocksRingBuffer(ThreadSafety aThreadSafety, Buffer::Byte* aExternalBuffer,
                   PowerOfTwo<Length> aLength)
      : mMutex(aThreadSafety != ThreadSafety::WithoutMutex),
        mMaybeUnderlyingBuffer(
            Some(UnderlyingBuffer(aExternalBuffer, aLength))) {}

  // Destructor doesn't need to do anything special. (Clearing entries would
  // only update indices and stats, which won't be accessible after the object
  // is destroyed anyway.)
  ~BlocksRingBuffer() = default;

  // Remove underlying buffer, if any.
  void Reset() {
    baseprofiler::detail::BaseProfilerMaybeAutoLock lock(mMutex);
    ResetUnderlyingBuffer();
  }

  // Create a buffer of the given length.
  void Set(PowerOfTwo<Length> aLength) {
    baseprofiler::detail::BaseProfilerMaybeAutoLock lock(mMutex);
    ResetUnderlyingBuffer();
    mMaybeUnderlyingBuffer.emplace(aLength);
  }

  // Take ownership of an existing buffer.
  void Set(UniquePtr<Buffer::Byte[]> aExistingBuffer,
           PowerOfTwo<Length> aLength) {
    baseprofiler::detail::BaseProfilerMaybeAutoLock lock(mMutex);
    ResetUnderlyingBuffer();
    mMaybeUnderlyingBuffer.emplace(std::move(aExistingBuffer), aLength);
  }

  // Use an externally-owned buffer.
  void Set(Buffer::Byte* aExternalBuffer, PowerOfTwo<Length> aLength) {
    baseprofiler::detail::BaseProfilerMaybeAutoLock lock(mMutex);
    ResetUnderlyingBuffer();
    mMaybeUnderlyingBuffer.emplace(aExternalBuffer, aLength);
  }

  // This cannot change during the lifetime of this buffer, so there's no need
  // to lock.
  bool IsThreadSafe() const { return mMutex.IsActivated(); }

  [[nodiscard]] bool IsInSession() const {
    baseprofiler::detail::BaseProfilerMaybeAutoLock lock(mMutex);
    return !!mMaybeUnderlyingBuffer;
  }

  // Lock the buffer mutex and run the provided callback.
  // This can be useful when the caller needs to explicitly lock down this
  // buffer, but not do anything else with it.
  template <typename Callback>
  auto LockAndRun(Callback&& aCallback) const {
    baseprofiler::detail::BaseProfilerMaybeAutoLock lock(mMutex);
    return std::forward<Callback>(aCallback)();
  }

  // Buffer length in bytes.
  Maybe<PowerOfTwo<Length>> BufferLength() const {
    baseprofiler::detail::BaseProfilerMaybeAutoLock lock(mMutex);
    return mMaybeUnderlyingBuffer.map([](const UnderlyingBuffer& aBuffer) {
      return aBuffer.mBuffer.BufferLength();
    });
    ;
  }

  // Size of external resources.
  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
    if (!mMaybeUnderlyingBuffer) {
      return 0;
    }
    return mMaybeUnderlyingBuffer->mBuffer.SizeOfExcludingThis(aMallocSizeOf);
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  // Snapshot of the buffer state.
  struct State {
    // Index to the first block.
    ProfileBufferBlockIndex mRangeStart;

    // Index past the last block. Equals mRangeStart if empty.
    ProfileBufferBlockIndex mRangeEnd;

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
  State GetState() const {
    baseprofiler::detail::BaseProfilerMaybeAutoLock lock(mMutex);
    return {
        mFirstReadIndex, mNextWriteIndex,
        mMaybeUnderlyingBuffer ? mMaybeUnderlyingBuffer->mPushedBlockCount : 0,
        mMaybeUnderlyingBuffer ? mMaybeUnderlyingBuffer->mClearedBlockCount
                               : 0};
  }

  class Reader;

  // Class that can iterate through blocks and provide
  // `ProfileBufferEntryReader`s.
  // Created through `Reader`, lives within a lock guard lifetime.
  class BlockIterator {
   public:
#ifdef DEBUG
    ~BlockIterator() {
      // No BlockIterator should live outside of a mutexed call.
      mRing->mMutex.AssertCurrentThreadOwns();
    }
#endif  // DEBUG

    // Comparison with other iterator, mostly used in range-for loops.
    bool operator==(const BlockIterator aRhs) const {
      MOZ_ASSERT(mRing == aRhs.mRing);
      return mBlockIndex == aRhs.mBlockIndex;
    }
    bool operator!=(const BlockIterator aRhs) const {
      MOZ_ASSERT(mRing == aRhs.mRing);
      return mBlockIndex != aRhs.mBlockIndex;
    }

    // Advance to next BlockIterator.
    BlockIterator& operator++() {
      mBlockIndex = NextBlockIndex();
      return *this;
    }

    // Dereferencing creates a `ProfileBufferEntryReader` for the entry inside
    // this block.
    ProfileBufferEntryReader operator*() const {
      return mRing->ReaderInBlockAt(mBlockIndex);
    }

    // True if this iterator is just past the last entry.
    bool IsAtEnd() const {
      MOZ_ASSERT(mBlockIndex <= BufferRangeEnd());
      return mBlockIndex == BufferRangeEnd();
    }

    // Can be used as reference to come back to this entry with `ReadAt()`.
    ProfileBufferBlockIndex CurrentBlockIndex() const { return mBlockIndex; }

    // Index past the end of this block, which is the start of the next block.
    ProfileBufferBlockIndex NextBlockIndex() const {
      MOZ_ASSERT(!IsAtEnd());
      const Length entrySize =
          mRing->ReaderInBlockAt(mBlockIndex).RemainingBytes();
      return ProfileBufferBlockIndex::CreateFromProfileBufferIndex(
          mBlockIndex.ConvertToProfileBufferIndex() + ULEB128Size(entrySize) +
          entrySize);
    }

    // Index of the first block in the whole buffer.
    ProfileBufferBlockIndex BufferRangeStart() const {
      return mRing->mFirstReadIndex;
    }

    // Index past the last block in the whole buffer.
    ProfileBufferBlockIndex BufferRangeEnd() const {
      return mRing->mNextWriteIndex;
    }

   private:
    // Only a Reader can instantiate a BlockIterator.
    friend class Reader;

    BlockIterator(const BlocksRingBuffer& aRing,
                  ProfileBufferBlockIndex aBlockIndex)
        : mRing(WrapNotNull(&aRing)), mBlockIndex(aBlockIndex) {
      // No BlockIterator should live outside of a mutexed call.
      mRing->mMutex.AssertCurrentThreadOwns();
    }

    // Using a non-null pointer instead of a reference, to allow copying.
    // This BlockIterator should only live inside one of the thread-safe
    // BlocksRingBuffer functions, for this reference to stay valid.
    NotNull<const BlocksRingBuffer*> mRing;
    ProfileBufferBlockIndex mBlockIndex;
  };

  // Class that can create `BlockIterator`s (e.g., for range-for), or just
  // iterate through entries; lives within a lock guard lifetime.
  class MOZ_RAII Reader {
   public:
    Reader(const Reader&) = delete;
    Reader& operator=(const Reader&) = delete;
    Reader(Reader&&) = delete;
    Reader& operator=(Reader&&) = delete;

#ifdef DEBUG
    ~Reader() {
      // No Reader should live outside of a mutexed call.
      mRing.mMutex.AssertCurrentThreadOwns();
    }
#endif  // DEBUG

    // Index of the first block in the whole buffer.
    ProfileBufferBlockIndex BufferRangeStart() const {
      return mRing.mFirstReadIndex;
    }

    // Index past the last block in the whole buffer.
    ProfileBufferBlockIndex BufferRangeEnd() const {
      return mRing.mNextWriteIndex;
    }

    // Iterators to the first and past-the-last blocks.
    // Compatible with range-for (see `ForEach` below as example).
    BlockIterator begin() const {
      return BlockIterator(mRing, BufferRangeStart());
    }
    // Note that a `BlockIterator` at the `end()` should not be dereferenced, as
    // there is no actual block there!
    BlockIterator end() const { return BlockIterator(mRing, BufferRangeEnd()); }

    // Get a `BlockIterator` at the given `ProfileBufferBlockIndex`, clamped to
    // the stored range. Note that a `BlockIterator` at the `end()` should not
    // be dereferenced, as there is no actual block there!
    BlockIterator At(ProfileBufferBlockIndex aBlockIndex) const {
      if (aBlockIndex < BufferRangeStart()) {
        // Anything before the range (including null ProfileBufferBlockIndex) is
        // clamped at the beginning.
        return begin();
      }
      // Otherwise we at least expect the index to be valid (pointing exactly at
      // a live block, or just past the end.)
      mRing.AssertBlockIndexIsValidOrEnd(aBlockIndex);
      return BlockIterator(mRing, aBlockIndex);
    }

    // Run `aCallback(ProfileBufferEntryReader&)` on each entry from first to
    // last. Callback should not store `ProfileBufferEntryReader`, as it may
    // become invalid after this thread-safe call.
    template <typename Callback>
    void ForEach(Callback&& aCallback) const {
      for (ProfileBufferEntryReader reader : *this) {
        aCallback(reader);
      }
    }

   private:
    friend class BlocksRingBuffer;

    explicit Reader(const BlocksRingBuffer& aRing) : mRing(aRing) {
      // No Reader should live outside of a mutexed call.
      mRing.mMutex.AssertCurrentThreadOwns();
    }

    // This Reader should only live inside one of the thread-safe
    // BlocksRingBuffer functions, for this reference to stay valid.
    const BlocksRingBuffer& mRing;
  };

  // Call `aCallback(BlocksRingBuffer::Reader*)` (nullptr when out-of-session),
  // and return whatever `aCallback` returns. Callback should not store
  // `Reader`, because it may become invalid after this call.
  template <typename Callback>
  auto Read(Callback&& aCallback) const {
    {
      baseprofiler::detail::BaseProfilerMaybeAutoLock lock(mMutex);
      if (MOZ_LIKELY(mMaybeUnderlyingBuffer)) {
        Reader reader(*this);
        return std::forward<Callback>(aCallback)(&reader);
      }
    }
    return std::forward<Callback>(aCallback)(nullptr);
  }

  // Call `aCallback(ProfileBufferEntryReader&)` on each item.
  // Callback should not store `ProfileBufferEntryReader`, because it may become
  // invalid after this call.
  template <typename Callback>
  void ReadEach(Callback&& aCallback) const {
    Read([&](Reader* aReader) {
      if (MOZ_LIKELY(aReader)) {
        aReader->ForEach(aCallback);
      }
    });
  }

  // Call `aCallback(Maybe<ProfileBufferEntryReader>&&)` on the entry at
  // the given ProfileBufferBlockIndex; The `Maybe` will be `Nothing` if
  // out-of-session, or if that entry doesn't exist anymore, or if we've reached
  // just past the last entry. Return whatever `aCallback` returns. Callback
  // should not store `ProfileBufferEntryReader`, because it may become invalid
  // after this call.
  template <typename Callback>
  auto ReadAt(ProfileBufferBlockIndex aBlockIndex, Callback&& aCallback) const {
    baseprofiler::detail::BaseProfilerMaybeAutoLock lock(mMutex);
    MOZ_ASSERT(aBlockIndex <= mNextWriteIndex);
    Maybe<ProfileBufferEntryReader> maybeEntryReader;
    if (MOZ_LIKELY(mMaybeUnderlyingBuffer) && aBlockIndex >= mFirstReadIndex &&
        aBlockIndex < mNextWriteIndex) {
      AssertBlockIndexIsValid(aBlockIndex);
      maybeEntryReader.emplace(ReaderInBlockAt(aBlockIndex));
    }
    return std::forward<Callback>(aCallback)(std::move(maybeEntryReader));
  }

  // Main function to write entries.
  // Reserve `aCallbackBytes()` bytes, call `aCallback()` with a pointer to an
  // on-stack temporary ProfileBufferEntryWriter (nullptr when out-of-session),
  // and return whatever `aCallback` returns. Callback should not store
  // `ProfileBufferEntryWriter`, because it may become invalid after this
  // thread-safe call. Note: `aCallbackBytes` is a callback instead of a simple
  // value, to delay this potentially-expensive computation until after we're
  // checked that we're in-session; use `Put(Length, Callback)` below if you
  // know the size already.
  template <typename CallbackBytes, typename Callback>
  auto ReserveAndPut(CallbackBytes aCallbackBytes, Callback&& aCallback) {
    Maybe<ProfileBufferEntryWriter> maybeEntryWriter;

    baseprofiler::detail::BaseProfilerMaybeAutoLock lock(mMutex);

    if (MOZ_LIKELY(mMaybeUnderlyingBuffer)) {
      const Length entryBytes = std::forward<CallbackBytes>(aCallbackBytes)();
      MOZ_RELEASE_ASSERT(entryBytes > 0);
      const Length bufferBytes =
          mMaybeUnderlyingBuffer->mBuffer.BufferLength().Value();
      MOZ_RELEASE_ASSERT(entryBytes <= bufferBytes - ULEB128Size(entryBytes),
                         "Entry would wrap and overwrite itself");
      // Compute block size from the requested entry size.
      const Length blockBytes = ULEB128Size(entryBytes) + entryBytes;
      // We will put this new block at the end of the current buffer.
      const ProfileBufferIndex blockIndex =
          mNextWriteIndex.ConvertToProfileBufferIndex();
      // Compute the end of this new block.
      const ProfileBufferIndex blockEnd = blockIndex + blockBytes;
      while (blockEnd >
             mFirstReadIndex.ConvertToProfileBufferIndex() + bufferBytes) {
        // About to trample on an old block.
        ProfileBufferEntryReader reader = ReaderInBlockAt(mFirstReadIndex);
        mMaybeUnderlyingBuffer->mClearedBlockCount += 1;
        // Move the buffer reading start past this cleared block.
        mFirstReadIndex = ProfileBufferBlockIndex::CreateFromProfileBufferIndex(
            mFirstReadIndex.ConvertToProfileBufferIndex() +
            ULEB128Size(reader.RemainingBytes()) + reader.RemainingBytes());
      }
      // Store the new end of buffer.
      mNextWriteIndex =
          ProfileBufferBlockIndex::CreateFromProfileBufferIndex(blockEnd);
      mMaybeUnderlyingBuffer->mPushedBlockCount += 1;
      // Finally, let aCallback write into the entry.
      mMaybeUnderlyingBuffer->mBuffer.EntryWriterFromTo(maybeEntryWriter,
                                                        blockIndex, blockEnd);
      MOZ_ASSERT(maybeEntryWriter.isSome(),
                 "Non-empty entry should always create an EntryWriter");
      maybeEntryWriter->WriteULEB128(entryBytes);
      MOZ_ASSERT(maybeEntryWriter->RemainingBytes() == entryBytes);
    }

#ifdef DEBUG
    auto checkAllWritten = MakeScopeExit([&]() {
      MOZ_ASSERT(!maybeEntryWriter || maybeEntryWriter->RemainingBytes() == 0);
    });
#endif  // DEBUG
    return std::forward<Callback>(aCallback)(maybeEntryWriter);
  }

  // Add a new entry of known size, call `aCallback` with a pointer to a
  // temporary ProfileBufferEntryWriter (can be null when out-of-session), and
  // return whatever `aCallback` returns. Callback should not store the
  // `ProfileBufferEntryWriter`, as it may become invalid after this thread-safe
  // call.
  template <typename Callback>
  auto Put(Length aBytes, Callback&& aCallback) {
    return ReserveAndPut([aBytes]() { return aBytes; },
                         std::forward<Callback>(aCallback));
  }

  // Add a new entry copied from the given buffer, return block index.
  ProfileBufferBlockIndex PutFrom(const void* aSrc, Length aBytes) {
    return ReserveAndPut([aBytes]() { return aBytes; },
                         [&](Maybe<ProfileBufferEntryWriter>& aEntryWriter) {
                           if (MOZ_UNLIKELY(aEntryWriter.isNothing())) {
                             // Out-of-session, return "empty" index.
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
        [&](Maybe<ProfileBufferEntryWriter>& aEntryWriter) {
          if (MOZ_UNLIKELY(aEntryWriter.isNothing())) {
            // Out-of-session, return "empty" index.
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

  // Append the contents of another BlocksRingBuffer to this one.
  ProfileBufferBlockIndex AppendContents(const BlocksRingBuffer& aSrc) {
    baseprofiler::detail::BaseProfilerMaybeAutoLock lock(mMutex);

    if (MOZ_UNLIKELY(!mMaybeUnderlyingBuffer)) {
      // We are out-of-session, could not append contents.
      return ProfileBufferBlockIndex{};
    }

    baseprofiler::detail::BaseProfilerMaybeAutoLock srcLock(aSrc.mMutex);

    if (MOZ_UNLIKELY(!aSrc.mMaybeUnderlyingBuffer)) {
      // The other BRB is out-of-session, nothing to copy, we're done.
      return ProfileBufferBlockIndex{};
    }

    const ProfileBufferIndex srcStartIndex =
        aSrc.mFirstReadIndex.ConvertToProfileBufferIndex();
    const ProfileBufferIndex srcEndIndex =
        aSrc.mNextWriteIndex.ConvertToProfileBufferIndex();
    const Length bytesToCopy = static_cast<Length>(srcEndIndex - srcStartIndex);

    if (MOZ_UNLIKELY(bytesToCopy == 0)) {
      // The other BRB is empty, nothing to copy, we're done.
      return ProfileBufferBlockIndex{};
    }

    const Length bufferBytes =
        mMaybeUnderlyingBuffer->mBuffer.BufferLength().Value();

    MOZ_RELEASE_ASSERT(bytesToCopy <= bufferBytes,
                       "Entry would wrap and overwrite itself");

    // We will put all copied blocks at the end of the current buffer.
    const ProfileBufferIndex dstStartIndex =
        mNextWriteIndex.ConvertToProfileBufferIndex();
    // Compute where the copy will end...
    const ProfileBufferIndex dstEndIndex = dstStartIndex + bytesToCopy;

    while (dstEndIndex >
           mFirstReadIndex.ConvertToProfileBufferIndex() + bufferBytes) {
      // About to trample on an old block.
      ProfileBufferEntryReader reader = ReaderInBlockAt(mFirstReadIndex);
      mMaybeUnderlyingBuffer->mClearedBlockCount += 1;
      // Move the buffer reading start past this cleared block.
      mFirstReadIndex = ProfileBufferBlockIndex::CreateFromProfileBufferIndex(
          mFirstReadIndex.ConvertToProfileBufferIndex() +
          ULEB128Size(reader.RemainingBytes()) + reader.RemainingBytes());
    }

    // Store the new end of buffer.
    mNextWriteIndex =
        ProfileBufferBlockIndex::CreateFromProfileBufferIndex(dstEndIndex);
    // Update our pushed count with the number of live blocks we are copying.
    mMaybeUnderlyingBuffer->mPushedBlockCount +=
        aSrc.mMaybeUnderlyingBuffer->mPushedBlockCount -
        aSrc.mMaybeUnderlyingBuffer->mClearedBlockCount;

    auto reader = aSrc.mMaybeUnderlyingBuffer->mBuffer.EntryReaderFromTo(
        srcStartIndex, srcEndIndex, nullptr, nullptr);
    auto writer = mMaybeUnderlyingBuffer->mBuffer.EntryWriterFromTo(
        dstStartIndex, dstEndIndex);
    writer.WriteFromReader(reader, bytesToCopy);

    return ProfileBufferBlockIndex::CreateFromProfileBufferIndex(dstStartIndex);
  }

  // Clear all entries: Move read index to the end so that these entries cannot
  // be read anymore.
  void Clear() {
    baseprofiler::detail::BaseProfilerMaybeAutoLock lock(mMutex);
    ClearAllEntries();
  }

  // Clear all entries strictly before aBlockIndex, and move read index to the
  // end so that these entries cannot be read anymore.
  void ClearBefore(ProfileBufferBlockIndex aBlockIndex) {
    baseprofiler::detail::BaseProfilerMaybeAutoLock lock(mMutex);
    if (!mMaybeUnderlyingBuffer) {
      return;
    }
    // Don't accept a not-yet-written index. One-past-the-end is ok.
    MOZ_ASSERT(aBlockIndex <= mNextWriteIndex);
    if (aBlockIndex <= mFirstReadIndex) {
      // Already cleared.
      return;
    }
    if (aBlockIndex == mNextWriteIndex) {
      // Right past the end, just clear everything.
      ClearAllEntries();
      return;
    }
    // Otherwise we need to clear a subset of entries.
    AssertBlockIndexIsValid(aBlockIndex);
    // Just count skipped entries.
    Reader reader(*this);
    BlockIterator it = reader.begin();
    for (; it.CurrentBlockIndex() < aBlockIndex; ++it) {
      MOZ_ASSERT(it.CurrentBlockIndex() < reader.end().CurrentBlockIndex());
      mMaybeUnderlyingBuffer->mClearedBlockCount += 1;
    }
    MOZ_ASSERT(it.CurrentBlockIndex() == aBlockIndex);
    // Move read index to given index, so there's effectively no more entries
    // before.
    mFirstReadIndex = aBlockIndex;
  }

#ifdef DEBUG
  void Dump() const {
    baseprofiler::detail::BaseProfilerMaybeAutoLock lock(mMutex);
    if (!mMaybeUnderlyingBuffer) {
      printf("empty BlocksRingBuffer\n");
      return;
    }
    using ULL = unsigned long long;
    printf("start=%llu (%llu) end=%llu (%llu) - ",
           ULL(mFirstReadIndex.ConvertToProfileBufferIndex()),
           ULL(mFirstReadIndex.ConvertToProfileBufferIndex() &
               (mMaybeUnderlyingBuffer->mBuffer.BufferLength().Value() - 1)),
           ULL(mNextWriteIndex.ConvertToProfileBufferIndex()),
           ULL(mNextWriteIndex.ConvertToProfileBufferIndex() &
               (mMaybeUnderlyingBuffer->mBuffer.BufferLength().Value() - 1)));
    mMaybeUnderlyingBuffer->mBuffer.Dump();
  }
#endif  // DEBUG

 private:
  // In DEBUG mode, assert that `aBlockIndex` is a valid index for a live block.
  // (Not just in range, but points exactly at the start of a block.)
  // Slow, so avoid it for internal checks; this is more to check what callers
  // provide us.
  void AssertBlockIndexIsValid(ProfileBufferBlockIndex aBlockIndex) const {
#ifdef DEBUG
    mMutex.AssertCurrentThreadOwns();
    MOZ_ASSERT(aBlockIndex >= mFirstReadIndex);
    MOZ_ASSERT(aBlockIndex < mNextWriteIndex);
    // Quick check (default), or slow check (change '1' to '0') below:
#  if 1
    // Quick check that this looks like a valid block start.
    // Read the entry size at the start of the block.
    const Length entryBytes = ReaderInBlockAt(aBlockIndex).RemainingBytes();
    MOZ_ASSERT(entryBytes > 0, "Empty entries are not allowed");
    MOZ_ASSERT(
        entryBytes < mMaybeUnderlyingBuffer->mBuffer.BufferLength().Value() -
                         ULEB128Size(entryBytes),
        "Entry would wrap and overwrite itself");
    // The end of the block should be inside the live buffer range.
    MOZ_ASSERT(aBlockIndex.ConvertToProfileBufferIndex() +
                   ULEB128Size(entryBytes) + entryBytes <=
               mNextWriteIndex.ConvertToProfileBufferIndex());
#  else
    // Slow check that the index is really the start of the block.
    // This kills performances, as it reads from the first index until
    // aBlockIndex. Only use to debug issues locally.
    Reader reader(*this);
    BlockIterator it = reader.begin();
    for (; it.CurrentBlockIndex() < aBlockIndex; ++it) {
      MOZ_ASSERT(it.CurrentBlockIndex() < reader.end().CurrentBlockIndex());
    }
    MOZ_ASSERT(it.CurrentBlockIndex() == aBlockIndex);
#  endif
#endif  // DEBUG
  }

  // In DEBUG mode, assert that `aBlockIndex` is a valid index for a live block,
  // or is just past-the-end. (Not just in range, but points exactly at the
  // start of a block.) Slow, so avoid it for internal checks; this is more to
  // check what callers provide us.
  void AssertBlockIndexIsValidOrEnd(ProfileBufferBlockIndex aBlockIndex) const {
#ifdef DEBUG
    mMutex.AssertCurrentThreadOwns();
    if (aBlockIndex == mNextWriteIndex) {
      return;
    }
    AssertBlockIndexIsValid(aBlockIndex);
#endif  // DEBUG
  }

  // Create a reader for the block starting at aBlockIndex.
  ProfileBufferEntryReader ReaderInBlockAt(
      ProfileBufferBlockIndex aBlockIndex) const {
    mMutex.AssertCurrentThreadOwns();
    MOZ_ASSERT(mMaybeUnderlyingBuffer.isSome());
    MOZ_ASSERT(aBlockIndex >= mFirstReadIndex);
    MOZ_ASSERT(aBlockIndex < mNextWriteIndex);
    // Create a reader from the given index until the end of the buffer.
    ProfileBufferEntryReader reader =
        mMaybeUnderlyingBuffer->mBuffer.EntryReaderFromTo(
            aBlockIndex.ConvertToProfileBufferIndex(),
            mNextWriteIndex.ConvertToProfileBufferIndex(), nullptr, nullptr);
    // Read the block size at the beginning.
    const Length entryBytes = reader.ReadULEB128<Length>();
    // Make sure we don't overshoot the buffer.
    MOZ_RELEASE_ASSERT(entryBytes <= reader.RemainingBytes());
    ProfileBufferIndex nextBlockIndex =
        aBlockIndex.ConvertToProfileBufferIndex() + ULEB128Size(entryBytes) +
        entryBytes;
    // And reduce the reader to the entry area. Only provide a next-block-index
    // if it's not at the end of the buffer (i.e., there's an actual block
    // there).
    reader = mMaybeUnderlyingBuffer->mBuffer.EntryReaderFromTo(
        aBlockIndex.ConvertToProfileBufferIndex() + ULEB128Size(entryBytes),
        nextBlockIndex, aBlockIndex,
        (nextBlockIndex < mNextWriteIndex.ConvertToProfileBufferIndex())
            ? ProfileBufferBlockIndex::CreateFromProfileBufferIndex(
                  nextBlockIndex)
            : ProfileBufferBlockIndex{});
    return reader;
  }

  ProfileBufferEntryReader FullBufferReader() const {
    mMutex.AssertCurrentThreadOwns();
    if (!mMaybeUnderlyingBuffer) {
      return {};
    }
    return mMaybeUnderlyingBuffer->mBuffer.EntryReaderFromTo(
        mFirstReadIndex.ConvertToProfileBufferIndex(),
        mNextWriteIndex.ConvertToProfileBufferIndex(), nullptr, nullptr);
  }

  // Clear all entries: Move read index to the end so that these entries cannot
  // be read anymore.
  void ClearAllEntries() {
    mMutex.AssertCurrentThreadOwns();
    if (!mMaybeUnderlyingBuffer) {
      return;
    }
    // Mark all entries pushed so far as cleared.
    mMaybeUnderlyingBuffer->mClearedBlockCount =
        mMaybeUnderlyingBuffer->mPushedBlockCount;
    // Move read index to write index, so there's effectively no more entries
    // that can be read. (Not setting both to 0, in case user is keeping
    // `ProfileBufferBlockIndex`'es to old entries.)
    mFirstReadIndex = mNextWriteIndex;
  }

  // If there is an underlying buffer, clear all entries, and discard the
  // buffer. This BlocksRingBuffer will now gracefully reject all API calls, and
  // is in a state where a new underlying buffer may be set.
  void ResetUnderlyingBuffer() {
    mMutex.AssertCurrentThreadOwns();
    if (!mMaybeUnderlyingBuffer) {
      return;
    }
    ClearAllEntries();
    mMaybeUnderlyingBuffer.reset();
  }

  // Used to de/serialize a BlocksRingBuffer (e.g., containing a backtrace).
  friend ProfileBufferEntryWriter::Serializer<BlocksRingBuffer>;
  friend ProfileBufferEntryReader::Deserializer<BlocksRingBuffer>;
  friend ProfileBufferEntryWriter::Serializer<UniquePtr<BlocksRingBuffer>>;
  friend ProfileBufferEntryReader::Deserializer<UniquePtr<BlocksRingBuffer>>;

  // Mutex guarding the following members.
  mutable baseprofiler::detail::BaseProfilerMaybeMutex mMutex;

  struct UnderlyingBuffer {
    // Create a buffer of the given length.
    explicit UnderlyingBuffer(PowerOfTwo<Length> aLength) : mBuffer(aLength) {
      MOZ_ASSERT(aLength.Value() > ULEB128MaxSize<Length>(),
                 "Buffer should be able to contain more than a block size");
    }

    // Take ownership of an existing buffer.
    UnderlyingBuffer(UniquePtr<Buffer::Byte[]> aExistingBuffer,
                     PowerOfTwo<Length> aLength)
        : mBuffer(std::move(aExistingBuffer), aLength) {
      MOZ_ASSERT(aLength.Value() > ULEB128MaxSize<Length>(),
                 "Buffer should be able to contain more than a block size");
    }

    // Use an externally-owned buffer.
    UnderlyingBuffer(Buffer::Byte* aExternalBuffer, PowerOfTwo<Length> aLength)
        : mBuffer(aExternalBuffer, aLength) {
      MOZ_ASSERT(aLength.Value() > ULEB128MaxSize<Length>(),
                 "Buffer should be able to contain more than a block size");
    }

    // Only allow move-construction.
    UnderlyingBuffer(UnderlyingBuffer&&) = default;

    // Copies and move-assignment are explictly disallowed.
    UnderlyingBuffer(const UnderlyingBuffer&) = delete;
    UnderlyingBuffer& operator=(const UnderlyingBuffer&) = delete;
    UnderlyingBuffer& operator=(UnderlyingBuffer&&) = delete;

    // Underlying circular byte buffer.
    Buffer mBuffer;

    // Statistics.
    uint64_t mPushedBlockCount = 0;
    uint64_t mClearedBlockCount = 0;
  };

  // Underlying buffer, with stats.
  // Only valid during in-session period.
  Maybe<UnderlyingBuffer> mMaybeUnderlyingBuffer;

  // Index to the first block to be read (or cleared). Initialized to 1 because
  // 0 is reserved for the "empty" ProfileBufferBlockIndex value. Kept between
  // sessions, so that stored indices from one session will be gracefully denied
  // in future sessions.
  ProfileBufferBlockIndex mFirstReadIndex =
      ProfileBufferBlockIndex::CreateFromProfileBufferIndex(
          ProfileBufferIndex(1));
  // Index where the next new block should be allocated. Initialized to 1
  // because 0 is reserved for the "empty" ProfileBufferBlockIndex value. Kept
  // between sessions, so that stored indices from one session will be
  // gracefully denied in future sessions.
  ProfileBufferBlockIndex mNextWriteIndex =
      ProfileBufferBlockIndex::CreateFromProfileBufferIndex(
          ProfileBufferIndex(1));
};

// ----------------------------------------------------------------------------
// BlocksRingBuffer serialization

// A BlocksRingBuffer can hide another one!
// This will be used to store marker backtraces; They can be read back into a
// UniquePtr<BlocksRingBuffer>.
// Format: len (ULEB128) | start | end | buffer (len bytes) | pushed | cleared
// len==0 marks an out-of-session buffer, or empty buffer.
template <>
struct ProfileBufferEntryWriter::Serializer<BlocksRingBuffer> {
  static Length Bytes(const BlocksRingBuffer& aBuffer) {
    baseprofiler::detail::BaseProfilerMaybeAutoLock lock(aBuffer.mMutex);
    if (aBuffer.mMaybeUnderlyingBuffer.isNothing()) {
      // Out-of-session, we only need 1 byte to store a length of 0.
      return ULEB128Size<Length>(0);
    }
    const auto start = aBuffer.mFirstReadIndex.ConvertToProfileBufferIndex();
    const auto end = aBuffer.mNextWriteIndex.ConvertToProfileBufferIndex();
    const auto len = end - start;
    if (len == 0) {
      // In-session but empty, also store a length of 0.
      return ULEB128Size<Length>(0);
    }
    return ULEB128Size(len) + sizeof(start) + sizeof(end) + len +
           sizeof(aBuffer.mMaybeUnderlyingBuffer->mPushedBlockCount) +
           sizeof(aBuffer.mMaybeUnderlyingBuffer->mClearedBlockCount);
  }

  static void Write(ProfileBufferEntryWriter& aEW,
                    const BlocksRingBuffer& aBuffer) {
    baseprofiler::detail::BaseProfilerMaybeAutoLock lock(aBuffer.mMutex);
    if (aBuffer.mMaybeUnderlyingBuffer.isNothing()) {
      // Out-of-session, only store a length of 0.
      aEW.WriteULEB128<Length>(0);
      return;
    }
    const auto start = aBuffer.mFirstReadIndex.ConvertToProfileBufferIndex();
    const auto end = aBuffer.mNextWriteIndex.ConvertToProfileBufferIndex();
    MOZ_ASSERT(end - start <= std::numeric_limits<Length>::max());
    const auto len = static_cast<Length>(end - start);
    if (len == 0) {
      // In-session but empty, only store a length of 0.
      aEW.WriteULEB128<Length>(0);
      return;
    }
    // In-session.
    // Store buffer length, start and end indices.
    aEW.WriteULEB128<Length>(len);
    aEW.WriteObject(start);
    aEW.WriteObject(end);
    // Write all the bytes.
    auto reader = aBuffer.FullBufferReader();
    aEW.WriteFromReader(reader, reader.RemainingBytes());
    // And write stats.
    aEW.WriteObject(aBuffer.mMaybeUnderlyingBuffer->mPushedBlockCount);
    aEW.WriteObject(aBuffer.mMaybeUnderlyingBuffer->mClearedBlockCount);
  }
};

// A serialized BlocksRingBuffer can be read into an empty buffer (either
// out-of-session, or in-session with enough room).
template <>
struct ProfileBufferEntryReader::Deserializer<BlocksRingBuffer> {
  static void ReadInto(ProfileBufferEntryReader& aER,
                       BlocksRingBuffer& aBuffer) {
    // Expect an empty buffer, as we're going to overwrite it.
    MOZ_ASSERT(aBuffer.GetState().mRangeStart == aBuffer.GetState().mRangeEnd);
    // Read the stored buffer length.
    const auto len = aER.ReadULEB128<Length>();
    if (len == 0) {
      // 0-length means an "uninteresting" buffer, just return now.
      return;
    }
    // We have a non-empty buffer to read.
    if (aBuffer.BufferLength().isSome()) {
      // Output buffer is in-session (i.e., it already has a memory buffer
      // attached). Make sure the caller allocated enough space.
      MOZ_RELEASE_ASSERT(aBuffer.BufferLength()->Value() >= len);
    } else {
      // Output buffer is out-of-session, attach a new memory buffer.
      aBuffer.Set(PowerOfTwo<Length>(len));
      MOZ_ASSERT(aBuffer.BufferLength()->Value() >= len);
    }
    // Read start and end indices.
    const auto start = aER.ReadObject<ProfileBufferIndex>();
    aBuffer.mFirstReadIndex =
        ProfileBufferBlockIndex::CreateFromProfileBufferIndex(start);
    const auto end = aER.ReadObject<ProfileBufferIndex>();
    aBuffer.mNextWriteIndex =
        ProfileBufferBlockIndex::CreateFromProfileBufferIndex(end);
    MOZ_ASSERT(end - start == len);
    // Copy bytes into the buffer.
    auto writer =
        aBuffer.mMaybeUnderlyingBuffer->mBuffer.EntryWriterFromTo(start, end);
    writer.WriteFromReader(aER, end - start);
    MOZ_ASSERT(writer.RemainingBytes() == 0);
    // Finally copy stats.
    aBuffer.mMaybeUnderlyingBuffer->mPushedBlockCount = aER.ReadObject<decltype(
        aBuffer.mMaybeUnderlyingBuffer->mPushedBlockCount)>();
    aBuffer.mMaybeUnderlyingBuffer->mClearedBlockCount =
        aER.ReadObject<decltype(
            aBuffer.mMaybeUnderlyingBuffer->mClearedBlockCount)>();
  }

  // We cannot output a BlocksRingBuffer object (not copyable), use `ReadInto()`
  // or `aER.ReadObject<UniquePtr<BlocksRinbBuffer>>()` instead.
  static BlocksRingBuffer Read(ProfileBufferEntryReader& aER) = delete;
};

// A BlocksRingBuffer is usually refererenced through a UniquePtr, for
// convenience we support (de)serializing that UniquePtr directly.
// This is compatible with the non-UniquePtr serialization above, with a null
// pointer being treated like an out-of-session or empty buffer; and any of
// these would be deserialized into a null pointer.
template <>
struct ProfileBufferEntryWriter::Serializer<UniquePtr<BlocksRingBuffer>> {
  static Length Bytes(const UniquePtr<BlocksRingBuffer>& aBufferUPtr) {
    if (!aBufferUPtr) {
      // Null pointer, treat it like an empty buffer, i.e., write length of 0.
      return ULEB128Size<Length>(0);
    }
    // Otherwise write the pointed-at BlocksRingBuffer (which could be
    // out-of-session or empty.)
    return SumBytes(*aBufferUPtr);
  }

  static void Write(ProfileBufferEntryWriter& aEW,
                    const UniquePtr<BlocksRingBuffer>& aBufferUPtr) {
    if (!aBufferUPtr) {
      // Null pointer, treat it like an empty buffer, i.e., write length of 0.
      aEW.WriteULEB128<Length>(0);
      return;
    }
    // Otherwise write the pointed-at BlocksRingBuffer (which could be
    // out-of-session or empty.)
    aEW.WriteObject(*aBufferUPtr);
  }
};

template <>
struct ProfileBufferEntryReader::Deserializer<UniquePtr<BlocksRingBuffer>> {
  static void ReadInto(ProfileBufferEntryReader& aER,
                       UniquePtr<BlocksRingBuffer>& aBuffer) {
    aBuffer = Read(aER);
  }

  static UniquePtr<BlocksRingBuffer> Read(ProfileBufferEntryReader& aER) {
    UniquePtr<BlocksRingBuffer> bufferUPtr;
    // Keep a copy of the reader before reading the length, so we can restart
    // from here below.
    ProfileBufferEntryReader readerBeforeLen = aER;
    // Read the stored buffer length.
    const auto len = aER.ReadULEB128<Length>();
    if (len == 0) {
      // 0-length means an "uninteresting" buffer, just return nullptr.
      return bufferUPtr;
    }
    // We have a non-empty buffer.
    // allocate an empty BlocksRingBuffer without mutex.
    bufferUPtr = MakeUnique<BlocksRingBuffer>(
        BlocksRingBuffer::ThreadSafety::WithoutMutex);
    // Rewind the reader before the length and deserialize the contents, using
    // the non-UniquePtr Deserializer.
    aER = readerBeforeLen;
    aER.ReadIntoObject(*bufferUPtr);
    return bufferUPtr;
  }
};

}  // namespace mozilla

#endif  // BlocksRingBuffer_h
