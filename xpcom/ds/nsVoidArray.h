/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; c-file-offsets: ((substatement-open . 0))  -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsVoidArray_h___
#define nsVoidArray_h___

//#define DEBUG_VOIDARRAY 1

#include "nscore.h"
#include "nsAString.h"

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

  inline PRInt32 Count() const {
    return mImpl ? mImpl->mCount : 0;
  }
  // returns the max number that can be held without allocating
  inline PRInt32 GetArraySize() const {
    return mImpl ? (PRInt32(mImpl->mBits) & kArraySizeMask) : 0;
  }

  void* FastElementAt(PRInt32 aIndex) const
  {
    NS_ASSERTION(0 <= aIndex && aIndex < Count(), "index out of range");
    return mImpl->mArray[aIndex];
  }

  // This both asserts and bounds-checks, because (1) we don't want
  // people to write bad code, but (2) we don't want to change it to
  // crashing for backwards compatibility.  See bug 96108.
  void* ElementAt(PRInt32 aIndex) const
  {
    NS_ASSERTION(0 <= aIndex && aIndex < Count(), "index out of range");
    return SafeElementAt(aIndex);
  }

  // bounds-checked version
  void* SafeElementAt(PRInt32 aIndex) const
  {
    if (PRUint32(aIndex) >= PRUint32(Count())) // handles aIndex < 0 too
    {
      return nsnull;
    }
    // The bounds check ensures mImpl is non-null.
    return mImpl->mArray[aIndex];
  }

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

  PRBool AppendElements(nsVoidArray& aElements) {
    return InsertElementsAt(aElements, Count());
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
    return mImpl ? (PRBool(mImpl->mBits) & kArrayOwnerMask) : PR_FALSE;
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

  PRInt32 Count(void) const {
    return nsVoidArray::Count();
  }

  void StringAt(PRInt32 aIndex, nsAString& aString) const;
  nsString* StringAt(PRInt32 aIndex) const;
  nsString* operator[](PRInt32 aIndex) const { return StringAt(aIndex); }

  PRInt32 IndexOf(const nsAString& aPossibleString) const;

  PRBool InsertStringAt(const nsAString& aString, PRInt32 aIndex);

  PRBool ReplaceStringAt(const nsAString& aString, PRInt32 aIndex);

  PRBool AppendString(const nsAString& aString) {
    return InsertStringAt(aString, Count());
  }

  PRBool RemoveString(const nsAString& aString);
  PRBool RemoveStringAt(PRInt32 aIndex);
  void   Clear(void);

  void Compact(void) {
    nsVoidArray::Compact();
  }

  void Sort(void);
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

  // Parses a given string using the delimiter passed in. If the array
  // already has some elements, items parsed from string will be appended 
  // to array. For example, array.ParseString("a,b,c", ","); will add strings
  // "a", "b" and "c" to the array. Parsing process has the same tokenizing 
  // behavior as strtok().  
  void ParseString(const char* string, const char* delimiter);

  PRInt32 Count(void) const {
    return nsVoidArray::Count();
  }

  void CStringAt(PRInt32 aIndex, nsACString& aCString) const;
  nsCString* CStringAt(PRInt32 aIndex) const;
  nsCString* operator[](PRInt32 aIndex) const { return CStringAt(aIndex); }

  PRInt32 IndexOf(const nsACString& aPossibleString) const;
  PRInt32 IndexOfIgnoreCase(const nsACString& aPossibleString) const;

  PRBool InsertCStringAt(const nsACString& aCString, PRInt32 aIndex);

  PRBool ReplaceCStringAt(const nsACString& aCString, PRInt32 aIndex);

  PRBool AppendCString(const nsACString& aCString) {
    return InsertCStringAt(aCString, Count());
  }

  PRBool RemoveCString(const nsACString& aCString);
  PRBool RemoveCStringIgnoreCase(const nsACString& aCString);
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


//===================================================================
//  nsSmallVoidArray is not a general-purpose replacement for
//  ns(Auto)VoidArray because there is (some) extra CPU overhead for arrays
//  larger than 1 element, though not a lot.  It is appropriate for
//  space-sensitive uses where sizes of 0 or 1 are moderately common or
//  more, and where we're NOT storing arbitrary integers or arbitrary
//  pointers.

// NOTE: nsSmallVoidArray can ONLY be used for holding items that always
// have the low bit as a 0 - i.e. element & 1 == 0.  This happens to be
// true for allocated and object pointers for all the architectures we run
// on, but conceivably there might be some architectures/compilers for
// which it is NOT true.  We know this works for all existing architectures
// because if it didn't then nsCheapVoidArray would have failed.  Also note
// that we will ASSERT if this assumption is violated in DEBUG builds.

// XXX we're really re-implementing the whole nsVoidArray interface here -
// some form of abstract class would be useful

// I disagree on the abstraction here.  If the point of this class is to be
// as small as possible, and no one will ever derive from it, as I found
// today, there should not be any virtualness to it to avoid the vtable
// ptr overhead.

class NS_COM nsSmallVoidArray
{
public:
  nsSmallVoidArray();
  ~nsSmallVoidArray();

  nsSmallVoidArray& operator=(nsSmallVoidArray& other);
  void* operator[](PRInt32 aIndex) const { return ElementAt(aIndex); }

  PRInt32 GetArraySize() const;

  PRInt32 Count() const;
  void* ElementAt(PRInt32 aIndex) const;
  void* SafeElementAt(PRInt32 aIndex) const {
    // let compiler inline; it may be able to remove these checks
    if (aIndex < 0 || aIndex >= Count())
      return nsnull;
    return ElementAt(aIndex);
  }
  PRInt32 IndexOf(void* aPossibleElement) const;
  PRBool InsertElementAt(void* aElement, PRInt32 aIndex);
  PRBool InsertElementsAt(const nsVoidArray &other, PRInt32 aIndex);
  PRBool ReplaceElementAt(void* aElement, PRInt32 aIndex);
  PRBool MoveElement(PRInt32 aFrom, PRInt32 aTo);
  PRBool AppendElement(void* aElement);
  PRBool AppendElements(nsVoidArray& aElements) {
    return InsertElementsAt(aElements, Count());
  }
  PRBool RemoveElement(void* aElement);
  PRBool RemoveElementsAt(PRInt32 aIndex, PRInt32 aCount);
  PRBool RemoveElementAt(PRInt32 aIndex);

  void Clear();
  PRBool SizeTo(PRInt32 aMin);
  void Compact();
  void Sort(nsVoidArrayComparatorFunc aFunc, void* aData);

  PRBool EnumerateForwards(nsVoidArrayEnumFunc aFunc, void* aData);
  PRBool EnumerateBackwards(nsVoidArrayEnumFunc aFunc, void* aData);

private:
  typedef unsigned long PtrBits;

  PRBool HasSingleChild() const
  {
    return (mChildren && (PtrBits(mChildren) & 0x1));
  }
  PRBool HasVector() const
  {
    return (mChildren && !(PtrBits(mChildren) & 0x1));
  }
  void* GetSingleChild() const
  {
    return (mChildren ? ((void*)(PtrBits(mChildren) & ~0x1)) : nsnull);
  }
  void SetSingleChild(void *aChild);
  nsVoidArray* GetChildVector() const
  {
    return (nsVoidArray*)mChildren;
  }
  nsVoidArray* SwitchToVector();

  // A tagged pointer that's either a pointer to a single child
  // or a pointer to a vector of multiple children. This is a space
  // optimization since a large number of containers have only a 
  // single child.
  void *mChildren;  
};

#endif /* nsVoidArray_h___ */
