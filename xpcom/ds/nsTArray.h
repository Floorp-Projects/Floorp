/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTArray_h__
#define nsTArray_h__

#include <string.h>

#include <functional>
#include <initializer_list>
#include <new>
#include <ostream>
#include <utility>

#include "mozilla/Alignment.h"
#include "mozilla/ArrayIterator.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/BinarySearch.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/DbgMacro.h"
#include "mozilla/FunctionTypeTraits.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/ReverseIterator.h"
#include "mozilla/Span.h"
#include "mozilla/TypeTraits.h"
#include "mozilla/fallible.h"
#include "mozilla/mozalloc.h"
#include "nsAlgorithm.h"
#include "nsCycleCollectionNoteChild.h"
#include "nsDebug.h"
#include "nsISupportsImpl.h"
#include "nsQuickSort.h"
#include "nsRegionFwd.h"
#include "nsTArrayForwardDeclare.h"
#include "nscore.h"

namespace JS {
template <class T>
class Heap;
} /* namespace JS */

class nsRegion;

namespace mozilla::a11y {
class BatchData;
}

namespace mozilla {
namespace layers {
class Animation;
class FrameStats;
struct PropertyAnimationGroup;
struct TileClient;
struct RenderRootDisplayListData;
struct RenderRootUpdates;
}  // namespace layers
}  // namespace mozilla

namespace mozilla {
struct SerializedStructuredCloneBuffer;
class SourceBufferTask;
}  // namespace mozilla

namespace mozilla::dom::binding_detail {
template <typename, typename>
class RecordEntry;
}

namespace mozilla::dom::ipc {
class StructuredCloneData;
}  // namespace mozilla::dom::ipc

namespace mozilla::dom {
class ClonedMessageData;
class MessageData;
class MessagePortIdentifier;
struct MozPluginParameter;
template <typename T>
struct Nullable;
class OwningFileOrDirectory;
class OwningStringOrBooleanOrObject;
class OwningUTF8StringOrDouble;
class Pref;
class RefMessageData;
class ResponsiveImageCandidate;
class ServiceWorkerRegistrationData;
namespace indexedDB {
class SerializedStructuredCloneReadInfo;
class ObjectStoreCursorResponse;
class IndexCursorResponse;
}  // namespace indexedDB
}  // namespace mozilla::dom

namespace mozilla::ipc {
class AutoIPCStream;
class ContentSecurityPolicy;
}  // namespace mozilla::ipc

class JSStructuredCloneData;

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
//   AutoTArray<T, N>, and
//
// nsTArray and AutoTArray are infallible by default. To opt-in to fallible
// behaviour, use the `mozilla::fallible` parameter and check the return value.
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
// nsTArray_RelocationStrategy below.)
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
struct nsTArrayFallibleResult {
  // Note: allows implicit conversions from and to bool
  MOZ_IMPLICIT nsTArrayFallibleResult(bool aResult) : mResult(aResult) {}

  MOZ_IMPLICIT operator bool() { return mResult; }

 private:
  bool mResult;
};

struct nsTArrayInfallibleResult {};

//
// nsTArray*Allocators must all use the same |free()|, to allow swap()'ing
// between fallible and infallible variants.
//

struct nsTArrayFallibleAllocatorBase {
  typedef bool ResultType;
  typedef nsTArrayFallibleResult ResultTypeProxy;

  static ResultType Result(ResultTypeProxy aResult) { return aResult; }
  static bool Successful(ResultTypeProxy aResult) { return aResult; }
  static ResultTypeProxy SuccessResult() { return true; }
  static ResultTypeProxy FailureResult() { return false; }
  static ResultType ConvertBoolToResultType(bool aValue) { return aValue; }
};

struct nsTArrayInfallibleAllocatorBase {
  typedef void ResultType;
  typedef nsTArrayInfallibleResult ResultTypeProxy;

  static ResultType Result(ResultTypeProxy aResult) {}
  static bool Successful(ResultTypeProxy) { return true; }
  static ResultTypeProxy SuccessResult() { return ResultTypeProxy(); }

  static ResultTypeProxy FailureResult() {
    MOZ_CRASH("Infallible nsTArray should never fail");
    return ResultTypeProxy();
  }

  static ResultType ConvertBoolToResultType(bool aValue) {
    if (!aValue) {
      MOZ_CRASH("infallible nsTArray should never convert false to ResultType");
    }
  }
};

struct nsTArrayFallibleAllocator : nsTArrayFallibleAllocatorBase {
  static void* Malloc(size_t aSize) { return malloc(aSize); }
  static void* Realloc(void* aPtr, size_t aSize) {
    return realloc(aPtr, aSize);
  }

  static void Free(void* aPtr) { free(aPtr); }
  static void SizeTooBig(size_t) {}
};

struct nsTArrayInfallibleAllocator : nsTArrayInfallibleAllocatorBase {
  static void* Malloc(size_t aSize) { return moz_xmalloc(aSize); }
  static void* Realloc(void* aPtr, size_t aSize) {
    return moz_xrealloc(aPtr, aSize);
  }

  static void Free(void* aPtr) { free(aPtr); }
  static void SizeTooBig(size_t aSize) { NS_ABORT_OOM(aSize); }
};

// nsTArray_base stores elements into the space allocated beyond
// sizeof(*this).  This is done to minimize the size of the nsTArray
// object when it is empty.
struct nsTArrayHeader {
  uint32_t mLength;
  uint32_t mCapacity : 31;
  uint32_t mIsAutoArray : 1;
};

extern "C" {
extern nsTArrayHeader sEmptyTArrayHeader;
}

namespace detail {
// SpecializableIsCopyConstructible is a wrapper around
// std::is_copy_constructible, which explicitly disallows defining
// specializations of it. However, we need to specialize it to allow
// instantiating nsTArray for incomplete value types, for which it must be known
// whether the value type is copy-constructible. Specialization should always be
// done using the MOZ_DECLARE_COPY_CONSTRUCTIBLE or
// MOZ_DECLARE_NON_COPY_CONSTRUCTIBLE macros.
template <typename E>
struct SpecializableIsCopyConstructible {
  static constexpr bool Value = std::is_copy_constructible_v<E>;
};

#define MOZ_DECLARE_COPY_CONSTRUCTIBLE(E)      \
  namespace detail {                           \
  template <>                                  \
  struct SpecializableIsCopyConstructible<E> { \
    static constexpr bool Value = true;        \
  };                                           \
  }
#define MOZ_DECLARE_NON_COPY_CONSTRUCTIBLE(E)  \
  namespace detail {                           \
  template <>                                  \
  struct SpecializableIsCopyConstructible<E> { \
    static constexpr bool Value = false;       \
  };                                           \
  }

template <typename E>
constexpr bool SpecializableIsCopyConstructibleValue =
    SpecializableIsCopyConstructible<E>::Value;

// nsTArray_CopyEnabler is used as a base class of nsTArray_Impl to ensure
// nsTArray_Impl only is copy-constructible and copy-assignable if E is
// copy-constructible. nsTArray_Impl never makes use of E's copy assignment
// operator, so the decision is made solely based on E's copy-constructibility.
template <typename E, typename Impl,
          bool IsCopyConstructible = SpecializableIsCopyConstructibleValue<E>>
class nsTArray_CopyEnabler;

template <typename E, typename Impl>
class nsTArray_CopyEnabler<E, Impl, false> {
 public:
  nsTArray_CopyEnabler() = default;

  nsTArray_CopyEnabler(const nsTArray_CopyEnabler&) = delete;
  nsTArray_CopyEnabler& operator=(const nsTArray_CopyEnabler&) = delete;
};

template <typename E, typename Impl>
class nsTArray_CopyEnabler<E, Impl, true> {
 public:
  nsTArray_CopyEnabler() = default;

  nsTArray_CopyEnabler(const nsTArray_CopyEnabler& aOther) {
    static_cast<Impl*>(this)->AppendElements(static_cast<const Impl&>(aOther));
  }

  nsTArray_CopyEnabler& operator=(const nsTArray_CopyEnabler& aOther) {
    if (this != &aOther) {
      static_cast<Impl*>(this)->ReplaceElementsAt(
          0, static_cast<Impl*>(this)->Length(),
          static_cast<const Impl&>(aOther).Elements(),
          static_cast<const Impl&>(aOther).Length());
    }
    return *this;
  }
};

}  // namespace detail

// This class provides a SafeElementAt method to nsTArray<T*> which does
// not take a second default value parameter.
template <class E, class Derived>
struct nsTArray_SafeElementAtHelper
    : public ::detail::nsTArray_CopyEnabler<E, Derived> {
  typedef E* elem_type;
  typedef size_t index_type;

  // No implementation is provided for these two methods, and that is on
  // purpose, since we don't support these functions on non-pointer type
  // instantiations.
  elem_type& SafeElementAt(index_type aIndex);
  const elem_type& SafeElementAt(index_type aIndex) const;
};

template <class E, class Derived>
struct nsTArray_SafeElementAtHelper<E*, Derived>
    : public ::detail::nsTArray_CopyEnabler<E*, Derived> {
  typedef E* elem_type;
  // typedef const E* const_elem_type;   XXX: see below
  typedef size_t index_type;

  elem_type SafeElementAt(index_type aIndex) {
    return static_cast<Derived*>(this)->SafeElementAt(aIndex, nullptr);
  }

  // XXX: Probably should return const_elem_type, but callsites must be fixed.
  // Also, the use of const_elem_type for nsTArray<xpcGCCallback> in
  // xpcprivate.h causes build failures on Windows because xpcGCCallback is a
  // function pointer and MSVC doesn't like qualifying it with |const|.
  elem_type SafeElementAt(index_type aIndex) const {
    return static_cast<const Derived*>(this)->SafeElementAt(aIndex, nullptr);
  }
};

// E is a smart pointer type; the
// smart pointer can act as its element_type*.
template <class E, class Derived>
struct nsTArray_SafeElementAtSmartPtrHelper
    : public ::detail::nsTArray_CopyEnabler<E, Derived> {
  typedef typename E::element_type* elem_type;
  typedef const typename E::element_type* const_elem_type;
  typedef size_t index_type;

  elem_type SafeElementAt(index_type aIndex) {
    auto* derived = static_cast<Derived*>(this);
    if (aIndex < derived->Length()) {
      return derived->Elements()[aIndex];
    }
    return nullptr;
  }

  // XXX: Probably should return const_elem_type, but callsites must be fixed.
  elem_type SafeElementAt(index_type aIndex) const {
    auto* derived = static_cast<const Derived*>(this);
    if (aIndex < derived->Length()) {
      return derived->Elements()[aIndex];
    }
    return nullptr;
  }
};

template <class T>
class nsCOMPtr;

template <class E, class Derived>
struct nsTArray_SafeElementAtHelper<nsCOMPtr<E>, Derived>
    : public nsTArray_SafeElementAtSmartPtrHelper<nsCOMPtr<E>, Derived> {};

template <class E, class Derived>
struct nsTArray_SafeElementAtHelper<RefPtr<E>, Derived>
    : public nsTArray_SafeElementAtSmartPtrHelper<RefPtr<E>, Derived> {};

namespace mozilla {
template <class T>
class OwningNonNull;
}  // namespace mozilla

template <class E, class Derived>
struct nsTArray_SafeElementAtHelper<mozilla::OwningNonNull<E>, Derived>
    : public nsTArray_SafeElementAtSmartPtrHelper<mozilla::OwningNonNull<E>,
                                                  Derived> {};

// Servo bindings.
extern "C" void Gecko_EnsureTArrayCapacity(void* aArray, size_t aCapacity,
                                           size_t aElementSize);
extern "C" void Gecko_ClearPODTArray(void* aArray, size_t aElementSize,
                                     size_t aElementAlign);

MOZ_NORETURN MOZ_COLD void InvalidArrayIndex_CRASH(size_t aIndex,
                                                   size_t aLength);

//
// This class serves as a base class for nsTArray.  It shouldn't be used
// directly.  It holds common implementation code that does not depend on the
// element type of the nsTArray.
//
template <class Alloc, class RelocationStrategy>
class nsTArray_base {
  // Allow swapping elements with |nsTArray_base|s created using a
  // different allocator.  This is kosher because all allocators use
  // the same free().
  template <class XAlloc, class XRelocationStrategy>
  friend class nsTArray_base;
  friend void Gecko_EnsureTArrayCapacity(void* aArray, size_t aCapacity,
                                         size_t aElemSize);
  friend void Gecko_ClearPODTArray(void* aTArray, size_t aElementSize,
                                   size_t aElementAlign);

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
  size_type Capacity() const { return mHdr->mCapacity; }

#ifdef DEBUG
  void* DebugGetHeader() const { return mHdr; }
#endif

 protected:
  nsTArray_base();

  ~nsTArray_base();

  nsTArray_base(const nsTArray_base&);
  nsTArray_base& operator=(const nsTArray_base&);

  // Resize the storage if necessary to achieve the requested capacity.
  // @param aCapacity The requested number of array elements.
  // @param aElemSize The size of an array element.
  // @return False if insufficient memory is available; true otherwise.
  template <typename ActualAlloc>
  typename ActualAlloc::ResultTypeProxy EnsureCapacity(size_type aCapacity,
                                                       size_type aElemSize);

  // Extend the storage to accommodate aCount extra elements.
  // @param aLength The current size of the array.
  // @param aCount The number of elements to add.
  // @param aElemSize The size of an array element.
  // @return False if insufficient memory is available or the new length
  //   would overflow; true otherwise.
  template <typename ActualAlloc>
  typename ActualAlloc::ResultTypeProxy ExtendCapacity(size_type aLength,
                                                       size_type aCount,
                                                       size_type aElemSize);

  // Tries to resize the storage to the minimum required amount. If this fails,
  // the array is left as-is.
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
  template <typename ActualAlloc>
  void ShiftData(index_type aStart, size_type aOldLen, size_type aNewLen,
                 size_type aElemSize, size_t aElemAlign);

  // This method may be called to swap elements from the end of the array to
  // fill a "gap" in the array. If the resulting array has zero elements, then
  // the array's memory is free'd.
  // @param aStart     The starting index of the gap.
  // @param aCount     The length of the gap.
  // @param aElemSize  The size of an array element.
  // @param aElemAlign The alignment in bytes of an array element.
  template <typename ActualAlloc>
  void SwapFromEnd(index_type aStart, size_type aCount, size_type aElemSize,
                   size_t aElemAlign);

  // This method increments the length member of the array's header.
  // Note that mHdr may actually be sEmptyTArrayHeader in the case where a
  // zero-length array is inserted into our array. But then aNum should
  // always be 0.
  void IncrementLength(size_t aNum) {
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
  template <typename ActualAlloc>
  typename ActualAlloc::ResultTypeProxy InsertSlotsAt(index_type aIndex,
                                                      size_type aCount,
                                                      size_type aElementSize,
                                                      size_t aElemAlign);

  template <typename ActualAlloc, class Allocator>
  typename ActualAlloc::ResultTypeProxy SwapArrayElements(
      nsTArray_base<Allocator, RelocationStrategy>& aOther, size_type aElemSize,
      size_t aElemAlign);

  // This is an RAII class used in SwapArrayElements.
  class IsAutoArrayRestorer {
   public:
    IsAutoArrayRestorer(nsTArray_base<Alloc, RelocationStrategy>& aArray,
                        size_t aElemAlign);
    ~IsAutoArrayRestorer();

   private:
    nsTArray_base<Alloc, RelocationStrategy>& mArray;
    size_t mElemAlign;
    bool mIsAuto;
  };

  // Helper function for SwapArrayElements. Ensures that if the array
  // is an AutoTArray that it doesn't use the built-in buffer.
  template <typename ActualAlloc>
  bool EnsureNotUsingAutoArrayBuffer(size_type aElemSize);

  // Returns true if this nsTArray is an AutoTArray with a built-in buffer.
  bool IsAutoArray() const { return mHdr->mIsAutoArray; }

  // Returns a Header for the built-in buffer of this AutoTArray.
  Header* GetAutoArrayBuffer(size_t aElemAlign) {
    MOZ_ASSERT(IsAutoArray(), "Should be an auto array to call this");
    return GetAutoArrayBufferUnsafe(aElemAlign);
  }
  const Header* GetAutoArrayBuffer(size_t aElemAlign) const {
    MOZ_ASSERT(IsAutoArray(), "Should be an auto array to call this");
    return GetAutoArrayBufferUnsafe(aElemAlign);
  }

  // Returns a Header for the built-in buffer of this AutoTArray, but doesn't
  // assert that we are an AutoTArray.
  Header* GetAutoArrayBufferUnsafe(size_t aElemAlign) {
    return const_cast<Header*>(
        static_cast<const nsTArray_base<Alloc, RelocationStrategy>*>(this)
            ->GetAutoArrayBufferUnsafe(aElemAlign));
  }
  const Header* GetAutoArrayBufferUnsafe(size_t aElemAlign) const;

  // Returns true if this is an AutoTArray and it currently uses the
  // built-in buffer to store its elements.
  bool UsesAutoArrayBuffer() const;

  // The array's elements (prefixed with a Header).  This pointer is never
  // null.  If the array is empty, then this will point to sEmptyTArrayHeader.
  Header* mHdr;

  Header* Hdr() const { return mHdr; }
  Header** PtrToHdr() { return &mHdr; }
  static Header* EmptyHdr() { return &sEmptyTArrayHeader; }
};

namespace detail {

// Used for argument checking in nsTArrayElementTraits::Emplace.
template <typename... T>
struct ChooseFirst;

template <>
struct ChooseFirst<> {
  // Choose a default type that is guaranteed to not match E* for any
  // nsTArray<E>.
  typedef void Type;
};

template <typename A, typename... Args>
struct ChooseFirst<A, Args...> {
  typedef A Type;
};

}  // namespace detail

//
// This class defines convenience functions for element specific operations.
// Specialize this template if necessary.
//
template <class E>
class nsTArrayElementTraits {
 public:
  // Invoke the default constructor in place.
  static inline void Construct(E* aE) {
    // Do NOT call "E()"! That triggers C++ "default initialization"
    // which zeroes out POD ("plain old data") types such as regular
    // ints.  We don't want that because it can be a performance issue
    // and people don't expect it; nsTArray should work like a regular
    // C/C++ array in this respect.
    new (static_cast<void*>(aE)) E;
  }
  // Invoke the copy-constructor in place.
  template <class A>
  static inline void Construct(E* aE, A&& aArg) {
    typedef typename mozilla::RemoveCV<E>::Type E_NoCV;
    typedef typename mozilla::RemoveCV<A>::Type A_NoCV;
    static_assert(!mozilla::IsSame<E_NoCV*, A_NoCV>::value,
                  "For safety, we disallow constructing nsTArray<E> elements "
                  "from E* pointers. See bug 960591.");
    new (static_cast<void*>(aE)) E(std::forward<A>(aArg));
  }
  // Construct in place.
  template <class... Args>
  static inline void Emplace(E* aE, Args&&... aArgs) {
    typedef typename mozilla::RemoveCV<E>::Type E_NoCV;
    typedef typename mozilla::RemoveCV<
        typename ::detail::ChooseFirst<Args...>::Type>::Type A_NoCV;
    static_assert(!mozilla::IsSame<E_NoCV*, A_NoCV>::value,
                  "For safety, we disallow constructing nsTArray<E> elements "
                  "from E* pointers. See bug 960591.");
    new (static_cast<void*>(aE)) E(std::forward<Args>(aArgs)...);
  }
  // Invoke the destructor in place.
  static inline void Destruct(E* aE) { aE->~E(); }
};

// The default comparator used by nsTArray
template <class A, class B>
class nsDefaultComparator {
 public:
  bool Equals(const A& aA, const B& aB) const { return aA == aB; }
  bool LessThan(const A& aA, const B& aB) const { return aA < aB; }
};

template <bool IsPod, bool IsSameType>
struct AssignRangeAlgorithm {
  template <class Item, class ElemType, class IndexType, class SizeType>
  static void implementation(ElemType* aElements, IndexType aStart,
                             SizeType aCount, const Item* aValues) {
    ElemType* iter = aElements + aStart;
    ElemType* end = iter + aCount;
    for (; iter != end; ++iter, ++aValues) {
      nsTArrayElementTraits<ElemType>::Construct(iter, *aValues);
    }
  }
};

template <>
struct AssignRangeAlgorithm<true, true> {
  template <class Item, class ElemType, class IndexType, class SizeType>
  static void implementation(ElemType* aElements, IndexType aStart,
                             SizeType aCount, const Item* aValues) {
    if (aValues) {
      memcpy(aElements + aStart, aValues, aCount * sizeof(ElemType));
    }
  }
};

//
// Normally elements are copied with memcpy and memmove, but for some element
// types that is problematic.  The nsTArray_RelocationStrategy template class
// can be specialized to ensure that copying calls constructors and destructors
// instead, as is done below for JS::Heap<E> elements.
//

//
// A class that defines how to copy elements using memcpy/memmove.
//
struct nsTArray_RelocateUsingMemutils {
  const static bool allowRealloc = true;

  static void RelocateNonOverlappingRegionWithHeader(void* aDest,
                                                     const void* aSrc,
                                                     size_t aCount,
                                                     size_t aElemSize) {
    memcpy(aDest, aSrc, sizeof(nsTArrayHeader) + aCount * aElemSize);
  }

  static void RelocateOverlappingRegion(void* aDest, void* aSrc, size_t aCount,
                                        size_t aElemSize) {
    memmove(aDest, aSrc, aCount * aElemSize);
  }

  static void RelocateNonOverlappingRegion(void* aDest, void* aSrc,
                                           size_t aCount, size_t aElemSize) {
    memcpy(aDest, aSrc, aCount * aElemSize);
  }
};

//
// A template class that defines how to copy elements calling their constructors
// and destructors appropriately.
//
template <class ElemType>
struct nsTArray_RelocateUsingMoveConstructor {
  typedef nsTArrayElementTraits<ElemType> traits;

  const static bool allowRealloc = false;

  static void RelocateNonOverlappingRegionWithHeader(void* aDest, void* aSrc,
                                                     size_t aCount,
                                                     size_t aElemSize) {
    nsTArrayHeader* destHeader = static_cast<nsTArrayHeader*>(aDest);
    nsTArrayHeader* srcHeader = static_cast<nsTArrayHeader*>(aSrc);
    *destHeader = *srcHeader;
    RelocateNonOverlappingRegion(
        static_cast<uint8_t*>(aDest) + sizeof(nsTArrayHeader),
        static_cast<uint8_t*>(aSrc) + sizeof(nsTArrayHeader), aCount,
        aElemSize);
  }

  // These functions are defined by analogy with memmove and memcpy.
  // What they actually do is slightly different: RelocateOverlappingRegion
  // checks to see which direction the movement needs to take place,
  // whether from back-to-front of the range to be moved or from
  // front-to-back.  RelocateNonOverlappingRegion assumes that moving
  // front-to-back is always valid.  So they're really more like
  // std::move{_backward,} in that respect.  We keep these names because
  // we think they read slightly better, and RelocateNonOverlappingRegion is
  // only ever called on overlapping regions from RelocateOverlappingRegion.
  static void RelocateOverlappingRegion(void* aDest, void* aSrc, size_t aCount,
                                        size_t aElemSize) {
    ElemType* destElem = static_cast<ElemType*>(aDest);
    ElemType* srcElem = static_cast<ElemType*>(aSrc);
    ElemType* destElemEnd = destElem + aCount;
    ElemType* srcElemEnd = srcElem + aCount;
    if (destElem == srcElem) {
      return;  // In practice, we don't do this.
    }

    // Figure out whether to copy back-to-front or front-to-back.
    if (srcElemEnd > destElem && srcElemEnd < destElemEnd) {
      while (destElemEnd != destElem) {
        --destElemEnd;
        --srcElemEnd;
        traits::Construct(destElemEnd, std::move(*srcElemEnd));
        traits::Destruct(srcElemEnd);
      }
    } else {
      RelocateNonOverlappingRegion(aDest, aSrc, aCount, aElemSize);
    }
  }

  static void RelocateNonOverlappingRegion(void* aDest, void* aSrc,
                                           size_t aCount, size_t aElemSize) {
    ElemType* destElem = static_cast<ElemType*>(aDest);
    ElemType* srcElem = static_cast<ElemType*>(aSrc);
    ElemType* destElemEnd = destElem + aCount;
#ifdef DEBUG
    ElemType* srcElemEnd = srcElem + aCount;
    MOZ_ASSERT(srcElemEnd <= destElem || srcElemEnd > destElemEnd);
#endif
    while (destElem != destElemEnd) {
      traits::Construct(destElem, std::move(*srcElem));
      traits::Destruct(srcElem);
      ++destElem;
      ++srcElem;
    }
  }
};

//
// The default behaviour is to use memcpy/memmove for everything.
//
template <class E>
struct MOZ_NEEDS_MEMMOVABLE_TYPE nsTArray_RelocationStrategy {
  using Type = nsTArray_RelocateUsingMemutils;
};

//
// Some classes require constructors/destructors to be called, so they are
// specialized here.
//
#define MOZ_DECLARE_RELOCATE_USING_MOVE_CONSTRUCTOR(T)     \
  template <>                                              \
  struct nsTArray_RelocationStrategy<T> {                  \
    using Type = nsTArray_RelocateUsingMoveConstructor<T>; \
  };

#define MOZ_DECLARE_RELOCATE_USING_MOVE_CONSTRUCTOR_FOR_TEMPLATE(T) \
  template <typename S>                                             \
  struct nsTArray_RelocationStrategy<T<S>> {                        \
    using Type = nsTArray_RelocateUsingMoveConstructor<T<S>>;       \
  };

MOZ_DECLARE_COPY_CONSTRUCTIBLE(nsTString<char>)
MOZ_DECLARE_COPY_CONSTRUCTIBLE(nsTString<char16_t>)

MOZ_DECLARE_COPY_CONSTRUCTIBLE(mozilla::a11y::BatchData)

// TODO mozilla::ipc::AutoIPCStream is not even movable, so memmovable use with
// nsTArray (in StructuredCloneData) seems at least quirky
MOZ_DECLARE_NON_COPY_CONSTRUCTIBLE(mozilla::ipc::AutoIPCStream)
MOZ_DECLARE_COPY_CONSTRUCTIBLE(mozilla::ipc::ContentSecurityPolicy)
MOZ_DECLARE_COPY_CONSTRUCTIBLE(mozilla::layers::Animation)
MOZ_DECLARE_COPY_CONSTRUCTIBLE(mozilla::layers::FrameStats)
MOZ_DECLARE_COPY_CONSTRUCTIBLE(mozilla::layers::PropertyAnimationGroup)
#define MOZ_NSTARRAY_COMMA ,
MOZ_DECLARE_COPY_CONSTRUCTIBLE(
    mozilla::dom::binding_detail::RecordEntry<
        nsTString<char> MOZ_NSTARRAY_COMMA
            mozilla::dom::Nullable<mozilla::dom::OwningUTF8StringOrDouble>>)
MOZ_DECLARE_COPY_CONSTRUCTIBLE(mozilla::dom::binding_detail::RecordEntry<
                               nsTString<char16_t> MOZ_NSTARRAY_COMMA
                                   mozilla::dom::OwningStringOrBooleanOrObject>)
#undef MOZ_NSTARRAY_COMMA
MOZ_DECLARE_COPY_CONSTRUCTIBLE(mozilla::dom::MessagePortIdentifier)
MOZ_DECLARE_COPY_CONSTRUCTIBLE(mozilla::dom::MozPluginParameter)
MOZ_DECLARE_COPY_CONSTRUCTIBLE(mozilla::dom::OwningFileOrDirectory)
MOZ_DECLARE_COPY_CONSTRUCTIBLE(mozilla::dom::Pref)
MOZ_DECLARE_COPY_CONSTRUCTIBLE(mozilla::dom::ResponsiveImageCandidate)
MOZ_DECLARE_COPY_CONSTRUCTIBLE(mozilla::dom::ServiceWorkerRegistrationData)

MOZ_DECLARE_RELOCATE_USING_MOVE_CONSTRUCTOR_FOR_TEMPLATE(JS::Heap)
MOZ_DECLARE_RELOCATE_USING_MOVE_CONSTRUCTOR_FOR_TEMPLATE(std::function)

MOZ_DECLARE_RELOCATE_USING_MOVE_CONSTRUCTOR(nsRegion)
MOZ_DECLARE_RELOCATE_USING_MOVE_CONSTRUCTOR(nsIntRegion)
MOZ_DECLARE_RELOCATE_USING_MOVE_CONSTRUCTOR(mozilla::layers::TileClient)
MOZ_DECLARE_RELOCATE_USING_MOVE_CONSTRUCTOR(
    mozilla::layers::RenderRootDisplayListData)
MOZ_DECLARE_RELOCATE_USING_MOVE_CONSTRUCTOR(mozilla::layers::RenderRootUpdates)
MOZ_DECLARE_RELOCATE_USING_MOVE_CONSTRUCTOR(
    mozilla::SerializedStructuredCloneBuffer)
MOZ_DECLARE_RELOCATE_USING_MOVE_CONSTRUCTOR(
    mozilla::dom::ipc::StructuredCloneData)
MOZ_DECLARE_RELOCATE_USING_MOVE_CONSTRUCTOR(mozilla::dom::ClonedMessageData)
MOZ_DECLARE_RELOCATE_USING_MOVE_CONSTRUCTOR(
    mozilla::dom::indexedDB::ObjectStoreCursorResponse)
MOZ_DECLARE_RELOCATE_USING_MOVE_CONSTRUCTOR(
    mozilla::dom::indexedDB::IndexCursorResponse)
MOZ_DECLARE_RELOCATE_USING_MOVE_CONSTRUCTOR(
    mozilla::dom::indexedDB::SerializedStructuredCloneReadInfo);
MOZ_DECLARE_RELOCATE_USING_MOVE_CONSTRUCTOR(JSStructuredCloneData)
MOZ_DECLARE_RELOCATE_USING_MOVE_CONSTRUCTOR(mozilla::dom::MessageData)
MOZ_DECLARE_RELOCATE_USING_MOVE_CONSTRUCTOR(mozilla::dom::RefMessageData)
MOZ_DECLARE_RELOCATE_USING_MOVE_CONSTRUCTOR(mozilla::SourceBufferTask)

//
// Base class for nsTArray_Impl that is templated on element type and derived
// nsTArray_Impl class, to allow extra conversions to be added for specific
// types.
//
template <class E, class Derived>
struct nsTArray_TypedBase : public nsTArray_SafeElementAtHelper<E, Derived> {};

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
template <class E, class Derived>
struct nsTArray_TypedBase<JS::Heap<E>, Derived>
    : public nsTArray_SafeElementAtHelper<JS::Heap<E>, Derived> {
  operator const nsTArray<E>&() {
    static_assert(sizeof(E) == sizeof(JS::Heap<E>),
                  "JS::Heap<E> must be binary compatible with E.");
    Derived* self = static_cast<Derived*>(this);
    return *reinterpret_cast<nsTArray<E>*>(self);
  }

  operator const FallibleTArray<E>&() {
    Derived* self = static_cast<Derived*>(this);
    return *reinterpret_cast<FallibleTArray<E>*>(self);
  }
};

namespace detail {

// These helpers allow us to differentiate between tri-state comparator
// functions and classes with LessThan() and Equal() methods. If an object, when
// called as a function with two instances of our element type, returns an int,
// we treat it as a tri-state comparator.
//
// T is the type of the comparator object we want to check. U is the array
// element type that we'll be comparing.
//
// V is never passed, and is only used to allow us to specialize on the return
// value of the comparator function.
template <typename T, typename U, typename V = int>
struct IsCompareMethod : mozilla::FalseType {};

template <typename T, typename U>
struct IsCompareMethod<T, U,
                       decltype(mozilla::DeclVal<T>()(mozilla::DeclVal<U>(),
                                                      mozilla::DeclVal<U>()))>
    : mozilla::TrueType {};

// These two wrappers allow us to use either a tri-state comparator, or an
// object with Equals() and LessThan() methods interchangeably. They provide a
// tri-state Compare() method, and Equals() method, and a LessThan() method.
//
// Depending on the type of the underlying comparator, they either pass these
// through directly, or synthesize them from the methods available on the
// comparator.
//
// Callers should always use the most-specific of these methods that match their
// purpose.

// Comparator wrapper for a tri-state comparator function
template <typename T, typename U, bool IsCompare = IsCompareMethod<T, U>::value>
struct CompareWrapper {
#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable : 4180) /* Silence "qualifier applied to function \
                                     type has no meaning" warning */
#endif
  MOZ_IMPLICIT CompareWrapper(const T& aComparator)
      : mComparator(aComparator) {}

  template <typename A, typename B>
  int Compare(A& aLeft, B& aRight) const {
    return mComparator(aLeft, aRight);
  }

  template <typename A, typename B>
  bool Equals(A& aLeft, B& aRight) const {
    return Compare(aLeft, aRight) == 0;
  }

  template <typename A, typename B>
  bool LessThan(A& aLeft, B& aRight) const {
    return Compare(aLeft, aRight) < 0;
  }

  const T& mComparator;
#ifdef _MSC_VER
#  pragma warning(pop)
#endif
};

// Comparator wrapper for a class with Equals() and LessThan() methods.
template <typename T, typename U>
struct CompareWrapper<T, U, false> {
  MOZ_IMPLICIT CompareWrapper(const T& aComparator)
      : mComparator(aComparator) {}

  template <typename A, typename B>
  int Compare(A& aLeft, B& aRight) const {
    if (Equals(aLeft, aRight)) {
      return 0;
    }
    return LessThan(aLeft, aRight) ? -1 : 1;
  }

  template <typename A, typename B>
  bool Equals(A& aLeft, B& aRight) const {
    return mComparator.Equals(aLeft, aRight);
  }

  template <typename A, typename B>
  bool LessThan(A& aLeft, B& aRight) const {
    return mComparator.LessThan(aLeft, aRight);
  }

  const T& mComparator;
};

}  // namespace detail

//
// nsTArray_Impl contains most of the guts supporting nsTArray, FallibleTArray,
// AutoTArray.
//
// The only situation in which you might need to use nsTArray_Impl in your code
// is if you're writing code which mutates a TArray which may or may not be
// infallible.
//
// Code which merely reads from a TArray which may or may not be infallible can
// simply cast the TArray to |const nsTArray&|; both fallible and infallible
// TArrays can be cast to |const nsTArray&|.
//
template <class E, class Alloc>
class nsTArray_Impl
    : public nsTArray_base<Alloc,
                           typename nsTArray_RelocationStrategy<E>::Type>,
      public nsTArray_TypedBase<
          E,
          nsTArray_Impl<E, Alloc>>  // This must come last to ensure the members
                                    // from nsTArray_base are initialized before
                                    // the delegated constructor calls from
                                    // nsTArray_CopyEnabler are executed.
{
 private:
  friend class ::detail::nsTArray_CopyEnabler<E, nsTArray_Impl<E, Alloc>>;

  typedef nsTArrayFallibleAllocator FallibleAlloc;
  typedef nsTArrayInfallibleAllocator InfallibleAlloc;

 public:
  typedef typename nsTArray_RelocationStrategy<E>::Type relocation_type;
  typedef nsTArray_base<Alloc, relocation_type> base_type;
  typedef typename base_type::size_type size_type;
  typedef typename base_type::index_type index_type;
  typedef E elem_type;
  typedef nsTArray_Impl<E, Alloc> self_type;
  typedef nsTArrayElementTraits<E> elem_traits;
  typedef nsTArray_SafeElementAtHelper<E, self_type> safeelementat_helper_type;
  typedef mozilla::ArrayIterator<elem_type&, nsTArray<E>> iterator;
  typedef mozilla::ArrayIterator<const elem_type&, nsTArray<E>> const_iterator;
  typedef mozilla::ReverseIterator<iterator> reverse_iterator;
  typedef mozilla::ReverseIterator<const_iterator> const_reverse_iterator;

  using base_type::EmptyHdr;
  using safeelementat_helper_type::SafeElementAt;

  // A special value that is used to indicate an invalid or unknown index
  // into the array.
  static const index_type NoIndex = index_type(-1);

  using base_type::Length;

  //
  // Finalization method
  //

  ~nsTArray_Impl() {
    if (!base_type::IsEmpty()) {
      ClearAndRetainStorage();
    }
    // mHdr cleanup will be handled by base destructor
  }

  //
  // Initialization methods
  //

  nsTArray_Impl() = default;

  // Initialize this array and pre-allocate some number of elements.
  explicit nsTArray_Impl(size_type aCapacity) { SetCapacity(aCapacity); }

  // Initialize this array with an r-value.
  // Allow different types of allocators, since the allocator doesn't matter.
  template <typename Allocator>
  explicit nsTArray_Impl(nsTArray_Impl<E, Allocator>&& aOther) {
    SwapElements(aOther);
  }

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
  nsTArray_Impl(const nsTArray_Impl&) = default;

  explicit nsTArray_Impl(std::initializer_list<E> aIL) {
    AppendElements(aIL.begin(), aIL.size());
  }
  // Allow converting to a const array with a different kind of allocator,
  // Since the allocator doesn't matter for const arrays
  template <typename Allocator>
  operator const nsTArray_Impl<E, Allocator>&() const& {
    return *reinterpret_cast<const nsTArray_Impl<E, Allocator>*>(this);
  }
  // And we have to do this for our subclasses too
  operator const nsTArray<E>&() const& {
    return *reinterpret_cast<const nsTArray<E>*>(this);
  }
  operator const FallibleTArray<E>&() const& {
    return *reinterpret_cast<const FallibleTArray<E>*>(this);
  }

  // The array's assignment operator performs a 'deep' copy of the given
  // array.  It is optimized to reuse existing storage if possible.
  // @param aOther The array object to copy.
  nsTArray_Impl& operator=(const nsTArray_Impl&) = default;

  // The array's move assignment operator steals the underlying data from
  // the other array.
  // @param other  The array object to move from.
  self_type& operator=(self_type&& aOther) {
    if (this != &aOther) {
      Clear();
      SwapElements(aOther);
    }
    return *this;
  }

  // Return true if this array has the same length and the same
  // elements as |aOther|.
  template <typename Allocator>
  bool operator==(const nsTArray_Impl<E, Allocator>& aOther) const {
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

  template <typename Allocator>
  self_type& operator=(const nsTArray_Impl<E, Allocator>& aOther) {
    ReplaceElementsAt(0, Length(), aOther.Elements(), aOther.Length());
    return *this;
  }

  template <typename Allocator>
  self_type& operator=(nsTArray_Impl<E, Allocator>&& aOther) {
    Clear();
    SwapElements(aOther);
    return *this;
  }

  // @return The amount of memory used by this nsTArray_Impl, excluding
  // sizeof(*this). If you want to measure anything hanging off the array, you
  // must iterate over the elements and measure them individually; hence the
  // "Shallow" prefix.
  size_t ShallowSizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
    if (this->UsesAutoArrayBuffer() || Hdr() == EmptyHdr()) {
      return 0;
    }
    return aMallocSizeOf(this->Hdr());
  }

  // @return The amount of memory used by this nsTArray_Impl, including
  // sizeof(*this). If you want to measure anything hanging off the array, you
  // must iterate over the elements and measure them individually; hence the
  // "Shallow" prefix.
  size_t ShallowSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
    return aMallocSizeOf(this) + ShallowSizeOfExcludingThis(aMallocSizeOf);
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
  const elem_type* Elements() const {
    return reinterpret_cast<const elem_type*>(Hdr() + 1);
  }

  // This method provides direct access to an element of the array. The given
  // index must be within the array bounds.
  // @param aIndex The index of an element in the array.
  // @return A reference to the i'th element of the array.
  elem_type& ElementAt(index_type aIndex) {
    if (MOZ_UNLIKELY(aIndex >= Length())) {
      InvalidArrayIndex_CRASH(aIndex, Length());
    }
    return Elements()[aIndex];
  }

  // This method provides direct, readonly access to an element of the array
  // The given index must be within the array bounds.
  // @param aIndex The index of an element in the array.
  // @return A const reference to the i'th element of the array.
  const elem_type& ElementAt(index_type aIndex) const {
    if (MOZ_UNLIKELY(aIndex >= Length())) {
      InvalidArrayIndex_CRASH(aIndex, Length());
    }
    return Elements()[aIndex];
  }

  // This method provides direct access to an element of the array in a bounds
  // safe manner. If the requested index is out of bounds the provided default
  // value is returned.
  // @param aIndex The index of an element in the array.
  // @param aDef   The value to return if the index is out of bounds.
  elem_type& SafeElementAt(index_type aIndex, elem_type& aDef) {
    return aIndex < Length() ? Elements()[aIndex] : aDef;
  }

  // This method provides direct access to an element of the array in a bounds
  // safe manner. If the requested index is out of bounds the provided default
  // value is returned.
  // @param aIndex The index of an element in the array.
  // @param aDef   The value to return if the index is out of bounds.
  const elem_type& SafeElementAt(index_type aIndex,
                                 const elem_type& aDef) const {
    return aIndex < Length() ? Elements()[aIndex] : aDef;
  }

  // Shorthand for ElementAt(aIndex)
  elem_type& operator[](index_type aIndex) { return ElementAt(aIndex); }

  // Shorthand for ElementAt(aIndex)
  const elem_type& operator[](index_type aIndex) const {
    return ElementAt(aIndex);
  }

  // Shorthand for ElementAt(length - 1)
  elem_type& LastElement() { return ElementAt(Length() - 1); }

  // Shorthand for ElementAt(length - 1)
  const elem_type& LastElement() const { return ElementAt(Length() - 1); }

  // Shorthand for SafeElementAt(length - 1, def)
  elem_type& SafeLastElement(elem_type& aDef) {
    return SafeElementAt(Length() - 1, aDef);
  }

  // Shorthand for SafeElementAt(length - 1, def)
  const elem_type& SafeLastElement(const elem_type& aDef) const {
    return SafeElementAt(Length() - 1, aDef);
  }

  // Methods for range-based for loops.
  iterator begin() { return iterator(*this, 0); }
  const_iterator begin() const { return const_iterator(*this, 0); }
  const_iterator cbegin() const { return begin(); }
  iterator end() { return iterator(*this, Length()); }
  const_iterator end() const { return const_iterator(*this, Length()); }
  const_iterator cend() const { return end(); }

  // Methods for reverse iterating.
  reverse_iterator rbegin() { return reverse_iterator(end()); }
  const_reverse_iterator rbegin() const {
    return const_reverse_iterator(end());
  }
  const_reverse_iterator crbegin() const { return rbegin(); }
  reverse_iterator rend() { return reverse_iterator(begin()); }
  const_reverse_iterator rend() const {
    return const_reverse_iterator(begin());
  }
  const_reverse_iterator crend() const { return rend(); }

  // Span integration

  operator mozilla::Span<elem_type>() {
    return mozilla::Span<elem_type>(Elements(), Length());
  }

  operator mozilla::Span<const elem_type>() const {
    return mozilla::Span<const elem_type>(Elements(), Length());
  }

  //
  // Search methods
  //

  // This method searches for the first element in this array that is equal
  // to the given element.
  // @param aItem  The item to search for.
  // @param aComp  The Comparator used to determine element equality.
  // @return       true if the element was found.
  template <class Item, class Comparator>
  bool Contains(const Item& aItem, const Comparator& aComp) const {
    return ApplyIf(
        aItem, 0, aComp, []() { return true; }, []() { return false; });
  }

  // Like Contains(), but assumes a sorted array.
  template <class Item, class Comparator>
  bool ContainsSorted(const Item& aItem, const Comparator& aComp) const {
    return BinaryIndexOf(aItem, aComp) != NoIndex;
  }

  // This method searches for the first element in this array that is equal
  // to the given element.  This method assumes that 'operator==' is defined
  // for elem_type.
  // @param aItem  The item to search for.
  // @return       true if the element was found.
  template <class Item>
  bool Contains(const Item& aItem) const {
    return Contains(aItem, nsDefaultComparator<elem_type, Item>());
  }

  // Like Contains(), but assumes a sorted array.
  template <class Item>
  bool ContainsSorted(const Item& aItem) const {
    return BinaryIndexOf(aItem) != NoIndex;
  }

  // This method searches for the offset of the first element in this
  // array that is equal to the given element.
  // @param aItem  The item to search for.
  // @param aStart The index to start from.
  // @param aComp  The Comparator used to determine element equality.
  // @return       The index of the found element or NoIndex if not found.
  template <class Item, class Comparator>
  index_type IndexOf(const Item& aItem, index_type aStart,
                     const Comparator& aComp) const {
    ::detail::CompareWrapper<Comparator, Item> comp(aComp);

    const elem_type* iter = Elements() + aStart;
    const elem_type* iend = Elements() + Length();
    for (; iter != iend; ++iter) {
      if (comp.Equals(*iter, aItem)) {
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
  template <class Item>
  index_type IndexOf(const Item& aItem, index_type aStart = 0) const {
    return IndexOf(aItem, aStart, nsDefaultComparator<elem_type, Item>());
  }

  // This method searches for the offset of the last element in this
  // array that is equal to the given element.
  // @param aItem  The item to search for.
  // @param aStart The index to start from.  If greater than or equal to the
  //               length of the array, then the entire array is searched.
  // @param aComp  The Comparator used to determine element equality.
  // @return       The index of the found element or NoIndex if not found.
  template <class Item, class Comparator>
  index_type LastIndexOf(const Item& aItem, index_type aStart,
                         const Comparator& aComp) const {
    ::detail::CompareWrapper<Comparator, Item> comp(aComp);

    size_type endOffset = aStart >= Length() ? Length() : aStart + 1;
    const elem_type* iend = Elements() - 1;
    const elem_type* iter = iend + endOffset;
    for (; iter != iend; --iter) {
      if (comp.Equals(*iter, aItem)) {
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
  template <class Item>
  index_type LastIndexOf(const Item& aItem, index_type aStart = NoIndex) const {
    return LastIndexOf(aItem, aStart, nsDefaultComparator<elem_type, Item>());
  }

  // This method searches for the offset for the element in this array
  // that is equal to the given element. The array is assumed to be sorted.
  // If there is more than one equivalent element, there is no guarantee
  // on which one will be returned.
  // @param aItem  The item to search for.
  // @param aComp  The Comparator used.
  // @return       The index of the found element or NoIndex if not found.
  template <class Item, class Comparator>
  index_type BinaryIndexOf(const Item& aItem, const Comparator& aComp) const {
    using mozilla::BinarySearchIf;
    ::detail::CompareWrapper<Comparator, Item> comp(aComp);

    size_t index;
    bool found = BinarySearchIf(
        Elements(), 0, Length(),
        // Note: We pass the Compare() args here in reverse order and negate the
        // results for compatibility reasons. Some existing callers use Equals()
        // functions with first arguments which match aElement but not aItem, or
        // second arguments that match aItem but not aElement. To accommodate
        // those callers, we preserve the argument order of the older version of
        // this API. These callers, however, should be fixed, and this special
        // case removed.
        [&](const elem_type& aElement) {
          return -comp.Compare(aElement, aItem);
        },
        &index);
    return found ? index : NoIndex;
  }

  // This method searches for the offset for the element in this array
  // that is equal to the given element. The array is assumed to be sorted.
  // This method assumes that 'operator==' and 'operator<' are defined.
  // @param aItem  The item to search for.
  // @return       The index of the found element or NoIndex if not found.
  template <class Item>
  index_type BinaryIndexOf(const Item& aItem) const {
    return BinaryIndexOf(aItem, nsDefaultComparator<elem_type, Item>());
  }

  //
  // Mutation methods
  //

  template <class Allocator, typename ActualAlloc = Alloc>
  typename ActualAlloc::ResultType Assign(
      const nsTArray_Impl<E, Allocator>& aOther) {
    return ActualAlloc::ConvertBoolToResultType(
        !!ReplaceElementsAt<E, ActualAlloc>(0, Length(), aOther.Elements(),
                                            aOther.Length()));
  }

  template <class Allocator>
  MOZ_MUST_USE bool Assign(const nsTArray_Impl<E, Allocator>& aOther,
                           const mozilla::fallible_t&) {
    return Assign<Allocator, FallibleAlloc>(aOther);
  }

  template <class Allocator>
  void Assign(nsTArray_Impl<E, Allocator>&& aOther) {
    Clear();
    SwapElements(aOther);
  }

  // This method call the destructor on each element of the array, empties it,
  // but does not shrink the array's capacity.
  // See also SetLengthAndRetainStorage.
  // Make sure to call Compact() if needed to avoid keeping a huge array
  // around.
  void ClearAndRetainStorage() {
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
  void SetLengthAndRetainStorage(size_type aNewLen) {
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
 protected:
  template <class Item, typename ActualAlloc = Alloc>
  elem_type* ReplaceElementsAt(index_type aStart, size_type aCount,
                               const Item* aArray, size_type aArrayLen);

 public:
  template <class Item>
  MOZ_MUST_USE elem_type* ReplaceElementsAt(index_type aStart, size_type aCount,
                                            const Item* aArray,
                                            size_type aArrayLen,
                                            const mozilla::fallible_t&) {
    return ReplaceElementsAt<Item, FallibleAlloc>(aStart, aCount, aArray,
                                                  aArrayLen);
  }

  // A variation on the ReplaceElementsAt method defined above.
 protected:
  template <class Item, typename ActualAlloc = Alloc>
  elem_type* ReplaceElementsAt(index_type aStart, size_type aCount,
                               const nsTArray<Item>& aArray) {
    return ReplaceElementsAt<Item, ActualAlloc>(
        aStart, aCount, aArray.Elements(), aArray.Length());
  }

  template <class Item, typename ActualAlloc = Alloc>
  elem_type* ReplaceElementsAt(index_type aStart, size_type aCount,
                               mozilla::Span<const Item> aSpan) {
    return ReplaceElementsAt<Item, ActualAlloc>(
        aStart, aCount, aSpan.Elements(), aSpan.Length());
  }

 public:
  template <class Item>
  MOZ_MUST_USE elem_type* ReplaceElementsAt(index_type aStart, size_type aCount,
                                            const nsTArray<Item>& aArray,
                                            const mozilla::fallible_t&) {
    return ReplaceElementsAt<Item, FallibleAlloc>(aStart, aCount, aArray);
  }

  template <class Item>
  MOZ_MUST_USE elem_type* ReplaceElementsAt(index_type aStart, size_type aCount,
                                            mozilla::Span<const Item> aSpan,
                                            const mozilla::fallible_t&) {
    return ReplaceElementsAt<Item, FallibleAlloc>(aStart, aCount, aSpan);
  }

  // A variation on the ReplaceElementsAt method defined above.
 protected:
  template <class Item, typename ActualAlloc = Alloc>
  elem_type* ReplaceElementsAt(index_type aStart, size_type aCount,
                               const Item& aItem) {
    return ReplaceElementsAt<Item, ActualAlloc>(aStart, aCount, &aItem, 1);
  }

 public:
  template <class Item>
  MOZ_MUST_USE elem_type* ReplaceElementsAt(index_type aStart, size_type aCount,
                                            const Item& aItem,
                                            const mozilla::fallible_t&) {
    return ReplaceElementsAt<Item, FallibleAlloc>(aStart, aCount, aItem);
  }

  // A variation on the ReplaceElementsAt method defined above.
  template <class Item>
  elem_type* ReplaceElementAt(index_type aIndex, const Item& aItem) {
    return ReplaceElementsAt(aIndex, 1, &aItem, 1);
  }

  // A variation on the ReplaceElementsAt method defined above.
 protected:
  template <class Item, typename ActualAlloc = Alloc>
  elem_type* InsertElementsAt(index_type aIndex, const Item* aArray,
                              size_type aArrayLen) {
    return ReplaceElementsAt<Item, ActualAlloc>(aIndex, 0, aArray, aArrayLen);
  }

 public:
  template <class Item>
  MOZ_MUST_USE elem_type* InsertElementsAt(index_type aIndex,
                                           const Item* aArray,
                                           size_type aArrayLen,
                                           const mozilla::fallible_t&) {
    return InsertElementsAt<Item, FallibleAlloc>(aIndex, aArray, aArrayLen);
  }

  // A variation on the ReplaceElementsAt method defined above.
 protected:
  template <class Item, class Allocator, typename ActualAlloc = Alloc>
  elem_type* InsertElementsAt(index_type aIndex,
                              const nsTArray_Impl<Item, Allocator>& aArray) {
    return ReplaceElementsAt<Item, ActualAlloc>(aIndex, 0, aArray.Elements(),
                                                aArray.Length());
  }

  template <class Item, typename ActualAlloc = Alloc>
  elem_type* InsertElementsAt(index_type aIndex,
                              mozilla::Span<const Item> aSpan) {
    return ReplaceElementsAt<Item, ActualAlloc>(aIndex, 0, aSpan.Elements(),
                                                aSpan.Length());
  }

 public:
  template <class Item, class Allocator>
  MOZ_MUST_USE elem_type* InsertElementsAt(
      index_type aIndex, const nsTArray_Impl<Item, Allocator>& aArray,
      const mozilla::fallible_t&) {
    return InsertElementsAt<Item, Allocator, FallibleAlloc>(aIndex, aArray);
  }

  template <class Item>
  MOZ_MUST_USE elem_type* InsertElementsAt(index_type aIndex,
                                           mozilla::Span<const Item> aSpan,
                                           const mozilla::fallible_t&) {
    return InsertElementsAt<Item, FallibleAlloc>(aIndex, aSpan);
  }

  // Insert a new element without copy-constructing. This is useful to avoid
  // temporaries.
  // @return A pointer to the newly inserted element, or null on OOM.
 protected:
  template <typename ActualAlloc = Alloc>
  elem_type* InsertElementAt(index_type aIndex);

 public:
  MOZ_MUST_USE
  elem_type* InsertElementAt(index_type aIndex, const mozilla::fallible_t&) {
    return InsertElementAt<FallibleAlloc>(aIndex);
  }

  // Insert a new element, move constructing if possible.
 protected:
  template <class Item, typename ActualAlloc = Alloc>
  elem_type* InsertElementAt(index_type aIndex, Item&& aItem);

 public:
  template <class Item>
  MOZ_MUST_USE elem_type* InsertElementAt(index_type aIndex, Item&& aItem,
                                          const mozilla::fallible_t&) {
    return InsertElementAt<Item, FallibleAlloc>(aIndex,
                                                std::forward<Item>(aItem));
  }

  // Reconstruct the element at the given index, and return a pointer to the
  // reconstructed element.  This will destroy the existing element and
  // default-construct a new one, giving you a state much like what single-arg
  // InsertElementAt(), or no-arg AppendElement() does, but without changing the
  // length of the array.
  //
  // array[idx] = T()
  //
  // would accomplish the same thing as long as T has the appropriate moving
  // operator=, but some types don't for various reasons.
  elem_type* ReconstructElementAt(index_type aIndex) {
    elem_type* elem = &ElementAt(aIndex);
    elem_traits::Destruct(elem);
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
  template <class Item, class Comparator>
  index_type IndexOfFirstElementGt(const Item& aItem,
                                   const Comparator& aComp) const {
    using mozilla::BinarySearchIf;
    ::detail::CompareWrapper<Comparator, Item> comp(aComp);

    size_t index;
    BinarySearchIf(
        Elements(), 0, Length(),
        [&](const elem_type& aElement) {
          return comp.Compare(aElement, aItem) <= 0 ? 1 : -1;
        },
        &index);
    return index;
  }

  // A variation on the IndexOfFirstElementGt method defined above.
  template <class Item>
  index_type IndexOfFirstElementGt(const Item& aItem) const {
    return IndexOfFirstElementGt(aItem, nsDefaultComparator<elem_type, Item>());
  }

  // Inserts |aItem| at such an index to guarantee that if the array
  // was previously sorted, it will remain sorted after this
  // insertion.
 protected:
  template <class Item, class Comparator, typename ActualAlloc = Alloc>
  elem_type* InsertElementSorted(Item&& aItem, const Comparator& aComp) {
    index_type index = IndexOfFirstElementGt<Item, Comparator>(aItem, aComp);
    return InsertElementAt<Item, ActualAlloc>(index, std::forward<Item>(aItem));
  }

 public:
  template <class Item, class Comparator>
  MOZ_MUST_USE elem_type* InsertElementSorted(Item&& aItem,
                                              const Comparator& aComp,
                                              const mozilla::fallible_t&) {
    return InsertElementSorted<Item, Comparator, FallibleAlloc>(
        std::forward<Item>(aItem), aComp);
  }

  // A variation on the InsertElementSorted method defined above.
 protected:
  template <class Item, typename ActualAlloc = Alloc>
  elem_type* InsertElementSorted(Item&& aItem) {
    nsDefaultComparator<elem_type, Item> comp;
    return InsertElementSorted<Item, decltype(comp), ActualAlloc>(
        std::forward<Item>(aItem), comp);
  }

 public:
  template <class Item>
  MOZ_MUST_USE elem_type* InsertElementSorted(Item&& aItem,
                                              const mozilla::fallible_t&) {
    return InsertElementSorted<Item, FallibleAlloc>(std::forward<Item>(aItem));
  }

  // This method appends elements to the end of this array.
  // @param aArray    The elements to append to this array.
  // @param aArrayLen The number of elements to append to this array.
  // @return          A pointer to the new elements in the array, or null if
  //                  the operation failed due to insufficient memory.
 protected:
  template <class Item, typename ActualAlloc = Alloc>
  elem_type* AppendElements(const Item* aArray, size_type aArrayLen);

  template <class Item, typename ActualAlloc = Alloc>
  elem_type* AppendElements(mozilla::Span<const Item> aSpan) {
    return AppendElements<Item, FallibleAlloc>(aSpan.Elements(),
                                               aSpan.Length());
  }

  template <class Item, size_t Length, typename ActualAlloc = Alloc>
  elem_type* AppendElements(const mozilla::Array<Item, Length>& aArray) {
    return AppendElements<Item, ActualAlloc>(&aArray[0], Length);
  }

 public:
  template <class Item>
  /* MOZ_MUST_USE */
  elem_type* AppendElements(const Item* aArray, size_type aArrayLen,
                            const mozilla::fallible_t&) {
    return AppendElements<Item, FallibleAlloc>(aArray, aArrayLen);
  }

  template <class Item>
  /* MOZ_MUST_USE */
  elem_type* AppendElements(mozilla::Span<const Item> aSpan,
                            const mozilla::fallible_t&) {
    return AppendElements<Item, FallibleAlloc>(aSpan.Elements(),
                                               aSpan.Length());
  }

  // A variation on the AppendElements method defined above.
 protected:
  template <class Item, class Allocator, typename ActualAlloc = Alloc>
  elem_type* AppendElements(const nsTArray_Impl<Item, Allocator>& aArray) {
    return AppendElements<Item, ActualAlloc>(aArray.Elements(),
                                             aArray.Length());
  }

 public:
  template <class Item, class Allocator>
  /* MOZ_MUST_USE */
  elem_type* AppendElements(const nsTArray_Impl<Item, Allocator>& aArray,
                            const mozilla::fallible_t&) {
    return AppendElements<Item, Allocator, FallibleAlloc>(aArray);
  }

  // Move all elements from another array to the end of this array.
  // @return A pointer to the newly appended elements, or null on OOM.
 protected:
  template <class Item, class Allocator, typename ActualAlloc = Alloc>
  elem_type* AppendElements(nsTArray_Impl<Item, Allocator>&& aArray);

 public:
  template <class Item, class Allocator, typename ActualAlloc = Alloc>
  /* MOZ_MUST_USE */
  elem_type* AppendElements(nsTArray_Impl<Item, Allocator>&& aArray,
                            const mozilla::fallible_t&) {
    return AppendElements<Item, Allocator>(std::move(aArray));
  }

  // Append a new element, constructed in place from the provided arguments.
 protected:
  template <typename ActualAlloc, class... Args>
  elem_type* EmplaceBackInternal(Args&&... aItem);

 public:
  template <class... Args>
  MOZ_MUST_USE elem_type* EmplaceBack(const mozilla::fallible_t&,
                                      Args&&... aArgs) {
    return EmplaceBackInternal<FallibleAlloc, Args...>(
        std::forward<Args>(aArgs)...);
  }

  // Append a new element, move constructing if possible.
 protected:
  template <class Item, typename ActualAlloc = Alloc>
  elem_type* AppendElement(Item&& aItem);

 public:
  template <class Item>
  /* MOZ_MUST_USE */
  elem_type* AppendElement(Item&& aItem, const mozilla::fallible_t&) {
    return AppendElement<Item, FallibleAlloc>(std::forward<Item>(aItem));
  }

  // Append new elements without copy-constructing. This is useful to avoid
  // temporaries.
  // @return A pointer to the newly appended elements, or null on OOM.
 protected:
  template <typename ActualAlloc = Alloc>
  elem_type* AppendElements(size_type aCount) {
    if (!ActualAlloc::Successful(this->template ExtendCapacity<ActualAlloc>(
            Length(), aCount, sizeof(elem_type)))) {
      return nullptr;
    }
    elem_type* elems = Elements() + Length();
    size_type i;
    for (i = 0; i < aCount; ++i) {
      elem_traits::Construct(elems + i);
    }
    this->IncrementLength(aCount);
    return elems;
  }

 public:
  /* MOZ_MUST_USE */
  elem_type* AppendElements(size_type aCount, const mozilla::fallible_t&) {
    return AppendElements<FallibleAlloc>(aCount);
  }

  // Append a new element without copy-constructing. This is useful to avoid
  // temporaries.
  // @return A pointer to the newly appended element, or null on OOM.
 protected:
  template <typename ActualAlloc = Alloc>
  elem_type* AppendElement() {
    return AppendElements<ActualAlloc>(1);
  }

 public:
  /* MOZ_MUST_USE */
  elem_type* AppendElement(const mozilla::fallible_t&) {
    return AppendElement<FallibleAlloc>();
  }

  // This method removes a single element from this array, like
  // std::vector::erase.
  // @param pos to the element to remove
  const_iterator RemoveElementAt(const_iterator pos) {
    MOZ_ASSERT(pos.GetArray() == this);

    RemoveElementAt(pos.GetIndex());
    return pos;
  }

  // This method removes a range of elements from this array, like
  // std::vector::erase.
  // @param first iterator to the first of elements to remove
  // @param last iterator to the last of elements to remove
  const_iterator RemoveElementsAt(const_iterator first, const_iterator last) {
    MOZ_ASSERT(first.GetArray() == this);
    MOZ_ASSERT(last.GetArray() == this);
    MOZ_ASSERT(last.GetIndex() >= first.GetIndex());

    RemoveElementsAt(first.GetIndex(), last.GetIndex() - first.GetIndex());
    return first;
  }

  // This method removes a range of elements from this array.
  // @param aStart The starting index of the elements to remove.
  // @param aCount The number of elements to remove.
  void RemoveElementsAt(index_type aStart, size_type aCount);

 private:
  // Remove a range of elements from this array, but do not check that
  // the range is in bounds.
  // @param aStart The starting index of the elements to remove.
  // @param aCount The number of elements to remove.
  void RemoveElementsAtUnsafe(index_type aStart, size_type aCount);

 public:
  // A variation on the RemoveElementsAt method defined above.
  void RemoveElementAt(index_type aIndex) { RemoveElementsAt(aIndex, 1); }

  // A variation on the RemoveElementAt that removes the last element.
  void RemoveLastElement() { RemoveElementAt(Length() - 1); }

  // Removes the last element of the array and returns a copy of it.
  MOZ_MUST_USE
  elem_type PopLastElement() {
    elem_type elem = std::move(LastElement());
    RemoveLastElement();
    return elem;
  }

  // This method performs index-based removals from an array without preserving
  // the order of the array. This is useful if you are using the array as a
  // set-like data structure.
  //
  // These removals are efficient, as they move as few elements as possible. At
  // most N elements, where N is the number of removed elements, will have to
  // be relocated.
  //
  // ## Examples
  //
  // When removing an element from the end of the array, it can be removed in
  // place, by destroying it and decrementing the length.
  //
  // [ 1, 2, 3 ] => [ 1, 2 ]
  //         ^
  //
  // When removing any other single element, it is removed by swapping it with
  // the last element, and then decrementing the length as before.
  //
  // [ 1, 2, 3, 4, 5, 6 ]  => [ 1, 6, 3, 4, 5 ]
  //      ^
  //
  // This method also supports efficiently removing a range of elements. If they
  // are at the end, then they can all be removed like in the one element case.
  //
  // [ 1, 2, 3, 4, 5, 6 ] => [ 1, 2 ]
  //         ^--------^
  //
  // If more elements are removed than exist after the removed section, the
  // remaining elements will be shifted down like in a normal removal.
  //
  // [ 1, 2, 3, 4, 5, 6, 7, 8 ] => [ 1, 2, 7, 8 ]
  //         ^--------^
  //
  // And if fewer elements are removed than exist after the removed section,
  // elements will be moved from the end of the array to fill the vacated space.
  //
  // [ 1, 2, 3, 4, 5, 6, 7, 8 ] => [ 1, 7, 8, 4, 5, 6 ]
  //      ^--^
  //
  // @param aStart The starting index of the elements to remove. @param aCount
  // The number of elements to remove.
  void UnorderedRemoveElementsAt(index_type aStart, size_type aCount);

  // A variation on the UnorderedRemoveElementsAt method defined above to remove
  // a single element. This operation is sometimes called `SwapRemove`.
  //
  // This method is O(1), but does not preserve the order of the elements.
  void UnorderedRemoveElementAt(index_type aIndex) {
    UnorderedRemoveElementsAt(aIndex, 1);
  }

  void Clear() {
    ClearAndRetainStorage();
    Compact();
  }

  // This method removes elements based on the return value of the
  // callback function aPredicate. If the function returns true for
  // an element, the element is removed. aPredicate will be called
  // for each element in order. It is not safe to access the array
  // inside aPredicate.
  template <typename Predicate>
  void RemoveElementsBy(Predicate aPredicate);

  // This helper function combines IndexOf with RemoveElementAt to "search
  // and destroy" the first element that is equal to the given element.
  // @param aItem The item to search for.
  // @param aComp The Comparator used to determine element equality.
  // @return true if the element was found
  template <class Item, class Comparator>
  bool RemoveElement(const Item& aItem, const Comparator& aComp) {
    index_type i = IndexOf(aItem, 0, aComp);
    if (i == NoIndex) {
      return false;
    }

    RemoveElementsAtUnsafe(i, 1);
    return true;
  }

  // A variation on the RemoveElement method defined above that assumes
  // that 'operator==' is defined for elem_type.
  template <class Item>
  bool RemoveElement(const Item& aItem) {
    return RemoveElement(aItem, nsDefaultComparator<elem_type, Item>());
  }

  // This helper function combines IndexOfFirstElementGt with
  // RemoveElementAt to "search and destroy" the last element that
  // is equal to the given element.
  // @param aItem The item to search for.
  // @param aComp The Comparator used to determine element equality.
  // @return true if the element was found
  template <class Item, class Comparator>
  bool RemoveElementSorted(const Item& aItem, const Comparator& aComp) {
    index_type index = IndexOfFirstElementGt(aItem, aComp);
    if (index > 0 && aComp.Equals(ElementAt(index - 1), aItem)) {
      RemoveElementsAtUnsafe(index - 1, 1);
      return true;
    }
    return false;
  }

  // A variation on the RemoveElementSorted method defined above.
  template <class Item>
  bool RemoveElementSorted(const Item& aItem) {
    return RemoveElementSorted(aItem, nsDefaultComparator<elem_type, Item>());
  }

  // This method causes the elements contained in this array and the given
  // array to be swapped.
  template <class Allocator>
  typename Alloc::ResultType SwapElements(nsTArray_Impl<E, Allocator>& aOther) {
    return Alloc::Result(this->template SwapArrayElements<Alloc>(
        aOther, sizeof(elem_type), MOZ_ALIGNOF(elem_type)));
  }

 private:
  // Used by ApplyIf functions to invoke a callable that takes either:
  // - Nothing: F(void)
  // - Index only: F(size_t)
  // - Reference to element only: F(maybe-const elem_type&)
  // - Both index and reference: F(size_t, maybe-const elem_type&)
  // `elem_type` must be const when called from const method.
  template <typename T, typename Param0, typename Param1>
  struct InvokeWithIndexAndOrReferenceHelper {
    static constexpr bool valid = false;
  };
  template <typename T>
  struct InvokeWithIndexAndOrReferenceHelper<T, void, void> {
    static constexpr bool valid = true;
    template <typename F>
    static auto Invoke(F&& f, size_t, T&) {
      return f();
    }
  };
  template <typename T>
  struct InvokeWithIndexAndOrReferenceHelper<T, size_t, void> {
    static constexpr bool valid = true;
    template <typename F>
    static auto Invoke(F&& f, size_t i, T&) {
      return f(i);
    }
  };
  template <typename T>
  struct InvokeWithIndexAndOrReferenceHelper<T, T&, void> {
    static constexpr bool valid = true;
    template <typename F>
    static auto Invoke(F&& f, size_t, T& e) {
      return f(e);
    }
  };
  template <typename T>
  struct InvokeWithIndexAndOrReferenceHelper<T, const T&, void> {
    static constexpr bool valid = true;
    template <typename F>
    static auto Invoke(F&& f, size_t, T& e) {
      return f(e);
    }
  };
  template <typename T>
  struct InvokeWithIndexAndOrReferenceHelper<T, size_t, T&> {
    static constexpr bool valid = true;
    template <typename F>
    static auto Invoke(F&& f, size_t i, T& e) {
      return f(i, e);
    }
  };
  template <typename T>
  struct InvokeWithIndexAndOrReferenceHelper<T, size_t, const T&> {
    static constexpr bool valid = true;
    template <typename F>
    static auto Invoke(F&& f, size_t i, T& e) {
      return f(i, e);
    }
  };
  template <typename T, typename F>
  static auto InvokeWithIndexAndOrReference(F&& f, size_t i, T& e) {
    using Invoker = InvokeWithIndexAndOrReferenceHelper<
        T, typename mozilla::FunctionTypeTraits<F>::template ParameterType<0>,
        typename mozilla::FunctionTypeTraits<F>::template ParameterType<1>>;
    static_assert(Invoker::valid,
                  "ApplyIf's Function parameters must match either: (void), "
                  "(size_t), (maybe-const elem_type&), or "
                  "(size_t, maybe-const elem_type&)");
    return Invoker::Invoke(std::forward<F>(f), i, e);
  }

 public:
  // 'Apply' family of methods.
  //
  // The advantages of using Apply methods with lambdas include:
  // - Safety of accessing elements from within the call, when the array cannot
  //   have been modified between the iteration and the subsequent access.
  // - Avoiding moot conversions: pointer->index during a search, followed by
  //   index->pointer after the search when accessing the element.
  // - Embedding your code into the algorithm, giving the compiler more chances
  //   to optimize.

  // Search for the first element comparing equal to aItem with the given
  // comparator (`==` by default).
  // If such an element exists, return the result of evaluating either:
  // - `aFunction()`
  // - `aFunction(index_type)`
  // - `aFunction(maybe-const? elem_type&)`
  // - `aFunction(index_type, maybe-const? elem_type&)`
  // (`aFunction` must have one of the above signatures with these exact types,
  //  including references; implicit conversions or generic types not allowed.
  //  If `this` array is const, the referenced `elem_type` must be const too;
  //  otherwise it may be either const or non-const.)
  // But if the element is not found, return the result of evaluating
  // `aFunctionElse()`.
  template <class Item, class Comparator, class Function, class FunctionElse>
  auto ApplyIf(const Item& aItem, index_type aStart, const Comparator& aComp,
               Function&& aFunction, FunctionElse&& aFunctionElse) const {
    static_assert(
        mozilla::IsSame<
            typename mozilla::FunctionTypeTraits<Function>::ReturnType,
            typename mozilla::FunctionTypeTraits<FunctionElse>::ReturnType>::
            value,
        "ApplyIf's `Function` and `FunctionElse` must return the same type.");

    ::detail::CompareWrapper<Comparator, Item> comp(aComp);

    const elem_type* const elements = Elements();
    const elem_type* const iend = elements + Length();
    for (const elem_type* iter = elements + aStart; iter != iend; ++iter) {
      if (comp.Equals(*iter, aItem)) {
        return InvokeWithIndexAndOrReference<const elem_type>(
            std::forward<Function>(aFunction), iter - elements, *iter);
      }
    }
    return aFunctionElse();
  }
  template <class Item, class Comparator, class Function, class FunctionElse>
  auto ApplyIf(const Item& aItem, index_type aStart, const Comparator& aComp,
               Function&& aFunction, FunctionElse&& aFunctionElse) {
    static_assert(
        mozilla::IsSame<
            typename mozilla::FunctionTypeTraits<Function>::ReturnType,
            typename mozilla::FunctionTypeTraits<FunctionElse>::ReturnType>::
            value,
        "ApplyIf's `Function` and `FunctionElse` must return the same type.");

    ::detail::CompareWrapper<Comparator, Item> comp(aComp);

    elem_type* const elements = Elements();
    elem_type* const iend = elements + Length();
    for (elem_type* iter = elements + aStart; iter != iend; ++iter) {
      if (comp.Equals(*iter, aItem)) {
        return InvokeWithIndexAndOrReference<elem_type>(
            std::forward<Function>(aFunction), iter - elements, *iter);
      }
    }
    return aFunctionElse();
  }
  template <class Item, class Function, class FunctionElse>
  auto ApplyIf(const Item& aItem, index_type aStart, Function&& aFunction,
               FunctionElse&& aFunctionElse) const {
    return ApplyIf(aItem, aStart, nsDefaultComparator<elem_type, Item>(),
                   std::forward<Function>(aFunction),
                   std::forward<FunctionElse>(aFunctionElse));
  }
  template <class Item, class Function, class FunctionElse>
  auto ApplyIf(const Item& aItem, index_type aStart, Function&& aFunction,
               FunctionElse&& aFunctionElse) {
    return ApplyIf(aItem, aStart, nsDefaultComparator<elem_type, Item>(),
                   std::forward<Function>(aFunction),
                   std::forward<FunctionElse>(aFunctionElse));
  }
  template <class Item, class Function, class FunctionElse>
  auto ApplyIf(const Item& aItem, Function&& aFunction,
               FunctionElse&& aFunctionElse) const {
    return ApplyIf(aItem, 0, std::forward<Function>(aFunction),
                   std::forward<FunctionElse>(aFunctionElse));
  }
  template <class Item, class Function, class FunctionElse>
  auto ApplyIf(const Item& aItem, Function&& aFunction,
               FunctionElse&& aFunctionElse) {
    return ApplyIf(aItem, 0, std::forward<Function>(aFunction),
                   std::forward<FunctionElse>(aFunctionElse));
  }

  //
  // Allocation
  //

  // This method may increase the capacity of this array object to the
  // specified amount.  This method may be called in advance of several
  // AppendElement operations to minimize heap re-allocations.  This method
  // will not reduce the number of elements in this array.
  // @param aCapacity The desired capacity of this array.
  // @return True if the operation succeeded; false if we ran out of memory
 protected:
  template <typename ActualAlloc = Alloc>
  typename ActualAlloc::ResultType SetCapacity(size_type aCapacity) {
    return ActualAlloc::Result(this->template EnsureCapacity<ActualAlloc>(
        aCapacity, sizeof(elem_type)));
  }

 public:
  MOZ_MUST_USE
  bool SetCapacity(size_type aCapacity, const mozilla::fallible_t&) {
    return SetCapacity<FallibleAlloc>(aCapacity);
  }

  // This method modifies the length of the array.  If the new length is
  // larger than the existing length of the array, then new elements will be
  // constructed using elem_type's default constructor.  Otherwise, this call
  // removes elements from the array (see also RemoveElementsAt).
  // @param aNewLen The desired length of this array.
  // @return True if the operation succeeded; false otherwise.
  // See also TruncateLength if the new length is guaranteed to be smaller than
  // the old.
 protected:
  template <typename ActualAlloc = Alloc>
  typename ActualAlloc::ResultType SetLength(size_type aNewLen) {
    size_type oldLen = Length();
    if (aNewLen > oldLen) {
      return ActualAlloc::ConvertBoolToResultType(
          InsertElementsAt<ActualAlloc>(oldLen, aNewLen - oldLen) != nullptr);
    }

    TruncateLength(aNewLen);
    return ActualAlloc::ConvertBoolToResultType(true);
  }

 public:
  MOZ_MUST_USE
  bool SetLength(size_type aNewLen, const mozilla::fallible_t&) {
    return SetLength<FallibleAlloc>(aNewLen);
  }

  // This method modifies the length of the array, but may only be
  // called when the new length is shorter than the old.  It can
  // therefore be called when elem_type has no default constructor,
  // unlike SetLength.  It removes elements from the array (see also
  // RemoveElementsAt).
  // @param aNewLen The desired length of this array.
  void TruncateLength(size_type aNewLen) {
    size_type oldLen = Length();
    MOZ_ASSERT(aNewLen <= oldLen, "caller should use SetLength instead");
    RemoveElementsAt(aNewLen, oldLen - aNewLen);
  }

  // This method ensures that the array has length at least the given
  // length.  If the current length is shorter than the given length,
  // then new elements will be constructed using elem_type's default
  // constructor.
  // @param aMinLen The desired minimum length of this array.
  // @return True if the operation succeeded; false otherwise.
 protected:
  template <typename ActualAlloc = Alloc>
  typename ActualAlloc::ResultType EnsureLengthAtLeast(size_type aMinLen) {
    size_type oldLen = Length();
    if (aMinLen > oldLen) {
      return ActualAlloc::ConvertBoolToResultType(
          !!InsertElementsAt<ActualAlloc>(oldLen, aMinLen - oldLen));
    }
    return ActualAlloc::ConvertBoolToResultType(true);
  }

 public:
  MOZ_MUST_USE
  bool EnsureLengthAtLeast(size_type aMinLen, const mozilla::fallible_t&) {
    return EnsureLengthAtLeast<FallibleAlloc>(aMinLen);
  }

  // This method inserts elements into the array, constructing
  // them using elem_type's default constructor.
  // @param aIndex the place to insert the new elements. This must be no
  //               greater than the current length of the array.
  // @param aCount the number of elements to insert
 protected:
  template <typename ActualAlloc = Alloc>
  elem_type* InsertElementsAt(index_type aIndex, size_type aCount) {
    if (!ActualAlloc::Successful(this->template InsertSlotsAt<ActualAlloc>(
            aIndex, aCount, sizeof(elem_type), MOZ_ALIGNOF(elem_type)))) {
      return nullptr;
    }

    // Initialize the extra array elements
    elem_type* iter = Elements() + aIndex;
    elem_type* iend = iter + aCount;
    for (; iter != iend; ++iter) {
      elem_traits::Construct(iter);
    }

    return Elements() + aIndex;
  }

 public:
  MOZ_MUST_USE
  elem_type* InsertElementsAt(index_type aIndex, size_type aCount,
                              const mozilla::fallible_t&) {
    return InsertElementsAt<FallibleAlloc>(aIndex, aCount);
  }

  // This method inserts elements into the array, constructing them
  // elem_type's copy constructor (or whatever one-arg constructor
  // happens to match the Item type).
  // @param aIndex the place to insert the new elements. This must be no
  //               greater than the current length of the array.
  // @param aCount the number of elements to insert.
  // @param aItem the value to use when constructing the new elements.
 protected:
  template <class Item, typename ActualAlloc = Alloc>
  elem_type* InsertElementsAt(index_type aIndex, size_type aCount,
                              const Item& aItem);

 public:
  template <class Item>
  MOZ_MUST_USE elem_type* InsertElementsAt(index_type aIndex, size_type aCount,
                                           const Item& aItem,
                                           const mozilla::fallible_t&) {
    return InsertElementsAt<Item, FallibleAlloc>(aIndex, aCount, aItem);
  }

  // This method may be called to minimize the memory used by this array.
  void Compact() { ShrinkCapacity(sizeof(elem_type), MOZ_ALIGNOF(elem_type)); }

  //
  // Sorting
  //

  // This function is meant to be used with the NS_QuickSort function.  It
  // maps the callback API expected by NS_QuickSort to the Comparator API
  // used by nsTArray_Impl.  See nsTArray_Impl::Sort.
  template <class Comparator>
  static int Compare(const void* aE1, const void* aE2, void* aData) {
    const Comparator* c = reinterpret_cast<const Comparator*>(aData);
    const elem_type* a = static_cast<const elem_type*>(aE1);
    const elem_type* b = static_cast<const elem_type*>(aE2);
    return c->Compare(*a, *b);
  }

  // This method sorts the elements of the array.  It uses the LessThan
  // method defined on the given Comparator object to collate elements.
  // @param aComp The Comparator used to collate elements.
  template <class Comparator>
  void Sort(const Comparator& aComp) {
    ::detail::CompareWrapper<Comparator, elem_type> comp(aComp);

    NS_QuickSort(Elements(), Length(), sizeof(elem_type),
                 Compare<decltype(comp)>, &comp);
  }

  // A variation on the Sort method defined above that assumes that
  // 'operator<' is defined for elem_type.
  void Sort() { Sort(nsDefaultComparator<elem_type, elem_type>()); }

  // This method reverses the array in place.
  void Reverse() {
    elem_type* elements = Elements();
    const size_type len = Length();
    for (index_type i = 0, iend = len / 2; i < iend; ++i) {
      std::swap(elements[i], elements[len - i - 1]);
    }
  }

 protected:
  using base_type::Hdr;
  using base_type::ShrinkCapacity;

  // This method invokes elem_type's destructor on a range of elements.
  // @param aStart The index of the first element to destroy.
  // @param aCount The number of elements to destroy.
  void DestructRange(index_type aStart, size_type aCount) {
    elem_type* iter = Elements() + aStart;
    elem_type* iend = iter + aCount;
    for (; iter != iend; ++iter) {
      elem_traits::Destruct(iter);
    }
  }

  // This method invokes elem_type's copy-constructor on a range of elements.
  // @param aStart  The index of the first element to construct.
  // @param aCount  The number of elements to construct.
  // @param aValues The array of elements to copy.
  template <class Item>
  void AssignRange(index_type aStart, size_type aCount, const Item* aValues) {
    AssignRangeAlgorithm<
        mozilla::IsPod<Item>::value,
        mozilla::IsSame<Item, elem_type>::value>::implementation(Elements(),
                                                                 aStart, aCount,
                                                                 aValues);
  }
};

template <typename E, class Alloc>
template <class Item, typename ActualAlloc>
auto nsTArray_Impl<E, Alloc>::ReplaceElementsAt(index_type aStart,
                                                size_type aCount,
                                                const Item* aArray,
                                                size_type aArrayLen)
    -> elem_type* {
  if (MOZ_UNLIKELY(aStart > Length())) {
    InvalidArrayIndex_CRASH(aStart, Length());
  }

  // Adjust memory allocation up-front to catch errors.
  if (!ActualAlloc::Successful(this->template EnsureCapacity<ActualAlloc>(
          Length() + aArrayLen - aCount, sizeof(elem_type)))) {
    return nullptr;
  }
  DestructRange(aStart, aCount);
  this->template ShiftData<ActualAlloc>(
      aStart, aCount, aArrayLen, sizeof(elem_type), MOZ_ALIGNOF(elem_type));
  AssignRange(aStart, aArrayLen, aArray);
  return Elements() + aStart;
}

template <typename E, class Alloc>
void nsTArray_Impl<E, Alloc>::RemoveElementsAt(index_type aStart,
                                               size_type aCount) {
  MOZ_ASSERT(aCount == 0 || aStart < Length(), "Invalid aStart index");

  mozilla::CheckedInt<index_type> rangeEnd = aStart;
  rangeEnd += aCount;

  if (MOZ_UNLIKELY(!rangeEnd.isValid() || rangeEnd.value() > Length())) {
    InvalidArrayIndex_CRASH(aStart, Length());
  }

  RemoveElementsAtUnsafe(aStart, aCount);
}

template <typename E, class Alloc>
void nsTArray_Impl<E, Alloc>::RemoveElementsAtUnsafe(index_type aStart,
                                                     size_type aCount) {
  DestructRange(aStart, aCount);
  this->template ShiftData<InfallibleAlloc>(
      aStart, aCount, 0, sizeof(elem_type), MOZ_ALIGNOF(elem_type));
}

template <typename E, class Alloc>
void nsTArray_Impl<E, Alloc>::UnorderedRemoveElementsAt(index_type aStart,
                                                        size_type aCount) {
  MOZ_ASSERT(aCount == 0 || aStart < Length(), "Invalid aStart index");

  mozilla::CheckedInt<index_type> rangeEnd = aStart;
  rangeEnd += aCount;

  if (MOZ_UNLIKELY(!rangeEnd.isValid() || rangeEnd.value() > Length())) {
    InvalidArrayIndex_CRASH(aStart, Length());
  }

  // Destroy the elements which are being removed, and then swap elements in to
  // replace them from the end. See the docs on the declaration of this
  // function.
  DestructRange(aStart, aCount);
  this->template SwapFromEnd<InfallibleAlloc>(aStart, aCount, sizeof(elem_type),
                                              MOZ_ALIGNOF(elem_type));
}

template <typename E, class Alloc>
template <typename Predicate>
void nsTArray_Impl<E, Alloc>::RemoveElementsBy(Predicate aPredicate) {
  if (base_type::mHdr == EmptyHdr()) {
    return;
  }

  index_type j = 0;
  index_type len = Length();
  for (index_type i = 0; i < len; ++i) {
    if (aPredicate(Elements()[i])) {
      elem_traits::Destruct(Elements() + i);
    } else {
      if (j < i) {
        relocation_type::RelocateNonOverlappingRegion(
            Elements() + j, Elements() + i, 1, sizeof(elem_type));
      }
      ++j;
    }
  }
  base_type::mHdr->mLength = j;
}

template <typename E, class Alloc>
template <class Item, typename ActualAlloc>
auto nsTArray_Impl<E, Alloc>::InsertElementsAt(index_type aIndex,
                                               size_type aCount,
                                               const Item& aItem)
    -> elem_type* {
  if (!ActualAlloc::Successful(this->template InsertSlotsAt<ActualAlloc>(
          aIndex, aCount, sizeof(elem_type), MOZ_ALIGNOF(elem_type)))) {
    return nullptr;
  }

  // Initialize the extra array elements
  elem_type* iter = Elements() + aIndex;
  elem_type* iend = iter + aCount;
  for (; iter != iend; ++iter) {
    elem_traits::Construct(iter, aItem);
  }

  return Elements() + aIndex;
}

template <typename E, class Alloc>
template <typename ActualAlloc>
auto nsTArray_Impl<E, Alloc>::InsertElementAt(index_type aIndex) -> elem_type* {
  if (MOZ_UNLIKELY(aIndex > Length())) {
    InvalidArrayIndex_CRASH(aIndex, Length());
  }

  // Length() + 1 is guaranteed to not overflow, so EnsureCapacity is OK.
  if (!ActualAlloc::Successful(this->template EnsureCapacity<ActualAlloc>(
          Length() + 1, sizeof(elem_type)))) {
    return nullptr;
  }
  this->template ShiftData<ActualAlloc>(aIndex, 0, 1, sizeof(elem_type),
                                        MOZ_ALIGNOF(elem_type));
  elem_type* elem = Elements() + aIndex;
  elem_traits::Construct(elem);
  return elem;
}

template <typename E, class Alloc>
template <class Item, typename ActualAlloc>
auto nsTArray_Impl<E, Alloc>::InsertElementAt(index_type aIndex, Item&& aItem)
    -> elem_type* {
  if (MOZ_UNLIKELY(aIndex > Length())) {
    InvalidArrayIndex_CRASH(aIndex, Length());
  }

  // Length() + 1 is guaranteed to not overflow, so EnsureCapacity is OK.
  if (!ActualAlloc::Successful(this->template EnsureCapacity<ActualAlloc>(
          Length() + 1, sizeof(elem_type)))) {
    return nullptr;
  }
  this->template ShiftData<ActualAlloc>(aIndex, 0, 1, sizeof(elem_type),
                                        MOZ_ALIGNOF(elem_type));
  elem_type* elem = Elements() + aIndex;
  elem_traits::Construct(elem, std::forward<Item>(aItem));
  return elem;
}

template <typename E, class Alloc>
template <class Item, typename ActualAlloc>
auto nsTArray_Impl<E, Alloc>::AppendElements(const Item* aArray,
                                             size_type aArrayLen)
    -> elem_type* {
  if (!ActualAlloc::Successful(this->template ExtendCapacity<ActualAlloc>(
          Length(), aArrayLen, sizeof(elem_type)))) {
    return nullptr;
  }
  index_type len = Length();
  AssignRange(len, aArrayLen, aArray);
  this->IncrementLength(aArrayLen);
  return Elements() + len;
}

template <typename E, class Alloc>
template <class Item, class Allocator, typename ActualAlloc>
auto nsTArray_Impl<E, Alloc>::AppendElements(
    nsTArray_Impl<Item, Allocator>&& aArray) -> elem_type* {
  MOZ_ASSERT(&aArray != this, "argument must be different aArray");
  if (Length() == 0) {
    SwapElements<ActualAlloc>(aArray);
    return Elements();
  }

  index_type len = Length();
  index_type otherLen = aArray.Length();
  if (!Alloc::Successful(this->template ExtendCapacity<Alloc>(
          len, otherLen, sizeof(elem_type)))) {
    return nullptr;
  }
  relocation_type::RelocateNonOverlappingRegion(
      Elements() + len, aArray.Elements(), otherLen, sizeof(elem_type));
  this->IncrementLength(otherLen);
  aArray.template ShiftData<Alloc>(0, otherLen, 0, sizeof(elem_type),
                                   MOZ_ALIGNOF(elem_type));
  return Elements() + len;
}

template <typename E, class Alloc>
template <class Item, typename ActualAlloc>
auto nsTArray_Impl<E, Alloc>::AppendElement(Item&& aItem) -> elem_type* {
  // Length() + 1 is guaranteed to not overflow, so EnsureCapacity is OK.
  if (!ActualAlloc::Successful(this->template EnsureCapacity<ActualAlloc>(
          Length() + 1, sizeof(elem_type)))) {
    return nullptr;
  }
  elem_type* elem = Elements() + Length();
  elem_traits::Construct(elem, std::forward<Item>(aItem));
  this->mHdr->mLength += 1;
  return elem;
}

template <typename E, class Alloc>
template <typename ActualAlloc, class... Args>
auto nsTArray_Impl<E, Alloc>::EmplaceBackInternal(Args&&... aArgs)
    -> elem_type* {
  // Length() + 1 is guaranteed to not overflow, so EnsureCapacity is OK.
  if (!ActualAlloc::Successful(this->template EnsureCapacity<ActualAlloc>(
          Length() + 1, sizeof(elem_type)))) {
    return nullptr;
  }
  elem_type* elem = Elements() + Length();
  elem_traits::Emplace(elem, std::forward<Args>(aArgs)...);
  this->mHdr->mLength += 1;
  return elem;
}

template <typename E, typename Alloc>
inline void ImplCycleCollectionUnlink(nsTArray_Impl<E, Alloc>& aField) {
  aField.Clear();
}

template <typename E, typename Alloc>
inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback,
    nsTArray_Impl<E, Alloc>& aField, const char* aName, uint32_t aFlags = 0) {
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
template <class E>
class nsTArray : public nsTArray_Impl<E, nsTArrayInfallibleAllocator> {
 public:
  typedef nsTArray_Impl<E, nsTArrayInfallibleAllocator> base_type;
  typedef nsTArray<E> self_type;
  typedef typename base_type::size_type size_type;

  nsTArray() {}
  explicit nsTArray(size_type aCapacity) : base_type(aCapacity) {}
  MOZ_IMPLICIT nsTArray(std::initializer_list<E> aIL) : base_type(aIL) {}

  template <class Allocator>
  explicit nsTArray(const nsTArray_Impl<E, Allocator>& aOther)
      : base_type(aOther) {}
  template <class Allocator>
  MOZ_IMPLICIT nsTArray(nsTArray_Impl<E, Allocator>&& aOther)
      : base_type(std::move(aOther)) {}

  template <class Allocator>
  self_type& operator=(const nsTArray_Impl<E, Allocator>& aOther) {
    base_type::operator=(aOther);
    return *this;
  }
  template <class Allocator>
  self_type& operator=(nsTArray_Impl<E, Allocator>&& aOther) {
    base_type::operator=(std::move(aOther));
    return *this;
  }

  using base_type::AppendElement;
  using base_type::AppendElements;
  using base_type::EmplaceBack;
  using base_type::EnsureLengthAtLeast;
  using base_type::InsertElementAt;
  using base_type::InsertElementsAt;
  using base_type::InsertElementSorted;
  using base_type::ReplaceElementsAt;
  using base_type::SetCapacity;
  using base_type::SetLength;

  template <class... Args>
  typename base_type::elem_type* EmplaceBack(Args&&... aArgs) {
    return this
        ->template EmplaceBackInternal<nsTArrayInfallibleAllocator, Args...>(
            std::forward<Args>(aArgs)...);
  }
};

//
// FallibleTArray is a fallible vector class.
//
template <class E>
class FallibleTArray : public nsTArray_Impl<E, nsTArrayFallibleAllocator> {
 public:
  typedef nsTArray_Impl<E, nsTArrayFallibleAllocator> base_type;
  typedef FallibleTArray<E> self_type;
  typedef typename base_type::size_type size_type;

  FallibleTArray() = default;
  explicit FallibleTArray(size_type aCapacity) : base_type(aCapacity) {}

  template <class Allocator>
  explicit FallibleTArray(const nsTArray_Impl<E, Allocator>& aOther)
      : base_type(aOther) {}
  template <class Allocator>
  explicit FallibleTArray(nsTArray_Impl<E, Allocator>&& aOther)
      : base_type(std::move(aOther)) {}

  template <class Allocator>
  self_type& operator=(const nsTArray_Impl<E, Allocator>& aOther) {
    base_type::operator=(aOther);
    return *this;
  }
  template <class Allocator>
  self_type& operator=(nsTArray_Impl<E, Allocator>&& aOther) {
    base_type::operator=(std::move(aOther));
    return *this;
  }
};

//
// AutoTArray<E, N> is like nsTArray<E>, but with N elements of inline storage.
// Storing more than N elements is fine, but it will cause a heap allocation.
//
template <class E, size_t N>
class MOZ_NON_MEMMOVABLE AutoTArray : public nsTArray<E> {
  static_assert(N != 0, "AutoTArray<E, 0> should be specialized");

 public:
  typedef AutoTArray<E, N> self_type;
  typedef nsTArray<E> base_type;
  typedef typename base_type::Header Header;
  typedef typename base_type::elem_type elem_type;

  AutoTArray() : mAlign() { Init(); }

  AutoTArray(const self_type& aOther) : nsTArray<E>() {
    Init();
    this->AppendElements(aOther);
  }

  AutoTArray(self_type&& aOther) : nsTArray<E>() {
    Init();
    this->SwapElements(aOther);
  }

  explicit AutoTArray(const base_type& aOther) : mAlign() {
    Init();
    this->AppendElements(aOther);
  }

  explicit AutoTArray(base_type&& aOther) : mAlign() {
    Init();
    this->SwapElements(aOther);
  }

  template <typename Allocator>
  explicit AutoTArray(nsTArray_Impl<elem_type, Allocator>&& aOther) {
    Init();
    this->SwapElements(aOther);
  }

  MOZ_IMPLICIT AutoTArray(std::initializer_list<E> aIL) : mAlign() {
    Init();
    this->AppendElements(aIL.begin(), aIL.size());
  }

  self_type& operator=(const self_type& aOther) {
    base_type::operator=(aOther);
    return *this;
  }

  self_type& operator=(self_type&& aOther) {
    base_type::operator=(std::move(aOther));
    return *this;
  }

  template <typename Allocator>
  self_type& operator=(nsTArray_Impl<elem_type, Allocator>&& aOther) {
    base_type::operator=(std::move(aOther));
    return *this;
  }

  template <typename Allocator>
  self_type& operator=(const nsTArray_Impl<elem_type, Allocator>& aOther) {
    base_type::operator=(aOther);
    return *this;
  }

 private:
  // nsTArray_base casts itself as an nsAutoArrayBase in order to get a pointer
  // to mAutoBuf.
  template <class Allocator, class RelocationStrategy>
  friend class nsTArray_base;

  void Init() {
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
  union {
    char mAutoBuf[sizeof(nsTArrayHeader) + N * sizeof(elem_type)];
    // Do the max operation inline to ensure that it is a compile-time constant.
    mozilla::AlignedElem<(MOZ_ALIGNOF(Header) > MOZ_ALIGNOF(elem_type))
                             ? MOZ_ALIGNOF(Header)
                             : MOZ_ALIGNOF(elem_type)>
        mAlign;
  };
};

//
// Specialization of AutoTArray<E, N> for the case where N == 0.
// AutoTArray<E, 0> behaves exactly like nsTArray<E>, but without this
// specialization, it stores a useless inline header.
//
// We do have many AutoTArray<E, 0> objects in memory: about 2,000 per tab as
// of May 2014. These are typically not explicitly AutoTArray<E, 0> but rather
// AutoTArray<E, N> for some value N depending on template parameters, in
// generic code.
//
// For that reason, we optimize this case with the below partial specialization,
// which ensures that AutoTArray<E, 0> is just like nsTArray<E>, without any
// inline header overhead.
//
template <class E>
class AutoTArray<E, 0> : public nsTArray<E> {};

template <class E, size_t N>
struct nsTArray_RelocationStrategy<AutoTArray<E, N>> {
  using Type = nsTArray_RelocateUsingMoveConstructor<AutoTArray<E, N>>;
};

// Span integration
namespace mozilla {

template <class ElementType, class TArrayAlloc>
Span<ElementType> MakeSpan(nsTArray_Impl<ElementType, TArrayAlloc>& aTArray) {
  return aTArray;
}

template <class ElementType, class TArrayAlloc>
Span<const ElementType> MakeSpan(
    const nsTArray_Impl<ElementType, TArrayAlloc>& aTArray) {
  return aTArray;
}

template <typename T>
class nsTArrayBackInserter
    : public std::iterator<std::output_iterator_tag, void, void, void, void> {
  nsTArray<T>* mArray;

 public:
  explicit nsTArrayBackInserter(nsTArray<T>& aArray) : mArray{&aArray} {}

  nsTArrayBackInserter& operator=(const T& aValue) {
    mArray->AppendElement(aValue);
    return *this;
  }

  nsTArrayBackInserter& operator=(T&& aValue) {
    mArray->AppendElement(std::move(aValue));
    return *this;
  }

  nsTArrayBackInserter& operator*() { return *this; }

  void operator++() {}
};

template <typename T>
auto MakeBackInserter(nsTArray<T>& aArray) {
  return nsTArrayBackInserter<T>{aArray};
}

}  // namespace mozilla

// MOZ_DBG support

template <class E, class Alloc>
std::ostream& operator<<(std::ostream& aOut,
                         const nsTArray_Impl<E, Alloc>& aTArray) {
  return aOut << mozilla::MakeSpan(aTArray);
}

// Assert that AutoTArray doesn't have any extra padding inside.
//
// It's important that the data stored in this auto array takes up a multiple of
// 8 bytes; e.g. AutoTArray<uint32_t, 1> wouldn't work.  Since AutoTArray
// contains a pointer, its size must be a multiple of alignof(void*).  (This is
// because any type may be placed into an array, and there's no padding between
// elements of an array.)  The compiler pads the end of the structure to
// enforce this rule.
//
// If we used AutoTArray<uint32_t, 1> below, this assertion would fail on a
// 64-bit system, where the compiler inserts 4 bytes of padding at the end of
// the auto array to make its size a multiple of alignof(void*) == 8 bytes.

static_assert(sizeof(AutoTArray<uint32_t, 2>) ==
                  sizeof(void*) + sizeof(nsTArrayHeader) + sizeof(uint32_t) * 2,
              "AutoTArray shouldn't contain any extra padding, "
              "see the comment");

// Definitions of nsTArray_Impl methods
#include "nsTArray-inl.h"

#endif  // nsTArray_h__
