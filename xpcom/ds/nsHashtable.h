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

#ifndef nsHashtable_h__
#define nsHashtable_h__

#include "plhash.h"
#include "prlock.h"
#include "nscore.h"
#include "nsCom.h"
#include "nsString.h"

class nsIObjectInputStream;
class nsIObjectOutputStream;

class nsHashtable;
class nsStringKey;

class NS_COM nsHashKey {
  protected:
    nsHashKey(void);

  public:
    virtual ~nsHashKey(void);
    virtual PRUint32 HashCode(void) const = 0;
    virtual PRBool Equals(const nsHashKey *aKey) const = 0;
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
        StringKey,
        OpaqueKey
    };
    nsHashKeyType GetKeyType() const { return mKeyType; }
  protected:
    nsHashKeyType mKeyType;
#endif
};

// Enumerator and Read/Write callback functions.

typedef PRBool
(*PR_CALLBACK nsHashtableEnumFunc)(nsHashKey *aKey, void *aData, void* aClosure);

typedef nsresult
(*PR_CALLBACK nsHashtableReadEntryFunc)(nsIObjectInputStream *aStream,
                                        nsHashKey **aKey,
                                        void **aData);

// NB: may be called with null aKey or aData, to free just one of the two.
typedef void
(*PR_CALLBACK nsHashtableFreeEntryFunc)(nsIObjectInputStream *aStream,
                                        nsHashKey *aKey,
                                        void *aData);

typedef nsresult
(*PR_CALLBACK nsHashtableWriteDataFunc)(nsIObjectOutputStream *aStream,
                                        void *aData);

class NS_COM nsHashtable {
  protected:
    // members  
    PRLock*     mLock;
    PLHashTable mHashtable;
    PRBool      mEnumerating;

  public:
    nsHashtable(PRUint32 aSize = 16, PRBool threadSafe = PR_FALSE);
    virtual ~nsHashtable();

    PRInt32 Count(void) { return mHashtable.nentries; }
    PRBool Exists(nsHashKey *aKey);
    void *Put(nsHashKey *aKey, void *aData);
    void *Get(nsHashKey *aKey);
    void *Remove(nsHashKey *aKey);
    nsHashtable *Clone();
    void Enumerate(nsHashtableEnumFunc aEnumFunc, void* aClosure = NULL);
    void Reset();
    void Reset(nsHashtableEnumFunc destroyFunc, void* aClosure = NULL);

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

typedef void* (*PR_CALLBACK nsHashtableCloneElementFunc)(nsHashKey *aKey, void *aData, void* aClosure);

class NS_COM nsObjectHashtable : public nsHashtable {
  public:
    nsObjectHashtable(nsHashtableCloneElementFunc cloneElementFun,
                      void* cloneElementClosure,
                      nsHashtableEnumFunc destroyElementFun,
                      void* destroyElementClosure,
                      PRUint32 aSize = 16, PRBool threadSafe = PR_FALSE);
    ~nsObjectHashtable();

    nsHashtable *Clone();
    void Reset();
    PRBool RemoveAndDelete(nsHashKey *aKey);

  protected:
    static PRIntn PR_CALLBACK CopyElement(PLHashEntry *he, PRIntn i, void *arg);
    
    nsHashtableCloneElementFunc mCloneElementFun;
    void*                       mCloneElementClosure;
    nsHashtableEnumFunc         mDestroyElementFun;
    void*                       mDestroyElementClosure;
};

////////////////////////////////////////////////////////////////////////////////
// nsSupportsHashtable: an nsHashtable where the elements are nsISupports*

class nsISupports;

class NS_COM nsSupportsHashtable
  : private nsHashtable
{
  public:
    typedef PRBool (* PR_CALLBACK EnumFunc) (nsHashKey *aKey, void *aData, void* aClosure);

    nsSupportsHashtable(PRUint32 aSize = 16, PRBool threadSafe = PR_FALSE)
      : nsHashtable(aSize, threadSafe) {}
    ~nsSupportsHashtable();

    PRInt32 Count(void) {
        return nsHashtable::Count();
    }
    PRBool Exists(nsHashKey *aKey) {
        return nsHashtable::Exists (aKey);
    }
    PRBool Put(nsHashKey *aKey,
               nsISupports *aData,
               nsISupports **value = nsnull);
    nsISupports* Get(nsHashKey *aKey);
    PRBool Remove(nsHashKey *aKey, nsISupports **value = nsnull);
    nsHashtable *Clone();
    void Enumerate(EnumFunc aEnumFunc, void* aClosure = NULL) {
        nsHashtable::Enumerate(aEnumFunc, aClosure);
    }
    void Reset();

  private:
    static PRBool PR_CALLBACK ReleaseElement(nsHashKey *, void *, void *);
    static PRIntn PR_CALLBACK EnumerateCopy(PLHashEntry *, PRIntn, void *);
};

////////////////////////////////////////////////////////////////////////////////
// nsISupportsKey: Where keys are nsISupports objects that get refcounted.

#include "nsISupports.h"

class NS_COM nsISupportsKey : public nsHashKey {
  protected:
    nsISupports* mKey;
    
  public:
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
    
    PRUint32 HashCode(void) const {
        return NS_PTR_TO_INT32(mKey);
    }

    PRBool Equals(const nsHashKey *aKey) const {
        NS_ASSERTION(aKey->GetKeyType() == SupportsKey, "mismatched key types");
        return (mKey == ((nsISupportsKey *) aKey)->mKey);
    }

    nsHashKey *Clone() const {
        return new nsISupportsKey(mKey);
    }

    nsISupportsKey(nsIObjectInputStream* aStream, nsresult *aResult);
    nsresult Write(nsIObjectOutputStream* aStream) const;
};


class nsPRUint32Key : public nsHashKey {
protected:
    PRUint32 mKey;
public:
    nsPRUint32Key(PRUint32 key) {
#ifdef DEBUG
        mKeyType = PRUint32Key;
#endif
        mKey = key;
    }

    PRUint32 HashCode(void) const {
        return mKey;
    }

    PRBool Equals(const nsHashKey *aKey) const {
        return mKey == ((const nsPRUint32Key *) aKey)->mKey;
    }
    nsHashKey *Clone() const {
        return new nsPRUint32Key(mKey);
    }
    PRUint32 GetValue() { return mKey; }
};

////////////////////////////////////////////////////////////////////////////////
// nsVoidKey: Where keys are void* objects that don't get refcounted.

class nsVoidKey : public nsHashKey {
  protected:
    void* mKey;
    
  public:
    nsVoidKey(void* key) {
#ifdef DEBUG
        mKeyType = VoidKey;
#endif
        mKey = key;
    }
    
    PRUint32 HashCode(void) const {
        return NS_PTR_TO_INT32(mKey);
    }

    PRBool Equals(const nsHashKey *aKey) const {
        NS_ASSERTION(aKey->GetKeyType() == VoidKey, "mismatched key types");
        return (mKey == ((const nsVoidKey *) aKey)->mKey);
    }

    nsHashKey *Clone() const {
        return new nsVoidKey(mKey);
    }

    void* GetValue() { return mKey; }
};

////////////////////////////////////////////////////////////////////////////////
// nsIDKey: Where keys are nsIDs (e.g. nsIID, nsCID).

#include "nsID.h"

class NS_COM nsIDKey : public nsHashKey {
  protected:
    nsID mID;
    
  public:
    nsIDKey(const nsID &aID) {
#ifdef DEBUG
        mKeyType = IDKey;
#endif
        mID = aID;
    }
    
    PRUint32 HashCode(void) const {
        return mID.m0;
    }

    PRBool Equals(const nsHashKey *aKey) const {
        NS_ASSERTION(aKey->GetKeyType() == IDKey, "mismatched key types");
        return (mID.Equals(((const nsIDKey *) aKey)->mID));
    }

    nsHashKey *Clone() const {
        return new nsIDKey(mID);
    }

    nsIDKey(nsIObjectInputStream* aStream, nsresult *aResult);
    nsresult Write(nsIObjectOutputStream* aStream) const;
};

////////////////////////////////////////////////////////////////////////////////

#include "nsString.h"

// for null-terminated c-strings
class NS_COM nsCStringKey : public nsHashKey {
  public:

    // NB: when serializing, NEVER_OWN keys are deserialized as OWN.
    enum Ownership {
        NEVER_OWN,  // very long lived, even clones don't need to copy it.
        OWN_CLONE,  // as long lived as this key. But clones make a copy.
        OWN         // to be free'd in key dtor. Clones make their own copy.
    };

    nsCStringKey(const char* str, PRInt32 strLen = -1, Ownership own = OWN_CLONE);
    nsCStringKey(const nsAFlatCString& str);
    nsCStringKey(const nsACString& str);
    ~nsCStringKey(void);

    PRUint32 HashCode(void) const;
    PRBool Equals(const nsHashKey* aKey) const;
    nsHashKey* Clone() const;
    nsCStringKey(nsIObjectInputStream* aStream, nsresult *aResult);
    nsresult Write(nsIObjectOutputStream* aStream) const;

    // For when the owner of the hashtable wants to peek at the actual
    // string in the key. No copy is made, so be careful.
    const char* GetString() const { return mStr; }
    PRUint32 GetStringLength() const { return mStrLen; }

  protected:
    char*       mStr;
    PRUint32    mStrLen;
    Ownership   mOwnership;
};

// for null-terminated unicode strings
class NS_COM nsStringKey : public nsHashKey {
  public:

    // NB: when serializing, NEVER_OWN keys are deserialized as OWN.
    enum Ownership {
        NEVER_OWN,  // very long lived, even clones don't need to copy it.
        OWN_CLONE,  // as long lived as this key. But clones make a copy.
        OWN         // to be free'd in key dtor. Clones make their own copy.
    };

    nsStringKey(const PRUnichar* str, PRInt32 strLen = -1, Ownership own = OWN_CLONE);
    nsStringKey(const nsAFlatString& str);
    nsStringKey(const nsAString& str);
    ~nsStringKey(void);

    PRUint32 HashCode(void) const;
    PRBool Equals(const nsHashKey* aKey) const;
    nsHashKey* Clone() const;
    nsStringKey(nsIObjectInputStream* aStream, nsresult *aResult);
    nsresult Write(nsIObjectOutputStream* aStream) const;

    // For when the owner of the hashtable wants to peek at the actual
    // string in the key. No copy is made, so be careful.
    const PRUnichar* GetString() const { return mStr; }
    PRUint32 GetStringLength() const { return mStrLen; }

  protected:
    PRUnichar*  mStr;
    PRUint32    mStrLen;
    Ownership   mOwnership;
};

// for opaque buffers of data which may contain nulls
class NS_COM nsOpaqueKey : public nsHashKey {
  public:

    // NB: when serializing, NEVER_OWN keys are deserialized as OWN.
    enum Ownership {
        NEVER_OWN,  // very long lived, even clones don't need to copy it.
        OWN_CLONE,  // as long lived as this key. But clones make a copy.
        OWN         // to be free'd in key dtor. Clones make their own copy.
    };

    nsOpaqueKey(const char* buf, PRUint32 bufLen, Ownership own = OWN_CLONE);
    ~nsOpaqueKey(void);

    PRUint32 HashCode(void) const;
    PRBool Equals(const nsHashKey* aKey) const;
    nsHashKey* Clone() const;
    nsOpaqueKey(nsIObjectInputStream* aStream, nsresult *aResult);
    nsresult Write(nsIObjectOutputStream* aStream) const;

    // For when the owner of the hashtable wants to peek at the actual
    // string in the key. No copy is made, so be careful.
    const char* GetBuffer() const { return mBuf; }
    PRUint32 GetBufferLength() const { return mBufLen; }

  protected:
    char*       mBuf;
    PRUint32    mBufLen;
    Ownership   mOwnership;
};

////////////////////////////////////////////////////////////////////////////////

#endif // nsHashtable_h__
