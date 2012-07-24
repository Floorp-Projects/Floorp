/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTArray_h__
#define nsTArray_h__

#include "mozilla/Assertions.h"
#include "mozilla/Util.h"

#include <string.h>

#include "prtypes.h"
#include "nsAlgorithm.h"
#include "nscore.h"
#include "nsQuickSort.h"
#include "nsDebug.h"
#include "nsTraceRefcnt.h"
#include NEW_H

//
// NB: nsTArray assumes that your "T" can be memmove()d.  This is in
// contrast to STL containers, which follow C++
// construction/destruction rules.
//
// Don't use nsTArray if your "T" can't be memmove()d correctly.
//

//
// nsTArray*Allocators must all use the same |free()|, to allow
// swapping between fallible and infallible variants.  (NS_Free() and
// moz_free() end up calling the same underlying free()).
//

#if defined(MOZALLOC_HAVE_XMALLOC)
struct nsTArrayFallibleAllocator
{
  static void* Malloc(size_t size) {
    return moz_malloc(size);
  }

  static void* Realloc(void* ptr, size_t size) {
    return moz_realloc(ptr, size);
  }

  static void Free(void* ptr) {
    moz_free(ptr);
  }
};

struct nsTArrayInfallibleAllocator
{
  static void* Malloc(size_t size) {
    return moz_xmalloc(size);
  }

  static void* Realloc(void* ptr, size_t size) {
    return moz_xrealloc(ptr, size);
  }

  static void Free(void* ptr) {
    moz_free(ptr);
  }
};

#else

#include <stdlib.h>
struct nsTArrayFallibleAllocator
{
  static void* Malloc(size_t size) {
    return malloc(size);
  }

  static void* Realloc(void* ptr, size_t size) {
    return realloc(ptr, size);
  }

  static void Free(void* ptr) {
    free(ptr);
  }
};

#endif

#if defined(MOZALLOC_HAVE_XMALLOC)
struct nsTArrayDefaultAllocator : public nsTArrayInfallibleAllocator { };
#else
struct nsTArrayDefaultAllocator : public nsTArrayFallibleAllocator { };
#endif

// nsTArray_base stores elements into the space allocated beyond
// sizeof(*this).  This is done to minimize the size of the nsTArray
// object when it is empty.
struct NS_COM_GLUE nsTArrayHeader
{
  static nsTArrayHeader sEmptyHdr;

  PRUint32 mLength;
  PRUint32 mCapacity : 31;
  PRUint32 mIsAutoArray : 1;
};

// This class provides a SafeElementAt method to nsTArray<T*> which does
// not take a second default value parameter.
template <class E, class Derived>
struct nsTArray_SafeElementAtHelper
{
  typedef E*       elem_type;
  typedef PRUint32 index_type;

  // No implementation is provided for these two methods, and that is on
  // purpose, since we don't support these functions on non-pointer type
  // instantiations.
  elem_type& SafeElementAt(index_type i);
  const elem_type& SafeElementAt(index_type i) const;
};

template <class E, class Derived>
struct nsTArray_SafeElementAtHelper<E*, Derived>
{
  typedef E*       elem_type;
  typedef PRUint32 index_type;

  elem_type SafeElementAt(index_type i) {
    return static_cast<Derived*> (this)->SafeElementAt(i, nsnull);
  }

  const elem_type SafeElementAt(index_type i) const {
    return static_cast<const Derived*> (this)->SafeElementAt(i, nsnull);
  }
};

// E is the base type that the smart pointer is templated over; the
// smart pointer can act as E*.
template <class E, class Derived>
struct nsTArray_SafeElementAtSmartPtrHelper
{
  typedef E*       elem_type;
  typedef PRUint32 index_type;

  elem_type SafeElementAt(index_type i) {
    return static_cast<Derived*> (this)->SafeElementAt(i, nsnull);
  }

  const elem_type SafeElementAt(index_type i) const {
    return static_cast<const Derived*> (this)->SafeElementAt(i, nsnull);
  }
};

template <class T> class nsCOMPtr;

template <class E, class Derived>
struct nsTArray_SafeElementAtHelper<nsCOMPtr<E>, Derived> :
  public nsTArray_SafeElementAtSmartPtrHelper<E, Derived>
{
};

template <class T> class nsRefPtr;

template <class E, class Derived>
struct nsTArray_SafeElementAtHelper<nsRefPtr<E>, Derived> :
  public nsTArray_SafeElementAtSmartPtrHelper<E, Derived>
{
};

//
// This class serves as a base class for nsTArray.  It shouldn't be used
// directly.  It holds common implementation code that does not depend on the
// element type of the nsTArray.
//
template<class Alloc>
class nsTArray_base
{
  // Allow swapping elements with |nsTArray_base|s created using a
  // different allocator.  This is kosher because all allocators use
  // the same free().
  template<class Allocator>
  friend class nsTArray_base;

protected:
  typedef nsTArrayHeader Header;

public:
  typedef PRUint32 size_type;
  typedef PRUint32 index_type;

  // @return The number of elements in the array.
  size_type Length() const {
    return mHdr->mLength;
  }

  // @return True if the array is empty or false otherwise.
  bool IsEmpty() const {
    return Length() == 0;
  }

  // @return The number of elements that can fit in the array without forcing
  // the array to be re-allocated.  The length of an array is always less
  // than or equal to its capacity.
  size_type Capacity() const {
    return mHdr->mCapacity;
  }

#ifdef DEBUG
  void* DebugGetHeader() const {
    return mHdr;
  }
#endif

protected:
  nsTArray_base();

  ~nsTArray_base();

  // Resize the storage if necessary to achieve the requested capacity.
  // @param capacity     The requested number of array elements.
  // @param elemSize     The size of an array element.
  // @return False if insufficient memory is available; true otherwise.
  bool EnsureCapacity(size_type capacity, size_type elemSize);

  // Resize the storage to the minimum required amount.
  // @param elemSize     The size of an array element.
  // @param elemAlign    The alignment in bytes of an array element.
  void ShrinkCapacity(size_type elemSize, size_t elemAlign);
    
  // This method may be called to resize a "gap" in the array by shifting
  // elements around.  It updates mLength appropriately.  If the resulting
  // array has zero elements, then the array's memory is free'd.
  // @param start        The starting index of the gap.
  // @param oldLen       The current length of the gap.
  // @param newLen       The desired length of the gap.
  // @param elemSize     The size of an array element.
  // @param elemAlign    The alignment in bytes of an array element.
  void ShiftData(index_type start, size_type oldLen, size_type newLen,
                 size_type elemSize, size_t elemAlign);

  // This method increments the length member of the array's header.
  // Note that mHdr may actually be sEmptyHdr in the case where a
  // zero-length array is inserted into our array. But then n should
  // always be 0.
  void IncrementLength(PRUint32 n) {
    MOZ_ASSERT(mHdr != EmptyHdr() || n == 0, "bad data pointer");
    mHdr->mLength += n;
  }

  // This method inserts blank slots into the array.
  // @param index the place to insert the new elements. This must be no
  //              greater than the current length of the array.
  // @param count the number of slots to insert
  // @param elementSize the size of an array element.
  // @param elemAlign the alignment in bytes of an array element.
  bool InsertSlotsAt(index_type index, size_type count,
                       size_type elementSize, size_t elemAlign);

protected:
  template<class Allocator>
  bool SwapArrayElements(nsTArray_base<Allocator>& other,
                           size_type elemSize,
                           size_t elemAlign);

  // This is an RAII class used in SwapArrayElements.
  class IsAutoArrayRestorer {
    public:
      IsAutoArrayRestorer(nsTArray_base<Alloc> &array, size_t elemAlign);
      ~IsAutoArrayRestorer();

    private:
      nsTArray_base<Alloc> &mArray;
      size_t mElemAlign;
      bool mIsAuto;
  };

  // Helper function for SwapArrayElements. Ensures that if the array
  // is an nsAutoTArray that it doesn't use the built-in buffer.
  bool EnsureNotUsingAutoArrayBuffer(size_type elemSize);

  // Returns true if this nsTArray is an nsAutoTArray with a built-in buffer.
  bool IsAutoArray() const {
    return mHdr->mIsAutoArray;
  }

  // Returns a Header for the built-in buffer of this nsAutoTArray.
  Header* GetAutoArrayBuffer(size_t elemAlign) {
    MOZ_ASSERT(IsAutoArray(), "Should be an auto array to call this");
    return GetAutoArrayBufferUnsafe(elemAlign);
  }
  const Header* GetAutoArrayBuffer(size_t elemAlign) const {
    MOZ_ASSERT(IsAutoArray(), "Should be an auto array to call this");
    return GetAutoArrayBufferUnsafe(elemAlign);
  }

  // Returns a Header for the built-in buffer of this nsAutoTArray, but doesn't
  // assert that we are an nsAutoTArray.
  Header* GetAutoArrayBufferUnsafe(size_t elemAlign) {
    return const_cast<Header*>(static_cast<const nsTArray_base<Alloc>*>(this)->
                               GetAutoArrayBufferUnsafe(elemAlign));
  }
  const Header* GetAutoArrayBufferUnsafe(size_t elemAlign) const;

  // Returns true if this is an nsAutoTArray and it currently uses the
  // built-in buffer to store its elements.
  bool UsesAutoArrayBuffer() const;

  // The array's elements (prefixed with a Header).  This pointer is never
  // null.  If the array is empty, then this will point to sEmptyHdr.
  Header *mHdr;

  Header* Hdr() const { 
    return mHdr;
  }

  Header** PtrToHdr() {
    return &mHdr;
  }

  static Header* EmptyHdr() {
    return &Header::sEmptyHdr;
  }
};

//
// This class defines convenience functions for element specific operations.
// Specialize this template if necessary.
//
template<class E>
class nsTArrayElementTraits
{
public:
  // Invoke the default constructor in place.
  static inline void Construct(E *e) {
    // Do NOT call "E()"! That triggers C++ "default initialization"
    // which zeroes out POD ("plain old data") types such as regular
    // ints.  We don't want that because it can be a performance issue
    // and people don't expect it; nsTArray should work like a regular
    // C/C++ array in this respect.
    new (static_cast<void *>(e)) E;
  }
  // Invoke the copy-constructor in place.
  template<class A>
  static inline void Construct(E *e, const A &arg) {
    new (static_cast<void *>(e)) E(arg);
  }
  // Invoke the destructor in place.
  static inline void Destruct(E *e) {
    e->~E();
  }
};

// The default comparator used by nsTArray
template<class A, class B>
class nsDefaultComparator
{
public:
  bool Equals(const A& a, const B& b) const {
    return a == b;
  }
  bool LessThan(const A& a, const B& b) const {
    return a < b;
  }
};

//
// The templatized array class that dynamically resizes its storage as
// elements are added.  This class is designed to behave a bit like
// std::vector, though note that unlike std::vector, nsTArray doesn't
// follow C++ construction/destruction rules.
//
// The template parameter specifies the type of the elements (elem_type), and
// has the following requirements:
//
//   elem_type MUST define a copy-constructor.
//   elem_type MAY define operator< for sorting.
//   elem_type MAY define operator== for searching.
//
// For methods taking a Comparator instance, the Comparator must be a class
// defining the following methods:
//
//   class Comparator {
//     public:
//       /** @return True if the elements are equals; false otherwise. */
//       bool Equals(const elem_type& a, const Item& b) const;
//
//       /** @return True if (a < b); false otherwise. */
//       bool LessThan(const elem_type& a, const Item& b) const;
//   };
//
// The Equals method is used for searching, and the LessThan method is used
// for sorting.  The |Item| type above can be arbitrary, but must match the
// Item type passed to the sort or search function.
//
// The Alloc template parameter can be used to choose between
// "fallible" and "infallible" nsTArray (if available), defaulting to
// fallible.  If the *fallible* allocator is used, the return value of
// methods that might allocate needs to be checked; Append() is
// one such method.  These return values don't need to be checked if
// the *in*fallible allocator is chosen.  When in doubt, choose the
// infallible allocator.
//
template<class E, class Alloc=nsTArrayDefaultAllocator>
class nsTArray : public nsTArray_base<Alloc>,
                 public nsTArray_SafeElementAtHelper<E, nsTArray<E, Alloc> >
{
public:
  typedef nsTArray_base<Alloc>           base_type;
  typedef typename base_type::size_type  size_type;
  typedef typename base_type::index_type index_type;
  typedef E                              elem_type;
  typedef nsTArray<E, Alloc>             self_type;
  typedef nsTArrayElementTraits<E>       elem_traits;
  typedef nsTArray_SafeElementAtHelper<E, self_type> safeelementat_helper_type;

  using safeelementat_helper_type::SafeElementAt;
  using base_type::EmptyHdr;

  // A special value that is used to indicate an invalid or unknown index
  // into the array.
  enum {
    NoIndex = index_type(-1)
  };

  using base_type::Length;

  //
  // Finalization method
  //

  ~nsTArray() { Clear(); }

  //
  // Initialization methods
  //

  nsTArray() {}

  // Initialize this array and pre-allocate some number of elements.
  explicit nsTArray(size_type capacity) {
    SetCapacity(capacity);
  }

  // The array's copy-constructor performs a 'deep' copy of the given array.
  // @param other  The array object to copy.
  nsTArray(const self_type& other) {
    AppendElements(other);
  }

  template<typename Allocator>
  nsTArray(const nsTArray<E, Allocator>& other) {
    AppendElements(other);
  }

  // The array's assignment operator performs a 'deep' copy of the given
  // array.  It is optimized to reuse existing storage if possible.
  // @param other  The array object to copy.
  nsTArray& operator=(const self_type& other) {
    ReplaceElementsAt(0, Length(), other.Elements(), other.Length());
    return *this;
  }

  // Return true if this array has the same length and the same
  // elements as |other|.
  bool operator==(const self_type& other) const {
    size_type len = Length();
    if (len != other.Length())
      return false;

    // XXX std::equal would be as fast or faster here
    for (index_type i = 0; i < len; ++i)
      if (!(operator[](i) == other[i]))
        return false;

    return true;
  }

  // Return true if this array does not have the same length and the same
  // elements as |other|.
  bool operator!=(const self_type& other) const {
    return !operator==(other);
  }

  template<typename Allocator>
  nsTArray& operator=(const nsTArray<E, Allocator>& other) {
    ReplaceElementsAt(0, Length(), other.Elements(), other.Length());
    return *this;
  }

  // @return The amount of memory used by this nsTArray, excluding
  // sizeof(*this).
  size_t SizeOfExcludingThis(nsMallocSizeOfFun mallocSizeOf) const {
    if (this->UsesAutoArrayBuffer() || Hdr() == EmptyHdr())
      return 0;
    return mallocSizeOf(this->Hdr());
  }

  // @return The amount of memory used by this nsTArray, including
  // sizeof(*this).
  size_t SizeOfIncludingThis(nsMallocSizeOfFun mallocSizeOf) const {
    return mallocSizeOf(this) + SizeOfExcludingThis(mallocSizeOf);
  }

  //
  // Accessor methods
  //

  // This method provides direct access to the array elements.
  // @return A pointer to the first element of the array.  If the array is
  // empty, then this pointer must not be dereferenced.
  elem_type* Elements() {
    return reinterpret_cast<elem_type *>(Hdr() + 1);
  }

  // This method provides direct, readonly access to the array elements.
  // @return A pointer to the first element of the array.  If the array is
  // empty, then this pointer must not be dereferenced.
  const elem_type* Elements() const {
    return reinterpret_cast<const elem_type *>(Hdr() + 1);
  }
    
  // This method provides direct access to the i'th element of the array.
  // The given index must be within the array bounds.
  // @param i  The index of an element in the array.
  // @return   A reference to the i'th element of the array.
  elem_type& ElementAt(index_type i) {
    MOZ_ASSERT(i < Length(), "invalid array index");
    return Elements()[i];
  }

  // This method provides direct, readonly access to the i'th element of the
  // array.  The given index must be within the array bounds.
  // @param i  The index of an element in the array.
  // @return   A const reference to the i'th element of the array.
  const elem_type& ElementAt(index_type i) const {
    MOZ_ASSERT(i < Length(), "invalid array index");
    return Elements()[i];
  }

  // This method provides direct access to the i'th element of the array in
  // a bounds safe manner. If the requested index is out of bounds the
  // provided default value is returned.
  // @param i  The index of an element in the array.
  // @param def The value to return if the index is out of bounds.
  elem_type& SafeElementAt(index_type i, elem_type& def) {
    return i < Length() ? Elements()[i] : def;
  }

  // This method provides direct access to the i'th element of the array in
  // a bounds safe manner. If the requested index is out of bounds the
  // provided default value is returned.
  // @param i  The index of an element in the array.
  // @param def The value to return if the index is out of bounds.
  const elem_type& SafeElementAt(index_type i, const elem_type& def) const {
    return i < Length() ? Elements()[i] : def;
  }

  // Shorthand for ElementAt(i)
  elem_type& operator[](index_type i) {
    return ElementAt(i);
  }

  // Shorthand for ElementAt(i)
  const elem_type& operator[](index_type i) const {
    return ElementAt(i);
  }

  // Shorthand for ElementAt(length - 1)
  elem_type& LastElement() {
    return ElementAt(Length() - 1);
  }

  // Shorthand for ElementAt(length - 1)
  const elem_type& LastElement() const {
    return ElementAt(Length() - 1);
  }

  // Shorthand for SafeElementAt(length - 1, def)
  elem_type& SafeLastElement(elem_type& def) {
    return SafeElementAt(Length() - 1, def);
  }

  // Shorthand for SafeElementAt(length - 1, def)
  const elem_type& SafeLastElement(const elem_type& def) const {
    return SafeElementAt(Length() - 1, def);
  }

  //
  // Search methods
  //

  // This method searches for the first element in this array that is equal
  // to the given element.
  // @param item   The item to search for.
  // @param comp   The Comparator used to determine element equality.
  // @return       true if the element was found.
  template<class Item, class Comparator>
  bool Contains(const Item& item, const Comparator& comp) const {
    return IndexOf(item, 0, comp) != NoIndex;
  }

  // This method searches for the first element in this array that is equal
  // to the given element.  This method assumes that 'operator==' is defined
  // for elem_type.
  // @param item   The item to search for.
  // @return       true if the element was found.
  template<class Item>
  bool Contains(const Item& item) const {
    return IndexOf(item) != NoIndex;
  }

  // This method searches for the offset of the first element in this
  // array that is equal to the given element.
  // @param item   The item to search for.
  // @param start  The index to start from.
  // @param comp   The Comparator used to determine element equality.
  // @return       The index of the found element or NoIndex if not found.
  template<class Item, class Comparator>
  index_type IndexOf(const Item& item, index_type start,
                     const Comparator& comp) const {
    const elem_type* iter = Elements() + start, *end = Elements() + Length();
    for (; iter != end; ++iter) {
      if (comp.Equals(*iter, item))
        return index_type(iter - Elements());
    }
    return NoIndex;
  }

  // This method searches for the offset of the first element in this
  // array that is equal to the given element.  This method assumes
  // that 'operator==' is defined for elem_type.
  // @param item   The item to search for.
  // @param start  The index to start from.
  // @return       The index of the found element or NoIndex if not found.
  template<class Item>
  index_type IndexOf(const Item& item, index_type start = 0) const {
    return IndexOf(item, start, nsDefaultComparator<elem_type, Item>());
  }

  // This method searches for the offset of the last element in this
  // array that is equal to the given element.
  // @param item   The item to search for.
  // @param start  The index to start from.  If greater than or equal to the
  //               length of the array, then the entire array is searched.
  // @param comp   The Comparator used to determine element equality.
  // @return       The index of the found element or NoIndex if not found.
  template<class Item, class Comparator>
  index_type LastIndexOf(const Item& item, index_type start,
                         const Comparator& comp) const {
    if (start >= Length())
      start = Length() - 1;
    const elem_type* end = Elements() - 1, *iter = end + start + 1;
    for (; iter != end; --iter) {
      if (comp.Equals(*iter, item))
        return index_type(iter - Elements());
    }
    return NoIndex;
  }

  // This method searches for the offset of the last element in this
  // array that is equal to the given element.  This method assumes
  // that 'operator==' is defined for elem_type.
  // @param item   The item to search for.
  // @param start  The index to start from.  If greater than or equal to the
  //               length of the array, then the entire array is searched.
  // @return       The index of the found element or NoIndex if not found.
  template<class Item>
  index_type LastIndexOf(const Item& item,
                         index_type start = NoIndex) const {
    return LastIndexOf(item, start, nsDefaultComparator<elem_type, Item>());
  }

  // This method searches for the offset for the element in this array
  // that is equal to the given element. The array is assumed to be sorted.
  // @param item   The item to search for.
  // @param comp   The Comparator used.
  // @return       The index of the found element or NoIndex if not found.
  template<class Item, class Comparator>
  index_type BinaryIndexOf(const Item& item, const Comparator& comp) const {
    index_type low = 0, high = Length();
    while (high > low) {
      index_type mid = (high + low) >> 1;
      if (comp.Equals(ElementAt(mid), item))
        return mid;
      if (comp.LessThan(ElementAt(mid), item))
        low = mid + 1;
      else
        high = mid;
    }
    return NoIndex;
  }

  // This method searches for the offset for the element in this array
  // that is equal to the given element. The array is assumed to be sorted.
  // This method assumes that 'operator==' and 'operator<' are defined.
  // @param item   The item to search for.
  // @return       The index of the found element or NoIndex if not found.
  template<class Item>
  index_type BinaryIndexOf(const Item& item) const {
    return BinaryIndexOf(item, nsDefaultComparator<elem_type, Item>());
  }

  //
  // Mutation methods
  //

  // This method replaces a range of elements in this array.
  // @param start     The starting index of the elements to replace.
  // @param count     The number of elements to replace.  This may be zero to
  //                  insert elements without removing any existing elements.
  // @param array     The values to copy into this array.  Must be non-null,
  //                  and these elements must not already exist in the array
  //                  being modified.
  // @param arrayLen  The number of values to copy into this array.
  // @return          A pointer to the new elements in the array, or null if
  //                  the operation failed due to insufficient memory.
  template<class Item>
  elem_type *ReplaceElementsAt(index_type start, size_type count,
                               const Item* array, size_type arrayLen) {
    // Adjust memory allocation up-front to catch errors.
    if (!this->EnsureCapacity(Length() + arrayLen - count, sizeof(elem_type)))
      return nsnull;
    DestructRange(start, count);
    this->ShiftData(start, count, arrayLen, sizeof(elem_type), MOZ_ALIGNOF(elem_type));
    AssignRange(start, arrayLen, array);
    return Elements() + start;
  }

  // A variation on the ReplaceElementsAt method defined above.
  template<class Item>
  elem_type *ReplaceElementsAt(index_type start, size_type count,
                               const nsTArray<Item>& array) {
    return ReplaceElementsAt(start, count, array.Elements(), array.Length());
  }

  // A variation on the ReplaceElementsAt method defined above.
  template<class Item>
  elem_type *ReplaceElementsAt(index_type start, size_type count,
                               const Item& item) {
    return ReplaceElementsAt(start, count, &item, 1);
  }

  // A variation on the ReplaceElementsAt method defined above.
  template<class Item>
  elem_type *ReplaceElementAt(index_type index, const Item& item) {
    return ReplaceElementsAt(index, 1, &item, 1);
  }

  // A variation on the ReplaceElementsAt method defined above.
  template<class Item>
  elem_type *InsertElementsAt(index_type index, const Item* array,
                              size_type arrayLen) {
    return ReplaceElementsAt(index, 0, array, arrayLen);
  }

  // A variation on the ReplaceElementsAt method defined above.
  template<class Item>
  elem_type *InsertElementsAt(index_type index, const nsTArray<Item>& array) {
    return ReplaceElementsAt(index, 0, array.Elements(), array.Length());
  }

  // A variation on the ReplaceElementsAt method defined above.
  template<class Item>
  elem_type *InsertElementAt(index_type index, const Item& item) {
    return ReplaceElementsAt(index, 0, &item, 1);
  }

  // Insert a new element without copy-constructing. This is useful to avoid
  // temporaries.
  // @return A pointer to the newly inserted element, or null on OOM.
  elem_type* InsertElementAt(index_type index) {
    if (!this->EnsureCapacity(Length() + 1, sizeof(elem_type)))
      return nsnull;
    this->ShiftData(index, 0, 1, sizeof(elem_type), MOZ_ALIGNOF(elem_type));
    elem_type *elem = Elements() + index;
    elem_traits::Construct(elem);
    return elem;
  }

  // This method searches for the least index of the greatest
  // element less than or equal to |item|.  If |item| is inserted at
  // this index, the array will remain sorted.  True is returned iff
  // this index is also equal to |item|.  In this case, the returned
  // index may point to the start of multiple copies of |item|.
  // @param item   The item to search for.
  // @param comp   The Comparator used.
  // @outparam idx The index of greatest element <= to |item|
  // @return       True iff |item == array[*idx]|.
  // @precondition The array is sorted
  template<class Item, class Comparator>
  bool
  GreatestIndexLtEq(const Item& item,
                    const Comparator& comp,
                    index_type* idx) const {
    // Nb: we could replace all the uses of "BinaryIndexOf" with this
    // function, but BinaryIndexOf will be oh-so-slightly faster so
    // it's not strictly desired to do.

    // invariant: low <= [idx] < high
    index_type low = 0, high = Length();
    while (high > low) {
      index_type mid = (high + low) >> 1;
      if (comp.Equals(ElementAt(mid), item)) {
        // we might have the array [..., 2, 4, 4, 4, 4, 4, 5, ...]
        // and be searching for "4". it's arbitrary where mid ends
        // up here, so we back it up to the first instance to maintain
        // the "least index ..." we promised above.
        do {
          --mid;
        } while (NoIndex != mid && comp.Equals(ElementAt(mid), item));
        *idx = ++mid;
        return true;
      }
      if (comp.LessThan(ElementAt(mid), item))
        // invariant: low <= idx < high
        low = mid + 1;
      else
        // invariant: low <= idx < high
        high = mid;
    }
    // low <= idx < high, so insert at high ("shifting" high up by
    // 1) to maintain invariant.
    // (or insert at low, since low==high; just a matter of taste here.)
    *idx = high;
    return false;
  }

  // A variation on the GreatestIndexLtEq method defined above.
  template<class Item, class Comparator>
  bool
  GreatestIndexLtEq(const Item& item,
                    index_type& idx,
                    const Comparator& comp) const {
    return GreatestIndexLtEq(item, comp, &idx);
  }

  // A variation on the GreatestIndexLtEq method defined above.
  template<class Item>
  bool
  GreatestIndexLtEq(const Item& item,
                    index_type& idx) const {
    return GreatestIndexLtEq(item, nsDefaultComparator<elem_type, Item>(), &idx);
  }

  // Inserts |item| at such an index to guarantee that if the array
  // was previously sorted, it will remain sorted after this
  // insertion.
  template<class Item, class Comparator>
  elem_type *InsertElementSorted(const Item& item, const Comparator& comp) {
    index_type index;
    GreatestIndexLtEq(item, comp, &index);
    return InsertElementAt(index, item);
  }

  // A variation on the InsertElementSorted method defined above.
  template<class Item>
  elem_type *InsertElementSorted(const Item& item) {
    return InsertElementSorted(item, nsDefaultComparator<elem_type, Item>());
  }

  // This method appends elements to the end of this array.
  // @param array     The elements to append to this array.
  // @param arrayLen  The number of elements to append to this array.
  // @return          A pointer to the new elements in the array, or null if
  //                  the operation failed due to insufficient memory.
  template<class Item>
  elem_type *AppendElements(const Item* array, size_type arrayLen) {
    if (!this->EnsureCapacity(Length() + arrayLen, sizeof(elem_type)))
      return nsnull;
    index_type len = Length();
    AssignRange(len, arrayLen, array);
    this->IncrementLength(arrayLen);
    return Elements() + len;
  }

  // A variation on the AppendElements method defined above.
  template<class Item, class Allocator>
  elem_type *AppendElements(const nsTArray<Item, Allocator>& array) {
    return AppendElements(array.Elements(), array.Length());
  }

  // A variation on the AppendElements method defined above.
  template<class Item>
  elem_type *AppendElement(const Item& item) {
    return AppendElements(&item, 1);
  }

  // Append new elements without copy-constructing. This is useful to avoid
  // temporaries.
  // @return A pointer to the newly appended elements, or null on OOM.
  elem_type *AppendElements(size_type count) {
    if (!this->EnsureCapacity(Length() + count, sizeof(elem_type)))
      return nsnull;
    elem_type *elems = Elements() + Length();
    size_type i;
    for (i = 0; i < count; ++i) {
      elem_traits::Construct(elems + i);
    }
    this->IncrementLength(count);
    return elems;
  }

  // Append a new element without copy-constructing. This is useful to avoid
  // temporaries.
  // @return A pointer to the newly appended element, or null on OOM.
  elem_type *AppendElement() {
    return AppendElements(1);
  }

  // Move all elements from another array to the end of this array without 
  // calling copy constructors or destructors.
  // @return A pointer to the newly appended elements, or null on OOM.
  template<class Item, class Allocator>
  elem_type *MoveElementsFrom(nsTArray<Item, Allocator>& array) {
    MOZ_ASSERT(&array != this, "argument must be different array");
    index_type len = Length();
    index_type otherLen = array.Length();
    if (!this->EnsureCapacity(len + otherLen, sizeof(elem_type)))
      return nsnull;
    memcpy(Elements() + len, array.Elements(), otherLen * sizeof(elem_type));
    this->IncrementLength(otherLen);      
    array.ShiftData(0, otherLen, 0, sizeof(elem_type), MOZ_ALIGNOF(elem_type));
    return Elements() + len;
  }

  // This method removes a range of elements from this array.
  // @param start  The starting index of the elements to remove.
  // @param count  The number of elements to remove.
  void RemoveElementsAt(index_type start, size_type count) {
    MOZ_ASSERT(count == 0 || start < Length(), "Invalid start index");
    MOZ_ASSERT(start + count <= Length(), "Invalid length");
    // Check that the previous assert didn't overflow
    MOZ_ASSERT(start <= start + count, "Start index plus length overflows");
    DestructRange(start, count);
    this->ShiftData(start, count, 0, sizeof(elem_type), MOZ_ALIGNOF(elem_type));
  }

  // A variation on the RemoveElementsAt method defined above.
  void RemoveElementAt(index_type index) {
    RemoveElementsAt(index, 1);
  }

  // A variation on the RemoveElementsAt method defined above.
  void Clear() {
    RemoveElementsAt(0, Length());
  }

  // This helper function combines IndexOf with RemoveElementAt to "search
  // and destroy" the first element that is equal to the given element.
  // @param item  The item to search for.
  // @param comp  The Comparator used to determine element equality.
  // @return true if the element was found
  template<class Item, class Comparator>
  bool RemoveElement(const Item& item, const Comparator& comp) {
    index_type i = IndexOf(item, 0, comp);
    if (i == NoIndex)
      return false;

    RemoveElementAt(i);
    return true;
  }

  // A variation on the RemoveElement method defined above that assumes
  // that 'operator==' is defined for elem_type.
  template<class Item>
  bool RemoveElement(const Item& item) {
    return RemoveElement(item, nsDefaultComparator<elem_type, Item>());
  }

  // This helper function combines GreatestIndexLtEq with
  // RemoveElementAt to "search and destroy" the first element that
  // is equal to the given element.
  // @param item  The item to search for.
  // @param comp  The Comparator used to determine element equality.
  // @return true if the element was found
  template<class Item, class Comparator>
  bool RemoveElementSorted(const Item& item, const Comparator& comp) {
    index_type index;
    bool found = GreatestIndexLtEq(item, comp, &index);
    if (found)
      RemoveElementAt(index);
    return found;
  }

  // A variation on the RemoveElementSorted method defined above.
  template<class Item>
  bool RemoveElementSorted(const Item& item) {
    return RemoveElementSorted(item, nsDefaultComparator<elem_type, Item>());
  }

  // This method causes the elements contained in this array and the given
  // array to be swapped.
  template<class Allocator>
  bool SwapElements(nsTArray<E, Allocator>& other) {
    return this->SwapArrayElements(other, sizeof(elem_type), MOZ_ALIGNOF(elem_type));
  }

  //
  // Allocation
  //

  // This method may increase the capacity of this array object by the
  // specified amount.  This method may be called in advance of several
  // AppendElement operations to minimize heap re-allocations.  This method
  // will not reduce the number of elements in this array.
  // @param capacity  The desired capacity of this array.
  // @return True if the operation succeeded; false if we ran out of memory
  bool SetCapacity(size_type capacity) {
    return this->EnsureCapacity(capacity, sizeof(elem_type));
  }

  // This method modifies the length of the array.  If the new length is
  // larger than the existing length of the array, then new elements will be
  // constructed using elem_type's default constructor.  Otherwise, this call
  // removes elements from the array (see also RemoveElementsAt).
  // @param newLen  The desired length of this array.
  // @return        True if the operation succeeded; false otherwise.
  // See also TruncateLength if the new length is guaranteed to be
  // smaller than the old.
  bool SetLength(size_type newLen) {
    size_type oldLen = Length();
    if (newLen > oldLen) {
      return InsertElementsAt(oldLen, newLen - oldLen) != nsnull;
    }
      
    TruncateLength(newLen);
    return true;
  }

  // This method modifies the length of the array, but may only be
  // called when the new length is shorter than the old.  It can
  // therefore be called when elem_type has no default constructor,
  // unlike SetLength.  It removes elements from the array (see also
  // RemoveElementsAt).
  // @param newLen  The desired length of this array.
  void TruncateLength(size_type newLen) {
    size_type oldLen = Length();
    NS_ABORT_IF_FALSE(newLen <= oldLen,
                      "caller should use SetLength instead");
    RemoveElementsAt(newLen, oldLen - newLen);
  }

  // This method ensures that the array has length at least the given
  // length.  If the current length is shorter than the given length,
  // then new elements will be constructed using elem_type's default
  // constructor.
  // @param minLen  The desired minimum length of this array.
  // @return        True if the operation succeeded; false otherwise.
  bool EnsureLengthAtLeast(size_type minLen) {
    size_type oldLen = Length();
    if (minLen > oldLen) {
      return InsertElementsAt(oldLen, minLen - oldLen) != nsnull;
    }
    return true;
  }

  // This method inserts elements into the array, constructing
  // them using elem_type's default constructor.
  // @param index the place to insert the new elements. This must be no
  //              greater than the current length of the array.
  // @param count the number of elements to insert
  elem_type *InsertElementsAt(index_type index, size_type count) {
    if (!base_type::InsertSlotsAt(index, count, sizeof(elem_type), MOZ_ALIGNOF(elem_type))) {
      return nsnull;
    }

    // Initialize the extra array elements
    elem_type *iter = Elements() + index, *end = iter + count;
    for (; iter != end; ++iter) {
      elem_traits::Construct(iter);
    }

    return Elements() + index;
  }

  // This method inserts elements into the array, constructing them
  // elem_type's copy constructor (or whatever one-arg constructor
  // happens to match the Item type).
  // @param index the place to insert the new elements. This must be no
  //              greater than the current length of the array.
  // @param count the number of elements to insert.
  // @param item the value to use when constructing the new elements.
  template<class Item>
  elem_type *InsertElementsAt(index_type index, size_type count,
                              const Item& item) {
    if (!base_type::InsertSlotsAt(index, count, sizeof(elem_type), MOZ_ALIGNOF(elem_type))) {
      return nsnull;
    }

    // Initialize the extra array elements
    elem_type *iter = Elements() + index, *end = iter + count;
    for (; iter != end; ++iter) {
      elem_traits::Construct(iter, item);
    }

    return Elements() + index;
  }

  // This method may be called to minimize the memory used by this array.
  void Compact() {
    ShrinkCapacity(sizeof(elem_type), MOZ_ALIGNOF(elem_type));
  }

  //
  // Sorting
  //
 
  // This function is meant to be used with the NS_QuickSort function.  It
  // maps the callback API expected by NS_QuickSort to the Comparator API
  // used by nsTArray.  See nsTArray::Sort.
  template<class Comparator>
  static int Compare(const void* e1, const void* e2, void *data) {
    const Comparator* c = reinterpret_cast<const Comparator*>(data);
    const elem_type* a = static_cast<const elem_type*>(e1);
    const elem_type* b = static_cast<const elem_type*>(e2);
    return c->LessThan(*a, *b) ? -1 : (c->Equals(*a, *b) ? 0 : 1);
  }

  // This method sorts the elements of the array.  It uses the LessThan
  // method defined on the given Comparator object to collate elements.
  // @param comp The Comparator used to collate elements.
  template<class Comparator>
  void Sort(const Comparator& comp) {
    NS_QuickSort(Elements(), Length(), sizeof(elem_type),
                 Compare<Comparator>, const_cast<Comparator*>(&comp));
  }

  // A variation on the Sort method defined above that assumes that
  // 'operator<' is defined for elem_type.
  void Sort() {
    Sort(nsDefaultComparator<elem_type, elem_type>());
  }

  //
  // Binary Heap
  //

  // Sorts the array into a binary heap.
  // @param comp The Comparator used to create the heap
  template<class Comparator>
  void MakeHeap(const Comparator& comp) {
    if (!Length()) {
      return;
    }
    index_type index = (Length() - 1) / 2;
    do {
      SiftDown(index, comp);
    } while (index--);
  }

  // A variation on the MakeHeap method defined above.
  void MakeHeap() {
    MakeHeap(nsDefaultComparator<elem_type, elem_type>());
  }

  // Adds an element to the heap
  // @param item The item to add
  // @param comp The Comparator used to sift-up the item
  template<class Item, class Comparator>
  elem_type *PushHeap(const Item& item, const Comparator& comp) {
    if (!base_type::InsertSlotsAt(Length(), 1, sizeof(elem_type), MOZ_ALIGNOF(elem_type))) {
      return nsnull;
    }
    // Sift up the new node
    elem_type *elem = Elements();
    index_type index = Length() - 1;
    index_type parent_index = (index - 1) / 2;
    while (index && comp.LessThan(elem[parent_index], item)) {
      elem[index] = elem[parent_index];
      index = parent_index;
      parent_index = (index - 1) / 2;
    }
    elem[index] = item;
    return &elem[index];
  }

  // A variation on the PushHeap method defined above.
  template<class Item>
  elem_type *PushHeap(const Item& item) {
    return PushHeap(item, nsDefaultComparator<elem_type, Item>());
  }

  // Delete the root of the heap and restore the heap
  // @param comp The Comparator used to restore the heap
  template<class Comparator>
  void PopHeap(const Comparator& comp) {
    if (!Length()) {
      return;
    }
    index_type last_index = Length() - 1;
    elem_type *elem = Elements();
    elem[0] = elem[last_index];
    TruncateLength(last_index);
    if (Length()) {
      SiftDown(0, comp);
    }
  }

  // A variation on the PopHeap method defined above.
  void PopHeap() {
    PopHeap(nsDefaultComparator<elem_type, elem_type>());
  }

protected:
  using base_type::Hdr;
  using base_type::ShrinkCapacity;

  // This method invokes elem_type's destructor on a range of elements.
  // @param start  The index of the first element to destroy.
  // @param count  The number of elements to destroy.
  void DestructRange(index_type start, size_type count) {
    elem_type *iter = Elements() + start, *end = iter + count;
    for (; iter != end; ++iter) {
      elem_traits::Destruct(iter);
    }
  }

  // This method invokes elem_type's copy-constructor on a range of elements.
  // @param start   The index of the first element to construct.
  // @param count   The number of elements to construct. 
  // @param values  The array of elements to copy. 
  template<class Item>
  void AssignRange(index_type start, size_type count,
                   const Item *values) {
    elem_type *iter = Elements() + start, *end = iter + count;
    for (; iter != end; ++iter, ++values) {
      elem_traits::Construct(iter, *values);
    }
  }

  // This method sifts an item down to its proper place in a binary heap
  // @param index The index of the node to start sifting down from
  // @param comp  The Comparator used to sift down
  template<class Comparator>
  void SiftDown(index_type index, const Comparator& comp) {
    elem_type *elem = Elements();
    elem_type item = elem[index];
    index_type end = Length() - 1;
    while ((index * 2) < end) {
      const index_type left = (index * 2) + 1;
      const index_type right = (index * 2) + 2;
      const index_type parent_index = index;
      if (comp.LessThan(item, elem[left])) {
        if (left < end &&
            comp.LessThan(elem[left], elem[right])) {
          index = right;
        } else {
          index = left;
        }
      } else if (left < end &&
                 comp.LessThan(item, elem[right])) {
        index = right;
      } else {
        break;
      }
      elem[parent_index] = elem[index];
    }
    elem[index] = item;
  }
};

//
// Convenience subtypes of nsTArray.
//
template<class E>
class FallibleTArray : public nsTArray<E, nsTArrayFallibleAllocator>
{
public:
  typedef nsTArray<E, nsTArrayFallibleAllocator>   base_type;
  typedef typename base_type::size_type            size_type;

  FallibleTArray() {}
  explicit FallibleTArray(size_type capacity) : base_type(capacity) {}
  FallibleTArray(const FallibleTArray& other) : base_type(other) {}
};

#ifdef MOZALLOC_HAVE_XMALLOC
template<class E>
class InfallibleTArray : public nsTArray<E, nsTArrayInfallibleAllocator>
{
public:
  typedef nsTArray<E, nsTArrayInfallibleAllocator> base_type;
  typedef typename base_type::size_type            size_type;
 
  InfallibleTArray() {}
  explicit InfallibleTArray(size_type capacity) : base_type(capacity) {}
  InfallibleTArray(const InfallibleTArray& other) : base_type(other) {}
};
#endif

template<class TArrayBase, PRUint32 N>
class nsAutoArrayBase : public TArrayBase
{
public:
  typedef TArrayBase base_type;
  typedef typename base_type::Header Header;
  typedef typename base_type::elem_type elem_type;

protected:
  nsAutoArrayBase() {
    Init();
  }

  // We need this constructor because nsAutoTArray and friends all have
  // implicit copy-constructors.  If we don't have this method, those
  // copy-constructors will call nsAutoArrayBase's implicit copy-constructor,
  // which won't call Init() and set up the auto buffer!
  nsAutoArrayBase(const TArrayBase &aOther) {
    Init();
    AppendElements(aOther);
  }

private:
  // nsTArray_base casts itself as an nsAutoArrayBase in order to get a pointer
  // to mAutoBuf.
  template<class Allocator>
  friend class nsTArray_base;

  void Init() {
    MOZ_STATIC_ASSERT(MOZ_ALIGNOF(elem_type) <= 8,
                      "can't handle alignments greater than 8, "
                      "see nsTArray_base::UsesAutoArrayBuffer()");
    // Temporary work around for VS2012 RC compiler crash
    Header** phdr = base_type::PtrToHdr();
    *phdr = reinterpret_cast<Header*>(&mAutoBuf);
    (*phdr)->mLength = 0;
    (*phdr)->mCapacity = N;
    (*phdr)->mIsAutoArray = 1;

    MOZ_ASSERT(base_type::GetAutoArrayBuffer(MOZ_ALIGNOF(elem_type)) ==
               reinterpret_cast<Header*>(&mAutoBuf),
               "GetAutoArrayBuffer needs to be fixed");
  }

  // Declare mAutoBuf aligned to the maximum of the header's alignment and
  // elem_type's alignment.  We need to use a union rather than
  // MOZ_ALIGNED_DECL because GCC is picky about what goes into
  // __attribute__((aligned(foo))).
  union {
    char mAutoBuf[sizeof(nsTArrayHeader) + N * sizeof(elem_type)];
    mozilla::AlignedElem<PR_MAX(MOZ_ALIGNOF(Header), MOZ_ALIGNOF(elem_type))> mAlign;
  };
};

template<class E, PRUint32 N, class Alloc=nsTArrayDefaultAllocator>
class nsAutoTArray : public nsAutoArrayBase<nsTArray<E, Alloc>, N>
{
  typedef nsAutoArrayBase<nsTArray<E, Alloc>, N> Base;

public:
  nsAutoTArray() {}

  template<typename Allocator>
  nsAutoTArray(const nsTArray<E, Allocator>& other) {
    Base::AppendElements(other);
  }
};

// Assert that nsAutoTArray doesn't have any extra padding inside.
//
// It's important that the data stored in this auto array takes up a multiple of
// 8 bytes; e.g. nsAutoTArray<PRUint32, 1> wouldn't work.  Since nsAutoTArray
// contains a pointer, its size must be a multiple of alignof(void*).  (This is
// because any type may be placed into an array, and there's no padding between
// elements of an array.)  The compiler pads the end of the structure to
// enforce this rule.
//
// If we used nsAutoTArray<PRUint32, 1> below, this assertion would fail on a
// 64-bit system, where the compiler inserts 4 bytes of padding at the end of
// the auto array to make its size a multiple of alignof(void*) == 8 bytes.

MOZ_STATIC_ASSERT(sizeof(nsAutoTArray<PRUint32, 2>) ==
                  sizeof(void*) + sizeof(nsTArrayHeader) + sizeof(PRUint32) * 2,
                  "nsAutoTArray shouldn't contain any extra padding, "
                  "see the comment");

template<class E, PRUint32 N>
class AutoFallibleTArray : public nsAutoArrayBase<FallibleTArray<E>, N>
{
  typedef nsAutoArrayBase<FallibleTArray<E>, N> Base;

public:
  AutoFallibleTArray() {}

  template<typename Allocator>
  AutoFallibleTArray(const nsTArray<E, Allocator>& other) {
    Base::AppendElements(other);
  }
};

#if defined(MOZALLOC_HAVE_XMALLOC)
template<class E, PRUint32 N>
class AutoInfallibleTArray : public nsAutoArrayBase<InfallibleTArray<E>, N>
{
  typedef nsAutoArrayBase<InfallibleTArray<E>, N> Base;

public:
  AutoInfallibleTArray() {}

  template<typename Allocator>
  AutoInfallibleTArray(const nsTArray<E, Allocator>& other) {
    Base::AppendElements(other);
  }
};
#endif

// specializations for N = 0. this makes the inheritance model easier for
// templated users of nsAutoTArray.
template<class E>
class nsAutoTArray<E, 0, nsTArrayDefaultAllocator> :
  public nsAutoArrayBase< nsTArray<E, nsTArrayDefaultAllocator>, 0>
{
public:
  nsAutoTArray() {}
};

template<class E>
class AutoFallibleTArray<E, 0> :
  public nsAutoArrayBase< FallibleTArray<E>, 0>
{
public:
  AutoFallibleTArray() {}
};

#if defined(MOZALLOC_HAVE_XMALLOC)
template<class E>
class AutoInfallibleTArray<E, 0> :
  public nsAutoArrayBase< InfallibleTArray<E>, 0>
{
public:
  AutoInfallibleTArray() {}
};
#endif
 
// Definitions of nsTArray methods
#include "nsTArray-inl.h"

#endif  // nsTArray_h__
