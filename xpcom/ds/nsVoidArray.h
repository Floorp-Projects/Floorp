/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsVoidArray_h___
#define nsVoidArray_h___

//#define DEBUG_VOIDARRAY 1

#include "nscore.h"
#include "nsAWritableString.h"
#include "nsQuickSort.h"

class nsISizeOfHandler;

// Comparator callback function for sorting array values.
typedef int (* PR_CALLBACK nsVoidArrayComparatorFunc)
            (const void* aElement1, const void* aElement2, void* aData);

// Enumerator callback function. Return PR_FALSE to stop
typedef PRBool (* PR_CALLBACK nsVoidArrayEnumFunc)(void* aElement, void *aData);

/// A basic zero-based array of void*'s that manages its own memory
class NS_COM nsVoidArray {
public:
  nsVoidArray();
  nsVoidArray(PRInt32 aCount);  // initial count of aCount elements set to nsnull
  virtual ~nsVoidArray();

  nsVoidArray& operator=(const nsVoidArray& other);

  virtual void  SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const;

  inline PRInt32 Count() const {
    return mImpl ? mImpl->mCount : 0;
  }
  // returns the max number that can be held without allocating
  inline PRInt32 GetArraySize() const {
    return mImpl ? PRInt32(mImpl->mBits & kArraySizeMask) : 0;
  }

  void* ElementAt(PRInt32 aIndex) const;
  void* operator[](PRInt32 aIndex) const { return ElementAt(aIndex); }

  PRInt32 IndexOf(void* aPossibleElement) const;

  PRBool InsertElementAt(void* aElement, PRInt32 aIndex);
  PRBool InsertElementsAt(const nsVoidArray &other, PRInt32 aIndex);

  PRBool ReplaceElementAt(void* aElement, PRInt32 aIndex);

  // useful for doing LRU arrays, sorting, etc
  PRBool MoveElement(PRInt32 aFrom, PRInt32 aTo);

  PRBool AppendElement(void* aElement) {
    return InsertElementAt(aElement, Count());
  }

  PRBool RemoveElement(void* aElement);
  PRBool RemoveElementsAt(PRInt32 aIndex, PRInt32 aCount);
  PRBool RemoveElementAt(PRInt32 aIndex) { return RemoveElementsAt(aIndex,1); }

  virtual void   Clear();

  virtual PRBool SizeTo(PRInt32 aMin);
  // Subtly different - Compact() tries to be smart about whether we
  // should reallocate the array; SizeTo() just does it.
  virtual void Compact();

  void Sort(nsVoidArrayComparatorFunc aFunc, void* aData);

  PRBool EnumerateForwards(nsVoidArrayEnumFunc aFunc, void* aData);
  PRBool EnumerateBackwards(nsVoidArrayEnumFunc aFunc, void* aData);

protected:
  virtual PRBool GrowArrayBy(PRInt32 aGrowBy);

  struct Impl {
    /**
     * Packed bits. The low 31 bits are the array's size.
     * The highest bit is a flag that indicates 
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
#if DEBUG_VOIDARRAY
  PRInt32 mMaxCount;
  PRInt32 mMaxSize;
  PRBool  mIsAuto;
#endif

  enum {
    kArrayOwnerMask = 1 << 31,
    kArraySizeMask = ~kArrayOwnerMask
  };


  // bit twiddlers
  void SetArray(Impl *newImpl, PRInt32 aSize, PRInt32 aCount, PRBool owner);
  inline PRBool IsArrayOwner() const {
    return mImpl ? PRBool(mImpl->mBits & kArrayOwnerMask) : PR_FALSE;
  }

private:
  /// Copy constructors are not allowed
  nsVoidArray(const nsVoidArray& other);
};


// A zero-based array with a bit of automatic internal storage
class NS_COM nsAutoVoidArray : public nsVoidArray {
public:
  nsAutoVoidArray();
  void Clear();

  virtual PRBool SizeTo(PRInt32 aMin);
  virtual void Compact();
  
protected:
  // The internal storage
  enum { kAutoBufSize = 8 };
  char mAutoBuf[sizeof(Impl) + (kAutoBufSize - 1) * sizeof(void*)];
};


class nsString;

typedef int (* PR_CALLBACK nsStringArrayComparatorFunc)
            (const nsString* aElement1, const nsString* aElement2, void* aData);

typedef PRBool (*nsStringArrayEnumFunc)(nsString& aElement, void *aData);

class NS_COM nsStringArray: protected nsVoidArray
{
public:
  nsStringArray(void);
  nsStringArray(PRInt32 aCount);  // Storage for aCount elements will be pre-allocated
  virtual ~nsStringArray(void);

  nsStringArray& operator=(const nsStringArray& other);

  void  SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const;

  PRInt32 Count(void) const {
    return nsVoidArray::Count();
  }

  void StringAt(PRInt32 aIndex, nsAWritableString& aString) const;
  nsString* StringAt(PRInt32 aIndex) const;
  nsString* operator[](PRInt32 aIndex) const { return StringAt(aIndex); }

  PRInt32 IndexOf(const nsAReadableString& aPossibleString) const;
  PRInt32 IndexOfIgnoreCase(const nsAReadableString& aPossibleString) const;

  PRBool InsertStringAt(const nsAReadableString& aString, PRInt32 aIndex);

  PRBool ReplaceStringAt(const nsAReadableString& aString, PRInt32 aIndex);

  PRBool AppendString(const nsAReadableString& aString) {
    return InsertStringAt(aString, Count());
  }

  PRBool RemoveString(const nsAReadableString& aString);
  PRBool RemoveStringIgnoreCase(const nsAReadableString& aString);
  PRBool RemoveStringAt(PRInt32 aIndex);
  void   Clear(void);

  void Compact(void) {
    nsVoidArray::Compact();
  }

  void Sort(void);
  void SortIgnoreCase(void);
  void Sort(nsStringArrayComparatorFunc aFunc, void* aData);

  PRBool EnumerateForwards(nsStringArrayEnumFunc aFunc, void* aData);
  PRBool EnumerateBackwards(nsStringArrayEnumFunc aFunc, void* aData);

private:
  /// Copy constructors are not allowed
  nsStringArray(const nsStringArray& other);
};


class nsCString;

typedef int (* PR_CALLBACK nsCStringArrayComparatorFunc)
            (const nsCString* aElement1, const nsCString* aElement2, void* aData);

typedef PRBool (*nsCStringArrayEnumFunc)(nsCString& aElement, void *aData);

class NS_COM nsCStringArray: protected nsVoidArray
{
public:
  nsCStringArray(void);
  nsCStringArray(PRInt32 aCount); // Storage for aCount elements will be pre-allocated
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

  void Sort(void);
  void SortIgnoreCase(void);
  void Sort(nsCStringArrayComparatorFunc aFunc, void* aData);

  PRBool EnumerateForwards(nsCStringArrayEnumFunc aFunc, void* aData);
  PRBool EnumerateBackwards(nsCStringArrayEnumFunc aFunc, void* aData);

private:
  /// Copy constructors are not allowed
  nsCStringArray(const nsCStringArray& other);
};


#endif /* nsVoidArray_h___ */
