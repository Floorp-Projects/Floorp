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
private:
  // members  
  PLHashTable *hashtable;

public:
  nsHashtable(PRUint32 aSize = 256);
  ~nsHashtable();

  PRInt32 Count(void) { return hashtable->nentries; }
  void *Put(nsHashKey *aKey, void *aData);
  void *Get(nsHashKey *aKey);
  void *Remove(nsHashKey *aKey);
  nsHashtable *Clone();
  void Enumerate(nsHashtableEnumFunc aEnumFunc, void* closure = NULL);
};

#endif
