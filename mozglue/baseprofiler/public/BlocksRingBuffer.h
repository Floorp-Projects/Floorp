/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BlocksRingBuffer_h
#define BlocksRingBuffer_h

#include "mozilla/DebugOnly.h"
#include "mozilla/BaseProfilerDetail.h"
#include "mozilla/ModuloBuffer.h"
#include "mozilla/Pair.h"
#include "mozilla/Unused.h"

#include "mozilla/Maybe.h"
#include "mozilla/Span.h"
#include "mozilla/Tuple.h"
#include "mozilla/UniquePtrExtensions.h"
#include "mozilla/Variant.h"

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
//                   [&](BlocksRingBuffer::EntryWriter& aEW) {
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
//   for (BlocksRingBuffer::EntryReader aER : aR) {
//     /* Use EntryReader functions to read back serialized objects. */
//     int n = aER.ReadObject<int>();
//   }
// });
// ```
// Different type of objects may be deserialized from an entry, see
// `Deserializer` for more information.
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

  // Class to be specialized for types to be written in a BlocksRingBuffer.
  // See common specializations at the bottom of this header.
  // The following static functions must be provided:
  //   static Length Bytes(const T& aT) {
  //     /* Return number of bytes that will be written. */
  //   }
  //   static void Write(EntryWriter& aEW,
  //                     const T& aT) {
  //     /* Call `aEW.WriteX(...)` functions to serialize aT, be sure to write
  //        exactly `Bytes(aT)` bytes! */
  //   }
  template <typename T>
  struct Serializer;

  // Class to be specialized for types to be read from a BlocksRingBuffer.
  // See common specializations at the bottom of this header.
  // The following static functions must be provided:
  //   static void ReadInto(EntryReader aER&, T& aT)
  //   {
  //     /* Call `aER.ReadX(...)` function to deserialize into aT, be sure to
  //        read exactly `Bytes(aT)`! */
  //   }
  //   static T Read(EntryReader& aER) {
  //     /* Call `aER.ReadX(...)` function to deserialize and return a `T`, be
  //        sure to read exactly `Bytes(returned value)`! */
  //   }
  template <typename T>
  struct Deserializer;

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
    // Only BlocksRingBuffer internal functions and serializers can convert
    // between `BlockIndex` and `Index`.
    friend class BlocksRingBuffer;
    explicit BlockIndex(Index aBlockIndex) : mBlockIndex(aBlockIndex) {}
    explicit operator Index() const { return mBlockIndex; }

    Index mBlockIndex;
  };

  // Default constructor starts out-of-session (nothing to read or write).
  BlocksRingBuffer() = default;

  // Constructors with no entry destructor, the oldest entries will be silently
  // overwritten/destroyed.

  // Create a buffer of the given length.
  explicit BlocksRingBuffer(PowerOfTwo<Length> aLength)
      : mMaybeUnderlyingBuffer(Some(UnderlyingBuffer(aLength))) {}

  // Take ownership of an existing buffer.
  BlocksRingBuffer(UniquePtr<Buffer::Byte[]> aExistingBuffer,
                   PowerOfTwo<Length> aLength)
      : mMaybeUnderlyingBuffer(
            Some(UnderlyingBuffer(std::move(aExistingBuffer), aLength))) {}

  // Use an externally-owned buffer.
  BlocksRingBuffer(Buffer::Byte* aExternalBuffer, PowerOfTwo<Length> aLength)
      : mMaybeUnderlyingBuffer(
            Some(UnderlyingBuffer(aExternalBuffer, aLength))) {}

  // Constructors with an entry destructor, which will be called with an
  // `EntryReader` before the oldest entries get overwritten/destroyed.
  // Note that this entry destructor may be invoked from another caller's
  // function that writes/clears data, be aware of this re-entrancy! (Details
  // above class.)

  // Create a buffer of the given length.
  template <typename EntryDestructor>
  explicit BlocksRingBuffer(PowerOfTwo<Length> aLength,
                            EntryDestructor&& aEntryDestructor)
      : mMaybeUnderlyingBuffer(Some(UnderlyingBuffer(
            aLength, std::forward<EntryDestructor>(aEntryDestructor)))) {}

  // Take ownership of an existing buffer.
  template <typename EntryDestructor>
  explicit BlocksRingBuffer(UniquePtr<Buffer::Byte[]> aExistingBuffer,
                            PowerOfTwo<Length> aLength,
                            EntryDestructor&& aEntryDestructor)
      : mMaybeUnderlyingBuffer(Some(UnderlyingBuffer(
            std::move(aExistingBuffer), aLength,
            std::forward<EntryDestructor>(aEntryDestructor)))) {}

  // Use an externally-owned buffer.
  template <typename EntryDestructor>
  explicit BlocksRingBuffer(Buffer::Byte* aExternalBuffer,
                            PowerOfTwo<Length> aLength,
                            EntryDestructor&& aEntryDestructor)
      : mMaybeUnderlyingBuffer(Some(UnderlyingBuffer(
            aExternalBuffer, aLength,
            std::forward<EntryDestructor>(aEntryDestructor)))) {}

  // Destructor explictly destroys all remaining entries, this may invoke the
  // caller-provided entry destructor.
  ~BlocksRingBuffer() {
#ifdef DEBUG
    // Needed because of lock DEBUG-check in `DestroyAllEntries()`.
    baseprofiler::detail::BaseProfilerAutoLock lock(mMutex);
#endif  // DEBUG
    DestroyAllEntries();
  }

  // Remove underlying buffer, if any.
  void Reset() {
    baseprofiler::detail::BaseProfilerAutoLock lock(mMutex);
    ResetUnderlyingBuffer();
  }

  // Create a buffer of the given length.
  void Set(PowerOfTwo<Length> aLength) {
    baseprofiler::detail::BaseProfilerAutoLock lock(mMutex);
    ResetUnderlyingBuffer();
    mMaybeUnderlyingBuffer.emplace(aLength);
  }

  // Take ownership of an existing buffer.
  void Set(UniquePtr<Buffer::Byte[]> aExistingBuffer,
           PowerOfTwo<Length> aLength) {
    baseprofiler::detail::BaseProfilerAutoLock lock(mMutex);
    ResetUnderlyingBuffer();
    mMaybeUnderlyingBuffer.emplace(std::move(aExistingBuffer), aLength);
  }

  // Use an externally-owned buffer.
  void Set(Buffer::Byte* aExternalBuffer, PowerOfTwo<Length> aLength) {
    baseprofiler::detail::BaseProfilerAutoLock lock(mMutex);
    ResetUnderlyingBuffer();
    mMaybeUnderlyingBuffer.emplace(aExternalBuffer, aLength);
  }

  // Create a buffer of the given length, with entry destructor.
  template <typename EntryDestructor>
  void Set(PowerOfTwo<Length> aLength, EntryDestructor&& aEntryDestructor) {
    baseprofiler::detail::BaseProfilerAutoLock lock(mMutex);
    ResetUnderlyingBuffer();
    mMaybeUnderlyingBuffer.emplace(
        aLength, std::forward<EntryDestructor>(aEntryDestructor));
  }

  // Take ownership of an existing buffer, with entry destructor.
  template <typename EntryDestructor>
  void Set(UniquePtr<Buffer::Byte[]> aExistingBuffer,
           PowerOfTwo<Length> aLength, EntryDestructor&& aEntryDestructor) {
    baseprofiler::detail::BaseProfilerAutoLock lock(mMutex);
    ResetUnderlyingBuffer();
    mMaybeUnderlyingBuffer.emplace(
        std::move(aExistingBuffer), aLength,
        std::forward<EntryDestructor>(aEntryDestructor));
  }

  // Use an externally-owned buffer, with entry destructor.
  template <typename EntryDestructor>
  void Set(Buffer::Byte* aExternalBuffer, PowerOfTwo<Length> aLength,
           EntryDestructor&& aEntryDestructor) {
    baseprofiler::detail::BaseProfilerAutoLock lock(mMutex);
    ResetUnderlyingBuffer();
    mMaybeUnderlyingBuffer.emplace(
        aExternalBuffer, aLength,
        std::forward<EntryDestructor>(aEntryDestructor));
  }

  // Lock the buffer mutex and run the provided callback.
  // This can be useful when the caller needs to explicitly lock down this
  // buffer, but not do anything else with it.
  template <typename Callback>
  auto LockAndRun(Callback&& aCallback) const {
    baseprofiler::detail::BaseProfilerAutoLock lock(mMutex);
    return std::forward<Callback>(aCallback)();
  }

  // Buffer length in bytes.
  Maybe<PowerOfTwo<Length>> BufferLength() const {
    baseprofiler::detail::BaseProfilerAutoLock lock(mMutex);
    return mMaybeUnderlyingBuffer.map([](const UnderlyingBuffer& aBuffer) {
      return aBuffer.mBuffer.BufferLength();
    });
    ;
  }

  // Size of external resources.
  // Note: `mEntryDestructor`'s potential external data (for its captures) is
  // not included, as it's hidden in the `std::function` implementation.
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
  // When out-of-session, mFirstReadIndex==mNextWriteIndex, and
  // mPushedBlockCount==mClearedBlockCount==0.
  // Note that these may change right after this thread-safe call, so they
  // should only be used for statistical purposes.
  State GetState() const {
    baseprofiler::detail::BaseProfilerAutoLock lock(mMutex);
    return {
        mFirstReadIndex, mNextWriteIndex,
        mMaybeUnderlyingBuffer ? mMaybeUnderlyingBuffer->mPushedBlockCount : 0,
        mMaybeUnderlyingBuffer ? mMaybeUnderlyingBuffer->mClearedBlockCount
                               : 0};
  }

  // No objects, no bytes! (May be useful for generic programming.)
  static Length SumBytes() { return 0; }

  // Number of bytes needed to serialize objects.
  template <typename T0, typename... Ts>
  static Length SumBytes(const T0& aT0, const Ts&... aTs) {
    return Serializer<T0>::Bytes(aT0) + SumBytes(aTs...);
  }

  // Iterator-like class used to read from an entry.
  // Created through `BlockIterator`, or a `GetEntryAt()` function, lives
  // within a lock guard lifetime.
  class EntryReader : public BufferReader {
   public:
    // Allow move-construction.
    EntryReader(EntryReader&& aOther) = default;
    // Disallow copying and assignments.
    EntryReader(const EntryReader& aOther) = delete;
    EntryReader& operator=(const EntryReader& aOther) = delete;
    EntryReader& operator=(EntryReader&& aOther) = delete;

#ifdef DEBUG
    ~EntryReader() {
      // Expect reader to stay within the entry.
      MOZ_ASSERT(CurrentIndex() >= mEntryStart);
      MOZ_ASSERT(CurrentIndex() <= mEntryStart + mEntryBytes);
      // No EntryReader should live outside of a mutexed call.
      mRing.mMutex.AssertCurrentThreadOwns();
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

    template <typename T>
    void ReadIntoObject(T& aObject) {
      DebugOnly<Length> start = IndexInEntry();
      Deserializer<T>::ReadInto(*this, aObject);
      // Make sure the helper moved the iterator as expected.
      MOZ_ASSERT(IndexInEntry() == start + SumBytes(aObject));
    }

    // Allow `EntryReader::ReadIntoObjects()` with nothing, this could be
    // useful for generic programming.
    void ReadIntoObjects() {}

    // Read into one or more objects, sequentially.
    template <typename T0, typename... Ts>
    void ReadIntoObjects(T0& aT0, Ts&... aTs) {
      ReadIntoObject(aT0);
      ReadIntoObjects(aTs...);
    }

    // Read data as an object and move iterator ahead.
    template <typename T>
    T ReadObject() {
      DebugOnly<Length> start = IndexInEntry();
      T ob = Deserializer<T>::Read(*this);
      // Make sure the helper moved the iterator as expected.
      MOZ_ASSERT(IndexInEntry() == start + SumBytes(ob));
      return ob;
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
    BlockIndex BufferRangeStart() const { return mRing.mFirstReadIndex; }

    // Index past the last block in the whole buffer.
    BlockIndex BufferRangeEnd() const { return mRing.mNextWriteIndex; }

    // Get another entry based on a {Current,Next}BlockIndex(). This may fail if
    // the buffer has already looped around and destroyed that block, or for the
    // one-past-the-end index.
    Maybe<EntryReader> GetEntryAt(BlockIndex aBlockIndex) {
      // Don't accept a not-yet-written index.
      MOZ_ASSERT(aBlockIndex <= BufferRangeEnd());
      if (aBlockIndex >= BufferRangeStart() && aBlockIndex < BufferRangeEnd()) {
        // Block is still alive -> Return reader for it.
        mRing.AssertBlockIndexIsValid(aBlockIndex);
        return Some(EntryReader(mRing, aBlockIndex));
      }
      // Block has been overwritten/cleared.
      return Nothing();
    }

    // Get the next entry. This may fail if this is already the last entry.
    Maybe<EntryReader> GetNextEntry() {
      const BlockIndex nextBlockIndex = NextBlockIndex();
      if (nextBlockIndex < BufferRangeEnd()) {
        return Some(EntryReader(mRing, nextBlockIndex));
      }
      return Nothing();
    }

   private:
    // Only a BlocksRingBuffer can instantiate an EntryReader.
    friend class BlocksRingBuffer;

    explicit EntryReader(const BlocksRingBuffer& aRing, BlockIndex aBlockIndex)
        : BufferReader(aRing.mMaybeUnderlyingBuffer->mBuffer.ReaderAt(
              Index(aBlockIndex))),
          mRing(aRing),
          mEntryBytes(BufferReader::ReadULEB128<Length>()),
          mEntryStart(CurrentIndex()) {
      // No EntryReader should live outside of a mutexed call.
      mRing.mMutex.AssertCurrentThreadOwns();
    }

    // Using a non-null pointer instead of a reference, to allow copying.
    // This EntryReader should only live inside one of the thread-safe
    // BlocksRingBuffer functions, for this reference to stay valid.
    const BlocksRingBuffer& mRing;
    const Length mEntryBytes;
    const Index mEntryStart;
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
      BufferReader reader =
          mRing->mMaybeUnderlyingBuffer->mBuffer.ReaderAt(Index(mBlockIndex));
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
    BlockIndex BufferRangeStart() const { return mRing.mFirstReadIndex; }

    // Index past the last block in the whole buffer.
    BlockIndex BufferRangeEnd() const { return mRing.mNextWriteIndex; }

    // Iterators to the first and past-the-last blocks.
    // Compatible with range-for (see `ForEach` below as example).
    BlockIterator begin() const {
      return BlockIterator(mRing, BufferRangeStart());
    }
    // Note that a `BlockIterator` at the `end()` should not be dereferenced, as
    // there is no actual block there!
    BlockIterator end() const { return BlockIterator(mRing, BufferRangeEnd()); }

    // Get a `BlockIterator` at the given `BlockIndex`, clamped to the stored
    // range. Note that a `BlockIterator` at the `end()` should not be
    // dereferenced, as there is no actual block there!
    BlockIterator At(BlockIndex aBlockIndex) const {
      if (aBlockIndex < BufferRangeStart()) {
        // Anything before the range (including null BlockIndex) is clamped at
        // the beginning.
        return begin();
      }
      // Otherwise we at least expect the index to be valid (pointing exactly at
      // a live block, or just past the end.)
      mRing.AssertBlockIndexIsValidOrEnd(aBlockIndex);
      return BlockIterator(mRing, aBlockIndex);
    }

    // Run `aCallback(EntryReader&)` on each entry from first to last.
    // Callback should not store `EntryReader`, as it may become invalid after
    // this thread-safe call.
    template <typename Callback>
    void ForEach(Callback&& aCallback) const {
      for (EntryReader reader : *this) {
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
      baseprofiler::detail::BaseProfilerAutoLock lock(mMutex);
      if (MOZ_LIKELY(mMaybeUnderlyingBuffer)) {
        Reader reader(*this);
        return std::forward<Callback>(aCallback)(&reader);
      }
    }
    return std::forward<Callback>(aCallback)(nullptr);
  }

  // Call `aCallback(BlocksRingBuffer::EntryReader&)` on each item.
  // Callback should not store `EntryReader`, because it may become invalid
  // after this call.
  template <typename Callback>
  void ReadEach(Callback&& aCallback) const {
    Read([&](Reader* aReader) {
      if (MOZ_LIKELY(aReader)) {
        aReader->ForEach(aCallback);
      }
    });
  }

  // Call `aCallback(Maybe<BlocksRingBuffer::EntryReader>&&)` on the entry at
  // the given BlockIndex; The `Maybe` will be `Nothing` if out-of-session, or
  // if that entry doesn't exist anymore, or if we've reached just past the
  // last entry. Return whatever `aCallback` returns. Callback should not
  // store `EntryReader`, because it may become invalid after this call.
  template <typename Callback>
  auto ReadAt(BlockIndex aBlockIndex, Callback&& aCallback) const {
    baseprofiler::detail::BaseProfilerAutoLock lock(mMutex);
    MOZ_ASSERT(aBlockIndex <= mNextWriteIndex);
    Maybe<EntryReader> maybeEntryReader;
    if (MOZ_LIKELY(mMaybeUnderlyingBuffer) && aBlockIndex >= mFirstReadIndex &&
        aBlockIndex < mNextWriteIndex) {
      AssertBlockIndexIsValid(aBlockIndex);
      maybeEntryReader.emplace(ReaderInBlockAt(aBlockIndex));
    }
    return std::forward<Callback>(aCallback)(std::move(maybeEntryReader));
  }

  // Class used to write an entry contents.
  // Created through `Put()`, lives within a lock guard lifetime.
  class MOZ_RAII EntryWriter : public BufferWriter {
   public:
    // Disallow copying, moving, and assignments.
    EntryWriter(const EntryWriter& aOther) = delete;
    EntryWriter& operator=(const EntryWriter& aOther) = delete;
    EntryWriter(EntryWriter&& aOther) = delete;
    EntryWriter& operator=(EntryWriter&& aOther) = delete;

#ifdef DEBUG
    ~EntryWriter() {
      // We expect the caller to completely fill the entry.
      // (Or at least pretend to, by moving this iterator to the end.)
      MOZ_ASSERT(RemainingBytes() == 0);
      // No EntryWriter should live outside of a mutexed call.
      mRing.mMutex.AssertCurrentThreadOwns();
    }
#endif  // DEBUG

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

    template <typename T>
    void WriteObject(const T& aObject) {
      DebugOnly<Length> start = IndexInEntry();
      Serializer<T>::Write(*this, aObject);
      // Make sure the helper moved the iterator as expected.
      MOZ_ASSERT(IndexInEntry() == start + SumBytes(aObject));
    }

    // Allow `EntryWrite::WriteObjects()` with nothing, this could be useful
    // for generic programming.
    void WriteObjects() {}

    // Write one or more objects, sequentially.
    template <typename T0, typename... Ts>
    void WriteObjects(const T0& aT0, const Ts&... aTs) {
      WriteObject(aT0);
      WriteObjects(aTs...);
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
    // Only a BlocksRingBuffer can instantiate an EntryWriter.
    friend class BlocksRingBuffer;

    // Compute space needed for a block that can contain an entry of size
    // `aEntryBytes`.
    static Length BlockSizeForEntrySize(Length aEntryBytes) {
      return aEntryBytes +
             static_cast<Length>(BufferWriter::ULEB128Size(aEntryBytes));
    }

    EntryWriter(BlocksRingBuffer& aRing, BlockIndex aBlockIndex,
                Length aEntryBytes)
        : BufferWriter(aRing.mMaybeUnderlyingBuffer->mBuffer.WriterAt(
              Index(aBlockIndex))),
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

  // Main function to write entries.
  // Reserve `aCallbackBytes()` bytes, call `aCallback()` with a pointer to an
  // on-stack temporary EntryWriter (nullptr when out-of-session), and return
  // whatever `aCallback` returns. Callback should not store `EntryWriter`,
  // because it may become invalid after this thread-safe call.
  // Note: `aCallbackBytes` is a callback instead of a simple value, to delay
  // this potentially-expensive computation until after we're checked that we're
  // in-session; use `Put(Length, Callback)` below if you know the size already.
  template <typename CallbackBytes, typename Callback>
  auto ReserveAndPut(CallbackBytes aCallbackBytes, Callback&& aCallback) {
    {  // Locked block.
      baseprofiler::detail::BaseProfilerAutoLock lock(mMutex);
      if (MOZ_LIKELY(mMaybeUnderlyingBuffer)) {
        Length bytes = std::forward<CallbackBytes>(aCallbackBytes)();
        // Don't allow even half of the buffer length. More than that would
        // probably be unreasonable, and much more would risk having an entry
        // wrapping around and overwriting itself!
        MOZ_RELEASE_ASSERT(
            bytes < mMaybeUnderlyingBuffer->mBuffer.BufferLength().Value() / 2);
        // COmpute block size from the requested entry size.
        const Length blockBytes = EntryWriter::BlockSizeForEntrySize(bytes);
        // We will put this new block at the end of the current buffer.
        const BlockIndex blockIndex = mNextWriteIndex;
        // Compute the end of this new block...
        const Index blockEnd = Index(blockIndex) + blockBytes;
        // ... which is where the following block will go.
        mNextWriteIndex = BlockIndex(blockEnd);
        while (blockEnd >
               Index(mFirstReadIndex) +
                   mMaybeUnderlyingBuffer->mBuffer.BufferLength().Value()) {
          // About to trample on an old block.
          EntryReader reader = ReaderInBlockAt(mFirstReadIndex);
          // Call provided entry destructor for that entry.
          if (mMaybeUnderlyingBuffer->mEntryDestructor) {
            mMaybeUnderlyingBuffer->mEntryDestructor(reader);
          }
          mMaybeUnderlyingBuffer->mClearedBlockCount += 1;
          MOZ_ASSERT(reader.CurrentIndex() <= Index(reader.NextBlockIndex()));
          // Move the buffer reading start past this cleared block.
          mFirstReadIndex = reader.NextBlockIndex();
        }
        mMaybeUnderlyingBuffer->mPushedBlockCount += 1;
        // Finally, let aCallback write into the entry.
        EntryWriter entryWriter(*this, blockIndex, bytes);
        return std::forward<Callback>(aCallback)(&entryWriter);
      }
    }  // End of locked block.
    // Out-of-session, just invoke the callback with nullptr, no need to hold
    // the lock.
    return std::forward<Callback>(aCallback)(nullptr);
  }

  // Add a new entry of known size, call `aCallback` with a pointer to a
  // temporary EntryWriter (can be null when out-of-session), and return
  // whatever `aCallback` returns. Callback should not store the `EntryWriter`,
  // as it may become invalid after this thread-safe call.
  template <typename Callback>
  auto Put(Length aBytes, Callback&& aCallback) {
    return ReserveAndPut([aBytes]() { return aBytes; },
                         std::forward<Callback>(aCallback));
  }

  // Add a new entry copied from the given buffer, return block index.
  BlockIndex PutFrom(const void* aSrc, Length aBytes) {
    return ReserveAndPut([aBytes]() { return aBytes; },
                         [&](EntryWriter* aEntryWriter) {
                           if (MOZ_LIKELY(aEntryWriter)) {
                             aEntryWriter->Write(aSrc, aBytes);
                             return aEntryWriter->CurrentBlockIndex();
                           }
                           // Out-of-session, return "empty" BlockIndex.
                           return BlockIndex{};
                         });
  }

  // Add a new single entry with *all* given object (using a Serializer for
  // each), return block index.
  template <typename... Ts>
  BlockIndex PutObjects(const Ts&... aTs) {
    static_assert(sizeof...(Ts) > 0,
                  "PutObjects must be given at least one object.");
    return ReserveAndPut([&]() { return SumBytes(aTs...); },
                         [&](EntryWriter* aEntryWriter) {
                           if (MOZ_LIKELY(aEntryWriter)) {
                             aEntryWriter->WriteObjects(aTs...);
                             return aEntryWriter->CurrentBlockIndex();
                           }
                           // Out-of-session, return "empty" BlockIndex.
                           return BlockIndex{};
                         });
  }

  // Add a new entry copied from the given object, return block index.
  template <typename T>
  BlockIndex PutObject(const T& aOb) {
    return PutObjects(aOb);
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
    if (mMaybeUnderlyingBuffer->mEntryDestructor) {
      // We have an entry destructor, destroy entries before aBlockIndex.
      Reader reader(*this);
      BlockIterator it = reader.begin();
      for (; it.CurrentBlockIndex() < aBlockIndex; ++it) {
        MOZ_ASSERT(it.CurrentBlockIndex() < reader.end().CurrentBlockIndex());
        EntryReader reader = *it;
        mMaybeUnderlyingBuffer->mEntryDestructor(reader);
        mMaybeUnderlyingBuffer->mClearedBlockCount += 1;
      }
      MOZ_ASSERT(it.CurrentBlockIndex() == aBlockIndex);
    } else {
      // No entry destructor, just count skipped entries.
      Reader reader(*this);
      BlockIterator it = reader.begin();
      for (; it.CurrentBlockIndex() < aBlockIndex; ++it) {
        MOZ_ASSERT(it.CurrentBlockIndex() < reader.end().CurrentBlockIndex());
        mMaybeUnderlyingBuffer->mClearedBlockCount += 1;
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
    if (!mMaybeUnderlyingBuffer) {
      printf("empty BlocksRingBuffer\n");
      return;
    }
    using ULL = unsigned long long;
    printf("start=%llu (%llu) end=%llu (%llu) - ", ULL(Index(mFirstReadIndex)),
           ULL(Index(mFirstReadIndex) &
               (mMaybeUnderlyingBuffer->mBuffer.BufferLength().Value() - 1)),
           ULL(Index(mNextWriteIndex)),
           ULL(Index(mNextWriteIndex) &
               (mMaybeUnderlyingBuffer->mBuffer.BufferLength().Value() - 1)));
    mMaybeUnderlyingBuffer->mBuffer.Dump();
  }
#endif  // DEBUG

 private:
  // In DEBUG mode, assert that `aBlockIndex` is a valid index for a live block.
  // (Not just in range, but points exactly at the start of a block.)
  // Slow, so avoid it for internal checks; this is more to check what callers
  // provide us.
  void AssertBlockIndexIsValid(BlockIndex aBlockIndex) const {
#ifdef DEBUG
    mMutex.AssertCurrentThreadOwns();
    MOZ_ASSERT(aBlockIndex >= mFirstReadIndex);
    MOZ_ASSERT(aBlockIndex < mNextWriteIndex);
    // Quick check (default), or slow check (change '1' to '0') below:
#  if 1
    // Quick check that this looks like a valid block start.
    // Read the entry size at the start of the block.
    BufferReader br =
        mMaybeUnderlyingBuffer->mBuffer.ReaderAt(Index(aBlockIndex));
    Length entryBytes = br.ReadULEB128<Length>();
    // It should be between 1 and half of the buffer length max.
    MOZ_ASSERT(entryBytes > 0);
    MOZ_ASSERT(entryBytes <
               mMaybeUnderlyingBuffer->mBuffer.BufferLength().Value() / 2);
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

  // In DEBUG mode, assert that `aBlockIndex` is a valid index for a live block,
  // or is just past-the-end. (Not just in range, but points exactly at the
  // start of a block.) Slow, so avoid it for internal checks; this is more to
  // check what callers provide us.
  void AssertBlockIndexIsValidOrEnd(BlockIndex aBlockIndex) const {
#ifdef DEBUG
    mMutex.AssertCurrentThreadOwns();
    if (aBlockIndex == mNextWriteIndex) {
      return;
    }
    AssertBlockIndexIsValid(aBlockIndex);
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
    if (!mMaybeUnderlyingBuffer) {
      return;
    }
    if (mMaybeUnderlyingBuffer->mEntryDestructor) {
      // We have an entry destructor, destroy all the things!
      Reader reader(*this);
      reader.ForEach([this](EntryReader& aReader) {
        mMaybeUnderlyingBuffer->mEntryDestructor(aReader);
      });
    }
    mMaybeUnderlyingBuffer->mClearedBlockCount =
        mMaybeUnderlyingBuffer->mPushedBlockCount;
  }

  // Clear all entries, calling entry destructor (if any), and move read index
  // to the end so that these entries cannot be read anymore.
  void ClearAllEntries() {
    mMutex.AssertCurrentThreadOwns();
    if (!mMaybeUnderlyingBuffer) {
      return;
    }
    DestroyAllEntries();
    // Move read index to write index, so there's effectively no more entries
    // that can be read. (Not setting both to 0, in case user is keeping
    // `BlockIndex`'es to old entries.)
    mFirstReadIndex = mNextWriteIndex;
  }

  // If there is an underlying buffer (with optional entry destructor), destroy
  // all entries, move read index to the end, and discard the buffer and entry
  // destructor. This BlocksRingBuffer will now gracefully reject all API calls,
  // and is in a state where a new underlying buffer&entry deleter may be
  // installed.
  void ResetUnderlyingBuffer() {
    if (!mMaybeUnderlyingBuffer) {
      return;
    }
    ClearAllEntries();
    mMaybeUnderlyingBuffer.reset();
  }

  // Used to de/serialize a BlocksRingBuffer (e.g., containing a backtrace).
  friend struct Serializer<BlocksRingBuffer>;
  friend struct Deserializer<BlocksRingBuffer>;
  friend struct Serializer<UniquePtr<BlocksRingBuffer>>;
  friend struct Deserializer<UniquePtr<BlocksRingBuffer>>;

  // Mutex guarding the following members.
  mutable baseprofiler::detail::BaseProfilerMutex mMutex;

  struct UnderlyingBuffer {
    // Create a buffer of the given length.
    explicit UnderlyingBuffer(PowerOfTwo<Length> aLength) : mBuffer(aLength) {}

    // Take ownership of an existing buffer.
    UnderlyingBuffer(UniquePtr<Buffer::Byte[]> aExistingBuffer,
                     PowerOfTwo<Length> aLength)
        : mBuffer(std::move(aExistingBuffer), aLength) {}

    // Use an externally-owned buffer.
    UnderlyingBuffer(Buffer::Byte* aExternalBuffer, PowerOfTwo<Length> aLength)
        : mBuffer(aExternalBuffer, aLength) {}

    // Create a buffer of the given length.
    template <typename EntryDestructor>
    explicit UnderlyingBuffer(PowerOfTwo<Length> aLength,
                              EntryDestructor&& aEntryDestructor)
        : mBuffer(aLength),
          mEntryDestructor(std::forward<EntryDestructor>(aEntryDestructor)) {}

    // Take ownership of an existing buffer.
    template <typename EntryDestructor>
    explicit UnderlyingBuffer(UniquePtr<Buffer::Byte[]> aExistingBuffer,
                              PowerOfTwo<Length> aLength,
                              EntryDestructor&& aEntryDestructor)
        : mBuffer(std::move(aExistingBuffer), aLength),
          mEntryDestructor(std::forward<EntryDestructor>(aEntryDestructor)) {}

    // Use an externally-owned buffer.
    template <typename EntryDestructor>
    explicit UnderlyingBuffer(Buffer::Byte* aExternalBuffer,
                              PowerOfTwo<Length> aLength,
                              EntryDestructor&& aEntryDestructor)
        : mBuffer(aExternalBuffer, aLength),
          mEntryDestructor(std::forward<EntryDestructor>(aEntryDestructor)) {}

    // Only allow move-construction.
    UnderlyingBuffer(UnderlyingBuffer&&) = default;

    // Copies and move-assignment are explictly disallowed.
    UnderlyingBuffer(const UnderlyingBuffer&) = delete;
    UnderlyingBuffer& operator=(const UnderlyingBuffer&) = delete;
    UnderlyingBuffer& operator=(UnderlyingBuffer&&) = delete;

    // Underlying circular byte buffer.
    Buffer mBuffer;
    // If set, function to call for each entry that is about to be destroyed.
    std::function<void(EntryReader&)> mEntryDestructor;

    // Statistics.
    uint64_t mPushedBlockCount = 0;
    uint64_t mClearedBlockCount = 0;
  };

  // Underlying buffer, with entry destructor and stats.
  // Only valid during in-session period.
  Maybe<UnderlyingBuffer> mMaybeUnderlyingBuffer;

  // Index to the first block to be read (or cleared). Initialized to 1 because
  // 0 is reserved for the "empty" BlockIndex value. Kept between sessions, so
  // that stored indices from one session will be gracefully denied in future
  // sessions.
  BlockIndex mFirstReadIndex = BlockIndex(Index(1));
  // Index where the next new block should be allocated. Initialized to 1
  // because 0 is reserved for the "empty" BlockIndex value. Kept between
  // sessions, so that stored indices from one session will be gracefully denied
  // in future sessions.
  BlockIndex mNextWriteIndex = BlockIndex(Index(1));
};

// ============================================================================
// Serializer and Deserializer ready-to-use specializations.

// ----------------------------------------------------------------------------
// Trivially-copyable types (default)

// The default implementation works for all trivially-copyable types (e.g.,
// PODs).
//
// Usage: `aEW.WriteObject(123);`.
//
// Raw pointers, though trivially-copyable, are explictly forbidden when writing
// (to avoid unexpected leaks/UAFs), instead use one of
// `WrapBlocksRingBufferLiteralCStringPointer`,
// `WrapBlocksRingBufferUnownedCString`, or `WrapBlocksRingBufferRawPointer` as
// needed.
template <typename T>
struct BlocksRingBuffer::Serializer {
  static_assert(std::is_trivially_copyable<T>::value,
                "Serializer only works with trivially-copyable types by "
                "default, use/add specialization for other types.");

  static constexpr Length Bytes(const T&) { return sizeof(T); }

  static void Write(EntryWriter& aEW, const T& aT) {
    static_assert(!std::is_pointer<T>::value,
                  "Serializer won't write raw pointers by default, use "
                  "WrapBlocksRingBufferRawPointer or other.");
    aEW.Write(&aT, sizeof(T));
  }
};

// Usage: `aER.ReadObject<int>();` or `int x; aER.ReadIntoObject(x);`.
template <typename T>
struct BlocksRingBuffer::Deserializer {
  static_assert(std::is_trivially_copyable<T>::value,
                "Deserializer only works with trivially-copyable types by "
                "default, use/add specialization for other types.");

  static void ReadInto(EntryReader& aER, T& aT) { aER.Read(&aT, sizeof(T)); }

  static T Read(EntryReader& aER) {
    // Note that this creates a default `T` first, and then overwrites it with
    // bytes from the buffer. Trivially-copyable types support this without UB.
    T ob;
    ReadInto(aER, ob);
    return ob;
  }
};

// ----------------------------------------------------------------------------
// Strip const/volatile/reference from types.

// Automatically strip `const`.
template <typename T>
struct BlocksRingBuffer::Serializer<const T>
    : public BlocksRingBuffer::Serializer<T> {};

template <typename T>
struct BlocksRingBuffer::Deserializer<const T>
    : public BlocksRingBuffer::Deserializer<T> {};

// Automatically strip `volatile`.
template <typename T>
struct BlocksRingBuffer::Serializer<volatile T>
    : public BlocksRingBuffer::Serializer<T> {};

template <typename T>
struct BlocksRingBuffer::Deserializer<volatile T>
    : public BlocksRingBuffer::Deserializer<T> {};

// Automatically strip `lvalue-reference`.
template <typename T>
struct BlocksRingBuffer::Serializer<T&>
    : public BlocksRingBuffer::Serializer<T> {};

template <typename T>
struct BlocksRingBuffer::Deserializer<T&>
    : public BlocksRingBuffer::Deserializer<T> {};

// Automatically strip `rvalue-reference`.
template <typename T>
struct BlocksRingBuffer::Serializer<T&&>
    : public BlocksRingBuffer::Serializer<T> {};

template <typename T>
struct BlocksRingBuffer::Deserializer<T&&>
    : public BlocksRingBuffer::Deserializer<T> {};

// ----------------------------------------------------------------------------
// BlockIndex

// BlockIndex, serialized as the underlying value.
template <>
struct BlocksRingBuffer::Serializer<BlocksRingBuffer::BlockIndex> {
  static constexpr Length Bytes(const BlockIndex& aBlockIndex) {
    return sizeof(BlockIndex);
  }

  static void Write(EntryWriter& aEW, const BlockIndex& aBlockIndex) {
    aEW.Write(&aBlockIndex, sizeof(aBlockIndex));
  }
};

template <>
struct BlocksRingBuffer::Deserializer<BlocksRingBuffer::BlockIndex> {
  static void ReadInto(EntryReader& aER, BlockIndex& aBlockIndex) {
    aER.Read(&aBlockIndex, sizeof(aBlockIndex));
  }

  static BlockIndex Read(EntryReader& aER) {
    BlockIndex blockIndex;
    ReadInto(aER, blockIndex);
    return blockIndex;
  }
};

// ----------------------------------------------------------------------------
// Literal C string pointer

// Wrapper around a pointer to a literal C string.
template <BlocksRingBuffer::Length NonTerminalCharacters>
struct BlocksRingBufferLiteralCStringPointer {
  const char* mCString;
};

// Wrap a pointer to a literal C string.
template <BlocksRingBuffer::Length CharactersIncludingTerminal>
BlocksRingBufferLiteralCStringPointer<CharactersIncludingTerminal - 1>
WrapBlocksRingBufferLiteralCStringPointer(
    const char (&aCString)[CharactersIncludingTerminal]) {
  return {aCString};
}

// Literal C strings, serialized as the raw pointer because it is unique and
// valid for the whole program lifetime.
//
// Usage: `aEW.WriteObject(WrapBlocksRingBufferLiteralCStringPointer("hi"));`.
//
// No deserializer is provided for this type, instead it must be deserialized as
// a raw pointer: `aER.ReadObject<const char*>();`
template <BlocksRingBuffer::Length CharactersIncludingTerminal>
struct BlocksRingBuffer::Deserializer<
    BlocksRingBufferLiteralCStringPointer<CharactersIncludingTerminal>> {
  static constexpr Length Bytes(const BlocksRingBufferLiteralCStringPointer<
                                CharactersIncludingTerminal>&) {
    // We're only storing a pointer, its size is independent from the pointer
    // value.
    return sizeof(const char*);
  }

  static void Write(
      EntryWriter& aEW,
      const BlocksRingBufferLiteralCStringPointer<CharactersIncludingTerminal>&
          aWrapper) {
    // Write the pointer *value*, not the string contents.
    aEW.Write(&aWrapper.mCString, sizeof(aWrapper.mCString));
  }
};

// ----------------------------------------------------------------------------
// C string contents

// Wrapper around a pointer to a C string whose contents will be serialized.
struct BlocksRingBufferUnownedCString {
  const char* mCString;
};

// Wrap a pointer to a C string whose contents will be serialized.
inline BlocksRingBufferUnownedCString WrapBlocksRingBufferUnownedCString(
    const char* aCString) {
  return {aCString};
}

// The contents of a (probably) unowned C string are serialized as the number of
// characters (encoded as ULEB128) and all the characters in the string. The
// terminal '\0' is omitted.
//
// Usage: `aEW.WriteObject(WrapBlocksRingBufferUnownedCString(str.c_str()))`.
//
// No deserializer is provided for this pointer type, instead it must be
// deserialized as one of the other string types that manages its contents,
// e.g.: `aER.ReadObject<std::string>();`
template <>
struct BlocksRingBuffer::Serializer<BlocksRingBufferUnownedCString> {
  static Length Bytes(const BlocksRingBufferUnownedCString& aS) {
    const auto len = static_cast<Length>(strlen(aS.mCString));
    return EntryWriter::ULEB128Size(len) + len;
  }

  static void Write(EntryWriter& aEW,
                    const BlocksRingBufferUnownedCString& aS) {
    const auto len = static_cast<Length>(strlen(aS.mCString));
    aEW.WriteULEB128(len);
    aEW.Write(aS.mCString, len);
  }
};

// ----------------------------------------------------------------------------
// Raw pointers

// Wrapper around a pointer to be serialized as the raw pointer value.
template <typename T>
struct BlocksRingBufferRawPointer {
  T* mRawPointer;
};

// Wrap a pointer to be serialized as the raw pointer value.
template <typename T>
BlocksRingBufferRawPointer<T> WrapBlocksRingBufferRawPointer(T* aRawPointer) {
  return {aRawPointer};
}

// Raw pointers are serialized as the raw pointer value.
//
// Usage: `aEW.WriteObject(WrapBlocksRingBufferRawPointer(ptr));`
//
// The wrapper is compulsory when writing pointers (to avoid unexpected
// leaks/UAFs), but reading can be done straight into a raw pointer object,
// e.g.: `aER.ReadObject<Foo*>;`.
template <typename T>
struct BlocksRingBuffer::Serializer<BlocksRingBufferRawPointer<T>> {
  template <typename U>
  static constexpr Length Bytes(const U&) {
    return sizeof(T*);
  }

  static void Write(EntryWriter& aEW,
                    const BlocksRingBufferRawPointer<T>& aWrapper) {
    aEW.Write(&aWrapper.mRawPointer, sizeof(aWrapper.mRawPointer));
  }
};

// Usage: `aER.ReadObject<Foo*>;` or `Foo* p; aER.ReadIntoObject(p);`, no
// wrapper necessary.
template <typename T>
struct BlocksRingBuffer::Deserializer<BlocksRingBufferRawPointer<T>> {
  static void ReadInto(EntryReader& aER, T*& aPtr) {
    aER.Read(&aPtr, sizeof(aPtr));
  }

  static T* Read(EntryReader& aER) {
    T* ptr;
    ReadInto(aER, ptr);
    return ptr;
  }
};

// ----------------------------------------------------------------------------
// std::string contents

// std::string contents are serialized as the number of characters (encoded as
// ULEB128) and all the characters in the string. The terminal '\0' is omitted.
//
// Usage: `std::string s = ...; aEW.WriteObject(s);`
template <>
struct BlocksRingBuffer::Serializer<std::string> {
  static Length Bytes(const std::string& aS) {
    const auto len = aS.length();
    return EntryWriter::ULEB128Size(len) + static_cast<Length>(len);
  }

  static void Write(EntryWriter& aEW, const std::string& aS) {
    const auto len = aS.length();
    aEW.WriteULEB128(len);
    aEW.Write(aS.c_str(), len);
  }
};

// Usage: `std::string s = aEW.ReadObject<std::string>(s);` or
// `std::string s; aER.ReadIntoObject(s);`
template <>
struct BlocksRingBuffer::Deserializer<std::string> {
  static void ReadInto(EntryReader& aER, std::string& aS) {
    const auto len = aER.ReadULEB128<std::string::size_type>();
    // Assign to `aS` by using iterators.
    // (`aER+0` so we get the same iterator type as `aER+len`.)
    aS.assign(aER + 0, aER + len);
    aER += len;
  }

  static std::string Read(EntryReader& aER) {
    const auto len = aER.ReadULEB128<std::string::size_type>();
    // Construct a string by using iterators.
    // (`aER+0` so we get the same iterator type as `aER+len`.)
    std::string s(aER + 0, aER + len);
    aER += len;
    return s;
  }
};

// ----------------------------------------------------------------------------
// mozilla::UniqueFreePtr<CHAR>

// UniqueFreePtr<CHAR>, which points at a string allocated with `malloc`
// (typically generated by `strdup()`), is serialized as the number of
// *bytes* (encoded as ULEB128) and all the characters in the string. The
// null terminator is omitted.
// `CHAR` can be any type that has a specialization for
// `std::char_traits<CHAR>::length(const CHAR*)`.
//
// Note: A nullptr pointer will be serialized like an empty string, so when
// deserializing it will result in an allocated buffer only containing a
// single null terminator.
template <typename CHAR>
struct BlocksRingBuffer::Serializer<UniqueFreePtr<CHAR>> {
  static Length Bytes(const UniqueFreePtr<CHAR>& aS) {
    if (!aS) {
      // Null pointer, store it as if it was an empty string (so: 0 bytes).
      return EntryWriter::ULEB128Size(0u);
    }
    // Note that we store the size in *bytes*, not in number of characters.
    const auto bytes = static_cast<Length>(
        std::char_traits<CHAR>::length(aS.get()) * sizeof(CHAR));
    return EntryWriter::ULEB128Size(bytes) + bytes;
  }

  static void Write(EntryWriter& aEW, const UniqueFreePtr<CHAR>& aS) {
    if (!aS) {
      // Null pointer, store it as if it was an empty string (so we write a
      // length of 0 bytes).
      aEW.WriteULEB128(0u);
      return;
    }
    // Note that we store the size in *bytes*, not in number of characters.
    const auto bytes = static_cast<Length>(
        std::char_traits<CHAR>::length(aS.get()) * sizeof(CHAR));
    aEW.WriteULEB128(bytes);
    aEW.Write(aS.get(), bytes);
  }
};

template <typename CHAR>
struct BlocksRingBuffer::Deserializer<UniqueFreePtr<CHAR>> {
  static void ReadInto(EntryReader& aER, UniqueFreePtr<CHAR>& aS) {
    aS = Read(aER);
  }

  static UniqueFreePtr<CHAR> Read(EntryReader& aER) {
    // Read the number of *bytes* that follow.
    const auto bytes = aER.ReadULEB128<Length>();
    // We need a buffer of the non-const character type.
    using NC_CHAR = std::remove_const_t<CHAR>;
    // We allocate the required number of bytes, plus one extra character for
    // the null terminator.
    NC_CHAR* buffer = static_cast<NC_CHAR*>(malloc(bytes + sizeof(NC_CHAR)));
    // Copy the characters into the buffer.
    aER.Read(buffer, bytes);
    // And append a null terminator.
    buffer[bytes / sizeof(NC_CHAR)] = NC_CHAR(0);
    return UniqueFreePtr<CHAR>(buffer);
  }
};

// ----------------------------------------------------------------------------
// std::tuple

// std::tuple is serialized as a sequence of each recursively-serialized item.
//
// This is equivalent to manually serializing each item, so reading/writing
// tuples is equivalent to reading/writing their elements in order, e.g.:
// ```
// std::tuple<int, std::string> is = ...;
// aEW.WriteObject(is); // Write the tuple, equivalent to:
// aEW.WriteObject(/* int */ std::get<0>(is), /* string */ std::get<1>(is));
// ...
// // Reading back can be done directly into a tuple:
// auto is = aER.ReadObject<std::tuple<int, std::string>>();
// // Or each item could be read separately:
// auto i = aER.ReadObject<int>(); auto s = aER.ReadObject<std::string>();
// ```
template <typename... Ts>
struct BlocksRingBuffer::Serializer<std::tuple<Ts...>> {
 private:
  template <size_t... Is>
  static Length TupleBytes(const std::tuple<Ts...>& aTuple,
                           std::index_sequence<Is...>) {
    Length bytes = 0;
    // This is pre-C++17 fold trick, using `initializer_list` to unpack the `Is`
    // variadic pack, and the comma operator to:
    // (work on each tuple item, generate an int for the list). Note that the
    // `initializer_list` is unused; the compiler will nicely optimize it away.
    // `std::index_sequence` is just a way to have `Is` be a variadic pack
    // containing numbers between 0 and (tuple size - 1).
    // The compiled end result is like doing:
    //   bytes += SumBytes(get<0>(aTuple));
    //   bytes += SumBytes(get<1>(aTuple));
    //   ...
    //   bytes += SumBytes(get<sizeof...(aTuple) - 1>(aTuple));
    // See https://articles.emptycrate.com/2016/05/14/folds_in_cpp11_ish.html
    // TODO: Replace with C++17 fold.
    Unused << std::initializer_list<int>{
        (bytes += SumBytes(std::get<Is>(aTuple)), 0)...};
    return bytes;
  }

  template <size_t... Is>
  static void TupleWrite(EntryWriter& aEW, const std::tuple<Ts...>& aTuple,
                         std::index_sequence<Is...>) {
    // TODO: Replace with C++17 fold.
    Unused << std::initializer_list<int>{
        (aEW.WriteObject(std::get<Is>(aTuple)), 0)...};
  }

 public:
  static Length Bytes(const std::tuple<Ts...>& aTuple) {
    // Generate a 0..N-1 index pack, we'll add the sizes of each item.
    return TupleBytes(aTuple, std::index_sequence_for<Ts...>());
  }

  static void Write(EntryWriter& aEW, const std::tuple<Ts...>& aTuple) {
    // Generate a 0..N-1 index pack, we'll write each item.
    TupleWrite(aEW, aTuple, std::index_sequence_for<Ts...>());
  }
};

template <typename... Ts>
struct BlocksRingBuffer::Deserializer<std::tuple<Ts...>> {
  static void ReadInto(EntryReader& aER, std::tuple<Ts...>& aTuple) {
    aER.Read(&aTuple, Bytes(aTuple));
  }

  static std::tuple<Ts...> Read(EntryReader& aER) {
    // Note that this creates default `Ts` first, and then overwrites them.
    std::tuple<Ts...> ob;
    ReadInto(aER, ob);
    return ob;
  }
};

// ----------------------------------------------------------------------------
// mozilla::Tuple

// Tuple is serialized as a sequence of each recursively-serialized
// item.
//
// This is equivalent to manually serializing each item, so reading/writing
// tuples is equivalent to reading/writing their elements in order, e.g.:
// ```
// Tuple<int, std::string> is = ...;
// aEW.WriteObject(is); // Write the Tuple, equivalent to:
// aEW.WriteObject(/* int */ std::get<0>(is), /* string */ std::get<1>(is));
// ...
// // Reading back can be done directly into a Tuple:
// auto is = aER.ReadObject<Tuple<int, std::string>>();
// // Or each item could be read separately:
// auto i = aER.ReadObject<int>(); auto s = aER.ReadObject<std::string>();
// ```
template <typename... Ts>
struct BlocksRingBuffer::Serializer<Tuple<Ts...>> {
 private:
  template <size_t... Is>
  static Length TupleBytes(const Tuple<Ts...>& aTuple,
                           std::index_sequence<Is...>) {
    Length bytes = 0;
    // TODO: Replace with C++17 fold.
    Unused << std::initializer_list<int>{
        (bytes += SumBytes(Get<Is>(aTuple)), 0)...};
    return bytes;
  }

  template <size_t... Is>
  static void TupleWrite(EntryWriter& aEW, const Tuple<Ts...>& aTuple,
                         std::index_sequence<Is...>) {
    // TODO: Replace with C++17 fold.
    Unused << std::initializer_list<int>{
        (aEW.WriteObject(Get<Is>(aTuple)), 0)...};
  }

 public:
  static Length Bytes(const Tuple<Ts...>& aTuple) {
    // Generate a 0..N-1 index pack, we'll add the sizes of each item.
    return TupleBytes(aTuple, std::index_sequence_for<Ts...>());
  }

  static void Write(EntryWriter& aEW, const Tuple<Ts...>& aTuple) {
    // Generate a 0..N-1 index pack, we'll write each item.
    TupleWrite(aEW, aTuple, std::index_sequence_for<Ts...>());
  }
};

template <typename... Ts>
struct BlocksRingBuffer::Deserializer<Tuple<Ts...>> {
  static void ReadInto(EntryReader& aER, Tuple<Ts...>& aTuple) {
    aER.Read(&aTuple, Bytes(aTuple));
  }

  static Tuple<Ts...> Read(EntryReader& aER) {
    // Note that this creates default `Ts` first, and then overwrites them.
    Tuple<Ts...> ob;
    ReadInto(aER, ob);
    return ob;
  }
};

// ----------------------------------------------------------------------------
// mozilla::Span

// Span. All elements are serialized in sequence.
// The caller is assumed to know the number of elements (they may manually
// write&read it before the span if needed).
// Similar to tuples, reading/writing spans is equivalent to reading/writing
// their elements in order.
template <class T, size_t N>
struct BlocksRingBuffer::Serializer<Span<T, N>> {
  static Length Bytes(const Span<T, N>& aSpan) {
    Length bytes = 0;
    for (const T& element : aSpan) {
      bytes += SumBytes(element);
    }
    return bytes;
  }

  static void Write(EntryWriter& aEW, const Span<T, N>& aSpan) {
    for (const T& element : aSpan) {
      aEW.WriteObject(element);
    }
  }
};

template <class T, size_t N>
struct BlocksRingBuffer::Deserializer<Span<T, N>> {
  // Read elements back into span pointing at a pre-allocated buffer.
  static void ReadInto(EntryReader& aER, Span<T, N>& aSpan) {
    for (T& element : aSpan) {
      aER.ReadIntoObject(element);
    }
  }

  // A Span does not own its data, this would probably leak so we forbid this.
  static Span<T, N> Read(EntryReader& aER) = delete;
};

// ----------------------------------------------------------------------------
// mozilla::Maybe

// Maybe<T> is serialized as one byte containing either 'm' (Nothing),
// or 'M' followed by the recursively-serialized `T` object.
template <typename T>
struct BlocksRingBuffer::Serializer<Maybe<T>> {
  static Length Bytes(const Maybe<T>& aMaybe) {
    // 1 byte to store nothing/something flag, then object size if present.
    return aMaybe.isNothing() ? 1 : (1 + SumBytes(aMaybe.ref()));
  }

  static void Write(EntryWriter& aEW, const Maybe<T>& aMaybe) {
    // 'm'/'M' is just an arbitrary 1-byte value to distinguish states.
    if (aMaybe.isNothing()) {
      aEW.WriteObject<char>('m');
    } else {
      aEW.WriteObject<char>('M');
      // Use the Serializer for the contained type.
      aEW.WriteObject(aMaybe.ref());
    }
  }
};

template <typename T>
struct BlocksRingBuffer::Deserializer<Maybe<T>> {
  static void ReadInto(EntryReader& aER, Maybe<T>& aMaybe) {
    char c = aER.ReadObject<char>();
    if (c == 'm') {
      aMaybe.reset();
    } else {
      MOZ_ASSERT(c == 'M');
      // If aMaybe is empty, create a default `T` first, to be overwritten.
      // Otherwise we'll just overwrite whatever was already there.
      if (aMaybe.isNothing()) {
        aMaybe.emplace();
      }
      // Use the Deserializer for the contained type.
      aER.ReadIntoObject(aMaybe.ref());
    }
  }

  static Maybe<T> Read(EntryReader& aER) {
    Maybe<T> maybe;
    char c = aER.ReadObject<char>();
    MOZ_ASSERT(c == 'M' || c == 'm');
    if (c == 'M') {
      // Note that this creates a default `T` inside the Maybe first, and then
      // overwrites it.
      maybe = Some(T{});
      // Use the Deserializer for the contained type.
      aER.ReadIntoObject(maybe.ref());
    }
    return maybe;
  }
};

// ----------------------------------------------------------------------------
// mozilla::Variant

// Variant is serialized as the tag (0-based index of the stored type, encoded
// as ULEB128), and the recursively-serialized object.
template <typename... Ts>
struct BlocksRingBuffer::Serializer<Variant<Ts...>> {
 private:
  // Called from the fold expression in `VariantBytes()`, only the selected
  // variant will write into `aOutBytes`.
  template <size_t I>
  static void VariantIBytes(const Variant<Ts...>& aVariantTs,
                            Length& aOutBytes) {
    if (aVariantTs.template is<I>()) {
      aOutBytes =
          EntryReader::ULEB128Size(I) + SumBytes(aVariantTs.template as<I>());
    }
  }

  // Go through all variant tags, and let the selected one write the correct
  // number of bytes.
  template <size_t... Is>
  static Length VariantBytes(const Variant<Ts...>& aVariantTs,
                             std::index_sequence<Is...>) {
    Length bytes = 0;
    // TODO: Replace with C++17 fold.
    Unused << std::initializer_list<int>{
        (VariantIBytes<Is>(aVariantTs, bytes), 0)...};
    MOZ_ASSERT(bytes != 0);
    return bytes;
  }

  // Called from the fold expression in `VariantWrite()`, only the selected
  // variant will serialize the tag and object.
  template <size_t I>
  static void VariantIWrite(EntryWriter& aEW,
                            const Variant<Ts...>& aVariantTs) {
    if (aVariantTs.template is<I>()) {
      aEW.WriteULEB128(I);
      // Use the Serializer for the contained type.
      aEW.WriteObject(aVariantTs.template as<I>());
    }
  }

  // Go through all variant tags, and let the selected one serialize the correct
  // tag and object.
  template <size_t... Is>
  static void VariantWrite(EntryWriter& aEW, const Variant<Ts...>& aVariantTs,
                           std::index_sequence<Is...>) {
    // TODO: Replace with C++17 fold.
    Unused << std::initializer_list<int>{
        (VariantIWrite<Is>(aEW, aVariantTs), 0)...};
  }

 public:
  static Length Bytes(const Variant<Ts...>& aVariantTs) {
    // Generate a 0..N-1 index pack, the selected variant will return its size.
    return VariantBytes(aVariantTs, std::index_sequence_for<Ts...>());
  }

  static void Write(EntryWriter& aEW, const Variant<Ts...>& aVariantTs) {
    // Generate a 0..N-1 index pack, the selected variant will serialize itself.
    VariantWrite(aEW, aVariantTs, std::index_sequence_for<Ts...>());
  }
};

template <typename... Ts>
struct BlocksRingBuffer::Deserializer<Variant<Ts...>> {
 private:
  // Called from the fold expression in `VariantReadInto()`, only the selected
  // variant will deserialize the object.
  template <size_t I>
  static void VariantIReadInto(EntryReader& aER, Variant<Ts...>& aVariantTs,
                               unsigned aTag) {
    if (I == aTag) {
      // Ensure the variant contains the target type. Note that this may create
      // a default object.
      if (!aVariantTs.template is<I>()) {
        aVariantTs = Variant<Ts...>(VariantIndex<I>{});
      }
      aER.ReadIntoObject(aVariantTs.template as<I>());
    }
  }

  template <size_t... Is>
  static void VariantReadInto(EntryReader& aER, Variant<Ts...>& aVariantTs,
                              std::index_sequence<Is...>) {
    unsigned tag = aER.ReadULEB128<unsigned>();
    // TODO: Replace with C++17 fold.
    Unused << std::initializer_list<int>{
        (VariantIReadInto<Is>(aER, aVariantTs, tag), 0)...};
  }

 public:
  static void ReadInto(EntryReader& aER, Variant<Ts...>& aVariantTs) {
    // Generate a 0..N-1 index pack, the selected variant will deserialize
    // itself.
    VariantReadInto(aER, aVariantTs, std::index_sequence_for<Ts...>());
  }

  static Variant<Ts...> Read(EntryReader& aER) {
    // Note that this creates a default `Variant` of the first type, and then
    // overwrites it. Consider using `ReadInto` for more control if needed.
    Variant<Ts...> variant(VariantIndex<0>{});
    ReadInto(aER, variant);
    return variant;
  }
};

// ----------------------------------------------------------------------------
// BlocksRingBuffer

// A BlocksRingBuffer can hide another one!
// This will be used to store marker backtraces; They can be read back into a
// UniquePtr<BlocksRingBuffer>.
// Format: len (ULEB128) | start | end | buffer (len bytes) | pushed | cleared
// len==0 marks an out-of-session buffer, or empty buffer.
template <>
struct BlocksRingBuffer::Serializer<BlocksRingBuffer> {
  static Length Bytes(const BlocksRingBuffer& aBuffer) {
    baseprofiler::detail::BaseProfilerAutoLock lock(aBuffer.mMutex);
    if (aBuffer.mMaybeUnderlyingBuffer.isNothing()) {
      // Out-of-session, we only need 1 byte to store a length of 0.
      return ULEB128Size<Length>(0);
    }
    const auto start = Index(aBuffer.mFirstReadIndex);
    const auto end = Index(aBuffer.mNextWriteIndex);
    const auto len = end - start;
    if (len == 0) {
      // In-session but empty, also store a length of 0.
      return ULEB128Size<Length>(0);
    }
    return ULEB128Size(len) + sizeof(start) + sizeof(end) + len +
           sizeof(aBuffer.mMaybeUnderlyingBuffer->mPushedBlockCount) +
           sizeof(aBuffer.mMaybeUnderlyingBuffer->mClearedBlockCount);
  }

  static void Write(EntryWriter& aEW, const BlocksRingBuffer& aBuffer) {
    baseprofiler::detail::BaseProfilerAutoLock lock(aBuffer.mMutex);
    if (aBuffer.mMaybeUnderlyingBuffer.isNothing()) {
      // Out-of-session, only store a length of 0.
      aEW.WriteULEB128<Length>(0);
      return;
    }
    const auto start = Index(aBuffer.mFirstReadIndex);
    const auto end = Index(aBuffer.mNextWriteIndex);
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
    // Write all the bytes. TODO: Optimize with memcpy's?
    const auto readerEnd =
        aBuffer.mMaybeUnderlyingBuffer->mBuffer.ReaderAt(end);
    for (auto reader = aBuffer.mMaybeUnderlyingBuffer->mBuffer.ReaderAt(start);
         reader != readerEnd; ++reader) {
      aEW.WriteObject(*reader);
    }
    // And write stats.
    aEW.WriteObject(aBuffer.mMaybeUnderlyingBuffer->mPushedBlockCount);
    aEW.WriteObject(aBuffer.mMaybeUnderlyingBuffer->mClearedBlockCount);
  }
};

// A serialized BlocksRingBuffer can be read into an empty buffer (either
// out-of-session, or in-session with enough room).
template <>
struct BlocksRingBuffer::Deserializer<BlocksRingBuffer> {
  static void ReadInto(EntryReader& aER, BlocksRingBuffer& aBuffer) {
    // Expect an empty buffer, as we're going to overwrite it.
    MOZ_ASSERT(aBuffer.GetState().mRangeStart == aBuffer.GetState().mRangeEnd);
    // Read the stored buffer length.
    const auto len = aER.ReadULEB128<BlocksRingBuffer::Length>();
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
      aBuffer.Set(PowerOfTwo<BlocksRingBuffer::Length>(len));
      MOZ_ASSERT(aBuffer.BufferLength()->Value() >= len);
    }
    // Read start and end indices.
    const auto start = aER.ReadObject<BlocksRingBuffer::Index>();
    aBuffer.mFirstReadIndex = BlocksRingBuffer::BlockIndex(start);
    const auto end = aER.ReadObject<BlocksRingBuffer::Index>();
    aBuffer.mNextWriteIndex = BlocksRingBuffer::BlockIndex(end);
    MOZ_ASSERT(end - start == len);
    // Copy bytes into the buffer.
    const auto writerEnd =
        aBuffer.mMaybeUnderlyingBuffer->mBuffer.WriterAt(end);
    for (auto writer = aBuffer.mMaybeUnderlyingBuffer->mBuffer.WriterAt(start);
         writer != writerEnd; ++writer, ++aER) {
      *writer = *aER;
    }
    // Finally copy stats.
    aBuffer.mMaybeUnderlyingBuffer->mPushedBlockCount = aER.ReadObject<decltype(
        aBuffer.mMaybeUnderlyingBuffer->mPushedBlockCount)>();
    aBuffer.mMaybeUnderlyingBuffer->mClearedBlockCount =
        aER.ReadObject<decltype(
            aBuffer.mMaybeUnderlyingBuffer->mClearedBlockCount)>();
  }

  // We cannot output a BlocksRingBuffer object (not copyable), use `ReadInto()`
  // or `aER.ReadObject<UniquePtr<BlocksRinbBuffer>>()` instead.
  static BlocksRingBuffer Read(BlocksRingBuffer::EntryReader& aER) = delete;
};

// A BlocksRingBuffer is usually refererenced through a UniquePtr, for
// convenience we support (de)serializing that UniquePtr directly.
// This is compatible with the non-UniquePtr serialization above, with a null
// pointer being treated like an out-of-session or empty buffer; and any of
// these would be deserialized into a null pointer.
template <>
struct BlocksRingBuffer::Serializer<UniquePtr<BlocksRingBuffer>> {
  static Length Bytes(const UniquePtr<BlocksRingBuffer>& aBufferUPtr) {
    if (!aBufferUPtr) {
      // Null pointer, treat it like an empty buffer, i.e., write length of 0.
      return ULEB128Size<Length>(0);
    }
    // Otherwise write the pointed-at BlocksRingBuffer (which could be
    // out-of-session or empty.)
    return SumBytes(*aBufferUPtr);
  }

  static void Write(EntryWriter& aEW,
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
struct BlocksRingBuffer::Deserializer<UniquePtr<BlocksRingBuffer>> {
  static void ReadInto(EntryReader& aER, UniquePtr<BlocksRingBuffer>& aBuffer) {
    aBuffer = Read(aER);
  }

  static UniquePtr<BlocksRingBuffer> Read(BlocksRingBuffer::EntryReader& aER) {
    UniquePtr<BlocksRingBuffer> bufferUPtr;
    // Read the stored buffer length.
    const auto len = aER.ReadULEB128<BlocksRingBuffer::Length>();
    if (len == 0) {
      // 0-length means an "uninteresting" buffer, just return nullptr.
      return bufferUPtr;
    }
    // We have a non-empty buffer.
    // allocate an empty BlocksRingBuffer.
    bufferUPtr = MakeUnique<BlocksRingBuffer>();
    // Rewind the reader before the length and deserialize the contents, using
    // the non-UniquePtr Deserializer.
    aER -= ULEB128Size(len);
    aER.ReadIntoObject(*bufferUPtr);
    return bufferUPtr;
  }
};

}  // namespace mozilla

#endif  // BlocksRingBuffer_h
