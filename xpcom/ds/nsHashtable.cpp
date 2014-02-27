/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date         Modified by     Description of modification
 * 04/20/2000   IBM Corp.       Added PR_CALLBACK for Optlink use in OS2
 */

#include <string.h>
#include "prlog.h"
#include "prlock.h"
#include "nsHashtable.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsCRTGlue.h"
#include "mozilla/HashFunctions.h"

using namespace mozilla;

struct HTEntry : PLDHashEntryHdr
{
    nsHashKey* key;
    void* value;
};

//
// Key operations
//

static bool
matchKeyEntry(PLDHashTable*, const PLDHashEntryHdr* entry,
              const void* key)
{
    const HTEntry* hashEntry =
        static_cast<const HTEntry*>(entry);

    if (hashEntry->key == key)
        return true;

    const nsHashKey* otherKey = reinterpret_cast<const nsHashKey*>(key);
    return otherKey->Equals(hashEntry->key);
}

static PLDHashNumber
hashKey(PLDHashTable* table, const void* key)
{
    const nsHashKey* hashKey = static_cast<const nsHashKey*>(key);

    return hashKey->HashCode();
}

static void
clearHashEntry(PLDHashTable* table, PLDHashEntryHdr* entry)
{
    HTEntry* hashEntry = static_cast<HTEntry*>(entry);

    // leave it up to the nsHashKey destructor to free the "value"
    delete hashEntry->key;
    hashEntry->key = nullptr;
    hashEntry->value = nullptr;  // probably not necessary, but for
                                // sanity's sake
}


static const PLDHashTableOps hashtableOps = {
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    hashKey,
    matchKeyEntry,
    PL_DHashMoveEntryStub,
    clearHashEntry,
    PL_DHashFinalizeStub,
    nullptr,
};


//
// Enumerator callback
//

struct _HashEnumerateArgs {
    nsHashtableEnumFunc fn;
    void* arg;
};

static PLDHashOperator
hashEnumerate(PLDHashTable* table, PLDHashEntryHdr* hdr, uint32_t i, void *arg)
{
    _HashEnumerateArgs* thunk = (_HashEnumerateArgs*)arg;
    HTEntry* entry = static_cast<HTEntry*>(hdr);

    if (thunk->fn(entry->key, entry->value, thunk->arg))
        return PL_DHASH_NEXT;
    return PL_DHASH_STOP;
}

//
// HashKey
//

nsHashKey::~nsHashKey(void)
{
    MOZ_COUNT_DTOR(nsHashKey);
}

nsresult
nsHashKey::Write(nsIObjectOutputStream* aStream) const
{
    NS_NOTREACHED("oops");
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsHashtable::nsHashtable(uint32_t aInitSize, bool aThreadSafe)
  : mLock(nullptr), mEnumerating(false)
{
    MOZ_COUNT_CTOR(nsHashtable);

    bool result = PL_DHashTableInit(&mHashtable, &hashtableOps, nullptr,
                                    sizeof(HTEntry), aInitSize, fallible_t());
    NS_ASSERTION(result, "Hashtable failed to initialize");

    // make sure we detect this later
    if (!result)
        mHashtable.ops = nullptr;

    if (aThreadSafe) {
        mLock = PR_NewLock();
        if (mLock == nullptr) {
            // Cannot create a lock. If running on a multiprocessing system
            // we are sure to die.
            PR_ASSERT(mLock != nullptr);
        }
    }
}

nsHashtable::~nsHashtable() {
    MOZ_COUNT_DTOR(nsHashtable);
    if (mHashtable.ops)
        PL_DHashTableFinish(&mHashtable);
    if (mLock) PR_DestroyLock(mLock);
}

bool nsHashtable::Exists(nsHashKey *aKey)
{
    if (mLock) PR_Lock(mLock);

    if (!mHashtable.ops) {
        if (mLock) PR_Unlock(mLock);
        return false;
    }

    PLDHashEntryHdr *entry =
        PL_DHashTableOperate(&mHashtable, aKey, PL_DHASH_LOOKUP);

    bool exists = PL_DHASH_ENTRY_IS_BUSY(entry);

    if (mLock) PR_Unlock(mLock);

    return exists;
}

void *nsHashtable::Put(nsHashKey *aKey, void *aData)
{
    void *res =  nullptr;

    if (!mHashtable.ops) return nullptr;

    if (mLock) PR_Lock(mLock);

    // shouldn't be adding an item during enumeration
    PR_ASSERT(!mEnumerating);

    HTEntry* entry =
        static_cast<HTEntry*>
                   (PL_DHashTableOperate(&mHashtable, aKey, PL_DHASH_ADD));

    if (entry) {                // don't return early, or you'll be locked!
        if (entry->key) {
            // existing entry, need to boot the old value
            res = entry->value;
            entry->value = aData;
        } else {
            // new entry (leave res == null)
            entry->key = aKey->Clone();
            entry->value = aData;
        }
    }

    if (mLock) PR_Unlock(mLock);

    return res;
}

void *nsHashtable::Get(nsHashKey *aKey)
{
    if (!mHashtable.ops) return nullptr;

    if (mLock) PR_Lock(mLock);

    HTEntry* entry =
        static_cast<HTEntry*>
                   (PL_DHashTableOperate(&mHashtable, aKey, PL_DHASH_LOOKUP));
    void *ret = PL_DHASH_ENTRY_IS_BUSY(entry) ? entry->value : nullptr;

    if (mLock) PR_Unlock(mLock);

    return ret;
}

void *nsHashtable::Remove(nsHashKey *aKey)
{
    if (!mHashtable.ops) return nullptr;

    if (mLock) PR_Lock(mLock);

    // shouldn't be adding an item during enumeration
    PR_ASSERT(!mEnumerating);


    // need to see if the entry is actually there, in order to get the
    // old value for the result
    HTEntry* entry =
        static_cast<HTEntry*>
                   (PL_DHashTableOperate(&mHashtable, aKey, PL_DHASH_LOOKUP));
    void *res;

    if (PL_DHASH_ENTRY_IS_FREE(entry)) {
        // value wasn't in the table anyway
        res = nullptr;
    } else {
        res = entry->value;
        PL_DHashTableRawRemove(&mHashtable, entry);
    }

    if (mLock) PR_Unlock(mLock);

    return res;
}

// XXX This method was called _hashEnumerateCopy, but it didn't copy the element!
// I don't know how this was supposed to work since the elements are neither copied
// nor refcounted.
static PLDHashOperator
hashEnumerateShare(PLDHashTable *table, PLDHashEntryHdr *hdr,
                   uint32_t i, void *arg)
{
    nsHashtable *newHashtable = (nsHashtable *)arg;
    HTEntry * entry = static_cast<HTEntry*>(hdr);

    newHashtable->Put(entry->key, entry->value);
    return PL_DHASH_NEXT;
}

nsHashtable * nsHashtable::Clone()
{
    if (!mHashtable.ops) return nullptr;

    bool threadSafe = (mLock != nullptr);
    nsHashtable *newHashTable = new nsHashtable(mHashtable.entryCount, threadSafe);

    PL_DHashTableEnumerate(&mHashtable, hashEnumerateShare, newHashTable);
    return newHashTable;
}

void nsHashtable::Enumerate(nsHashtableEnumFunc aEnumFunc, void* aClosure)
{
    if (!mHashtable.ops) return;

    bool wasEnumerating = mEnumerating;
    mEnumerating = true;
    _HashEnumerateArgs thunk;
    thunk.fn = aEnumFunc;
    thunk.arg = aClosure;
    PL_DHashTableEnumerate(&mHashtable, hashEnumerate, &thunk);
    mEnumerating = wasEnumerating;
}

static PLDHashOperator
hashEnumerateRemove(PLDHashTable*, PLDHashEntryHdr* hdr, uint32_t i, void *arg)
{
    HTEntry* entry = static_cast<HTEntry*>(hdr);
    _HashEnumerateArgs* thunk = (_HashEnumerateArgs*)arg;
    if (thunk) {
        return thunk->fn(entry->key, entry->value, thunk->arg)
            ? PL_DHASH_REMOVE
            : PL_DHASH_STOP;
    }
    return PL_DHASH_REMOVE;
}

void nsHashtable::Reset() {
    Reset(nullptr);
}

void nsHashtable::Reset(nsHashtableEnumFunc destroyFunc, void* aClosure)
{
    if (!mHashtable.ops) return;

    _HashEnumerateArgs thunk, *thunkp;
    if (!destroyFunc) {
        thunkp = nullptr;
    } else {
        thunkp = &thunk;
        thunk.fn = destroyFunc;
        thunk.arg = aClosure;
    }
    PL_DHashTableEnumerate(&mHashtable, hashEnumerateRemove, thunkp);
}

// nsISerializable helpers

nsHashtable::nsHashtable(nsIObjectInputStream* aStream,
                         nsHashtableReadEntryFunc aReadEntryFunc,
                         nsHashtableFreeEntryFunc aFreeEntryFunc,
                         nsresult *aRetVal)
  : mLock(nullptr),
    mEnumerating(false)
{
    MOZ_COUNT_CTOR(nsHashtable);

    bool threadSafe;
    nsresult rv = aStream->ReadBoolean(&threadSafe);
    if (NS_SUCCEEDED(rv)) {
        if (threadSafe) {
            mLock = PR_NewLock();
            if (!mLock)
                rv = NS_ERROR_OUT_OF_MEMORY;
        }

        if (NS_SUCCEEDED(rv)) {
            uint32_t count;
            rv = aStream->Read32(&count);

            if (NS_SUCCEEDED(rv)) {
                bool status =
                    PL_DHashTableInit(&mHashtable, &hashtableOps,
                                      nullptr, sizeof(HTEntry), count,
                                      fallible_t());
                if (!status) {
                    mHashtable.ops = nullptr;
                    rv = NS_ERROR_OUT_OF_MEMORY;
                } else {
                    for (uint32_t i = 0; i < count; i++) {
                        nsHashKey* key;
                        void *data;

                        rv = aReadEntryFunc(aStream, &key, &data);
                        if (NS_SUCCEEDED(rv)) {
                            Put(key, data);

                            // XXXbe must we clone key? can't we hand off
                            aFreeEntryFunc(aStream, key, nullptr);
                        }
                    }
                }
            }
        }
    }
    *aRetVal = rv;
}

struct WriteEntryArgs {
    nsIObjectOutputStream*    mStream;
    nsHashtableWriteDataFunc  mWriteDataFunc;
    nsresult                  mRetVal;
};

static bool
WriteEntry(nsHashKey *aKey, void *aData, void* aClosure)
{
    WriteEntryArgs* args = (WriteEntryArgs*) aClosure;
    nsIObjectOutputStream* stream = args->mStream;

    nsresult rv = aKey->Write(stream);
    if (NS_SUCCEEDED(rv))
        rv = args->mWriteDataFunc(stream, aData);

    args->mRetVal = rv;
    return true;
}

nsresult
nsHashtable::Write(nsIObjectOutputStream* aStream,
                   nsHashtableWriteDataFunc aWriteDataFunc) const
{
    if (!mHashtable.ops)
        return NS_ERROR_OUT_OF_MEMORY;
    bool threadSafe = (mLock != nullptr);
    nsresult rv = aStream->WriteBoolean(threadSafe);
    if (NS_FAILED(rv)) return rv;

    // Write the entry count first, so we know how many key/value pairs to read.
    uint32_t count = mHashtable.entryCount;
    rv = aStream->Write32(count);
    if (NS_FAILED(rv)) return rv;

    // Write all key/value pairs in the table.
    WriteEntryArgs args = {aStream, aWriteDataFunc};
    const_cast<nsHashtable*>(this)->Enumerate(WriteEntry, (void*) &args);
    return args.mRetVal;
}

////////////////////////////////////////////////////////////////////////////////

nsISupportsKey::nsISupportsKey(nsIObjectInputStream* aStream, nsresult *aResult)
    : mKey(nullptr)
{
    bool nonnull;
    nsresult rv = aStream->ReadBoolean(&nonnull);
    if (NS_SUCCEEDED(rv) && nonnull)
        rv = aStream->ReadObject(true, &mKey);
    *aResult = rv;
}

nsresult
nsISupportsKey::Write(nsIObjectOutputStream* aStream) const
{
    bool nonnull = (mKey != nullptr);
    nsresult rv = aStream->WriteBoolean(nonnull);
    if (NS_SUCCEEDED(rv) && nonnull)
        rv = aStream->WriteObject(mKey, true);
    return rv;
}

// Copy Constructor
// We need to free mStr if the object is passed with mOwnership as OWN. As the
// destructor here is freeing mStr in that case, mStr is NOT getting leaked here.

nsCStringKey::nsCStringKey(const nsCStringKey& aKey)
    : mStr(aKey.mStr), mStrLen(aKey.mStrLen), mOwnership(aKey.mOwnership)
{
    if (mOwnership != NEVER_OWN) {
      uint32_t len = mStrLen * sizeof(char);
      char* str = reinterpret_cast<char*>(nsMemory::Alloc(len + sizeof(char)));
      if (!str) {
        // Pray we don't dangle!
        mOwnership = NEVER_OWN;
      } else {
        // Use memcpy in case there are embedded NULs.
        memcpy(str, mStr, len);
        str[mStrLen] = '\0';
        mStr = str;
        mOwnership = OWN;
      }
    }
#ifdef DEBUG
    mKeyType = CStringKey;
#endif
    MOZ_COUNT_CTOR(nsCStringKey);
}

nsCStringKey::nsCStringKey(const nsAFlatCString& str)
    : mStr(const_cast<char*>(str.get())),
      mStrLen(str.Length()),
      mOwnership(OWN_CLONE)
{
    NS_ASSERTION(mStr, "null string key");
#ifdef DEBUG
    mKeyType = CStringKey;
#endif
    MOZ_COUNT_CTOR(nsCStringKey);
}

nsCStringKey::nsCStringKey(const nsACString& str)
    : mStr(ToNewCString(str)),
      mStrLen(str.Length()),
      mOwnership(OWN)
{
    NS_ASSERTION(mStr, "null string key");
#ifdef DEBUG
    mKeyType = CStringKey;
#endif
    MOZ_COUNT_CTOR(nsCStringKey);
}

nsCStringKey::nsCStringKey(const char* str, int32_t strLen, Ownership own)
    : mStr((char*)str), mStrLen(strLen), mOwnership(own)
{
    NS_ASSERTION(mStr, "null string key");
    if (mStrLen == uint32_t(-1))
        mStrLen = strlen(str);
#ifdef DEBUG
    mKeyType = CStringKey;
#endif
    MOZ_COUNT_CTOR(nsCStringKey);
}

nsCStringKey::~nsCStringKey(void)
{
    if (mOwnership == OWN)
        nsMemory::Free(mStr);
    MOZ_COUNT_DTOR(nsCStringKey);
}

uint32_t
nsCStringKey::HashCode(void) const
{
    return HashString(mStr, mStrLen);
}

bool
nsCStringKey::Equals(const nsHashKey* aKey) const
{
    NS_ASSERTION(aKey->GetKeyType() == CStringKey, "mismatched key types");
    nsCStringKey* other = (nsCStringKey*)aKey;
    NS_ASSERTION(mStrLen != uint32_t(-1), "never called HashCode");
    NS_ASSERTION(other->mStrLen != uint32_t(-1), "never called HashCode");
    if (mStrLen != other->mStrLen)
        return false;
    return memcmp(mStr, other->mStr, mStrLen * sizeof(char)) == 0;
}

nsHashKey*
nsCStringKey::Clone() const
{
    if (mOwnership == NEVER_OWN)
        return new nsCStringKey(mStr, mStrLen, NEVER_OWN);

    // Since this might hold binary data OR a string, we ensure that the
    // clone string is zero terminated, but don't assume that the source
    // string was so terminated.

    uint32_t len = mStrLen * sizeof(char);
    char* str = (char*)nsMemory::Alloc(len + sizeof(char));
    if (str == nullptr)
        return nullptr;
    memcpy(str, mStr, len);
    str[len] = 0;
    return new nsCStringKey(str, mStrLen, OWN);
}

nsCStringKey::nsCStringKey(nsIObjectInputStream* aStream, nsresult *aResult)
    : mStr(nullptr), mStrLen(0), mOwnership(OWN)
{
    nsAutoCString str;
    nsresult rv = aStream->ReadCString(str);
    mStr = ToNewCString(str);
    if (NS_SUCCEEDED(rv))
        mStrLen = str.Length();
    *aResult = rv;
    MOZ_COUNT_CTOR(nsCStringKey);
}

nsresult
nsCStringKey::Write(nsIObjectOutputStream* aStream) const
{
    return aStream->WriteStringZ(mStr);
}

////////////////////////////////////////////////////////////////////////////////

// Copy Constructor
// We need to free mStr if the object is passed with mOwnership as OWN. As the
// destructor here is freeing mStr in that case, mStr is NOT getting leaked here.

nsStringKey::nsStringKey(const nsStringKey& aKey)
    : mStr(aKey.mStr), mStrLen(aKey.mStrLen), mOwnership(aKey.mOwnership)
{
    if (mOwnership != NEVER_OWN) {
        uint32_t len = mStrLen * sizeof(char16_t);
        char16_t* str = reinterpret_cast<char16_t*>(nsMemory::Alloc(len + sizeof(char16_t)));
        if (!str) {
            // Pray we don't dangle!
            mOwnership = NEVER_OWN;
        } else {
            // Use memcpy in case there are embedded NULs.
            memcpy(str, mStr, len);
            str[mStrLen] = 0;
            mStr = str;
            mOwnership = OWN;
        }
    }
#ifdef DEBUG
    mKeyType = StringKey;
#endif
    MOZ_COUNT_CTOR(nsStringKey);
}

nsStringKey::nsStringKey(const nsAFlatString& str)
    : mStr((char16_t*)str.get()),
      mStrLen(str.Length()),
      mOwnership(OWN_CLONE)
{
    NS_ASSERTION(mStr, "null string key");
#ifdef DEBUG
    mKeyType = StringKey;
#endif
    MOZ_COUNT_CTOR(nsStringKey);
}

nsStringKey::nsStringKey(const nsAString& str)
    : mStr(ToNewUnicode(str)),
      mStrLen(str.Length()),
      mOwnership(OWN)
{
    NS_ASSERTION(mStr, "null string key");
#ifdef DEBUG
    mKeyType = StringKey;
#endif
    MOZ_COUNT_CTOR(nsStringKey);
}

nsStringKey::nsStringKey(const char16_t* str, int32_t strLen, Ownership own)
    : mStr((char16_t*)str), mStrLen(strLen), mOwnership(own)
{
    NS_ASSERTION(mStr, "null string key");
    if (mStrLen == uint32_t(-1))
        mStrLen = NS_strlen(str);
#ifdef DEBUG
    mKeyType = StringKey;
#endif
    MOZ_COUNT_CTOR(nsStringKey);
}

nsStringKey::~nsStringKey(void)
{
    if (mOwnership == OWN)
        nsMemory::Free(mStr);
    MOZ_COUNT_DTOR(nsStringKey);
}

uint32_t
nsStringKey::HashCode(void) const
{
    return HashString(mStr, mStrLen);
}

bool
nsStringKey::Equals(const nsHashKey* aKey) const
{
    NS_ASSERTION(aKey->GetKeyType() == StringKey, "mismatched key types");
    nsStringKey* other = (nsStringKey*)aKey;
    NS_ASSERTION(mStrLen != uint32_t(-1), "never called HashCode");
    NS_ASSERTION(other->mStrLen != uint32_t(-1), "never called HashCode");
    if (mStrLen != other->mStrLen)
        return false;
    return memcmp(mStr, other->mStr, mStrLen * sizeof(char16_t)) == 0;
}

nsHashKey*
nsStringKey::Clone() const
{
    if (mOwnership == NEVER_OWN)
        return new nsStringKey(mStr, mStrLen, NEVER_OWN);

    uint32_t len = (mStrLen+1) * sizeof(char16_t);
    char16_t* str = (char16_t*)nsMemory::Alloc(len);
    if (str == nullptr)
        return nullptr;
    memcpy(str, mStr, len);
    return new nsStringKey(str, mStrLen, OWN);
}

nsStringKey::nsStringKey(nsIObjectInputStream* aStream, nsresult *aResult)
    : mStr(nullptr), mStrLen(0), mOwnership(OWN)
{
    nsAutoString str;
    nsresult rv = aStream->ReadString(str);
    mStr = ToNewUnicode(str);
    if (NS_SUCCEEDED(rv))
        mStrLen = str.Length();
    *aResult = rv;
    MOZ_COUNT_CTOR(nsStringKey);
}

nsresult
nsStringKey::Write(nsIObjectOutputStream* aStream) const
{
  return aStream->WriteWStringZ(mStr);
}

////////////////////////////////////////////////////////////////////////////////
// nsObjectHashtable: an nsHashtable where the elements are C++ objects to be
// deleted

nsObjectHashtable::nsObjectHashtable(nsHashtableCloneElementFunc cloneElementFun,
                                     void* cloneElementClosure,
                                     nsHashtableEnumFunc destroyElementFun,
                                     void* destroyElementClosure,
                                     uint32_t aSize, bool threadSafe)
    : nsHashtable(aSize, threadSafe),
      mCloneElementFun(cloneElementFun),
      mCloneElementClosure(cloneElementClosure),
      mDestroyElementFun(destroyElementFun),
      mDestroyElementClosure(destroyElementClosure)
{
}

nsObjectHashtable::~nsObjectHashtable()
{
    Reset();
}


PLDHashOperator
nsObjectHashtable::CopyElement(PLDHashTable* table,
                               PLDHashEntryHdr* hdr,
                               uint32_t i, void *arg)
{
    nsObjectHashtable *newHashtable = (nsObjectHashtable *)arg;
    HTEntry *entry = static_cast<HTEntry*>(hdr);

    void* newElement =
        newHashtable->mCloneElementFun(entry->key, entry->value,
                                       newHashtable->mCloneElementClosure);
    if (newElement == nullptr)
        return PL_DHASH_STOP;
    newHashtable->Put(entry->key, newElement);
    return PL_DHASH_NEXT;
}

nsHashtable*
nsObjectHashtable::Clone()
{
    if (!mHashtable.ops) return nullptr;

    bool threadSafe = false;
    if (mLock)
        threadSafe = true;
    nsObjectHashtable* newHashTable =
        new nsObjectHashtable(mCloneElementFun, mCloneElementClosure,
                              mDestroyElementFun, mDestroyElementClosure,
                              mHashtable.entryCount, threadSafe);

    PL_DHashTableEnumerate(&mHashtable, CopyElement, newHashTable);
    return newHashTable;
}

void
nsObjectHashtable::Reset()
{
    nsHashtable::Reset(mDestroyElementFun, mDestroyElementClosure);
}

bool
nsObjectHashtable::RemoveAndDelete(nsHashKey *aKey)
{
    void *value = Remove(aKey);
    if (value && mDestroyElementFun)
        return !!(*mDestroyElementFun)(aKey, value, mDestroyElementClosure);
    return false;
}
