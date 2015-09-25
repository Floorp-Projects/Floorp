/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nsRefPtr_h
#define mozilla_nsRefPtr_h

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"

/*****************************************************************************/

// template <class T> class nsRefPtrGetterAddRefs;

class nsCOMPtr_helper;

namespace mozilla {
template<class T> class OwningNonNull;
template<class T> class RefPtr;
} // namespace mozilla

template <class T>
class nsRefPtr
{
private:
  void
  assign_with_AddRef(T* aRawPtr)
  {
    if (aRawPtr) {
      AddRefTraits<T>::AddRef(aRawPtr);
    }
    assign_assuming_AddRef(aRawPtr);
  }

  void
  assign_assuming_AddRef(T* aNewPtr)
  {
    T* oldPtr = mRawPtr;
    mRawPtr = aNewPtr;
    if (oldPtr) {
      AddRefTraits<T>::Release(oldPtr);
    }
  }

private:
  T* MOZ_OWNING_REF mRawPtr;

public:
  typedef T element_type;

  ~nsRefPtr()
  {
    if (mRawPtr) {
      AddRefTraits<T>::Release(mRawPtr);
    }
  }

  // Constructors

  nsRefPtr()
    : mRawPtr(0)
    // default constructor
  {
  }

  nsRefPtr(const nsRefPtr<T>& aSmartPtr)
    : mRawPtr(aSmartPtr.mRawPtr)
    // copy-constructor
  {
    if (mRawPtr) {
      AddRefTraits<T>::AddRef(mRawPtr);
    }
  }

  nsRefPtr(nsRefPtr<T>&& aRefPtr)
    : mRawPtr(aRefPtr.mRawPtr)
  {
    aRefPtr.mRawPtr = nullptr;
  }

  // construct from a raw pointer (of the right type)

  MOZ_IMPLICIT nsRefPtr(T* aRawPtr)
    : mRawPtr(aRawPtr)
  {
    if (mRawPtr) {
      AddRefTraits<T>::AddRef(mRawPtr);
    }
  }

  template <typename I>
  MOZ_IMPLICIT nsRefPtr(already_AddRefed<I>& aSmartPtr)
    : mRawPtr(aSmartPtr.take())
    // construct from |already_AddRefed|
  {
  }

  template <typename I>
  MOZ_IMPLICIT nsRefPtr(already_AddRefed<I>&& aSmartPtr)
    : mRawPtr(aSmartPtr.take())
    // construct from |otherRefPtr.forget()|
  {
  }

  template <typename I>
  MOZ_IMPLICIT nsRefPtr(const nsRefPtr<I>& aSmartPtr)
    : mRawPtr(aSmartPtr.get())
    // copy-construct from a smart pointer with a related pointer type
  {
    if (mRawPtr) {
      AddRefTraits<T>::AddRef(mRawPtr);
    }
  }

  template <typename I>
  MOZ_IMPLICIT nsRefPtr(nsRefPtr<I>&& aSmartPtr)
    : mRawPtr(aSmartPtr.forget().take())
    // construct from |Move(nsRefPtr<SomeSubclassOfT>)|.
  {
  }

  MOZ_IMPLICIT nsRefPtr(const nsCOMPtr_helper& aHelper);

  // Defined in OwningNonNull.h
  template<class U>
  MOZ_IMPLICIT nsRefPtr(const mozilla::OwningNonNull<U>& aOther);

  // Defined in RefPtr.h
  template<class U>
  MOZ_IMPLICIT nsRefPtr(mozilla::RefPtr<U>&& aOther);

  // Assignment operators

  nsRefPtr<T>&
  operator=(const nsRefPtr<T>& aRhs)
  // copy assignment operator
  {
    assign_with_AddRef(aRhs.mRawPtr);
    return *this;
  }

  template <typename I>
  nsRefPtr<T>&
  operator=(const nsRefPtr<I>& aRhs)
  // assign from an nsRefPtr of a related pointer type
  {
    assign_with_AddRef(aRhs.get());
    return *this;
  }

  nsRefPtr<T>&
  operator=(T* aRhs)
  // assign from a raw pointer (of the right type)
  {
    assign_with_AddRef(aRhs);
    return *this;
  }

  template <typename I>
  nsRefPtr<T>&
  operator=(already_AddRefed<I>& aRhs)
  // assign from |already_AddRefed|
  {
    assign_assuming_AddRef(aRhs.take());
    return *this;
  }

  template <typename I>
  nsRefPtr<T>&
  operator=(already_AddRefed<I> && aRhs)
  // assign from |otherRefPtr.forget()|
  {
    assign_assuming_AddRef(aRhs.take());
    return *this;
  }

  nsRefPtr<T>& operator=(const nsCOMPtr_helper& aHelper);

  nsRefPtr<T>&
  operator=(nsRefPtr<T> && aRefPtr)
  {
    assign_assuming_AddRef(aRefPtr.mRawPtr);
    aRefPtr.mRawPtr = nullptr;
    return *this;
  }

  // Defined in OwningNonNull.h
  template<class U>
  nsRefPtr<T>&
  operator=(const mozilla::OwningNonNull<U>& aOther);

  // Defined in RefPtr.h
  template<class U>
  nsRefPtr<T>&
  operator=(mozilla::RefPtr<U>&& aOther);

  // Other pointer operators

  void
  swap(nsRefPtr<T>& aRhs)
  // ...exchange ownership with |aRhs|; can save a pair of refcount operations
  {
    T* temp = aRhs.mRawPtr;
    aRhs.mRawPtr = mRawPtr;
    mRawPtr = temp;
  }

  void
  swap(T*& aRhs)
  // ...exchange ownership with |aRhs|; can save a pair of refcount operations
  {
    T* temp = aRhs;
    aRhs = mRawPtr;
    mRawPtr = temp;
  }

  already_AddRefed<T>
  forget()
  // return the value of mRawPtr and null out mRawPtr. Useful for
  // already_AddRefed return values.
  {
    T* temp = 0;
    swap(temp);
    return already_AddRefed<T>(temp);
  }

  template <typename I>
  void
  forget(I** aRhs)
  // Set the target of aRhs to the value of mRawPtr and null out mRawPtr.
  // Useful to avoid unnecessary AddRef/Release pairs with "out"
  // parameters where aRhs bay be a T** or an I** where I is a base class
  // of T.
  {
    MOZ_ASSERT(aRhs, "Null pointer passed to forget!");
    *aRhs = mRawPtr;
    mRawPtr = 0;
  }

  T*
  get() const
  /*
    Prefer the implicit conversion provided automatically by |operator T*() const|.
    Use |get()| to resolve ambiguity or to get a castable pointer.
  */
  {
    return const_cast<T*>(mRawPtr);
  }

  operator T*() const
#ifdef MOZ_HAVE_REF_QUALIFIERS
  &
#endif
  /*
    ...makes an |nsRefPtr| act like its underlying raw pointer type whenever it
    is used in a context where a raw pointer is expected.  It is this operator
    that makes an |nsRefPtr| substitutable for a raw pointer.

    Prefer the implicit use of this operator to calling |get()|, except where
    necessary to resolve ambiguity.
  */
  {
    return get();
  }

#ifdef MOZ_HAVE_REF_QUALIFIERS
  // Don't allow implicit conversion of temporary nsRefPtr to raw pointer,
  // because the refcount might be one and the pointer will immediately become
  // invalid.
  operator T*() const && = delete;

  // These are needed to avoid the deleted operator above.  XXX Why is operator!
  // needed separately?  Shouldn't the compiler prefer using the non-deleted
  // operator bool instead of the deleted operator T*?
  explicit operator bool() const { return !!mRawPtr; }
  bool operator!() const { return !mRawPtr; }
#endif

  T*
  operator->() const MOZ_NO_ADDREF_RELEASE_ON_RETURN
  {
    MOZ_ASSERT(mRawPtr != 0,
               "You can't dereference a NULL nsRefPtr with operator->().");
    return get();
  }

  template <typename R, typename... Args>
  class Proxy
  {
    typedef R (T::*member_function)(Args...);
    T* mRawPtr;
    member_function mFunction;
  public:
    Proxy(T* aRawPtr, member_function aFunction)
      : mRawPtr(aRawPtr),
        mFunction(aFunction)
    {
    }
    template<typename... ActualArgs>
    R operator()(ActualArgs&&... aArgs)
    {
      return ((*mRawPtr).*mFunction)(mozilla::Forward<ActualArgs>(aArgs)...);
    }
  };

  template <typename R, typename... Args>
  Proxy<R, Args...> operator->*(R (T::*aFptr)(Args...)) const
  {
    MOZ_ASSERT(mRawPtr != 0,
               "You can't dereference a NULL nsRefPtr with operator->*().");
    return Proxy<R, Args...>(get(), aFptr);
  }

  nsRefPtr<T>*
  get_address()
  // This is not intended to be used by clients.  See |address_of|
  // below.
  {
    return this;
  }

  const nsRefPtr<T>*
  get_address() const
  // This is not intended to be used by clients.  See |address_of|
  // below.
  {
    return this;
  }

public:
  T&
  operator*() const
  {
    MOZ_ASSERT(mRawPtr != 0,
               "You can't dereference a NULL nsRefPtr with operator*().");
    return *get();
  }

  T**
  StartAssignment()
  {
    assign_assuming_AddRef(0);
    return reinterpret_cast<T**>(&mRawPtr);
  }
private:
  // This helper class makes |nsRefPtr<const T>| possible by casting away
  // the constness from the pointer when calling AddRef() and Release().
  //
  // This is necessary because AddRef() and Release() implementations can't
  // generally expected to be const themselves (without heavy use of |mutable|
  // and |const_cast| in their own implementations).
  //
  // This should be sound because while |nsRefPtr<const T>| provides a
  // const view of an object, the object itself should not be const (it
  // would have to be allocated as |new const T| or similar to be const).
  template<class U>
  struct AddRefTraits
  {
    static void AddRef(U* aPtr) {
      aPtr->AddRef();
    }
    static void Release(U* aPtr) {
      aPtr->Release();
    }
  };
  template<class U>
  struct AddRefTraits<const U>
  {
    static void AddRef(const U* aPtr) {
      const_cast<U*>(aPtr)->AddRef();
    }
    static void Release(const U* aPtr) {
      const_cast<U*>(aPtr)->Release();
    }
  };
};

class nsCycleCollectionTraversalCallback;
template <typename T>
void
CycleCollectionNoteChild(nsCycleCollectionTraversalCallback& aCallback,
                         T* aChild, const char* aName, uint32_t aFlags);

template <typename T>
inline void
ImplCycleCollectionUnlink(nsRefPtr<T>& aField)
{
  aField = nullptr;
}

template <typename T>
inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            nsRefPtr<T>& aField,
                            const char* aName,
                            uint32_t aFlags = 0)
{
  CycleCollectionNoteChild(aCallback, aField.get(), aName, aFlags);
}

template <class T>
inline nsRefPtr<T>*
address_of(nsRefPtr<T>& aPtr)
{
  return aPtr.get_address();
}

template <class T>
inline const nsRefPtr<T>*
address_of(const nsRefPtr<T>& aPtr)
{
  return aPtr.get_address();
}

template <class T>
class nsRefPtrGetterAddRefs
/*
  ...

  This class is designed to be used for anonymous temporary objects in the
  argument list of calls that return COM interface pointers, e.g.,

    nsRefPtr<IFoo> fooP;
    ...->GetAddRefedPointer(getter_AddRefs(fooP))

  DO NOT USE THIS TYPE DIRECTLY IN YOUR CODE.  Use |getter_AddRefs()| instead.

  When initialized with a |nsRefPtr|, as in the example above, it returns
  a |void**|, a |T**|, or an |nsISupports**| as needed, that the
  outer call (|GetAddRefedPointer| in this case) can fill in.

  This type should be a nested class inside |nsRefPtr<T>|.
*/
{
public:
  explicit
  nsRefPtrGetterAddRefs(nsRefPtr<T>& aSmartPtr)
    : mTargetSmartPtr(aSmartPtr)
  {
    // nothing else to do
  }

  operator void**()
  {
    return reinterpret_cast<void**>(mTargetSmartPtr.StartAssignment());
  }

  operator T**()
  {
    return mTargetSmartPtr.StartAssignment();
  }

  T*&
  operator*()
  {
    return *(mTargetSmartPtr.StartAssignment());
  }

private:
  nsRefPtr<T>& mTargetSmartPtr;
};

template <class T>
inline nsRefPtrGetterAddRefs<T>
getter_AddRefs(nsRefPtr<T>& aSmartPtr)
/*
  Used around a |nsRefPtr| when
  ...makes the class |nsRefPtrGetterAddRefs<T>| invisible.
*/
{
  return nsRefPtrGetterAddRefs<T>(aSmartPtr);
}


// Comparing two |nsRefPtr|s

template <class T, class U>
inline bool
operator==(const nsRefPtr<T>& aLhs, const nsRefPtr<U>& aRhs)
{
  return static_cast<const T*>(aLhs.get()) == static_cast<const U*>(aRhs.get());
}


template <class T, class U>
inline bool
operator!=(const nsRefPtr<T>& aLhs, const nsRefPtr<U>& aRhs)
{
  return static_cast<const T*>(aLhs.get()) != static_cast<const U*>(aRhs.get());
}


// Comparing an |nsRefPtr| to a raw pointer

template <class T, class U>
inline bool
operator==(const nsRefPtr<T>& aLhs, const U* aRhs)
{
  return static_cast<const T*>(aLhs.get()) == static_cast<const U*>(aRhs);
}

template <class T, class U>
inline bool
operator==(const U* aLhs, const nsRefPtr<T>& aRhs)
{
  return static_cast<const U*>(aLhs) == static_cast<const T*>(aRhs.get());
}

template <class T, class U>
inline bool
operator!=(const nsRefPtr<T>& aLhs, const U* aRhs)
{
  return static_cast<const T*>(aLhs.get()) != static_cast<const U*>(aRhs);
}

template <class T, class U>
inline bool
operator!=(const U* aLhs, const nsRefPtr<T>& aRhs)
{
  return static_cast<const U*>(aLhs) != static_cast<const T*>(aRhs.get());
}

template <class T, class U>
inline bool
operator==(const nsRefPtr<T>& aLhs, U* aRhs)
{
  return static_cast<const T*>(aLhs.get()) == const_cast<const U*>(aRhs);
}

template <class T, class U>
inline bool
operator==(U* aLhs, const nsRefPtr<T>& aRhs)
{
  return const_cast<const U*>(aLhs) == static_cast<const T*>(aRhs.get());
}

template <class T, class U>
inline bool
operator!=(const nsRefPtr<T>& aLhs, U* aRhs)
{
  return static_cast<const T*>(aLhs.get()) != const_cast<const U*>(aRhs);
}

template <class T, class U>
inline bool
operator!=(U* aLhs, const nsRefPtr<T>& aRhs)
{
  return const_cast<const U*>(aLhs) != static_cast<const T*>(aRhs.get());
}

// Comparing an |nsRefPtr| to |nullptr|

template <class T>
inline bool
operator==(const nsRefPtr<T>& aLhs, decltype(nullptr))
{
  return aLhs.get() == nullptr;
}

template <class T>
inline bool
operator==(decltype(nullptr), const nsRefPtr<T>& aRhs)
{
  return nullptr == aRhs.get();
}

template <class T>
inline bool
operator!=(const nsRefPtr<T>& aLhs, decltype(nullptr))
{
  return aLhs.get() != nullptr;
}

template <class T>
inline bool
operator!=(decltype(nullptr), const nsRefPtr<T>& aRhs)
{
  return nullptr != aRhs.get();
}

/*****************************************************************************/

template <class T>
inline already_AddRefed<T>
do_AddRef(T*&& aObj)
{
  nsRefPtr<T> ref(aObj);
  return ref.forget();
}

namespace mozilla {

/**
 * Helper function to be able to conveniently write things like:
 *
 *   already_AddRefed<T>
 *   f(...)
 *   {
 *     return MakeAndAddRef<T>(...);
 *   }
 */
template<typename T, typename... Args>
already_AddRefed<T>
MakeAndAddRef(Args&&... aArgs)
{
  nsRefPtr<T> p(new T(Forward<Args>(aArgs)...));
  return p.forget();
}

} // namespace mozilla

#endif /* mozilla_nsRefPtr_h */
