/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Double hashing implementation.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pldhash.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/MathAlgorithms.h"
#include "nsDebug.h"     /* for PR_ASSERT */
#include "nsAlgorithm.h"
#include "mozilla/Likely.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/ChaosMode.h"

#ifdef PL_DHASHMETER
# define METER(x)       x
#else
# define METER(x)       /* nothing */
#endif

/*
 * The following DEBUG-only code is used to assert that calls to one of
 * table->mOps or to an enumerator do not cause re-entry into a call that
 * can mutate the table.
 */
#ifdef DEBUG

/*
 * Most callers that assert about the recursion level don't care about
 * this magical value because they are asserting that mutation is
 * allowed (and therefore the level is 0 or 1, depending on whether they
 * incremented it).
 *
 * Only PL_DHashTableFinish needs to allow this special value.
 */
#define IMMUTABLE_RECURSION_LEVEL UINT32_MAX

#define RECURSION_LEVEL_SAFE_TO_FINISH(table_)                                \
    (table_->mRecursionLevel == 0 ||                                          \
     table_->mRecursionLevel == IMMUTABLE_RECURSION_LEVEL)

#define INCREMENT_RECURSION_LEVEL(table_)                                     \
    do {                                                                      \
        if (table_->mRecursionLevel != IMMUTABLE_RECURSION_LEVEL) {           \
            const uint32_t oldRecursionLevel = table_->mRecursionLevel++;     \
            MOZ_ASSERT(oldRecursionLevel < IMMUTABLE_RECURSION_LEVEL - 1);    \
        }                                                                     \
    } while(0)
#define DECREMENT_RECURSION_LEVEL(table_)                                     \
    do {                                                                      \
        if (table_->mRecursionLevel != IMMUTABLE_RECURSION_LEVEL) {           \
            const uint32_t oldRecursionLevel = table_->mRecursionLevel--;     \
            MOZ_ASSERT(oldRecursionLevel > 0);                                \
        }                                                                     \
    } while(0)

#else

#define INCREMENT_RECURSION_LEVEL(table_)   do { } while(0)
#define DECREMENT_RECURSION_LEVEL(table_)   do { } while(0)

#endif /* defined(DEBUG) */

using namespace mozilla;

PLDHashNumber
PL_DHashStringKey(PLDHashTable* aTable, const void* aKey)
{
  return HashString(static_cast<const char*>(aKey));
}

PLDHashNumber
PL_DHashVoidPtrKeyStub(PLDHashTable* aTable, const void* aKey)
{
  return (PLDHashNumber)(ptrdiff_t)aKey >> 2;
}

bool
PL_DHashMatchEntryStub(PLDHashTable* aTable,
                       const PLDHashEntryHdr* aEntry,
                       const void* aKey)
{
  const PLDHashEntryStub* stub = (const PLDHashEntryStub*)aEntry;

  return stub->key == aKey;
}

bool
PL_DHashMatchStringKey(PLDHashTable* aTable,
                       const PLDHashEntryHdr* aEntry,
                       const void* aKey)
{
  const PLDHashEntryStub* stub = (const PLDHashEntryStub*)aEntry;

  /* XXX tolerate null keys on account of sloppy Mozilla callers. */
  return stub->key == aKey ||
         (stub->key && aKey &&
          strcmp((const char*)stub->key, (const char*)aKey) == 0);
}

MOZ_ALWAYS_INLINE void
PLDHashTable::MoveEntryStub(const PLDHashEntryHdr* aFrom,
                            PLDHashEntryHdr* aTo)
{
  memcpy(aTo, aFrom, mEntrySize);
}

void
PL_DHashMoveEntryStub(PLDHashTable* aTable,
                      const PLDHashEntryHdr* aFrom,
                      PLDHashEntryHdr* aTo)
{
  aTable->MoveEntryStub(aFrom, aTo);
}

MOZ_ALWAYS_INLINE void
PLDHashTable::ClearEntryStub(PLDHashEntryHdr* aEntry)
{
  memset(aEntry, 0, mEntrySize);
}

void
PL_DHashClearEntryStub(PLDHashTable* aTable, PLDHashEntryHdr* aEntry)
{
  aTable->ClearEntryStub(aEntry);
}

MOZ_ALWAYS_INLINE void
PLDHashTable::FreeStringKey(PLDHashEntryHdr* aEntry)
{
  const PLDHashEntryStub* stub = (const PLDHashEntryStub*)aEntry;

  free((void*)stub->key);
  memset(aEntry, 0, mEntrySize);
}

void
PL_DHashFreeStringKey(PLDHashTable* aTable, PLDHashEntryHdr* aEntry)
{
  aTable->FreeStringKey(aEntry);
}

static const PLDHashTableOps stub_ops = {
  PL_DHashVoidPtrKeyStub,
  PL_DHashMatchEntryStub,
  PL_DHashMoveEntryStub,
  PL_DHashClearEntryStub,
  nullptr
};

const PLDHashTableOps*
PL_DHashGetStubOps(void)
{
  return &stub_ops;
}

static bool
SizeOfEntryStore(uint32_t aCapacity, uint32_t aEntrySize, uint32_t* aNbytes)
{
  uint64_t nbytes64 = uint64_t(aCapacity) * uint64_t(aEntrySize);
  *aNbytes = aCapacity * aEntrySize;
  return uint64_t(*aNbytes) == nbytes64;   // returns false on overflow
}

PLDHashTable*
PL_NewDHashTable(const PLDHashTableOps* aOps, uint32_t aEntrySize,
                 uint32_t aLength)
{
  PLDHashTable* table = new PLDHashTable();
  PL_DHashTableInit(table, aOps, aEntrySize, aLength);
  return table;
}

void
PL_DHashTableDestroy(PLDHashTable* aTable)
{
  PL_DHashTableFinish(aTable);
  delete aTable;
}

/*
 * Compute max and min load numbers (entry counts).  We have a secondary max
 * that allows us to overload a table reasonably if it cannot be grown further
 * (i.e. if ChangeTable() fails).  The table slows down drastically if the
 * secondary max is too close to 1, but 0.96875 gives only a slight slowdown
 * while allowing 1.3x more elements.
 */
static inline uint32_t
MaxLoad(uint32_t aCapacity)
{
  return aCapacity - (aCapacity >> 2);  // == aCapacity * 0.75
}
static inline uint32_t
MaxLoadOnGrowthFailure(uint32_t aCapacity)
{
  return aCapacity - (aCapacity >> 5);  // == aCapacity * 0.96875
}
static inline uint32_t
MinLoad(uint32_t aCapacity)
{
  return aCapacity >> 2;                // == aCapacity * 0.25
}

static inline uint32_t
MinCapacity(uint32_t aLength)
{
  return (aLength * 4 + (3 - 1)) / 3;   // == ceil(aLength * 4 / 3)
}

MOZ_ALWAYS_INLINE void
PLDHashTable::Init(const PLDHashTableOps* aOps,
                   uint32_t aEntrySize, uint32_t aLength)
{
  MOZ_ASSERT(!IsInitialized());

  // Check that the important fields have been set by the constructor.
  MOZ_ASSERT(mOps == nullptr);
  MOZ_ASSERT(mRecursionLevel == 0);
  MOZ_ASSERT(mEntryStore == nullptr);

  if (aLength > PL_DHASH_MAX_INITIAL_LENGTH) {
    MOZ_CRASH("Initial length is too large");
  }

  // Compute the smallest capacity allowing |aLength| elements to be inserted
  // without rehashing.
  uint32_t capacity = MinCapacity(aLength);
  if (capacity < PL_DHASH_MIN_CAPACITY) {
    capacity = PL_DHASH_MIN_CAPACITY;
  }

  int log2 = CeilingLog2(capacity);

  capacity = 1u << log2;
  MOZ_ASSERT(capacity <= PL_DHASH_MAX_CAPACITY);
  mOps = aOps;
  mHashShift = PL_DHASH_BITS - log2;
  mEntrySize = aEntrySize;
  mEntryCount = mRemovedCount = 0;
  mGeneration = 0;
  uint32_t nbytes;
  if (!SizeOfEntryStore(capacity, aEntrySize, &nbytes)) {
    MOZ_CRASH("Initial entry store size is too large");
  }

  mEntryStore = nullptr;

  METER(memset(&mStats, 0, sizeof(mStats)));

#ifdef DEBUG
  mRecursionLevel = 0;
#endif
}

void
PL_DHashTableInit(PLDHashTable* aTable, const PLDHashTableOps* aOps,
                  uint32_t aEntrySize, uint32_t aLength)
{
  aTable->Init(aOps, aEntrySize, aLength);
}

void
PL_DHashTableInit(PLDHashTable2* aTable, const PLDHashTableOps* aOps,
                  uint32_t aEntrySize, uint32_t aLength)
{
  aTable->Init(aOps, aEntrySize, aLength);
}

PLDHashTable2::PLDHashTable2(const PLDHashTableOps* aOps, uint32_t aEntrySize,
                             uint32_t aLength)
  : PLDHashTable()
{
  PLDHashTable::Init(aOps, aEntrySize, aLength);
}

PLDHashTable& PLDHashTable::operator=(PLDHashTable&& aOther)
{
  if (this == &aOther) {
    return *this;
  }

  // Destruct |this|.
  Finish();

  // Move pieces over.
  mOps = Move(aOther.mOps);
  mHashShift = Move(aOther.mHashShift);
  mEntrySize = Move(aOther.mEntrySize);
  mEntryCount = Move(aOther.mEntryCount);
  mRemovedCount = Move(aOther.mRemovedCount);
  mGeneration = Move(aOther.mGeneration);
  mEntryStore = Move(aOther.mEntryStore);
#ifdef PL_DHASHMETER
  mStats = Move(aOther.mStats);
#endif
#ifdef DEBUG
  // Atomic<> doesn't have an |operator=(Atomic<>&&)|.
  mRecursionLevel = uint32_t(aOther.mRecursionLevel);
#endif

  // Clear up |aOther| so its destruction will be a no-op.
  aOther.mOps = nullptr;
  aOther.mEntryStore = nullptr;
#ifdef DEBUG
  aOther.mRecursionLevel = 0;
#endif

  return *this;
}

/*
 * Double hashing needs the second hash code to be relatively prime to table
 * size, so we simply make hash2 odd.
 */
#define HASH1(hash0, shift)         ((hash0) >> (shift))
#define HASH2(hash0,log2,shift)     ((((hash0) << (log2)) >> (shift)) | 1)

/*
 * Reserve mKeyHash 0 for free entries and 1 for removed-entry sentinels.  Note
 * that a removed-entry sentinel need be stored only if the removed entry had
 * a colliding entry added after it.  Therefore we can use 1 as the collision
 * flag in addition to the removed-entry sentinel value.  Multiplicative hash
 * uses the high order bits of mKeyHash, so this least-significant reservation
 * should not hurt the hash function's effectiveness much.
 */
#define COLLISION_FLAG              ((PLDHashNumber) 1)
#define MARK_ENTRY_FREE(entry)      ((entry)->mKeyHash = 0)
#define MARK_ENTRY_REMOVED(entry)   ((entry)->mKeyHash = 1)
#define ENTRY_IS_REMOVED(entry)     ((entry)->mKeyHash == 1)
#define ENTRY_IS_LIVE(entry)        ((entry)->mKeyHash >= 2)
#define ENSURE_LIVE_KEYHASH(hash0)  if (hash0 < 2) hash0 -= 2; else (void)0

/* Match an entry's mKeyHash against an unstored one computed from a key. */
#define MATCH_ENTRY_KEYHASH(entry,hash0) \
    (((entry)->mKeyHash & ~COLLISION_FLAG) == (hash0))

/* Compute the address of the indexed entry in table. */
#define ADDRESS_ENTRY(table, index) \
    ((PLDHashEntryHdr *)((table)->mEntryStore + (index) * (table)->mEntrySize))

/* static */ MOZ_ALWAYS_INLINE bool
PLDHashTable::EntryIsFree(PLDHashEntryHdr* aEntry)
{
  return aEntry->mKeyHash == 0;
}

MOZ_ALWAYS_INLINE void
PLDHashTable::Finish()
{
  if (!IsInitialized()) {
    MOZ_ASSERT(!mEntryStore);
    return;
  }

  INCREMENT_RECURSION_LEVEL(this);

  /* Clear any remaining live entries. */
  char* entryAddr = mEntryStore;
  char* entryLimit = entryAddr + Capacity() * mEntrySize;
  while (entryAddr < entryLimit) {
    PLDHashEntryHdr* entry = (PLDHashEntryHdr*)entryAddr;
    if (ENTRY_IS_LIVE(entry)) {
      METER(mStats.mRemoveEnums++);
      mOps->clearEntry(this, entry);
    }
    entryAddr += mEntrySize;
  }

  mOps = nullptr;

  DECREMENT_RECURSION_LEVEL(this);
  MOZ_ASSERT(RECURSION_LEVEL_SAFE_TO_FINISH(this));

  /* Free entry storage last. */
  free(mEntryStore);
  mEntryStore = nullptr;
}

void
PL_DHashTableFinish(PLDHashTable* aTable)
{
  aTable->Finish();
}

void
PL_DHashTableFinish(PLDHashTable2* aTable)
{
  aTable->Finish();
}

PLDHashTable2::~PLDHashTable2()
{
  PLDHashTable::Finish();
}

void
PLDHashTable2::ClearAndPrepareForLength(uint32_t aLength)
{
  MOZ_ASSERT(IsInitialized());

  // Get these values before the call to Finish() clobbers them.
  const PLDHashTableOps* ops = mOps;
  uint32_t entrySize = mEntrySize;

  PLDHashTable::Finish();
  PLDHashTable::Init(ops, entrySize, aLength);
}

void
PLDHashTable2::Clear()
{
  ClearAndPrepareForLength(PL_DHASH_DEFAULT_INITIAL_LENGTH);
}

// If |IsAdd| is true, the return value is always non-null and it may be a
// previously-removed entry. If |IsAdd| is false, the return value is null on a
// miss, and will never be a previously-removed entry on a hit. This
// distinction is a bit grotty but this function is hot enough that these
// differences are worthwhile.
template <PLDHashTable::SearchReason Reason>
PLDHashEntryHdr* PL_DHASH_FASTCALL
PLDHashTable::SearchTable(const void* aKey, PLDHashNumber aKeyHash)
{
  MOZ_ASSERT(mEntryStore);
  METER(mStats.mSearches++);
  NS_ASSERTION(!(aKeyHash & COLLISION_FLAG),
               "!(aKeyHash & COLLISION_FLAG)");

  /* Compute the primary hash address. */
  PLDHashNumber hash1 = HASH1(aKeyHash, mHashShift);
  PLDHashEntryHdr* entry = ADDRESS_ENTRY(this, hash1);

  /* Miss: return space for a new entry. */
  if (EntryIsFree(entry)) {
    METER(mStats.mMisses++);
    return (Reason == ForAdd) ? entry : nullptr;
  }

  /* Hit: return entry. */
  PLDHashMatchEntry matchEntry = mOps->matchEntry;
  if (MATCH_ENTRY_KEYHASH(entry, aKeyHash) &&
      matchEntry(this, entry, aKey)) {
    METER(mStats.mHits++);
    return entry;
  }

  /* Collision: double hash. */
  int sizeLog2 = PL_DHASH_BITS - mHashShift;
  PLDHashNumber hash2 = HASH2(aKeyHash, sizeLog2, mHashShift);
  uint32_t sizeMask = (1u << sizeLog2) - 1;

  /*
   * Save the first removed entry pointer so Add() can recycle it. (Only used
   * if Reason==ForAdd.)
   */
  PLDHashEntryHdr* firstRemoved = nullptr;

  for (;;) {
    if (Reason == ForAdd) {
      if (MOZ_UNLIKELY(ENTRY_IS_REMOVED(entry))) {
        if (!firstRemoved) {
          firstRemoved = entry;
        }
      } else {
        entry->mKeyHash |= COLLISION_FLAG;
      }
    }

    METER(mStats.mSteps++);
    hash1 -= hash2;
    hash1 &= sizeMask;

    entry = ADDRESS_ENTRY(this, hash1);
    if (EntryIsFree(entry)) {
      METER(mStats.mMisses++);
      return (Reason == ForAdd) ? (firstRemoved ? firstRemoved : entry)
                                : nullptr;
    }

    if (MATCH_ENTRY_KEYHASH(entry, aKeyHash) &&
        matchEntry(this, entry, aKey)) {
      METER(mStats.mHits++);
      return entry;
    }
  }

  /* NOTREACHED */
  return nullptr;
}

/*
 * This is a copy of SearchTable, used by ChangeTable, hardcoded to
 *   1. assume |aIsAdd| is true,
 *   2. assume that |aKey| will never match an existing entry, and
 *   3. assume that no entries have been removed from the current table
 *      structure.
 * Avoiding the need for |aKey| means we can avoid needing a way to map
 * entries to keys, which means callers can use complex key types more
 * easily.
 */
PLDHashEntryHdr* PL_DHASH_FASTCALL
PLDHashTable::FindFreeEntry(PLDHashNumber aKeyHash)
{
  METER(mStats.mSearches++);
  MOZ_ASSERT(mEntryStore);
  NS_ASSERTION(!(aKeyHash & COLLISION_FLAG),
               "!(aKeyHash & COLLISION_FLAG)");

  /* Compute the primary hash address. */
  PLDHashNumber hash1 = HASH1(aKeyHash, mHashShift);
  PLDHashEntryHdr* entry = ADDRESS_ENTRY(this, hash1);

  /* Miss: return space for a new entry. */
  if (EntryIsFree(entry)) {
    METER(mStats.mMisses++);
    return entry;
  }

  /* Collision: double hash. */
  int sizeLog2 = PL_DHASH_BITS - mHashShift;
  PLDHashNumber hash2 = HASH2(aKeyHash, sizeLog2, mHashShift);
  uint32_t sizeMask = (1u << sizeLog2) - 1;

  for (;;) {
    NS_ASSERTION(!ENTRY_IS_REMOVED(entry),
                 "!ENTRY_IS_REMOVED(entry)");
    entry->mKeyHash |= COLLISION_FLAG;

    METER(mStats.mSteps++);
    hash1 -= hash2;
    hash1 &= sizeMask;

    entry = ADDRESS_ENTRY(this, hash1);
    if (EntryIsFree(entry)) {
      METER(mStats.mMisses++);
      return entry;
    }
  }

  /* NOTREACHED */
  return nullptr;
}

bool
PLDHashTable::ChangeTable(int aDeltaLog2)
{
  MOZ_ASSERT(mEntryStore);

  /* Look, but don't touch, until we succeed in getting new entry store. */
  int oldLog2 = PL_DHASH_BITS - mHashShift;
  int newLog2 = oldLog2 + aDeltaLog2;
  uint32_t newCapacity = 1u << newLog2;
  if (newCapacity > PL_DHASH_MAX_CAPACITY) {
    return false;
  }

  uint32_t nbytes;
  if (!SizeOfEntryStore(newCapacity, mEntrySize, &nbytes)) {
    return false;   // overflowed
  }

  char* newEntryStore = (char*)malloc(nbytes);
  if (!newEntryStore) {
    return false;
  }

  /* We can't fail from here on, so update table parameters. */
  mHashShift = PL_DHASH_BITS - newLog2;
  mRemovedCount = 0;
  mGeneration++;

  /* Assign the new entry store to table. */
  memset(newEntryStore, 0, nbytes);
  char* oldEntryStore;
  char* oldEntryAddr;
  oldEntryAddr = oldEntryStore = mEntryStore;
  mEntryStore = newEntryStore;
  PLDHashMoveEntry moveEntry = mOps->moveEntry;

  /* Copy only live entries, leaving removed ones behind. */
  uint32_t oldCapacity = 1u << oldLog2;
  for (uint32_t i = 0; i < oldCapacity; ++i) {
    PLDHashEntryHdr* oldEntry = (PLDHashEntryHdr*)oldEntryAddr;
    if (ENTRY_IS_LIVE(oldEntry)) {
      oldEntry->mKeyHash &= ~COLLISION_FLAG;
      PLDHashEntryHdr* newEntry = FindFreeEntry(oldEntry->mKeyHash);
      NS_ASSERTION(EntryIsFree(newEntry), "EntryIsFree(newEntry)");
      moveEntry(this, oldEntry, newEntry);
      newEntry->mKeyHash = oldEntry->mKeyHash;
    }
    oldEntryAddr += mEntrySize;
  }

  free(oldEntryStore);
  return true;
}

MOZ_ALWAYS_INLINE PLDHashNumber
PLDHashTable::ComputeKeyHash(const void* aKey)
{
  MOZ_ASSERT(mEntryStore);

  PLDHashNumber keyHash = mOps->hashKey(this, aKey);
  keyHash *= PL_DHASH_GOLDEN_RATIO;

  /* Avoid 0 and 1 hash codes, they indicate free and removed entries. */
  ENSURE_LIVE_KEYHASH(keyHash);
  keyHash &= ~COLLISION_FLAG;

  return keyHash;
}

MOZ_ALWAYS_INLINE PLDHashEntryHdr*
PLDHashTable::Search(const void* aKey)
{
  MOZ_ASSERT(IsInitialized());

  INCREMENT_RECURSION_LEVEL(this);

  METER(mStats.mSearches++);

  PLDHashEntryHdr* entry =
    mEntryStore ? SearchTable<ForSearchOrRemove>(aKey, ComputeKeyHash(aKey))
                : nullptr;

  DECREMENT_RECURSION_LEVEL(this);

  return entry;
}

MOZ_ALWAYS_INLINE PLDHashEntryHdr*
PLDHashTable::Add(const void* aKey, const mozilla::fallible_t&)
{
  MOZ_ASSERT(IsInitialized());

  PLDHashNumber keyHash;
  PLDHashEntryHdr* entry;
  uint32_t capacity;

  MOZ_ASSERT(mRecursionLevel == 0);
  INCREMENT_RECURSION_LEVEL(this);

  // Allocate the entry storage if it hasn't already been allocated.
  if (!mEntryStore) {
    uint32_t nbytes;
    // We already checked this in Init(), so it must still be true.
    MOZ_RELEASE_ASSERT(SizeOfEntryStore(CapacityFromHashShift(), mEntrySize,
                                        &nbytes));
    mEntryStore = (char*)malloc(nbytes);
    if (!mEntryStore) {
      METER(mStats.mAddFailures++);
      entry = nullptr;
      goto exit;
    }
    memset(mEntryStore, 0, nbytes);
  }

  /*
   * If alpha is >= .75, grow or compress the table.  If aKey is already
   * in the table, we may grow once more than necessary, but only if we
   * are on the edge of being overloaded.
   */
  capacity = Capacity();
  if (mEntryCount + mRemovedCount >= MaxLoad(capacity)) {
    /* Compress if a quarter or more of all entries are removed. */
    int deltaLog2;
    if (mRemovedCount >= capacity >> 2) {
      METER(mStats.mCompresses++);
      deltaLog2 = 0;
    } else {
      METER(mStats.mGrows++);
      deltaLog2 = 1;
    }

    /*
     * Grow or compress the table.  If ChangeTable() fails, allow
     * overloading up to the secondary max.  Once we hit the secondary
     * max, return null.
     */
    if (!ChangeTable(deltaLog2) &&
        mEntryCount + mRemovedCount >= MaxLoadOnGrowthFailure(capacity)) {
      METER(mStats.mAddFailures++);
      entry = nullptr;
      goto exit;
    }
  }

  /*
   * Look for entry after possibly growing, so we don't have to add it,
   * then skip it while growing the table and re-add it after.
   */
  keyHash = ComputeKeyHash(aKey);
  entry = SearchTable<ForAdd>(aKey, keyHash);
  if (!ENTRY_IS_LIVE(entry)) {
    /* Initialize the entry, indicating that it's no longer free. */
    METER(mStats.mAddMisses++);
    if (ENTRY_IS_REMOVED(entry)) {
      METER(mStats.mAddOverRemoved++);
      mRemovedCount--;
      keyHash |= COLLISION_FLAG;
    }
    if (mOps->initEntry) {
      mOps->initEntry(entry, aKey);
    }
    entry->mKeyHash = keyHash;
    mEntryCount++;
  }
  METER(else {
    mStats.mAddHits++;
  });

exit:
  DECREMENT_RECURSION_LEVEL(this);
  return entry;
}

MOZ_ALWAYS_INLINE PLDHashEntryHdr*
PLDHashTable::Add(const void* aKey)
{
  PLDHashEntryHdr* entry = Add(aKey, fallible);
  if (!entry) {
    if (!mEntryStore) {
      // We OOM'd while allocating the initial entry storage.
      uint32_t nbytes;
      (void) SizeOfEntryStore(CapacityFromHashShift(), mEntrySize, &nbytes);
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

MOZ_ALWAYS_INLINE void
PLDHashTable::Remove(const void* aKey)
{
  MOZ_ASSERT(IsInitialized());

  MOZ_ASSERT(mRecursionLevel == 0);
  INCREMENT_RECURSION_LEVEL(this);

  PLDHashEntryHdr* entry =
    mEntryStore ? SearchTable<ForSearchOrRemove>(aKey, ComputeKeyHash(aKey))
                : nullptr;
  if (entry) {
    /* Clear this entry and mark it as "removed". */
    METER(mStats.mRemoveHits++);
    PL_DHashTableRawRemove(this, entry);

    /* Shrink if alpha is <= .25 and the table isn't too small already. */
    uint32_t capacity = Capacity();
    if (capacity > PL_DHASH_MIN_CAPACITY &&
        mEntryCount <= MinLoad(capacity)) {
      METER(mStats.mShrinks++);
      (void) ChangeTable(-1);
    }
  }
  METER(else {
    mStats.mRemoveMisses++;
  });

  DECREMENT_RECURSION_LEVEL(this);
}

PLDHashEntryHdr* PL_DHASH_FASTCALL
PL_DHashTableSearch(PLDHashTable* aTable, const void* aKey)
{
  return aTable->Search(aKey);
}

PLDHashEntryHdr* PL_DHASH_FASTCALL
PL_DHashTableAdd(PLDHashTable* aTable, const void* aKey,
                 const fallible_t& aFallible)
{
  return aTable->Add(aKey, aFallible);
}

PLDHashEntryHdr* PL_DHASH_FASTCALL
PL_DHashTableAdd(PLDHashTable* aTable, const void* aKey)
{
  return aTable->Add(aKey);
}

void PL_DHASH_FASTCALL
PL_DHashTableRemove(PLDHashTable* aTable, const void* aKey)
{
  aTable->Remove(aKey);
}

MOZ_ALWAYS_INLINE void
PLDHashTable::RawRemove(PLDHashEntryHdr* aEntry)
{
  MOZ_ASSERT(IsInitialized());
  MOZ_ASSERT(mEntryStore);

  MOZ_ASSERT(mRecursionLevel != IMMUTABLE_RECURSION_LEVEL);

  NS_ASSERTION(ENTRY_IS_LIVE(aEntry), "ENTRY_IS_LIVE(aEntry)");

  /* Load keyHash first in case clearEntry() goofs it. */
  PLDHashNumber keyHash = aEntry->mKeyHash;
  mOps->clearEntry(this, aEntry);
  if (keyHash & COLLISION_FLAG) {
    MARK_ENTRY_REMOVED(aEntry);
    mRemovedCount++;
  } else {
    METER(mStats.mRemoveFrees++);
    MARK_ENTRY_FREE(aEntry);
  }
  mEntryCount--;
}

void
PL_DHashTableRawRemove(PLDHashTable* aTable, PLDHashEntryHdr* aEntry)
{
  aTable->RawRemove(aEntry);
}

MOZ_ALWAYS_INLINE uint32_t
PLDHashTable::Enumerate(PLDHashEnumerator aEtor, void* aArg)
{
  MOZ_ASSERT(IsInitialized());

  if (!mEntryStore) {
    return 0;
  }

  INCREMENT_RECURSION_LEVEL(this);

  // Please keep this method in sync with the PLDHashTable::Iterator constructor
  // and ::NextEntry methods below in this file.
  char* entryAddr = mEntryStore;
  uint32_t capacity = Capacity();
  uint32_t tableSize = capacity * mEntrySize;
  char* entryLimit = entryAddr + tableSize;
  uint32_t i = 0;
  bool didRemove = false;

  if (ChaosMode::isActive(ChaosMode::HashTableIteration)) {
    // Start iterating at a random point in the hashtable. It would be
    // even more chaotic to iterate in fully random order, but that's a lot
    // more work.
    entryAddr += ChaosMode::randomUint32LessThan(capacity) * mEntrySize;
    if (entryAddr >= entryLimit) {
      entryAddr -= tableSize;
    }
  }

  for (uint32_t e = 0; e < capacity; ++e) {
    PLDHashEntryHdr* entry = (PLDHashEntryHdr*)entryAddr;
    if (ENTRY_IS_LIVE(entry)) {
      PLDHashOperator op = aEtor(this, entry, i++, aArg);
      if (op & PL_DHASH_REMOVE) {
        METER(mStats.mRemoveEnums++);
        PL_DHashTableRawRemove(this, entry);
        didRemove = true;
      }
      if (op & PL_DHASH_STOP) {
        break;
      }
    }
    entryAddr += mEntrySize;
    if (entryAddr >= entryLimit) {
      entryAddr -= tableSize;
    }
  }

  MOZ_ASSERT(!didRemove || mRecursionLevel == 1);

  /*
   * Shrink or compress if a quarter or more of all entries are removed, or
   * if the table is underloaded according to the minimum alpha, and is not
   * minimal-size already.  Do this only if we removed above, so non-removing
   * enumerations can count on stable |mEntryStore| until the next
   * Add, Remove, or removing-Enumerate.
   */
  if (didRemove &&
      (mRemovedCount >= capacity >> 2 ||
       (capacity > PL_DHASH_MIN_CAPACITY &&
        mEntryCount <= MinLoad(capacity)))) {
    METER(mStats.mEnumShrinks++);
    capacity = mEntryCount;
    capacity += capacity >> 1;
    if (capacity < PL_DHASH_MIN_CAPACITY) {
      capacity = PL_DHASH_MIN_CAPACITY;
    }

    uint32_t ceiling = CeilingLog2(capacity);
    ceiling -= PL_DHASH_BITS - mHashShift;

    (void) ChangeTable(ceiling);
  }

  DECREMENT_RECURSION_LEVEL(this);

  return i;
}

uint32_t
PL_DHashTableEnumerate(PLDHashTable* aTable, PLDHashEnumerator aEtor,
                       void* aArg)
{
  return aTable->Enumerate(aEtor, aArg);
}

struct SizeOfEntryExcludingThisArg
{
  size_t total;
  PLDHashSizeOfEntryExcludingThisFun sizeOfEntryExcludingThis;
  MallocSizeOf mallocSizeOf;
  void* arg;  // the arg passed by the user
};

static PLDHashOperator
SizeOfEntryExcludingThisEnumerator(PLDHashTable* aTable, PLDHashEntryHdr* aHdr,
                                   uint32_t aNumber, void* aArg)
{
  SizeOfEntryExcludingThisArg* e = (SizeOfEntryExcludingThisArg*)aArg;
  e->total += e->sizeOfEntryExcludingThis(aHdr, e->mallocSizeOf, e->arg);
  return PL_DHASH_NEXT;
}

MOZ_ALWAYS_INLINE size_t
PLDHashTable::SizeOfExcludingThis(
    PLDHashSizeOfEntryExcludingThisFun aSizeOfEntryExcludingThis,
    MallocSizeOf aMallocSizeOf, void* aArg /* = nullptr */) const
{
  MOZ_ASSERT(IsInitialized());

  if (!mEntryStore) {
    return 0;
  }

  size_t n = 0;
  n += aMallocSizeOf(mEntryStore);
  if (aSizeOfEntryExcludingThis) {
    SizeOfEntryExcludingThisArg arg2 = {
      0, aSizeOfEntryExcludingThis, aMallocSizeOf, aArg
    };
    PL_DHashTableEnumerate(const_cast<PLDHashTable*>(this),
                           SizeOfEntryExcludingThisEnumerator, &arg2);
    n += arg2.total;
  }
  return n;
}

MOZ_ALWAYS_INLINE size_t
PLDHashTable::SizeOfIncludingThis(
    PLDHashSizeOfEntryExcludingThisFun aSizeOfEntryExcludingThis,
    MallocSizeOf aMallocSizeOf, void* aArg /* = nullptr */) const
{
  MOZ_ASSERT(IsInitialized());

  return aMallocSizeOf(this) +
         SizeOfExcludingThis(aSizeOfEntryExcludingThis, aMallocSizeOf, aArg);
}

size_t
PL_DHashTableSizeOfExcludingThis(
    const PLDHashTable* aTable,
    PLDHashSizeOfEntryExcludingThisFun aSizeOfEntryExcludingThis,
    MallocSizeOf aMallocSizeOf, void* aArg /* = nullptr */)
{
  return aTable->SizeOfExcludingThis(aSizeOfEntryExcludingThis,
                                     aMallocSizeOf, aArg);
}

size_t
PL_DHashTableSizeOfIncludingThis(
    const PLDHashTable* aTable,
    PLDHashSizeOfEntryExcludingThisFun aSizeOfEntryExcludingThis,
    MallocSizeOf aMallocSizeOf, void* aArg /* = nullptr */)
{
  return aTable->SizeOfIncludingThis(aSizeOfEntryExcludingThis,
                                     aMallocSizeOf, aArg);
}

PLDHashTable::Iterator::Iterator(const PLDHashTable* aTable)
: mTable(aTable),
  mEntryAddr(mTable->mEntryStore),
  mEntryOffset(0)
{
  MOZ_ASSERT(mTable->IsInitialized());

  // Make sure that modifications can't simultaneously happen while the iterator
  // is active.
  INCREMENT_RECURSION_LEVEL(mTable);

  // The following code is taken from, and should be kept in sync with, the
  // PLDHashTable::Enumerate method above. The variables i and entryAddr (which
  // vary over the course of the for loop) are converted into mEntryOffset and
  // mEntryAddr, respectively.
  uint32_t capacity = mTable->Capacity();
  uint32_t tableSize = capacity * mTable->EntrySize();
  char* entryLimit = mEntryAddr + tableSize;

  if (ChaosMode::isActive(ChaosMode::HashTableIteration)) {
    // Start iterating at a random point in the hashtable. It would be
    // even more chaotic to iterate in fully random order, but that's a lot
    // more work.
    mEntryAddr += ChaosMode::randomUint32LessThan(capacity) * mTable->mEntrySize;
    if (mEntryAddr >= entryLimit) {
      mEntryAddr -= tableSize;
    }
  }
}

PLDHashTable::Iterator::Iterator(const Iterator& aIterator)
: mTable(aIterator.mTable),
  mEntryAddr(aIterator.mEntryAddr),
  mEntryOffset(aIterator.mEntryOffset)
{
  MOZ_ASSERT(mTable->IsInitialized());

  // We need the copy constructor only so that we can keep the recursion level
  // consistent.
  INCREMENT_RECURSION_LEVEL(mTable);
}

PLDHashTable::Iterator::~Iterator()
{
  MOZ_ASSERT(mTable->IsInitialized());

  DECREMENT_RECURSION_LEVEL(mTable);
}

bool PLDHashTable::Iterator::HasMoreEntries() const
{
  MOZ_ASSERT(mTable->IsInitialized());

  // Check the number of live entries seen, not the total number of entries
  // seen. To see why, consider what happens if the last entry is not live: we
  // would have to iterate after returning an entry to see if more live entries
  // exist.
  return mEntryOffset < mTable->EntryCount();
}

PLDHashEntryHdr* PLDHashTable::Iterator::NextEntry()
{
  MOZ_ASSERT(HasMoreEntries());

  // The following code is taken from, and should be kept in sync with, the
  // PLDHashTable::Enumerate method above. The variables i and entryAddr (which
  // vary over the course of the for loop) are converted into mEntryOffset and
  // mEntryAddr, respectively.
  uint32_t capacity = mTable->Capacity();
  uint32_t tableSize = capacity * mTable->mEntrySize;
  char* entryLimit = mEntryAddr + tableSize;

  // Strictly speaking, we don't need to iterate over the full capacity each
  // time. However, it is simpler to do so rather than unnecessarily track the
  // current number of entries checked as opposed to only live entries. If debug
  // checks pass, then this method will only iterate through the full capacity
  // once. If they fail, then this loop may end up returning the early entries
  // more than once.
  MOZ_ASSERT_IF(capacity > 0, mTable->mEntryStore);
  for (uint32_t e = 0; e < capacity; ++e) {
    PLDHashEntryHdr* entry = (PLDHashEntryHdr*)mEntryAddr;

    // Increment the count before returning so we don't keep returning the same
    // address. This may wrap around if ChaosMode is enabled.
    mEntryAddr += mTable->mEntrySize;
    if (mEntryAddr >= entryLimit) {
      mEntryAddr -= tableSize;
    }
    if (ENTRY_IS_LIVE(entry)) {
      ++mEntryOffset;
      return entry;
    }
  }

  // If the debug checks pass, then the above loop should always find a live
  // entry. If those checks are disabled, then it may be possible to reach this
  // if the table is empty and this method is called.
  MOZ_CRASH("Flagrant misuse of hashtable iterators not caught by checks.");
}

#ifdef DEBUG
MOZ_ALWAYS_INLINE void
PLDHashTable::MarkImmutable()
{
  MOZ_ASSERT(IsInitialized());

  mRecursionLevel = IMMUTABLE_RECURSION_LEVEL;
}

void
PL_DHashMarkTableImmutable(PLDHashTable* aTable)
{
  aTable->MarkImmutable();
}
#endif

#ifdef PL_DHASHMETER
#include <math.h>

void
PLDHashTable::DumpMeter(PLDHashEnumerator aDump, FILE* aFp)
{
  MOZ_ASSERT(IsInitialized());

  PLDHashNumber hash1, hash2, maxChainHash1, maxChainHash2;
  double sqsum, mean, variance, sigma;
  PLDHashEntryHdr* entry;

  char* entryAddr = mEntryStore;
  int sizeLog2 = PL_DHASH_BITS - mHashShift;
  uint32_t capacity = Capacity();
  uint32_t sizeMask = (1u << sizeLog2) - 1;
  uint32_t chainCount = 0, maxChainLen = 0;
  hash2 = 0;
  sqsum = 0;

  MOZ_ASSERT_IF(capacity > 0, mEntryStore);
  for (uint32_t i = 0; i < capacity; i++) {
    entry = (PLDHashEntryHdr*)entryAddr;
    entryAddr += mEntrySize;
    if (!ENTRY_IS_LIVE(entry)) {
      continue;
    }
    hash1 = HASH1(entry->mKeyHash & ~COLLISION_FLAG, mHashShift);
    PLDHashNumber saveHash1 = hash1;
    PLDHashEntryHdr* probe = ADDRESS_ENTRY(this, hash1);
    uint32_t chainLen = 1;
    if (probe == entry) {
      /* Start of a (possibly unit-length) chain. */
      chainCount++;
    } else {
      hash2 = HASH2(entry->mKeyHash & ~COLLISION_FLAG, sizeLog2, mHashShift);
      do {
        chainLen++;
        hash1 -= hash2;
        hash1 &= sizeMask;
        probe = ADDRESS_ENTRY(this, hash1);
      } while (probe != entry);
    }
    sqsum += chainLen * chainLen;
    if (chainLen > maxChainLen) {
      maxChainLen = chainLen;
      maxChainHash1 = saveHash1;
      maxChainHash2 = hash2;
    }
  }

  if (mEntryCount && chainCount) {
    mean = (double)mEntryCount / chainCount;
    variance = chainCount * sqsum - mEntryCount * mEntryCount;
    if (variance < 0 || chainCount == 1) {
      variance = 0;
    } else {
      variance /= chainCount * (chainCount - 1);
    }
    sigma = sqrt(variance);
  } else {
    mean = sigma = 0;
  }

  fprintf(aFp, "Double hashing statistics:\n");
  fprintf(aFp, "      capacity (in entries): %u\n", Capacity());
  fprintf(aFp, "          number of entries: %u\n", mEntryCount);
  fprintf(aFp, "  number of removed entries: %u\n", mRemovedCount);
  fprintf(aFp, "         number of searches: %u\n", mStats.mSearches);
  fprintf(aFp, "             number of hits: %u\n", mStats.mHits);
  fprintf(aFp, "           number of misses: %u\n", mStats.mMisses);
  fprintf(aFp, "      mean steps per search: %g\n",
          mStats.mSearches ? (double)mStats.mSteps / mStats.mSearches : 0.);
  fprintf(aFp, "     mean hash chain length: %g\n", mean);
  fprintf(aFp, "         standard deviation: %g\n", sigma);
  fprintf(aFp, "  maximum hash chain length: %u\n", maxChainLen);
  fprintf(aFp, "         number of searches: %u\n", mStats.mSearches);
  fprintf(aFp, " adds that made a new entry: %u\n", mStats.mAddMisses);
  fprintf(aFp, "adds that recycled removeds: %u\n", mStats.mAddOverRemoved);
  fprintf(aFp, "   adds that found an entry: %u\n", mStats.mAddHits);
  fprintf(aFp, "               add failures: %u\n", mStats.mAddFailures);
  fprintf(aFp, "             useful removes: %u\n", mStats.mRemoveHits);
  fprintf(aFp, "            useless removes: %u\n", mStats.mRemoveMisses);
  fprintf(aFp, "removes that freed an entry: %u\n", mStats.mRemoveFrees);
  fprintf(aFp, "  removes while enumerating: %u\n", mStats.mRemoveEnums);
  fprintf(aFp, "            number of grows: %u\n", mStats.mGrows);
  fprintf(aFp, "          number of shrinks: %u\n", mStats.mShrinks);
  fprintf(aFp, "       number of compresses: %u\n", mStats.mCompresses);
  fprintf(aFp, "number of enumerate shrinks: %u\n", mStats.mEnumShrinks);

  if (aDump && maxChainLen && hash2) {
    fputs("Maximum hash chain:\n", aFp);
    hash1 = maxChainHash1;
    hash2 = maxChainHash2;
    entry = ADDRESS_ENTRY(this, hash1);
    uint32_t i = 0;
    do {
      if (aDump(this, entry, i++, aFp) != PL_DHASH_NEXT) {
        break;
      }
      hash1 -= hash2;
      hash1 &= sizeMask;
      entry = ADDRESS_ENTRY(this, hash1);
    } while (!EntryIsFree(entry));
  }
}

void
PL_DHashTableDumpMeter(PLDHashTable* aTable, PLDHashEnumerator aDump, FILE* aFp)
{
  aTable->DumpMeter(aDump, aFp);
}
#endif /* PL_DHASHMETER */
