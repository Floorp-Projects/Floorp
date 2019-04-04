/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <new>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "PLDHashTable.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/OperatorNewExtensions.h"
#include "nsAlgorithm.h"
#include "nsPointerHashKeys.h"
#include "mozilla/Likely.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Maybe.h"
#include "mozilla/ChaosMode.h"

using namespace mozilla;

#ifdef DEBUG

class AutoReadOp {
  Checker& mChk;

 public:
  explicit AutoReadOp(Checker& aChk) : mChk(aChk) { mChk.StartReadOp(); }
  ~AutoReadOp() { mChk.EndReadOp(); }
};

class AutoWriteOp {
  Checker& mChk;

 public:
  explicit AutoWriteOp(Checker& aChk) : mChk(aChk) { mChk.StartWriteOp(); }
  ~AutoWriteOp() { mChk.EndWriteOp(); }
};

class AutoIteratorRemovalOp {
  Checker& mChk;

 public:
  explicit AutoIteratorRemovalOp(Checker& aChk) : mChk(aChk) {
    mChk.StartIteratorRemovalOp();
  }
  ~AutoIteratorRemovalOp() { mChk.EndIteratorRemovalOp(); }
};

class AutoDestructorOp {
  Checker& mChk;

 public:
  explicit AutoDestructorOp(Checker& aChk) : mChk(aChk) {
    mChk.StartDestructorOp();
  }
  ~AutoDestructorOp() { mChk.EndDestructorOp(); }
};

#endif

/* static */
PLDHashNumber PLDHashTable::HashStringKey(const void* aKey) {
  return HashString(static_cast<const char*>(aKey));
}

/* static */
PLDHashNumber PLDHashTable::HashVoidPtrKeyStub(const void* aKey) {
  return nsPtrHashKey<void>::HashKey(aKey);
}

/* static */
bool PLDHashTable::MatchEntryStub(const PLDHashEntryHdr* aEntry,
                                  const void* aKey) {
  const PLDHashEntryStub* stub = (const PLDHashEntryStub*)aEntry;

  return stub->key == aKey;
}

/* static */
bool PLDHashTable::MatchStringKey(const PLDHashEntryHdr* aEntry,
                                  const void* aKey) {
  const PLDHashEntryStub* stub = (const PLDHashEntryStub*)aEntry;

  // XXX tolerate null keys on account of sloppy Mozilla callers.
  return stub->key == aKey ||
         (stub->key && aKey &&
          strcmp((const char*)stub->key, (const char*)aKey) == 0);
}

/* static */
void PLDHashTable::MoveEntryStub(PLDHashTable* aTable,
                                 const PLDHashEntryHdr* aFrom,
                                 PLDHashEntryHdr* aTo) {
  memcpy(aTo, aFrom, aTable->mEntrySize);
}

/* static */
void PLDHashTable::ClearEntryStub(PLDHashTable* aTable,
                                  PLDHashEntryHdr* aEntry) {
  memset(aEntry, 0, aTable->mEntrySize);
}

static const PLDHashTableOps gStubOps = {
    PLDHashTable::HashVoidPtrKeyStub, PLDHashTable::MatchEntryStub,
    PLDHashTable::MoveEntryStub, PLDHashTable::ClearEntryStub, nullptr};

/* static */ const PLDHashTableOps* PLDHashTable::StubOps() {
  return &gStubOps;
}

static bool SizeOfEntryStore(uint32_t aCapacity, uint32_t aEntrySize,
                             uint32_t* aNbytes) {
  uint32_t slotSize = aEntrySize + sizeof(PLDHashNumber);
  uint64_t nbytes64 = uint64_t(aCapacity) * uint64_t(slotSize);
  *aNbytes = aCapacity * slotSize;
  return uint64_t(*aNbytes) == nbytes64;  // returns false on overflow
}

// Compute max and min load numbers (entry counts). We have a secondary max
// that allows us to overload a table reasonably if it cannot be grown further
// (i.e. if ChangeTable() fails). The table slows down drastically if the
// secondary max is too close to 1, but 0.96875 gives only a slight slowdown
// while allowing 1.3x more elements.
static inline uint32_t MaxLoad(uint32_t aCapacity) {
  return aCapacity - (aCapacity >> 2);  // == aCapacity * 0.75
}
static inline uint32_t MaxLoadOnGrowthFailure(uint32_t aCapacity) {
  return aCapacity - (aCapacity >> 5);  // == aCapacity * 0.96875
}
static inline uint32_t MinLoad(uint32_t aCapacity) {
  return aCapacity >> 2;  // == aCapacity * 0.25
}

// Compute the minimum capacity (and the Log2 of that capacity) for a table
// containing |aLength| elements while respecting the following contraints:
// - table must be at most 75% full;
// - capacity must be a power of two;
// - capacity cannot be too small.
static inline void BestCapacity(uint32_t aLength, uint32_t* aCapacityOut,
                                uint32_t* aLog2CapacityOut) {
  // Callers should ensure this is true.
  MOZ_ASSERT(aLength <= PLDHashTable::kMaxInitialLength);

  // Compute the smallest capacity allowing |aLength| elements to be inserted
  // without rehashing.
  uint32_t capacity = (aLength * 4 + (3 - 1)) / 3;  // == ceil(aLength * 4 / 3)
  if (capacity < PLDHashTable::kMinCapacity) {
    capacity = PLDHashTable::kMinCapacity;
  }

  // Round up capacity to next power-of-two.
  uint32_t log2 = CeilingLog2(capacity);
  capacity = 1u << log2;
  MOZ_ASSERT(capacity <= PLDHashTable::kMaxCapacity);

  *aCapacityOut = capacity;
  *aLog2CapacityOut = log2;
}

/* static */ MOZ_ALWAYS_INLINE uint32_t
PLDHashTable::HashShift(uint32_t aEntrySize, uint32_t aLength) {
  if (aLength > kMaxInitialLength) {
    MOZ_CRASH("Initial length is too large");
  }

  uint32_t capacity, log2;
  BestCapacity(aLength, &capacity, &log2);

  uint32_t nbytes;
  if (!SizeOfEntryStore(capacity, aEntrySize, &nbytes)) {
    MOZ_CRASH("Initial entry store size is too large");
  }

  // Compute the hashShift value.
  return kPLDHashNumberBits - log2;
}

PLDHashTable::PLDHashTable(const PLDHashTableOps* aOps, uint32_t aEntrySize,
                           uint32_t aLength)
    : mOps(recordreplay::GeneratePLDHashTableCallbacks(aOps)),
      mEntryStore(),
      mGeneration(0),
      mHashShift(HashShift(aEntrySize, aLength)),
      mEntrySize(aEntrySize),
      mEntryCount(0),
      mRemovedCount(0)
#ifdef DEBUG
      ,
      mChecker()
#endif
{
  // An entry size greater than 0xff is unlikely, but let's check anyway. If
  // you hit this, your hashtable would waste lots of space for unused entries
  // and you should change your hash table's entries to pointers.
  if (aEntrySize != uint32_t(mEntrySize)) {
    MOZ_CRASH("Entry size is too large");
  }
}

PLDHashTable& PLDHashTable::operator=(PLDHashTable&& aOther) {
  if (this == &aOther) {
    return *this;
  }

  // |mOps| and |mEntrySize| are required to stay the same, they're
  // conceptually part of the type -- indeed, if PLDHashTable was a templated
  // type like nsTHashtable, they *would* be part of the type -- so it only
  // makes sense to assign in cases where they match. An exception is when we
  // are recording or replaying the execution, in which case custom ops are
  // generated for each table.
  MOZ_RELEASE_ASSERT(mOps == aOther.mOps || !mOps ||
                     recordreplay::IsRecordingOrReplaying());
  MOZ_RELEASE_ASSERT(mEntrySize == aOther.mEntrySize || !mEntrySize);

  // Reconstruct |this|.
  const PLDHashTableOps* ops =
      recordreplay::UnwrapPLDHashTableCallbacks(aOther.mOps);
  this->~PLDHashTable();
  new (KnownNotNull, this) PLDHashTable(ops, aOther.mEntrySize, 0);

  // Move non-const pieces over.
  mHashShift = std::move(aOther.mHashShift);
  mEntryCount = std::move(aOther.mEntryCount);
  mRemovedCount = std::move(aOther.mRemovedCount);
  mEntryStore.Set(aOther.mEntryStore.Get(), &mGeneration);
#ifdef DEBUG
  mChecker = std::move(aOther.mChecker);
#endif

  recordreplay::MovePLDHashTableContents(aOther.mOps, mOps);

  // Clear up |aOther| so its destruction will be a no-op and it reports being
  // empty.
  {
#ifdef DEBUG
    AutoDestructorOp op(mChecker);
#endif
    aOther.mEntryCount = 0;
    aOther.mEntryStore.Set(nullptr, &aOther.mGeneration);
  }

  return *this;
}

PLDHashNumber PLDHashTable::Hash1(PLDHashNumber aHash0) const {
  return aHash0 >> mHashShift;
}

void PLDHashTable::Hash2(PLDHashNumber aHash0, uint32_t& aHash2Out,
                         uint32_t& aSizeMaskOut) const {
  uint32_t sizeLog2 = kPLDHashNumberBits - mHashShift;
  uint32_t sizeMask = (PLDHashNumber(1) << sizeLog2) - 1;
  aSizeMaskOut = sizeMask;

  // The incoming aHash0 always has the low bit unset (since we leave it
  // free for the collision flag), and should have reasonably random
  // data in the other 31 bits.  We used the high bits of aHash0 for
  // Hash1, so we use the low bits here.  If the table size is large,
  // the bits we use may overlap, but that's still more random than
  // filling with 0s.
  //
  // Double hashing needs the second hash code to be relatively prime to table
  // size, so we simply make hash2 odd.
  //
  // This also conveniently covers up the fact that we have the low bit
  // unset since aHash0 has the low bit unset.
  aHash2Out = (aHash0 & sizeMask) | 1;
}

// Reserve mKeyHash 0 for free entries and 1 for removed-entry sentinels. Note
// that a removed-entry sentinel need be stored only if the removed entry had
// a colliding entry added after it. Therefore we can use 1 as the collision
// flag in addition to the removed-entry sentinel value. Multiplicative hash
// uses the high order bits of mKeyHash, so this least-significant reservation
// should not hurt the hash function's effectiveness much.

// Match an entry's mKeyHash against an unstored one computed from a key.
/* static */
bool PLDHashTable::MatchSlotKeyhash(Slot& aSlot, const PLDHashNumber aKeyHash) {
  return (aSlot.KeyHash() & ~kCollisionFlag) == aKeyHash;
}

// Compute the address of the indexed entry in table.
auto PLDHashTable::SlotForIndex(uint32_t aIndex) const -> Slot {
  return mEntryStore.SlotForIndex(aIndex, mEntrySize, CapacityFromHashShift());
}

PLDHashTable::~PLDHashTable() {
#ifdef DEBUG
  AutoDestructorOp op(mChecker);
#endif

  if (!mEntryStore.Get()) {
    recordreplay::DestroyPLDHashTableCallbacks(mOps);
    return;
  }

  // Clear any remaining live entries.
  mEntryStore.ForEachSlot(Capacity(), mEntrySize, [&](const Slot& aSlot) {
    if (aSlot.IsLive()) {
      mOps->clearEntry(this, aSlot.ToEntry());
    }
  });

  recordreplay::DestroyPLDHashTableCallbacks(mOps);

  // Entry storage is freed last, by ~EntryStore().
}

void PLDHashTable::ClearAndPrepareForLength(uint32_t aLength) {
  // Get these values before the destructor clobbers them.
  const PLDHashTableOps* ops = recordreplay::UnwrapPLDHashTableCallbacks(mOps);
  uint32_t entrySize = mEntrySize;

  this->~PLDHashTable();
  new (KnownNotNull, this) PLDHashTable(ops, entrySize, aLength);
}

void PLDHashTable::Clear() { ClearAndPrepareForLength(kDefaultInitialLength); }

// If |Reason| is |ForAdd|, the return value is always non-null and it may be
// a previously-removed entry. If |Reason| is |ForSearchOrRemove|, the return
// value is null on a miss, and will never be a previously-removed entry on a
// hit. This distinction is a bit grotty but this function is hot enough that
// these differences are worthwhile. (It's also hot enough that
// MOZ_ALWAYS_INLINE makes a significant difference.)
template <PLDHashTable::SearchReason Reason, typename Success, typename Failure>
MOZ_ALWAYS_INLINE auto PLDHashTable::SearchTable(const void* aKey,
                                                 PLDHashNumber aKeyHash,
                                                 Success&& aSuccess,
                                                 Failure&& aFailure) const {
  MOZ_ASSERT(mEntryStore.Get());
  NS_ASSERTION(!(aKeyHash & kCollisionFlag), "!(aKeyHash & kCollisionFlag)");

  // Compute the primary hash address.
  PLDHashNumber hash1 = Hash1(aKeyHash);
  Slot slot = SlotForIndex(hash1);

  // Miss: return space for a new entry.
  if (slot.IsFree()) {
    return (Reason == ForAdd) ? aSuccess(slot) : aFailure();
  }

  // Hit: return entry.
  PLDHashMatchEntry matchEntry = mOps->matchEntry;
  if (MatchSlotKeyhash(slot, aKeyHash)) {
    PLDHashEntryHdr* e = slot.ToEntry();
    if (matchEntry(e, aKey)) {
      return aSuccess(slot);
    }
  }

  // Collision: double hash.
  PLDHashNumber hash2;
  uint32_t sizeMask;
  Hash2(aKeyHash, hash2, sizeMask);

  // Save the first removed entry slot so Add() can recycle it. (Only used
  // if Reason==ForAdd.)
  Maybe<Slot> firstRemoved;

  for (;;) {
    if (Reason == ForAdd && !firstRemoved) {
      if (MOZ_UNLIKELY(slot.IsRemoved())) {
        firstRemoved.emplace(slot);
      } else {
        slot.MarkColliding();
      }
    }

    hash1 -= hash2;
    hash1 &= sizeMask;

    slot = SlotForIndex(hash1);
    if (slot.IsFree()) {
      if (Reason != ForAdd) {
        return aFailure();
      }
      return aSuccess(firstRemoved.refOr(slot));
    }

    if (MatchSlotKeyhash(slot, aKeyHash)) {
      PLDHashEntryHdr* e = slot.ToEntry();
      if (matchEntry(e, aKey)) {
        return aSuccess(slot);
      }
    }
  }

  // NOTREACHED
  return aFailure();
}

// This is a copy of SearchTable(), used by ChangeTable(), hardcoded to
//   1. assume |Reason| is |ForAdd|,
//   2. assume that |aKey| will never match an existing entry, and
//   3. assume that no entries have been removed from the current table
//      structure.
// Avoiding the need for |aKey| means we can avoid needing a way to map entries
// to keys, which means callers can use complex key types more easily.
MOZ_ALWAYS_INLINE auto PLDHashTable::FindFreeSlot(PLDHashNumber aKeyHash) const
    -> Slot {
  MOZ_ASSERT(mEntryStore.Get());
  NS_ASSERTION(!(aKeyHash & kCollisionFlag), "!(aKeyHash & kCollisionFlag)");

  // Compute the primary hash address.
  PLDHashNumber hash1 = Hash1(aKeyHash);
  Slot slot = SlotForIndex(hash1);

  // Miss: return space for a new entry.
  if (slot.IsFree()) {
    return slot;
  }

  // Collision: double hash.
  PLDHashNumber hash2;
  uint32_t sizeMask;
  Hash2(aKeyHash, hash2, sizeMask);

  for (;;) {
    MOZ_ASSERT(!slot.IsRemoved());
    slot.MarkColliding();

    hash1 -= hash2;
    hash1 &= sizeMask;

    slot = SlotForIndex(hash1);
    if (slot.IsFree()) {
      return slot;
    }
  }

  // NOTREACHED
}

bool PLDHashTable::ChangeTable(int32_t aDeltaLog2) {
  MOZ_ASSERT(mEntryStore.Get());

  // Look, but don't touch, until we succeed in getting new entry store.
  int32_t oldLog2 = kPLDHashNumberBits - mHashShift;
  int32_t newLog2 = oldLog2 + aDeltaLog2;
  uint32_t newCapacity = 1u << newLog2;
  if (newCapacity > kMaxCapacity) {
    return false;
  }

  uint32_t nbytes;
  if (!SizeOfEntryStore(newCapacity, mEntrySize, &nbytes)) {
    return false;  // overflowed
  }

  char* newEntryStore = (char*)calloc(1, nbytes);
  if (!newEntryStore) {
    return false;
  }

  // We can't fail from here on, so update table parameters.
  mHashShift = kPLDHashNumberBits - newLog2;
  mRemovedCount = 0;

  // Assign the new entry store to table.
  char* oldEntryStore = mEntryStore.Get();
  mEntryStore.Set(newEntryStore, &mGeneration);
  PLDHashMoveEntry moveEntry = mOps->moveEntry;

  // Copy only live entries, leaving removed ones behind.
  uint32_t oldCapacity = 1u << oldLog2;
  EntryStore::ForEachSlot(
      oldEntryStore, oldCapacity, mEntrySize, [&](const Slot& slot) {
        if (slot.IsLive()) {
          const PLDHashNumber key = slot.KeyHash() & ~kCollisionFlag;
          Slot newSlot = FindFreeSlot(key);
          MOZ_ASSERT(newSlot.IsFree());
          moveEntry(this, slot.ToEntry(), newSlot.ToEntry());
          newSlot.SetKeyHash(key);
        }
      });

  free(oldEntryStore);
  return true;
}

MOZ_ALWAYS_INLINE PLDHashNumber
PLDHashTable::ComputeKeyHash(const void* aKey) const {
  MOZ_ASSERT(mEntryStore.Get());

  PLDHashNumber keyHash = mozilla::ScrambleHashCode(mOps->hashKey(aKey));

  // Avoid 0 and 1 hash codes, they indicate free and removed entries.
  if (keyHash < 2) {
    keyHash -= 2;
  }
  keyHash &= ~kCollisionFlag;

  return keyHash;
}

PLDHashEntryHdr* PLDHashTable::Search(const void* aKey) const {
#ifdef DEBUG
  AutoReadOp op(mChecker);
#endif

  if (!mEntryStore.Get()) {
    return nullptr;
  }

  return SearchTable<ForSearchOrRemove>(
      aKey, ComputeKeyHash(aKey),
      [&](Slot& slot) -> PLDHashEntryHdr* { return slot.ToEntry(); },
      [&]() -> PLDHashEntryHdr* { return nullptr; });
}

PLDHashEntryHdr* PLDHashTable::Add(const void* aKey,
                                   const mozilla::fallible_t&) {
#ifdef DEBUG
  AutoWriteOp op(mChecker);
#endif

  // Allocate the entry storage if it hasn't already been allocated.
  if (!mEntryStore.Get()) {
    uint32_t nbytes;
    // We already checked this in the constructor, so it must still be true.
    MOZ_RELEASE_ASSERT(
        SizeOfEntryStore(CapacityFromHashShift(), mEntrySize, &nbytes));
    mEntryStore.Set((char*)calloc(1, nbytes), &mGeneration);
    if (!mEntryStore.Get()) {
      return nullptr;
    }
  }

  // If alpha is >= .75, grow or compress the table. If aKey is already in the
  // table, we may grow once more than necessary, but only if we are on the
  // edge of being overloaded.
  uint32_t capacity = Capacity();
  if (mEntryCount + mRemovedCount >= MaxLoad(capacity)) {
    // Compress if a quarter or more of all entries are removed.
    int deltaLog2;
    if (mRemovedCount >= capacity >> 2) {
      deltaLog2 = 0;
    } else {
      deltaLog2 = 1;
    }

    // Grow or compress the table. If ChangeTable() fails, allow overloading up
    // to the secondary max. Once we hit the secondary max, return null.
    if (!ChangeTable(deltaLog2) &&
        mEntryCount + mRemovedCount >= MaxLoadOnGrowthFailure(capacity)) {
      return nullptr;
    }
  }

  // Look for entry after possibly growing, so we don't have to add it,
  // then skip it while growing the table and re-add it after.
  PLDHashNumber keyHash = ComputeKeyHash(aKey);
  Slot slot = SearchTable<ForAdd>(
      aKey, keyHash, [&](Slot& found) -> Slot { return found; },
      [&]() -> Slot {
        MOZ_CRASH("Nope");
        return Slot(nullptr, nullptr);
      });
  if (!slot.IsLive()) {
    // Initialize the slot, indicating that it's no longer free.
    if (slot.IsRemoved()) {
      mRemovedCount--;
      keyHash |= kCollisionFlag;
    }
    if (mOps->initEntry) {
      mOps->initEntry(slot.ToEntry(), aKey);
    }
    slot.SetKeyHash(keyHash);
    mEntryCount++;
  }

  return slot.ToEntry();
}

PLDHashEntryHdr* PLDHashTable::Add(const void* aKey) {
  PLDHashEntryHdr* entry = Add(aKey, fallible);
  if (!entry) {
    if (!mEntryStore.Get()) {
      // We OOM'd while allocating the initial entry storage.
      uint32_t nbytes;
      (void)SizeOfEntryStore(CapacityFromHashShift(), mEntrySize, &nbytes);
      NS_ABORT_OOM(nbytes);
    } else {
      // We failed to resize the existing entry storage, either due to OOM or
      // because we exceeded the maximum table capacity or size; report it as
      // an OOM. The multiplication by 2 gets us the size we tried to allocate,
      // which is double the current size.
      NS_ABORT_OOM(2 * EntrySize() * EntryCount());
    }
  }
  return entry;
}

void PLDHashTable::Remove(const void* aKey) {
#ifdef DEBUG
  AutoWriteOp op(mChecker);
#endif

  if (!mEntryStore.Get()) {
    return;
  }

  PLDHashNumber keyHash = ComputeKeyHash(aKey);
  SearchTable<ForSearchOrRemove>(
      aKey, keyHash,
      [&](Slot& slot) {
        RawRemove(slot);
        ShrinkIfAppropriate();
      },
      [&]() {
        // Do nothing.
      });
}

void PLDHashTable::RemoveEntry(PLDHashEntryHdr* aEntry) {
#ifdef DEBUG
  AutoWriteOp op(mChecker);
#endif

  RawRemove(aEntry);
  ShrinkIfAppropriate();
}

void PLDHashTable::RawRemove(PLDHashEntryHdr* aEntry) {
  Slot slot(mEntryStore.SlotForPLDHashEntry(aEntry, Capacity(), mEntrySize));
  RawRemove(slot);
}

void PLDHashTable::RawRemove(Slot& aSlot) {
  // Unfortunately, we can only do weak checking here. That's because
  // RawRemove() can be called legitimately while an Enumerate() call is
  // active, which doesn't fit well into how Checker's mState variable works.
  MOZ_ASSERT(mChecker.IsWritable());

  MOZ_ASSERT(mEntryStore.Get());

  MOZ_ASSERT(aSlot.IsLive());

  // Load keyHash first in case clearEntry() goofs it.
  PLDHashEntryHdr* entry = aSlot.ToEntry();
  PLDHashNumber keyHash = aSlot.KeyHash();
  mOps->clearEntry(this, entry);
  if (keyHash & kCollisionFlag) {
    aSlot.MarkRemoved();
    mRemovedCount++;
  } else {
    aSlot.MarkFree();
  }
  mEntryCount--;
}

// Shrink or compress if a quarter or more of all entries are removed, or if the
// table is underloaded according to the minimum alpha, and is not minimal-size
// already.
void PLDHashTable::ShrinkIfAppropriate() {
  uint32_t capacity = Capacity();
  if (mRemovedCount >= capacity >> 2 ||
      (capacity > kMinCapacity && mEntryCount <= MinLoad(capacity))) {
    uint32_t log2;
    BestCapacity(mEntryCount, &capacity, &log2);

    int32_t deltaLog2 = log2 - (kPLDHashNumberBits - mHashShift);
    MOZ_ASSERT(deltaLog2 <= 0);

    (void)ChangeTable(deltaLog2);
  }
}

size_t PLDHashTable::ShallowSizeOfExcludingThis(
    MallocSizeOf aMallocSizeOf) const {
#ifdef DEBUG
  AutoReadOp op(mChecker);
#endif

  return aMallocSizeOf(mEntryStore.Get());
}

size_t PLDHashTable::ShallowSizeOfIncludingThis(
    MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this) + ShallowSizeOfExcludingThis(aMallocSizeOf);
}

PLDHashTable::Iterator::Iterator(Iterator&& aOther)
    : mTable(aOther.mTable),
      mCurrent(aOther.mCurrent),
      mNexts(aOther.mNexts),
      mNextsLimit(aOther.mNextsLimit),
      mHaveRemoved(aOther.mHaveRemoved),
      mEntrySize(aOther.mEntrySize) {
  // No need to change |mChecker| here.
  aOther.mTable = nullptr;
  // We don't really have the concept of a null slot, so leave mCurrent.
  aOther.mNexts = 0;
  aOther.mNextsLimit = 0;
  aOther.mHaveRemoved = false;
  aOther.mEntrySize = 0;
}

PLDHashTable::Iterator::Iterator(PLDHashTable* aTable)
    : mTable(aTable),
      mCurrent(mTable->mEntryStore.SlotForIndex(0, mTable->mEntrySize,
                                                mTable->Capacity())),
      mNexts(0),
      mNextsLimit(mTable->EntryCount()),
      mHaveRemoved(false),
      mEntrySize(aTable->mEntrySize) {
#ifdef DEBUG
  mTable->mChecker.StartReadOp();
#endif

  if (ChaosMode::isActive(ChaosFeature::HashTableIteration) &&
      mTable->Capacity() > 0) {
    // Start iterating at a random entry. It would be even more chaotic to
    // iterate in fully random order, but that's harder.
    uint32_t capacity = mTable->CapacityFromHashShift();
    uint32_t i = ChaosMode::randomUint32LessThan(capacity);
    mCurrent =
        mTable->mEntryStore.SlotForIndex(i, mTable->mEntrySize, capacity);
  }

  // Advance to the first live entry, if there is one.
  if (!Done() && IsOnNonLiveEntry()) {
    MoveToNextLiveEntry();
  }
}

PLDHashTable::Iterator::~Iterator() {
  if (mTable) {
    if (mHaveRemoved) {
      mTable->ShrinkIfAppropriate();
    }
#ifdef DEBUG
    mTable->mChecker.EndReadOp();
#endif
  }
}

MOZ_ALWAYS_INLINE bool PLDHashTable::Iterator::IsOnNonLiveEntry() const {
  MOZ_ASSERT(!Done());
  return !mCurrent.IsLive();
}

void PLDHashTable::Iterator::Next() {
  MOZ_ASSERT(!Done());

  mNexts++;

  // Advance to the next live entry, if there is one.
  if (!Done()) {
    MoveToNextLiveEntry();
  }
}

MOZ_ALWAYS_INLINE void PLDHashTable::Iterator::MoveToNextLiveEntry() {
  // Chaos mode requires wraparound to cover all possible entries, so we can't
  // simply move to the next live entry and stop when we hit the end of the
  // entry store. But we don't want to introduce extra branches into our inner
  // loop. So we are going to exploit the structure of the entry store in this
  // method to implement an efficient inner loop.
  //
  // The idea is that since we are really only iterating through the stored
  // hashes and because we know that there are a power-of-two number of
  // hashes, we can use masking to implement the wraparound for us. This
  // method does have the downside of needing to recalculate where the
  // associated entry is once we've found it, but that seems OK.

  // Our current slot and its associated hash.
  Slot slot = mCurrent;
  PLDHashNumber* p = slot.HashPtr();
  const uint32_t capacity = mTable->CapacityFromHashShift();
  const uint32_t mask = capacity - 1;
  auto hashes = reinterpret_cast<PLDHashNumber*>(mTable->mEntryStore.Get());
  uint32_t slotIndex = p - hashes;

  do {
    slotIndex = (slotIndex + 1) & mask;
  } while (!Slot::IsLiveHash(hashes[slotIndex]));

  // slotIndex now indicates where a live slot is. Rematerialize the entry
  // and the slot.
  auto entries = reinterpret_cast<char*>(&hashes[capacity]);
  char* entryPtr = entries + slotIndex * mEntrySize;
  auto entry = reinterpret_cast<PLDHashEntryHdr*>(entryPtr);

  mCurrent = Slot(entry, &hashes[slotIndex]);
}

void PLDHashTable::Iterator::Remove() {
  mTable->RawRemove(mCurrent);
  mHaveRemoved = true;
}

#ifdef DEBUG
void PLDHashTable::MarkImmutable() { mChecker.SetNonWritable(); }
#endif
