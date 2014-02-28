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

/**
 * nsHashtable is OBSOLETE. Use nsTHashtable or a derivative instead.
 */

#ifndef nsHashtable_h__
#define nsHashtable_h__

#include "pldhash.h"
#include "nscore.h"
#include "nsISupports.h"
#include "nsISupportsImpl.h"
#include "nsStringFwd.h"

class nsIObjectInputStream;
class nsIObjectOutputStream;

struct PRLock;

class nsHashKey {
  protected:
    nsHashKey(void) {
#ifdef DEBUG
        mKeyType = UnknownKey;
#endif
        MOZ_COUNT_CTOR(nsHashKey);
    }


  public:
    // Virtual destructor because all hash keys are |delete|d via a
    // nsHashKey pointer.

    virtual ~nsHashKey(void);
    virtual uint32_t HashCode(void) const = 0;
    virtual bool Equals(const nsHashKey *aKey) const = 0;
    virtual nsHashKey *Clone() const = 0;
    virtual nsresult Write(nsIObjectOutputStream* aStream) const;

#ifdef DEBUG
  public:
    // used for verification that we're casting to the correct key type
    enum nsHashKeyType {
        UnknownKey,
        SupportsKey,
        PRUint32Key,
        VoidKey,
        IDKey,
        CStringKey,
        StringKey
    };
    nsHashKeyType GetKeyType() const { return mKeyType; }
  protected:
    nsHashKeyType mKeyType;
#endif
};

// Enumerator and Read/Write callback functions.

// Return values for nsHashtableEnumFunc
enum {
    kHashEnumerateStop      = false,
    kHashEnumerateNext      = true
};

typedef bool
(* nsHashtableEnumFunc)(nsHashKey *aKey, void *aData, void* aClosure);

typedef nsresult
(* nsHashtableReadEntryFunc)(nsIObjectInputStream *aStream, nsHashKey **aKey,
                             void **aData);

// NB: may be called with null aKey or aData, to free just one of the two.
typedef void
(* nsHashtableFreeEntryFunc)(nsIObjectInputStream *aStream, nsHashKey *aKey,
                             void *aData);

typedef nsresult
(* nsHashtableWriteDataFunc)(nsIObjectOutputStream *aStream, void *aData);

class nsHashtable {
  protected:
    // members
    PRLock*         mLock;
    PLDHashTable    mHashtable;
    bool            mEnumerating;

  public:
    nsHashtable(uint32_t aSize = 16, bool aThreadSafe = false);
    virtual ~nsHashtable();

    int32_t Count(void) { return mHashtable.entryCount; }
    bool Exists(nsHashKey *aKey);
    void *Put(nsHashKey *aKey, void *aData);
    void *Get(nsHashKey *aKey);
    void *Remove(nsHashKey *aKey);
    nsHashtable *Clone();
    void Enumerate(nsHashtableEnumFunc aEnumFunc, void* aClosure = nullptr);
    void Reset();
    void Reset(nsHashtableEnumFunc destroyFunc, void* aClosure = nullptr);

    nsHashtable(nsIObjectInputStream* aStream,
                nsHashtableReadEntryFunc aReadEntryFunc,
                nsHashtableFreeEntryFunc aFreeEntryFunc,
                nsresult *aRetVal);
    nsresult Write(nsIObjectOutputStream* aStream,
                   nsHashtableWriteDataFunc aWriteDataFunc) const;
};

////////////////////////////////////////////////////////////////////////////////
// nsObjectHashtable: an nsHashtable where the elements are C++ objects to be
// deleted

typedef void* (* nsHashtableCloneElementFunc)(nsHashKey *aKey, void *aData, void* aClosure);

class nsObjectHashtable : public nsHashtable {
  public:
    nsObjectHashtable(nsHashtableCloneElementFunc cloneElementFun,
                      void* cloneElementClosure,
                      nsHashtableEnumFunc destroyElementFun,
                      void* destroyElementClosure,
                      uint32_t aSize = 16, bool threadSafe = false);
    ~nsObjectHashtable();

    nsHashtable *Clone();
    void Reset();
    bool RemoveAndDelete(nsHashKey *aKey);

  protected:
    static PLDHashOperator CopyElement(PLDHashTable* table,
                                       PLDHashEntryHdr* hdr,
                                       uint32_t i, void *arg);

    nsHashtableCloneElementFunc mCloneElementFun;
    void*                       mCloneElementClosure;
    nsHashtableEnumFunc         mDestroyElementFun;
    void*                       mDestroyElementClosure;
};

////////////////////////////////////////////////////////////////////////////////
// nsISupportsKey: Where keys are nsISupports objects that get refcounted.

class nsISupportsKey : public nsHashKey {
  protected:
    nsISupports* mKey;

  public:
    nsISupportsKey(const nsISupportsKey& aKey) : mKey(aKey.mKey) {
#ifdef DEBUG
        mKeyType = SupportsKey;
#endif
        NS_IF_ADDREF(mKey);
    }

    nsISupportsKey(nsISupports* key) {
#ifdef DEBUG
        mKeyType = SupportsKey;
#endif
        mKey = key;
        NS_IF_ADDREF(mKey);
    }

    ~nsISupportsKey(void) {
        NS_IF_RELEASE(mKey);
    }

    uint32_t HashCode(void) const {
        return NS_PTR_TO_INT32(mKey);
    }

    bool Equals(const nsHashKey *aKey) const {
        NS_ASSERTION(aKey->GetKeyType() == SupportsKey, "mismatched key types");
        return (mKey == ((nsISupportsKey *) aKey)->mKey);
    }

    nsHashKey *Clone() const {
        return new nsISupportsKey(mKey);
    }

    nsISupportsKey(nsIObjectInputStream* aStream, nsresult *aResult);
    nsresult Write(nsIObjectOutputStream* aStream) const;

    nsISupports* GetValue() { return mKey; }
};


class nsPRUint32Key : public nsHashKey {
protected:
    uint32_t mKey;
public:
    nsPRUint32Key(uint32_t key) {
#ifdef DEBUG
        mKeyType = PRUint32Key;
#endif
        mKey = key;
    }

    uint32_t HashCode(void) const {
        return mKey;
    }

    bool Equals(const nsHashKey *aKey) const {
        return mKey == ((const nsPRUint32Key *) aKey)->mKey;
    }
    nsHashKey *Clone() const {
        return new nsPRUint32Key(mKey);
    }
    uint32_t GetValue() { return mKey; }
};

// for null-terminated c-strings
class nsCStringKey : public nsHashKey {
  public:

    // NB: when serializing, NEVER_OWN keys are deserialized as OWN.
    enum Ownership {
        NEVER_OWN,  // very long lived, even clones don't need to copy it.
        OWN_CLONE,  // as long lived as this key. But clones make a copy.
        OWN         // to be free'd in key dtor. Clones make their own copy.
    };

    nsCStringKey(const nsCStringKey& aStrKey);
    nsCStringKey(const char* str, int32_t strLen = -1, Ownership own = OWN_CLONE);
    nsCStringKey(const nsAFlatCString& str);
    nsCStringKey(const nsACString& str);
    ~nsCStringKey(void);

    uint32_t HashCode(void) const;
    bool Equals(const nsHashKey* aKey) const;
    nsHashKey* Clone() const;
    nsCStringKey(nsIObjectInputStream* aStream, nsresult *aResult);
    nsresult Write(nsIObjectOutputStream* aStream) const;

    // For when the owner of the hashtable wants to peek at the actual
    // string in the key. No copy is made, so be careful.
    const char* GetString() const { return mStr; }
    uint32_t GetStringLength() const { return mStrLen; }

  protected:
    char*       mStr;
    uint32_t    mStrLen;
    Ownership   mOwnership;
};

// for null-terminated unicode strings
class nsStringKey : public nsHashKey {
  public:

    // NB: when serializing, NEVER_OWN keys are deserialized as OWN.
    enum Ownership {
        NEVER_OWN,  // very long lived, even clones don't need to copy it.
        OWN_CLONE,  // as long lived as this key. But clones make a copy.
        OWN         // to be free'd in key dtor. Clones make their own copy.
    };

    nsStringKey(const nsStringKey& aKey);
    nsStringKey(const char16_t* str, int32_t strLen = -1, Ownership own = OWN_CLONE);
    nsStringKey(const nsAFlatString& str);
    nsStringKey(const nsAString& str);
    ~nsStringKey(void);

    uint32_t HashCode(void) const;
    bool Equals(const nsHashKey* aKey) const;
    nsHashKey* Clone() const;
    nsStringKey(nsIObjectInputStream* aStream, nsresult *aResult);
    nsresult Write(nsIObjectOutputStream* aStream) const;

    // For when the owner of the hashtable wants to peek at the actual
    // string in the key. No copy is made, so be careful.
    const char16_t* GetString() const { return mStr; }
    uint32_t GetStringLength() const { return mStrLen; }

  protected:
    char16_t*  mStr;
    uint32_t    mStrLen;
    Ownership   mOwnership;
};

////////////////////////////////////////////////////////////////////////////////

#endif // nsHashtable_h__
