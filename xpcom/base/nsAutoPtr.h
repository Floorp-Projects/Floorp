/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Scott Collins <scc@mozilla.org> (original author of nsCOMPtr)
 *   L. David Baron <dbaron@dbaron.org>
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

#ifndef nsAutoPtr_h___
#define nsAutoPtr_h___

  // Wrapping includes can speed up compiles (see "Large Scale C++ Software Design")
#ifndef nsCOMPtr_h___
  // For |already_AddRefed|, |nsDerivedSafe|, |NSCAP_Zero|,
  // |NSCAP_DONT_PROVIDE_NONCONST_OPEQ|,
  // |NSCAP_FEATURE_INLINE_STARTASSIGNMENT|
#include "nsCOMPtr.h"
#endif

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
          return NS_REINTERPRET_CAST(void**, &mRawPtr);
        }

      void
      assign( T* newPtr )
        {
          T* oldPtr = mRawPtr;
          mRawPtr = newPtr;
          delete oldPtr;
        }

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

      nsAutoPtr( T* aRawPtr )
            : mRawPtr(aRawPtr)
          // construct from a raw pointer (of the right type)
        {
        }

      nsAutoPtr( nsAutoPtr<T>& aSmartPtr )
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

#ifdef CANT_RESOLVE_CPP_CONST_AMBIGUITY
  // broken version for IRIX

      nsAutoPtr<T>*
      get_address() const
          // This is not intended to be used by clients.  See |address_of|
          // below.
        {
          return NS_CONST_CAST(nsAutoPtr<T>*, this);
        }

#else // CANT_RESOLVE_CPP_CONST_AMBIGUITY

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

#endif // CANT_RESOLVE_CPP_CONST_AMBIGUITY

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
          return NS_REINTERPRET_CAST(T**, begin_assignment());
#else
          assign(0);
          return NS_REINTERPRET_CAST(T**, &mRawPtr);
#endif
        }
  };

#ifdef CANT_RESOLVE_CPP_CONST_AMBIGUITY

// This is the broken version for IRIX, which can't handle the version below.

template <class T>
inline
nsAutoPtr<T>*
address_of( const nsAutoPtr<T>& aPtr )
  {
    return aPtr.get_address();
  }

#else // CANT_RESOLVE_CPP_CONST_AMBIGUITY

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

#endif // CANT_RESOLVE_CPP_CONST_AMBIGUITY

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
          return NS_REINTERPRET_CAST(void**, mTargetSmartPtr.StartAssignment());
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
NSCAP_BOOL
operator==( const nsAutoPtr<T>& lhs, const nsAutoPtr<U>& rhs )
  {
    return NS_STATIC_CAST(const T*, lhs.get()) == NS_STATIC_CAST(const U*, rhs.get());
  }


template <class T, class U>
inline
NSCAP_BOOL
operator!=( const nsAutoPtr<T>& lhs, const nsAutoPtr<U>& rhs )
  {
    return NS_STATIC_CAST(const T*, lhs.get()) != NS_STATIC_CAST(const U*, rhs.get());
  }


  // Comparing an |nsAutoPtr| to a raw pointer

template <class T, class U>
inline
NSCAP_BOOL
operator==( const nsAutoPtr<T>& lhs, const U* rhs )
  {
    return NS_STATIC_CAST(const T*, lhs.get()) == NS_STATIC_CAST(const U*, rhs);
  }

template <class T, class U>
inline
NSCAP_BOOL
operator==( const U* lhs, const nsAutoPtr<T>& rhs )
  {
    return NS_STATIC_CAST(const U*, lhs) == NS_STATIC_CAST(const T*, rhs.get());
  }

template <class T, class U>
inline
NSCAP_BOOL
operator!=( const nsAutoPtr<T>& lhs, const U* rhs )
  {
    return NS_STATIC_CAST(const T*, lhs.get()) != NS_STATIC_CAST(const U*, rhs);
  }

template <class T, class U>
inline
NSCAP_BOOL
operator!=( const U* lhs, const nsAutoPtr<T>& rhs )
  {
    return NS_STATIC_CAST(const U*, lhs) != NS_STATIC_CAST(const T*, rhs.get());
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
NSCAP_BOOL
operator==( const nsAutoPtr<T>& lhs, U* rhs )
  {
    return NS_STATIC_CAST(const T*, lhs.get()) == NS_CONST_CAST(const U*, rhs);
  }

template <class T, class U>
inline
NSCAP_BOOL
operator==( U* lhs, const nsAutoPtr<T>& rhs )
  {
    return NS_CONST_CAST(const U*, lhs) == NS_STATIC_CAST(const T*, rhs.get());
  }

template <class T, class U>
inline
NSCAP_BOOL
operator!=( const nsAutoPtr<T>& lhs, U* rhs )
  {
    return NS_STATIC_CAST(const T*, lhs.get()) != NS_CONST_CAST(const U*, rhs);
  }

template <class T, class U>
inline
NSCAP_BOOL
operator!=( U* lhs, const nsAutoPtr<T>& rhs )
  {
    return NS_CONST_CAST(const U*, lhs) != NS_STATIC_CAST(const T*, rhs.get());
  }
#endif



  // Comparing an |nsAutoPtr| to |0|

template <class T>
inline
NSCAP_BOOL
operator==( const nsAutoPtr<T>& lhs, NSCAP_Zero* rhs )
    // specifically to allow |smartPtr == 0|
  {
    return NS_STATIC_CAST(const void*, lhs.get()) == NS_REINTERPRET_CAST(const void*, rhs);
  }

template <class T>
inline
NSCAP_BOOL
operator==( NSCAP_Zero* lhs, const nsAutoPtr<T>& rhs )
    // specifically to allow |0 == smartPtr|
  {
    return NS_REINTERPRET_CAST(const void*, lhs) == NS_STATIC_CAST(const void*, rhs.get());
  }

template <class T>
inline
NSCAP_BOOL
operator!=( const nsAutoPtr<T>& lhs, NSCAP_Zero* rhs )
    // specifically to allow |smartPtr != 0|
  {
    return NS_STATIC_CAST(const void*, lhs.get()) != NS_REINTERPRET_CAST(const void*, rhs);
  }

template <class T>
inline
NSCAP_BOOL
operator!=( NSCAP_Zero* lhs, const nsAutoPtr<T>& rhs )
    // specifically to allow |0 != smartPtr|
  {
    return NS_REINTERPRET_CAST(const void*, lhs) != NS_STATIC_CAST(const void*, rhs.get());
  }


#ifdef HAVE_CPP_TROUBLE_COMPARING_TO_ZERO

  // We need to explicitly define comparison operators for `int'
  // because the compiler is lame.

template <class T>
inline
NSCAP_BOOL
operator==( const nsAutoPtr<T>& lhs, int rhs )
    // specifically to allow |smartPtr == 0|
  {
    return NS_STATIC_CAST(const void*, lhs.get()) == NS_REINTERPRET_CAST(const void*, rhs);
  }

template <class T>
inline
NSCAP_BOOL
operator==( int lhs, const nsAutoPtr<T>& rhs )
    // specifically to allow |0 == smartPtr|
  {
    return NS_REINTERPRET_CAST(const void*, lhs) == NS_STATIC_CAST(const void*, rhs.get());
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
          return NS_REINTERPRET_CAST(void**, &mRawPtr);
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

#ifdef CANT_RESOLVE_CPP_CONST_AMBIGUITY
  // broken version for IRIX

      nsAutoArrayPtr<T>*
      get_address() const
          // This is not intended to be used by clients.  See |address_of|
          // below.
        {
          return NS_CONST_CAST(nsAutoArrayPtr<T>*, this);
        }

#else // CANT_RESOLVE_CPP_CONST_AMBIGUITY

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

#endif // CANT_RESOLVE_CPP_CONST_AMBIGUITY

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
          return NS_REINTERPRET_CAST(T**, begin_assignment());
#else
          assign(0);
          return NS_REINTERPRET_CAST(T**, &mRawPtr);
#endif
        }
  };

#ifdef CANT_RESOLVE_CPP_CONST_AMBIGUITY

// This is the broken version for IRIX, which can't handle the version below.

template <class T>
inline
nsAutoArrayPtr<T>*
address_of( const nsAutoArrayPtr<T>& aPtr )
  {
    return aPtr.get_address();
  }

#else // CANT_RESOLVE_CPP_CONST_AMBIGUITY

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

#endif // CANT_RESOLVE_CPP_CONST_AMBIGUITY

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
          return NS_REINTERPRET_CAST(void**, mTargetSmartPtr.StartAssignment());
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
NSCAP_BOOL
operator==( const nsAutoArrayPtr<T>& lhs, const nsAutoArrayPtr<U>& rhs )
  {
    return NS_STATIC_CAST(const T*, lhs.get()) == NS_STATIC_CAST(const U*, rhs.get());
  }


template <class T, class U>
inline
NSCAP_BOOL
operator!=( const nsAutoArrayPtr<T>& lhs, const nsAutoArrayPtr<U>& rhs )
  {
    return NS_STATIC_CAST(const T*, lhs.get()) != NS_STATIC_CAST(const U*, rhs.get());
  }


  // Comparing an |nsAutoArrayPtr| to a raw pointer

template <class T, class U>
inline
NSCAP_BOOL
operator==( const nsAutoArrayPtr<T>& lhs, const U* rhs )
  {
    return NS_STATIC_CAST(const T*, lhs.get()) == NS_STATIC_CAST(const U*, rhs);
  }

template <class T, class U>
inline
NSCAP_BOOL
operator==( const U* lhs, const nsAutoArrayPtr<T>& rhs )
  {
    return NS_STATIC_CAST(const U*, lhs) == NS_STATIC_CAST(const T*, rhs.get());
  }

template <class T, class U>
inline
NSCAP_BOOL
operator!=( const nsAutoArrayPtr<T>& lhs, const U* rhs )
  {
    return NS_STATIC_CAST(const T*, lhs.get()) != NS_STATIC_CAST(const U*, rhs);
  }

template <class T, class U>
inline
NSCAP_BOOL
operator!=( const U* lhs, const nsAutoArrayPtr<T>& rhs )
  {
    return NS_STATIC_CAST(const U*, lhs) != NS_STATIC_CAST(const T*, rhs.get());
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
NSCAP_BOOL
operator==( const nsAutoArrayPtr<T>& lhs, U* rhs )
  {
    return NS_STATIC_CAST(const T*, lhs.get()) == NS_CONST_CAST(const U*, rhs);
  }

template <class T, class U>
inline
NSCAP_BOOL
operator==( U* lhs, const nsAutoArrayPtr<T>& rhs )
  {
    return NS_CONST_CAST(const U*, lhs) == NS_STATIC_CAST(const T*, rhs.get());
  }

template <class T, class U>
inline
NSCAP_BOOL
operator!=( const nsAutoArrayPtr<T>& lhs, U* rhs )
  {
    return NS_STATIC_CAST(const T*, lhs.get()) != NS_CONST_CAST(const U*, rhs);
  }

template <class T, class U>
inline
NSCAP_BOOL
operator!=( U* lhs, const nsAutoArrayPtr<T>& rhs )
  {
    return NS_CONST_CAST(const U*, lhs) != NS_STATIC_CAST(const T*, rhs.get());
  }
#endif



  // Comparing an |nsAutoArrayPtr| to |0|

template <class T>
inline
NSCAP_BOOL
operator==( const nsAutoArrayPtr<T>& lhs, NSCAP_Zero* rhs )
    // specifically to allow |smartPtr == 0|
  {
    return NS_STATIC_CAST(const void*, lhs.get()) == NS_REINTERPRET_CAST(const void*, rhs);
  }

template <class T>
inline
NSCAP_BOOL
operator==( NSCAP_Zero* lhs, const nsAutoArrayPtr<T>& rhs )
    // specifically to allow |0 == smartPtr|
  {
    return NS_REINTERPRET_CAST(const void*, lhs) == NS_STATIC_CAST(const void*, rhs.get());
  }

template <class T>
inline
NSCAP_BOOL
operator!=( const nsAutoArrayPtr<T>& lhs, NSCAP_Zero* rhs )
    // specifically to allow |smartPtr != 0|
  {
    return NS_STATIC_CAST(const void*, lhs.get()) != NS_REINTERPRET_CAST(const void*, rhs);
  }

template <class T>
inline
NSCAP_BOOL
operator!=( NSCAP_Zero* lhs, const nsAutoArrayPtr<T>& rhs )
    // specifically to allow |0 != smartPtr|
  {
    return NS_REINTERPRET_CAST(const void*, lhs) != NS_STATIC_CAST(const void*, rhs.get());
  }


#ifdef HAVE_CPP_TROUBLE_COMPARING_TO_ZERO

  // We need to explicitly define comparison operators for `int'
  // because the compiler is lame.

template <class T>
inline
NSCAP_BOOL
operator==( const nsAutoArrayPtr<T>& lhs, int rhs )
    // specifically to allow |smartPtr == 0|
  {
    return NS_STATIC_CAST(const void*, lhs.get()) == NS_REINTERPRET_CAST(const void*, rhs);
  }

template <class T>
inline
NSCAP_BOOL
operator==( int lhs, const nsAutoArrayPtr<T>& rhs )
    // specifically to allow |0 == smartPtr|
  {
    return NS_REINTERPRET_CAST(const void*, lhs) == NS_STATIC_CAST(const void*, rhs.get());
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
          return NS_REINTERPRET_CAST(void**, &mRawPtr);
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

      nsRefPtr( const already_AddRefed<T>& aSmartPtr )
            : mRawPtr(aSmartPtr.mRawPtr)
          // construct from |dont_AddRef(expr)|
        {
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

      nsRefPtr<T>&
      operator=( const already_AddRefed<T>& rhs )
          // assign from |dont_AddRef(expr)|
        {
          assign_assuming_AddRef(rhs.mRawPtr);
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

      nsDerivedSafe<T>*
      get() const
          /*
            Prefer the implicit conversion provided automatically by |operator nsDerivedSafe<T>*() const|.
             Use |get()| _only_ to resolve ambiguity.

            Returns a |nsDerivedSafe<T>*| to deny clients the use of |AddRef| and |Release|.
          */
        {
          return NS_CONST_CAST(nsDerivedSafe<T>*,
                        NS_REINTERPRET_CAST(const nsDerivedSafe<T>*, mRawPtr));
        }

      operator nsDerivedSafe<T>*() const
          /*
            ...makes an |nsRefPtr| act like its underlying raw pointer type (except against |AddRef()|, |Release()|,
              and |delete|) whenever it is used in a context where a raw pointer is expected.  It is this operator
              that makes an |nsRefPtr| substitutable for a raw pointer.

            Prefer the implicit use of this operator to calling |get()|, except where necessary to resolve ambiguity.
          */
        {
          return get();
        }

      nsDerivedSafe<T>*
      operator->() const
        {
          NS_PRECONDITION(mRawPtr != 0, "You can't dereference a NULL nsRefPtr with operator->().");
          return get();
        }

#ifdef CANT_RESOLVE_CPP_CONST_AMBIGUITY
  // broken version for IRIX

      nsRefPtr<T>*
      get_address() const
          // This is not intended to be used by clients.  See |address_of|
          // below.
        {
          return NS_CONST_CAST(nsRefPtr<T>*, this);
        }

#else // CANT_RESOLVE_CPP_CONST_AMBIGUITY

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

#endif // CANT_RESOLVE_CPP_CONST_AMBIGUITY

    public:
      nsDerivedSafe<T>&
      operator*() const
        {
          NS_PRECONDITION(mRawPtr != 0, "You can't dereference a NULL nsRefPtr with operator*().");
          return *get();
        }

      T**
      StartAssignment()
        {
#ifndef NSCAP_FEATURE_INLINE_STARTASSIGNMENT
          return NS_REINTERPRET_CAST(T**, begin_assignment());
#else
          assign_assuming_AddRef(0);
          return NS_REINTERPRET_CAST(T**, &mRawPtr);
#endif
        }
  };

#ifdef CANT_RESOLVE_CPP_CONST_AMBIGUITY

// This is the broken version for IRIX, which can't handle the version below.

template <class T>
inline
nsRefPtr<T>*
address_of( const nsRefPtr<T>& aPtr )
  {
    return aPtr.get_address();
  }

#else // CANT_RESOLVE_CPP_CONST_AMBIGUITY

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

#endif // CANT_RESOLVE_CPP_CONST_AMBIGUITY

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
          return NS_REINTERPRET_CAST(void**, mTargetSmartPtr.StartAssignment());
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
NSCAP_BOOL
operator==( const nsRefPtr<T>& lhs, const nsRefPtr<U>& rhs )
  {
    return NS_STATIC_CAST(const T*, lhs.get()) == NS_STATIC_CAST(const U*, rhs.get());
  }


template <class T, class U>
inline
NSCAP_BOOL
operator!=( const nsRefPtr<T>& lhs, const nsRefPtr<U>& rhs )
  {
    return NS_STATIC_CAST(const T*, lhs.get()) != NS_STATIC_CAST(const U*, rhs.get());
  }


  // Comparing an |nsRefPtr| to a raw pointer

template <class T, class U>
inline
NSCAP_BOOL
operator==( const nsRefPtr<T>& lhs, const U* rhs )
  {
    return NS_STATIC_CAST(const T*, lhs.get()) == NS_STATIC_CAST(const U*, rhs);
  }

template <class T, class U>
inline
NSCAP_BOOL
operator==( const U* lhs, const nsRefPtr<T>& rhs )
  {
    return NS_STATIC_CAST(const U*, lhs) == NS_STATIC_CAST(const T*, rhs.get());
  }

template <class T, class U>
inline
NSCAP_BOOL
operator!=( const nsRefPtr<T>& lhs, const U* rhs )
  {
    return NS_STATIC_CAST(const T*, lhs.get()) != NS_STATIC_CAST(const U*, rhs);
  }

template <class T, class U>
inline
NSCAP_BOOL
operator!=( const U* lhs, const nsRefPtr<T>& rhs )
  {
    return NS_STATIC_CAST(const U*, lhs) != NS_STATIC_CAST(const T*, rhs.get());
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
NSCAP_BOOL
operator==( const nsRefPtr<T>& lhs, U* rhs )
  {
    return NS_STATIC_CAST(const T*, lhs.get()) == NS_CONST_CAST(const U*, rhs);
  }

template <class T, class U>
inline
NSCAP_BOOL
operator==( U* lhs, const nsRefPtr<T>& rhs )
  {
    return NS_CONST_CAST(const U*, lhs) == NS_STATIC_CAST(const T*, rhs.get());
  }

template <class T, class U>
inline
NSCAP_BOOL
operator!=( const nsRefPtr<T>& lhs, U* rhs )
  {
    return NS_STATIC_CAST(const T*, lhs.get()) != NS_CONST_CAST(const U*, rhs);
  }

template <class T, class U>
inline
NSCAP_BOOL
operator!=( U* lhs, const nsRefPtr<T>& rhs )
  {
    return NS_CONST_CAST(const U*, lhs) != NS_STATIC_CAST(const T*, rhs.get());
  }
#endif



  // Comparing an |nsRefPtr| to |0|

template <class T>
inline
NSCAP_BOOL
operator==( const nsRefPtr<T>& lhs, NSCAP_Zero* rhs )
    // specifically to allow |smartPtr == 0|
  {
    return NS_STATIC_CAST(const void*, lhs.get()) == NS_REINTERPRET_CAST(const void*, rhs);
  }

template <class T>
inline
NSCAP_BOOL
operator==( NSCAP_Zero* lhs, const nsRefPtr<T>& rhs )
    // specifically to allow |0 == smartPtr|
  {
    return NS_REINTERPRET_CAST(const void*, lhs) == NS_STATIC_CAST(const void*, rhs.get());
  }

template <class T>
inline
NSCAP_BOOL
operator!=( const nsRefPtr<T>& lhs, NSCAP_Zero* rhs )
    // specifically to allow |smartPtr != 0|
  {
    return NS_STATIC_CAST(const void*, lhs.get()) != NS_REINTERPRET_CAST(const void*, rhs);
  }

template <class T>
inline
NSCAP_BOOL
operator!=( NSCAP_Zero* lhs, const nsRefPtr<T>& rhs )
    // specifically to allow |0 != smartPtr|
  {
    return NS_REINTERPRET_CAST(const void*, lhs) != NS_STATIC_CAST(const void*, rhs.get());
  }


#ifdef HAVE_CPP_TROUBLE_COMPARING_TO_ZERO

  // We need to explicitly define comparison operators for `int'
  // because the compiler is lame.

template <class T>
inline
NSCAP_BOOL
operator==( const nsRefPtr<T>& lhs, int rhs )
    // specifically to allow |smartPtr == 0|
  {
    return NS_STATIC_CAST(const void*, lhs.get()) == NS_REINTERPRET_CAST(const void*, rhs);
  }

template <class T>
inline
NSCAP_BOOL
operator==( int lhs, const nsRefPtr<T>& rhs )
    // specifically to allow |0 == smartPtr|
  {
    return NS_REINTERPRET_CAST(const void*, lhs) == NS_STATIC_CAST(const void*, rhs.get());
  }

#endif // !defined(HAVE_CPP_TROUBLE_COMPARING_TO_ZERO)

/*****************************************************************************/

#endif // !defined(nsAutoPtr_h___)
