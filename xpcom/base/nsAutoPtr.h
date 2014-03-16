/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAutoPtr_h___
#define nsAutoPtr_h___

  // Wrapping includes can speed up compiles (see "Large Scale C++ Software Design")
#ifndef nsCOMPtr_h___
  // For |already_AddRefed|, |NSCAP_Zero|,
  // |NSCAP_DONT_PROVIDE_NONCONST_OPEQ|,
  // |NSCAP_FEATURE_INLINE_STARTASSIGNMENT|
#include "nsCOMPtr.h"
#endif

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
      assign( T* newPtr )
        {
          T* oldPtr = mRawPtr;

          if (newPtr != nullptr && newPtr == oldPtr) {
            NS_RUNTIMEABORT("Logic flaw in the caller");
          }

          mRawPtr = newPtr;
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
            Ptr( T* aPtr )
                  : mPtr(aPtr)
              {
              }

            operator T*() const
              {
                return mPtr;
              }

          private:
            T* mPtr;
        };

    private:
      T* mRawPtr;

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

      nsAutoPtr( Ptr aRawPtr )
            : mRawPtr(aRawPtr)
          // construct from a raw pointer (of the right type)
        {
        }

      // This constructor shouldn't exist; we should just use the &&
      // constructor.
      nsAutoPtr( nsAutoPtr<T>& aSmartPtr )
            : mRawPtr( aSmartPtr.forget() )
          // Construct by transferring ownership from another smart pointer.
        {
        }

      nsAutoPtr( nsAutoPtr<T>&& aSmartPtr )
            : mRawPtr( aSmartPtr.forget() )
          // Construct by transferring ownership from another smart pointer.
        {
        }

        // Assignment operators

      nsAutoPtr<T>&
      operator=( T* rhs )
          // assign from a raw pointer (of the right type)
        {
          assign(rhs);
          return *this;
        }

      nsAutoPtr<T>& operator=( nsAutoPtr<T>& rhs )
          // assign by transferring ownership from another smart pointer.
        {
          assign(rhs.forget());
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
          NS_PRECONDITION(mRawPtr != 0, "You can't dereference a NULL nsAutoPtr with operator->().");
          return get();
        }

      // This operator is needed for gcc <= 4.0.* and for Sun Studio; it
      // causes internal compiler errors for some MSVC versions.  (It's not
      // clear to me whether it should be needed.)
#ifndef _MSC_VER
      template <class U, class V>
      U&
      operator->*(U V::* aMember)
        {
          NS_PRECONDITION(mRawPtr != 0, "You can't dereference a NULL nsAutoPtr with operator->*().");
          return get()->*aMember;
        }
#endif

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
          NS_PRECONDITION(mRawPtr != 0, "You can't dereference a NULL nsAutoPtr with operator*().");
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
inline
nsAutoPtr<T>*
address_of( nsAutoPtr<T>& aPtr )
  {
    return aPtr.get_address();
  }

template <class T>
inline
const nsAutoPtr<T>*
address_of( const nsAutoPtr<T>& aPtr )
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
      nsAutoPtrGetterTransfers( nsAutoPtr<T>& aSmartPtr )
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
inline
nsAutoPtrGetterTransfers<T>
getter_Transfers( nsAutoPtr<T>& aSmartPtr )
    /*
      Used around a |nsAutoPtr| when 
      ...makes the class |nsAutoPtrGetterTransfers<T>| invisible.
    */
  {
    return nsAutoPtrGetterTransfers<T>(aSmartPtr);
  }



  // Comparing two |nsAutoPtr|s

template <class T, class U>
inline
bool
operator==( const nsAutoPtr<T>& lhs, const nsAutoPtr<U>& rhs )
  {
    return static_cast<const T*>(lhs.get()) == static_cast<const U*>(rhs.get());
  }


template <class T, class U>
inline
bool
operator!=( const nsAutoPtr<T>& lhs, const nsAutoPtr<U>& rhs )
  {
    return static_cast<const T*>(lhs.get()) != static_cast<const U*>(rhs.get());
  }


  // Comparing an |nsAutoPtr| to a raw pointer

template <class T, class U>
inline
bool
operator==( const nsAutoPtr<T>& lhs, const U* rhs )
  {
    return static_cast<const T*>(lhs.get()) == static_cast<const U*>(rhs);
  }

template <class T, class U>
inline
bool
operator==( const U* lhs, const nsAutoPtr<T>& rhs )
  {
    return static_cast<const U*>(lhs) == static_cast<const T*>(rhs.get());
  }

template <class T, class U>
inline
bool
operator!=( const nsAutoPtr<T>& lhs, const U* rhs )
  {
    return static_cast<const T*>(lhs.get()) != static_cast<const U*>(rhs);
  }

template <class T, class U>
inline
bool
operator!=( const U* lhs, const nsAutoPtr<T>& rhs )
  {
    return static_cast<const U*>(lhs) != static_cast<const T*>(rhs.get());
  }

  // To avoid ambiguities caused by the presence of builtin |operator==|s
  // creating a situation where one of the |operator==| defined above
  // has a better conversion for one argument and the builtin has a
  // better conversion for the other argument, define additional
  // |operator==| without the |const| on the raw pointer.
  // See bug 65664 for details.

#ifndef NSCAP_DONT_PROVIDE_NONCONST_OPEQ
template <class T, class U>
inline
bool
operator==( const nsAutoPtr<T>& lhs, U* rhs )
  {
    return static_cast<const T*>(lhs.get()) == const_cast<const U*>(rhs);
  }

template <class T, class U>
inline
bool
operator==( U* lhs, const nsAutoPtr<T>& rhs )
  {
    return const_cast<const U*>(lhs) == static_cast<const T*>(rhs.get());
  }

template <class T, class U>
inline
bool
operator!=( const nsAutoPtr<T>& lhs, U* rhs )
  {
    return static_cast<const T*>(lhs.get()) != const_cast<const U*>(rhs);
  }

template <class T, class U>
inline
bool
operator!=( U* lhs, const nsAutoPtr<T>& rhs )
  {
    return const_cast<const U*>(lhs) != static_cast<const T*>(rhs.get());
  }
#endif



  // Comparing an |nsAutoPtr| to |0|

template <class T>
inline
bool
operator==( const nsAutoPtr<T>& lhs, NSCAP_Zero* rhs )
    // specifically to allow |smartPtr == 0|
  {
    return static_cast<const void*>(lhs.get()) == reinterpret_cast<const void*>(rhs);
  }

template <class T>
inline
bool
operator==( NSCAP_Zero* lhs, const nsAutoPtr<T>& rhs )
    // specifically to allow |0 == smartPtr|
  {
    return reinterpret_cast<const void*>(lhs) == static_cast<const void*>(rhs.get());
  }

template <class T>
inline
bool
operator!=( const nsAutoPtr<T>& lhs, NSCAP_Zero* rhs )
    // specifically to allow |smartPtr != 0|
  {
    return static_cast<const void*>(lhs.get()) != reinterpret_cast<const void*>(rhs);
  }

template <class T>
inline
bool
operator!=( NSCAP_Zero* lhs, const nsAutoPtr<T>& rhs )
    // specifically to allow |0 != smartPtr|
  {
    return reinterpret_cast<const void*>(lhs) != static_cast<const void*>(rhs.get());
  }


#ifdef HAVE_CPP_TROUBLE_COMPARING_TO_ZERO

  // We need to explicitly define comparison operators for `int'
  // because the compiler is lame.

template <class T>
inline
bool
operator==( const nsAutoPtr<T>& lhs, int rhs )
    // specifically to allow |smartPtr == 0|
  {
    return static_cast<const void*>(lhs.get()) == reinterpret_cast<const void*>(rhs);
  }

template <class T>
inline
bool
operator==( int lhs, const nsAutoPtr<T>& rhs )
    // specifically to allow |0 == smartPtr|
  {
    return reinterpret_cast<const void*>(lhs) == static_cast<const void*>(rhs.get());
  }

#endif // !defined(HAVE_CPP_TROUBLE_COMPARING_TO_ZERO)

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
      assign( T* newPtr )
        {
          T* oldPtr = mRawPtr;
          mRawPtr = newPtr;
          delete [] oldPtr;
        }

    private:
      T* mRawPtr;

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

      nsAutoArrayPtr( T* aRawPtr )
            : mRawPtr(aRawPtr)
          // construct from a raw pointer (of the right type)
        {
        }

      nsAutoArrayPtr( nsAutoArrayPtr<T>& aSmartPtr )
            : mRawPtr( aSmartPtr.forget() )
          // Construct by transferring ownership from another smart pointer.
        {
        }


        // Assignment operators

      nsAutoArrayPtr<T>&
      operator=( T* rhs )
          // assign from a raw pointer (of the right type)
        {
          assign(rhs);
          return *this;
        }

      nsAutoArrayPtr<T>& operator=( nsAutoArrayPtr<T>& rhs )
          // assign by transferring ownership from another smart pointer.
        {
          assign(rhs.forget());
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
          NS_PRECONDITION(mRawPtr != 0, "You can't dereference a NULL nsAutoArrayPtr with operator->().");
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
          NS_PRECONDITION(mRawPtr != 0, "You can't dereference a NULL nsAutoArrayPtr with operator*().");
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
inline
nsAutoArrayPtr<T>*
address_of( nsAutoArrayPtr<T>& aPtr )
  {
    return aPtr.get_address();
  }

template <class T>
inline
const nsAutoArrayPtr<T>*
address_of( const nsAutoArrayPtr<T>& aPtr )
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
      nsAutoArrayPtrGetterTransfers( nsAutoArrayPtr<T>& aSmartPtr )
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
inline
nsAutoArrayPtrGetterTransfers<T>
getter_Transfers( nsAutoArrayPtr<T>& aSmartPtr )
    /*
      Used around a |nsAutoArrayPtr| when 
      ...makes the class |nsAutoArrayPtrGetterTransfers<T>| invisible.
    */
  {
    return nsAutoArrayPtrGetterTransfers<T>(aSmartPtr);
  }



  // Comparing two |nsAutoArrayPtr|s

template <class T, class U>
inline
bool
operator==( const nsAutoArrayPtr<T>& lhs, const nsAutoArrayPtr<U>& rhs )
  {
    return static_cast<const T*>(lhs.get()) == static_cast<const U*>(rhs.get());
  }


template <class T, class U>
inline
bool
operator!=( const nsAutoArrayPtr<T>& lhs, const nsAutoArrayPtr<U>& rhs )
  {
    return static_cast<const T*>(lhs.get()) != static_cast<const U*>(rhs.get());
  }


  // Comparing an |nsAutoArrayPtr| to a raw pointer

template <class T, class U>
inline
bool
operator==( const nsAutoArrayPtr<T>& lhs, const U* rhs )
  {
    return static_cast<const T*>(lhs.get()) == static_cast<const U*>(rhs);
  }

template <class T, class U>
inline
bool
operator==( const U* lhs, const nsAutoArrayPtr<T>& rhs )
  {
    return static_cast<const U*>(lhs) == static_cast<const T*>(rhs.get());
  }

template <class T, class U>
inline
bool
operator!=( const nsAutoArrayPtr<T>& lhs, const U* rhs )
  {
    return static_cast<const T*>(lhs.get()) != static_cast<const U*>(rhs);
  }

template <class T, class U>
inline
bool
operator!=( const U* lhs, const nsAutoArrayPtr<T>& rhs )
  {
    return static_cast<const U*>(lhs) != static_cast<const T*>(rhs.get());
  }

  // To avoid ambiguities caused by the presence of builtin |operator==|s
  // creating a situation where one of the |operator==| defined above
  // has a better conversion for one argument and the builtin has a
  // better conversion for the other argument, define additional
  // |operator==| without the |const| on the raw pointer.
  // See bug 65664 for details.

#ifndef NSCAP_DONT_PROVIDE_NONCONST_OPEQ
template <class T, class U>
inline
bool
operator==( const nsAutoArrayPtr<T>& lhs, U* rhs )
  {
    return static_cast<const T*>(lhs.get()) == const_cast<const U*>(rhs);
  }

template <class T, class U>
inline
bool
operator==( U* lhs, const nsAutoArrayPtr<T>& rhs )
  {
    return const_cast<const U*>(lhs) == static_cast<const T*>(rhs.get());
  }

template <class T, class U>
inline
bool
operator!=( const nsAutoArrayPtr<T>& lhs, U* rhs )
  {
    return static_cast<const T*>(lhs.get()) != const_cast<const U*>(rhs);
  }

template <class T, class U>
inline
bool
operator!=( U* lhs, const nsAutoArrayPtr<T>& rhs )
  {
    return const_cast<const U*>(lhs) != static_cast<const T*>(rhs.get());
  }
#endif



  // Comparing an |nsAutoArrayPtr| to |0|

template <class T>
inline
bool
operator==( const nsAutoArrayPtr<T>& lhs, NSCAP_Zero* rhs )
    // specifically to allow |smartPtr == 0|
  {
    return static_cast<const void*>(lhs.get()) == reinterpret_cast<const void*>(rhs);
  }

template <class T>
inline
bool
operator==( NSCAP_Zero* lhs, const nsAutoArrayPtr<T>& rhs )
    // specifically to allow |0 == smartPtr|
  {
    return reinterpret_cast<const void*>(lhs) == static_cast<const void*>(rhs.get());
  }

template <class T>
inline
bool
operator!=( const nsAutoArrayPtr<T>& lhs, NSCAP_Zero* rhs )
    // specifically to allow |smartPtr != 0|
  {
    return static_cast<const void*>(lhs.get()) != reinterpret_cast<const void*>(rhs);
  }

template <class T>
inline
bool
operator!=( NSCAP_Zero* lhs, const nsAutoArrayPtr<T>& rhs )
    // specifically to allow |0 != smartPtr|
  {
    return reinterpret_cast<const void*>(lhs) != static_cast<const void*>(rhs.get());
  }


#ifdef HAVE_CPP_TROUBLE_COMPARING_TO_ZERO

  // We need to explicitly define comparison operators for `int'
  // because the compiler is lame.

template <class T>
inline
bool
operator==( const nsAutoArrayPtr<T>& lhs, int rhs )
    // specifically to allow |smartPtr == 0|
  {
    return static_cast<const void*>(lhs.get()) == reinterpret_cast<const void*>(rhs);
  }

template <class T>
inline
bool
operator==( int lhs, const nsAutoArrayPtr<T>& rhs )
    // specifically to allow |0 == smartPtr|
  {
    return reinterpret_cast<const void*>(lhs) == static_cast<const void*>(rhs.get());
  }

#endif // !defined(HAVE_CPP_TROUBLE_COMPARING_TO_ZERO)


/*****************************************************************************/

// template <class T> class nsRefPtrGetterAddRefs;

template <class T>
class nsRefPtr
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
            oldPtr->Release();
        }

    private:
      T* mRawPtr;

    public:
      typedef T element_type;
      
     ~nsRefPtr()
        {
          if ( mRawPtr )
            mRawPtr->Release();
        }

        // Constructors

      nsRefPtr()
            : mRawPtr(0)
          // default constructor
        {
        }

      nsRefPtr( const nsRefPtr<T>& aSmartPtr )
            : mRawPtr(aSmartPtr.mRawPtr)
          // copy-constructor
        {
          if ( mRawPtr )
            mRawPtr->AddRef();
        }

      nsRefPtr( T* aRawPtr )
            : mRawPtr(aRawPtr)
          // construct from a raw pointer (of the right type)
        {
          if ( mRawPtr )
            mRawPtr->AddRef();
        }

      template <typename I>
      nsRefPtr( already_AddRefed<I>& aSmartPtr )
            : mRawPtr(aSmartPtr.take())
          // construct from |already_AddRefed|
        {
        }

      template <typename I>
      nsRefPtr( already_AddRefed<I>&& aSmartPtr )
            : mRawPtr(aSmartPtr.take())
          // construct from |otherRefPtr.forget()|
        {
        }

      nsRefPtr( const nsCOMPtr_helper& helper )
        {
          void* newRawPtr;
          if (NS_FAILED(helper(NS_GET_TEMPLATE_IID(T), &newRawPtr)))
            newRawPtr = 0;
          mRawPtr = static_cast<T*>(newRawPtr);
        }

        // Assignment operators

      nsRefPtr<T>&
      operator=( const nsRefPtr<T>& rhs )
          // copy assignment operator
        {
          assign_with_AddRef(rhs.mRawPtr);
          return *this;
        }

      nsRefPtr<T>&
      operator=( T* rhs )
          // assign from a raw pointer (of the right type)
        {
          assign_with_AddRef(rhs);
          return *this;
        }

      template <typename I>
      nsRefPtr<T>&
      operator=( already_AddRefed<I>& rhs )
          // assign from |already_AddRefed|
        {
          assign_assuming_AddRef(rhs.take());
          return *this;
        }

      template <typename I>
      nsRefPtr<T>&
      operator=( already_AddRefed<I>&& rhs )
          // assign from |otherRefPtr.forget()|
        {
          assign_assuming_AddRef(rhs.take());
          return *this;
        }

      nsRefPtr<T>&
      operator=( const nsCOMPtr_helper& helper )
        {
          void* newRawPtr;
          if (NS_FAILED(helper(NS_GET_TEMPLATE_IID(T), &newRawPtr)))
            newRawPtr = 0;
          assign_assuming_AddRef(static_cast<T*>(newRawPtr));
          return *this;
        }

        // Other pointer operators

      void
      swap( nsRefPtr<T>& rhs )
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
          return already_AddRefed<T>(temp);
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
            ...makes an |nsRefPtr| act like its underlying raw pointer type whenever it
            is used in a context where a raw pointer is expected.  It is this operator
            that makes an |nsRefPtr| substitutable for a raw pointer.

            Prefer the implicit use of this operator to calling |get()|, except where
            necessary to resolve ambiguity.
          */
        {
          return get();
        }

      T*
      operator->() const
        {
          NS_PRECONDITION(mRawPtr != 0, "You can't dereference a NULL nsRefPtr with operator->().");
          return get();
        }

      // This operator is needed for gcc <= 4.0.* and for Sun Studio; it
      // causes internal compiler errors for some MSVC versions.  (It's not
      // clear to me whether it should be needed.)
#ifndef _MSC_VER
      template <class U, class V>
      U&
      operator->*(U V::* aMember)
        {
          NS_PRECONDITION(mRawPtr != 0, "You can't dereference a NULL nsRefPtr with operator->*().");
          return get()->*aMember;
        }
#endif

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
          NS_PRECONDITION(mRawPtr != 0, "You can't dereference a NULL nsRefPtr with operator*().");
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
inline
nsRefPtr<T>*
address_of( nsRefPtr<T>& aPtr )
  {
    return aPtr.get_address();
  }

template <class T>
inline
const nsRefPtr<T>*
address_of( const nsRefPtr<T>& aPtr )
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
      nsRefPtrGetterAddRefs( nsRefPtr<T>& aSmartPtr )
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
inline
nsRefPtrGetterAddRefs<T>
getter_AddRefs( nsRefPtr<T>& aSmartPtr )
    /*
      Used around a |nsRefPtr| when 
      ...makes the class |nsRefPtrGetterAddRefs<T>| invisible.
    */
  {
    return nsRefPtrGetterAddRefs<T>(aSmartPtr);
  }



  // Comparing two |nsRefPtr|s

template <class T, class U>
inline
bool
operator==( const nsRefPtr<T>& lhs, const nsRefPtr<U>& rhs )
  {
    return static_cast<const T*>(lhs.get()) == static_cast<const U*>(rhs.get());
  }


template <class T, class U>
inline
bool
operator!=( const nsRefPtr<T>& lhs, const nsRefPtr<U>& rhs )
  {
    return static_cast<const T*>(lhs.get()) != static_cast<const U*>(rhs.get());
  }


  // Comparing an |nsRefPtr| to a raw pointer

template <class T, class U>
inline
bool
operator==( const nsRefPtr<T>& lhs, const U* rhs )
  {
    return static_cast<const T*>(lhs.get()) == static_cast<const U*>(rhs);
  }

template <class T, class U>
inline
bool
operator==( const U* lhs, const nsRefPtr<T>& rhs )
  {
    return static_cast<const U*>(lhs) == static_cast<const T*>(rhs.get());
  }

template <class T, class U>
inline
bool
operator!=( const nsRefPtr<T>& lhs, const U* rhs )
  {
    return static_cast<const T*>(lhs.get()) != static_cast<const U*>(rhs);
  }

template <class T, class U>
inline
bool
operator!=( const U* lhs, const nsRefPtr<T>& rhs )
  {
    return static_cast<const U*>(lhs) != static_cast<const T*>(rhs.get());
  }

  // To avoid ambiguities caused by the presence of builtin |operator==|s
  // creating a situation where one of the |operator==| defined above
  // has a better conversion for one argument and the builtin has a
  // better conversion for the other argument, define additional
  // |operator==| without the |const| on the raw pointer.
  // See bug 65664 for details.

#ifndef NSCAP_DONT_PROVIDE_NONCONST_OPEQ
template <class T, class U>
inline
bool
operator==( const nsRefPtr<T>& lhs, U* rhs )
  {
    return static_cast<const T*>(lhs.get()) == const_cast<const U*>(rhs);
  }

template <class T, class U>
inline
bool
operator==( U* lhs, const nsRefPtr<T>& rhs )
  {
    return const_cast<const U*>(lhs) == static_cast<const T*>(rhs.get());
  }

template <class T, class U>
inline
bool
operator!=( const nsRefPtr<T>& lhs, U* rhs )
  {
    return static_cast<const T*>(lhs.get()) != const_cast<const U*>(rhs);
  }

template <class T, class U>
inline
bool
operator!=( U* lhs, const nsRefPtr<T>& rhs )
  {
    return const_cast<const U*>(lhs) != static_cast<const T*>(rhs.get());
  }
#endif



  // Comparing an |nsRefPtr| to |0|

template <class T>
inline
bool
operator==( const nsRefPtr<T>& lhs, NSCAP_Zero* rhs )
    // specifically to allow |smartPtr == 0|
  {
    return static_cast<const void*>(lhs.get()) == reinterpret_cast<const void*>(rhs);
  }

template <class T>
inline
bool
operator==( NSCAP_Zero* lhs, const nsRefPtr<T>& rhs )
    // specifically to allow |0 == smartPtr|
  {
    return reinterpret_cast<const void*>(lhs) == static_cast<const void*>(rhs.get());
  }

template <class T>
inline
bool
operator!=( const nsRefPtr<T>& lhs, NSCAP_Zero* rhs )
    // specifically to allow |smartPtr != 0|
  {
    return static_cast<const void*>(lhs.get()) != reinterpret_cast<const void*>(rhs);
  }

template <class T>
inline
bool
operator!=( NSCAP_Zero* lhs, const nsRefPtr<T>& rhs )
    // specifically to allow |0 != smartPtr|
  {
    return reinterpret_cast<const void*>(lhs) != static_cast<const void*>(rhs.get());
  }


#ifdef HAVE_CPP_TROUBLE_COMPARING_TO_ZERO

  // We need to explicitly define comparison operators for `int'
  // because the compiler is lame.

template <class T>
inline
bool
operator==( const nsRefPtr<T>& lhs, int rhs )
    // specifically to allow |smartPtr == 0|
  {
    return static_cast<const void*>(lhs.get()) == reinterpret_cast<const void*>(rhs);
  }

template <class T>
inline
bool
operator==( int lhs, const nsRefPtr<T>& rhs )
    // specifically to allow |0 == smartPtr|
  {
    return reinterpret_cast<const void*>(lhs) == static_cast<const void*>(rhs.get());
  }

#endif // !defined(HAVE_CPP_TROUBLE_COMPARING_TO_ZERO)

template <class SourceType, class DestinationType>
inline
nsresult
CallQueryInterface( nsRefPtr<SourceType>& aSourcePtr, DestinationType** aDestPtr )
  {
    return CallQueryInterface(aSourcePtr.get(), aDestPtr);
  }

/*****************************************************************************/

template<class T>
class nsQueryObject : public nsCOMPtr_helper
{
public:
  nsQueryObject(T* aRawPtr)
    : mRawPtr(aRawPtr) {}

  virtual nsresult NS_FASTCALL operator()( const nsIID& aIID, void** aResult ) const {
    nsresult status = mRawPtr ? mRawPtr->QueryInterface(aIID, aResult)
                              : NS_ERROR_NULL_POINTER;
    return status;
  }
private:
  T* mRawPtr;
};

template<class T>
class nsQueryObjectWithError : public nsCOMPtr_helper
{
public:
  nsQueryObjectWithError(T* aRawPtr, nsresult* aErrorPtr)
    : mRawPtr(aRawPtr), mErrorPtr(aErrorPtr) {}

  virtual nsresult NS_FASTCALL operator()( const nsIID& aIID, void** aResult ) const {
    nsresult status = mRawPtr ? mRawPtr->QueryInterface(aIID, aResult)
                              : NS_ERROR_NULL_POINTER;
    if (mErrorPtr)
      *mErrorPtr = status;
    return status;
  }
private:
  T* mRawPtr;
  nsresult* mErrorPtr;
};

template<class T>
inline
nsQueryObject<T>
do_QueryObject(T* aRawPtr)
{
  return nsQueryObject<T>(aRawPtr);
}

template<class T>
inline
nsQueryObject<T>
do_QueryObject(nsCOMPtr<T>& aRawPtr)
{
  return nsQueryObject<T>(aRawPtr);
}

template<class T>
inline
nsQueryObject<T>
do_QueryObject(nsRefPtr<T>& aRawPtr)
{
  return nsQueryObject<T>(aRawPtr);
}

template<class T>
inline
nsQueryObjectWithError<T>
do_QueryObject(T* aRawPtr, nsresult* aErrorPtr)
{
  return nsQueryObjectWithError<T>(aRawPtr, aErrorPtr);
}

template<class T>
inline
nsQueryObjectWithError<T>
do_QueryObject(nsCOMPtr<T>& aRawPtr, nsresult* aErrorPtr)
{
  return nsQueryObjectWithError<T>(aRawPtr, aErrorPtr);
}

template<class T>
inline
nsQueryObjectWithError<T>
do_QueryObject(nsRefPtr<T>& aRawPtr, nsresult* aErrorPtr)
{
  return nsQueryObjectWithError<T>(aRawPtr, aErrorPtr);
}

/*****************************************************************************/

#endif // !defined(nsAutoPtr_h___)
