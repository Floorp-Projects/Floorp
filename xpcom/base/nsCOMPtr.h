/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsCOMPtr_h___
#define nsCOMPtr_h___



  // Wrapping includes can speed up compiles (see "Large Scale C++ Software Design")
#ifndef nsDebug_h___
#include "nsDebug.h"
  // for |NS_PRECONDITION|
#endif

#ifndef nsISupports_h___
#include "nsISupports.h"
  // for |nsresult|, |NS_ADDREF|, et al
#endif

#ifndef __gen_nsIWeakReference_h__
#include "nsIWeakReference.h"
	// for |nsIWeakReference|
#endif

/*
  Public things defined in this file:

                                                        T* rawTptr;
    class nsCOMPtr<T>                                   nsCOMPtr<T> smartTptr;

    null_nsCOMPtr()                                     smartTptr = null_nsCOMPtr();

    do_QueryInterface( nsISupports* )                   smartTptr = do_QueryInterface(other_ptr);
    do_QueryInterface( nsISupports*, nsresult* )        smartTptr = do_QueryInterface(other_ptr, &status);

    dont_QueryInterface( T* )                           smartTptr = dont_QueryInterface(rawTptr);

    getter_AddRefs( nsCOMPtr<T>& )
    getter_AddRefs( T* )
    dont_AddRef( T* )

    CallQueryInterface( nsISupports*, T** )
    CallQueryInterface( nsISupports*, nsCOMPtr<T>* )
*/

/*
  Having problems?
  
  See the User Manual at:
    <http://www.meer.net/ScottCollins/doc/nsCOMPtr.html>, or
    <http://www.mozilla.org/projects/xpcom/nsCOMPtr.html>
*/




/*
  TO DO...
  
    + Improve internal documentation
      + mention *&
      + do_QueryInterface
*/

/*
  WARNING:
    This file defines several macros for internal use only.  These macros begin with the
    prefix |NSCAP_|.  Do not use these macros in your own code.  They are for internal use
    only for cross-platform compatibility, and are subject to change without notice.
*/


  /*
    Set up some |#define|s to turn off a couple of troublesome C++ features.
    Interestingly, none of the compilers barf on template stuff.  These are set up automatically
    by the autoconf system for all Unixes.  (Temporarily, I hope) I have to define them
    myself for Mac and Windows.
  */

  // under Metrowerks (Mac), we don't have autoconf yet
#ifdef __MWERKS__
  #define HAVE_CPP_USING
  #define HAVE_CPP_EXPLICIT
  #define HAVE_CPP_NEW_CASTS
  #define HAVE_CPP_BOOL
#endif

  // under VC++ (Windows), we don't have autoconf yet
#ifdef _MSC_VER
  #define HAVE_CPP_EXPLICIT
  #define HAVE_CPP_USING
  #define HAVE_CPP_NEW_CASTS

  #if (_MSC_VER<1100)
      // before 5.0, VC++ couldn't handle explicit
    #undef HAVE_CPP_EXPLICIT
  #elif (_MSC_VER==1100)
      // VC++5.0 has an internal compiler error (sometimes) without this
    #undef HAVE_CPP_USING
  #endif

  #define NSCAP_FEATURE_INLINE_STARTASSIGNMENT
    // under VC++, we win by inlining StartAssignment
#endif

#define NSCAP_FEATURE_FACTOR_DESTRUCTOR

#ifdef NS_DEBUG
	#define NSCAP_FEATURE_TEST_DONTQUERY_CASES
	#define NSCAP_FEATURE_DEBUG_PTR_TYPES
#endif

	/*
		|...TEST_DONTQUERY_CASES| and |...DEBUG_PTR_TYPES| introduce some code that is 
		problematic on a select few of our platforms, e.g., QNX.  Therefore, I'm providing
		a mechanism by which these features can be explicitly disabled from the command-line.
	*/

#ifdef NSCAP_DISABLE_TEST_DONTQUERY_CASES
	#undef NSCAP_FEATURE_TEST_DONTQUERY_CASES
#endif

#if defined(NSCAP_DISABLE_DEBUG_PTR_TYPES) || !defined(NS_DEBUG)
	#undef NSCAP_FEATURE_DEBUG_PTR_TYPES
#endif

#ifdef NSCAP_FEATURE_DEBUG_PTR_TYPES
	#undef NSCAP_FEATURE_FACTOR_DESTRUCTOR
#endif


  /*
    If the compiler doesn't support |explicit|, we'll just make it go away, trusting
    that the builds under compilers that do have it will keep us on the straight and narrow.
  */
#ifndef HAVE_CPP_EXPLICIT
  #define explicit
#endif

#ifdef HAVE_CPP_BOOL
  typedef bool NSCAP_BOOL;
#else
  typedef PRBool NSCAP_BOOL;
#endif

#ifdef HAVE_CPP_NEW_CASTS
  #define NSCAP_STATIC_CAST(T,x)       static_cast<T>(x)
  #define NSCAP_REINTERPRET_CAST(T,x)  reinterpret_cast<T>(x)
#else
  #define NSCAP_STATIC_CAST(T,x) ((T)(x))
  #define NSCAP_REINTERPRET_CAST(T,x) ((T)(x))
#endif

#ifdef NSCAP_FEATURE_DEBUG_MACROS
  #define NSCAP_ADDREF(ptr)    NS_ADDREF(ptr)
  #define NSCAP_RELEASE(ptr)   NS_RELEASE(ptr)
#else
  #define NSCAP_ADDREF(ptr)    (ptr)->AddRef()
  #define NSCAP_RELEASE(ptr)   (ptr)->Release()
#endif

  /*
    WARNING:
      VC++4.2 is very picky.  To compile under VC++4.2, the classes must be defined
      in an order that satisfies:
    
        nsDerivedSafe < nsCOMPtr
        nsDontAddRef < nsCOMPtr
        nsCOMPtr < nsGetterAddRefs

      The other compilers probably won't complain, so please don't reorder these
      classes, on pain of breaking 4.2 compatibility.
  */


template <class T>
class nsDerivedSafe : public T
    /*
      No client should ever see or have to type the name of this class.  It is the
      artifact that makes it a compile-time error to call |AddRef| and |Release|
      on a |nsCOMPtr|.  DO NOT USE THIS TYPE DIRECTLY IN YOUR CODE.

      See |nsCOMPtr::operator->|, |nsCOMPtr::operator*|, et al.

			This type should be a nested class inside |nsCOMPtr<T>|.
    */
  {
    private:
#ifdef HAVE_CPP_USING
      using T::AddRef;
      using T::Release;
#else
      NS_IMETHOD_(nsrefcnt) AddRef(void);
      NS_IMETHOD_(nsrefcnt) Release(void);
#endif

      void operator delete( void*, size_t );                  // NOT TO BE IMPLEMENTED
        // declaring |operator delete| private makes calling delete on an interface pointer a compile error

				/*
					The new way (not commented-out, below) didn't work until I chopped |operator=()| out of
					|nsISupportsArray|.  I have tested this on as many platforms as I can, I'm switching it on now.
					I'm leaving the old way commented out, but visible so we can switch back if there is any
					problem.
				*/

//    nsDerivedSafe<T>& operator=( const nsDerivedSafe<T>& ); // NOT TO BE IMPLEMENTED
    	nsDerivedSafe<T>& operator=( const T& );                // NOT TO BE IMPLEMENTED
        // you may not call |operator=()| through a dereferenced |nsCOMPtr|, because you'd get the wrong one

				/*
					Compiler warnings and errors: nsDerivedSafe operator=() hides inherited operator=().
					If you see that, that means somebody checked in a (XP)COM interface class that declares an
					|operator=()|, and that's _bad_.  So bad, in fact, that this declaration exists explicitly
					to stop people from doing it.
				*/
  };

#if !defined(HAVE_CPP_USING) && defined(NEED_CPP_UNUSED_IMPLEMENTATIONS)
template <class T>
nsrefcnt
nsDerivedSafe<T>::AddRef()
  {
    return 0;
  }

template <class T>
nsrefcnt
nsDerivedSafe<T>::Release()
  {
    return 0;
  }

#endif




template <class T>
struct nsDontQueryInterface
    /*
      ...

      DO NOT USE THIS TYPE DIRECTLY IN YOUR CODE.  Use |dont_QueryInterface()| instead.

			This type should be a nested class inside |nsCOMPtr<T>|.
    */
  {
    explicit
    nsDontQueryInterface( T* aRawPtr )
        : mRawPtr(aRawPtr)
      {
        // nothing else to do here
      }

    T* mRawPtr;
  };

template <class T>
inline
const nsDontQueryInterface<T>
dont_QueryInterface( T* aRawPtr )
  {
    return nsDontQueryInterface<T>(aRawPtr);
  }


#if 1

	template <class T>
	struct
	nsCOMPtrQueryRequest
		{
			T*				mRawPtr;
			nsresult*	mErrorPtr;

			explicit
			nsCOMPtrQueryRequest( T* aRawPtr, nsresult* error = 0 )
					: mRawPtr(aRawPtr),
						mErrorPtr(error)
				{
					// nothing else to do here
				}
		};

	typedef nsCOMPtrQueryRequest<nsISupports>				nsQueryInterface;
	typedef nsCOMPtrQueryRequest<nsIWeakReference>	nsQueryReferent;

#else

	struct nsQueryInterface
	    /*
	      ...

	      DO NOT USE THIS TYPE DIRECTLY IN YOUR CODE.  Use |do_QueryInterface()| instead.
	    */
	  {
	    explicit
	    nsQueryInterface( nsISupports* aRawPtr, nsresult* error = 0 )
	        : mRawPtr(aRawPtr),
	          mErrorPtr(error)
	      {
	        // nothing else to do here
	      }

	    nsISupports* mRawPtr;
	    nsresult*    mErrorPtr;
	  };

	struct nsQueryReferent
		{
			explicit
			nsQueryReferent( nsIWeakReference* aRawPtr, nsresult* error = 0 )
					: mRawPtr(aRawPtr),
						mErrorPtr(error)
				{
					// nothing else to do here
				}

			nsIWeakReference* mRawPtr;
			nsresult*					mErrorPtr;
		};

#endif

inline
const nsQueryInterface
do_QueryInterface( nsISupports* aRawPtr, nsresult* error = 0 )
  {
    return nsQueryInterface(aRawPtr, error);
  }

inline
const nsQueryReferent
do_QueryReferent( nsIWeakReference* aRawPtr, nsresult* error = 0 )
	{
		return nsQueryReferent(aRawPtr, error);
	}


#define null_nsCOMPtr() (0)



template <class T>
struct nsDontAddRef
    /*
      ...cooperates with |nsCOMPtr| to allow you to assign in a pointer _without_
      |AddRef|ing it.  You would rarely use this directly, but rather through the
      machinery of |getter_AddRefs| in the argument list to functions that |AddRef|
      their results before returning them to the caller.

      DO NOT USE THIS TYPE DIRECTLY IN YOUR CODE.  Use |getter_AddRefs()| or
      |dont_AddRef()| instead.

      See also |getter_AddRefs()|, |dont_AddRef()|, and |class nsGetterAddRefs|.

			This type should be a nested class inside |nsCOMPtr<T>|.
    */
  {
    explicit
    nsDontAddRef( T* aRawPtr )
        : mRawPtr(aRawPtr)
      {
        // nothing else to do here
      }

    T* mRawPtr;
  };

template <class T>
inline
const nsDontAddRef<T>
getter_AddRefs( T* aRawPtr )
    /*
      ...makes typing easier, because it deduces the template type, e.g., 
      you write |dont_AddRef(fooP)| instead of |nsDontAddRef<IFoo>(fooP)|.
    */
  {
    return nsDontAddRef<T>(aRawPtr);
  }

template <class T>
inline
const nsDontAddRef<T>
dont_AddRef( T* aRawPtr )
  {
    return nsDontAddRef<T>(aRawPtr);
  }



class nsCOMPtr_base
    /*
      ...factors implementation for all template versions of |nsCOMPtr|.

			This should really be an |nsCOMPtr<nsISupports>|, but this wouldn't work
			because unlike the

			Here's the way people normally do things like this
			
				template <class T> class Foo { ... };
				template <> class Foo<void*> { ... };
				template <class T> class Foo<T*> : private Foo<void*> { ... };
    */
  {
    public:

      nsCOMPtr_base( nsISupports* rawPtr = 0 )
          : mRawPtr(rawPtr)
        {
          // nothing else to do here
        }

#ifdef NSCAP_FEATURE_FACTOR_DESTRUCTOR
     NS_EXPORT ~nsCOMPtr_base();
#endif

      NS_EXPORT void    assign_with_AddRef( nsISupports* );
      NS_EXPORT void    assign_with_QueryInterface( nsISupports*, const nsIID&, nsresult* );
      NS_EXPORT void    assign_with_QueryReferent( nsIWeakReference*, const nsIID&, nsresult* );
      NS_EXPORT void**  begin_assignment();

    protected:
      nsISupports* mRawPtr;
  };

// template <class T> class nsGetterAddRefs;

template <class T>
class nsCOMPtr
#ifndef NSCAP_FEATURE_DEBUG_PTR_TYPES
		: private nsCOMPtr_base
#endif
  {
#ifdef NSCAP_FEATURE_DEBUG_PTR_TYPES
		private:
			void		assign_with_AddRef( nsISupports* );
			void		assign_with_QueryInterface( nsISupports*, const nsIID&, nsresult* );
			void		assign_with_QueryReferent( nsIWeakReference*, const nsIID&, nsresult* );
			void**	begin_assignment();

		private:
			T* mRawPtr;

	#define NSCAP_CTOR_BASE(x) mRawPtr(x)
#else
	#define NSCAP_CTOR_BASE(x) nsCOMPtr_base(x)
#endif

    public:
      typedef T element_type;
      
#ifndef NSCAP_FEATURE_FACTOR_DESTRUCTOR
     ~nsCOMPtr()
        {
          if ( mRawPtr )
            NSCAP_RELEASE(mRawPtr);
        }
#endif

      nsCOMPtr()
      		: NSCAP_CTOR_BASE(0)
        {
          // nothing else to do here
        }

      nsCOMPtr( const nsQueryInterface& aSmartPtr )
      		: NSCAP_CTOR_BASE(0)
        {
          assign_with_QueryInterface(aSmartPtr.mRawPtr, nsCOMTypeInfo<T>::GetIID(), aSmartPtr.mErrorPtr);
        }

			nsCOMPtr( const nsQueryReferent& aSmartPtr )
					: NSCAP_CTOR_BASE(0)
				{
					assign_with_QueryReferent(aSmartPtr.mRawPtr, nsCOMTypeInfo<T>::GetIID(), aSmartPtr.mErrorPtr);
				}

#ifdef NSCAP_FEATURE_TEST_DONTQUERY_CASES
			void
			Assert_NoQueryNeeded()
				{
          if ( mRawPtr )
          	{
							T* query_result = 0;
							nsresult status = CallQueryInterface(mRawPtr, &query_result);
							NS_ASSERTION(query_result == mRawPtr, "QueryInterface needed");
							if ( NS_SUCCEEDED(status) )
								NSCAP_RELEASE(query_result);
						}
				}

			#define NSCAP_ASSERT_NO_QUERY_NEEDED();		Assert_NoQueryNeeded();
#else
			#define NSCAP_ASSERT_NO_QUERY_NEEDED();
#endif

      nsCOMPtr( const nsDontAddRef<T>& aSmartPtr )
      		: NSCAP_CTOR_BASE(aSmartPtr.mRawPtr)
        {
					NSCAP_ASSERT_NO_QUERY_NEEDED();
        }

      nsCOMPtr( const nsDontQueryInterface<T>& aSmartPtr )
      		: NSCAP_CTOR_BASE(aSmartPtr.mRawPtr)
        {
          if ( mRawPtr )
            NSCAP_ADDREF(mRawPtr);

					NSCAP_ASSERT_NO_QUERY_NEEDED();
        }

      nsCOMPtr( const nsCOMPtr<T>& aSmartPtr )
      		: NSCAP_CTOR_BASE(aSmartPtr.mRawPtr)
        {
          if ( mRawPtr )
            NSCAP_ADDREF(mRawPtr);
        }

      nsCOMPtr( T* aRawPtr )
      		: NSCAP_CTOR_BASE(aRawPtr)
        {
          if ( mRawPtr )
            NSCAP_ADDREF(mRawPtr);

					NSCAP_ASSERT_NO_QUERY_NEEDED();
        }

      nsCOMPtr<T>&
      operator=( T* rhs )
        {
          assign_with_AddRef(rhs);
					NSCAP_ASSERT_NO_QUERY_NEEDED();
          return *this;
        }

      nsCOMPtr<T>&
      operator=( const nsQueryInterface& rhs )
        {
          assign_with_QueryInterface(rhs.mRawPtr, nsCOMTypeInfo<T>::GetIID(), rhs.mErrorPtr);
          return *this;
        }

      nsCOMPtr<T>&
      operator=( const nsQueryReferent& rhs )
        {
          assign_with_QueryReferent(rhs.mRawPtr, nsCOMTypeInfo<T>::GetIID(), rhs.mErrorPtr);
          return *this;
        }

      nsCOMPtr<T>&
      operator=( const nsDontAddRef<T>& rhs )
        {
          if ( mRawPtr )
            NSCAP_RELEASE(mRawPtr);
          mRawPtr = rhs.mRawPtr;

					NSCAP_ASSERT_NO_QUERY_NEEDED();
          return *this;
        }

      nsCOMPtr<T>&
      operator=( const nsDontQueryInterface<T>& rhs )
        {
          assign_with_AddRef(rhs.mRawPtr);
					NSCAP_ASSERT_NO_QUERY_NEEDED();
          return *this;
        }

      nsCOMPtr<T>&
      operator=( const nsCOMPtr<T>& rhs )
        {
          assign_with_AddRef(rhs.mRawPtr);
          return *this;
        }

      nsDerivedSafe<T>*
      get() const
          // returns a |nsDerivedSafe<T>*| to deny clients the use of |AddRef| and |Release|
        {
          return NSCAP_REINTERPRET_CAST(nsDerivedSafe<T>*, mRawPtr);
        }

      nsDerivedSafe<T>*
      operator->() const
          // returns a |nsDerivedSafe<T>*| to deny clients the use of |AddRef| and |Release|
        {
          NS_PRECONDITION(mRawPtr != 0, "You can't dereference a NULL nsCOMPtr with operator->().");
          return get();
        }

      nsDerivedSafe<T>&
      operator*() const
          // returns a |nsDerivedSafe<T>*| to deny clients the use of |AddRef| and |Release|
        {
          NS_PRECONDITION(mRawPtr != 0, "You can't dereference a NULL nsCOMPtr with operator*().");
          return *get();
        }

      operator nsDerivedSafe<T>*() const
        {
          return get();
        }

#if 0
    private:
      friend class nsGetterAddRefs<T>;
#endif

      T**
      StartAssignment()
        {
#ifndef NSCAP_FEATURE_INLINE_STARTASSIGNMENT
          return NSCAP_REINTERPRET_CAST(T**, begin_assignment());
#else
          if ( mRawPtr )
            NSCAP_RELEASE(mRawPtr);
          mRawPtr = 0;
          return NSCAP_REINTERPRET_CAST(T**, &mRawPtr);
#endif
        }
  };

#ifdef NSCAP_FEATURE_DEBUG_PTR_TYPES
template <class T>
void
nsCOMPtr<T>::assign_with_AddRef( nsISupports* rawPtr )
	{
    if ( rawPtr )
    	NSCAP_ADDREF(rawPtr);
    if ( mRawPtr )
      NSCAP_RELEASE(mRawPtr);
    mRawPtr = NS_REINTERPRET_CAST(T*, rawPtr);
	}

template <class T>
void
nsCOMPtr<T>::assign_with_QueryInterface( nsISupports* rawPtr, const nsIID& iid, nsresult* result )
  {
    nsresult status = NS_ERROR_NULL_POINTER;
    T* newPtr;
    if ( !rawPtr || !NS_SUCCEEDED( status = rawPtr->QueryInterface(iid, NSCAP_REINTERPRET_CAST(void**, &newPtr)) ) )
      newPtr = 0;

    if ( mRawPtr )
      NSCAP_RELEASE(mRawPtr);

    mRawPtr = newPtr;

    if ( result )
      *result = status;
  }

template <class T>
void
nsCOMPtr<T>::assign_with_QueryReferent( nsIWeakReference* rawPtr, const nsIID& iid, nsresult* result )
	{
    nsresult status = NS_ERROR_NULL_POINTER;
    T* newPtr;
    if ( !rawPtr || !NS_SUCCEEDED( status = rawPtr->QueryReferent(iid, NSCAP_REINTERPRET_CAST(void**, &newPtr)) ) )
      newPtr = 0;

    if ( mRawPtr )
      NSCAP_RELEASE(mRawPtr);

    mRawPtr = newPtr;

    if ( result )
      *result = status;
	}

template <class T>
void**
nsCOMPtr<T>::begin_assignment()
  {
    if ( mRawPtr )
    	{
	      NSCAP_RELEASE(mRawPtr);
	    	mRawPtr = 0;
    	}

    return NSCAP_REINTERPRET_CAST(void**, &mRawPtr);
  }
#endif


template <class T>
class nsGetterAddRefs
    /*
      ...

      This class is designed to be used for anonymous temporary objects in the
      argument list of calls that return COM interface pointers, e.g.,

        nsCOMPtr<IFoo> fooP;
        ...->QueryInterface(iid, getter_AddRefs(fooP))

      DO NOT USE THIS TYPE DIRECTLY IN YOUR CODE.  Use |getter_AddRefs()| instead.

      When initialized with a |nsCOMPtr|, as in the example above, it returns
      a |void**| (or |T**| if needed) that the outer call (|QueryInterface| in this
      case) can fill in.

			This type should be a nested class inside |nsCOMPtr<T>|.
    */
  {
    public:
      explicit
      nsGetterAddRefs( nsCOMPtr<T>& aSmartPtr )
          : mTargetSmartPtr(aSmartPtr)
        {
          // nothing else to do
        }

      operator void**()
        {
          return NSCAP_REINTERPRET_CAST(void**, mTargetSmartPtr.StartAssignment());
        }

      T*&
      operator*()
        {
          return *(mTargetSmartPtr.StartAssignment());
        }

      operator T**()
        {
          return mTargetSmartPtr.StartAssignment();
        }

    private:
      nsCOMPtr<T>& mTargetSmartPtr;
  };

template <class T>
inline
nsGetterAddRefs<T>
getter_AddRefs( nsCOMPtr<T>& aSmartPtr )
    /*
      Used around a |nsCOMPtr| when 
      ...makes the class |nsGetterAddRefs<T>| invisible.
    */
  {
    return nsGetterAddRefs<T>(aSmartPtr);
  }


class NSCAP_Zero;

template <class T, class U>
inline
NSCAP_BOOL
operator==( const nsCOMPtr<T>& lhs, const nsCOMPtr<U>& rhs )
  {
    return NSCAP_STATIC_CAST(const void*, lhs.get()) == NSCAP_STATIC_CAST(const void*, rhs.get());
  }

template <class T, class U>
inline
NSCAP_BOOL
operator==( const nsCOMPtr<T>& lhs, const U* rhs )
  {
    return NSCAP_STATIC_CAST(const void*, lhs.get()) == NSCAP_STATIC_CAST(const void*, rhs);
  }

template <class T, class U>
inline
NSCAP_BOOL
operator==( const U* lhs, const nsCOMPtr<T>& rhs )
  {
    return NSCAP_STATIC_CAST(const void*, lhs) == NSCAP_STATIC_CAST(const void*, rhs.get());
  }

template <class T>
inline
NSCAP_BOOL
operator==( const nsCOMPtr<T>& lhs, NSCAP_Zero* rhs )
    // specifically to allow |smartPtr == 0|
  {
    return NSCAP_STATIC_CAST(const void*, lhs.get()) == NSCAP_REINTERPRET_CAST(const void*, rhs);
  }

template <class T>
inline
NSCAP_BOOL
operator==( NSCAP_Zero* lhs, const nsCOMPtr<T>& rhs )
    // specifically to allow |0 == smartPtr|
  {
    return NSCAP_REINTERPRET_CAST(const void*, lhs) == NSCAP_STATIC_CAST(const void*, rhs.get());
  }

template <class T, class U>
inline
NSCAP_BOOL
operator!=( const nsCOMPtr<T>& lhs, const nsCOMPtr<U>& rhs )
  {
    return NSCAP_STATIC_CAST(const void*, lhs.get()) != NSCAP_STATIC_CAST(const void*, rhs.get());
  }

template <class T, class U>
inline
NSCAP_BOOL
operator!=( const nsCOMPtr<T>& lhs, const U* rhs )
  {
    return NSCAP_STATIC_CAST(const void*, lhs.get()) != NSCAP_STATIC_CAST(const void*, rhs);
  }

template <class T, class U>
inline
NSCAP_BOOL
operator!=( const U* lhs, const nsCOMPtr<T>& rhs )
  {
    return NSCAP_STATIC_CAST(const void*, lhs) != NSCAP_STATIC_CAST(const void*, rhs.get());
  }

template <class T>
inline
NSCAP_BOOL
operator!=( const nsCOMPtr<T>& lhs, NSCAP_Zero* rhs )
    // specifically to allow |smartPtr != 0|
  {
    return NSCAP_STATIC_CAST(const void*, lhs.get()) != NSCAP_REINTERPRET_CAST(const void*, rhs);
  }

template <class T>
inline
NSCAP_BOOL
operator!=( NSCAP_Zero* lhs, const nsCOMPtr<T>& rhs )
    // specifically to allow |0 != smartPtr|
  {
    return NSCAP_REINTERPRET_CAST(const void*, lhs) != NSCAP_STATIC_CAST(const void*, rhs.get());
  }

inline
NSCAP_BOOL
SameCOMIdentity( nsISupports* lhs, nsISupports* rhs )
  {
    return nsCOMPtr<nsISupports>( do_QueryInterface(lhs) ) == nsCOMPtr<nsISupports>( do_QueryInterface(rhs) );
  }


template <class DestinationType>
inline
nsresult
CallQueryInterface( nsISupports* aSource, nsCOMPtr<DestinationType>* aDestination )
    // a type-safe shortcut for calling the |QueryInterface()| member function
  {
    return CallQueryInterface(aSource, getter_AddRefs(*aDestination));
    	// this calls the _other_ |CallQueryInterface|
  }

#endif // !defined(nsCOMPtr_h___)
