/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
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
#include "prmem.h"
#include "prlog.h"
#include "nsHashtable.h"
#include "nsReadableUtils.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"

////////////////////////////////////////////////////////////////////////////////
// These functions really should be part of nspr, and have internal knowledge
// of its workings. They allow the PLHashTable to be embedded in the structure
// of the nsHashtable, thereby avoiding a secondary allocation. I've added them
// here because we don't have the "right" to add anything to nspr at this point.

#include "prbit.h"

/* Compute the number of buckets in ht */
#define NBUCKETS(ht)    (1 << (PL_HASH_BITS - (ht)->shift))

/* The smallest table has 16 buckets */
#define MINBUCKETSLOG2  4
#define MINBUCKETS      (1 << MINBUCKETSLOG2)

/* Compute the maximum entries given n buckets that we will tolerate, ~90% */
#define OVERLOADED(n)   ((n) - ((n) >> 3))

/* Compute the number of entries below which we shrink the table by half */
#define UNDERLOADED(n)  (((n) > MINBUCKETS) ? ((n) >> 2) : 0)

PR_IMPLEMENT(PRStatus)
PL_HashTableInit(PLHashTable *ht, PRUint32 n, PLHashFunction keyHash,
                 PLHashComparator keyCompare, PLHashComparator valueCompare,
                 const PLHashAllocOps *allocOps, void *allocPriv)
{
    PRSize nb;

    if (n <= MINBUCKETS) {
        n = MINBUCKETSLOG2;
    } else {
        n = PR_CeilingLog2(n);
        if ((PRInt32)n < 0)
            return PR_FAILURE;
    }

#if 0 // if we were in nspr...
    if (!allocOps) allocOps = &defaultHashAllocOps;
#else
    PR_ASSERT(allocOps);
#endif

    memset(ht, 0, sizeof *ht);
    ht->shift = PL_HASH_BITS - n;
    n = 1 << n;
#if defined(WIN16)
    if (n > 16000) {
        (*allocOps->freeTable)(allocPriv, ht);
        return PR_FAILURE;
    }
#endif  /* WIN16 */
    nb = n * sizeof(PLHashEntry *);
    ht->buckets = (PLHashEntry**)((*allocOps->allocTable)(allocPriv, nb));
    if (!ht->buckets) {
        (*allocOps->freeTable)(allocPriv, ht);
        return PR_FAILURE;
    }
    memset(ht->buckets, 0, nb);

    ht->keyHash = keyHash;
    ht->keyCompare = keyCompare;
    ht->valueCompare = valueCompare;
    ht->allocOps = allocOps;
    ht->allocPriv = allocPriv;
    return PR_SUCCESS;
}

PR_IMPLEMENT(void)
PL_HashTableFinalize(PLHashTable *ht)
{
    PRUint32 i, n;
    PLHashEntry *he, *next;
    const PLHashAllocOps *allocOps = ht->allocOps;
    void *allocPriv = ht->allocPriv;

    n = NBUCKETS(ht);
    for (i = 0; i < n; i++) {
        for (he = ht->buckets[i]; he; he = next) {
            next = he->next;
            (*allocOps->freeEntry)(allocPriv, he, HT_FREE_ENTRY);
        }
    }
#ifdef DEBUG
    memset(ht->buckets, 0xDB, n * sizeof ht->buckets[0]);
#endif
    (*allocOps->freeTable)(allocPriv, ht->buckets);
#ifdef DEBUG
    memset(ht, 0xDB, sizeof *ht);
#endif
}

// end of nspr stuff
////////////////////////////////////////////////////////////////////////////////

//
// Key operations
//

static PLHashNumber PR_CALLBACK _hashValue(const void *key)
{
    return ((const nsHashKey *) key)->HashCode();
}

static PRIntn PR_CALLBACK _hashKeyCompare(const void *key1, const void *key2) {
    return ((const nsHashKey *) key1)->Equals((const nsHashKey *) key2);
}

static PRIntn PR_CALLBACK _hashValueCompare(const void *value1,
                                            const void *value2) {
    // We're not going to make any assumptions about value equality
    return 0;
}

//
// Memory callbacks
//

static void * PR_CALLBACK _hashAllocTable(void *pool, PRSize size) {
    return PR_MALLOC(size);
}

static void PR_CALLBACK _hashFreeTable(void *pool, void *item) {
    PR_DELETE(item);
}

static PLHashEntry * PR_CALLBACK _hashAllocEntry(void *pool, const void *key) {
    return PR_NEW(PLHashEntry);
}

static void PR_CALLBACK _hashFreeEntry(void *pool, PLHashEntry *entry,
                                       PRUintn flag)
{
    if (flag == HT_FREE_ENTRY) {
        delete (nsHashKey *) (entry->key);
        PR_DELETE(entry);
    }
}

static PLHashAllocOps _hashAllocOps = {
    _hashAllocTable, _hashFreeTable,
    _hashAllocEntry, _hashFreeEntry
};

//
// Enumerator callback
//

struct _HashEnumerateArgs {
    nsHashtableEnumFunc fn;
    void* arg;
};

static PRIntn PR_CALLBACK _hashEnumerate(PLHashEntry *he, PRIntn i, void *arg)
{
    _HashEnumerateArgs* thunk = (_HashEnumerateArgs*)arg;
    return thunk->fn((nsHashKey *) he->key, he->value, thunk->arg)
           ? HT_ENUMERATE_NEXT
           : HT_ENUMERATE_STOP;
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

MOZ_DECL_CTOR_COUNTER(nsHashtable)

nsHashtable::nsHashtable(PRUint32 aInitSize, PRBool threadSafe)
  : mLock(NULL), mEnumerating(PR_FALSE)
{
    MOZ_COUNT_CTOR(nsHashtable);
    PRStatus status = PL_HashTableInit(&mHashtable,
                                       aInitSize,
                                       _hashValue,
                                       _hashKeyCompare,
                                       _hashValueCompare,
                                       &_hashAllocOps,
                                       NULL);
    PR_ASSERT(status == PR_SUCCESS);
    if (threadSafe) {
        mLock = PR_NewLock();
        if (mLock == NULL) {
            // Cannot create a lock. If running on a multiprocessing system
            // we are sure to die.
            PR_ASSERT(mLock != NULL);
        }
    }
}

#ifdef HASHMETER
PRIntn DontEnum(PLHashEntry *he, PRIntn i, void *arg) {
    return HT_ENUMERATE_STOP;
}
#endif

nsHashtable::~nsHashtable() {
#ifdef HASHMETER
    PL_HashTableDump(&mHashtable, DontEnum, stdout);
#endif
    MOZ_COUNT_DTOR(nsHashtable);
    PL_HashTableFinalize(&mHashtable);
    if (mLock) PR_DestroyLock(mLock);
}

PRBool nsHashtable::Exists(nsHashKey *aKey)
{
    PLHashNumber hash = aKey->HashCode();

    if (mLock) PR_Lock(mLock);

    PLHashEntry *const*hep = mEnumerating ?
      PL_HashTableRawLookupConst(&mHashtable, hash, (void *) aKey) :
      PL_HashTableRawLookup(&mHashtable, hash, (void *) aKey);

    if (mLock) PR_Unlock(mLock);

    return *hep != NULL;
}

void *nsHashtable::Put(nsHashKey *aKey, void *aData)
{
    void *res =  NULL;
    PLHashNumber hash = aKey->HashCode();
    PLHashEntry *he;

    if (mLock) PR_Lock(mLock);

    // shouldn't be adding an item during enumeration
    PR_ASSERT(!mEnumerating);
    PLHashEntry **hep = PL_HashTableRawLookup(&mHashtable, hash, (void *) aKey);

    if ((he = *hep) != NULL) {
        res = he->value;
        he->value = aData;
    } else {
        nsHashKey* key = aKey->Clone();
        if (key) {
            PL_HashTableRawAdd(&mHashtable, hep, hash,
                               (void *)key, aData);
        }
        else
            res = NULL;
    }

    if (mLock) PR_Unlock(mLock);

    return res;
}

void *nsHashtable::Get(nsHashKey *aKey)
{
    if (mLock) PR_Lock(mLock);

    void *ret = mEnumerating ?
      PL_HashTableLookupConst(&mHashtable, (void *) aKey) :
      PL_HashTableLookup(&mHashtable, (void *) aKey);
    if (mLock) PR_Unlock(mLock);

    return ret;
}

void *nsHashtable::Remove(nsHashKey *aKey)
{
    PLHashNumber hash = aKey->HashCode();
    PLHashEntry *he;

    if (mLock) PR_Lock(mLock);

    // shouldn't be adding an item during enumeration
    PR_ASSERT(!mEnumerating);
    PLHashEntry **hep = PL_HashTableRawLookup(&mHashtable, hash, (void *) aKey);
    void *res = NULL;

    if ((he = *hep) != NULL) {
        res = he->value;
        PL_HashTableRawRemove(&mHashtable, hep, he);
    }

    if (mLock) PR_Unlock(mLock);

    return res;
}

// XXX This method was called _hashEnumerateCopy, but it didn't copy the element!
// I don't know how this was supposed to work since the elements are neither copied
// nor refcounted.
static PRIntn PR_CALLBACK _hashEnumerateShare(PLHashEntry *he, PRIntn i, void *arg)
{
    nsHashtable *newHashtable = (nsHashtable *)arg;
    newHashtable->Put((nsHashKey *) he->key, he->value);
    return HT_ENUMERATE_NEXT;
}

nsHashtable * nsHashtable::Clone()
{
    PRBool threadSafe = (mLock != nsnull);
    nsHashtable *newHashTable = new nsHashtable(mHashtable.nentries, threadSafe);

    PL_HashTableEnumerateEntries(&mHashtable, _hashEnumerateShare, newHashTable);
    return newHashTable;
}

void nsHashtable::Enumerate(nsHashtableEnumFunc aEnumFunc, void* aClosure)
{
    PRBool wasEnumerating = mEnumerating;
    mEnumerating = PR_TRUE;
    _HashEnumerateArgs thunk;
    thunk.fn = aEnumFunc;
    thunk.arg = aClosure;
    PL_HashTableEnumerateEntries(&mHashtable, _hashEnumerate, &thunk);
    mEnumerating = wasEnumerating;
}

static PRIntn PR_CALLBACK _hashEnumerateRemove(PLHashEntry *he, PRIntn i, void *arg)
{
    _HashEnumerateArgs* thunk = (_HashEnumerateArgs*)arg;
    if (thunk) {
        return thunk->fn((nsHashKey *) he->key, he->value, thunk->arg)
            ? HT_ENUMERATE_REMOVE
            : HT_ENUMERATE_STOP;
    }
    return HT_ENUMERATE_REMOVE;
}

void nsHashtable::Reset() {
    Reset(NULL);
}

void nsHashtable::Reset(nsHashtableEnumFunc destroyFunc, void* aClosure)
{
    _HashEnumerateArgs thunk, *thunkp;
    if (!destroyFunc) {
        thunkp = nsnull;
    } else {
        thunkp = &thunk;
        thunk.fn = destroyFunc;
        thunk.arg = aClosure;
    }
    PL_HashTableEnumerateEntries(&mHashtable, _hashEnumerateRemove, thunkp);
}

// nsISerializable helpers

nsHashtable::nsHashtable(nsIObjectInputStream* aStream,
                         nsHashtableReadEntryFunc aReadEntryFunc,
                         nsHashtableFreeEntryFunc aFreeEntryFunc,
                         nsresult *aRetVal)
  : mLock(nsnull),
    mEnumerating(PR_FALSE)
{
    MOZ_COUNT_CTOR(nsHashtable);

    PRBool threadSafe;
    nsresult rv = aStream->ReadBoolean(&threadSafe);
    if (NS_SUCCEEDED(rv)) {
        if (threadSafe) {
            mLock = PR_NewLock();
            if (!mLock)
                rv = NS_ERROR_OUT_OF_MEMORY;
        }

        if (NS_SUCCEEDED(rv)) {
            PRUint32 count;
            rv = aStream->Read32(&count);

            if (NS_SUCCEEDED(rv)) {
                PRStatus status = PL_HashTableInit(&mHashtable,
                                                   count,
                                                   _hashValue,
                                                   _hashKeyCompare,
                                                   _hashValueCompare,
                                                   &_hashAllocOps,
                                                   NULL);
                if (status != PR_SUCCESS) {
                    rv = NS_ERROR_OUT_OF_MEMORY;
                } else {
                    for (PRUint32 i = 0; i < count; i++) {
                        nsHashKey* key;
                        void *data;

                        rv = aReadEntryFunc(aStream, &key, &data);
                        if (NS_SUCCEEDED(rv)) {
                            if (!Put(key, data)) {
                                rv = NS_ERROR_OUT_OF_MEMORY;
                                aFreeEntryFunc(aStream, key, data);
                            } else {
                                // XXXbe must we clone key? can't we hand off
                                aFreeEntryFunc(aStream, key, nsnull);
                            }
                            if (NS_FAILED(rv))
                                break;
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

PR_STATIC_CALLBACK(PRBool)
WriteEntry(nsHashKey *aKey, void *aData, void* aClosure)
{
    WriteEntryArgs* args = (WriteEntryArgs*) aClosure;
    nsIObjectOutputStream* stream = args->mStream;

    nsresult rv = aKey->Write(stream);
    if (NS_SUCCEEDED(rv))
        rv = args->mWriteDataFunc(stream, aData);

    args->mRetVal = rv;
    return PR_TRUE;
}

nsresult
nsHashtable::Write(nsIObjectOutputStream* aStream,
                   nsHashtableWriteDataFunc aWriteDataFunc) const
{
    PRBool threadSafe = (mLock != nsnull);
    nsresult rv = aStream->WriteBoolean(threadSafe);
    if (NS_FAILED(rv)) return rv;

    // Write the entry count first, so we know how many key/value pairs to read.
    PRUint32 count = mHashtable.nentries;
    rv = aStream->Write32(count);
    if (NS_FAILED(rv)) return rv;

    // Write all key/value pairs in the table.
    WriteEntryArgs args = {aStream, aWriteDataFunc};
    NS_CONST_CAST(nsHashtable*, this)->Enumerate(WriteEntry, (void*) &args);
    return args.mRetVal;
}

////////////////////////////////////////////////////////////////////////////////

nsISupportsKey::nsISupportsKey(nsIObjectInputStream* aStream, nsresult *aResult)
    : mKey(nsnull)
{
    PRBool nonnull;
    nsresult rv = aStream->ReadBoolean(&nonnull);
    if (NS_SUCCEEDED(rv) && nonnull)
        rv = aStream->ReadObject(PR_TRUE, &mKey);
    *aResult = rv;
}

nsresult
nsISupportsKey::Write(nsIObjectOutputStream* aStream) const
{
    PRBool nonnull = (mKey != nsnull);
    nsresult rv = aStream->WriteBoolean(nonnull);
    if (NS_SUCCEEDED(rv) && nonnull)
        rv = aStream->WriteObject(mKey, PR_TRUE);
    return rv;
}

nsIDKey::nsIDKey(nsIObjectInputStream* aStream, nsresult *aResult)
{
    *aResult = aStream->ReadID(&mID);
}

nsresult nsIDKey::Write(nsIObjectOutputStream* aStream) const
{
    return aStream->WriteID(mID);
}

////////////////////////////////////////////////////////////////////////////////

nsCStringKey::nsCStringKey(const nsAFlatCString& str)
    : mStr(NS_CONST_CAST(char*, str.get())),
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

nsCStringKey::nsCStringKey(const char* str, PRInt32 strLen, Ownership own)
    : mStr((char*)str), mStrLen(strLen), mOwnership(own)
{
    NS_ASSERTION(mStr, "null string key");
    if (mStrLen == PRUint32(-1))
        mStrLen = nsCRT::strlen(str);
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

PRUint32
nsCStringKey::HashCode(void) const
{
    return nsCRT::HashCode(mStr, (PRUint32*)&mStrLen);
}

PRBool
nsCStringKey::Equals(const nsHashKey* aKey) const
{
    NS_ASSERTION(aKey->GetKeyType() == CStringKey, "mismatched key types");
    nsCStringKey* other = (nsCStringKey*)aKey;
    NS_ASSERTION(mStrLen != PRUint32(-1), "never called HashCode");
    NS_ASSERTION(other->mStrLen != PRUint32(-1), "never called HashCode");
    if (mStrLen != other->mStrLen)
        return PR_FALSE;
    return nsCRT::memcmp(mStr, other->mStr, mStrLen * sizeof(char)) == 0;
}

nsHashKey*
nsCStringKey::Clone() const
{
    if (mOwnership == NEVER_OWN)
        return new nsCStringKey(mStr, mStrLen, NEVER_OWN);

    // Since this might hold binary data OR a string, we ensure that the
    // clone string is zero terminated, but don't assume that the source
    // string was so terminated.

    PRUint32 len = mStrLen * sizeof(char);
    char* str = (char*)nsMemory::Alloc(len + sizeof(char));
    if (str == NULL)
        return NULL;
    nsCRT::memcpy(str, mStr, len);
    str[len] = 0;
    return new nsCStringKey(str, mStrLen, OWN);
}

nsCStringKey::nsCStringKey(nsIObjectInputStream* aStream, nsresult *aResult)
    : mStr(nsnull), mStrLen(0), mOwnership(OWN)
{
    nsresult rv = aStream->ReadStringZ(&mStr);
    if (NS_SUCCEEDED(rv))
        mStrLen = nsCRT::strlen(mStr);
    *aResult = rv;
    MOZ_COUNT_CTOR(nsCStringKey);
}

nsresult
nsCStringKey::Write(nsIObjectOutputStream* aStream) const
{
    return aStream->WriteStringZ(mStr);
}

////////////////////////////////////////////////////////////////////////////////

nsStringKey::nsStringKey(const nsAFlatString& str)
    : mStr(NS_CONST_CAST(PRUnichar*, str.get())),
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

nsStringKey::nsStringKey(const PRUnichar* str, PRInt32 strLen, Ownership own)
    : mStr((PRUnichar*)str), mStrLen(strLen), mOwnership(own)
{
    NS_ASSERTION(mStr, "null string key");
    if (mStrLen == PRUint32(-1))
        mStrLen = nsCRT::strlen(str);
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

PRUint32
nsStringKey::HashCode(void) const
{
    return nsCRT::HashCode(mStr, (PRUint32*)&mStrLen);
}

PRBool
nsStringKey::Equals(const nsHashKey* aKey) const
{
    NS_ASSERTION(aKey->GetKeyType() == StringKey, "mismatched key types");
    nsStringKey* other = (nsStringKey*)aKey;
    NS_ASSERTION(mStrLen != PRUint32(-1), "never called HashCode");
    NS_ASSERTION(other->mStrLen != PRUint32(-1), "never called HashCode");
    if (mStrLen != other->mStrLen)
        return PR_FALSE;
    return nsCRT::memcmp(mStr, other->mStr, mStrLen * sizeof(PRUnichar)) == 0;
}

nsHashKey*
nsStringKey::Clone() const
{
    if (mOwnership == NEVER_OWN)
        return new nsStringKey(mStr, mStrLen, NEVER_OWN);

    PRUint32 len = (mStrLen+1) * sizeof(PRUnichar);
    PRUnichar* str = (PRUnichar*)nsMemory::Alloc(len);
    if (str == NULL)
        return NULL;
    nsCRT::memcpy(str, mStr, len);
    return new nsStringKey(str, mStrLen, OWN);
}

nsStringKey::nsStringKey(nsIObjectInputStream* aStream, nsresult *aResult)
    : mStr(nsnull), mStrLen(0), mOwnership(OWN)
{
    nsresult rv = aStream->ReadWStringZ(&mStr);
    if (NS_SUCCEEDED(rv))
        mStrLen = nsCRT::strlen(mStr);
    *aResult = rv;
    MOZ_COUNT_CTOR(nsStringKey);
}

nsresult
nsStringKey::Write(nsIObjectOutputStream* aStream) const
{
  return aStream->WriteWStringZ(mStr);
}

////////////////////////////////////////////////////////////////////////////////

nsOpaqueKey::nsOpaqueKey(const char* str, PRUint32 strLen, Ownership own)
    : mBuf((char*)str), mBufLen(strLen), mOwnership(own)
{
    NS_ASSERTION(mBuf, "null buffer");
#ifdef DEBUG
    mKeyType = OpaqueKey;
#endif
    MOZ_COUNT_CTOR(nsOpaqueKey);
}

nsOpaqueKey::~nsOpaqueKey(void)
{
    if (mOwnership == OWN)
        nsMemory::Free(mBuf);
    MOZ_COUNT_DTOR(nsOpaqueKey);
}

PRUint32
nsOpaqueKey::HashCode(void) const
{
    return nsCRT::BufferHashCode(mBuf, mBufLen);
}

PRBool
nsOpaqueKey::Equals(const nsHashKey* aKey) const
{
    NS_ASSERTION(aKey->GetKeyType() == OpaqueKey, "mismatched key types");
    nsOpaqueKey* other = (nsOpaqueKey*)aKey;
    if (mBufLen != other->mBufLen)
        return PR_FALSE;
    return nsCRT::memcmp(mBuf, other->mBuf, mBufLen) == 0;
}

nsHashKey*
nsOpaqueKey::Clone() const
{
    if (mOwnership == NEVER_OWN)
        return new nsOpaqueKey(mBuf, mBufLen, NEVER_OWN);

    // Since this might hold binary data OR a string, we ensure that the
    // clone string is zero terminated, but don't assume that the source
    // string was so terminated.

    PRUint32 len = mBufLen * sizeof(char);
    char* str = (char*)nsMemory::Alloc(len + sizeof(char));
    if (str == NULL)
        return NULL;
    nsCRT::memcpy(str, mBuf, len);
    str[len] = 0;
    return new nsOpaqueKey(str, mBufLen, OWN);
}

nsOpaqueKey::nsOpaqueKey(nsIObjectInputStream* aStream, nsresult *aResult)
    : mBuf(nsnull), mBufLen(0), mOwnership(OWN)
{
    nsresult rv = aStream->Read32(&mBufLen);
    if (NS_SUCCEEDED(rv))
        rv = aStream->ReadBytes(&mBuf, mBufLen);
    *aResult = rv;
    MOZ_COUNT_CTOR(nsOpaqueKey);
}

nsresult
nsOpaqueKey::Write(nsIObjectOutputStream* aStream) const
{
    nsresult rv = aStream->Write32(mBufLen);
    if (NS_SUCCEEDED(rv))
        rv = aStream->WriteBytes(mBuf, mBufLen);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsObjectHashtable: an nsHashtable where the elements are C++ objects to be
// deleted

nsObjectHashtable::nsObjectHashtable(nsHashtableCloneElementFunc cloneElementFun,
                                     void* cloneElementClosure,
                                     nsHashtableEnumFunc destroyElementFun,
                                     void* destroyElementClosure,
                                     PRUint32 aSize, PRBool threadSafe)
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

PRIntn PR_CALLBACK
nsObjectHashtable::CopyElement(PLHashEntry *he, PRIntn i, void *arg)
{
    nsObjectHashtable *newHashtable = (nsObjectHashtable *)arg;
    void* newElement =
        newHashtable->mCloneElementFun((nsHashKey*)he->key, he->value,
                                       newHashtable->mCloneElementClosure);
    if (newElement == nsnull)
        return HT_ENUMERATE_STOP;
    newHashtable->Put((nsHashKey*)he->key, newElement);
    return HT_ENUMERATE_NEXT;
}

nsHashtable*
nsObjectHashtable::Clone()
{
    PRBool threadSafe = PR_FALSE;
    if (mLock)
        threadSafe = PR_TRUE;
    nsObjectHashtable* newHashTable =
        new nsObjectHashtable(mCloneElementFun, mCloneElementClosure,
                              mDestroyElementFun, mDestroyElementClosure,
                              mHashtable.nentries, threadSafe);

    PL_HashTableEnumerateEntries(&mHashtable, CopyElement, newHashTable);
    return newHashTable;
}

void
nsObjectHashtable::Reset()
{
    nsHashtable::Reset(mDestroyElementFun, mDestroyElementClosure);
}

PRBool
nsObjectHashtable::RemoveAndDelete(nsHashKey *aKey)
{
    void *value = Remove(aKey);
    if (value && mDestroyElementFun)
        return (*mDestroyElementFun)(aKey, value, mDestroyElementClosure);
    return PR_FALSE;
}

////////////////////////////////////////////////////////////////////////////////
// nsSupportsHashtable: an nsHashtable where the elements are nsISupports*

PRBool PR_CALLBACK
nsSupportsHashtable::ReleaseElement(nsHashKey *aKey, void *aData, void* aClosure)
{
    nsISupports* element = NS_STATIC_CAST(nsISupports*, aData);
    NS_IF_RELEASE(element);
    return PR_TRUE;
}

nsSupportsHashtable::~nsSupportsHashtable()
{
    Enumerate(ReleaseElement, nsnull);
}

// Return true if we overwrote something

PRBool
nsSupportsHashtable::Put(nsHashKey *aKey, nsISupports* aData, nsISupports **value)
{
    NS_IF_ADDREF(aData);
    void *prev = nsHashtable::Put(aKey, aData);
    nsISupports *old = NS_REINTERPRET_CAST(nsISupports *, prev);
    if (value)  // pass own the ownership to the caller
        *value = old;
    else        // the caller doesn't care, we do
        NS_IF_RELEASE(old);
    return prev != nsnull;
}

nsISupports *
nsSupportsHashtable::Get(nsHashKey *aKey)
{
    void* data = nsHashtable::Get(aKey);
    if (!data)
        return nsnull;
    nsISupports* element = NS_REINTERPRET_CAST(nsISupports*, data);
    NS_IF_ADDREF(element);
    return element;
}

// Return true if we found something (useful for checks)

PRBool
nsSupportsHashtable::Remove(nsHashKey *aKey, nsISupports **value)
{
    void* data = nsHashtable::Remove(aKey);
    nsISupports* element = NS_STATIC_CAST(nsISupports*, data);
    if (value)            // caller wants it
        *value = element;
    else                  // caller doesn't care, we do
        NS_IF_RELEASE(element);
    return data != nsnull;
}

PRIntn PR_CALLBACK
nsSupportsHashtable::EnumerateCopy(PLHashEntry *he, PRIntn i, void *arg)
{
    nsHashtable *newHashtable = (nsHashtable *)arg;
    nsISupports* element = NS_STATIC_CAST(nsISupports*, he->value);
    NS_IF_ADDREF(element);
    newHashtable->Put((nsHashKey*)he->key, he->value);
    return HT_ENUMERATE_NEXT;
}

nsHashtable*
nsSupportsHashtable::Clone()
{
    PRBool threadSafe = (mLock != nsnull);
    nsSupportsHashtable* newHashTable =
        new nsSupportsHashtable(mHashtable.nentries, threadSafe);

    PL_HashTableEnumerateEntries(&mHashtable, EnumerateCopy, newHashTable);
    return newHashTable;
}

void
nsSupportsHashtable::Reset()
{
    Enumerate(ReleaseElement, nsnull);
    nsHashtable::Reset();
}

////////////////////////////////////////////////////////////////////////////////

