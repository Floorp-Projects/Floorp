/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAutoPtr_h
#define nsAutoPtr_h

#include "nsCOMPtr.h"
#include "mozilla/nsRefPtr.h"

#include "nsCycleCollectionNoteChild.h"
#include "mozilla/MemoryReporting.h"

/*****************************************************************************/

// template <class T> class nsAutoPtrGetterTransfers;

template <class T>
class nsAutoPtr
{
private:
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
      NS_RUNTIMEABORT("Logic flaw in the caller");
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
    NS_PRECONDITION(mRawPtr != 0,
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
    NS_PRECONDITION(mRawPtr != 0,
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
    NS_PRECONDITION(mRawPtr != 0,
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



// Comparing an |nsAutoPtr| to |0|

template <class T>
inline bool
operator==(const nsAutoPtr<T>& aLhs, NSCAP_Zero* aRhs)
// specifically to allow |smartPtr == 0|
{
  return static_cast<const void*>(aLhs.get()) == reinterpret_cast<const void*>(aRhs);
}

template <class T>
inline bool
operator==(NSCAP_Zero* aLhs, const nsAutoPtr<T>& aRhs)
// specifically to allow |0 == smartPtr|
{
  return reinterpret_cast<const void*>(aLhs) == static_cast<const void*>(aRhs.get());
}

template <class T>
inline bool
operator!=(const nsAutoPtr<T>& aLhs, NSCAP_Zero* aRhs)
// specifically to allow |smartPtr != 0|
{
  return static_cast<const void*>(aLhs.get()) != reinterpret_cast<const void*>(aRhs);
}

template <class T>
inline bool
operator!=(NSCAP_Zero* aLhs, const nsAutoPtr<T>& aRhs)
// specifically to allow |0 != smartPtr|
{
  return reinterpret_cast<const void*>(aLhs) != static_cast<const void*>(aRhs.get());
}


/*****************************************************************************/

// template <class T> class nsAutoArrayPtrGetterTransfers;

template <class T>
class nsAutoArrayPtr
{
private:
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
    mRawPtr = aNewPtr;
    delete [] oldPtr;
  }

private:
  T* MOZ_OWNING_REF mRawPtr;

public:
  typedef T element_type;

  ~nsAutoArrayPtr()
  {
    delete [] mRawPtr;
  }

  // Constructors

  nsAutoArrayPtr()
    : mRawPtr(0)
    // default constructor
  {
  }

  MOZ_IMPLICIT nsAutoArrayPtr(T* aRawPtr)
    : mRawPtr(aRawPtr)
    // construct from a raw pointer (of the right type)
  {
  }

  nsAutoArrayPtr(nsAutoArrayPtr<T>& aSmartPtr)
    : mRawPtr(aSmartPtr.forget())
    // Construct by transferring ownership from another smart pointer.
  {
  }


  // Assignment operators

  nsAutoArrayPtr<T>&
  operator=(T* aRhs)
  // assign from a raw pointer (of the right type)
  {
    assign(aRhs);
    return *this;
  }

  nsAutoArrayPtr<T>& operator=(nsAutoArrayPtr<T>& aRhs)
  // assign by transferring ownership from another smart pointer.
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
    ...makes an |nsAutoArrayPtr| act like its underlying raw pointer
    type  whenever it is used in a context where a raw pointer
    is expected.  It is this operator that makes an |nsAutoArrayPtr|
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
    NS_PRECONDITION(mRawPtr != 0,
                    "You can't dereference a NULL nsAutoArrayPtr with operator->().");
    return get();
  }

  nsAutoArrayPtr<T>*
  get_address()
  // This is not intended to be used by clients.  See |address_of|
  // below.
  {
    return this;
  }

  const nsAutoArrayPtr<T>*
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
    NS_PRECONDITION(mRawPtr != 0,
                    "You can't dereference a NULL nsAutoArrayPtr with operator*().");
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

  size_t
  SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
  {
    return aMallocSizeOf(mRawPtr);
  }

  size_t
  SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }
};

template <class T>
inline nsAutoArrayPtr<T>*
address_of(nsAutoArrayPtr<T>& aPtr)
{
  return aPtr.get_address();
}

template <class T>
inline const nsAutoArrayPtr<T>*
address_of(const nsAutoArrayPtr<T>& aPtr)
{
  return aPtr.get_address();
}

template <class T>
class nsAutoArrayPtrGetterTransfers
/*
  ...

  This class is designed to be used for anonymous temporary objects in the
  argument list of calls that return COM interface pointers, e.g.,

    nsAutoArrayPtr<IFoo> fooP;
    ...->GetTransferedPointer(getter_Transfers(fooP))

  DO NOT USE THIS TYPE DIRECTLY IN YOUR CODE.  Use |getter_Transfers()| instead.

  When initialized with a |nsAutoArrayPtr|, as in the example above, it returns
  a |void**|, a |T**|, or an |nsISupports**| as needed, that the
  outer call (|GetTransferedPointer| in this case) can fill in.

  This type should be a nested class inside |nsAutoArrayPtr<T>|.
*/
{
public:
  explicit
  nsAutoArrayPtrGetterTransfers(nsAutoArrayPtr<T>& aSmartPtr)
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
  nsAutoArrayPtr<T>& mTargetSmartPtr;
};

template <class T>
inline nsAutoArrayPtrGetterTransfers<T>
getter_Transfers(nsAutoArrayPtr<T>& aSmartPtr)
/*
  Used around a |nsAutoArrayPtr| when
  ...makes the class |nsAutoArrayPtrGetterTransfers<T>| invisible.
*/
{
  return nsAutoArrayPtrGetterTransfers<T>(aSmartPtr);
}



// Comparing two |nsAutoArrayPtr|s

template <class T, class U>
inline bool
operator==(const nsAutoArrayPtr<T>& aLhs, const nsAutoArrayPtr<U>& aRhs)
{
  return static_cast<const T*>(aLhs.get()) == static_cast<const U*>(aRhs.get());
}


template <class T, class U>
inline bool
operator!=(const nsAutoArrayPtr<T>& aLhs, const nsAutoArrayPtr<U>& aRhs)
{
  return static_cast<const T*>(aLhs.get()) != static_cast<const U*>(aRhs.get());
}


// Comparing an |nsAutoArrayPtr| to a raw pointer

template <class T, class U>
inline bool
operator==(const nsAutoArrayPtr<T>& aLhs, const U* aRhs)
{
  return static_cast<const T*>(aLhs.get()) == static_cast<const U*>(aRhs);
}

template <class T, class U>
inline bool
operator==(const U* aLhs, const nsAutoArrayPtr<T>& aRhs)
{
  return static_cast<const U*>(aLhs) == static_cast<const T*>(aRhs.get());
}

template <class T, class U>
inline bool
operator!=(const nsAutoArrayPtr<T>& aLhs, const U* aRhs)
{
  return static_cast<const T*>(aLhs.get()) != static_cast<const U*>(aRhs);
}

template <class T, class U>
inline bool
operator!=(const U* aLhs, const nsAutoArrayPtr<T>& aRhs)
{
  return static_cast<const U*>(aLhs) != static_cast<const T*>(aRhs.get());
}

template <class T, class U>
inline bool
operator==(const nsAutoArrayPtr<T>& aLhs, U* aRhs)
{
  return static_cast<const T*>(aLhs.get()) == const_cast<const U*>(aRhs);
}

template <class T, class U>
inline bool
operator==(U* aLhs, const nsAutoArrayPtr<T>& aRhs)
{
  return const_cast<const U*>(aLhs) == static_cast<const T*>(aRhs.get());
}

template <class T, class U>
inline bool
operator!=(const nsAutoArrayPtr<T>& aLhs, U* aRhs)
{
  return static_cast<const T*>(aLhs.get()) != const_cast<const U*>(aRhs);
}

template <class T, class U>
inline bool
operator!=(U* aLhs, const nsAutoArrayPtr<T>& aRhs)
{
  return const_cast<const U*>(aLhs) != static_cast<const T*>(aRhs.get());
}



// Comparing an |nsAutoArrayPtr| to |0|

template <class T>
inline bool
operator==(const nsAutoArrayPtr<T>& aLhs, NSCAP_Zero* aRhs)
// specifically to allow |smartPtr == 0|
{
  return static_cast<const void*>(aLhs.get()) == reinterpret_cast<const void*>(aRhs);
}

template <class T>
inline bool
operator==(NSCAP_Zero* aLhs, const nsAutoArrayPtr<T>& aRhs)
// specifically to allow |0 == smartPtr|
{
  return reinterpret_cast<const void*>(aLhs) == static_cast<const void*>(aRhs.get());
}

template <class T>
inline bool
operator!=(const nsAutoArrayPtr<T>& aLhs, NSCAP_Zero* aRhs)
// specifically to allow |smartPtr != 0|
{
  return static_cast<const void*>(aLhs.get()) != reinterpret_cast<const void*>(aRhs);
}

template <class T>
inline bool
operator!=(NSCAP_Zero* aLhs, const nsAutoArrayPtr<T>& aRhs)
// specifically to allow |0 != smartPtr|
{
  return reinterpret_cast<const void*>(aLhs) != static_cast<const void*>(aRhs.get());
}


/*****************************************************************************/

#endif // !defined(nsAutoPtr_h)
