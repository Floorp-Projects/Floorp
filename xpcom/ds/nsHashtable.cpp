/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK *****
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
#include "nsCRT.h"

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
    hashEntry->key = nsnull;
    hashEntry->value = nsnull;  // probably not necessary, but for
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
    nsnull,
};


//
// Enumerator callback
//

struct _HashEnumerateArgs {
    nsHashtableEnumFunc fn;
    void* arg;
};

static PLDHashOperator
hashEnumerate(PLDHashTable* table, PLDHashEntryHdr* hdr, PRUint32 i, void *arg)
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

nsHashtable::nsHashtable(PRUint32 aInitSize, bool threadSafe)
  : mLock(NULL), mEnumerating(false)
{
    MOZ_COUNT_CTOR(nsHashtable);

    bool result = PL_DHashTableInit(&mHashtable, &hashtableOps, nsnull,
                                      sizeof(HTEntry), aInitSize);
    
    NS_ASSERTION(result, "Hashtable failed to initialize");

    // make sure we detect this later
    if (!result)
        mHashtable.ops = nsnull;
    
    if (threadSafe) {
        mLock = PR_NewLock();
        if (mLock == NULL) {
            // Cannot create a lock. If running on a multiprocessing system
            // we are sure to die.
            PR_ASSERT(mLock != NULL);
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
    void *res =  NULL;

    if (!mHashtable.ops) return nsnull;
    
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
    if (!mHashtable.ops) return nsnull;
    
    if (mLock) PR_Lock(mLock);

    HTEntry* entry =
        static_cast<HTEntry*>
                   (PL_DHashTableOperate(&mHashtable, aKey, PL_DHASH_LOOKUP));
    void *ret = PL_DHASH_ENTRY_IS_BUSY(entry) ? entry->value : nsnull;
    
    if (mLock) PR_Unlock(mLock);

    return ret;
}

void *nsHashtable::Remove(nsHashKey *aKey)
{
    if (!mHashtable.ops) return nsnull;
    
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
        res = nsnull;
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
                   PRUint32 i, void *arg)
{
    nsHashtable *newHashtable = (nsHashtable *)arg;
    HTEntry * entry = static_cast<HTEntry*>(hdr);
    
    newHashtable->Put(entry->key, entry->value);
    return PL_DHASH_NEXT;
}

nsHashtable * nsHashtable::Clone()
{
    if (!mHashtable.ops) return nsnull;
    
    bool threadSafe = (mLock != nsnull);
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
hashEnumerateRemove(PLDHashTable*, PLDHashEntryHdr* hdr, PRUint32 i, void *arg)
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
    Reset(NULL);
}

void nsHashtable::Reset(nsHashtableEnumFunc destroyFunc, void* aClosure)
{
    if (!mHashtable.ops) return;
    
    _HashEnumerateArgs thunk, *thunkp;
    if (!destroyFunc) {
        thunkp = nsnull;
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
  : mLock(nsnull),
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
            PRUint32 count;
            rv = aStream->Read32(&count);

            if (NS_SUCCEEDED(rv)) {
                bool status =
                    PL_DHashTableInit(&mHashtable, &hashtableOps,
                                      nsnull, sizeof(HTEntry), count);
                if (!status) {
                    mHashtable.ops = nsnull;
                    rv = NS_ERROR_OUT_OF_MEMORY;
                } else {
                    for (PRUint32 i = 0; i < count; i++) {
                        nsHashKey* key;
                        void *data;

                        rv = aReadEntryFunc(aStream, &key, &data);
                        if (NS_SUCCEEDED(rv)) {
                            Put(key, data);

                            // XXXbe must we clone key? can't we hand off
                            aFreeEntryFunc(aStream, key, nsnull);
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
    bool threadSafe = (mLock != nsnull);
    nsresult rv = aStream->WriteBoolean(threadSafe);
    if (NS_FAILED(rv)) return rv;

    // Write the entry count first, so we know how many key/value pairs to read.
    PRUint32 count = mHashtable.entryCount;
    rv = aStream->Write32(count);
    if (NS_FAILED(rv)) return rv;

    // Write all key/value pairs in the table.
    WriteEntryArgs args = {aStream, aWriteDataFunc};
    const_cast<nsHashtable*>(this)->Enumerate(WriteEntry, (void*) &args);
    return args.mRetVal;
}

////////////////////////////////////////////////////////////////////////////////

nsISupportsKey::nsISupportsKey(nsIObjectInputStream* aStream, nsresult *aResult)
    : mKey(nsnull)
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
    bool nonnull = (mKey != nsnull);
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
      PRUint32 len = mStrLen * sizeof(char);
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

nsCStringKey::nsCStringKey(const char* str, PRInt32 strLen, Ownership own)
    : mStr((char*)str), mStrLen(strLen), mOwnership(own)
{
    NS_ASSERTION(mStr, "null string key");
    if (mStrLen == PRUint32(-1))
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

PRUint32
nsCStringKey::HashCode(void) const
{
    return nsCRT::HashCode(mStr, (PRUint32*)&mStrLen);
}

bool
nsCStringKey::Equals(const nsHashKey* aKey) const
{
    NS_ASSERTION(aKey->GetKeyType() == CStringKey, "mismatched key types");
    nsCStringKey* other = (nsCStringKey*)aKey;
    NS_ASSERTION(mStrLen != PRUint32(-1), "never called HashCode");
    NS_ASSERTION(other->mStrLen != PRUint32(-1), "never called HashCode");
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

    PRUint32 len = mStrLen * sizeof(char);
    char* str = (char*)nsMemory::Alloc(len + sizeof(char));
    if (str == NULL)
        return NULL;
    memcpy(str, mStr, len);
    str[len] = 0;
    return new nsCStringKey(str, mStrLen, OWN);
}

nsCStringKey::nsCStringKey(nsIObjectInputStream* aStream, nsresult *aResult)
    : mStr(nsnull), mStrLen(0), mOwnership(OWN)
{
    nsCAutoString str;
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
        PRUint32 len = mStrLen * sizeof(PRUnichar);
        PRUnichar* str = reinterpret_cast<PRUnichar*>(nsMemory::Alloc(len + sizeof(PRUnichar)));
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
    : mStr(const_cast<PRUnichar*>(str.get())),
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

bool
nsStringKey::Equals(const nsHashKey* aKey) const
{
    NS_ASSERTION(aKey->GetKeyType() == StringKey, "mismatched key types");
    nsStringKey* other = (nsStringKey*)aKey;
    NS_ASSERTION(mStrLen != PRUint32(-1), "never called HashCode");
    NS_ASSERTION(other->mStrLen != PRUint32(-1), "never called HashCode");
    if (mStrLen != other->mStrLen)
        return false;
    return memcmp(mStr, other->mStr, mStrLen * sizeof(PRUnichar)) == 0;
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
    memcpy(str, mStr, len);
    return new nsStringKey(str, mStrLen, OWN);
}

nsStringKey::nsStringKey(nsIObjectInputStream* aStream, nsresult *aResult)
    : mStr(nsnull), mStrLen(0), mOwnership(OWN)
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
                                     PRUint32 aSize, bool threadSafe)
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
                               PRUint32 i, void *arg)
{
    nsObjectHashtable *newHashtable = (nsObjectHashtable *)arg;
    HTEntry *entry = static_cast<HTEntry*>(hdr);
    
    void* newElement =
        newHashtable->mCloneElementFun(entry->key, entry->value,
                                       newHashtable->mCloneElementClosure);
    if (newElement == nsnull)
        return PL_DHASH_STOP;
    newHashtable->Put(entry->key, newElement);
    return PL_DHASH_NEXT;
}

nsHashtable*
nsObjectHashtable::Clone()
{
    if (!mHashtable.ops) return nsnull;
    
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

////////////////////////////////////////////////////////////////////////////////
// nsSupportsHashtable: an nsHashtable where the elements are nsISupports*

bool
nsSupportsHashtable::ReleaseElement(nsHashKey *aKey, void *aData, void* aClosure)
{
    nsISupports* element = static_cast<nsISupports*>(aData);
    NS_IF_RELEASE(element);
    return true;
}

nsSupportsHashtable::~nsSupportsHashtable()
{
    Enumerate(ReleaseElement, nsnull);
}

// Return true if we overwrote something

bool
nsSupportsHashtable::Put(nsHashKey *aKey, nsISupports* aData, nsISupports **value)
{
    NS_IF_ADDREF(aData);
    void *prev = nsHashtable::Put(aKey, aData);
    nsISupports *old = reinterpret_cast<nsISupports *>(prev);
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
    nsISupports* element = reinterpret_cast<nsISupports*>(data);
    NS_IF_ADDREF(element);
    return element;
}

// Return true if we found something (useful for checks)

bool
nsSupportsHashtable::Remove(nsHashKey *aKey, nsISupports **value)
{
    void* data = nsHashtable::Remove(aKey);
    nsISupports* element = static_cast<nsISupports*>(data);
    if (value)            // caller wants it
        *value = element;
    else                  // caller doesn't care, we do
        NS_IF_RELEASE(element);
    return data != nsnull;
}

PLDHashOperator
nsSupportsHashtable::EnumerateCopy(PLDHashTable*,
                                   PLDHashEntryHdr* hdr,
                                   PRUint32 i, void *arg)
{
    nsHashtable *newHashtable = (nsHashtable *)arg;
    HTEntry* entry = static_cast<HTEntry*>(hdr);
    
    nsISupports* element = static_cast<nsISupports*>(entry->value);
    NS_IF_ADDREF(element);
    newHashtable->Put(entry->key, entry->value);
    return PL_DHASH_NEXT;
}

nsHashtable*
nsSupportsHashtable::Clone()
{
    if (!mHashtable.ops) return nsnull;
    
    bool threadSafe = (mLock != nsnull);
    nsSupportsHashtable* newHashTable =
        new nsSupportsHashtable(mHashtable.entryCount, threadSafe);

    PL_DHashTableEnumerate(&mHashtable, EnumerateCopy, newHashTable);
    return newHashTable;
}

void
nsSupportsHashtable::Reset()
{
    Enumerate(ReleaseElement, nsnull);
    nsHashtable::Reset();
}

////////////////////////////////////////////////////////////////////////////////

