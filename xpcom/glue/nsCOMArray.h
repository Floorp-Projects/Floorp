/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCOMArray_h__
#define nsCOMArray_h__

#include "mozilla/Attributes.h"
#include "mozilla/MemoryReporting.h"

#include "nsCycleCollectionNoteChild.h"
#include "nsTArray.h"
#include "nsISupports.h"

// See below for the definition of nsCOMArray<T>

// a class that's nsISupports-specific, so that we can contain the
// work of this class in the XPCOM dll
class nsCOMArray_base
{
  friend class nsArrayBase;
protected:
  nsCOMArray_base() {}
  explicit nsCOMArray_base(int32_t aCount) : mArray(aCount) {}
  nsCOMArray_base(const nsCOMArray_base& aOther);
  ~nsCOMArray_base();

  int32_t IndexOf(nsISupports* aObject, uint32_t aStartIndex = 0) const;
  bool Contains(nsISupports* aObject) const
  {
    return IndexOf(aObject) != -1;
  }

  int32_t IndexOfObject(nsISupports* aObject) const;
  bool ContainsObject(nsISupports* aObject) const
  {
    return IndexOfObject(aObject) != -1;
  }

  typedef bool (*nsBaseArrayEnumFunc)(void* aElement, void* aData);

  // enumerate through the array with a callback.
  bool EnumerateForwards(nsBaseArrayEnumFunc aFunc, void* aData) const;

  bool EnumerateBackwards(nsBaseArrayEnumFunc aFunc, void* aData) const;

  typedef int (*nsBaseArrayComparatorFunc)(nsISupports* aElement1,
                                           nsISupports* aElement2,
                                           void* aData);

  struct nsCOMArrayComparatorContext
  {
    nsBaseArrayComparatorFunc mComparatorFunc;
    void* mData;
  };

  static int nsCOMArrayComparator(const void* aElement1, const void* aElement2,
                                  void* aData);
  void Sort(nsBaseArrayComparatorFunc aFunc, void* aData);

  bool InsertObjectAt(nsISupports* aObject, int32_t aIndex);
  void InsertElementAt(uint32_t aIndex, nsISupports* aElement);
  void InsertElementAt(uint32_t aIndex, already_AddRefed<nsISupports> aElement);
  bool InsertObjectsAt(const nsCOMArray_base& aObjects, int32_t aIndex);
  void InsertElementsAt(uint32_t aIndex, const nsCOMArray_base& aElements);
  void InsertElementsAt(uint32_t aIndex, nsISupports* const* aElements,
                        uint32_t aCount);
  bool ReplaceObjectAt(nsISupports* aObject, int32_t aIndex);
  void ReplaceElementAt(uint32_t aIndex, nsISupports* aElement)
  {
    nsISupports* oldElement = mArray[aIndex];
    NS_IF_ADDREF(mArray[aIndex] = aElement);
    NS_IF_RELEASE(oldElement);
  }
  bool AppendObject(nsISupports* aObject)
  {
    return InsertObjectAt(aObject, Count());
  }
  void AppendElement(nsISupports* aElement)
  {
    InsertElementAt(Length(), aElement);
  }
  void AppendElement(already_AddRefed<nsISupports> aElement)
  {
    InsertElementAt(Length(), mozilla::Move(aElement));
  }

  bool AppendObjects(const nsCOMArray_base& aObjects)
  {
    return InsertObjectsAt(aObjects, Count());
  }
  void AppendElements(const nsCOMArray_base& aElements)
  {
    return InsertElementsAt(Length(), aElements);
  }
  void AppendElements(nsISupports* const* aElements, uint32_t aCount)
  {
    return InsertElementsAt(Length(), aElements, aCount);
  }
  bool RemoveObject(nsISupports* aObject);
  nsISupports** Elements() { return mArray.Elements(); }
  void SwapElements(nsCOMArray_base& aOther)
  {
    mArray.SwapElements(aOther.mArray);
  }

  void Adopt(nsISupports** aElements, uint32_t aCount);
  uint32_t Forget(nsISupports*** aElements);
public:
  // elements in the array (including null elements!)
  int32_t Count() const { return mArray.Length(); }
  // nsTArray-compatible version
  uint32_t Length() const { return mArray.Length(); }
  bool IsEmpty() const { return mArray.IsEmpty(); }

  // If the array grows, the newly created entries will all be null;
  // if the array shrinks, the excess entries will all be released.
  bool SetCount(int32_t aNewCount);
  // nsTArray-compatible version
  void TruncateLength(uint32_t aNewLength)
  {
    if (mArray.Length() > aNewLength) {
      RemoveElementsAt(aNewLength, mArray.Length() - aNewLength);
    }
  }

  // remove all elements in the array, and call NS_RELEASE on each one
  void Clear();

  nsISupports* ObjectAt(int32_t aIndex) const { return mArray[aIndex]; }
  // nsTArray-compatible version
  nsISupports* ElementAt(uint32_t aIndex) const { return mArray[aIndex]; }

  nsISupports* SafeObjectAt(int32_t aIndex) const
  {
    return mArray.SafeElementAt(aIndex, nullptr);
  }
  // nsTArray-compatible version
  nsISupports* SafeElementAt(uint32_t aIndex) const
  {
    return mArray.SafeElementAt(aIndex, nullptr);
  }

  nsISupports* operator[](int32_t aIndex) const { return mArray[aIndex]; }

  // remove an element at a specific position, shrinking the array
  // as necessary
  bool RemoveObjectAt(int32_t aIndex);
  // nsTArray-compatible version
  void RemoveElementAt(uint32_t aIndex);

  // remove a range of elements at a specific position, shrinking the array
  // as necessary
  bool RemoveObjectsAt(int32_t aIndex, int32_t aCount);
  // nsTArray-compatible version
  void RemoveElementsAt(uint32_t aIndex, uint32_t aCount);

  void SwapElementsAt(uint32_t aIndex1, uint32_t aIndex2)
  {
    nsISupports* tmp = mArray[aIndex1];
    mArray[aIndex1] = mArray[aIndex2];
    mArray[aIndex2] = tmp;
  }

  // Ensures there is enough space to store a total of aCapacity objects.
  // This method never deletes any objects.
  void SetCapacity(uint32_t aCapacity) { mArray.SetCapacity(aCapacity); }
  uint32_t Capacity() { return mArray.Capacity(); }

  // Measures the size of the array's element storage. If you want to measure
  // anything hanging off the array, you must iterate over the elements and
  // measure them individually; hence the "Shallow" prefix. Note that because
  // each element in an nsCOMArray<T> is actually a T* any such iteration
  // should use a SizeOfIncludingThis() function on each element rather than a
  // SizeOfExcludingThis() function, so that the memory taken by the T itself
  // is included as well as anything it points to.
  size_t ShallowSizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
  {
    return mArray.ShallowSizeOfExcludingThis(aMallocSizeOf);
  }

private:

  // the actual storage
  nsTArray<nsISupports*> mArray;

  // don't implement these, defaults will muck with refcounts!
  nsCOMArray_base& operator=(const nsCOMArray_base& aOther) = delete;
};

inline void
ImplCycleCollectionUnlink(nsCOMArray_base& aField)
{
  aField.Clear();
}

inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            nsCOMArray_base& aField,
                            const char* aName,
                            uint32_t aFlags = 0)
{
  aFlags |= CycleCollectionEdgeNameArrayFlag;
  int32_t length = aField.Count();
  for (int32_t i = 0; i < length; ++i) {
    CycleCollectionNoteChild(aCallback, aField[i], aName, aFlags);
  }
}


// a non-XPCOM, refcounting array of XPCOM objects
// used as a member variable or stack variable - this object is NOT
// refcounted, but the objects that it holds are
//
// most of the read-only accessors like ObjectAt()/etc do NOT refcount
// on the way out. This means that you can do one of two things:
//
// * does an addref, but holds onto a reference
// nsCOMPtr<T> foo = array[i];
//
// * avoids the refcount, but foo might go stale if array[i] is ever
// * modified/removed. Be careful not to NS_RELEASE(foo)!
// T* foo = array[i];
//
// This array will accept null as an argument for any object, and will store
// null in the array. But that also means that methods like ObjectAt() may
// return null when referring to an existing, but null entry in the array.
template<class T>
class nsCOMArray : public nsCOMArray_base
{
public:
  nsCOMArray() {}
  explicit nsCOMArray(int32_t aCount) : nsCOMArray_base(aCount) {}
  explicit nsCOMArray(const nsCOMArray<T>& aOther) : nsCOMArray_base(aOther) {}
  nsCOMArray(nsCOMArray<T>&& aOther) { SwapElements(aOther); }
  ~nsCOMArray() {}

  // We have a move assignment operator, but no copy assignment operator.
  nsCOMArray<T>& operator=(nsCOMArray<T> && aOther)
  {
    SwapElements(aOther);
    return *this;
  }

  // these do NOT refcount on the way out, for speed
  T* ObjectAt(int32_t aIndex) const
  {
    return static_cast<T*>(nsCOMArray_base::ObjectAt(aIndex));
  }
  // nsTArray-compatible version
  T* ElementAt(uint32_t aIndex) const
  {
    return static_cast<T*>(nsCOMArray_base::ElementAt(aIndex));
  }

  // these do NOT refcount on the way out, for speed
  T* SafeObjectAt(int32_t aIndex) const
  {
    return static_cast<T*>(nsCOMArray_base::SafeObjectAt(aIndex));
  }
  // nsTArray-compatible version
  T* SafeElementAt(uint32_t aIndex) const
  {
    return static_cast<T*>(nsCOMArray_base::SafeElementAt(aIndex));
  }

  // indexing operator for syntactic sugar
  T* operator[](int32_t aIndex) const { return ObjectAt(aIndex); }

  // index of the element in question.. does NOT refcount
  // note: this does not check COM object identity. Use
  // IndexOfObject() for that purpose
  int32_t IndexOf(T* aObject, uint32_t aStartIndex = 0) const
  {
    return nsCOMArray_base::IndexOf(aObject, aStartIndex);
  }
  bool Contains(T* aObject) const
  {
    return nsCOMArray_base::Contains(aObject);
  }

  // index of the element in question.. be careful!
  // this is much slower than IndexOf() because it uses
  // QueryInterface to determine actual COM identity of the object
  // if you need to do this frequently then consider enforcing
  // COM object identity before adding/comparing elements
  int32_t IndexOfObject(T* aObject) const
  {
    return nsCOMArray_base::IndexOfObject(aObject);
  }
  bool ContainsObject(nsISupports* aObject) const
  {
    return nsCOMArray_base::ContainsObject(aObject);
  }

  // inserts aObject at aIndex, shifting the objects at aIndex and
  // later to make space
  bool InsertObjectAt(T* aObject, int32_t aIndex)
  {
    return nsCOMArray_base::InsertObjectAt(aObject, aIndex);
  }
  // nsTArray-compatible version
  void InsertElementAt(uint32_t aIndex, T* aElement)
  {
    nsCOMArray_base::InsertElementAt(aIndex, aElement);
  }

  // inserts the objects from aObject at aIndex, shifting the
  // objects at aIndex and later to make space
  bool InsertObjectsAt(const nsCOMArray<T>& aObjects, int32_t aIndex)
  {
    return nsCOMArray_base::InsertObjectsAt(aObjects, aIndex);
  }
  // nsTArray-compatible version
  void InsertElementsAt(uint32_t aIndex, const nsCOMArray<T>& aElements)
  {
    nsCOMArray_base::InsertElementsAt(aIndex, aElements);
  }
  void InsertElementsAt(uint32_t aIndex, T* const* aElements, uint32_t aCount)
  {
    nsCOMArray_base::InsertElementsAt(
      aIndex, reinterpret_cast<nsISupports* const*>(aElements), aCount);
  }

  // replaces an existing element. Warning: if the array grows,
  // the newly created entries will all be null
  bool ReplaceObjectAt(T* aObject, int32_t aIndex)
  {
    return nsCOMArray_base::ReplaceObjectAt(aObject, aIndex);
  }
  // nsTArray-compatible version
  void ReplaceElementAt(uint32_t aIndex, T* aElement)
  {
    nsCOMArray_base::ReplaceElementAt(aIndex, aElement);
  }

  // Enumerator callback function. Return false to stop
  // Here's a more readable form:
  // bool enumerate(T* aElement, void* aData)
  typedef bool (*nsCOMArrayEnumFunc)(T* aElement, void* aData);

  // enumerate through the array with a callback.
  bool EnumerateForwards(nsCOMArrayEnumFunc aFunc, void* aData)
  {
    return nsCOMArray_base::EnumerateForwards(nsBaseArrayEnumFunc(aFunc),
                                              aData);
  }

  bool EnumerateBackwards(nsCOMArrayEnumFunc aFunc, void* aData)
  {
    return nsCOMArray_base::EnumerateBackwards(nsBaseArrayEnumFunc(aFunc),
                                               aData);
  }

  typedef int (*nsCOMArrayComparatorFunc)(T* aElement1, T* aElement2,
                                          void* aData);

  void Sort(nsCOMArrayComparatorFunc aFunc, void* aData)
  {
    nsCOMArray_base::Sort(nsBaseArrayComparatorFunc(aFunc), aData);
  }

  // append an object, growing the array as necessary
  bool AppendObject(T* aObject)
  {
    return nsCOMArray_base::AppendObject(aObject);
  }
  // nsTArray-compatible version
  void AppendElement(T* aElement)
  {
    nsCOMArray_base::AppendElement(aElement);
  }
  void AppendElement(already_AddRefed<T> aElement)
  {
    nsCOMArray_base::AppendElement(mozilla::Move(aElement));
  }

  // append objects, growing the array as necessary
  bool AppendObjects(const nsCOMArray<T>& aObjects)
  {
    return nsCOMArray_base::AppendObjects(aObjects);
  }
  // nsTArray-compatible version
  void AppendElements(const nsCOMArray<T>& aElements)
  {
    return nsCOMArray_base::AppendElements(aElements);
  }
  void AppendElements(T* const* aElements, uint32_t aCount)
  {
    InsertElementsAt(Length(), aElements, aCount);
  }

  // remove the first instance of the given object and shrink the
  // array as necessary
  // Warning: if you pass null here, it will remove the first null element
  bool RemoveObject(T* aObject)
  {
    return nsCOMArray_base::RemoveObject(aObject);
  }
  // nsTArray-compatible version
  bool RemoveElement(T* aElement)
  {
    return nsCOMArray_base::RemoveObject(aElement);
  }

  T** Elements()
  {
    return reinterpret_cast<T**>(nsCOMArray_base::Elements());
  }
  void SwapElements(nsCOMArray<T>& aOther)
  {
    nsCOMArray_base::SwapElements(aOther);
  }

  /**
   * Adopt parameters that resulted from an XPIDL outparam. The aElements
   * parameter will be freed as a result of the call.
   *
   * Example usage:
   * nsCOMArray<nsISomeInterface> array;
   * nsISomeInterface** elements;
   * uint32_t length;
   * ptr->GetSomeArray(&elements, &length);
   * array.Adopt(elements, length);
   */
  void Adopt(T** aElements, uint32_t aSize)
  {
    nsCOMArray_base::Adopt(reinterpret_cast<nsISupports**>(aElements), aSize);
  }

  /**
   * Export the contents of this array to an XPIDL outparam. The array will be
   * Clear()'d after this operation.
   *
   * Example usage:
   * nsCOMArray<nsISomeInterface> array;
   * *length = array.Forget(retval);
   */
  uint32_t Forget(T*** aElements)
  {
    return nsCOMArray_base::Forget(reinterpret_cast<nsISupports***>(aElements));
  }

private:

  // don't implement these!
  nsCOMArray<T>& operator=(const nsCOMArray<T>& aOther) = delete;
};

template<typename T>
inline void
ImplCycleCollectionUnlink(nsCOMArray<T>& aField)
{
  aField.Clear();
}

template<typename E>
inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            nsCOMArray<E>& aField,
                            const char* aName,
                            uint32_t aFlags = 0)
{
  aFlags |= CycleCollectionEdgeNameArrayFlag;
  int32_t length = aField.Count();
  for (int32_t i = 0; i < length; ++i) {
    CycleCollectionNoteChild(aCallback, aField[i], aName, aFlags);
  }
}

#endif
