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

#ifndef nsSupportsArray_h__
#define nsSupportsArray_h__

#include "nsISupportsArray.h"

static const PRUint32 kAutoArraySize = 4;

class nsSupportsArray : public nsISupportsArray {
public:
  nsSupportsArray(void);
  virtual ~nsSupportsArray(void);

  NS_DECL_ISUPPORTS

  // nsICollection methods:
  NS_IMETHOD_(PRUint32) Count(void) const { return mCount; }
  NS_IMETHOD AppendElement(nsISupports *aElement) {
    return InsertElementAt(aElement, mCount) ? NS_OK : NS_ERROR_FAILURE;
  }
  NS_IMETHOD RemoveElement(nsISupports *aElement) {
    return RemoveElement(aElement, 0) ? NS_OK : NS_ERROR_FAILURE;
  }
  NS_IMETHOD Enumerate(nsIEnumerator* *result);
  NS_IMETHOD Clear(void);

  // nsISupportsArray methods:
  NS_IMETHOD_(nsISupportsArray&) operator=(const nsISupportsArray& aOther);
  NS_IMETHOD_(PRBool) operator==(const nsISupportsArray& aOther) const { return Equals(&aOther); }
  NS_IMETHOD_(PRBool) Equals(const nsISupportsArray* aOther) const;

  NS_IMETHOD_(nsISupports*) ElementAt(PRUint32 aIndex) const;
  NS_IMETHOD_(nsISupports*) operator[](PRUint32 aIndex) const { return ElementAt(aIndex); }

  NS_IMETHOD_(PRInt32) IndexOf(const nsISupports* aPossibleElement, PRUint32 aStartIndex = 0) const;
  NS_IMETHOD_(PRInt32) LastIndexOf(const nsISupports* aPossibleElement) const;

  NS_IMETHOD_(PRBool) InsertElementAt(nsISupports* aElement, PRUint32 aIndex);

  NS_IMETHOD_(PRBool) ReplaceElementAt(nsISupports* aElement, PRUint32 aIndex);

  NS_IMETHOD_(PRBool) RemoveElementAt(PRUint32 aIndex);
  NS_IMETHOD_(PRBool) RemoveElement(const nsISupports* aElement, PRUint32 aStartIndex = 0);
  NS_IMETHOD_(PRBool) RemoveLastElement(const nsISupports* aElement);

  NS_IMETHOD_(PRBool) AppendElements(nsISupportsArray* aElements);
  
  NS_IMETHOD_(void)   Compact(void);

  NS_IMETHOD_(PRBool) EnumerateForwards(nsISupportsArrayEnumFunc aFunc, void* aData) const;
  NS_IMETHOD_(PRBool) EnumerateBackwards(nsISupportsArrayEnumFunc aFunc, void* aData) const;

protected:
  void DeleteArray(void);

  nsISupports** mArray;
  PRUint32 mArraySize;
  PRUint32 mCount;
  nsISupports*  mAutoArray[kAutoArraySize];

private:
  // Copy constructors are not allowed
  nsSupportsArray(const nsISupportsArray& other);
};

#endif // nsSupportsArray_h__
