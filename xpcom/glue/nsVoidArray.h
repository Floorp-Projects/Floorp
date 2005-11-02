/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 *   IBM Corp.
 */
#ifndef nsVoidArray_h___
#define nsVoidArray_h___

#include "nscore.h"
class nsISizeOfHandler;

// Enumerator callback function. Return PR_FALSE to stop
typedef PRBool (* PR_CALLBACK nsVoidArrayEnumFunc)(void* aElement, void *aData);

/// A basic zero-based array of void*'s that manages its own memory
class NS_COM nsVoidArray {
public:
  nsVoidArray();
  nsVoidArray(PRInt32 aCount);  // initial count of aCount elements set to nsnull
  virtual ~nsVoidArray();

  nsVoidArray& operator=(const nsVoidArray& other);

  void  SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const;

  PRInt32 Count() const {
    return mImpl ? mImpl->mCount : 0;
  }

  void* ElementAt(PRInt32 aIndex) const;
  void* operator[](PRInt32 aIndex) const { return ElementAt(aIndex); }

  PRInt32 IndexOf(void* aPossibleElement) const;

  PRBool InsertElementAt(void* aElement, PRInt32 aIndex);

  PRBool ReplaceElementAt(void* aElement, PRInt32 aIndex);

  PRBool AppendElement(void* aElement) {
    return InsertElementAt(aElement, Count());
  }

  PRBool RemoveElement(void* aElement);
  PRBool RemoveElementAt(PRInt32 aIndex);
  void   Clear();

  void Compact();

  PRBool EnumerateForwards(nsVoidArrayEnumFunc aFunc, void* aData);
  PRBool EnumerateBackwards(nsVoidArrayEnumFunc aFunc, void* aData);

protected:
  struct Impl {
    /**
     * Packed bits. The highest 31 bits are the array's size, which must
     * always be 0 mod 2. The lowest bit is a flag that indicates
     * whether or not we "own" mArray, and must free() it when
     * destroyed.
     */
    PRUint32 mBits;

    /**
     * The number of elements in the array
     */
    PRInt32 mCount;

    /**
     * Array data, padded out to the actual size of the array.
     */
    void*   mArray[1];
  };

  Impl* mImpl;

  enum {
    kArrayOwnerMask = 1 << 31,
    kArraySizeMask = ~kArrayOwnerMask
  };


  // bit twiddlers
  PRInt32 GetArraySize() const;
  void SetArraySize(PRInt32 aSize);
  PRBool IsArrayOwner() const;
  void SetArrayOwner(PRBool aOwner);

private:
  /// Copy constructors are not allowed
  nsVoidArray(const nsVoidArray& other);
};


// A zero-based array with a bit of automatic internal storage
class NS_COM nsAutoVoidArray : public nsVoidArray {
public:
  nsAutoVoidArray();

protected:
  // The internal storage. Note that this value must be divisible by
  // two because we use the LSB of mInfo to indicate array ownership.
  enum { kAutoBufSize = 8 };
  char mAutoBuf[sizeof(Impl) + (kAutoBufSize - 1) * sizeof(void*)];
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
    return nsVoidArray::Count();
  }

  void StringAt(PRInt32 aIndex, nsString& aString) const;
  nsString* StringAt(PRInt32 aIndex) const;
  nsString* operator[](PRInt32 aIndex) const { return StringAt(aIndex); }

  PRInt32 IndexOf(const nsString& aPossibleString) const;
  PRInt32 IndexOfIgnoreCase(const nsString& aPossibleString) const;

  PRBool InsertStringAt(const nsString& aString, PRInt32 aIndex);

  PRBool ReplaceStringAt(const nsString& aString, PRInt32 aIndex);

  PRBool AppendString(const nsString& aString) {
    return InsertStringAt(aString, Count());
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


class nsCString;

typedef PRBool (*nsCStringArrayEnumFunc)(nsCString& aElement, void *aData);

class NS_COM nsCStringArray: protected nsVoidArray
{
public:
  nsCStringArray(void);
  virtual ~nsCStringArray(void);

  nsCStringArray& operator=(const nsCStringArray& other);

  void  SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const;

  PRInt32 Count(void) const {
    return nsVoidArray::Count();
  }

  void CStringAt(PRInt32 aIndex, nsCString& aCString) const;
  nsCString* CStringAt(PRInt32 aIndex) const;
  nsCString* operator[](PRInt32 aIndex) const { return CStringAt(aIndex); }

  PRInt32 IndexOf(const nsCString& aPossibleString) const;
  PRInt32 IndexOfIgnoreCase(const nsCString& aPossibleString) const;

  PRBool InsertCStringAt(const nsCString& aCString, PRInt32 aIndex);

  PRBool ReplaceCStringAt(const nsCString& aCString, PRInt32 aIndex);

  PRBool AppendCString(const nsCString& aCString) {
    return InsertCStringAt(aCString, Count());
  }

  PRBool RemoveCString(const nsCString& aCString);
  PRBool RemoveCStringIgnoreCase(const nsCString& aCString);
  PRBool RemoveCStringAt(PRInt32 aIndex);
  void   Clear(void);

  void Compact(void) {
    nsVoidArray::Compact();
  }

  PRBool EnumerateForwards(nsCStringArrayEnumFunc aFunc, void* aData);
  PRBool EnumerateBackwards(nsCStringArrayEnumFunc aFunc, void* aData);

private:
  /// Copy constructors are not allowed
  nsCStringArray(const nsCStringArray& other);
};


#endif /* nsVoidArray_h___ */
