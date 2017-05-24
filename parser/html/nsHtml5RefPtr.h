/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHtml5RefPtr_h
#define nsHtml5RefPtr_h

#include "nsThreadUtils.h"

template <class T>
class nsHtml5RefPtrReleaser : public mozilla::Runnable
  {
    private:
      T* mPtr;
    public:
      explicit nsHtml5RefPtrReleaser(T* aPtr)
          : mozilla::Runnable("nsHtml5RefPtrReleaser")
          , mPtr(aPtr)
        {}
      NS_IMETHOD Run() override
        {
          mPtr->Release();
          return NS_OK;
        }
  };

// template <class T> class nsHtml5RefPtrGetterAddRefs;

/**
 * Like nsRefPtr except release is proxied to the main thread. Mostly copied
 * from nsRefPtr.
 */
template <class T>
class nsHtml5RefPtr
  {
    private:

      void
      assign_with_AddRef( T* rawPtr )
        {
          if ( rawPtr )
            rawPtr->AddRef();
          assign_assuming_AddRef(rawPtr);
        }

      void**
      begin_assignment()
        {
          assign_assuming_AddRef(0);
          return reinterpret_cast<void**>(&mRawPtr);
        }

      void
      assign_assuming_AddRef( T* newPtr )
        {
          T* oldPtr = mRawPtr;
          mRawPtr = newPtr;
          if ( oldPtr )
            release(oldPtr);
        }

      void
      release( T* aPtr )
        {
          nsCOMPtr<nsIRunnable> releaser = new nsHtml5RefPtrReleaser<T>(aPtr);
          if (NS_FAILED(NS_DispatchToMainThread(releaser))) 
            {
              NS_WARNING("Failed to dispatch releaser event.");
            }
        }

    private:
      T* mRawPtr;

    public:
      typedef T element_type;
      
     ~nsHtml5RefPtr()
        {
          if ( mRawPtr )
            release(mRawPtr);
        }

        // Constructors

      nsHtml5RefPtr()
            : mRawPtr(0)
          // default constructor
        {
        }

      nsHtml5RefPtr( const nsHtml5RefPtr<T>& aSmartPtr )
            : mRawPtr(aSmartPtr.mRawPtr)
          // copy-constructor
        {
          if ( mRawPtr )
            mRawPtr->AddRef();
        }

      explicit nsHtml5RefPtr( T* aRawPtr )
            : mRawPtr(aRawPtr)
          // construct from a raw pointer (of the right type)
        {
          if ( mRawPtr )
            mRawPtr->AddRef();
        }

      explicit nsHtml5RefPtr( const already_AddRefed<T>& aSmartPtr )
            : mRawPtr(aSmartPtr.mRawPtr)
          // construct from |dont_AddRef(expr)|
        {
        }

        // Assignment operators

      nsHtml5RefPtr<T>&
      operator=( const nsHtml5RefPtr<T>& rhs )
          // copy assignment operator
        {
          assign_with_AddRef(rhs.mRawPtr);
          return *this;
        }

      nsHtml5RefPtr<T>&
      operator=( T* rhs )
          // assign from a raw pointer (of the right type)
        {
          assign_with_AddRef(rhs);
          return *this;
        }

      nsHtml5RefPtr<T>&
      operator=( const already_AddRefed<T>& rhs )
          // assign from |dont_AddRef(expr)|
        {
          assign_assuming_AddRef(rhs.mRawPtr);
          return *this;
        }

        // Other pointer operators

      void
      swap( nsHtml5RefPtr<T>& rhs )
          // ...exchange ownership with |rhs|; can save a pair of refcount operations
        {
          T* temp = rhs.mRawPtr;
          rhs.mRawPtr = mRawPtr;
          mRawPtr = temp;
        }

      void
      swap( T*& rhs )
          // ...exchange ownership with |rhs|; can save a pair of refcount operations
        {
          T* temp = rhs;
          rhs = mRawPtr;
          mRawPtr = temp;
        }

      already_AddRefed<T>
      forget()
          // return the value of mRawPtr and null out mRawPtr. Useful for
          // already_AddRefed return values.
        {
          T* temp = 0;
          swap(temp);
          return temp;
        }

      template <typename I>
      void
      forget( I** rhs)
          // Set the target of rhs to the value of mRawPtr and null out mRawPtr.
          // Useful to avoid unnecessary AddRef/Release pairs with "out"
          // parameters where rhs bay be a T** or an I** where I is a base class
          // of T.
        {
          NS_ASSERTION(rhs, "Null pointer passed to forget!");
          *rhs = mRawPtr;
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
          /*
            ...makes an |nsHtml5RefPtr| act like its underlying raw pointer type whenever it
            is used in a context where a raw pointer is expected.  It is this operator
            that makes an |nsHtml5RefPtr| substitutable for a raw pointer.

            Prefer the implicit use of this operator to calling |get()|, except where
            necessary to resolve ambiguity.
          */
        {
          return get();
        }

      T*
      operator->() const MOZ_NO_ADDREF_RELEASE_ON_RETURN
        {
          NS_PRECONDITION(mRawPtr != 0, "You can't dereference a NULL nsHtml5RefPtr with operator->().");
          return get();
        }

      nsHtml5RefPtr<T>*
      get_address()
          // This is not intended to be used by clients.  See |address_of|
          // below.
        {
          return this;
        }

      const nsHtml5RefPtr<T>*
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
          NS_PRECONDITION(mRawPtr != 0, "You can't dereference a NULL nsHtml5RefPtr with operator*().");
          return *get();
        }

      T**
      StartAssignment()
        {
#ifndef NSCAP_FEATURE_INLINE_STARTASSIGNMENT
          return reinterpret_cast<T**>(begin_assignment());
#else
          assign_assuming_AddRef(0);
          return reinterpret_cast<T**>(&mRawPtr);
#endif
        }
  };

template <class T>
inline
nsHtml5RefPtr<T>*
address_of( nsHtml5RefPtr<T>& aPtr )
  {
    return aPtr.get_address();
  }

template <class T>
inline
const nsHtml5RefPtr<T>*
address_of( const nsHtml5RefPtr<T>& aPtr )
  {
    return aPtr.get_address();
  }

template <class T>
class nsHtml5RefPtrGetterAddRefs
    /*
      ...

      This class is designed to be used for anonymous temporary objects in the
      argument list of calls that return COM interface pointers, e.g.,

        nsHtml5RefPtr<IFoo> fooP;
        ...->GetAddRefedPointer(getter_AddRefs(fooP))

      DO NOT USE THIS TYPE DIRECTLY IN YOUR CODE.  Use |getter_AddRefs()| instead.

      When initialized with a |nsHtml5RefPtr|, as in the example above, it returns
      a |void**|, a |T**|, or an |nsISupports**| as needed, that the
      outer call (|GetAddRefedPointer| in this case) can fill in.

      This type should be a nested class inside |nsHtml5RefPtr<T>|.
    */
  {
    public:
      explicit
      nsHtml5RefPtrGetterAddRefs( nsHtml5RefPtr<T>& aSmartPtr )
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
      nsHtml5RefPtr<T>& mTargetSmartPtr;
  };

template <class T>
inline
nsHtml5RefPtrGetterAddRefs<T>
getter_AddRefs( nsHtml5RefPtr<T>& aSmartPtr )
    /*
      Used around a |nsHtml5RefPtr| when 
      ...makes the class |nsHtml5RefPtrGetterAddRefs<T>| invisible.
    */
  {
    return nsHtml5RefPtrGetterAddRefs<T>(aSmartPtr);
  }



  // Comparing two |nsHtml5RefPtr|s

template <class T, class U>
inline
bool
operator==( const nsHtml5RefPtr<T>& lhs, const nsHtml5RefPtr<U>& rhs )
  {
    return static_cast<const T*>(lhs.get()) == static_cast<const U*>(rhs.get());
  }


template <class T, class U>
inline
bool
operator!=( const nsHtml5RefPtr<T>& lhs, const nsHtml5RefPtr<U>& rhs )
  {
    return static_cast<const T*>(lhs.get()) != static_cast<const U*>(rhs.get());
  }


  // Comparing an |nsHtml5RefPtr| to a raw pointer

template <class T, class U>
inline
bool
operator==( const nsHtml5RefPtr<T>& lhs, const U* rhs )
  {
    return static_cast<const T*>(lhs.get()) == static_cast<const U*>(rhs);
  }

template <class T, class U>
inline
bool
operator==( const U* lhs, const nsHtml5RefPtr<T>& rhs )
  {
    return static_cast<const U*>(lhs) == static_cast<const T*>(rhs.get());
  }

template <class T, class U>
inline
bool
operator!=( const nsHtml5RefPtr<T>& lhs, const U* rhs )
  {
    return static_cast<const T*>(lhs.get()) != static_cast<const U*>(rhs);
  }

template <class T, class U>
inline
bool
operator!=( const U* lhs, const nsHtml5RefPtr<T>& rhs )
  {
    return static_cast<const U*>(lhs) != static_cast<const T*>(rhs.get());
  }

template <class T, class U>
inline
bool
operator==( const nsHtml5RefPtr<T>& lhs, U* rhs )
  {
    return static_cast<const T*>(lhs.get()) == const_cast<const U*>(rhs);
  }

template <class T, class U>
inline
bool
operator==( U* lhs, const nsHtml5RefPtr<T>& rhs )
  {
    return const_cast<const U*>(lhs) == static_cast<const T*>(rhs.get());
  }

template <class T, class U>
inline
bool
operator!=( const nsHtml5RefPtr<T>& lhs, U* rhs )
  {
    return static_cast<const T*>(lhs.get()) != const_cast<const U*>(rhs);
  }

template <class T, class U>
inline
bool
operator!=( U* lhs, const nsHtml5RefPtr<T>& rhs )
  {
    return const_cast<const U*>(lhs) != static_cast<const T*>(rhs.get());
  }



  // Comparing an |nsHtml5RefPtr| to |0|

template <class T>
inline
bool
operator==( const nsHtml5RefPtr<T>& lhs, decltype(nullptr) )
  {
    return lhs.get() == nullptr;
  }

template <class T>
inline
bool
operator==( decltype(nullptr), const nsHtml5RefPtr<T>& rhs )
  {
    return nullptr == rhs.get();
  }

template <class T>
inline
bool
operator!=( const nsHtml5RefPtr<T>& lhs, decltype(nullptr) )
  {
    return lhs.get() != nullptr;
  }

template <class T>
inline
bool
operator!=( decltype(nullptr), const nsHtml5RefPtr<T>& rhs )
  {
    return nullptr != rhs.get();
  }

#endif // !defined(nsHtml5RefPtr_h)
