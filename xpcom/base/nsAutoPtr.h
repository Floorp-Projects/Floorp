/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAutoPtr_h
#define nsAutoPtr_h

#include "nsCOMPtr.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TypeTraits.h"

#include "nsCycleCollectionNoteChild.h"
#include "mozilla/MemoryReporting.h"

/*****************************************************************************/

// template <class T> class nsAutoPtrGetterTransfers;

template <class T>
class nsAutoPtr
{
private:
  static_assert(!mozilla::IsScalar<T>::value, "If you are using "
                "nsAutoPtr to hold an array, use UniquePtr<T[]> instead");

  void**
  begin_assignment()
  {
    assign(0);
    return reinterpret_cast<void**>(&mRawPtr);
  }

  void
  assign(T* aNewPtr)
  {
    T* oldPtr = mRawPtr;

    if (aNewPtr && aNewPtr == oldPtr) {
      MOZ_CRASH("Logic flaw in the caller");
    }

    mRawPtr = aNewPtr;
    delete oldPtr;
  }

  // |class Ptr| helps us prevent implicit "copy construction"
  // through |operator T*() const| from a |const nsAutoPtr<T>|
  // because two implicit conversions in a row aren't allowed.
  // It still allows assignment from T* through implicit conversion
  // from |T*| to |nsAutoPtr<T>::Ptr|
  class Ptr
  {
  public:
    MOZ_IMPLICIT Ptr(T* aPtr)
      : mPtr(aPtr)
    {
    }

    operator T*() const
    {
      return mPtr;
    }

  private:
    T* MOZ_NON_OWNING_REF mPtr;
  };

private:
  T* MOZ_OWNING_REF mRawPtr;

public:
  typedef T element_type;

  ~nsAutoPtr()
  {
    delete mRawPtr;
  }

  // Constructors

  nsAutoPtr()
    : mRawPtr(0)
    // default constructor
  {
  }

  MOZ_IMPLICIT nsAutoPtr(Ptr aRawPtr)
    : mRawPtr(aRawPtr)
    // construct from a raw pointer (of the right type)
  {
  }

  // This constructor shouldn't exist; we should just use the &&
  // constructor.
  nsAutoPtr(nsAutoPtr<T>& aSmartPtr)
    : mRawPtr(aSmartPtr.forget())
    // Construct by transferring ownership from another smart pointer.
  {
  }

  template <typename I>
  MOZ_IMPLICIT nsAutoPtr(nsAutoPtr<I>& aSmartPtr)
    : mRawPtr(aSmartPtr.forget())
    // Construct by transferring ownership from another smart pointer.
  {
  }

  nsAutoPtr(nsAutoPtr<T>&& aSmartPtr)
    : mRawPtr(aSmartPtr.forget())
    // Construct by transferring ownership from another smart pointer.
  {
  }

  template <typename I>
  MOZ_IMPLICIT nsAutoPtr(nsAutoPtr<I>&& aSmartPtr)
    : mRawPtr(aSmartPtr.forget())
    // Construct by transferring ownership from another smart pointer.
  {
  }

  // Assignment operators

  nsAutoPtr<T>&
  operator=(T* aRhs)
  // assign from a raw pointer (of the right type)
  {
    assign(aRhs);
    return *this;
  }

  nsAutoPtr<T>& operator=(nsAutoPtr<T>& aRhs)
  // assign by transferring ownership from another smart pointer.
  {
    assign(aRhs.forget());
    return *this;
  }

  template <typename I>
  nsAutoPtr<T>& operator=(nsAutoPtr<I>& aRhs)
  // assign by transferring ownership from another smart pointer.
  {
    assign(aRhs.forget());
    return *this;
  }

  nsAutoPtr<T>& operator=(nsAutoPtr<T>&& aRhs)
  {
    assign(aRhs.forget());
    return *this;
  }

  template <typename I>
  nsAutoPtr<T>& operator=(nsAutoPtr<I>&& aRhs)
  {
    assign(aRhs.forget());
    return *this;
  }

  // Other pointer operators

  T*
  get() const
  /*
    Prefer the implicit conversion provided automatically by
    |operator T*() const|.  Use |get()| _only_ to resolve
    ambiguity.
  */
  {
    return mRawPtr;
  }

  operator T*() const
  /*
    ...makes an |nsAutoPtr| act like its underlying raw pointer
    type  whenever it is used in a context where a raw pointer
    is expected.  It is this operator that makes an |nsAutoPtr|
    substitutable for a raw pointer.

    Prefer the implicit use of this operator to calling |get()|,
    except where necessary to resolve ambiguity.
  */
  {
    return get();
  }

  T*
  forget()
  {
    T* temp = mRawPtr;
    mRawPtr = 0;
    return temp;
  }

  T*
  operator->() const
  {
    MOZ_ASSERT(mRawPtr != 0,
               "You can't dereference a NULL nsAutoPtr with operator->().");
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

  template <typename R, typename C, typename... Args>
  Proxy<R, Args...> operator->*(R (C::*aFptr)(Args...)) const
  {
    MOZ_ASSERT(mRawPtr != 0,
               "You can't dereference a NULL nsAutoPtr with operator->*().");
    return Proxy<R, Args...>(get(), aFptr);
  }

  nsAutoPtr<T>*
  get_address()
  // This is not intended to be used by clients.  See |address_of|
  // below.
  {
    return this;
  }

  const nsAutoPtr<T>*
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
               "You can't dereference a NULL nsAutoPtr with operator*().");
    return *get();
  }

  T**
  StartAssignment()
  {
#ifndef NSCAP_FEATURE_INLINE_STARTASSIGNMENT
    return reinterpret_cast<T**>(begin_assignment());
#else
    assign(0);
    return reinterpret_cast<T**>(&mRawPtr);
#endif
  }
};

template <class T>
inline nsAutoPtr<T>*
address_of(nsAutoPtr<T>& aPtr)
{
  return aPtr.get_address();
}

template <class T>
inline const nsAutoPtr<T>*
address_of(const nsAutoPtr<T>& aPtr)
{
  return aPtr.get_address();
}

template <class T>
class nsAutoPtrGetterTransfers
/*
  ...

  This class is designed to be used for anonymous temporary objects in the
  argument list of calls that return COM interface pointers, e.g.,

    nsAutoPtr<IFoo> fooP;
    ...->GetTransferedPointer(getter_Transfers(fooP))

  DO NOT USE THIS TYPE DIRECTLY IN YOUR CODE.  Use |getter_Transfers()| instead.

  When initialized with a |nsAutoPtr|, as in the example above, it returns
  a |void**|, a |T**|, or an |nsISupports**| as needed, that the
  outer call (|GetTransferedPointer| in this case) can fill in.

  This type should be a nested class inside |nsAutoPtr<T>|.
*/
{
public:
  explicit
  nsAutoPtrGetterTransfers(nsAutoPtr<T>& aSmartPtr)
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
  nsAutoPtr<T>& mTargetSmartPtr;
};

template <class T>
inline nsAutoPtrGetterTransfers<T>
getter_Transfers(nsAutoPtr<T>& aSmartPtr)
/*
  Used around a |nsAutoPtr| when
  ...makes the class |nsAutoPtrGetterTransfers<T>| invisible.
*/
{
  return nsAutoPtrGetterTransfers<T>(aSmartPtr);
}



// Comparing two |nsAutoPtr|s

template <class T, class U>
inline bool
operator==(const nsAutoPtr<T>& aLhs, const nsAutoPtr<U>& aRhs)
{
  return static_cast<const T*>(aLhs.get()) == static_cast<const U*>(aRhs.get());
}


template <class T, class U>
inline bool
operator!=(const nsAutoPtr<T>& aLhs, const nsAutoPtr<U>& aRhs)
{
  return static_cast<const T*>(aLhs.get()) != static_cast<const U*>(aRhs.get());
}


// Comparing an |nsAutoPtr| to a raw pointer

template <class T, class U>
inline bool
operator==(const nsAutoPtr<T>& aLhs, const U* aRhs)
{
  return static_cast<const T*>(aLhs.get()) == static_cast<const U*>(aRhs);
}

template <class T, class U>
inline bool
operator==(const U* aLhs, const nsAutoPtr<T>& aRhs)
{
  return static_cast<const U*>(aLhs) == static_cast<const T*>(aRhs.get());
}

template <class T, class U>
inline bool
operator!=(const nsAutoPtr<T>& aLhs, const U* aRhs)
{
  return static_cast<const T*>(aLhs.get()) != static_cast<const U*>(aRhs);
}

template <class T, class U>
inline bool
operator!=(const U* aLhs, const nsAutoPtr<T>& aRhs)
{
  return static_cast<const U*>(aLhs) != static_cast<const T*>(aRhs.get());
}

template <class T, class U>
inline bool
operator==(const nsAutoPtr<T>& aLhs, U* aRhs)
{
  return static_cast<const T*>(aLhs.get()) == const_cast<const U*>(aRhs);
}

template <class T, class U>
inline bool
operator==(U* aLhs, const nsAutoPtr<T>& aRhs)
{
  return const_cast<const U*>(aLhs) == static_cast<const T*>(aRhs.get());
}

template <class T, class U>
inline bool
operator!=(const nsAutoPtr<T>& aLhs, U* aRhs)
{
  return static_cast<const T*>(aLhs.get()) != const_cast<const U*>(aRhs);
}

template <class T, class U>
inline bool
operator!=(U* aLhs, const nsAutoPtr<T>& aRhs)
{
  return const_cast<const U*>(aLhs) != static_cast<const T*>(aRhs.get());
}



// Comparing an |nsAutoPtr| to |nullptr|

template <class T>
inline bool
operator==(const nsAutoPtr<T>& aLhs, decltype(nullptr))
{
  return aLhs.get() == nullptr;
}

template <class T>
inline bool
operator==(decltype(nullptr), const nsAutoPtr<T>& aRhs)
{
  return nullptr == aRhs.get();
}

template <class T>
inline bool
operator!=(const nsAutoPtr<T>& aLhs, decltype(nullptr))
{
  return aLhs.get() != nullptr;
}

template <class T>
inline bool
operator!=(decltype(nullptr), const nsAutoPtr<T>& aRhs)
{
  return nullptr != aRhs.get();
}


/*****************************************************************************/

#endif // !defined(nsAutoPtr_h)
