/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
#ifndef nsVoidArray_h___
#define nsVoidArray_h___

#include "nscore.h"
class nsISizeOfHandler;

// Enumerator callback function. Return PR_FALSE to stop
typedef PRBool (*nsVoidArrayEnumFunc)(void* aElement, void *aData);

/// A basic zero-based array of void*'s that manages its own memory
class NS_COM nsVoidArray {
public:
  nsVoidArray();
  nsVoidArray(PRInt32 aCount);  // initial count of aCount elements set to nsnull
  virtual ~nsVoidArray();

  nsVoidArray& operator=(const nsVoidArray& other);

  void  SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const;

  PRInt32 Count() const {
    return mCount;
  }

  void* ElementAt(PRInt32 aIndex) const;
  void* operator[](PRInt32 aIndex) const { return ElementAt(aIndex); }

  PRInt32 IndexOf(void* aPossibleElement) const;

  PRBool InsertElementAt(void* aElement, PRInt32 aIndex);

  PRBool ReplaceElementAt(void* aElement, PRInt32 aIndex);

  PRBool AppendElement(void* aElement) {
    return InsertElementAt(aElement, mCount);
  }

  PRBool RemoveElement(void* aElement);
  PRBool RemoveElementAt(PRInt32 aIndex);
  void   Clear();

  void Compact();

  PRBool EnumerateForwards(nsVoidArrayEnumFunc aFunc, void* aData);
  PRBool EnumerateBackwards(nsVoidArrayEnumFunc aFunc, void* aData);

protected:
  void** mArray;
  PRInt32 mArraySize;
  PRInt32 mCount;

private:
  /// Copy constructors are not allowed
  nsVoidArray(const nsVoidArray& other);
};


class nsString;

typedef PRBool (*nsStringArrayEnumFunc)(nsString& aElement, void *aData);

class NS_COM nsStringArray: protected nsVoidArray
{
public:
  nsStringArray(void);
  virtual ~nsStringArray(void);

  nsStringArray& operator=(const nsStringArray& other);

  void  SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const;

  PRInt32 Count(void) const {
    return mCount;
  }

  void StringAt(PRInt32 aIndex, nsString& aString) const;
  nsString* StringAt(PRInt32 aIndex) const;
  nsString* operator[](PRInt32 aIndex) const { return StringAt(aIndex); }

  PRInt32 IndexOf(const nsString& aPossibleString) const;
  PRInt32 IndexOfIgnoreCase(const nsString& aPossibleString) const;

  PRBool InsertStringAt(const nsString& aString, PRInt32 aIndex);

  PRBool ReplaceStringAt(const nsString& aString, PRInt32 aIndex);

  PRBool AppendString(const nsString& aString) {
    return InsertStringAt(aString, mCount);
  }

  PRBool RemoveString(const nsString& aString);
  PRBool RemoveStringIgnoreCase(const nsString& aString);
  PRBool RemoveStringAt(PRInt32 aIndex);
  void   Clear(void);

  void Compact(void) {
    nsVoidArray::Compact();
  }

  PRBool EnumerateForwards(nsStringArrayEnumFunc aFunc, void* aData);
  PRBool EnumerateBackwards(nsStringArrayEnumFunc aFunc, void* aData);

private:
  /// Copy constructors are not allowed
  nsStringArray(const nsStringArray& other);
};

#endif /* nsVoidArray_h___ */
