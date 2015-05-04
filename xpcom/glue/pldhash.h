/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef pldhash_h___
#define pldhash_h___
/*
 * Double hashing, a la Knuth 6.
 */
#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h" // for MOZ_ALWAYS_INLINE
#include "mozilla/fallible.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Move.h"
#include "mozilla/Types.h"
#include "nscore.h"

#ifdef PL_DHASHMETER
#include <stdio.h>
#endif

#if defined(__GNUC__) && defined(__i386__)
#define PL_DHASH_FASTCALL __attribute__ ((regparm (3),stdcall))
#elif defined(XP_WIN)
#define PL_DHASH_FASTCALL __fastcall
#else
#define PL_DHASH_FASTCALL
#endif

/*
 * Table capacity limit; do not exceed.  The max capacity used to be 1<<23 but
 * that occasionally that wasn't enough.  Making it much bigger than 1<<26
 * probably isn't worthwhile -- tables that big are kind of ridiculous.  Also,
 * the growth operation will (deliberately) fail if |capacity * mEntrySize|
 * overflows a uint32_t, and mEntrySize is always at least 8 bytes.
 */
#define PL_DHASH_MAX_CAPACITY           ((uint32_t)1 << 26)

#define PL_DHASH_MIN_CAPACITY           8

/*
 * Making this half of the max capacity ensures it'll fit. Nobody should need
 * an initial length anywhere nearly this large, anyway.
 */
#define PL_DHASH_MAX_INITIAL_LENGTH     (PL_DHASH_MAX_CAPACITY / 2)

/* This gives a default initial capacity of 8. */
#define PL_DHASH_DEFAULT_INITIAL_LENGTH 4

/*
 * Multiplicative hash uses an unsigned 32 bit integer and the golden ratio,
 * expressed as a fixed-point 32-bit fraction.
 */
#define PL_DHASH_BITS           32
#define PL_DHASH_GOLDEN_RATIO   0x9E3779B9U

typedef uint32_t PLDHashNumber;

class PLDHashTable;
struct PLDHashTableOps;

/*
 * Table entry header structure.
 *
 * In order to allow in-line allocation of key and value, we do not declare
 * either here.  Instead, the API uses const void *key as a formal parameter.
 * The key need not be stored in the entry; it may be part of the value, but
 * need not be stored at all.
 *
 * Callback types are defined below and grouped into the PLDHashTableOps
 * structure, for single static initialization per hash table sub-type.
 *
 * Each hash table sub-type should make its entry type a subclass of
 * PLDHashEntryHdr. The mKeyHash member contains the result of multiplying the
 * hash code returned from the hashKey callback (see below) by
 * PL_DHASH_GOLDEN_RATIO, then constraining the result to avoid the magic 0 and
 * 1 values. The stored mKeyHash value is table size invariant, and it is
 * maintained automatically -- users need never access it.
 */
struct PLDHashEntryHdr
{
private:
  friend class PLDHashTable;

  PLDHashNumber mKeyHash;
};

/*
 * These are the codes returned by PLDHashEnumerator functions, which control
 * PL_DHashTableEnumerate's behavior.
 */
enum PLDHashOperator
{
  PL_DHASH_NEXT = 0,          /* enumerator says continue */
  PL_DHASH_STOP = 1,          /* enumerator says stop */
  PL_DHASH_REMOVE = 2         /* enumerator says remove */
};

/*
 * Enumerate entries in table using etor:
 *
 *   count = PL_DHashTableEnumerate(table, etor, arg);
 *
 * PL_DHashTableEnumerate calls etor like so:
 *
 *   op = etor(table, entry, number, arg);
 *
 * where number is a zero-based ordinal assigned to live entries according to
 * their order in aTable->mEntryStore.
 *
 * The return value, op, is treated as a set of flags.  If op is PL_DHASH_NEXT,
 * then continue enumerating.  If op contains PL_DHASH_REMOVE, then clear (via
 * aTable->mOps->clearEntry) and free entry.  Then we check whether op contains
 * PL_DHASH_STOP; if so, stop enumerating and return the number of live entries
 * that were enumerated so far.  Return the total number of live entries when
 * enumeration completes normally.
 *
 * If etor calls PL_DHashTableAdd or PL_DHashTableRemove on table, it must
 * return PL_DHASH_STOP; otherwise undefined behavior results.
 *
 * If any enumerator returns PL_DHASH_REMOVE, aTable->mEntryStore may be shrunk
 * or compressed after enumeration, but before PL_DHashTableEnumerate returns.
 * Such an enumerator therefore can't safely set aside entry pointers, but an
 * enumerator that never returns PL_DHASH_REMOVE can set pointers to entries
 * aside, e.g., to avoid copying live entries into an array of the entry type.
 * Copying entry pointers is cheaper, and safe so long as the caller of such a
 * "stable" Enumerate doesn't use the set-aside pointers after any call either
 * to PL_DHashTableAdd or PL_DHashTableRemove, or to an "unstable" form of
 * Enumerate, which might grow or shrink mEntryStore.
 *
 * If your enumerator wants to remove certain entries, but set aside pointers
 * to other entries that it retains, it can use PL_DHashTableRawRemove on the
 * entries to be removed, returning PL_DHASH_NEXT to skip them.  Likewise, if
 * you want to remove entries, but for some reason you do not want mEntryStore
 * to be shrunk or compressed, you can call PL_DHashTableRawRemove safely on
 * the entry being enumerated, rather than returning PL_DHASH_REMOVE.
 */
typedef PLDHashOperator (*PLDHashEnumerator)(PLDHashTable* aTable,
                                             PLDHashEntryHdr* aHdr,
                                             uint32_t aNumber, void* aArg);

typedef size_t (*PLDHashSizeOfEntryExcludingThisFun)(
  PLDHashEntryHdr* aHdr, mozilla::MallocSizeOf aMallocSizeOf, void* aArg);

/*
 * A PLDHashTable is currently 8 words (without the PL_DHASHMETER overhead)
 * on most architectures, and may be allocated on the stack or within another
 * structure or class (see below for the Init and Finish functions to use).
 *
 * No entry storage is allocated until the first element is added. This means
 * that empty hash tables are cheap, which is good because they are common.
 *
 * There used to be a long, math-heavy comment here about the merits of
 * double hashing vs. chaining; it was removed in bug 1058335. In short, double
 * hashing is more space-efficient unless the element size gets large (in which
 * case you should keep using double hashing but switch to using pointer
 * elements). Also, with double hashing, you can't safely hold an entry pointer
 * and use it after an ADD or REMOVE operation, unless you sample
 * aTable->mGeneration before adding or removing, and compare the sample after,
 * dereferencing the entry pointer only if aTable->mGeneration has not changed.
 */
class PLDHashTable
{
private:
  const PLDHashTableOps* mOps;        /* Virtual operations; see below. */
  int16_t             mHashShift;     /* multiplicative hash shift */
  uint32_t            mEntrySize;     /* number of bytes in an entry */
  uint32_t            mEntryCount;    /* number of entries in table */
  uint32_t            mRemovedCount;  /* removed entry sentinels in table */
  uint32_t            mGeneration;    /* entry storage generation number */
  char*               mEntryStore;    /* entry storage; allocated lazily */
#ifdef PL_DHASHMETER
  struct PLDHashStats
  {
    uint32_t        mSearches;      /* total number of table searches */
    uint32_t        mSteps;         /* hash chain links traversed */
    uint32_t        mHits;          /* searches that found key */
    uint32_t        mMisses;        /* searches that didn't find key */
    uint32_t        mSearches;      /* number of Search() calls */
    uint32_t        mAddMisses;     /* adds that miss, and do work */
    uint32_t        mAddOverRemoved;/* adds that recycled a removed entry */
    uint32_t        mAddHits;       /* adds that hit an existing entry */
    uint32_t        mAddFailures;   /* out-of-memory during add growth */
    uint32_t        mRemoveHits;    /* removes that hit, and do work */
    uint32_t        mRemoveMisses;  /* useless removes that miss */
    uint32_t        mRemoveFrees;   /* removes that freed entry directly */
    uint32_t        mRemoveEnums;   /* removes done by Enumerate */
    uint32_t        mGrows;         /* table expansions */
    uint32_t        mShrinks;       /* table contractions */
    uint32_t        mCompresses;    /* table compressions */
    uint32_t        mEnumShrinks;   /* contractions after Enumerate */
  } mStats;
#endif

#ifdef DEBUG
  // We use an atomic counter here so that the various ++/-- operations can't
  // get corrupted when a table is shared between threads. The associated
  // assertions should in no way be taken to mean that thread safety is being
  // validated! Proper synchronization and thread safety assertions must be
  // employed by any consumers.
  mutable mozilla::Atomic<uint32_t> mRecursionLevel;
#endif

public:
  // The most important thing here is that we zero |mOps| because it's used to
  // determine if Init() has been called. (The use of MOZ_CONSTEXPR means all
  // the other members must be initialized too.)
  MOZ_CONSTEXPR PLDHashTable()
    : mOps(nullptr)
    , mHashShift(0)
    , mEntrySize(0)
    , mEntryCount(0)
    , mRemovedCount(0)
    , mGeneration(0)
    , mEntryStore(nullptr)
#ifdef PL_DHASHMETER
    , mStats()
#endif
#ifdef DEBUG
    , mRecursionLevel()
#endif
  {}

  PLDHashTable(PLDHashTable&& aOther)
    : mOps(nullptr)
    , mEntryStore(nullptr)
#ifdef DEBUG
    , mRecursionLevel(0)
#endif
  {
    *this = mozilla::Move(aOther);
  }

  PLDHashTable& operator=(PLDHashTable&& aOther);

  bool IsInitialized() const { return !!mOps; }

  // These should be used rarely.
  const PLDHashTableOps* const Ops() { return mOps; }
  void SetOps(const PLDHashTableOps* aOps) { mOps = aOps; }

  /*
   * Size in entries (gross, not net of free and removed sentinels) for table.
   * This can be zero if no elements have been added yet, in which case the
   * entry storage will not have yet been allocated.
   */
  uint32_t Capacity() const
  {
    return mEntryStore ? CapacityFromHashShift() : 0;
  }

  uint32_t EntrySize()  const { return mEntrySize; }
  uint32_t EntryCount() const { return mEntryCount; }
  uint32_t Generation() const { return mGeneration; }

  void Init(const PLDHashTableOps* aOps, uint32_t aEntrySize, uint32_t aLength);

  void Finish();

  PLDHashEntryHdr* Search(const void* aKey);
  PLDHashEntryHdr* Add(const void* aKey, const mozilla::fallible_t&);
  PLDHashEntryHdr* Add(const void* aKey);
  void Remove(const void* aKey);

  void RawRemove(PLDHashEntryHdr* aEntry);

  uint32_t Enumerate(PLDHashEnumerator aEtor, void* aArg);

  size_t SizeOfIncludingThis(
    PLDHashSizeOfEntryExcludingThisFun aSizeOfEntryExcludingThis,
    mozilla::MallocSizeOf aMallocSizeOf, void* aArg = nullptr) const;

  size_t SizeOfExcludingThis(
    PLDHashSizeOfEntryExcludingThisFun aSizeOfEntryExcludingThis,
    mozilla::MallocSizeOf aMallocSizeOf, void* aArg = nullptr) const;

#ifdef DEBUG
  void MarkImmutable();
#endif

  void MoveEntryStub(const PLDHashEntryHdr* aFrom, PLDHashEntryHdr* aTo);

  void ClearEntryStub(PLDHashEntryHdr* aEntry);

  void FreeStringKey(PLDHashEntryHdr* aEntry);

#ifdef PL_DHASHMETER
  void DumpMeter(PLDHashEnumerator aDump, FILE* aFp);
#endif

  /**
   * This is an iterator that works over the elements of PLDHashtable. It is not
   * safe to modify the hashtable while it is being iterated over; on debug
   * builds, attempting to do so will result in an assertion failure.
   */
  class Iterator {
  public:
    explicit Iterator(const PLDHashTable* aTable);
    Iterator(const Iterator& aIterator);
    ~Iterator();
    bool HasMoreEntries() const;
    PLDHashEntryHdr* NextEntry();

  private:
    const PLDHashTable* mTable;       /* Main table pointer */
    char* mEntryAddr;                 /* Pointer to the next entry to check */
    uint32_t mEntryOffset;            /* The number of the elements returned */
  };

  Iterator Iterate() const { return Iterator(this); }

private:
  static bool EntryIsFree(PLDHashEntryHdr* aEntry);

  // We store mHashShift rather than sizeLog2 to optimize the collision-free
  // case in SearchTable.
  uint32_t CapacityFromHashShift() const
  {
    return ((uint32_t)1 << (PL_DHASH_BITS - mHashShift));
  }

  PLDHashNumber ComputeKeyHash(const void* aKey);

  enum SearchReason { ForSearchOrRemove, ForAdd };

  template <SearchReason Reason>
  PLDHashEntryHdr* PL_DHASH_FASTCALL
    SearchTable(const void* aKey, PLDHashNumber aKeyHash);

  PLDHashEntryHdr* PL_DHASH_FASTCALL FindFreeEntry(PLDHashNumber aKeyHash);

  bool ChangeTable(int aDeltaLog2);

  PLDHashTable(const PLDHashTable& aOther) = delete;
  PLDHashTable& operator=(const PLDHashTable& aOther) = delete;
};

/*
 * Compute the hash code for a given key to be looked up, added, or removed
 * from aTable.  A hash code may have any PLDHashNumber value.
 */
typedef PLDHashNumber (*PLDHashHashKey)(PLDHashTable* aTable,
                                        const void* aKey);

/*
 * Compare the key identifying aEntry in aTable with the provided key parameter.
 * Return true if keys match, false otherwise.
 */
typedef bool (*PLDHashMatchEntry)(PLDHashTable* aTable,
                                  const PLDHashEntryHdr* aEntry,
                                  const void* aKey);

/*
 * Copy the data starting at aFrom to the new entry storage at aTo. Do not add
 * reference counts for any strong references in the entry, however, as this
 * is a "move" operation: the old entry storage at from will be freed without
 * any reference-decrementing callback shortly.
 */
typedef void (*PLDHashMoveEntry)(PLDHashTable* aTable,
                                 const PLDHashEntryHdr* aFrom,
                                 PLDHashEntryHdr* aTo);

/*
 * Clear the entry and drop any strong references it holds.  This callback is
 * invoked by PL_DHashTableRemove(), but only if the given key is found in the
 * table.
 */
typedef void (*PLDHashClearEntry)(PLDHashTable* aTable,
                                  PLDHashEntryHdr* aEntry);

/*
 * Initialize a new entry, apart from mKeyHash.  This function is called when
 * PL_DHashTableAdd finds no existing entry for the given key, and must add a
 * new one.  At that point, aEntry->mKeyHash is not set yet, to avoid claiming
 * the last free entry in a severely overloaded table.
 */
typedef void (*PLDHashInitEntry)(PLDHashEntryHdr* aEntry, const void* aKey);

/*
 * Finally, the "vtable" structure for PLDHashTable.  The first four hooks
 * must be provided by implementations; they're called unconditionally by the
 * generic pldhash.c code.  Hooks after these may be null.
 *
 * Summary of allocation-related hook usage with C++ placement new emphasis:
 *  initEntry           Call placement new using default key-based ctor.
 *  moveEntry           Call placement new using copy ctor, run dtor on old
 *                      entry storage.
 *  clearEntry          Run dtor on entry.
 *
 * Note the reason why initEntry is optional: the default hooks (stubs) clear
 * entry storage:  On successful PL_DHashTableAdd(tbl, key), the returned entry
 * pointer addresses an entry struct whose mKeyHash member has been set
 * non-zero, but all other entry members are still clear (null).
 * PL_DHashTableAdd callers can test such members to see whether the entry was
 * newly created by the PL_DHashTableAdd call that just succeeded.  If
 * placement new or similar initialization is required, define an initEntry
 * hook.  Of course, the clearEntry hook must zero or null appropriately.
 *
 * XXX assumes 0 is null for pointer types.
 */
struct PLDHashTableOps
{
  /* Mandatory hooks.  All implementations must provide these. */
  PLDHashHashKey      hashKey;
  PLDHashMatchEntry   matchEntry;
  PLDHashMoveEntry    moveEntry;
  PLDHashClearEntry   clearEntry;

  /* Optional hooks start here.  If null, these are not called. */
  PLDHashInitEntry    initEntry;
};

/*
 * Default implementations for the above mOps.
 */

PLDHashNumber PL_DHashStringKey(PLDHashTable* aTable, const void* aKey);

/* A minimal entry is a subclass of PLDHashEntryHdr and has void key pointer. */
struct PLDHashEntryStub : public PLDHashEntryHdr
{
  const void* key;
};

PLDHashNumber PL_DHashVoidPtrKeyStub(PLDHashTable* aTable, const void* aKey);

bool PL_DHashMatchEntryStub(PLDHashTable* aTable,
                            const PLDHashEntryHdr* aEntry,
                            const void* aKey);

bool PL_DHashMatchStringKey(PLDHashTable* aTable,
                            const PLDHashEntryHdr* aEntry,
                            const void* aKey);

void
PL_DHashMoveEntryStub(PLDHashTable* aTable,
                      const PLDHashEntryHdr* aFrom,
                      PLDHashEntryHdr* aTo);

void PL_DHashClearEntryStub(PLDHashTable* aTable, PLDHashEntryHdr* aEntry);

void PL_DHashFreeStringKey(PLDHashTable* aTable, PLDHashEntryHdr* aEntry);

/*
 * If you use PLDHashEntryStub or a subclass of it as your entry struct, and
 * if your entries move via memcpy and clear via memset(0), you can use these
 * stub operations.
 */
const PLDHashTableOps* PL_DHashGetStubOps(void);

/*
 * Dynamically allocate a new PLDHashTable, initialize it using
 * PL_DHashTableInit, and return its address. Never returns null.
 */
PLDHashTable* PL_NewDHashTable(
  const PLDHashTableOps* aOps, uint32_t aEntrySize,
  uint32_t aLength = PL_DHASH_DEFAULT_INITIAL_LENGTH);

/*
 * Free |aTable|'s entry storage and |aTable| itself (both via
 * aTable->mOps->freeTable). Use this function to destroy a PLDHashTable that
 * was allocated on the heap via PL_NewDHashTable().
 */
void PL_DHashTableDestroy(PLDHashTable* aTable);

/*
 * Initialize aTable with aOps and aEntrySize. The table's initial capacity
 * will be chosen such that |aLength| elements can be inserted without
 * rehashing; if |aLength| is a power-of-two, this capacity will be |2*length|.
 * However, because entry storage is allocated lazily, this initial capacity
 * won't be relevant until the first element is added; prior to that the
 * capacity will be zero.
 *
 * This function will crash if |aEntrySize| and/or |aLength| are too large.
 */
void PL_DHashTableInit(
  PLDHashTable* aTable, const PLDHashTableOps* aOps,
  uint32_t aEntrySize, uint32_t aLength = PL_DHASH_DEFAULT_INITIAL_LENGTH);

/*
 * Free |aTable|'s entry storage (via aTable->mOps->freeTable). Use this
 * function to destroy a PLDHashTable that is allocated on the stack or in
 * static memory and was created via PL_DHashTableInit().
 */
void PL_DHashTableFinish(PLDHashTable* aTable);

/*
 * To search for a key in |table|, call:
 *
 *  entry = PL_DHashTableSearch(table, key);
 *
 * If |entry| is non-null, |key| was found.  If |entry| is null, key was not
 * found.
 */
PLDHashEntryHdr* PL_DHASH_FASTCALL
PL_DHashTableSearch(PLDHashTable* aTable, const void* aKey);

/*
 * To add an entry identified by key to table, call:
 *
 *  entry = PL_DHashTableAdd(table, key, mozilla::fallible);
 *
 * If entry is null upon return, then the table is severely overloaded and
 * memory can't be allocated for entry storage.
 *
 * Otherwise, aEntry->mKeyHash has been set so that
 * PLDHashTable::EntryIsFree(entry) is false, and it is up to the caller to
 * initialize the key and value parts of the entry sub-type, if they have not
 * been set already (i.e. if entry was not already in the table, and if the
 * optional initEntry hook was not used).
 */
PLDHashEntryHdr* PL_DHASH_FASTCALL
PL_DHashTableAdd(PLDHashTable* aTable, const void* aKey,
                 const mozilla::fallible_t&);

/*
 * This is like the other PL_DHashTableAdd() function, but infallible, and so
 * never returns null.
 */
PLDHashEntryHdr* PL_DHASH_FASTCALL
PL_DHashTableAdd(PLDHashTable* aTable, const void* aKey);

/*
 * To remove an entry identified by key from table, call:
 *
 *  PL_DHashTableRemove(table, key);
 *
 * If key's entry is found, it is cleared (via table->mOps->clearEntry) and
 * the entry is marked so that PL_DHASH_ENTRY_IS_FREE(entry).  This operation
 * returns null unconditionally; you should ignore its return value.
 */
void PL_DHASH_FASTCALL
PL_DHashTableRemove(PLDHashTable* aTable, const void* aKey);

/*
 * Remove an entry already accessed via PL_DHashTableSearch or PL_DHashTableAdd.
 *
 * NB: this is a "raw" or low-level routine, intended to be used only where
 * the inefficiency of a full PL_DHashTableRemove (which rehashes in order
 * to find the entry given its key) is not tolerable.  This function does not
 * shrink the table if it is underloaded.  It does not update mStats #ifdef
 * PL_DHASHMETER, either.
 */
void PL_DHashTableRawRemove(PLDHashTable* aTable, PLDHashEntryHdr* aEntry);

uint32_t
PL_DHashTableEnumerate(PLDHashTable* aTable, PLDHashEnumerator aEtor,
                       void* aArg);

/**
 * Measure the size of the table's entry storage, and if
 * |aSizeOfEntryExcludingThis| is non-nullptr, measure the size of things
 * pointed to by entries.  Doesn't measure |mOps| because it's often shared
 * between tables.
 */
size_t PL_DHashTableSizeOfExcludingThis(
  const PLDHashTable* aTable,
  PLDHashSizeOfEntryExcludingThisFun aSizeOfEntryExcludingThis,
  mozilla::MallocSizeOf aMallocSizeOf, void* aArg = nullptr);

/**
 * Like PL_DHashTableSizeOfExcludingThis, but includes sizeof(*this).
 */
size_t PL_DHashTableSizeOfIncludingThis(
  const PLDHashTable* aTable,
  PLDHashSizeOfEntryExcludingThisFun aSizeOfEntryExcludingThis,
  mozilla::MallocSizeOf aMallocSizeOf, void* aArg = nullptr);

#ifdef DEBUG
/**
 * Mark a table as immutable for the remainder of its lifetime.  This
 * changes the implementation from ASSERTing one set of invariants to
 * ASSERTing a different set.
 *
 * When a table is NOT marked as immutable, the table implementation
 * asserts that the table is not mutated from its own callbacks.  It
 * assumes the caller protects the table from being accessed on multiple
 * threads simultaneously.
 *
 * When the table is marked as immutable, the re-entry assertions will
 * no longer trigger erroneously due to multi-threaded access.  Instead,
 * mutations will cause assertions.
 */
void PL_DHashMarkTableImmutable(PLDHashTable* aTable);
#endif

#ifdef PL_DHASHMETER
void PL_DHashTableDumpMeter(PLDHashTable* aTable,
                            PLDHashEnumerator aDump, FILE* aFp);
#endif

#endif /* pldhash_h___ */
