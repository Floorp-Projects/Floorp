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
 */

#include "prmem.h"
#include "prlog.h"
#include "nsHashtable.h"

//
// Key operations
//

static PR_CALLBACK PLHashNumber _hashValue(const void *key) 
{
  return ((const nsHashKey *) key)->HashValue();
}

static PR_CALLBACK PRIntn _hashKeyCompare(const void *key1, const void *key2) {
  return ((const nsHashKey *) key1)->Equals((const nsHashKey *) key2);
}

static PR_CALLBACK PRIntn _hashValueCompare(const void *value1,
                                            const void *value2) {
  // We're not going to make any assumptions about value equality
  return 0;
}

//
// Memory callbacks
//

static PR_CALLBACK void *_hashAllocTable(void *pool, PRSize size) {
  return PR_MALLOC(size);
}

static PR_CALLBACK void _hashFreeTable(void *pool, void *item) {
  PR_DELETE(item);
}

static PR_CALLBACK PLHashEntry *_hashAllocEntry(void *pool, const void *key) {
  return PR_NEW(PLHashEntry);
}

static PR_CALLBACK void _hashFreeEntry(void *pool, PLHashEntry *entry, 
                                       PRUintn flag) {
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

static PR_CALLBACK PRIntn _hashEnumerate(PLHashEntry *he, PRIntn i, void *arg)
{
  _HashEnumerateArgs* thunk = (_HashEnumerateArgs*)arg;
  return thunk->fn((nsHashKey *) he->key, he->value, thunk->arg)
    ? HT_ENUMERATE_NEXT
    : HT_ENUMERATE_STOP;
}

//
// HashKey 
//
nsHashKey::nsHashKey(void)
{
}

nsHashKey::~nsHashKey(void)
{
}

nsHashtable::nsHashtable(PRUint32 aInitSize, PRBool threadSafe)
  : mLock(NULL)
{
  hashtable = PL_NewHashTable(aInitSize,
                              _hashValue,
                              _hashKeyCompare,
                              _hashValueCompare,
                              &_hashAllocOps,
                              NULL);
  if (threadSafe == PR_TRUE)
  {
    mLock = PR_NewLock();
    if (mLock == NULL)
    {
      // Cannot create a lock. If running on a multiprocessing system
      // we are sure to die.
      PR_ASSERT(mLock != NULL);
    }
  }
}

nsHashtable::~nsHashtable() {
  PL_HashTableDestroy(hashtable);
  if (mLock) PR_DestroyLock(mLock);
}

PRBool nsHashtable::Exists(nsHashKey *aKey)
{
  PLHashNumber hash = aKey->HashValue();

  if (mLock) PR_Lock(mLock);

  PLHashEntry **hep = PL_HashTableRawLookup(hashtable, hash, (void *) aKey);

  if (mLock) PR_Unlock(mLock);

  return *hep != NULL;
}

void *nsHashtable::Put(nsHashKey *aKey, void *aData) {
  void *res =  NULL;
  PLHashNumber hash = aKey->HashValue();
  PLHashEntry *he;

  if (mLock) PR_Lock(mLock);

  PLHashEntry **hep = PL_HashTableRawLookup(hashtable, hash, (void *) aKey);

  if ((he = *hep) != NULL) {
    res = he->value;
    he->value = aData;
  } else {
    PL_HashTableRawAdd(hashtable, hep, hash,
                       (void *) aKey->Clone(), aData);
  }

  if (mLock) PR_Unlock(mLock);

  return res;
}

void *nsHashtable::Get(nsHashKey *aKey) {

  if (mLock) PR_Lock(mLock);

  void *ret = PL_HashTableLookup(hashtable, (void *) aKey);

  if (mLock) PR_Unlock(mLock);

  return ret;
}

void *nsHashtable::Remove(nsHashKey *aKey) {
  PLHashNumber hash = aKey->HashValue();
  PLHashEntry *he;

  if (mLock) PR_Lock(mLock);

  PLHashEntry **hep = PL_HashTableRawLookup(hashtable, hash, (void *) aKey);
  void *res = NULL;
  
  if ((he = *hep) != NULL) {
    res = he->value;
    PL_HashTableRawRemove(hashtable, hep, he);
  }

  if (mLock) PR_Unlock(mLock);

  return res;
}

// XXX This method was called _hashEnumerateCopy, but it didn't copy the element!
// I don't know how this was supposed to work since the elements are neither copied
// nor refcounted.
static PR_CALLBACK PRIntn _hashEnumerateShare(PLHashEntry *he, PRIntn i, void *arg)
{
  nsHashtable *newHashtable = (nsHashtable *)arg;
  newHashtable->Put((nsHashKey *) he->key, he->value);
  return HT_ENUMERATE_NEXT;
}

nsHashtable * nsHashtable::Clone() {
  PRBool threadSafe = PR_FALSE;
  if (mLock)
    threadSafe = PR_TRUE;
  nsHashtable *newHashTable = new nsHashtable(hashtable->nentries, threadSafe);

  PL_HashTableEnumerateEntries(hashtable, _hashEnumerateShare, newHashTable);
  return newHashTable;
}

void nsHashtable::Enumerate(nsHashtableEnumFunc aEnumFunc, void* closure) {
  _HashEnumerateArgs thunk;
  thunk.fn = aEnumFunc;
  thunk.arg = closure;
  PL_HashTableEnumerateEntries(hashtable, _hashEnumerate, &thunk);
}

static PR_CALLBACK PRIntn _hashEnumerateRemove(PLHashEntry *he, PRIntn i, void *arg)
{
  _HashEnumerateArgs* thunk = (_HashEnumerateArgs*)arg;
  if (thunk)
      return thunk->fn((nsHashKey *) he->key, he->value, thunk->arg)
          ? HT_ENUMERATE_REMOVE
          : HT_ENUMERATE_STOP;
  else
      return HT_ENUMERATE_REMOVE;
}

void nsHashtable::Reset() {
  Reset(NULL);
}

void nsHashtable::Reset(nsHashtableEnumFunc destroyFunc, void* closure)
{
  if (destroyFunc != NULL)
  {
      _HashEnumerateArgs thunk;
      thunk.fn = destroyFunc;
      thunk.arg = closure;
      PL_HashTableEnumerateEntries(hashtable, _hashEnumerateRemove, &thunk);
  }
  else
      PL_HashTableEnumerateEntries(hashtable, _hashEnumerateRemove, NULL);
}

////////////////////////////////////////////////////////////////////////////////

nsStringKey::nsStringKey(const char* str)
{
	mStr.AssignWithConversion(str);
}

nsStringKey::nsStringKey(const PRUnichar* str)
{
	mStr.Assign(str);
}

nsStringKey::nsStringKey(const nsString& str) {
	mStr.Assign(str);
}

nsStringKey::nsStringKey(const nsCString& str) {
	mStr.AssignWithConversion(str);
}

nsStringKey::~nsStringKey(void)
{
}

PRUint32 nsStringKey::HashValue(void) const
{
    return nsStr::HashCode(mStr);
}

PRBool nsStringKey::Equals(const nsHashKey* aKey) const
{
	return ((nsStringKey*)aKey)->mStr == mStr;
}

nsHashKey* nsStringKey::Clone() const
{
	return new nsStringKey(mStr);
}

const nsString& nsStringKey::GetString() const
{
    return mStr;
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

PR_CALLBACK PRIntn 
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
                              hashtable->nentries, threadSafe);

    PL_HashTableEnumerateEntries(hashtable, CopyElement, newHashTable);
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
    {
        return (*mDestroyElementFun)(aKey, value, mDestroyElementClosure);
    }
    else
        return PR_FALSE;
}

////////////////////////////////////////////////////////////////////////////////
// nsSupportsHashtable: an nsHashtable where the elements are nsISupports*

static PR_CALLBACK PRBool
_ReleaseElement(nsHashKey *aKey, void *aData, void* closure)
{
    nsISupports* element = NS_STATIC_CAST(nsISupports*, aData);
    NS_IF_RELEASE(element);
    return PR_TRUE;
}

nsSupportsHashtable::~nsSupportsHashtable()
{
    Enumerate(_ReleaseElement, nsnull);
}

void*
nsSupportsHashtable::Put(nsHashKey *aKey, void *aData)
{
    nsISupports* element = NS_REINTERPRET_CAST(nsISupports*, aData);
    NS_IF_ADDREF(element);
    return nsHashtable::Put(aKey, aData);
}

void*
nsSupportsHashtable::Get(nsHashKey *aKey)
{
    void* data = nsHashtable::Get(aKey);
    if (!data)
        return nsnull;
    nsISupports* element = NS_REINTERPRET_CAST(nsISupports*, data);
    NS_IF_ADDREF(element);
    return data;
}

void*
nsSupportsHashtable::Remove(nsHashKey *aKey)
{
    void* data = nsHashtable::Remove(aKey);
    if (!data)
        return nsnull;
    nsISupports* element = NS_STATIC_CAST(nsISupports*, data);
    NS_IF_RELEASE(element);
    return data;
}

static PR_CALLBACK PRIntn
_hashEnumerateCopy(PLHashEntry *he, PRIntn i, void *arg)
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
    PRBool threadSafe = PR_FALSE;
    if (mLock)
        threadSafe = PR_TRUE;
    nsSupportsHashtable* newHashTable = 
        new nsSupportsHashtable(hashtable->nentries, threadSafe);

    PL_HashTableEnumerateEntries(hashtable, _hashEnumerateCopy, newHashTable);
    return newHashTable;
}

void 
nsSupportsHashtable::Reset()
{
    Enumerate(_ReleaseElement, nsnull);
    nsHashtable::Reset();
}

////////////////////////////////////////////////////////////////////////////////
// nsOpaqueKey: Where keys are opaque byte array blobs
//

// Note opaque keys are not copied by this constructor.  If you want a private
// copy in each hash key, you must create one and pass it in to this function.
nsOpaqueKey::nsOpaqueKey(const char *aOpaqueKey, PRUint32 aKeyLength)
{
    mOpaqueKey = aOpaqueKey;
    mKeyLength = aKeyLength;
}

nsOpaqueKey::~nsOpaqueKey(void)
{
}

PRUint32
nsOpaqueKey::HashValue(void) const
{
    PRUint32 h, i, k;

    h = 0;

    // Same hashing technique as for java.lang.String.hashCode()
    if (mKeyLength <= 15) {
        // A short key; Use a dense sampling to compute the hash code
        for (i = 0; i < mKeyLength; i++)
            h += 37 * mOpaqueKey[i];
    } else {
        // A long key; Use a sparse sampling to compute the hash code
        k = mKeyLength >> 3;
        for (i = 0; i < mKeyLength; i += k)
            h += 39 * mOpaqueKey[i];
    }
    return h;
}

PRBool
nsOpaqueKey::Equals(const nsHashKey* aKey) const
{
    nsOpaqueKey *otherKey = (nsOpaqueKey*)aKey;
    if (mKeyLength !=  otherKey->mKeyLength)
        return PR_FALSE;
	return !(PRBool)memcmp(otherKey->mOpaqueKey, mOpaqueKey, mKeyLength);
}

nsHashKey*
nsOpaqueKey::Clone() const
{
	return new nsOpaqueKey(mOpaqueKey, mKeyLength);
}

PRUint32
nsOpaqueKey::GetKeyLength() const
{
    return mKeyLength;
}

const char*
nsOpaqueKey::GetKey() const
{
    return mOpaqueKey;
}

////////////////////////////////////////////////////////////////////////////////

