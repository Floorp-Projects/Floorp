/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTArray_h__
#define nsTArray_h__

#include "nsTArrayForwardDeclare.h"
#include "mozilla/Alignment.h"
#include "mozilla/Assertions.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/TypeTraits.h"

#include <string.h>

#include "nsCycleCollectionNoteChild.h"
#include "nsAlgorithm.h"
#include "nscore.h"
#include "nsQuickSort.h"
#include "nsDebug.h"
#include "nsISupportsImpl.h"
#include <new>

namespace JS {
template<class T>
class Heap;
} /* namespace JS */

class nsRegion;
class nsIntRegion;

//
// nsTArray is a resizable array class, like std::vector.
//
// Unlike std::vector, which follows C++'s construction/destruction rules,
// nsTArray assumes that your "T" can be memmoved()'ed safely.
//
// The public classes defined in this header are
//
//   nsTArray<T>,
//   FallibleTArray<T>,
//   nsAutoTArray<T, N>, and
//   AutoFallibleTArray<T, N>.
//
// nsTArray and nsAutoTArray are infallible; if one tries to make an allocation
// which fails, it crashes the program.  In contrast, FallibleTArray and
// AutoFallibleTArray are fallible; if you use one of these classes, you must
// check the return values of methods such as Append() which may allocate.  If
// in doubt, choose an infallible type.
//
// InfallibleTArray and AutoInfallibleTArray are aliases for nsTArray and
// nsAutoTArray.
//
// If you just want to declare the nsTArray types (e.g., if you're in a header
// file and don't need the full nsTArray definitions) consider including
// nsTArrayForwardDeclare.h instead of nsTArray.h.
//
// The template parameter (i.e., T in nsTArray<T>) specifies the type of the
// elements and has the following requirements:
//
//   T MUST be safely memmove()'able.
//   T MUST define a copy-constructor.
//   T MAY define operator< for sorting.
//   T MAY define operator== for searching.
//
// (Note that the memmove requirement may be relaxed for certain types - see
// nsTArray_CopyChooser below.)
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
// The Equals method is used for searching, and the LessThan method is used for
// searching and sorting.  The |Item| type above can be arbitrary, but must
// match the Item type passed to the sort or search function.
//


//
// nsTArrayFallibleResult and nsTArrayInfallibleResult types are proxy types
// which are used because you cannot use a templated type which is bound to
// void as an argument to a void function.  In order to work around that, we
// encode either a void or a boolean inside these proxy objects, and pass them
// to the aforementioned function instead, and then use the type information to
// decide what to do in the function.
//
// Note that public nsTArray methods should never return a proxy type.  Such
// types are only meant to be used in the internal nsTArray helper methods.
// Public methods returning non-proxy types cannot be called from other
// nsTArray members.
//
struct nsTArrayFallibleResult
{
  // Note: allows implicit conversions from and to bool
  MOZ_IMPLICIT nsTArrayFallibleResult(bool aResult) : mResult(aResult) {}

  operator bool() { return mResult; }

private:
  bool mResult;
};

struct nsTArrayInfallibleResult
{
};

//
// nsTArray*Allocators must all use the same |free()|, to allow swap()'ing
// between fallible and infallible variants.
//

struct nsTArrayFallibleAllocatorBase
{
  typedef bool ResultType;
  typedef nsTArrayFallibleResult ResultTypeProxy;

  static ResultType Result(ResultTypeProxy aResult) { return aResult; }
  static bool Successful(ResultTypeProxy aResult) { return aResult; }
  static ResultTypeProxy SuccessResult() { return true; }
  static ResultTypeProxy FailureResult() { return false; }
  static ResultType ConvertBoolToResultType(bool aValue) { return aValue; }
};

struct nsTArrayInfallibleAllocatorBase
{
  typedef void ResultType;
  typedef nsTArrayInfallibleResult ResultTypeProxy;

  static ResultType Result(ResultTypeProxy aResult) {}
  static bool Successful(ResultTypeProxy) { return true; }
  static ResultTypeProxy SuccessResult() { return ResultTypeProxy(); }

  static ResultTypeProxy FailureResult()
  {
    NS_RUNTIMEABORT("Infallible nsTArray should never fail");
    return ResultTypeProxy();
  }

  static ResultType ConvertBoolToResultType(bool aValue)
  {
    if (!aValue) {
      NS_RUNTIMEABORT("infallible nsTArray should never convert false to ResultType");
    }
  }
};

#if defined(MOZALLOC_HAVE_XMALLOC)
#include "mozilla/mozalloc_abort.h"

struct nsTArrayFallibleAllocator : nsTArrayFallibleAllocatorBase
{
  static void* Malloc(size_t aSize) { return moz_malloc(aSize); }
  static void* Realloc(void* aPtr, size_t aSize)
  {
    return moz_realloc(aPtr, aSize);
  }

  static void Free(void* aPtr) { moz_free(aPtr); }
  static void SizeTooBig(size_t) {}
};

struct nsTArrayInfallibleAllocator : nsTArrayInfallibleAllocatorBase
{
  static void* Malloc(size_t aSize) { return moz_xmalloc(aSize); }
  static void* Realloc(void* aPtr, size_t aSize)
  {
    return moz_xrealloc(aPtr, aSize);
  }

  static void Free(void* aPtr) { moz_free(aPtr); }
  static void SizeTooBig(size_t aSize) { NS_ABORT_OOM(aSize); }
};

#else
#include <stdlib.h>

struct nsTArrayFallibleAllocator : nsTArrayFallibleAllocatorBase
{
  static void* Malloc(size_t aSize) { return malloc(aSize); }
  static void* Realloc(void* aPtr, size_t aSize) { return realloc(aPtr, aSize); }

  static void Free(void* aPtr) { free(aPtr); }
  static void SizeTooBig(size_t) {}
};

struct nsTArrayInfallibleAllocator : nsTArrayInfallibleAllocatorBase
{
  static void* Malloc(size_t aSize)
  {
    void* ptr = malloc(aSize);
    if (MOZ_UNLIKELY(!ptr)) {
      NS_ABORT_OOM(aSize);
    }
    return ptr;
  }

  static void* Realloc(void* aPtr, size_t aSize)
  {
    void* newptr = realloc(aPtr, aSize);
    if (MOZ_UNLIKELY(!newptr && aSize)) {
      NS_ABORT_OOM(aSize);
    }
    return newptr;
  }

  static void Free(void* aPtr) { free(aPtr); }
  static void SizeTooBig(size_t aSize) { NS_ABORT_OOM(aSize); }
};

#endif

// nsTArray_base stores elements into the space allocated beyond
// sizeof(*this).  This is done to minimize the size of the nsTArray
// object when it is empty.
struct NS_COM_GLUE nsTArrayHeader
{
  static nsTArrayHeader sEmptyHdr;

  uint32_t mLength;
  uint32_t mCapacity : 31;
  uint32_t mIsAutoArray : 1;
};

// This class provides a SafeElementAt method to nsTArray<T*> which does
// not take a second default value parameter.
template<class E, class Derived>
struct nsTArray_SafeElementAtHelper
{
  typedef E*       elem_type;
  typedef size_t   index_type;

  // No implementation is provided for these two methods, and that is on
  // purpose, since we don't support these functions on non-pointer type
  // instantiations.
  elem_type& SafeElementAt(index_type aIndex);
  const elem_type& SafeElementAt(index_type aIndex) const;
};

template<class E, class Derived>
struct nsTArray_SafeElementAtHelper<E*, Derived>
{
  typedef E*       elem_type;
  typedef size_t   index_type;

  elem_type SafeElementAt(index_type aIndex)
  {
    return static_cast<Derived*>(this)->SafeElementAt(aIndex, nullptr);
  }

  const elem_type SafeElementAt(index_type aIndex) const
  {
    return static_cast<const Derived*>(this)->SafeElementAt(aIndex, nullptr);
  }
};

// E is the base type that the smart pointer is templated over; the
// smart pointer can act as E*.
template<class E, class Derived>
struct nsTArray_SafeElementAtSmartPtrHelper
{
  typedef E*       elem_type;
  typedef size_t   index_type;

  elem_type SafeElementAt(index_type aIndex)
  {
    return static_cast<Derived*>(this)->SafeElementAt(aIndex, nullptr);
  }

  const elem_type SafeElementAt(index_type aIndex) const
  {
    return static_cast<const Derived*>(this)->SafeElementAt(aIndex, nullptr);
  }
};

template<class T> class nsCOMPtr;

template<class E, class Derived>
struct nsTArray_SafeElementAtHelper<nsCOMPtr<E>, Derived>
  : public nsTArray_SafeElementAtSmartPtrHelper<E, Derived>
{
};

template<class T> class nsRefPtr;

template<class E, class Derived>
struct nsTArray_SafeElementAtHelper<nsRefPtr<E>, Derived>
  : public nsTArray_SafeElementAtSmartPtrHelper<E, Derived>
{
};

//
// This class serves as a base class for nsTArray.  It shouldn't be used
// directly.  It holds common implementation code that does not depend on the
// element type of the nsTArray.
//
template<class Alloc, class Copy>
class nsTArray_base
{
  // Allow swapping elements with |nsTArray_base|s created using a
  // different allocator.  This is kosher because all allocators use
  // the same free().
  template<class Allocator, class Copier>
  friend class nsTArray_base;

protected:
  typedef nsTArrayHeader Header;

public:
  typedef size_t size_type;
  typedef size_t index_type;

  // @return The number of elements in the array.
  size_type Length() const { return mHdr->mLength; }

  // @return True if the array is empty or false otherwise.
  bool IsEmpty() const { return Length() == 0; }

  // @return The number of elements that can fit in the array without forcing
  // the array to be re-allocated.  The length of an array is always less
  // than or equal to its capacity.
  size_type Capacity() const {  return mHdr->mCapacity; }

#ifdef DEBUG
  void* DebugGetHeader() const { return mHdr; }
#endif

protected:
  nsTArray_base();

  ~nsTArray_base();

  // Resize the storage if necessary to achieve the requested capacity.
  // @param aCapacity The requested number of array elements.
  // @param aElemSize The size of an array element.
  // @return False if insufficient memory is available; true otherwise.
  typename Alloc::ResultTypeProxy EnsureCapacity(size_type aCapacity,
                                                 size_type aElemSize);

  // Resize the storage to the minimum required amount.
  // @param aElemSize  The size of an array element.
  // @param aElemAlign The alignment in bytes of an array element.
  void ShrinkCapacity(size_type aElemSize, size_t aElemAlign);

  // This method may be called to resize a "gap" in the array by shifting
  // elements around.  It updates mLength appropriately.  If the resulting
  // array has zero elements, then the array's memory is free'd.
  // @param aStart     The starting index of the gap.
  // @param aOldLen    The current length of the gap.
  // @param aNewLen    The desired length of the gap.
  // @param aElemSize  The size of an array element.
  // @param aElemAlign The alignment in bytes of an array element.
  void ShiftData(index_type aStart, size_type aOldLen, size_type aNewLen,
                 size_type aElemSize, size_t aElemAlign);

  // This method increments the length member of the array's header.
  // Note that mHdr may actually be sEmptyHdr in the case where a
  // zero-length array is inserted into our array. But then aNum should
  // always be 0.
  void IncrementLength(size_t aNum)
  {
    if (mHdr == EmptyHdr()) {
      if (MOZ_UNLIKELY(aNum != 0)) {
        // Writing a non-zero length to the empty header would be extremely bad.
        MOZ_CRASH();
      }
    } else {
      mHdr->mLength += aNum;
    }
  }

  // This method inserts blank slots into the array.
  // @param aIndex the place to insert the new elements. This must be no
  //               greater than the current length of the array.
  // @param aCount the number of slots to insert
  // @param aElementSize the size of an array element.
  // @param aElemAlign the alignment in bytes of an array element.
  bool InsertSlotsAt(index_type aIndex, size_type aCount,
                     size_type aElementSize, size_t aElemAlign);

protected:
  template<class Allocator>
  typename Alloc::ResultTypeProxy
  SwapArrayElements(nsTArray_base<Allocator, Copy>& aOther,
                    size_type aElemSize,
                    size_t aElemAlign);

  // This is an RAII class used in SwapArrayElements.
  class IsAutoArrayRestorer
  {
  public:
    IsAutoArrayRestorer(nsTArray_base<Alloc, Copy>& aArray, size_t aElemAlign);
    ~IsAutoArrayRestorer();

  private:
    nsTArray_base<Alloc, Copy>& mArray;
    size_t mElemAlign;
    bool mIsAuto;
  };

  // Helper function for SwapArrayElements. Ensures that if the array
  // is an nsAutoTArray that it doesn't use the built-in buffer.
  bool EnsureNotUsingAutoArrayBuffer(size_type aElemSize);

  // Returns true if this nsTArray is an nsAutoTArray with a built-in buffer.
  bool IsAutoArray() const { return mHdr->mIsAutoArray; }

  // Returns a Header for the built-in buffer of this nsAutoTArray.
  Header* GetAutoArrayBuffer(size_t aElemAlign)
  {
    MOZ_ASSERT(IsAutoArray(), "Should be an auto array to call this");
    return GetAutoArrayBufferUnsafe(aElemAlign);
  }
  const Header* GetAutoArrayBuffer(size_t aElemAlign) const
  {
    MOZ_ASSERT(IsAutoArray(), "Should be an auto array to call this");
    return GetAutoArrayBufferUnsafe(aElemAlign);
  }

  // Returns a Header for the built-in buffer of this nsAutoTArray, but doesn't
  // assert that we are an nsAutoTArray.
  Header* GetAutoArrayBufferUnsafe(size_t aElemAlign)
  {
    return const_cast<Header*>(static_cast<const nsTArray_base<Alloc, Copy>*>(
      this)->GetAutoArrayBufferUnsafe(aElemAlign));
  }
  const Header* GetAutoArrayBufferUnsafe(size_t aElemAlign) const;

  // Returns true if this is an nsAutoTArray and it currently uses the
  // built-in buffer to store its elements.
  bool UsesAutoArrayBuffer() const;

  // The array's elements (prefixed with a Header).  This pointer is never
  // null.  If the array is empty, then this will point to sEmptyHdr.
  Header* mHdr;

  Header* Hdr() const { return mHdr; }
  Header** PtrToHdr() { return &mHdr; }
  static Header* EmptyHdr() { return &Header::sEmptyHdr; }
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
  static inline void Construct(E* aE)
  {
    // Do NOT call "E()"! That triggers C++ "default initialization"
    // which zeroes out POD ("plain old data") types such as regular
    // ints.  We don't want that because it can be a performance issue
    // and people don't expect it; nsTArray should work like a regular
    // C/C++ array in this respect.
    new (static_cast<void*>(aE)) E;
  }
  // Invoke the copy-constructor in place.
  template<class A>
  static inline void Construct(E* aE, const A& aArg)
  {
    typedef typename mozilla::RemoveCV<E>::Type E_NoCV;
    typedef typename mozilla::RemoveCV<A>::Type A_NoCV;
    static_assert(!mozilla::IsSame<E_NoCV*, A_NoCV>::value,
                  "For safety, we disallow constructing nsTArray<E> elements "
                  "from E* pointers. See bug 960591.");
    new (static_cast<void*>(aE)) E(aArg);
  }
  // Invoke the destructor in place.
  static inline void Destruct(E* aE) { aE->~E(); }
};

// The default comparator used by nsTArray
template<class A, class B>
class nsDefaultComparator
{
public:
  bool Equals(const A& aA, const B& aB) const { return aA == aB; }
  bool LessThan(const A& aA, const B& aB) const { return aA < aB; }
};

template<class E> class InfallibleTArray;
template<class E> class FallibleTArray;

template<bool IsPod, bool IsSameType>
struct AssignRangeAlgorithm
{
  template<class Item, class ElemType, class IndexType, class SizeType>
  static void implementation(ElemType* aElements, IndexType aStart,
                             SizeType aCount, const Item* aValues)
  {
    ElemType* iter = aElements + aStart;
    ElemType* end = iter + aCount;
    for (; iter != end; ++iter, ++aValues) {
      nsTArrayElementTraits<ElemType>::Construct(iter, *aValues);
    }
  }
};

template<>
struct AssignRangeAlgorithm<true, true>
{
  template<class Item, class ElemType, class IndexType, class SizeType>
  static void implementation(ElemType* aElements, IndexType aStart,
                             SizeType aCount, const Item* aValues)
  {
    memcpy(aElements + aStart, aValues, aCount * sizeof(ElemType));
  }
};

//
// Normally elements are copied with memcpy and memmove, but for some element
// types that is problematic.  The nsTArray_CopyChooser template class can be
// specialized to ensure that copying calls constructors and destructors
// instead, as is done below for JS::Heap<E> elements.
//

//
// A class that defines how to copy elements using memcpy/memmove.
//
struct nsTArray_CopyWithMemutils
{
  const static bool allowRealloc = true;

  static void CopyElements(void* aDest, const void* aSrc, size_t aCount,
                           size_t aElemSize)
  {
    memcpy(aDest, aSrc, aCount * aElemSize);
  }

  static void CopyHeaderAndElements(void* aDest, const void* aSrc,
                                    size_t aCount, size_t aElemSize)
  {
    memcpy(aDest, aSrc, sizeof(nsTArrayHeader) + aCount * aElemSize);
  }

  static void MoveElements(void* aDest, const void* aSrc, size_t aCount,
                           size_t aElemSize)
  {
    memmove(aDest, aSrc, aCount * aElemSize);
  }
};

//
// A template class that defines how to copy elements calling their constructors
// and destructors appropriately.
//
template<class ElemType>
struct nsTArray_CopyWithConstructors
{
  typedef nsTArrayElementTraits<ElemType> traits;

  const static bool allowRealloc = false;

  static void CopyElements(void* aDest, void* aSrc, size_t aCount,
                           size_t aElemSize)
  {
    ElemType* destElem = static_cast<ElemType*>(aDest);
    ElemType* srcElem = static_cast<ElemType*>(aSrc);
    ElemType* destElemEnd = destElem + aCount;
#ifdef DEBUG
    ElemType* srcElemEnd = srcElem + aCount;
    MOZ_ASSERT(srcElemEnd <= destElem || srcElemEnd > destElemEnd);
#endif
    while (destElem != destElemEnd) {
      traits::Construct(destElem, *srcElem);
      traits::Destruct(srcElem);
      ++destElem;
      ++srcElem;
    }
  }

  static void CopyHeaderAndElements(void* aDest, void* aSrc, size_t aCount,
                                    size_t aElemSize)
  {
    nsTArrayHeader* destHeader = static_cast<nsTArrayHeader*>(aDest);
    nsTArrayHeader* srcHeader = static_cast<nsTArrayHeader*>(aSrc);
    *destHeader = *srcHeader;
    CopyElements(static_cast<uint8_t*>(aDest) + sizeof(nsTArrayHeader),
                 static_cast<uint8_t*>(aSrc) + sizeof(nsTArrayHeader),
                 aCount, aElemSize);
  }

  static void MoveElements(void* aDest, void* aSrc, size_t aCount,
                           size_t aElemSize)
  {
    ElemType* destElem = static_cast<ElemType*>(aDest);
    ElemType* srcElem = static_cast<ElemType*>(aSrc);
    ElemType* destElemEnd = destElem + aCount;
    ElemType* srcElemEnd = srcElem + aCount;
    if (destElem == srcElem) {
      return;  // In practice, we don't do this.
    } else if (srcElemEnd > destElem && srcElemEnd < destElemEnd) {
      while (destElemEnd != destElem) {
        --destElemEnd;
        --srcElemEnd;
        traits::Construct(destElemEnd, *srcElemEnd);
        traits::Destruct(srcElem);
      }
    } else {
      CopyElements(aDest, aSrc, aCount, aElemSize);
    }
  }
};

//
// The default behaviour is to use memcpy/memmove for everything.
//
template<class E>
struct nsTArray_CopyChooser
{
  typedef nsTArray_CopyWithMemutils Type;
};

//
// Some classes require constructors/destructors to be called, so they are
// specialized here.
//
template<class E>
struct nsTArray_CopyChooser<JS::Heap<E>>
{
  typedef nsTArray_CopyWithConstructors<JS::Heap<E>> Type;
};

template<>
struct nsTArray_CopyChooser<nsRegion>
{
  typedef nsTArray_CopyWithConstructors<nsRegion> Type;
};

template<>
struct nsTArray_CopyChooser<nsIntRegion>
{
  typedef nsTArray_CopyWithConstructors<nsIntRegion> Type;
};

//
// Base class for nsTArray_Impl that is templated on element type and derived
// nsTArray_Impl class, to allow extra conversions to be added for specific
// types.
//
template<class E, class Derived>
struct nsTArray_TypedBase : public nsTArray_SafeElementAtHelper<E, Derived>
{
};

//
// Specialization of nsTArray_TypedBase for arrays containing JS::Heap<E>
// elements.
//
// These conversions are safe because JS::Heap<E> and E share the same
// representation, and since the result of the conversions are const references
// we won't miss any barriers.
//
// The static_cast is necessary to obtain the correct address for the derived
// class since we are a base class used in multiple inheritance.
//
template<class E, class Derived>
struct nsTArray_TypedBase<JS::Heap<E>, Derived>
  : public nsTArray_SafeElementAtHelper<JS::Heap<E>, Derived>
{
  operator const nsTArray<E>&()
  {
    static_assert(sizeof(E) == sizeof(JS::Heap<E>),
                  "JS::Heap<E> must be binary compatible with E.");
    Derived* self = static_cast<Derived*>(this);
    return *reinterpret_cast<nsTArray<E> *>(self);
  }

  operator const FallibleTArray<E>&()
  {
    Derived* self = static_cast<Derived*>(this);
    return *reinterpret_cast<FallibleTArray<E> *>(self);
  }
};


//
// nsTArray_Impl contains most of the guts supporting nsTArray, FallibleTArray,
// nsAutoTArray, and AutoFallibleTArray.
//
// The only situation in which you might need to use nsTArray_Impl in your code
// is if you're writing code which mutates a TArray which may or may not be
// infallible.
//
// Code which merely reads from a TArray which may or may not be infallible can
// simply cast the TArray to |const nsTArray&|; both fallible and infallible
// TArrays can be cast to |const nsTArray&|.
//
template<class E, class Alloc>
class nsTArray_Impl
  : public nsTArray_base<Alloc, typename nsTArray_CopyChooser<E>::Type>
  , public nsTArray_TypedBase<E, nsTArray_Impl<E, Alloc>>
{
public:
  typedef typename nsTArray_CopyChooser<E>::Type     copy_type;
  typedef nsTArray_base<Alloc, copy_type>            base_type;
  typedef typename base_type::size_type              size_type;
  typedef typename base_type::index_type             index_type;
  typedef E                                          elem_type;
  typedef nsTArray_Impl<E, Alloc>                    self_type;
  typedef nsTArrayElementTraits<E>                   elem_traits;
  typedef nsTArray_SafeElementAtHelper<E, self_type> safeelementat_helper_type;

  using safeelementat_helper_type::SafeElementAt;
  using base_type::EmptyHdr;

  // A special value that is used to indicate an invalid or unknown index
  // into the array.
  static const index_type NoIndex = index_type(-1);

  using base_type::Length;

  //
  // Finalization method
  //

  ~nsTArray_Impl() { Clear(); }

  //
  // Initialization methods
  //

  nsTArray_Impl() {}

  // Initialize this array and pre-allocate some number of elements.
  explicit nsTArray_Impl(size_type aCapacity) { SetCapacity(aCapacity); }

  // The array's copy-constructor performs a 'deep' copy of the given array.
  // @param aOther The array object to copy.
  //
  // It's very important that we declare this method as taking |const
  // self_type&| as opposed to taking |const nsTArray_Impl<E, OtherAlloc>| for
  // an arbitrary OtherAlloc.
  //
  // If we don't declare a constructor taking |const self_type&|, C++ generates
  // a copy-constructor for this class which merely copies the object's
  // members, which is obviously wrong.
  //
  // You can pass an nsTArray_Impl<E, OtherAlloc> to this method because
  // nsTArray_Impl<E, X> can be cast to const nsTArray_Impl<E, Y>&.  So the
  // effect on the API is the same as if we'd declared this method as taking
  // |const nsTArray_Impl<E, OtherAlloc>&|.
  explicit nsTArray_Impl(const self_type& aOther) { AppendElements(aOther); }

  // Allow converting to a const array with a different kind of allocator,
  // Since the allocator doesn't matter for const arrays
  template<typename Allocator>
  operator const nsTArray_Impl<E, Allocator>&() const
  {
    return *reinterpret_cast<const nsTArray_Impl<E, Allocator>*>(this);
  }
  // And we have to do this for our subclasses too
  operator const nsTArray<E>&() const
  {
    return *reinterpret_cast<const InfallibleTArray<E>*>(this);
  }
  operator const FallibleTArray<E>&() const
  {
    return *reinterpret_cast<const FallibleTArray<E>*>(this);
  }

  // The array's assignment operator performs a 'deep' copy of the given
  // array.  It is optimized to reuse existing storage if possible.
  // @param aOther The array object to copy.
  self_type& operator=(const self_type& aOther)
  {
    ReplaceElementsAt(0, Length(), aOther.Elements(), aOther.Length());
    return *this;
  }

  // Return true if this array has the same length and the same
  // elements as |aOther|.
  template<typename Allocator>
  bool operator==(const nsTArray_Impl<E, Allocator>& aOther) const
  {
    size_type len = Length();
    if (len != aOther.Length()) {
      return false;
    }

    // XXX std::equal would be as fast or faster here
    for (index_type i = 0; i < len; ++i) {
      if (!(operator[](i) == aOther[i])) {
        return false;
      }
    }

    return true;
  }

  // Return true if this array does not have the same length and the same
  // elements as |aOther|.
  bool operator!=(const self_type& aOther) const { return !operator==(aOther); }

  template<typename Allocator>
  self_type& operator=(const nsTArray_Impl<E, Allocator>& aOther)
  {
    ReplaceElementsAt(0, Length(), aOther.Elements(), aOther.Length());
    return *this;
  }

  // @return The amount of memory used by this nsTArray_Impl, excluding
  // sizeof(*this).
  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
  {
    if (this->UsesAutoArrayBuffer() || Hdr() == EmptyHdr()) {
      return 0;
    }
    return aMallocSizeOf(this->Hdr());
  }

  // @return The amount of memory used by this nsTArray_Impl, including
  // sizeof(*this).
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  //
  // Accessor methods
  //

  // This method provides direct access to the array elements.
  // @return A pointer to the first element of the array.  If the array is
  // empty, then this pointer must not be dereferenced.
  elem_type* Elements() { return reinterpret_cast<elem_type*>(Hdr() + 1); }

  // This method provides direct, readonly access to the array elements.
  // @return A pointer to the first element of the array.  If the array is
  // empty, then this pointer must not be dereferenced.
  const elem_type* Elements() const
  {
    return reinterpret_cast<const elem_type*>(Hdr() + 1);
  }

  // This method provides direct access to an element of the array. The given
  // index must be within the array bounds.
  // @param aIndex The index of an element in the array.
  // @return A reference to the i'th element of the array.
  elem_type& ElementAt(index_type aIndex)
  {
    MOZ_ASSERT(aIndex < Length(), "invalid array index");
    return Elements()[aIndex];
  }

  // This method provides direct, readonly access to an element of the array
  // The given index must be within the array bounds.
  // @param aIndex The index of an element in the array.
  // @return A const reference to the i'th element of the array.
  const elem_type& ElementAt(index_type aIndex) const
  {
    MOZ_ASSERT(aIndex < Length(), "invalid array index");
    return Elements()[aIndex];
  }

  // This method provides direct access to an element of the array in a bounds
  // safe manner. If the requested index is out of bounds the provided default
  // value is returned.
  // @param aIndex The index of an element in the array.
  // @param aDef   The value to return if the index is out of bounds.
  elem_type& SafeElementAt(index_type aIndex, elem_type& aDef)
  {
    return aIndex < Length() ? Elements()[aIndex] : aDef;
  }

  // This method provides direct access to an element of the array in a bounds
  // safe manner. If the requested index is out of bounds the provided default
  // value is returned.
  // @param aIndex The index of an element in the array.
  // @param aDef   The value to return if the index is out of bounds.
  const elem_type& SafeElementAt(index_type aIndex, const elem_type& aDef) const
  {
    return aIndex < Length() ? Elements()[aIndex] : aDef;
  }

  // Shorthand for ElementAt(aIndex)
  elem_type& operator[](index_type aIndex) { return ElementAt(aIndex); }

  // Shorthand for ElementAt(aIndex)
  const elem_type& operator[](index_type aIndex) const { return ElementAt(aIndex); }

  // Shorthand for ElementAt(length - 1)
  elem_type& LastElement() { return ElementAt(Length() - 1); }

  // Shorthand for ElementAt(length - 1)
  const elem_type& LastElement() const { return ElementAt(Length() - 1); }

  // Shorthand for SafeElementAt(length - 1, def)
  elem_type& SafeLastElement(elem_type& aDef)
  {
    return SafeElementAt(Length() - 1, aDef);
  }

  // Shorthand for SafeElementAt(length - 1, def)
  const elem_type& SafeLastElement(const elem_type& aDef) const
  {
    return SafeElementAt(Length() - 1, aDef);
  }

  //
  // Search methods
  //

  // This method searches for the first element in this array that is equal
  // to the given element.
  // @param aItem  The item to search for.
  // @param aComp  The Comparator used to determine element equality.
  // @return       true if the element was found.
  template<class Item, class Comparator>
  bool Contains(const Item& aItem, const Comparator& aComp) const
  {
    return IndexOf(aItem, 0, aComp) != NoIndex;
  }

  // This method searches for the first element in this array that is equal
  // to the given element.  This method assumes that 'operator==' is defined
  // for elem_type.
  // @param aItem  The item to search for.
  // @return       true if the element was found.
  template<class Item>
  bool Contains(const Item& aItem) const
  {
    return IndexOf(aItem) != NoIndex;
  }

  // This method searches for the offset of the first element in this
  // array that is equal to the given element.
  // @param aItem  The item to search for.
  // @param aStart The index to start from.
  // @param aComp  The Comparator used to determine element equality.
  // @return       The index of the found element or NoIndex if not found.
  template<class Item, class Comparator>
  index_type IndexOf(const Item& aItem, index_type aStart,
                     const Comparator& aComp) const
  {
    const elem_type* iter = Elements() + aStart, *end = Elements() + Length();
    for (; iter != end; ++iter) {
      if (aComp.Equals(*iter, aItem)) {
        return index_type(iter - Elements());
      }
    }
    return NoIndex;
  }

  // This method searches for the offset of the first element in this
  // array that is equal to the given element.  This method assumes
  // that 'operator==' is defined for elem_type.
  // @param aItem  The item to search for.
  // @param aStart The index to start from.
  // @return       The index of the found element or NoIndex if not found.
  template<class Item>
  index_type IndexOf(const Item& aItem, index_type aStart = 0) const
  {
    return IndexOf(aItem, aStart, nsDefaultComparator<elem_type, Item>());
  }

  // This method searches for the offset of the last element in this
  // array that is equal to the given element.
  // @param aItem  The item to search for.
  // @param aStart The index to start from.  If greater than or equal to the
  //               length of the array, then the entire array is searched.
  // @param aComp  The Comparator used to determine element equality.
  // @return       The index of the found element or NoIndex if not found.
  template<class Item, class Comparator>
  index_type LastIndexOf(const Item& aItem, index_type aStart,
                         const Comparator& aComp) const
  {
    size_type endOffset = aStart >= Length() ? Length() : aStart + 1;
    const elem_type* end = Elements() - 1, *iter = end + endOffset;
    for (; iter != end; --iter) {
      if (aComp.Equals(*iter, aItem)) {
        return index_type(iter - Elements());
      }
    }
    return NoIndex;
  }

  // This method searches for the offset of the last element in this
  // array that is equal to the given element.  This method assumes
  // that 'operator==' is defined for elem_type.
  // @param aItem  The item to search for.
  // @param aStart The index to start from.  If greater than or equal to the
  //               length of the array, then the entire array is searched.
  // @return       The index of the found element or NoIndex if not found.
  template<class Item>
  index_type LastIndexOf(const Item& aItem,
                         index_type aStart = NoIndex) const
  {
    return LastIndexOf(aItem, aStart, nsDefaultComparator<elem_type, Item>());
  }

  // This method searches for the offset for the element in this array
  // that is equal to the given element. The array is assumed to be sorted.
  // @param aItem  The item to search for.
  // @param aComp  The Comparator used.
  // @return       The index of the found element or NoIndex if not found.
  template<class Item, class Comparator>
  index_type BinaryIndexOf(const Item& aItem, const Comparator& aComp) const
  {
    index_type low = 0, high = Length();
    while (high > low) {
      index_type mid = (high + low) >> 1;
      if (aComp.Equals(ElementAt(mid), aItem)) {
        return mid;
      }
      if (aComp.LessThan(ElementAt(mid), aItem)) {
        low = mid + 1;
      } else {
        high = mid;
      }
    }
    return NoIndex;
  }

  // This method searches for the offset for the element in this array
  // that is equal to the given element. The array is assumed to be sorted.
  // This method assumes that 'operator==' and 'operator<' are defined.
  // @param aItem  The item to search for.
  // @return       The index of the found element or NoIndex if not found.
  template<class Item>
  index_type BinaryIndexOf(const Item& aItem) const
  {
    return BinaryIndexOf(aItem, nsDefaultComparator<elem_type, Item>());
  }

  //
  // Mutation methods
  //
  // This method call the destructor on each element of the array, empties it,
  // but does not shrink the array's capacity.
  // See also SetLengthAndRetainStorage.
  // Make sure to call Compact() if needed to avoid keeping a huge array
  // around.
  void ClearAndRetainStorage()
  {
    if (base_type::mHdr == EmptyHdr()) {
      return;
    }

    DestructRange(0, Length());
    base_type::mHdr->mLength = 0;
  }

  // This method modifies the length of the array, but unlike SetLength
  // it doesn't deallocate/reallocate the current internal storage.
  // The new length MUST be shorter than or equal to the current capacity.
  // If the new length is larger than the existing length of the array,
  // then new elements will be constructed using elem_type's default
  // constructor.  If shorter, elements will be destructed and removed.
  // See also ClearAndRetainStorage.
  // @param aNewLen  The desired length of this array.
  void SetLengthAndRetainStorage(size_type aNewLen)
  {
    MOZ_ASSERT(aNewLen <= base_type::Capacity());
    size_type oldLen = Length();
    if (aNewLen > oldLen) {
      InsertElementsAt(oldLen, aNewLen - oldLen);
      return;
    }
    if (aNewLen < oldLen) {
      DestructRange(aNewLen, oldLen - aNewLen);
      base_type::mHdr->mLength = aNewLen;
    }
  }

  // This method replaces a range of elements in this array.
  // @param aStart    The starting index of the elements to replace.
  // @param aCount    The number of elements to replace.  This may be zero to
  //                  insert elements without removing any existing elements.
  // @param aArray    The values to copy into this array.  Must be non-null,
  //                  and these elements must not already exist in the array
  //                  being modified.
  // @param aArrayLen The number of values to copy into this array.
  // @return          A pointer to the new elements in the array, or null if
  //                  the operation failed due to insufficient memory.
  template<class Item>
  elem_type* ReplaceElementsAt(index_type aStart, size_type aCount,
                               const Item* aArray, size_type aArrayLen)
  {
    // Adjust memory allocation up-front to catch errors.
    if (!Alloc::Successful(this->EnsureCapacity(Length() + aArrayLen - aCount,
                                                sizeof(elem_type))))
      return nullptr;
    DestructRange(aStart, aCount);
    this->ShiftData(aStart, aCount, aArrayLen,
                    sizeof(elem_type), MOZ_ALIGNOF(elem_type));
    AssignRange(aStart, aArrayLen, aArray);
    return Elements() + aStart;
  }

  // A variation on the ReplaceElementsAt method defined above.
  template<class Item>
  elem_type* ReplaceElementsAt(index_type aStart, size_type aCount,
                               const nsTArray<Item>& aArray)
  {
    return ReplaceElementsAt(aStart, aCount, aArray.Elements(), aArray.Length());
  }

  // A variation on the ReplaceElementsAt method defined above.
  template<class Item>
  elem_type* ReplaceElementsAt(index_type aStart, size_type aCount,
                               const Item& aItem)
  {
    return ReplaceElementsAt(aStart, aCount, &aItem, 1);
  }

  // A variation on the ReplaceElementsAt method defined above.
  template<class Item>
  elem_type* ReplaceElementAt(index_type aIndex, const Item& aItem)
  {
    return ReplaceElementsAt(aIndex, 1, &aItem, 1);
  }

  // A variation on the ReplaceElementsAt method defined above.
  template<class Item>
  elem_type* InsertElementsAt(index_type aIndex, const Item* aArray,
                              size_type aArrayLen)
  {
    return ReplaceElementsAt(aIndex, 0, aArray, aArrayLen);
  }

  // A variation on the ReplaceElementsAt method defined above.
  template<class Item, class Allocator>
  elem_type* InsertElementsAt(index_type aIndex,
                              const nsTArray_Impl<Item, Allocator>& aArray)
  {
    return ReplaceElementsAt(aIndex, 0, aArray.Elements(), aArray.Length());
  }

  // A variation on the ReplaceElementsAt method defined above.
  template<class Item>
  elem_type* InsertElementAt(index_type aIndex, const Item& aItem)
  {
    return ReplaceElementsAt(aIndex, 0, &aItem, 1);
  }

  // Insert a new element without copy-constructing. This is useful to avoid
  // temporaries.
  // @return A pointer to the newly inserted element, or null on OOM.
  elem_type* InsertElementAt(index_type aIndex)
  {
    if (!Alloc::Successful(this->EnsureCapacity(Length() + 1,
                                                sizeof(elem_type))))
      return nullptr;
    this->ShiftData(aIndex, 0, 1, sizeof(elem_type), MOZ_ALIGNOF(elem_type));
    elem_type* elem = Elements() + aIndex;
    elem_traits::Construct(elem);
    return elem;
  }

  // This method searches for the smallest index of an element that is strictly
  // greater than |aItem|. If |aItem| is inserted at this index, the array will
  // remain sorted and |aItem| would come after all elements that are equal to
  // it. If |aItem| is greater than or equal to all elements in the array, the
  // array length is returned.
  //
  // Note that consumers who want to know whether there are existing items equal
  // to |aItem| in the array can just check that the return value here is > 0
  // and indexing into the previous slot gives something equal to |aItem|.
  //
  //
  // @param aItem  The item to search for.
  // @param aComp  The Comparator used.
  // @return        The index of greatest element <= to |aItem|
  // @precondition The array is sorted
  template<class Item, class Comparator>
  index_type IndexOfFirstElementGt(const Item& aItem,
                                   const Comparator& aComp) const
  {
    // invariant: low <= [idx] <= high
    index_type low = 0, high = Length();
    while (high > low) {
      index_type mid = (high + low) >> 1;
      // Comparators are not required to provide a LessThan(Item&, elem_type),
      // so we can't do aComp.LessThan(aItem, ElementAt(mid)).
      if (aComp.LessThan(ElementAt(mid), aItem) ||
          aComp.Equals(ElementAt(mid), aItem)) {
        // aItem >= ElementAt(mid), so our desired index is at least mid+1.
        low = mid + 1;
      } else {
        // aItem < ElementAt(mid).  Our desired index is therefore at most mid.
        high = mid;
      }
    }
    MOZ_ASSERT(high == low);
    return low;
  }

  // A variation on the IndexOfFirstElementGt method defined above.
  template<class Item>
  index_type
  IndexOfFirstElementGt(const Item& aItem) const
  {
    return IndexOfFirstElementGt(aItem, nsDefaultComparator<elem_type, Item>());
  }

  // Inserts |aItem| at such an index to guarantee that if the array
  // was previously sorted, it will remain sorted after this
  // insertion.
  template<class Item, class Comparator>
  elem_type* InsertElementSorted(const Item& aItem, const Comparator& aComp)
  {
    index_type index = IndexOfFirstElementGt(aItem, aComp);
    return InsertElementAt(index, aItem);
  }

  // A variation on the InsertElementSorted method defined above.
  template<class Item>
  elem_type* InsertElementSorted(const Item& aItem)
  {
    return InsertElementSorted(aItem, nsDefaultComparator<elem_type, Item>());
  }

  // This method appends elements to the end of this array.
  // @param aArray    The elements to append to this array.
  // @param aArrayLen The number of elements to append to this array.
  // @return          A pointer to the new elements in the array, or null if
  //                  the operation failed due to insufficient memory.
  template<class Item>
  elem_type* AppendElements(const Item* aArray, size_type aArrayLen)
  {
    if (!Alloc::Successful(this->EnsureCapacity(Length() + aArrayLen,
                                                sizeof(elem_type))))
      return nullptr;
    index_type len = Length();
    AssignRange(len, aArrayLen, aArray);
    this->IncrementLength(aArrayLen);
    return Elements() + len;
  }

  // A variation on the AppendElements method defined above.
  template<class Item, class Allocator>
  elem_type* AppendElements(const nsTArray_Impl<Item, Allocator>& aArray)
  {
    return AppendElements(aArray.Elements(), aArray.Length());
  }

  // A variation on the AppendElements method defined above.
  template<class Item>
  elem_type* AppendElement(const Item& aItem)
  {
    return AppendElements(&aItem, 1);
  }

  // Append new elements without copy-constructing. This is useful to avoid
  // temporaries.
  // @return A pointer to the newly appended elements, or null on OOM.
  elem_type* AppendElements(size_type aCount)
  {
    if (!Alloc::Successful(this->EnsureCapacity(Length() + aCount,
                                                sizeof(elem_type))))
      return nullptr;
    elem_type* elems = Elements() + Length();
    size_type i;
    for (i = 0; i < aCount; ++i) {
      elem_traits::Construct(elems + i);
    }
    this->IncrementLength(aCount);
    return elems;
  }

  // Append a new element without copy-constructing. This is useful to avoid
  // temporaries.
  // @return A pointer to the newly appended element, or null on OOM.
  elem_type* AppendElement() { return AppendElements(1); }

  // Move all elements from another array to the end of this array without
  // calling copy constructors or destructors.
  // @return A pointer to the newly appended elements, or null on OOM.
  template<class Item, class Allocator>
  elem_type* MoveElementsFrom(nsTArray_Impl<Item, Allocator>& aArray)
  {
    MOZ_ASSERT(&aArray != this, "argument must be different aArray");
    index_type len = Length();
    index_type otherLen = aArray.Length();
    if (!Alloc::Successful(this->EnsureCapacity(len + otherLen,
                                                sizeof(elem_type))))
      return nullptr;
    copy_type::CopyElements(Elements() + len, aArray.Elements(), otherLen,
                            sizeof(elem_type));
    this->IncrementLength(otherLen);
    aArray.ShiftData(0, otherLen, 0, sizeof(elem_type), MOZ_ALIGNOF(elem_type));
    return Elements() + len;
  }

  // This method removes a range of elements from this array.
  // @param aStart The starting index of the elements to remove.
  // @param aCount The number of elements to remove.
  void RemoveElementsAt(index_type aStart, size_type aCount)
  {
    MOZ_ASSERT(aCount == 0 || aStart < Length(), "Invalid aStart index");
    MOZ_ASSERT(aStart + aCount <= Length(), "Invalid length");
    // Check that the previous assert didn't overflow
    MOZ_ASSERT(aStart <= aStart + aCount, "Start index plus length overflows");
    DestructRange(aStart, aCount);
    this->ShiftData(aStart, aCount, 0,
                    sizeof(elem_type), MOZ_ALIGNOF(elem_type));
  }

  // A variation on the RemoveElementsAt method defined above.
  void RemoveElementAt(index_type aIndex) { RemoveElementsAt(aIndex, 1); }

  // A variation on the RemoveElementsAt method defined above.
  void Clear() { RemoveElementsAt(0, Length()); }

  // This helper function combines IndexOf with RemoveElementAt to "search
  // and destroy" the first element that is equal to the given element.
  // @param aItem The item to search for.
  // @param aComp The Comparator used to determine element equality.
  // @return true if the element was found
  template<class Item, class Comparator>
  bool RemoveElement(const Item& aItem, const Comparator& aComp)
  {
    index_type i = IndexOf(aItem, 0, aComp);
    if (i == NoIndex) {
      return false;
    }

    RemoveElementAt(i);
    return true;
  }

  // A variation on the RemoveElement method defined above that assumes
  // that 'operator==' is defined for elem_type.
  template<class Item>
  bool RemoveElement(const Item& aItem)
  {
    return RemoveElement(aItem, nsDefaultComparator<elem_type, Item>());
  }

  // This helper function combines IndexOfFirstElementGt with
  // RemoveElementAt to "search and destroy" the last element that
  // is equal to the given element.
  // @param aItem The item to search for.
  // @param aComp The Comparator used to determine element equality.
  // @return true if the element was found
  template<class Item, class Comparator>
  bool RemoveElementSorted(const Item& aItem, const Comparator& aComp)
  {
    index_type index = IndexOfFirstElementGt(aItem, aComp);
    if (index > 0 && aComp.Equals(ElementAt(index - 1), aItem)) {
      RemoveElementAt(index - 1);
      return true;
    }
    return false;
  }

  // A variation on the RemoveElementSorted method defined above.
  template<class Item>
  bool RemoveElementSorted(const Item& aItem)
  {
    return RemoveElementSorted(aItem, nsDefaultComparator<elem_type, Item>());
  }

  // This method causes the elements contained in this array and the given
  // array to be swapped.
  template<class Allocator>
  typename Alloc::ResultType SwapElements(nsTArray_Impl<E, Allocator>& aOther)
  {
    return Alloc::Result(this->SwapArrayElements(aOther, sizeof(elem_type),
                                                 MOZ_ALIGNOF(elem_type)));
  }

  //
  // Allocation
  //

  // This method may increase the capacity of this array object by the
  // specified amount.  This method may be called in advance of several
  // AppendElement operations to minimize heap re-allocations.  This method
  // will not reduce the number of elements in this array.
  // @param aCapacity The desired capacity of this array.
  // @return True if the operation succeeded; false if we ran out of memory
  typename Alloc::ResultType SetCapacity(size_type aCapacity)
  {
    return Alloc::Result(this->EnsureCapacity(aCapacity, sizeof(elem_type)));
  }

  // This method modifies the length of the array.  If the new length is
  // larger than the existing length of the array, then new elements will be
  // constructed using elem_type's default constructor.  Otherwise, this call
  // removes elements from the array (see also RemoveElementsAt).
  // @param aNewLen The desired length of this array.
  // @return True if the operation succeeded; false otherwise.
  // See also TruncateLength if the new length is guaranteed to be smaller than 
  // the old.
  typename Alloc::ResultType SetLength(size_type aNewLen)
  {
    size_type oldLen = Length();
    if (aNewLen > oldLen) {
      return Alloc::ConvertBoolToResultType(
        InsertElementsAt(oldLen, aNewLen - oldLen) != nullptr);
    }

    TruncateLength(aNewLen);
    return Alloc::ConvertBoolToResultType(true);
  }

  // This method modifies the length of the array, but may only be
  // called when the new length is shorter than the old.  It can
  // therefore be called when elem_type has no default constructor,
  // unlike SetLength.  It removes elements from the array (see also
  // RemoveElementsAt).
  // @param aNewLen The desired length of this array.
  void TruncateLength(size_type aNewLen)
  {
    size_type oldLen = Length();
    NS_ABORT_IF_FALSE(aNewLen <= oldLen,
                      "caller should use SetLength instead");
    RemoveElementsAt(aNewLen, oldLen - aNewLen);
  }

  // This method ensures that the array has length at least the given
  // length.  If the current length is shorter than the given length,
  // then new elements will be constructed using elem_type's default
  // constructor.
  // @param aMinLen The desired minimum length of this array.
  // @return True if the operation succeeded; false otherwise.
  typename Alloc::ResultType EnsureLengthAtLeast(size_type aMinLen)
  {
    size_type oldLen = Length();
    if (aMinLen > oldLen) {
      return Alloc::ConvertBoolToResultType(!!InsertElementsAt(oldLen,
                                                               aMinLen - oldLen));
    }
    return Alloc::ConvertBoolToResultType(true);
  }

  // This method inserts elements into the array, constructing
  // them using elem_type's default constructor.
  // @param aIndex the place to insert the new elements. This must be no
  //               greater than the current length of the array.
  // @param aCount the number of elements to insert
  elem_type* InsertElementsAt(index_type aIndex, size_type aCount)
  {
    if (!base_type::InsertSlotsAt(aIndex, aCount, sizeof(elem_type),
                                  MOZ_ALIGNOF(elem_type))) {
      return nullptr;
    }

    // Initialize the extra array elements
    elem_type* iter = Elements() + aIndex, *end = iter + aCount;
    for (; iter != end; ++iter) {
      elem_traits::Construct(iter);
    }

    return Elements() + aIndex;
  }

  // This method inserts elements into the array, constructing them
  // elem_type's copy constructor (or whatever one-arg constructor
  // happens to match the Item type).
  // @param aIndex the place to insert the new elements. This must be no
  //               greater than the current length of the array.
  // @param aCount the number of elements to insert.
  // @param aItem the value to use when constructing the new elements.
  template<class Item>
  elem_type* InsertElementsAt(index_type aIndex, size_type aCount,
                              const Item& aItem)
  {
    if (!base_type::InsertSlotsAt(aIndex, aCount, sizeof(elem_type),
                                  MOZ_ALIGNOF(elem_type))) {
      return nullptr;
    }

    // Initialize the extra array elements
    elem_type* iter = Elements() + aIndex, *end = iter + aCount;
    for (; iter != end; ++iter) {
      elem_traits::Construct(iter, aItem);
    }

    return Elements() + aIndex;
  }

  // This method may be called to minimize the memory used by this array.
  void Compact()
  {
    ShrinkCapacity(sizeof(elem_type), MOZ_ALIGNOF(elem_type));
  }

  //
  // Sorting
  //

  // This function is meant to be used with the NS_QuickSort function.  It
  // maps the callback API expected by NS_QuickSort to the Comparator API
  // used by nsTArray_Impl.  See nsTArray_Impl::Sort.
  template<class Comparator>
  static int Compare(const void* aE1, const void* aE2, void* aData)
  {
    const Comparator* c = reinterpret_cast<const Comparator*>(aData);
    const elem_type* a = static_cast<const elem_type*>(aE1);
    const elem_type* b = static_cast<const elem_type*>(aE2);
    return c->LessThan(*a, *b) ? -1 : (c->Equals(*a, *b) ? 0 : 1);
  }

  // This method sorts the elements of the array.  It uses the LessThan
  // method defined on the given Comparator object to collate elements.
  // @param aComp The Comparator used to collate elements.
  template<class Comparator>
  void Sort(const Comparator& aComp)
  {
    NS_QuickSort(Elements(), Length(), sizeof(elem_type),
                 Compare<Comparator>, const_cast<Comparator*>(&aComp));
  }

  // A variation on the Sort method defined above that assumes that
  // 'operator<' is defined for elem_type.
  void Sort() { Sort(nsDefaultComparator<elem_type, elem_type>()); }

  //
  // Binary Heap
  //

  // Sorts the array into a binary heap.
  // @param aComp The Comparator used to create the heap
  template<class Comparator>
  void MakeHeap(const Comparator& aComp)
  {
    if (!Length()) {
      return;
    }
    index_type index = (Length() - 1) / 2;
    do {
      SiftDown(index, aComp);
    } while (index--);
  }

  // A variation on the MakeHeap method defined above.
  void MakeHeap()
  {
    MakeHeap(nsDefaultComparator<elem_type, elem_type>());
  }

  // Adds an element to the heap
  // @param aItem The item to add
  // @param aComp The Comparator used to sift-up the item
  template<class Item, class Comparator>
  elem_type* PushHeap(const Item& aItem, const Comparator& aComp)
  {
    if (!base_type::InsertSlotsAt(Length(), 1, sizeof(elem_type),
                                  MOZ_ALIGNOF(elem_type))) {
      return nullptr;
    }
    // Sift up the new node
    elem_type* elem = Elements();
    index_type index = Length() - 1;
    index_type parent_index = (index - 1) / 2;
    while (index && aComp.LessThan(elem[parent_index], aItem)) {
      elem[index] = elem[parent_index];
      index = parent_index;
      parent_index = (index - 1) / 2;
    }
    elem[index] = aItem;
    return &elem[index];
  }

  // A variation on the PushHeap method defined above.
  template<class Item>
  elem_type* PushHeap(const Item& aItem)
  {
    return PushHeap(aItem, nsDefaultComparator<elem_type, Item>());
  }

  // Delete the root of the heap and restore the heap
  // @param aComp The Comparator used to restore the heap
  template<class Comparator>
  void PopHeap(const Comparator& aComp)
  {
    if (!Length()) {
      return;
    }
    index_type last_index = Length() - 1;
    elem_type* elem = Elements();
    elem[0] = elem[last_index];
    TruncateLength(last_index);
    if (Length()) {
      SiftDown(0, aComp);
    }
  }

  // A variation on the PopHeap method defined above.
  void PopHeap()
  {
    PopHeap(nsDefaultComparator<elem_type, elem_type>());
  }

protected:
  using base_type::Hdr;
  using base_type::ShrinkCapacity;

  // This method invokes elem_type's destructor on a range of elements.
  // @param aStart The index of the first element to destroy.
  // @param aCount The number of elements to destroy.
  void DestructRange(index_type aStart, size_type aCount)
  {
    elem_type* iter = Elements() + aStart, *end = iter + aCount;
    for (; iter != end; ++iter) {
      elem_traits::Destruct(iter);
    }
  }

  // This method invokes elem_type's copy-constructor on a range of elements.
  // @param aStart  The index of the first element to construct.
  // @param aCount  The number of elements to construct.
  // @param aValues The array of elements to copy.
  template<class Item>
  void AssignRange(index_type aStart, size_type aCount, const Item* aValues)
  {
    AssignRangeAlgorithm<mozilla::IsPod<Item>::value,
                         mozilla::IsSame<Item, elem_type>::value>
                         ::implementation(Elements(), aStart, aCount, aValues);
  }

  // This method sifts an item down to its proper place in a binary heap
  // @param aIndex The index of the node to start sifting down from
  // @param aComp  The Comparator used to sift down
  template<class Comparator>
  void SiftDown(index_type aIndex, const Comparator& aComp)
  {
    elem_type* elem = Elements();
    elem_type item = elem[aIndex];
    index_type end = Length() - 1;
    while ((aIndex * 2) < end) {
      const index_type left = (aIndex * 2) + 1;
      const index_type right = (aIndex * 2) + 2;
      const index_type parent_index = aIndex;
      if (aComp.LessThan(item, elem[left])) {
        if (left < end &&
            aComp.LessThan(elem[left], elem[right])) {
          aIndex = right;
        } else {
          aIndex = left;
        }
      } else if (left < end &&
                 aComp.LessThan(item, elem[right])) {
        aIndex = right;
      } else {
        break;
      }
      elem[parent_index] = elem[aIndex];
    }
    elem[aIndex] = item;
  }
};

template<typename E, typename Alloc>
inline void
ImplCycleCollectionUnlink(nsTArray_Impl<E, Alloc>& aField)
{
  aField.Clear();
}

template<typename E, typename Alloc>
inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            nsTArray_Impl<E, Alloc>& aField,
                            const char* aName,
                            uint32_t aFlags = 0)
{
  aFlags |= CycleCollectionEdgeNameArrayFlag;
  size_t length = aField.Length();
  for (size_t i = 0; i < length; ++i) {
    ImplCycleCollectionTraverse(aCallback, aField[i], aName, aFlags);
  }
}

//
// nsTArray is an infallible vector class.  See the comment at the top of this
// file for more details.
//
template<class E>
class nsTArray : public nsTArray_Impl<E, nsTArrayInfallibleAllocator>
{
public:
  typedef nsTArray_Impl<E, nsTArrayInfallibleAllocator> base_type;
  typedef nsTArray<E>                                   self_type;
  typedef typename base_type::size_type                 size_type;

  nsTArray() {}
  explicit nsTArray(size_type aCapacity) : base_type(aCapacity) {}
  explicit nsTArray(const nsTArray& aOther) : base_type(aOther) {}

  template<class Allocator>
  explicit nsTArray(const nsTArray_Impl<E, Allocator>& aOther)
    : base_type(aOther)
  {
  }
};

//
// FallibleTArray is a fallible vector class.
//
template<class E>
class FallibleTArray : public nsTArray_Impl<E, nsTArrayFallibleAllocator>
{
public:
  typedef nsTArray_Impl<E, nsTArrayFallibleAllocator>   base_type;
  typedef FallibleTArray<E>                             self_type;
  typedef typename base_type::size_type                 size_type;

  FallibleTArray() {}
  explicit FallibleTArray(size_type aCapacity) : base_type(aCapacity) {}
  explicit FallibleTArray(const FallibleTArray<E>& aOther) : base_type(aOther) {}

  template<class Allocator>
  explicit FallibleTArray(const nsTArray_Impl<E, Allocator>& aOther)
    : base_type(aOther)
  {
  }
};

//
// nsAutoArrayBase is a base class for AutoFallibleTArray and nsAutoTArray.
// You shouldn't use this class directly.
//
template<class TArrayBase, size_t N>
class nsAutoArrayBase : public TArrayBase
{
  static_assert(N != 0, "nsAutoArrayBase<TArrayBase, 0> should be specialized");
public:
  typedef nsAutoArrayBase<TArrayBase, N> self_type;
  typedef TArrayBase base_type;
  typedef typename base_type::Header Header;
  typedef typename base_type::elem_type elem_type;

  template<typename Allocator>
  self_type& operator=(const nsTArray_Impl<elem_type, Allocator>& aOther)
  {
    base_type::operator=(aOther);
    return *this;
  }

protected:
  nsAutoArrayBase() { Init(); }

  // We need this constructor because nsAutoTArray and friends all have
  // implicit copy-constructors.  If we don't have this method, those
  // copy-constructors will call nsAutoArrayBase's implicit copy-constructor,
  // which won't call Init() and set up the auto buffer!
  nsAutoArrayBase(const self_type& aOther)
  {
    Init();
    this->AppendElements(aOther);
  }

private:
  // nsTArray_base casts itself as an nsAutoArrayBase in order to get a pointer
  // to mAutoBuf.
  template<class Allocator, class Copier>
  friend class nsTArray_base;

  void Init()
  {
    static_assert(MOZ_ALIGNOF(elem_type) <= 8,
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
  union
  {
    char mAutoBuf[sizeof(nsTArrayHeader) + N * sizeof(elem_type)];
    // Do the max operation inline to ensure that it is a compile-time constant.
    mozilla::AlignedElem<(MOZ_ALIGNOF(Header) > MOZ_ALIGNOF(elem_type)) ?
                         MOZ_ALIGNOF(Header) : MOZ_ALIGNOF(elem_type)> mAlign;
  };
};

//
// Specialization of nsAutoArrayBase<TArrayBase, N> for the case where N == 0.
// nsAutoArrayBase<TArrayBase, 0> behaves exactly like TArrayBase, but without
// this specialization, it stores a useless inline header.
//
// We do have many nsAutoArrayBase<TArrayBase, 0> objects in memory: about
// 2,000 per tab as of May 2014. These are typically not explicitly
// nsAutoArrayBase<TArrayBase, 0> but rather nsAutoArrayBase<TArrayBase, N>
// for some value N depending on template parameters, in generic code.
//
// For that reason, we optimize this case with the below partial specialization,
// which ensures that nsAutoArrayBase<TArrayBase, 0> is just like TArrayBase,
// without any inline header overhead.
//
template<class TArrayBase>
class nsAutoArrayBase<TArrayBase, 0> : public TArrayBase
{
};

//
// nsAutoTArray<E, N> is an infallible vector class with N elements of inline
// storage.  If you try to store more than N elements inside an
// nsAutoTArray<E, N>, we'll call malloc() and store them all on the heap.
//
// Note that you can cast an nsAutoTArray<E, N> to
// |const AutoFallibleTArray<E, N>&|.
//
template<class E, size_t N>
class nsAutoTArray : public nsAutoArrayBase<nsTArray<E>, N>
{
  typedef nsAutoTArray<E, N> self_type;
  typedef nsAutoArrayBase<nsTArray<E>, N> Base;

public:
  nsAutoTArray() {}

  template<typename Allocator>
  explicit nsAutoTArray(const nsTArray_Impl<E, Allocator>& aOther)
  {
    Base::AppendElements(aOther);
  }

  operator const AutoFallibleTArray<E, N>&() const
  {
    return *reinterpret_cast<const AutoFallibleTArray<E, N>*>(this);
  }
};

//
// AutoFallibleTArray<E, N> is a fallible vector class with N elements of
// inline storage.
//
template<class E, size_t N>
class AutoFallibleTArray : public nsAutoArrayBase<FallibleTArray<E>, N>
{
  typedef AutoFallibleTArray<E, N> self_type;
  typedef nsAutoArrayBase<FallibleTArray<E>, N> Base;

public:
  AutoFallibleTArray() {}

  template<typename Allocator>
  explicit AutoFallibleTArray(const nsTArray_Impl<E, Allocator>& aOther)
  {
    Base::AppendElements(aOther);
  }

  operator const nsAutoTArray<E, N>&() const
  {
    return *reinterpret_cast<const nsAutoTArray<E, N>*>(this);
  }
};

// Assert that nsAutoTArray doesn't have any extra padding inside.
//
// It's important that the data stored in this auto array takes up a multiple of
// 8 bytes; e.g. nsAutoTArray<uint32_t, 1> wouldn't work.  Since nsAutoTArray
// contains a pointer, its size must be a multiple of alignof(void*).  (This is
// because any type may be placed into an array, and there's no padding between
// elements of an array.)  The compiler pads the end of the structure to
// enforce this rule.
//
// If we used nsAutoTArray<uint32_t, 1> below, this assertion would fail on a
// 64-bit system, where the compiler inserts 4 bytes of padding at the end of
// the auto array to make its size a multiple of alignof(void*) == 8 bytes.

static_assert(sizeof(nsAutoTArray<uint32_t, 2>) ==
              sizeof(void*) + sizeof(nsTArrayHeader) + sizeof(uint32_t) * 2,
              "nsAutoTArray shouldn't contain any extra padding, "
              "see the comment");

// Definitions of nsTArray_Impl methods
#include "nsTArray-inl.h"

#endif  // nsTArray_h__
