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
#include "mozilla/fallible.h"
#include "mozilla/MemoryReporting.h"
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

/* Primitive and forward-struct typedefs. */
typedef uint32_t                PLDHashNumber;
typedef struct PLDHashEntryHdr  PLDHashEntryHdr;
typedef struct PLDHashEntryStub PLDHashEntryStub;
typedef struct PLDHashTable     PLDHashTable;
typedef struct PLDHashTableOps  PLDHashTableOps;

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
 * Each hash table sub-type should nest the PLDHashEntryHdr structure at the
 * front of its particular entry type.  The keyHash member contains the result
 * of multiplying the hash code returned from the hashKey callback (see below)
 * by PL_DHASH_GOLDEN_RATIO, then constraining the result to avoid the magic 0
 * and 1 values.  The stored keyHash value is table size invariant, and it is
 * maintained automatically by PL_DHashTableOperate -- users should never set
 * it, and its only uses should be via the entry macros below.
 *
 * However, use PL_DHASH_ENTRY_IS_BUSY for faster liveness testing of entries
 * returned by PL_DHashTableOperate, as PL_DHashTableOperate never returns a
 * non-live, busy (i.e., removed) entry pointer to its caller.  See below for
 * more details on PL_DHashTableOperate's calling rules.
 */
struct PLDHashEntryHdr
{
  PLDHashNumber keyHash;  /* every entry must begin like this */
};

MOZ_ALWAYS_INLINE bool
PL_DHASH_ENTRY_IS_FREE(PLDHashEntryHdr* aEntry)
{
  return aEntry->keyHash == 0;
}

MOZ_ALWAYS_INLINE bool
PL_DHASH_ENTRY_IS_BUSY(PLDHashEntryHdr* aEntry)
{
  return !PL_DHASH_ENTRY_IS_FREE(aEntry);
}

/*
 * To consolidate keyHash computation and table grow/shrink code, we use a
 * single entry point for lookup, add, and remove operations.  The operation
 * codes are declared here, along with codes returned by PLDHashEnumerator
 * functions, which control PL_DHashTableEnumerate's behavior.
 */
typedef enum PLDHashOperator
{
  PL_DHASH_LOOKUP = 0,        /* lookup entry */
  PL_DHASH_ADD = 1,           /* add entry */
  PL_DHASH_REMOVE = 2,        /* remove entry, or enumerator says remove */
  PL_DHASH_NEXT = 0,          /* enumerator says continue */
  PL_DHASH_STOP = 1           /* enumerator says stop */
} PLDHashOperator;

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
 * aTable->ops->clearEntry) and free entry.  Then we check whether op contains
 * PL_DHASH_STOP; if so, stop enumerating and return the number of live entries
 * that were enumerated so far.  Return the total number of live entries when
 * enumeration completes normally.
 *
 * If etor calls PL_DHashTableOperate on table with op != PL_DHASH_LOOKUP, it
 * must return PL_DHASH_STOP; otherwise undefined behavior results.
 *
 * If any enumerator returns PL_DHASH_REMOVE, aTable->mEntryStore may be shrunk
 * or compressed after enumeration, but before PL_DHashTableEnumerate returns.
 * Such an enumerator therefore can't safely set aside entry pointers, but an
 * enumerator that never returns PL_DHASH_REMOVE can set pointers to entries
 * aside, e.g., to avoid copying live entries into an array of the entry type.
 * Copying entry pointers is cheaper, and safe so long as the caller of such a
 * "stable" Enumerate doesn't use the set-aside pointers after any call either
 * to PL_DHashTableOperate, or to an "unstable" form of Enumerate, which might
 * grow or shrink mEntryStore.
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
 * There used to be a long, math-heavy comment here about the merits of
 * double hashing vs. chaining; it was removed in bug 1058335. In short, double
 * hashing is more space-efficient unless the element size gets large (in which
 * case you should keep using double hashing but switch to using pointer
 * elements). Also, with double hashing, you can't safely hold an entry pointer
 * and use it after an ADD or REMOVE operation, unless you sample
 * aTable->mGeneration before adding or removing, and compare the sample after,
 * dereferencing the entry pointer only if aTable->mGeneration has not changed.
 */
struct PLDHashTable
{
  /*
   * Virtual operations; see below. This field is public because it's commonly
   * zeroed to indicate that a table is no longer live.
   */
  const PLDHashTableOps* ops;

  void*               data;           /* ops- and instance-specific data */

private:
  int16_t             mHashShift;     /* multiplicative hash shift */
  /*
   * |mRecursionLevel| is only used in debug builds, but is present in opt
   * builds to avoid binary compatibility problems when mixing DEBUG and
   * non-DEBUG components.  (Actually, even if it were removed,
   * sizeof(PLDHashTable) wouldn't change, due to struct padding.)
   */
  mutable uint16_t    mRecursionLevel;/* used to detect unsafe re-entry */
  uint32_t            mEntrySize;     /* number of bytes in an entry */
  uint32_t            mEntryCount;    /* number of entries in table */
  uint32_t            mRemovedCount;  /* removed entry sentinels in table */
  uint32_t            mGeneration;    /* entry storage generation number */
  char*               mEntryStore;    /* entry storage */

#ifdef PL_DHASHMETER
  struct PLDHashStats
  {
    uint32_t        mSearches;      /* total number of table searches */
    uint32_t        mSteps;         /* hash chain links traversed */
    uint32_t        mHits;          /* searches that found key */
    uint32_t        mMisses;        /* searches that didn't find key */
    uint32_t        mLookups;       /* number of PL_DHASH_LOOKUPs */
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

public:
  /*
   * Size in entries (gross, not net of free and removed sentinels) for table.
   * We store mHashShift rather than sizeLog2 to optimize the collision-free
   * case in SearchTable.
   */
  uint32_t Capacity() const
  {
    return ((uint32_t)1 << (PL_DHASH_BITS - mHashShift));
  }

  uint32_t EntrySize()  const { return mEntrySize; }
  uint32_t EntryCount() const { return mEntryCount; }
  uint32_t Generation() const { return mGeneration; }

  bool Init(const PLDHashTableOps* aOps, void* aData, uint32_t aEntrySize,
            const mozilla::fallible_t&, uint32_t aLength);

  void Finish();

  PLDHashEntryHdr* Operate(const void* aKey, PLDHashOperator aOp);

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
    Iterator(const PLDHashTable* aTable);
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
  PLDHashEntryHdr* PL_DHASH_FASTCALL
    SearchTable(const void* aKey, PLDHashNumber aKeyHash, PLDHashOperator aOp);

  PLDHashEntryHdr* PL_DHASH_FASTCALL FindFreeEntry(PLDHashNumber aKeyHash);

  bool ChangeTable(int aDeltaLog2);
};

/*
 * Table space at mEntryStore is allocated and freed using these callbacks.
 * The allocator should return null on error only (not if called with aNBytes
 * equal to 0; but note that pldhash.c code will never call with 0 aNBytes).
 */
typedef void* (*PLDHashAllocTable)(PLDHashTable* aTable, uint32_t aNBytes);

typedef void (*PLDHashFreeTable)(PLDHashTable* aTable, void* aPtr);

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
 * invoked during a PL_DHASH_REMOVE operation (see below for operation codes),
 * but only if the given key is found in the table.
 */
typedef void (*PLDHashClearEntry)(PLDHashTable* aTable,
                                  PLDHashEntryHdr* aEntry);

/*
 * Called when a table (whether allocated dynamically by itself, or nested in
 * a larger structure, or allocated on the stack) is finished.  This callback
 * allows aTable->ops-specific code to finalize aTable->data.
 */
typedef void (*PLDHashFinalize)(PLDHashTable* aTable);

/*
 * Initialize a new entry, apart from keyHash.  This function is called when
 * PL_DHashTableOperate's PL_DHASH_ADD case finds no existing entry for the
 * given key, and must add a new one.  At that point, aEntry->keyHash is not
 * set yet, to avoid claiming the last free entry in a severely overloaded
 * table.
 */
typedef bool (*PLDHashInitEntry)(PLDHashTable* aTable, PLDHashEntryHdr* aEntry,
                                 const void* aKey);

/*
 * Finally, the "vtable" structure for PLDHashTable.  The first eight hooks
 * must be provided by implementations; they're called unconditionally by the
 * generic pldhash.c code.  Hooks after these may be null.
 *
 * Summary of allocation-related hook usage with C++ placement new emphasis:
 *  allocTable          Allocate raw bytes with malloc, no ctors run.
 *  freeTable           Free raw bytes with free, no dtors run.
 *  initEntry           Call placement new using default key-based ctor.
 *                      Return true on success, false on error.
 *  moveEntry           Call placement new using copy ctor, run dtor on old
 *                      entry storage.
 *  clearEntry          Run dtor on entry.
 *  finalize            Stub unless aTable->data was initialized and needs to
 *                      be finalized.
 *
 * Note the reason why initEntry is optional: the default hooks (stubs) clear
 * entry storage:  On successful PL_DHashTableOperate(tbl, key, PL_DHASH_ADD),
 * the returned entry pointer addresses an entry struct whose keyHash member
 * has been set non-zero, but all other entry members are still clear (null).
 * PL_DHASH_ADD callers can test such members to see whether the entry was
 * newly created by the PL_DHASH_ADD call that just succeeded.  If placement
 * new or similar initialization is required, define an initEntry hook.  Of
 * course, the clearEntry hook must zero or null appropriately.
 *
 * XXX assumes 0 is null for pointer types.
 */
struct PLDHashTableOps
{
  /* Mandatory hooks.  All implementations must provide these. */
  PLDHashAllocTable   allocTable;
  PLDHashFreeTable    freeTable;
  PLDHashHashKey      hashKey;
  PLDHashMatchEntry   matchEntry;
  PLDHashMoveEntry    moveEntry;
  PLDHashClearEntry   clearEntry;
  PLDHashFinalize     finalize;

  /* Optional hooks start here.  If null, these are not called. */
  PLDHashInitEntry    initEntry;
};

/*
 * Default implementations for the above ops.
 */
void* PL_DHashAllocTable(PLDHashTable* aTable, uint32_t aNBytes);

void PL_DHashFreeTable(PLDHashTable* aTable, void* aPtr);

PLDHashNumber PL_DHashStringKey(PLDHashTable* aTable, const void* aKey);

/* A minimal entry contains a keyHash header and a void key pointer. */
struct PLDHashEntryStub
{
  PLDHashEntryHdr hdr;
  const void*     key;
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

void PL_DHashFinalizeStub(PLDHashTable* aTable);

/*
 * If you use PLDHashEntryStub or a subclass of it as your entry struct, and
 * if your entries move via memcpy and clear via memset(0), you can use these
 * stub operations.
 */
const PLDHashTableOps* PL_DHashGetStubOps(void);

/*
 * Dynamically allocate a new PLDHashTable using malloc, initialize it using
 * PL_DHashTableInit, and return its address.  Return null on malloc failure.
 * Note that the entry storage at aTable->mEntryStore will be allocated using
 * the aOps->allocTable callback.
 */
PLDHashTable* PL_NewDHashTable(
  const PLDHashTableOps* aOps, void* aData, uint32_t aEntrySize,
  uint32_t aLength = PL_DHASH_DEFAULT_INITIAL_LENGTH);

/*
 * Finalize aTable's data, free its entry storage (via aTable->ops->freeTable),
 * and return the memory starting at aTable to the malloc heap.
 */
void PL_DHashTableDestroy(PLDHashTable* aTable);

/*
 * Initialize aTable with aOps, aData, aEntrySize, and aCapacity. The table's
 * initial capacity will be chosen such that |aLength| elements can be inserted
 * without rehashing. If |aLength| is a power-of-two, this capacity will be
 * |2*length|.
 *
 * This function will crash if it can't allocate enough memory, or if
 * |aEntrySize| and/or |aLength| are too large.
 */
void PL_DHashTableInit(
  PLDHashTable* aTable, const PLDHashTableOps* aOps, void* aData,
  uint32_t aEntrySize, uint32_t aLength = PL_DHASH_DEFAULT_INITIAL_LENGTH);

/*
 * Initialize aTable. This is the same as PL_DHashTableInit, except that it
 * returns a boolean indicating success, rather than crashing on failure.
 */
MOZ_WARN_UNUSED_RESULT bool PL_DHashTableInit(
  PLDHashTable* aTable, const PLDHashTableOps* aOps, void* aData,
  uint32_t aEntrySize, const mozilla::fallible_t&,
  uint32_t aLength = PL_DHASH_DEFAULT_INITIAL_LENGTH);

/*
 * Finalize aTable's data, free its entry storage using aTable->ops->freeTable,
 * and leave its members unchanged from their last live values (which leaves
 * pointers dangling).  If you want to burn cycles clearing aTable, it's up to
 * your code to call memset.
 */
void PL_DHashTableFinish(PLDHashTable* aTable);

/*
 * To lookup a key in table, call:
 *
 *  entry = PL_DHashTableOperate(table, key, PL_DHASH_LOOKUP);
 *
 * If PL_DHASH_ENTRY_IS_BUSY(entry) is true, key was found and it identifies
 * entry.  If PL_DHASH_ENTRY_IS_FREE(entry) is true, key was not found.
 *
 * To add an entry identified by key to table, call:
 *
 *  entry = PL_DHashTableOperate(table, key, PL_DHASH_ADD);
 *
 * If entry is null upon return, then either the table is severely overloaded,
 * and memory can't be allocated for entry storage via aTable->ops->allocTable;
 * Or if aTable->ops->initEntry is non-null, the aTable->ops->initEntry op may
 * have returned false.
 *
 * Otherwise, aEntry->keyHash has been set so that PL_DHASH_ENTRY_IS_BUSY(entry)
 * is true, and it is up to the caller to initialize the key and value parts
 * of the entry sub-type, if they have not been set already (i.e. if entry was
 * not already in the table, and if the optional initEntry hook was not used).
 *
 * To remove an entry identified by key from table, call:
 *
 *  (void) PL_DHashTableOperate(table, key, PL_DHASH_REMOVE);
 *
 * If key's entry is found, it is cleared (via table->ops->clearEntry) and
 * the entry is marked so that PL_DHASH_ENTRY_IS_FREE(entry).  This operation
 * returns null unconditionally; you should ignore its return value.
 */
PLDHashEntryHdr* PL_DHASH_FASTCALL
PL_DHashTableOperate(PLDHashTable* aTable, const void* aKey,
                     PLDHashOperator aOp);

/*
 * Remove an entry already accessed via LOOKUP or ADD.
 *
 * NB: this is a "raw" or low-level routine, intended to be used only where
 * the inefficiency of a full PL_DHashTableOperate (which rehashes in order
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
 * pointed to by entries.  Doesn't measure |ops| because it's often shared
 * between tables, nor |data| because it's opaque.
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
