/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/Maybe.h"
#include "mozilla/StaticMutex.h"

#include "HashTable.h"
#include "InfallibleVector.h"
#include "ProcessRecordReplay.h"
#include "ProcessRedirect.h"
#include "ValueIndex.h"

#include <unordered_set>

namespace mozilla {
namespace recordreplay {

// Hash tables frequently incorporate pointer values into the hash numbers they
// compute, which are not guaranteed to be the same between recording and
// replaying and consequently lead to inconsistent hash numbers and iteration
// order between recording and replaying, which can in turn affect the order in
// which recorded events occur. HashTable stabilization is designed to deal
// with this problem, for specific kinds of hashtables (PLD and PL tables)
// which are based on callbacks.
//
// When the table is constructed, if we are recording/replaying then the
// callbacks are replaced with an alternate set that produces consistent hash
// numbers between recording and replay. If during replay the additions and
// removals to the tables occur in the same order that they did during
// recording, then the structure of the tables and the order in which elements
// are visited during iteration will be the same.
//
// Ensuring that hash numbers are consistent is done as follows: for each
// table, we keep track of the keys that are in the table. When computing the
// hash of an arbitrary key, we look for a matching key in the table, using
// that key's hash if found. Otherwise, a new hash is generated from an
// incrementing counter.

typedef uint32_t HashNumber;

class StableHashTableInfo {
  // Information about a key in the table: the key pointer, along with the new
  // hash number we have generated for the key.
  struct KeyInfo {
    const void* mKey;
    HashNumber mNewHash;
  };

  // Table mapping original hash numbers (produced by the table's hash
  // function) to a vector with all keys sharing that original hash number.
  struct HashInfo {
    InfallibleVector<KeyInfo> mKeys;
  };
  typedef std::unordered_map<HashNumber, UniquePtr<HashInfo>> HashToKeyMap;
  HashToKeyMap mHashToKey;

  // Table mapping key pointers to their original hash number.
  typedef std::unordered_map<const void*, HashNumber> KeyToHashMap;
  KeyToHashMap mKeyToHash;

  // The last key which the hash function was called on, and the new hash
  // number which we generated for that key.
  const void* mLastKey;
  HashNumber mLastNewHash;

  // Counter for generating new hash numbers for entries added to the table.
  // This increases monotonically, though it is fine if it overflows.
  uint32_t mHashGenerator;

  // Buffer with executable memory for use in binding functions.
  uint8_t* mCallbackStorage;
  uint32_t mCallbackStorageSize;
  static const size_t CallbackStorageCapacity = 4096;

  // Whether this table has been marked as destroyed and is unusable. This is
  // temporary state to detect UAF bugs related to this class.
  bool mDestroyed;

  // Associated table and hash of its callback storage, for more integrity
  // checking.
  void* mTable;
  uint32_t mCallbackHash;

  // Get an existing key in the table.
  KeyInfo* FindKeyInfo(HashNumber aOriginalHash, const void* aKey,
                       HashInfo** aHashInfo = nullptr) {
    HashToKeyMap::iterator iter = mHashToKey.find(aOriginalHash);
    MOZ_RELEASE_ASSERT(iter != mHashToKey.end());

    HashInfo* hashInfo = iter->second.get();
    for (KeyInfo& keyInfo : hashInfo->mKeys) {
      if (keyInfo.mKey == aKey) {
        if (aHashInfo) {
          *aHashInfo = hashInfo;
        }
        return &keyInfo;
      }
    }
    MOZ_CRASH();
  }

 public:
  StableHashTableInfo()
      : mLastKey(nullptr),
        mLastNewHash(0),
        mHashGenerator(0),
        mCallbackStorage(nullptr),
        mDestroyed(false),
        mTable(nullptr),
        mCallbackHash(0) {
    // Use AllocateMemory, as the result will have RWX permissions.
    mCallbackStorage =
        (uint8_t*)AllocateMemory(CallbackStorageCapacity, MemoryKind::Tracked);

    MarkValid();
  }

  ~StableHashTableInfo() {
    MOZ_RELEASE_ASSERT(mHashToKey.empty());
    DeallocateMemory(mCallbackStorage, CallbackStorageCapacity,
                     MemoryKind::Tracked);

    UnmarkValid();
  }

  bool IsDestroyed() { return mDestroyed; }

  void MarkDestroyed() {
    MOZ_RELEASE_ASSERT(!IsDestroyed());
    mDestroyed = true;
  }

  void CheckIntegrity(void* aTable) {
    MOZ_RELEASE_ASSERT(aTable);
    if (!mTable) {
      mTable = aTable;
      mCallbackHash = HashBytes(mCallbackStorage, mCallbackStorageSize);
    } else {
      MOZ_RELEASE_ASSERT(mTable == aTable);
      MOZ_RELEASE_ASSERT(mCallbackHash ==
                         HashBytes(mCallbackStorage, mCallbackStorageSize));
    }
  }

  void AddKey(HashNumber aOriginalHash, const void* aKey, HashNumber aNewHash) {
    HashToKeyMap::iterator iter = mHashToKey.find(aOriginalHash);
    if (iter == mHashToKey.end()) {
      iter = mHashToKey
                 .insert(HashToKeyMap::value_type(aOriginalHash,
                                                  MakeUnique<HashInfo>()))
                 .first;
    }
    HashInfo* hashInfo = iter->second.get();

    KeyInfo key;
    key.mKey = aKey;
    key.mNewHash = aNewHash;
    hashInfo->mKeys.append(key);

    mKeyToHash.insert(KeyToHashMap::value_type(aKey, aOriginalHash));
  }

  void RemoveKey(HashNumber aOriginalHash, const void* aKey) {
    HashInfo* hashInfo;
    KeyInfo* keyInfo = FindKeyInfo(aOriginalHash, aKey, &hashInfo);
    hashInfo->mKeys.erase(keyInfo);

    if (hashInfo->mKeys.length() == 0) {
      mHashToKey.erase(aOriginalHash);
    }

    mKeyToHash.erase(aKey);
  }

  HashNumber FindKeyHash(HashNumber aOriginalHash, const void* aKey) {
    KeyInfo* info = FindKeyInfo(aOriginalHash, aKey);
    return info->mNewHash;
  }

  // Look for a key in the table with a matching original hash and for which
  // aMatch() is true for, returning its new hash number if found.
  bool HasMatchingKey(HashNumber aOriginalHash,
                      const std::function<bool(const void*)>& aMatch,
                      HashNumber* aNewHash) {
    HashToKeyMap::const_iterator iter = mHashToKey.find(aOriginalHash);
    if (iter != mHashToKey.end()) {
      HashInfo* hashInfo = iter->second.get();
      for (const KeyInfo& keyInfo : hashInfo->mKeys) {
        if (aMatch(keyInfo.mKey)) {
          *aNewHash = keyInfo.mNewHash;
          return true;
        }
      }
    }
    return false;
  }

  HashNumber GetOriginalHashNumber(const void* aKey) {
    KeyToHashMap::iterator iter = mKeyToHash.find(aKey);
    MOZ_RELEASE_ASSERT(iter != mKeyToHash.end());
    return iter->second;
  }

  class Assembler : public recordreplay::Assembler {
   public:
    StableHashTableInfo& mInfo;

    explicit Assembler(StableHashTableInfo& aInfo)
        : recordreplay::Assembler(aInfo.mCallbackStorage,
                                  CallbackStorageCapacity),
          mInfo(aInfo) {}

    ~Assembler() {
      mInfo.mCallbackStorageSize = Current() - mInfo.mCallbackStorage;
    }
  };

  // Use the callback storage buffer to create a new function T which has one
  // fewer argument than S and calls S with aArgument bound to the last
  // argument position. See BindFunctionArgument in ProcessRedirect.h
  template <typename S, typename T>
  void NewBoundFunction(Assembler& aAssembler, S aFunction, void* aArgument,
                        size_t aArgumentPosition, T* aTarget) {
    void* nfn = BindFunctionArgument(BitwiseCast<void*>(aFunction), aArgument,
                                     aArgumentPosition, aAssembler);
    BitwiseCast(nfn, aTarget);
  }

  // Set the last queried key for this table, and generate a new hash number
  // for it.
  HashNumber SetLastKey(const void* aKey) {
    // Remember the last key queried, so that if it is then added to the table
    // we know what hash number to use.
    mLastKey = aKey;
    mLastNewHash = mHashGenerator++;
    return mLastNewHash;
  }

  bool HasLastKey() { return !!mLastKey; }

  HashNumber GetLastNewHash(const void* aKey) {
    MOZ_RELEASE_ASSERT(aKey == mLastKey);
    return mLastNewHash;
  }

  bool IsEmpty() { return mHashToKey.empty(); }

  // Move aOther's contents into this one and clear aOther out. Callbacks for
  // the tables are left alone.
  void MoveContentsFrom(StableHashTableInfo& aOther) {
    mHashToKey = std::move(aOther.mHashToKey);
    mKeyToHash = std::move(aOther.mKeyToHash);
    mHashGenerator = aOther.mHashGenerator;

    aOther.mHashToKey.clear();
    aOther.mKeyToHash.clear();
    aOther.mHashGenerator = 0;

    mLastKey = aOther.mLastKey = nullptr;
    mLastNewHash = aOther.mLastNewHash = 0;
  }

  // Set of valid StableHashTableInfo pointers. We use this to determine whether
  // we are dealing with an actual StableHashTableInfo. Despite our best efforts
  // some hashtables do not go through stabilization (e.g. they have static
  // constructors that run before record/replay state is initialized).
  static std::unordered_set<StableHashTableInfo*>* gHashInfos;
  static SpinLock gHashInfosLock;

  inline bool IsValid() {
    AutoSpinLock lock(gHashInfosLock);
    return gHashInfos && gHashInfos->find(this) != gHashInfos->end();
  }

  inline void MarkValid() {
    AutoSpinLock lock(gHashInfosLock);
    if (!gHashInfos) {
      gHashInfos = new std::unordered_set<StableHashTableInfo*>();
    }
    gHashInfos->insert(this);
  }

  inline void UnmarkValid() {
    AutoSpinLock lock(gHashInfosLock);
    gHashInfos->erase(this);
  }
};

std::unordered_set<StableHashTableInfo*>* StableHashTableInfo::gHashInfos;
SpinLock StableHashTableInfo::gHashInfosLock;

///////////////////////////////////////////////////////////////////////////////
// PLHashTable Stabilization
///////////////////////////////////////////////////////////////////////////////

// For each PLHashTable in the process, a PLHashTableInfo is generated. This
// structure becomes the |allocPriv| for the table, handled by the new
// callbacks given to it.
struct PLHashTableInfo : public StableHashTableInfo {
  // Original callbacks for the table.
  PLHashFunction mKeyHash;
  PLHashComparator mKeyCompare;
  PLHashComparator mValueCompare;
  const PLHashAllocOps* mAllocOps;

  // Original private value for the table.
  void* mAllocPrivate;

  PLHashTableInfo(PLHashFunction aKeyHash, PLHashComparator aKeyCompare,
                  PLHashComparator aValueCompare,
                  const PLHashAllocOps* aAllocOps, void* aAllocPrivate)
      : mKeyHash(aKeyHash),
        mKeyCompare(aKeyCompare),
        mValueCompare(aValueCompare),
        mAllocOps(aAllocOps),
        mAllocPrivate(aAllocPrivate) {}

  static PLHashTableInfo* MaybeFromPrivate(void* aAllocPrivate) {
    PLHashTableInfo* info = reinterpret_cast<PLHashTableInfo*>(aAllocPrivate);
    if (info->IsValid()) {
      MOZ_RELEASE_ASSERT(!info->IsDestroyed());
      return info;
    }
    return nullptr;
  }

  static PLHashTableInfo* FromPrivate(void* aAllocPrivate) {
    PLHashTableInfo* info = MaybeFromPrivate(aAllocPrivate);
    MOZ_RELEASE_ASSERT(info);
    return info;
  }
};

static void* WrapPLHashAllocTable(void* aAllocPrivate, PRSize aSize) {
  PLHashTableInfo* info = PLHashTableInfo::FromPrivate(aAllocPrivate);
  return info->mAllocOps
             ? info->mAllocOps->allocTable(info->mAllocPrivate, aSize)
             : malloc(aSize);
}

static void WrapPLHashFreeTable(void* aAllocPrivate, void* aItem) {
  PLHashTableInfo* info = PLHashTableInfo::FromPrivate(aAllocPrivate);
  if (info->mAllocOps) {
    info->mAllocOps->freeTable(info->mAllocPrivate, aItem);
  } else {
    free(aItem);
  }
}

static PLHashEntry* WrapPLHashAllocEntry(void* aAllocPrivate,
                                         const void* aKey) {
  PLHashTableInfo* info = PLHashTableInfo::FromPrivate(aAllocPrivate);

  if (info->HasLastKey()) {
    uint32_t originalHash = info->mKeyHash(aKey);
    info->AddKey(originalHash, aKey, info->GetLastNewHash(aKey));
  } else {
    // A few PLHashTables are manipulated directly by Gecko code, in which case
    // the hashes are supplied directly to the table and we don't have a chance
    // to modify them. Fortunately, none of these tables are iterated in a way
    // that can cause the replay to diverge, so just punt in these cases.
    MOZ_RELEASE_ASSERT(info->IsEmpty());
  }

  return info->mAllocOps
             ? info->mAllocOps->allocEntry(info->mAllocPrivate, aKey)
             : (PLHashEntry*)malloc(sizeof(PLHashEntry));
}

static void WrapPLHashFreeEntry(void* aAllocPrivate, PLHashEntry* he,
                                PRUintn flag) {
  PLHashTableInfo* info = PLHashTableInfo::FromPrivate(aAllocPrivate);

  // Ignore empty tables, due to the raw table manipulation described above.
  if (flag == HT_FREE_ENTRY && !info->IsEmpty()) {
    uint32_t originalHash = info->GetOriginalHashNumber(he->key);
    info->RemoveKey(originalHash, he->key);
  }

  if (info->mAllocOps) {
    info->mAllocOps->freeEntry(info->mAllocPrivate, he, flag);
  } else if (flag == HT_FREE_ENTRY) {
    free(he);
  }
}

static PLHashAllocOps gWrapPLHashAllocOps = {
    WrapPLHashAllocTable, WrapPLHashFreeTable, WrapPLHashAllocEntry,
    WrapPLHashFreeEntry};

static uint32_t PLHashComputeHash(void* aKey, PLHashTableInfo* aInfo) {
  MOZ_RELEASE_ASSERT(!aInfo->IsDestroyed());
  uint32_t originalHash = aInfo->mKeyHash(aKey);
  HashNumber newHash;
  if (aInfo->HasMatchingKey(
          originalHash,
          [=](const void* aExistingKey) {
            return aInfo->mKeyCompare(aKey, aExistingKey);
          },
          &newHash)) {
    return newHash;
  }
  return aInfo->SetLastKey(aKey);
}

void GeneratePLHashTableCallbacks(PLHashFunction* aKeyHash,
                                  PLHashComparator* aKeyCompare,
                                  PLHashComparator* aValueCompare,
                                  const PLHashAllocOps** aAllocOps,
                                  void** aAllocPrivate) {
  PLHashTableInfo* info = new PLHashTableInfo(
      *aKeyHash, *aKeyCompare, *aValueCompare, *aAllocOps, *aAllocPrivate);
  PLHashTableInfo::Assembler assembler(*info);
  info->NewBoundFunction(assembler, PLHashComputeHash, info, 1, aKeyHash);
  *aAllocOps = &gWrapPLHashAllocOps;
  *aAllocPrivate = info;
}

void DestroyPLHashTableCallbacks(void* aAllocPrivate) {
  PLHashTableInfo* info = PLHashTableInfo::MaybeFromPrivate(aAllocPrivate);
  if (info) {
    info->MarkDestroyed();
    // delete info;
  }
}

void CheckPLHashTable(PLHashTable* aTable) {
  PLHashTableInfo* info = PLHashTableInfo::MaybeFromPrivate(aTable->allocPriv);
  if (info) {
    info->CheckIntegrity(aTable);
  }
}

///////////////////////////////////////////////////////////////////////////////
// PLDHashTable Stabilization
///////////////////////////////////////////////////////////////////////////////

// For each PLDHashTable in the process, a PLDHashTableInfo is generated. This
// structure is supplied to its callbacks using bound functions.
struct PLDHashTableInfo : public StableHashTableInfo {
  // Original callbacks for the table.
  const PLDHashTableOps* mOps;

  // Wrapper callbacks for the table.
  PLDHashTableOps mNewOps;

  explicit PLDHashTableInfo(const PLDHashTableOps* aOps) : mOps(aOps) {
    PodZero(&mNewOps);
  }

  static PLDHashTableInfo* MaybeFromOps(const PLDHashTableOps* aOps) {
    PLDHashTableInfo* res = reinterpret_cast<PLDHashTableInfo*>(
        (uint8_t*)aOps - offsetof(PLDHashTableInfo, mNewOps));
    if (res->IsValid()) {
      MOZ_RELEASE_ASSERT(!res->IsDestroyed());
      return res;
    }
    return nullptr;
  }

  static PLDHashTableInfo* FromOps(const PLDHashTableOps* aOps) {
    PLDHashTableInfo* res = MaybeFromOps(aOps);
    MOZ_RELEASE_ASSERT(res);
    return res;
  }

  static void CheckIntegrity(PLDHashTable* aTable) {
    PLDHashTableInfo* info = MaybeFromOps(aTable->RecordReplayWrappedOps());
    if (info) {
      info->StableHashTableInfo::CheckIntegrity(aTable);
    }
  }
};

static PLDHashNumber PLDHashTableComputeHash(const void* aKey,
                                             PLDHashTableInfo* aInfo) {
  MOZ_RELEASE_ASSERT(!aInfo->IsDestroyed());
  uint32_t originalHash = aInfo->mOps->hashKey(aKey);
  HashNumber newHash;
  if (aInfo->HasMatchingKey(
          originalHash,
          [=](const void* aExistingKey) {
            return aInfo->mOps->matchEntry((PLDHashEntryHdr*)aExistingKey,
                                           aKey);
          },
          &newHash)) {
    return newHash;
  }
  return aInfo->SetLastKey(aKey);
}

static void PLDHashTableMoveEntry(PLDHashTable* aTable,
                                  const PLDHashEntryHdr* aFrom,
                                  PLDHashEntryHdr* aTo,
                                  PLDHashTableInfo* aInfo) {
  MOZ_RELEASE_ASSERT(!aInfo->IsDestroyed());
  aInfo->mOps->moveEntry(aTable, aFrom, aTo);

  uint32_t originalHash = aInfo->GetOriginalHashNumber(aFrom);
  uint32_t newHash = aInfo->FindKeyHash(originalHash, aFrom);

  aInfo->RemoveKey(originalHash, aFrom);
  aInfo->AddKey(originalHash, aTo, newHash);
}

static void PLDHashTableClearEntry(PLDHashTable* aTable,
                                   PLDHashEntryHdr* aEntry,
                                   PLDHashTableInfo* aInfo) {
  MOZ_RELEASE_ASSERT(!aInfo->IsDestroyed());
  aInfo->mOps->clearEntry(aTable, aEntry);

  uint32_t originalHash = aInfo->GetOriginalHashNumber(aEntry);
  aInfo->RemoveKey(originalHash, aEntry);
}

static void PLDHashTableInitEntry(PLDHashEntryHdr* aEntry, const void* aKey,
                                  PLDHashTableInfo* aInfo) {
  MOZ_RELEASE_ASSERT(!aInfo->IsDestroyed());

  if (aInfo->mOps->initEntry) {
    aInfo->mOps->initEntry(aEntry, aKey);
  }

  uint32_t originalHash = aInfo->mOps->hashKey(aKey);
  aInfo->AddKey(originalHash, aEntry, aInfo->GetLastNewHash(aKey));
}

extern "C" {

MOZ_EXPORT const PLDHashTableOps*
RecordReplayInterface_InternalGeneratePLDHashTableCallbacks(
    const PLDHashTableOps* aOps) {
  PLDHashTableInfo* info = new PLDHashTableInfo(aOps);
  PLDHashTableInfo::Assembler assembler(*info);
  info->NewBoundFunction(assembler, PLDHashTableComputeHash, info, 1,
                         &info->mNewOps.hashKey);
  info->mNewOps.matchEntry = aOps->matchEntry;
  info->NewBoundFunction(assembler, PLDHashTableMoveEntry, info, 3,
                         &info->mNewOps.moveEntry);
  info->NewBoundFunction(assembler, PLDHashTableClearEntry, info, 2,
                         &info->mNewOps.clearEntry);
  info->NewBoundFunction(assembler, PLDHashTableInitEntry, info, 2,
                         &info->mNewOps.initEntry);
  return &info->mNewOps;
}

MOZ_EXPORT const PLDHashTableOps*
RecordReplayInterface_InternalUnwrapPLDHashTableCallbacks(
    const PLDHashTableOps* aOps) {
  PLDHashTableInfo* info = PLDHashTableInfo::FromOps(aOps);
  return info->mOps;
}

MOZ_EXPORT void RecordReplayInterface_InternalDestroyPLDHashTableCallbacks(
    const PLDHashTableOps* aOps) {
  // Primordial PLDHashTables used in the copy constructor might not have any
  // ops.
  if (!aOps) {
    return;
  }

  // Note: PLDHashTables with static ctors might have been constructed before
  // record/replay state was initialized and have their normal ops. Check the
  // info is valid via MaybeFromOps before destroying the info.
  PLDHashTableInfo* info = PLDHashTableInfo::MaybeFromOps(aOps);
  if (info) {
    info->MarkDestroyed();
  }
  // delete info;
}

MOZ_EXPORT void RecordReplayInterface_InternalMovePLDHashTableContents(
    const PLDHashTableOps* aFirstOps, const PLDHashTableOps* aSecondOps) {
  PLDHashTableInfo* firstInfo = PLDHashTableInfo::FromOps(aFirstOps);
  PLDHashTableInfo* secondInfo = PLDHashTableInfo::FromOps(aSecondOps);

  secondInfo->MoveContentsFrom(*firstInfo);
}

}  // extern "C"

void CheckPLDHashTable(PLDHashTable* aTable) {
  PLDHashTableInfo::CheckIntegrity(aTable);
}

}  // namespace recordreplay
}  // namespace mozilla
