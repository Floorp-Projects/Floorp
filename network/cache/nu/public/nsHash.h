/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

/* A dummy class. Probably will be replaced/removed/recycled. 
 * 
 * -Gagan Saksena 09/15/98
 */
#ifndef nsHash_h__
#define nsHash_h__

#include "plhash.h"

class nsHashKey {
protected:
  nsHashKey(void);
public:
  virtual ~nsHashKey(void);
  virtual PRUint32 HashValue(void) const = 0;
  virtual PRBool Equals(const nsHashKey *aKey) const = 0;
  virtual nsHashKey *Clone(void) const = 0;
};

// Enumerator callback function. Use

typedef PRBool (*nsHashEnumFunc)(nsHashKey *aKey, void *aData);

class nsHash {
private:
  // members  
  PLHashTable *hashtable;

public:
  nsHash(PRUint32 aSize = 256);
  ~nsHash();

  void *Put(nsHashKey *aKey, void *aData);
  void *Get(nsHashKey *aKey);
  void *Remove(nsHashKey *aKey);
  void Enumerate(nsHashEnumFunc aEnumFunc);
};

#endif
