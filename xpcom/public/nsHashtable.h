/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsHashtable_h__
#define nsHashtable_h__

#include "plhash.h"
#include "prlock.h"
#include "nsCom.h"

class NS_COM nsHashKey {
protected:
  nsHashKey(void);
public:
  virtual ~nsHashKey(void);
  virtual PRUint32 HashValue(void) const = 0;
  virtual PRBool Equals(const nsHashKey *aKey) const = 0;
  virtual nsHashKey *Clone(void) const = 0;
};

// Enumerator callback function. Use

typedef PRBool (*nsHashtableEnumFunc)(nsHashKey *aKey, void *aData, void* closure);

class NS_COM nsHashtable {
protected:
  // members  
  PLHashTable *hashtable;
  PRLock *mLock;

public:
  nsHashtable(PRUint32 aSize = 256, PRBool threadSafe = PR_FALSE);
  ~nsHashtable();

  PRInt32 Count(void) { return hashtable->nentries; }
  PRBool Exists(nsHashKey *aKey);
  void *Put(nsHashKey *aKey, void *aData);
  void *Get(nsHashKey *aKey);
  void *Remove(nsHashKey *aKey);
  nsHashtable *Clone();
  void Enumerate(nsHashtableEnumFunc aEnumFunc, void* closure = NULL);
  void Reset();
};

////////////////////////////////////////////////////////////////////////////////
// nsISupportsKey: Where keys are nsISupports objects that get refcounted.

#include "nsISupports.h"

class nsISupportsKey : public nsHashKey {
protected:
  nsISupports* mKey;
  
public:
  nsISupportsKey(nsISupports* key) {
    mKey = key;
    NS_IF_ADDREF(mKey);
  }
  
  ~nsISupportsKey(void) {
    NS_IF_RELEASE(mKey);
  }
  
  PRUint32 HashValue(void) const {
    return (PRUint32)mKey;
  }

  PRBool Equals(const nsHashKey *aKey) const {
    return (mKey == ((nsISupportsKey *) aKey)->mKey);
  }

  nsHashKey *Clone(void) const {
    return new nsISupportsKey(mKey);
  }
};

////////////////////////////////////////////////////////////////////////////////
// nsVoidKey: Where keys are void* objects that don't get refcounted.

class nsVoidKey : public nsHashKey {
protected:
  const void* mKey;
  
public:
  nsVoidKey(const void* key) {
    mKey = key;
  }
  
  PRUint32 HashValue(void) const {
    return (PRUint32)mKey;
  }

  PRBool Equals(const nsHashKey *aKey) const {
    return (mKey == ((const nsVoidKey *) aKey)->mKey);
  }

  nsHashKey *Clone(void) const {
    return new nsVoidKey(mKey);
  }
};

////////////////////////////////////////////////////////////////////////////////
// nsIDKey: Where keys are nsIDs (e.g. nsIID, nsCID).

#include "nsID.h"

class nsIDKey : public nsHashKey {
protected:
  nsID mID;
  
public:
  nsIDKey(const nsID &aID) {
    mID = aID;
  }
  
  PRUint32 HashValue(void) const {
    return mID.m0;
  }

  PRBool Equals(const nsHashKey *aKey) const {
    return (mID.Equals(((const nsIDKey *) aKey)->mID));
  }

  nsHashKey *Clone(void) const {
    return new nsIDKey(mID);
  }
};

////////////////////////////////////////////////////////////////////////////////
// nsCStringKey: Where keys are char*'s
// Some uses: hashing ProgIDs, filenames, URIs

#include "plstr.h"

class NS_COM nsCStringKey : public nsHashKey {
protected:
  char  mBuf[64];
  char* mStr;

public:
  nsCStringKey(const char* str);

  ~nsCStringKey(void);

  PRUint32 HashValue(void) const;

  PRBool Equals(const nsHashKey* aKey) const;

  nsHashKey* Clone() const;

};

#endif
