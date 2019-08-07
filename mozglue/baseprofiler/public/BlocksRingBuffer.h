/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BlocksRingBuffer_h
#define BlocksRingBuffer_h

#include "mozilla/BaseProfilerDetail.h"
#include "mozilla/ModuloBuffer.h"
#include "mozilla/Pair.h"

#include "mozilla/Maybe.h"

#include <functional>
#include <utility>

namespace mozilla {

// Thread-safe Ring buffer that can store blocks of different sizes.
// Each *block* contains an *entry* and the entry size:
// [ entry_size | entry ] [ entry_size | entry ] ...
//
// To write an entry, the buffer reserves a block of sufficient size (to contain
// user data of predetermined size), writes the entry size, and lets the caller
// fill the entry contents using ModuloBuffer::Iterator APIs and a few entry-
// specific APIs. E.g.:
// ```
// BlockRingsBuffer brb(PowerOfTwo<BlockRingsBuffer::Length>(1024));
// brb.Put([&](BlocksRingBuffer::EntryReserver aER) {
//   aER.Reserve([&](BlocksRingBuffer::EntryWriter aEW) {
//     /* Use EntryWriter functions to serialize objects into entry. */
//     aEW.WriteObject(123);
//   });
// });
// ```
// Other `Put...` functions may be used as shortcuts for simple objects.
// The objects given to the caller's callbacks should only be used inside the
// callbacks and not stored elsewhere, because they keep their own references to
// the BlocksRingBuffer and therefore should not live longer.
//
// When reading data, the buffer iterates over blocks (it knows how to read the
// entry size, and therefore move to the next block), and lets the caller read
// the entry inside of each block. E.g.:
// ```
// brb.Read([](BlocksRingBuffer::Reader aR) {}
//   for (BlocksRingBuffer::EntryReader aER) {
//     /* Use EntryReader functions to read back serialized objects. */
//     int n = aER.ReadObject<int>();
//   }
// });
// ```
//
// The caller may retrieve the `BlockIndex` corresponding to an entry
// (`BlockIndex` is an opaque type preventing the user from modifying it). That
// index may later be used to get back to that particular entry if it still
// exists.
//
// The caller may register an "entry destructor" function on creation, which
// will be invoked on entries that are about to be removed, which may be:
// - Entry being overwritten by new data.
// - When the caller is explicitly `Clear()`ing parts of the buffer.
// - When the buffer is destroyed.
// Note that this means the caller's provided entry destructor may be invoked
// from inside of another of the caller's functions, so be ready for this
// re-entrancy; e.g., the entry destructor should not lock a non-recursive mutex
// that buffer-writing/clearing functions may also lock!
class BlocksRingBuffer {
  // Near-infinite index type, not expecting overflow.
  using Index = uint64_t;

  // Using ModuloBuffer as underlying circular byte buffer.
  using Buffer = ModuloBuffer<uint32_t, Index>;
  using BufferWriter = Buffer::Writer;
  using BufferReader = Buffer::Reader;

 public:
  // Length type for total buffer (as PowerOfTwo<Length>) and each entry.
  using Length = uint32_t;

  // Externally-opaque class encapsulating a block index.
  // User may get these from some BlocksRingBuffer functions, to later use them
  // to access previous blocks using `GetEntryAt(BlockIndex)` functions.
  class BlockIndex {
   public:
    // Default constructor with internal 0 value, for which BlocksRingBuffer
    // guarantees that it is before any valid entries. All public APIs should
    // fail gracefully, doing nothing and/or returning Nothing. It may be used
    // to initialize a BlockIndex variable that will be overwritten with a real
    // value (e.g., from `ReadObject()`, `CurrentBlockIndex()`, etc.).
    BlockIndex() : mBlockIndex(0) {}

    // Explicit conversion to bool, works in `if/while()` tests.
    // Only returns false for default `BlockIndex{}` value.
    explicit operator bool() const { return mBlockIndex != 0; }

    // Comparison over the wide `Index` range, ignoring wrapping of the
    // containing ring buffer.
    bool operator==(const BlockIndex& aRhs) const {
      return mBlockIndex == aRhs.mBlockIndex;
    }
    bool operator!=(const BlockIndex& aRhs) const {
      return mBlockIndex != aRhs.mBlockIndex;
    }
    bool operator<(const BlockIndex& aRhs) const {
      return mBlockIndex < aRhs.mBlockIndex;
    }
    bool operator<=(const BlockIndex& aRhs) const {
      return mBlockIndex <= aRhs.mBlockIndex;
    }
    bool operator>(const BlockIndex& aRhs) const {
      return mBlockIndex > aRhs.mBlockIndex;
    }
    bool operator>=(const BlockIndex& aRhs) const {
      return mBlockIndex >= aRhs.mBlockIndex;
    }

   private:
    // Only BlocksRingBuffer internal functions can convert between `BlockIndex`
    // and `Index`.
    friend class BlocksRingBuffer;
    explicit BlockIndex(Index aBlockIndex) : mBlockIndex(aBlockIndex) {}
    explicit operator Index() const { return mBlockIndex; }

    Index mBlockIndex;
  };

  // Constructors with no entry destructor, the oldest entries will be silently
  // overwritten/destroyed.

  // Create a buffer of the given length.
  explicit BlocksRingBuffer(PowerOfTwo<Length> aLength) : mBuffer(aLength) {}

  // Take ownership of an existing buffer.
  BlocksRingBuffer(UniquePtr<Buffer::Byte[]> aExistingBuffer,
                   PowerOfTwo<Length> aLength)
      : mBuffer(std::move(aExistingBuffer), aLength) {}

  // Use an externally-owned buffer.
  BlocksRingBuffer(Buffer::Byte* aExternalBuffer, PowerOfTwo<Length> aLength)
      : mBuffer(aExternalBuffer, aLength) {}

  // Constructors with an entry destructor, which will be called with an
  // `EntryReader` before the oldest entries get overwritten/destroyed.
  // Note that this entry destructor may be invoked from another caller's
  // function that writes/clears data, be aware of this re-entrancy! (Details
  // above class.)

  // Create a buffer of the given length.
  template <typename EntryDestructor>
  explicit BlocksRingBuffer(PowerOfTwo<Length> aLength,
                            EntryDestructor&& aEntryDestructor)
      : mBuffer(aLength),
        mEntryDestructor(std::forward<EntryDestructor>(aEntryDestructor)) {}

  // Take ownership of an existing buffer.
  template <typename EntryDestructor>
  explicit BlocksRingBuffer(UniquePtr<Buffer::Byte[]> aExistingBuffer,
                            PowerOfTwo<Length> aLength,
                            EntryDestructor&& aEntryDestructor)
      : mBuffer(std::move(aExistingBuffer), aLength),
        mEntryDestructor(std::forward<EntryDestructor>(aEntryDestructor)) {}

  // Use an externally-owned buffer.
  template <typename EntryDestructor>
  explicit BlocksRingBuffer(Buffer::Byte* aExternalBuffer,
                            PowerOfTwo<Length> aLength,
                            EntryDestructor&& aEntryDestructor)
      : mBuffer(aExternalBuffer, aLength),
        mEntryDestructor(std::forward<EntryDestructor>(aEntryDestructor)) {}

  // Destructor explictly destroys all remaining entries, this may invoke the
  // caller-provided entry destructor.
  ~BlocksRingBuffer() {
#ifdef DEBUG
    // Needed because of lock DEBUG-check in `DestroyAllEntries()`.
    baseprofiler::detail::BaseProfilerAutoLock lock(mMutex);
#endif  // DEBUG
    DestroyAllEntries();
  }

  // Buffer length, constant. No need for locking.
  PowerOfTwo<Length> BufferLength() const { return mBuffer.BufferLength(); }

  // Snapshot of the buffer state.
  struct State {
    // Index to the first block.
    BlockIndex mRangeStart;

    // Index past the last block. Equals mRangeStart if empty.
    BlockIndex mRangeEnd;

    // Number of blocks that have been pushed into this buffer.
    uint64_t mPushedBlockCount = 0;

    // Number of blocks that have been removed from this buffer.
    // Note: Live entries = pushed - cleared.
    uint64_t mClearedBlockCount = 0;
  };

  // Get a snapshot of the current state.
  // Note that these may change right after this thread-safe call, so they
  // should only be used for statistical purposes.
  State GetState() const {
    baseprofiler::detail::BaseProfilerAutoLock lock(mMutex);
    return {mFirstReadIndex, mNextWriteIndex, mPushedBlockCount,
            mClearedBlockCount};
  }

  // Iterator-like class used to read from an entry.
  // Created through `BlockIterator`, or a `GetEntryAt()` function, lives
  // within a lock guard lifetime.
  class EntryReader : public BufferReader {
   public:
#ifdef DEBUG
    ~EntryReader() {
      // Expect reader to stay within the entry.
      MOZ_ASSERT(CurrentIndex() >= mEntryStart);
      MOZ_ASSERT(CurrentIndex() <= mEntryStart + mEntryBytes);
      // No EntryReader should live outside of a mutexed call.
      mRing->mMutex.AssertCurrentThreadOwns();
    }
#endif  // DEBUG

    // All BufferReader (aka ModuloBuffer<uint32_t, Index>::Reader) APIs are
    // available to read data from this entry.
    // Note that there are no bound checks! So this should not be used with
    // external untrusted data.

    // Total number of bytes in this entry.
    Length EntryBytes() const { return mEntryBytes; }

    // Current position of this iterator in the entry.
    Length IndexInEntry() const {
      return static_cast<Length>(CurrentIndex() - mEntryStart);
    }

    // Number of bytes left in this entry after this iterator.
    Length RemainingBytes() const {
      return static_cast<Length>(mEntryStart + mEntryBytes - CurrentIndex());
    }

    // Can be used as reference to come back to this entry with GetEntryAt().
    BlockIndex CurrentBlockIndex() const {
      return BlockIndex(mEntryStart - BufferReader::ULEB128Size(mEntryBytes));
    }

    // Index past the end of this block, which is the start of the next block.
    BlockIndex NextBlockIndex() const {
      return BlockIndex(mEntryStart + mEntryBytes);
    }

    // Index of the first block in the whole buffer.
    BlockIndex BufferRangeStart() const { return mRing->mFirstReadIndex; }

    // Index past the last block in the whole buffer.
    BlockIndex BufferRangeEnd() const { return mRing->mNextWriteIndex; }

    // Get another entry based on a {Current,Next}BlockIndex(). This may fail if
    // the buffer has already looped around and destroyed that block, or for the
    // one-past-the-end index.
    Maybe<EntryReader> GetEntryAt(BlockIndex aBlockIndex) {
      // Don't accept a not-yet-written index.
      MOZ_ASSERT(aBlockIndex <= BufferRangeEnd());
      if (aBlockIndex >= BufferRangeStart() && aBlockIndex < BufferRangeEnd()) {
        // Block is still alive -> Return reader for it.
        mRing->AssertBlockIndexIsValid(aBlockIndex);
        return Some(EntryReader(*mRing, aBlockIndex));
      }
      // Block has been overwritten/cleared.
      return Nothing();
    }

    // Get the next entry. This may fail if this is already the last entry.
    Maybe<EntryReader> GetNextEntry() {
      const BlockIndex nextBlockIndex = NextBlockIndex();
      if (nextBlockIndex < BufferRangeEnd()) {
        return Some(EntryReader(*mRing, nextBlockIndex));
      }
      return Nothing();
    }

   private:
    // Only a BlocksRingBuffer can instantiate an EntryReader.
    friend class BlocksRingBuffer;

    explicit EntryReader(const BlocksRingBuffer& aRing, BlockIndex aBlockIndex)
        : BufferReader(aRing.mBuffer.ReaderAt(Index(aBlockIndex))),
          mRing(WrapNotNull(&aRing)),
          mEntryBytes(BufferReader::ReadULEB128<Length>()),
          mEntryStart(CurrentIndex()) {
      // No EntryReader should live outside of a mutexed call.
      mRing->mMutex.AssertCurrentThreadOwns();
    }

    // Using a non-null pointer instead of a reference, to allow copying.
    // This EntryReader should only live inside one of the thread-safe
    // BlocksRingBuffer functions, for this reference to stay valid.
    NotNull<const BlocksRingBuffer*> mRing;
    Length mEntryBytes;
    Index mEntryStart;
  };

  class Reader;

  // Class that can iterate through blocks and provide `EntryReader`s.
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

    // Dereferencing creates an `EntryReader` for the entry inside this block.
    EntryReader operator*() const {
      return mRing->ReaderInBlockAt(mBlockIndex);
    }

    // True if this iterator is just past the last entry.
    bool IsAtEnd() const {
      MOZ_ASSERT(mBlockIndex <= BufferRangeEnd());
      return mBlockIndex == BufferRangeEnd();
    }

    // Can be used as reference to come back to this entry with `GetEntryAt()`.
    BlockIndex CurrentBlockIndex() const { return mBlockIndex; }

    // Index past the end of this block, which is the start of the next block.
    BlockIndex NextBlockIndex() const {
      MOZ_ASSERT(!IsAtEnd());
      BufferReader reader = mRing->mBuffer.ReaderAt(Index(mBlockIndex));
      Length entrySize = reader.ReadULEB128<Length>();
      return BlockIndex(reader.CurrentIndex() + entrySize);
    }

    // Index of the first block in the whole buffer.
    BlockIndex BufferRangeStart() const { return mRing->mFirstReadIndex; }

    // Index past the last block in the whole buffer.
    BlockIndex BufferRangeEnd() const { return mRing->mNextWriteIndex; }

   private:
    // Only a Reader can instantiate a BlockIterator.
    friend class Reader;

    BlockIterator(const BlocksRingBuffer& aRing, BlockIndex aBlockIndex)
        : mRing(WrapNotNull(&aRing)), mBlockIndex(aBlockIndex) {
      // No BlockIterator should live outside of a mutexed call.
      mRing->mMutex.AssertCurrentThreadOwns();
    }

    // Using a non-null pointer instead of a reference, to allow copying.
    // This BlockIterator should only live inside one of the thread-safe
    // BlocksRingBuffer functions, for this reference to stay valid.
    NotNull<const BlocksRingBuffer*> mRing;
    BlockIndex mBlockIndex;
  };

  // Class that can create `BlockIterator`s (e.g., for range-for), or just
  // iterate through entries; lives within a lock guard lifetime.
  class Reader {
   public:
#ifdef DEBUG
    ~Reader() {
      // No Reader should live outside of a mutexed call.
      mRing->mMutex.AssertCurrentThreadOwns();
    }
#endif  // DEBUG

    // Index of the first block in the whole buffer.
    BlockIndex BufferRangeStart() const { return mRing->mFirstReadIndex; }

    // Index past the last block in the whole buffer.
    BlockIndex BufferRangeEnd() const { return mRing->mNextWriteIndex; }

    // Iterators to the first and past-the-last blocks.
    // Compatible with range-for (see `ForEach` below as example).
    BlockIterator begin() const {
      return BlockIterator(*mRing, BufferRangeStart());
    }
    BlockIterator end() const {
      return BlockIterator(*mRing, BufferRangeEnd());
    }

    // Run `aCallback(EntryReader)` on each entry from first to last.
    // Callback should not store `EntryReader`, as it may become invalid after
    // this thread-safe call.
    template <typename Callback>
    void ForEach(Callback&& aCallback) const {
      for (auto it : *this) {
        aCallback(it);
      }
    }

   private:
    friend class BlocksRingBuffer;

    explicit Reader(const BlocksRingBuffer& aRing)
        : mRing(WrapNotNull(&aRing)) {
      // No Reader should live outside of a mutexed call.
      mRing->mMutex.AssertCurrentThreadOwns();
    }

    // Using a non-null pointer instead of a reference, to allow copying.
    // This Reader should only live inside one of the thread-safe
    // BlocksRingBuffer functions, for this reference to stay valid.
    NotNull<const BlocksRingBuffer*> mRing;
  };

  // Call `aCallback(BlocksRingBuffer::Reader)` with temporary Reader, and
  // return whatever `aCallback` returns.
  // Callback should not store `Reader`, as it may become invalid after this
  // call.
  template <typename Callback>
  auto Read(Callback&& aCallback) const {
    baseprofiler::detail::BaseProfilerAutoLock lock(mMutex);
    return std::forward<Callback>(aCallback)(Reader(*this));
  }

  // Call `aCallback(BlocksRingBuffer::EntryReader)` on each item.
  // Callback should not store `EntryReader`, as it may become invalid after
  // this thread-safe call.
  template <typename Callback>
  void ReadEach(Callback&& aCallback) const {
    Read([&](const Reader& aReader) { aReader.ForEach(aCallback); });
  }

  // Call `aCallback(Maybe<BlocksRingBuffer::EntryReader>&&)` on the entry at
  // the given BlockIndex; The `Maybe` will be `Nothing` if that entry doesn't
  // exist anymore, or if we've reached just past the last entry. Return
  // whatever `aCallback` returns.
  // Callback should not store `EntryReader`, as it may become invalid after
  // this thread-safe call.
  template <typename Callback>
  auto ReadAt(BlockIndex aBlockIndex, Callback&& aCallback) const {
    baseprofiler::detail::BaseProfilerAutoLock lock(mMutex);
    MOZ_ASSERT(aBlockIndex <= mNextWriteIndex);
    Maybe<EntryReader> maybeReader;
    if (aBlockIndex >= mFirstReadIndex && aBlockIndex < mNextWriteIndex) {
      AssertBlockIndexIsValid(aBlockIndex);
      maybeReader = Some(ReaderInBlockAt(aBlockIndex));
    }
    return std::forward<Callback>(aCallback)(std::move(maybeReader));
  }

  class EntryReserver;

  // Class used to write an entry contents.
  // Created through `EntryReserver`, lives within a lock guard lifetime.
  class EntryWriter : public BufferWriter {
   public:
    // Allow move-construction.
#ifdef DEBUG
    EntryWriter(EntryWriter&& aOther)
        : BufferWriter(std::move(aOther)),
          mRing(aOther.mRing),
          mEntryBytes(aOther.mEntryBytes),
          mEntryStart(aOther.mEntryStart) {
      // No EntryWriter should live outside of a mutexed call.
      mRing.mMutex.AssertCurrentThreadOwns();
      // In DEBUG, we need to move the moved-from EntryWriter to the end of the
      // entry, so as not to trip the MOZ_ASSERT() in the destructor below.
      aOther += aOther.RemainingBytes();
    }

    ~EntryWriter() {
      // We expect the caller to completely fill the entry.
      // (Or at least pretend to, by moving this iterator to the end.)
      MOZ_ASSERT(RemainingBytes() == 0);
      // No EntryWriter should live outside of a mutexed call.
      mRing.mMutex.AssertCurrentThreadOwns();
    }
#else   // DEBUG
    EntryWriter(EntryWriter&& aOther) = default;
#endif  // DEBUG else
    // Disallow copying and assignments.
    EntryWriter(const EntryWriter& aOther) = delete;
    EntryWriter& operator=(const EntryWriter& aOther) = delete;
    EntryWriter& operator=(EntryWriter&& aOther) = delete;

    // All BufferWriter (aka ModuloBuffer<uint32_t, Index>::Writer) APIs are
    // available to read/write data from/to this entry.
    // Note that there are no bound checks! So this should not be used with
    // external data.

    // Total number of bytes in this entry.
    Length EntryBytes() const { return mEntryBytes; }

    // Current position of this iterator in the entry.
    Length IndexInEntry() const {
      return static_cast<Length>(CurrentIndex() - mEntryStart);
    }

    // Number of bytes left in this entry after this iterator.
    Length RemainingBytes() const {
      return static_cast<Length>(mEntryStart + mEntryBytes - CurrentIndex());
    }

    // Can be used as reference to come back to this entry with `GetEntryAt()`.
    BlockIndex CurrentBlockIndex() const {
      return BlockIndex(mEntryStart - BufferWriter::ULEB128Size(mEntryBytes));
    }

    // Index past the end of this block, which will be the start of the next
    // block to be written.
    BlockIndex BlockEndIndex() const {
      return BlockIndex(mEntryStart + mEntryBytes);
    }

    // Index of the first block in the whole buffer.
    BlockIndex BufferRangeStart() const { return mRing.mFirstReadIndex; }

    // Index past the last block in the whole buffer.
    BlockIndex BufferRangeEnd() const { return mRing.mNextWriteIndex; }

    // Get another entry based on a {Current,Next}BlockIndex(). This may fail if
    // the buffer has already looped around and destroyed that block.
    Maybe<EntryReader> GetEntryAt(BlockIndex aBlockIndex) {
      // Don't accept a not-yet-written index.
      MOZ_RELEASE_ASSERT(aBlockIndex < BufferRangeEnd());
      if (aBlockIndex >= BufferRangeStart()) {
        // Block is still alive -> Return reader for it.
        mRing.AssertBlockIndexIsValid(aBlockIndex);
        return Some(EntryReader(mRing, aBlockIndex));
      }
      // Block has been overwritten/cleared.
      return Nothing();
    }

   private:
    // Only an EntryReserver can instantiate an EntryWriter.
    friend class EntryReserver;

    // Compute space needed for a block that can contain an entry of size
    // `aEntryBytes`.
    static Length BlockSizeForEntrySize(Length aEntryBytes) {
      return aEntryBytes +
             static_cast<Length>(BufferWriter::ULEB128Size(aEntryBytes));
    }

    EntryWriter(BlocksRingBuffer& aRing, BlockIndex aBlockIndex,
                Length aEntryBytes)
        : BufferWriter(aRing.mBuffer.WriterAt(Index(aBlockIndex))),
          mRing(aRing),
          mEntryBytes(aEntryBytes),
          mEntryStart([&]() {
            // BufferWriter is at `aBlockIndex`. Write the entry size...
            BufferWriter::WriteULEB128(aEntryBytes);
            // ... BufferWriter now at start of entry section.
            return CurrentIndex();
          }()) {
      // No EntryWriter should live outside of a mutexed call.
      mRing.mMutex.AssertCurrentThreadOwns();
    }

    // This EntryWriter should only live inside one of the thread-safe
    // BlocksRingBuffer functions, for this reference to stay valid.
    BlocksRingBuffer& mRing;
    const Length mEntryBytes;
    const Index mEntryStart;
  };

  // Class used to reserve space for new blocks, and to create `EntryWriter`s
  // for them; lives within a lock guard lifetime.
  class EntryReserver {
   public:
#ifdef DEBUG
    ~EntryReserver() {
      // No EntryReserver should live outside of a mutexed call.
      mRing->mMutex.AssertCurrentThreadOwns();
    }
#endif  // DEBUG

    // Reserve `aBytes`, call `aCallback` with a temporary EntryWriter, and
    // return whatever `aCallback` returns.
    // Callback should not store `EntryWriter`, as it may become invalid after
    // this thread-safe call.
    template <typename Callback>
    auto Reserve(Length aBytes, Callback&& aCallback) {
      // Don't allow even half of the buffer length. More than that would
      // probably be unreasonable, and much more would risk having an entry
      // wrapping around and overwriting itself!
      MOZ_RELEASE_ASSERT(aBytes < mRing->BufferLength().Value() / 2);
      // COmpute block size from the requested entry size.
      const Length blockBytes = EntryWriter::BlockSizeForEntrySize(aBytes);
      // We will put this new block at the end of the current buffer.
      const BlockIndex blockIndex = mRing->mNextWriteIndex;
      // Compute the end of this new block...
      const Index blockEnd = Index(blockIndex) + blockBytes;
      // ... which is where the following block will go.
      mRing->mNextWriteIndex = BlockIndex(blockEnd);
      while (blockEnd >
             Index(mRing->mFirstReadIndex) + mRing->BufferLength().Value()) {
        // About to trample on an old block.
        EntryReader reader = mRing->ReaderInBlockAt(mRing->mFirstReadIndex);
        // Call provided entry destructor for that entry.
        if (mRing->mEntryDestructor) {
          mRing->mEntryDestructor(reader);
        }
        mRing->mClearedBlockCount += 1;
        MOZ_ASSERT(reader.CurrentIndex() <= Index(reader.NextBlockIndex()));
        // Move the buffer reading start past this cleared block.
        mRing->mFirstReadIndex = reader.NextBlockIndex();
      }
      mRing->mPushedBlockCount += 1;
      // Finally, let aCallback write into the entry.
      return std::forward<Callback>(aCallback)(
          EntryWriter(*mRing, blockIndex, aBytes));
    }

    // Write a new entry copied from the given buffer, return block index.
    BlockIndex Write(const void* aSrc, Length aBytes) {
      return Reserve(aBytes, [&](EntryWriter aEW) {
        aEW.Write(aSrc, aBytes);
        return aEW.CurrentBlockIndex();
      });
    }

    // Write a new entry copied from the given object, return block index.
    // Restricted to trivially-copyable types.
    // TODO: Allow more types (follow-up patches in progress).
    template <typename T>
    BlockIndex WriteObject(const T& aOb) {
      return Write(&aOb, sizeof(T));
    }

    // Index of the first block in the whole buffer.
    BlockIndex BufferRangeStart() const { return mRing->mFirstReadIndex; }

    // Index past the last block in the whole buffer.
    BlockIndex BufferRangeEnd() const { return mRing->mNextWriteIndex; }

    // Get another entry based on a {Current,Next}BlockIndex(). This may fail if
    // the buffer has already looped around and destroyed that block.
    Maybe<EntryReader> GetEntryAt(BlockIndex aBlockIndex) {
      // Don't accept a not-yet-written index.
      MOZ_ASSERT(aBlockIndex <= BufferRangeEnd());
      if (aBlockIndex >= BufferRangeStart() && aBlockIndex < BufferRangeEnd()) {
        // Block is still alive -> Return reader for it.
        mRing->AssertBlockIndexIsValid(aBlockIndex);
        return Some(EntryReader(*mRing, aBlockIndex));
      }
      // Block has been overwritten/cleared.
      return Nothing();
    }

   private:
    // Only a BlocksRingBuffer can instantiate an EntryReserver.
    friend class BlocksRingBuffer;

    explicit EntryReserver(BlocksRingBuffer& aRing)
        : mRing(WrapNotNull(&aRing)) {
      // No EntryReserver should live outside of a mutexed call.
      mRing->mMutex.AssertCurrentThreadOwns();
    }

    // Using a non-null pointer instead of a reference, to allow copying.
    // This EntryReserver should only live inside one of the thread-safe
    // BlocksRingBuffer functions, for this reference to stay valid.
    NotNull<BlocksRingBuffer*> mRing;
  };

  // Add a new entry, call `aCallback` with a temporary EntryReserver (so that
  // `aCallback` can reserve an entry or just write something), and return
  // whatever `aCallback` returns.
  // Callback should not store `EntryReserver`, as it may become invalid after
  // this thread-safe call.
  template <typename Callback>
  auto Put(Callback&& aCallback) {
    // Implementation note: We are locking during the whole operation (reserving
    // and writing entry), which means slow writers could block the buffer for a
    // while. It should be possible to only lock when reserving the space, and
    // then letting the callback write the entry without a need for the lock, as
    // it's the only thread that should be accessing this particular entry.
    // Extra safety would be necessary to ensure the entry cannot be read, and
    // fast writers going around the ring cannot trample on this entry until it
    // is fully written.
    // TODO: Investigate this potential improvement as part of bug 1562604.
    baseprofiler::detail::BaseProfilerAutoLock lock(mMutex);
    return std::forward<Callback>(aCallback)(EntryReserver(*this));
  }

  // Add a new entry of known size, call `aCallback` with a temporary
  // EntryWriter, and return whatever `aCallback` returns.
  // Callback should not store `EntryWriter`, as it may become invalid after
  // this thread-safe call.
  template <typename Callback>
  auto Put(Length aLength, Callback&& aCallback) {
    return Put([&](EntryReserver aER) {
      return aER.Reserve(aLength, std::forward<Callback>(aCallback));
    });
  }

  // Add a new entry copied from the given buffer, return block index.
  BlockIndex PutFrom(const void* aSrc, Length aBytes) {
    return Put([&](EntryReserver aER) { return aER.Write(aSrc, aBytes); });
  }

  // Add a new entry copied from the given object, return block index.
  // Restricted to trivially-copyable types.
  // TODO: Allow more types (follow-up patches in progress, see bug 1562604).
  template <typename T>
  BlockIndex PutObject(const T& aOb) {
    return Put([&](EntryReserver aER) { return aER.WriteObject<T>(aOb); });
  }

  // Clear all entries, calling entry destructor (if any), and move read index
  // to the end so that these entries cannot be read anymore.
  void Clear() {
    baseprofiler::detail::BaseProfilerAutoLock lock(mMutex);
    ClearAllEntries();
  }

  // Clear all entries strictly before aBlockIndex, calling calling entry
  // destructor (if any), and move read index to the end so that these entries
  // cannot be read anymore.
  void ClearBefore(BlockIndex aBlockIndex) {
    baseprofiler::detail::BaseProfilerAutoLock lock(mMutex);
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
    if (mEntryDestructor) {
      // We have an entry destructor, destroy entries before aBlockIndex.
      Reader reader(*this);
      BlockIterator it = reader.begin();
      for (; it.CurrentBlockIndex() < aBlockIndex; ++it) {
        MOZ_ASSERT(it.CurrentBlockIndex() < reader.end().CurrentBlockIndex());
        mEntryDestructor(*it);
        mClearedBlockCount += 1;
      }
      MOZ_ASSERT(it.CurrentBlockIndex() == aBlockIndex);
    } else {
      // No entry destructor, just count skipped entries.
      Reader reader(*this);
      BlockIterator it = reader.begin();
      for (; it.CurrentBlockIndex() < aBlockIndex; ++it) {
        MOZ_ASSERT(it.CurrentBlockIndex() < reader.end().CurrentBlockIndex());
        mClearedBlockCount += 1;
      }
      MOZ_ASSERT(it.CurrentBlockIndex() == aBlockIndex);
    }
    // Move read index to given index, so there's effectively no more entries
    // before.
    mFirstReadIndex = aBlockIndex;
  }

#ifdef DEBUG
  void Dump() const {
    baseprofiler::detail::BaseProfilerAutoLock lock(mMutex);
    using ULL = unsigned long long;
    printf("start=%llu (%llu) end=%llu (%llu) - ", ULL(Index(mFirstReadIndex)),
           ULL(Index(mFirstReadIndex) & (BufferLength().Value() - 1)),
           ULL(Index(mNextWriteIndex)),
           ULL(Index(mNextWriteIndex) & (BufferLength().Value() - 1)));
    mBuffer.Dump();
  }
#endif  // DEBUG

 private:
  // In DEBUG mode, assert that `aBlockIndex` is a valid index for a live block.
  // (Not just in range, but points exactly at the start of a block.)
  // Slow, so avoid it for internal checks; this is more to check what callers
  // provide us.
  void AssertBlockIndexIsValid(BlockIndex aBlockIndex) const {
    mMutex.AssertCurrentThreadOwns();
#ifdef DEBUG
    MOZ_ASSERT(aBlockIndex >= mFirstReadIndex);
    MOZ_ASSERT(aBlockIndex < mNextWriteIndex);
    // Quick check (default), or slow check (change '1' to '0') below:
#  if 1
    // Quick check that this looks like a valid block start.
    // Read the entry size at the start of the block.
    BufferReader br = mBuffer.ReaderAt(Index(aBlockIndex));
    Length entryBytes = br.ReadULEB128<Length>();
    // It should be between 1 and half of the buffer length max.
    MOZ_ASSERT(entryBytes > 0);
    MOZ_ASSERT(entryBytes < BufferLength().Value() / 2);
    // The end of the block should be inside the live buffer range.
    MOZ_ASSERT(Index(aBlockIndex) + BufferReader::ULEB128Size(entryBytes) +
                   entryBytes <=
               Index(mNextWriteIndex));
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

  // Create a reader for the block starting at aBlockIndex.
  EntryReader ReaderInBlockAt(BlockIndex aBlockIndex) const {
    mMutex.AssertCurrentThreadOwns();
    MOZ_ASSERT(aBlockIndex >= mFirstReadIndex);
    MOZ_ASSERT(aBlockIndex < mNextWriteIndex);
    return EntryReader(*this, aBlockIndex);
  }

  // Call entry destructor (if any) on all entries.
  // Note: The read index is not moved; this should only be called from the
  // destructor or ClearAllEntries.
  void DestroyAllEntries() {
    mMutex.AssertCurrentThreadOwns();
    if (mEntryDestructor) {
      // We have an entry destructor, destroy all the things!
      Reader(*this).ForEach(
          [this](EntryReader aReader) { mEntryDestructor(aReader); });
    }
    mClearedBlockCount = mPushedBlockCount;
  }

  // Clear all entries, calling entry destructor (if any), and move read index
  // to the end so that these entries cannot be read anymore.
  void ClearAllEntries() {
    mMutex.AssertCurrentThreadOwns();
    DestroyAllEntries();
    // Move read index to write index, so there's effectively no more entries
    // that can be read. (Not setting both to 0, in case user is keeping
    // `BlockIndex`'es to old entries.)
    mFirstReadIndex = mNextWriteIndex;
  }

  // Mutex guarding the following members.
  mutable baseprofiler::detail::BaseProfilerMutex mMutex;

  // Underlying circular byte buffer.
  Buffer mBuffer;
  // Index to the first block to be read (or cleared). Initialized to 1 because
  // 0 is reserved for the "empty" BlockIndex value.
  BlockIndex mFirstReadIndex = BlockIndex(Index(1));
  // Index where the next new block should be allocated. Initialized to 1
  // because 0 is reserved for the "empty" BlockIndex value.
  BlockIndex mNextWriteIndex = BlockIndex(Index(1));
  // If set, function to call for each entry that is about to be destroyed.
  std::function<void(EntryReader)> mEntryDestructor;

  // Statistics.
  uint64_t mPushedBlockCount = 0;
  uint64_t mClearedBlockCount = 0;
};

}  // namespace mozilla

#endif  // BlocksRingBuffer_h
