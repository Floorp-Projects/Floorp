/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef pldhash_h___
#define pldhash_h___

#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h" // for MOZ_ALWAYS_INLINE
#include "mozilla/fallible.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Move.h"
#include "mozilla/Types.h"
#include "nscore.h"

#if defined(__GNUC__) && defined(__i386__)
#define PL_DHASH_FASTCALL __attribute__ ((regparm (3),stdcall))
#elif defined(XP_WIN)
#define PL_DHASH_FASTCALL __fastcall
#else
#define PL_DHASH_FASTCALL
#endif

typedef uint32_t PLDHashNumber;

class PLDHashTable;
struct PLDHashTableOps;

// Table entry header structure.
//
// In order to allow in-line allocation of key and value, we do not declare
// either here. Instead, the API uses const void *key as a formal parameter.
// The key need not be stored in the entry; it may be part of the value, but
// need not be stored at all.
//
// Callback types are defined below and grouped into the PLDHashTableOps
// structure, for single static initialization per hash table sub-type.
//
// Each hash table sub-type should make its entry type a subclass of
// PLDHashEntryHdr. The mKeyHash member contains the result of multiplying the
// hash code returned from the hashKey callback (see below) by kGoldenRatio,
// then constraining the result to avoid the magic 0 and 1 values. The stored
// mKeyHash value is table size invariant, and it is maintained automatically
// -- users need never access it.
struct PLDHashEntryHdr
{
private:
  friend class PLDHashTable;

  PLDHashNumber mKeyHash;
};

#ifdef DEBUG

// This class does three kinds of checking:
//
// - that calls to one of |mOps| or to an enumerator do not cause re-entry into
//   the table in an unsafe way;
//
// - that multiple threads do not access the table in an unsafe way;
//
// - that a table marked as immutable is not modified.
//
// "Safe" here means that multiple concurrent read operations are ok, but a
// write operation (i.e. one that can cause the entry storage to be reallocated
// or destroyed) cannot safely run concurrently with another read or write
// operation. This meaning of "safe" is only partial; for example, it does not
// cover whether a single entry in the table is modified by two separate
// threads. (Doing such checking would be much harder.)
//
// It does this with two variables:
//
// - mState, which embodies a tri-stage tagged union with the following
//   variants:
//   - Idle
//   - Read(n), where 'n' is the number of concurrent read operations
//   - Write
//
// - mIsWritable, which indicates if the table is mutable.
//
class Checker
{
public:
  MOZ_CONSTEXPR Checker() : mState(kIdle), mIsWritable(1) {}

  Checker& operator=(Checker&& aOther) {
    // Atomic<> doesn't have an |operator=(Atomic<>&&)|.
    mState = uint32_t(aOther.mState);
    mIsWritable = uint32_t(aOther.mIsWritable);

    aOther.mState = kIdle;

    return *this;
  }

  static bool IsIdle(uint32_t aState)  { return aState == kIdle; }
  static bool IsRead(uint32_t aState)  { return kRead1 <= aState &&
                                                aState <= kReadMax; }
  static bool IsRead1(uint32_t aState) { return aState == kRead1; }
  static bool IsWrite(uint32_t aState) { return aState == kWrite; }

  bool IsIdle() const { return mState == kIdle; }

  bool IsWritable() const { return !!mIsWritable; }

  void SetNonWritable() { mIsWritable = 0; }

  // NOTE: the obvious way to implement these functions is to (a) check
  // |mState| is reasonable, and then (b) update |mState|. But the lack of
  // atomicity in such an implementation can cause problems if we get unlucky
  // thread interleaving between (a) and (b).
  //
  // So instead for |mState| we are careful to (a) first get |mState|'s old
  // value and assign it a new value in single atomic operation, and only then
  // (b) check the old value was reasonable. This ensures we don't have
  // interleaving problems.
  //
  // For |mIsWritable| we don't need to be as careful because it can only in
  // transition in one direction (from writable to non-writable).

  void StartReadOp()
  {
    uint32_t oldState = mState++;     // this is an atomic increment
    MOZ_ASSERT(IsIdle(oldState) || IsRead(oldState));
    MOZ_ASSERT(oldState < kReadMax);  // check for overflow
  }

  void EndReadOp()
  {
    uint32_t oldState = mState--;     // this is an atomic decrement
    MOZ_ASSERT(IsRead(oldState));
  }

  void StartWriteOp()
  {
    MOZ_ASSERT(IsWritable());
    uint32_t oldState = mState.exchange(kWrite);
    MOZ_ASSERT(IsIdle(oldState));
  }

  void EndWriteOp()
  {
    // Check again that the table is writable, in case it was marked as
    // non-writable just after the IsWritable() assertion in StartWriteOp()
    // occurred.
    MOZ_ASSERT(IsWritable());
    uint32_t oldState = mState.exchange(kIdle);
    MOZ_ASSERT(IsWrite(oldState));
  }

  void StartIteratorRemovalOp()
  {
    // When doing removals at the end of iteration, we go from Read1 state to
    // Write and then back.
    MOZ_ASSERT(IsWritable());
    uint32_t oldState = mState.exchange(kWrite);
    MOZ_ASSERT(IsRead1(oldState));
  }

  void EndIteratorRemovalOp()
  {
    // Check again that the table is writable, in case it was marked as
    // non-writable just after the IsWritable() assertion in
    // StartIteratorRemovalOp() occurred.
    MOZ_ASSERT(IsWritable());
    uint32_t oldState = mState.exchange(kRead1);
    MOZ_ASSERT(IsWrite(oldState));
  }

  void StartDestructorOp()
  {
    // A destructor op is like a write, but the table doesn't need to be
    // writable.
    uint32_t oldState = mState.exchange(kWrite);
    MOZ_ASSERT(IsIdle(oldState));
  }

  void EndDestructorOp()
  {
    uint32_t oldState = mState.exchange(kIdle);
    MOZ_ASSERT(IsWrite(oldState));
  }

private:
  // Things of note about the representation of |mState|.
  // - The values between kRead1..kReadMax represent valid Read(n) values.
  // - kIdle and kRead1 are deliberately chosen so that incrementing the -
  //   former gives the latter.
  // - 9999 concurrent readers should be enough for anybody.
  static const uint32_t kIdle    = 0;
  static const uint32_t kRead1   = 1;
  static const uint32_t kReadMax = 9999;
  static const uint32_t kWrite   = 10000;

  mutable mozilla::Atomic<uint32_t> mState;
  mutable mozilla::Atomic<uint32_t> mIsWritable;
};
#endif

// A PLDHashTable may be allocated on the stack or within another structure or
// class. No entry storage is allocated until the first element is added. This
// means that empty hash tables are cheap, which is good because they are
// common.
//
// There used to be a long, math-heavy comment here about the merits of
// double hashing vs. chaining; it was removed in bug 1058335. In short, double
// hashing is more space-efficient unless the element size gets large (in which
// case you should keep using double hashing but switch to using pointer
// elements). Also, with double hashing, you can't safely hold an entry pointer
// and use it after an add or remove operation, unless you sample Generation()
// before adding or removing, and compare the sample after, dereferencing the
// entry pointer only if Generation() has not changed.
class PLDHashTable
{
private:
  // This class maintains the invariant that every time the entry store is
  // changed, the generation is updated.
  class EntryStore
  {
  private:
    char* mEntryStore;
    uint32_t mGeneration;

  public:
    EntryStore() : mEntryStore(nullptr), mGeneration(0) {}

    ~EntryStore()
    {
      free(mEntryStore);
      mEntryStore = nullptr;
      mGeneration++;    // a little paranoid, but why not be extra safe?
    }

    char* Get() { return mEntryStore; }
    const char* Get() const { return mEntryStore; }

    void Set(char* aEntryStore)
    {
      mEntryStore = aEntryStore;
      mGeneration++;
    }

    uint32_t Generation() const { return mGeneration; }
  };

  const PLDHashTableOps* const mOps;  // Virtual operations; see below.
  int16_t             mHashShift;     // Multiplicative hash shift.
  const uint32_t      mEntrySize;     // Number of bytes in an entry.
  uint32_t            mEntryCount;    // Number of entries in table.
  uint32_t            mRemovedCount;  // Removed entry sentinels in table.
  EntryStore          mEntryStore;    // (Lazy) entry storage and generation.

#ifdef DEBUG
  mutable Checker mChecker;
#endif

public:
  // Table capacity limit; do not exceed. The max capacity used to be 1<<23 but
  // that occasionally that wasn't enough. Making it much bigger than 1<<26
  // probably isn't worthwhile -- tables that big are kind of ridiculous.
  // Also, the growth operation will (deliberately) fail if |capacity *
  // mEntrySize| overflows a uint32_t, and mEntrySize is always at least 8
  // bytes.
  static const uint32_t kMaxCapacity = ((uint32_t)1 << 26);

  static const uint32_t kMinCapacity = 8;

  // Making this half of kMaxCapacity ensures it'll fit. Nobody should need an
  // initial length anywhere nearly this large, anyway.
  static const uint32_t kMaxInitialLength = kMaxCapacity / 2;

  // This gives a default initial capacity of 8.
  static const uint32_t kDefaultInitialLength = 4;

  // Initialize the table with |aOps| and |aEntrySize|. The table's initial
  // capacity is chosen such that |aLength| elements can be inserted without
  // rehashing; if |aLength| is a power-of-two, this capacity will be
  // |2*length|. However, because entry storage is allocated lazily, this
  // initial capacity won't be relevant until the first element is added; prior
  // to that the capacity will be zero.
  //
  // This will crash if |aEntrySize| and/or |aLength| are too large.
  PLDHashTable(const PLDHashTableOps* aOps, uint32_t aEntrySize,
               uint32_t aLength = kDefaultInitialLength);

  PLDHashTable(PLDHashTable&& aOther)
      // These two fields are |const|. Initialize them here because the
      // move assignment operator cannot modify them.
    : mOps(aOther.mOps)
    , mEntrySize(aOther.mEntrySize)
      // Initialize this field because it is required for a safe call to the
      // destructor, which the move assignment operator does.
    , mEntryStore()
#ifdef DEBUG
    , mChecker()
#endif
  {
    *this = mozilla::Move(aOther);
  }

  PLDHashTable& operator=(PLDHashTable&& aOther);

  ~PLDHashTable();

  // This should be used rarely.
  const PLDHashTableOps* const Ops() { return mOps; }

  // Size in entries (gross, not net of free and removed sentinels) for table.
  // This can be zero if no elements have been added yet, in which case the
  // entry storage will not have yet been allocated.
  uint32_t Capacity() const
  {
    return mEntryStore.Get() ? CapacityFromHashShift() : 0;
  }

  uint32_t EntrySize()  const { return mEntrySize; }
  uint32_t EntryCount() const { return mEntryCount; }
  uint32_t Generation() const { return mEntryStore.Generation(); }

  // To search for a |key| in |table|, call:
  //
  //   entry = table.Search(key);
  //
  // If |entry| is non-null, |key| was found. If |entry| is null, key was not
  // found.
  PLDHashEntryHdr* Search(const void* aKey);

  // To add an entry identified by |key| to table, call:
  //
  //   entry = table.Add(key, mozilla::fallible);
  //
  // If |entry| is null upon return, then the table is severely overloaded and
  // memory can't be allocated for entry storage.
  //
  // Otherwise, |aEntry->mKeyHash| has been set so that
  // PLDHashTable::EntryIsFree(entry) is false, and it is up to the caller to
  // initialize the key and value parts of the entry sub-type, if they have not
  // been set already (i.e. if entry was not already in the table, and if the
  // optional initEntry hook was not used).
  PLDHashEntryHdr* Add(const void* aKey, const mozilla::fallible_t&);

  // This is like the other Add() function, but infallible, and so never
  // returns null.
  PLDHashEntryHdr* Add(const void* aKey);

  // To remove an entry identified by |key| from table, call:
  //
  //   table.Remove(key);
  //
  // If |key|'s entry is found, it is cleared (via table->mOps->clearEntry).
  // The table's capacity may be reduced afterwards.
  void Remove(const void* aKey);

  // To remove an entry found by a prior search, call:
  //
  //   table.RemoveEntry(entry);
  //
  // The entry, which must be present and in use, is cleared (via
  // table->mOps->clearEntry). The table's capacity may be reduced afterwards.
  void RemoveEntry(PLDHashEntryHdr* aEntry);

  // Remove an entry already accessed via Search() or Add().
  //
  // NB: this is a "raw" or low-level method. It does not shrink the table if
  // it is underloaded. Don't use it unless necessary and you know what you are
  // doing, and if so, please explain in a comment why it is necessary instead
  // of RemoveEntry().
  void RawRemove(PLDHashEntryHdr* aEntry);

  // This function is equivalent to
  // ClearAndPrepareForLength(kDefaultInitialLength).
  void Clear();

  // This function clears the table's contents and frees its entry storage,
  // leaving it in a empty state ready to be used again. Afterwards, when the
  // first element is added the entry storage that gets allocated will have a
  // capacity large enough to fit |aLength| elements without rehashing.
  //
  // It's conceptually the same as calling the destructor and then re-calling
  // the constructor with the original |aOps| and |aEntrySize| arguments, and
  // a new |aLength| argument.
  void ClearAndPrepareForLength(uint32_t aLength);

  // Measure the size of the table's entry storage. If the entries contain
  // pointers to other heap blocks, you have to iterate over the table and
  // measure those separately; hence the "Shallow" prefix.
  size_t ShallowSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  // Like ShallowSizeOfExcludingThis(), but includes sizeof(*this).
  size_t ShallowSizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

#ifdef DEBUG
  // Mark a table as immutable for the remainder of its lifetime. This
  // changes the implementation from asserting one set of invariants to
  // asserting a different set.
  void MarkImmutable();
#endif

  // If you use PLDHashEntryStub or a subclass of it as your entry struct, and
  // if your entries move via memcpy and clear via memset(0), you can use these
  // stub operations.
  static const PLDHashTableOps* StubOps();

  // The individual stub operations in StubOps().
  static PLDHashNumber HashVoidPtrKeyStub(PLDHashTable* aTable,
                                          const void* aKey);
  static bool MatchEntryStub(PLDHashTable* aTable,
                             const PLDHashEntryHdr* aEntry, const void* aKey);
  static void MoveEntryStub(PLDHashTable* aTable, const PLDHashEntryHdr* aFrom,
                            PLDHashEntryHdr* aTo);
  static void ClearEntryStub(PLDHashTable* aTable, PLDHashEntryHdr* aEntry);

  // Hash/match operations for tables holding C strings.
  static PLDHashNumber HashStringKey(PLDHashTable* aTable, const void* aKey);
  static bool MatchStringKey(PLDHashTable* aTable,
                             const PLDHashEntryHdr* aEntry, const void* aKey);

  // This is an iterator for PLDHashtable. Assertions will detect some, but not
  // all, mid-iteration table modifications that might invalidate (e.g.
  // reallocate) the entry storage.
  //
  // Any element can be removed during iteration using Remove(). If any
  // elements are removed, the table may be resized once iteration ends.
  //
  // Example usage:
  //
  //   for (auto iter = table.Iter(); !iter.Done(); iter.Next()) {
  //     auto entry = static_cast<FooEntry*>(iter.Get());
  //     // ... do stuff with |entry| ...
  //     // ... possibly call iter.Remove() once ...
  //   }
  //
  // or:
  //
  //   for (PLDHashTable::Iterator iter(&table); !iter.Done(); iter.Next()) {
  //     auto entry = static_cast<FooEntry*>(iter.Get());
  //     // ... do stuff with |entry| ...
  //     // ... possibly call iter.Remove() once ...
  //   }
  //
  // The latter form is more verbose but is easier to work with when
  // making subclasses of Iterator.
  //
  class Iterator
  {
  public:
    explicit Iterator(PLDHashTable* aTable);
    Iterator(Iterator&& aOther);
    ~Iterator();

    bool Done() const;                // Have we finished?
    PLDHashEntryHdr* Get() const;     // Get the current entry.
    void Next();                      // Advance to the next entry.

    // Remove the current entry. Must only be called once per entry, and Get()
    // must not be called on that entry afterwards.
    void Remove();

  protected:
    PLDHashTable* mTable;             // Main table pointer.

  private:
    char* mStart;                     // The first entry.
    char* mLimit;                     // One past the last entry.
    char* mCurrent;                   // Pointer to the current entry.
    uint32_t mNexts;                  // Number of Next() calls.
    uint32_t mNextsLimit;             // Next() call limit.

    bool mHaveRemoved;                // Have any elements been removed?

    bool IsOnNonLiveEntry() const;
    void MoveToNextEntry();

    Iterator() = delete;
    Iterator(const Iterator&) = delete;
    Iterator& operator=(const Iterator&) = delete;
    Iterator& operator=(const Iterator&&) = delete;
  };

  Iterator Iter() { return Iterator(this); }

  // Use this if you need to initialize an Iterator in a const method. If you
  // use this case, you should not call Remove() on the iterator.
  Iterator ConstIter() const
  {
    return Iterator(const_cast<PLDHashTable*>(this));
  }

private:
  // Multiplicative hash uses an unsigned 32 bit integer and the golden ratio,
  // expressed as a fixed-point 32-bit fraction.
  static const uint32_t kHashBits = 32;
  static const uint32_t kGoldenRatio = 0x9E3779B9U;

  static uint32_t HashShift(uint32_t aEntrySize, uint32_t aLength);

  static const PLDHashNumber kCollisionFlag = 1;

  static bool EntryIsFree(PLDHashEntryHdr* aEntry);
  static bool EntryIsRemoved(PLDHashEntryHdr* aEntry);
  static bool EntryIsLive(PLDHashEntryHdr* aEntry);
  static void MarkEntryFree(PLDHashEntryHdr* aEntry);
  static void MarkEntryRemoved(PLDHashEntryHdr* aEntry);

  PLDHashNumber Hash1(PLDHashNumber aHash0);
  void Hash2(PLDHashNumber aHash, uint32_t& aHash2Out, uint32_t& aSizeMaskOut);

  static bool MatchEntryKeyhash(PLDHashEntryHdr* aEntry, PLDHashNumber aHash);
  PLDHashEntryHdr* AddressEntry(uint32_t aIndex);

  // We store mHashShift rather than sizeLog2 to optimize the collision-free
  // case in SearchTable.
  uint32_t CapacityFromHashShift() const
  {
    return ((uint32_t)1 << (kHashBits - mHashShift));
  }

  PLDHashNumber ComputeKeyHash(const void* aKey);

  enum SearchReason { ForSearchOrRemove, ForAdd };

  template <SearchReason Reason>
  PLDHashEntryHdr* PL_DHASH_FASTCALL
    SearchTable(const void* aKey, PLDHashNumber aKeyHash);

  PLDHashEntryHdr* PL_DHASH_FASTCALL FindFreeEntry(PLDHashNumber aKeyHash);

  bool ChangeTable(int aDeltaLog2);

  void ShrinkIfAppropriate();

  PLDHashTable(const PLDHashTable& aOther) = delete;
  PLDHashTable& operator=(const PLDHashTable& aOther) = delete;
};

// Compute the hash code for a given key to be looked up, added, or removed
// from aTable. A hash code may have any PLDHashNumber value.
typedef PLDHashNumber (*PLDHashHashKey)(PLDHashTable* aTable,
                                        const void* aKey);

// Compare the key identifying aEntry in aTable with the provided key parameter.
// Return true if keys match, false otherwise.
typedef bool (*PLDHashMatchEntry)(PLDHashTable* aTable,
                                  const PLDHashEntryHdr* aEntry,
                                  const void* aKey);

// Copy the data starting at aFrom to the new entry storage at aTo. Do not add
// reference counts for any strong references in the entry, however, as this
// is a "move" operation: the old entry storage at from will be freed without
// any reference-decrementing callback shortly.
typedef void (*PLDHashMoveEntry)(PLDHashTable* aTable,
                                 const PLDHashEntryHdr* aFrom,
                                 PLDHashEntryHdr* aTo);

// Clear the entry and drop any strong references it holds. This callback is
// invoked by Remove(), but only if the given key is found in the table.
typedef void (*PLDHashClearEntry)(PLDHashTable* aTable,
                                  PLDHashEntryHdr* aEntry);

// Initialize a new entry, apart from mKeyHash. This function is called when
// Add() finds no existing entry for the given key, and must add a new one. At
// that point, |aEntry->mKeyHash| is not set yet, to avoid claiming the last
// free entry in a severely overloaded table.
typedef void (*PLDHashInitEntry)(PLDHashEntryHdr* aEntry, const void* aKey);

// Finally, the "vtable" structure for PLDHashTable. The first four hooks
// must be provided by implementations; they're called unconditionally by the
// generic pldhash.c code. Hooks after these may be null.
//
// Summary of allocation-related hook usage with C++ placement new emphasis:
//  initEntry           Call placement new using default key-based ctor.
//  moveEntry           Call placement new using copy ctor, run dtor on old
//                      entry storage.
//  clearEntry          Run dtor on entry.
//
// Note the reason why initEntry is optional: the default hooks (stubs) clear
// entry storage:  On successful Add(tbl, key), the returned entry pointer
// addresses an entry struct whose mKeyHash member has been set non-zero, but
// all other entry members are still clear (null). Add() callers can test such
// members to see whether the entry was newly created by the Add() call that
// just succeeded. If placement new or similar initialization is required,
// define an |initEntry| hook. Of course, the |clearEntry| hook must zero or
// null appropriately.
//
// XXX assumes 0 is null for pointer types.
struct PLDHashTableOps
{
  // Mandatory hooks. All implementations must provide these.
  PLDHashHashKey      hashKey;
  PLDHashMatchEntry   matchEntry;
  PLDHashMoveEntry    moveEntry;
  PLDHashClearEntry   clearEntry;

  // Optional hooks start here. If null, these are not called.
  PLDHashInitEntry    initEntry;
};

// A minimal entry is a subclass of PLDHashEntryHdr and has a void* key pointer.
struct PLDHashEntryStub : public PLDHashEntryHdr
{
  const void* key;
};

#endif /* pldhash_h___ */
