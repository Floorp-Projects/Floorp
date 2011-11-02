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

#include "nsDebug.h"

// Comparator callback function for sorting array values.
typedef int (* nsVoidArrayComparatorFunc)
            (const void* aElement1, const void* aElement2, void* aData);

// Enumerator callback function. Return false to stop
typedef bool (* nsVoidArrayEnumFunc)(void* aElement, void *aData);

/// A basic zero-based array of void*'s that manages its own memory
class NS_COM_GLUE nsVoidArray {
public:
  nsVoidArray();
  nsVoidArray(PRInt32 aCount);  // initial count of aCount elements set to nsnull
  ~nsVoidArray();

  nsVoidArray& operator=(const nsVoidArray& other);

  inline PRInt32 Count() const {
    return mImpl ? mImpl->mCount : 0;
  }
  // If the array grows, the newly created entries will all be null
  bool SetCount(PRInt32 aNewCount);
  // returns the max number that can be held without allocating
  inline PRInt32 GetArraySize() const {
    return mImpl ? (PRInt32(mImpl->mBits) & kArraySizeMask) : 0;
  }

  void* FastElementAt(PRInt32 aIndex) const
  {
    NS_ASSERTION(0 <= aIndex && aIndex < Count(), "nsVoidArray::FastElementAt: index out of range");
    return mImpl->mArray[aIndex];
  }

  // This both asserts and bounds-checks, because (1) we don't want
  // people to write bad code, but (2) we don't want to change it to
  // crashing for backwards compatibility.  See bug 96108.
  void* ElementAt(PRInt32 aIndex) const
  {
    NS_ASSERTION(0 <= aIndex && aIndex < Count(), "nsVoidArray::ElementAt: index out of range");
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

  bool InsertElementAt(void* aElement, PRInt32 aIndex);
  bool InsertElementsAt(const nsVoidArray &other, PRInt32 aIndex);

  bool ReplaceElementAt(void* aElement, PRInt32 aIndex);

  // useful for doing LRU arrays, sorting, etc
  bool MoveElement(PRInt32 aFrom, PRInt32 aTo);

  bool AppendElement(void* aElement) {
    return InsertElementAt(aElement, Count());
  }

  bool AppendElements(nsVoidArray& aElements) {
    return InsertElementsAt(aElements, Count());
  }

  bool RemoveElement(void* aElement);
  bool RemoveElementsAt(PRInt32 aIndex, PRInt32 aCount);
  bool RemoveElementAt(PRInt32 aIndex) { return RemoveElementsAt(aIndex,1); }

  void   Clear();

  bool SizeTo(PRInt32 aMin);
  // Subtly different - Compact() tries to be smart about whether we
  // should reallocate the array; SizeTo() always reallocates.
  void Compact();

  void Sort(nsVoidArrayComparatorFunc aFunc, void* aData);

  bool EnumerateForwards(nsVoidArrayEnumFunc aFunc, void* aData);
  bool EnumerateBackwards(nsVoidArrayEnumFunc aFunc, void* aData);

protected:
  bool GrowArrayBy(PRInt32 aGrowBy);

  struct Impl {
    /**
     * Packed bits. The low 30 bits are the array's size.
     * The two highest bits indicate whether or not we "own" mImpl and
     * must free() it when destroyed, and whether we have a preallocated
     * nsAutoVoidArray buffer.
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
  bool    mIsAuto;
#endif

  enum {
    kArrayOwnerMask = 1 << 31,
    kArrayHasAutoBufferMask = 1 << 30,
    kArraySizeMask = ~(kArrayOwnerMask | kArrayHasAutoBufferMask)
  };
  enum { kAutoBufSize = 8 };


  // bit twiddlers
  void SetArray(Impl *newImpl, PRInt32 aSize, PRInt32 aCount, bool aOwner,
                bool aHasAuto);
  inline bool IsArrayOwner() const {
    return mImpl && (mImpl->mBits & kArrayOwnerMask);
  }
  inline bool HasAutoBuffer() const {
    return mImpl && (mImpl->mBits & kArrayHasAutoBufferMask);
  }

private:
  /// Copy constructors are not allowed
  nsVoidArray(const nsVoidArray& other);
};


// A zero-based array with a bit of automatic internal storage
class NS_COM_GLUE nsAutoVoidArray : public nsVoidArray {
public:
  nsAutoVoidArray();

  void ResetToAutoBuffer()
  {
    SetArray(reinterpret_cast<Impl*>(mAutoBuf), kAutoBufSize, 0, false,
             true);
  }

  nsAutoVoidArray& operator=(const nsVoidArray& other)
  {
    nsVoidArray::operator=(other);
    return *this;
  }
  
protected:
  // The internal storage
  char mAutoBuf[sizeof(Impl) + (kAutoBufSize - 1) * sizeof(void*)];
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

class NS_COM_GLUE nsSmallVoidArray : private nsVoidArray
{
public:
  ~nsSmallVoidArray();

  nsSmallVoidArray& operator=(nsSmallVoidArray& other);
  void* operator[](PRInt32 aIndex) const { return ElementAt(aIndex); }

  PRInt32 GetArraySize() const;

  PRInt32 Count() const;
  void* FastElementAt(PRInt32 aIndex) const;
  // This both asserts and bounds-checks, because (1) we don't want
  // people to write bad code, but (2) we don't want to change it to
  // crashing for backwards compatibility.  See bug 96108.
  void* ElementAt(PRInt32 aIndex) const
  {
    NS_ASSERTION(0 <= aIndex && aIndex < Count(), "nsSmallVoidArray::ElementAt: index out of range");
    return SafeElementAt(aIndex);
  }
  void* SafeElementAt(PRInt32 aIndex) const {
    // let compiler inline; it may be able to remove these checks
    if (PRUint32(aIndex) >= PRUint32(Count())) // handles aIndex < 0 too
    {
      return nsnull;
    }
    return FastElementAt(aIndex);
  }
  PRInt32 IndexOf(void* aPossibleElement) const;
  bool InsertElementAt(void* aElement, PRInt32 aIndex);
  bool InsertElementsAt(const nsVoidArray &other, PRInt32 aIndex);
  bool ReplaceElementAt(void* aElement, PRInt32 aIndex);
  bool MoveElement(PRInt32 aFrom, PRInt32 aTo);
  bool AppendElement(void* aElement);
  bool AppendElements(nsVoidArray& aElements) {
    return InsertElementsAt(aElements, Count());
  }
  bool RemoveElement(void* aElement);
  bool RemoveElementsAt(PRInt32 aIndex, PRInt32 aCount);
  bool RemoveElementAt(PRInt32 aIndex);

  void Clear();
  bool SizeTo(PRInt32 aMin);
  void Compact();
  void Sort(nsVoidArrayComparatorFunc aFunc, void* aData);

  bool EnumerateForwards(nsVoidArrayEnumFunc aFunc, void* aData);
  bool EnumerateBackwards(nsVoidArrayEnumFunc aFunc, void* aData);

private:

  bool HasSingle() const
  {
    return !!(reinterpret_cast<PRWord>(mImpl) & 0x1);
  }
  void* GetSingle() const
  {
    NS_ASSERTION(HasSingle(), "wrong type");
    return reinterpret_cast<void*>
                           (reinterpret_cast<PRWord>(mImpl) & ~0x1);
  }
  void SetSingle(void *aChild)
  {
    NS_ASSERTION(HasSingle() || !mImpl, "overwriting array");
    mImpl = reinterpret_cast<Impl*>
                            (reinterpret_cast<PRWord>(aChild) | 0x1);
  }
  bool IsEmpty() const
  {
    // Note that this isn't the same as Count()==0
    return !mImpl;
  }
  const nsVoidArray* AsArray() const
  {
    NS_ASSERTION(!HasSingle(), "This is a single");
    return this;
  }
  nsVoidArray* AsArray()
  {
    NS_ASSERTION(!HasSingle(), "This is a single");
    return this;
  }
  bool EnsureArray();
};

#endif /* nsVoidArray_h___ */
