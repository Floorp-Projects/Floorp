/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Scott Collins <scc@ScottCollins.net>
 */

#ifndef nsCOMPtr_h___
#define nsCOMPtr_h___

/*
  Having problems?
  
  See the User Manual at:
    <http://www.mozilla.org/projects/xpcom/nsCOMPtr.html>


  nsCOMPtr
    better than a raw pointer
  for owning objects
                       -- scc
*/


  // Wrapping includes can speed up compiles (see "Large Scale C++ Software Design")
#ifndef nsDebug_h___
#include "nsDebug.h"
  // for |NS_PRECONDITION|
#endif

#ifndef nsISupports_h___
#include "nsISupports.h"
  // for |nsresult|, |NS_ADDREF|, |NS_GET_IID| et al
#endif

#ifndef nscore_h__
#include "nscore.h"
  // for |NS_..._CAST|, |NS_EXPORT|
#endif


/*
  WARNING:
    This file defines several macros for internal use only.  These macros begin with the
    prefix |NSCAP_|.  Do not use these macros in your own code.  They are for internal use
    only for cross-platform compatibility, and are subject to change without notice.
*/


#ifdef _MSC_VER
  #define NSCAP_FEATURE_INLINE_STARTASSIGNMENT
    // under VC++, we win by inlining StartAssignment

    // Also under VC++, at the highest warning level, we are overwhelmed  with warnings
    //  about (unused) inline functions being removed.  This is to be expected with
    //  templates, so we disable the warning.
  #pragma warning( disable: 4514 )
#endif

#define NSCAP_FEATURE_FACTOR_DESTRUCTOR

#ifdef NS_DEBUG
  #define NSCAP_FEATURE_TEST_DONTQUERY_CASES
  #define NSCAP_FEATURE_DEBUG_PTR_TYPES
//#define NSCAP_FEATURE_TEST_NONNULL_QUERY_SUCCEEDS
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
        already_AddRefed < nsCOMPtr
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
#ifdef HAVE_ACCESS_CHANGING_CPP_USING
      using T::AddRef;
      using T::Release;
#else
      NS_IMETHOD_(nsrefcnt) AddRef(void);
      NS_IMETHOD_(nsrefcnt) Release(void);
#endif

      void operator delete( void*, size_t );                  // NOT TO BE IMPLEMENTED
        // declaring |operator delete| private makes calling delete on an interface pointer a compile error

      nsDerivedSafe<T>& operator=( const T& );                // NOT TO BE IMPLEMENTED
        // you may not call |operator=()| through a dereferenced |nsCOMPtr|, because you'd get the wrong one

        /*
          Compiler warnings and errors: nsDerivedSafe operator=() hides inherited operator=().
          If you see that, that means somebody checked in a [XP]COM interface class that declares an
          |operator=()|, and that's _bad_.  So bad, in fact, that this declaration exists explicitly
          to stop people from doing it.
        */
  };

#if !defined(HAVE_ACCESS_CHANGING_CPP_USING) && defined(NEED_CPP_UNUSED_IMPLEMENTATIONS)
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
struct already_AddRefed
    /*
      ...cooperates with |nsCOMPtr| to allow you to assign in a pointer _without_
      |AddRef|ing it.  You might want to use this as a return type from a function
      that produces an already |AddRef|ed pointer as a result.

      See also |getter_AddRefs()|, |dont_AddRef()|, and |class nsGetterAddRefs|.

      This type should be a nested class inside |nsCOMPtr<T>|.

      Yes, |already_AddRefed| could have been implemented as an |nsCOMPtr_helper| to
      avoid adding specialized machinery to |nsCOMPtr| ... but this is the simplest
      case, and perhaps worth the savings in time and space that its specific
      implementation affords over the more general solution offered by
      |nsCOMPtr_helper|.
    */
  {
    already_AddRefed( T* aRawPtr )
        : mRawPtr(aRawPtr)
      {
        // nothing else to do here
      }

    operator T*() const
      {
        return mRawPtr;
      }

    T* mRawPtr;
  };

template <class T>
inline
const already_AddRefed<T>
getter_AddRefs( T* aRawPtr )
    /*
      ...makes typing easier, because it deduces the template type, e.g., 
      you write |dont_AddRef(fooP)| instead of |already_AddRefed<IFoo>(fooP)|.
    */
  {
    return already_AddRefed<T>(aRawPtr);
  }

template <class T>
inline
const already_AddRefed<T>
getter_AddRefs( const already_AddRefed<T>& aAlreadyAddRefedPtr )
  {
    return aAlreadyAddRefedPtr;
  }

template <class T>
inline
const already_AddRefed<T>
dont_AddRef( T* aRawPtr )
  {
    return already_AddRefed<T>(aRawPtr);
  }

template <class T>
inline
const already_AddRefed<T>
dont_AddRef( const already_AddRefed<T> aAlreadyAddRefedPtr )
  {
    return aAlreadyAddRefedPtr;
  }




  /*
    There used to be machinery to allow |dont_QueryInterface()| to work, but
     since it is now equivalent to using a raw pointer ... all that machinery
     has gone away.  For pointer arguments, the following definition should
     optimize away.  This is better than using a |#define| because it is
     scoped.
  */

template <class T>
inline
T*
dont_QueryInterface( T* expr )
  {
    return expr;
  }



class nsCOMPtr_helper
    /*
      An |nsCOMPtr_helper| transforms commonly called getters into typesafe forms
      that are more convenient to call, and more efficient to use with |nsCOMPtr|s.
      Good candidates for helpers are |QueryInterface()|, |CreateInstance()|, etc.

      Here are the rules for a helper:
        - it implements |operator()| to produce an interface pointer
        - (except for its name) |operator()| is a valid [XP]COM `getter'
        - the interface pointer that it returns is already |AddRef()|ed (as from any good getter)
        - it matches the type requested with the supplied |nsIID| argument
        - its constructor provides an optional |nsresult*| that |operator()| can fill
          in with an error when it is executed
          
      See |class nsQueryInterface| for an example.
    */
  {
    public:
      virtual nsresult operator()( const nsIID&, void** ) const = 0;
  };

class NS_EXPORT nsQueryInterface : public nsCOMPtr_helper
  {
    public:
      nsQueryInterface( nsISupports* aRawPtr, nsresult* error )
          : mRawPtr(aRawPtr),
            mErrorPtr(error)
        {
          // nothing else to do here
        }

      virtual nsresult operator()( const nsIID& aIID, void** ) const;

    private:
      nsISupports*  mRawPtr;
      nsresult*     mErrorPtr;
  };

inline
const nsQueryInterface
do_QueryInterface( nsISupports* aRawPtr, nsresult* error = 0 )
  {
    return nsQueryInterface(aRawPtr, error);
  }

template <class T>
inline
void
do_QueryInterface( already_AddRefed<T>& )
  {
    // This signature exists soley to _stop_ you from doing the bad thing.
    //  Saying |do_QueryInterface()| on a pointer that is not otherwise owned by
    //  someone else is an automatic leak.  See <http://bugzilla.mozilla.org/show_bug.cgi?id=8221>.
  }

template <class T>
inline
void
do_QueryInterface( already_AddRefed<T>&, nsresult* )
  {
    // This signature exists soley to _stop_ you from doing the bad thing.
    //  Saying |do_QueryInterface()| on a pointer that is not otherwise owned by
    //  someone else is an automatic leak.  See <http://bugzilla.mozilla.org/show_bug.cgi?id=8221>.
  }



  /**
   * |null_nsCOMPtr| is deprecated.  Please use the value |0| instead.
   *  |#define|s are bad, because they aren't scoped.  But I can't replace
   *  this definition with an inline, because only a compile-time |0| gets
   *  magically converted to arbitrary pointer types.  This doesn't automatically
   *  happen for just any |const int| with the value |0|.
   *
   * Ergo: we really want to eliminate all uses of |null_nsCOMPtr()| in favor of
   *  |0|.
   */
#define null_nsCOMPtr() (0)



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
      NS_EXPORT void    assign_from_helper( const nsCOMPtr_helper&, const nsIID& );
      NS_EXPORT void**  begin_assignment();

    protected:
      nsISupports* mRawPtr;

      void
      assign_assuming_AddRef( nsISupports* newPtr )
        {
            /*
              |AddRef()|ing the new value (before entering this function) before
              |Release()|ing the old lets us safely ignore the self-assignment case.
              We must, however, be careful only to |Release()| _after_ doing the
              assignment, in case the |Release()| leads to our _own_ destruction,
              which would, in turn, cause an incorrect second |Release()| of our old
              pointer.  Thank <waterson@netscape.com> for discovering this.
            */
          nsISupports* oldPtr = mRawPtr;
          mRawPtr = newPtr;
          if ( oldPtr )
            NSCAP_RELEASE(oldPtr);
        }
  };

// template <class T> class nsGetterAddRefs;

template <class T>
class nsCOMPtr
#ifndef NSCAP_FEATURE_DEBUG_PTR_TYPES
    : private nsCOMPtr_base
#endif
  {
    enum { _force_even_compliant_compilers_to_fail_ = sizeof(T) };
      /*
        The declaration above exists specifically to make |nsCOMPtr<T>| _not_ compile with only
        a forward declaration of |T|.  This should prevent Windows and Mac engineers from
        breaking Solaris and other compilers that naturally have this behavior.  Thank
        <law@netscape.com> for inventing this specific trick.

        Of course, if you're using |nsCOMPtr| outside the scope of wanting to compile on
        Solaris and old GCC, you probably want to remove the enum so you can exploit forward
        declarations.
      */

#ifdef NSCAP_FEATURE_DEBUG_PTR_TYPES
    private:
      void    assign_with_AddRef( nsISupports* );
      void    assign_from_helper( const nsCOMPtr_helper&, const nsIID& );
      void**  begin_assignment();

      void
      assign_assuming_AddRef( T* newPtr )
        {
          T* oldPtr = mRawPtr;
          mRawPtr = newPtr;
          if ( oldPtr )
            NSCAP_RELEASE(oldPtr);
        }

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

      #define NSCAP_ASSERT_NO_QUERY_NEEDED();    Assert_NoQueryNeeded();
#else
      #define NSCAP_ASSERT_NO_QUERY_NEEDED();
#endif


        // Constructors

      nsCOMPtr()
            : NSCAP_CTOR_BASE(0)
          // default constructor
        {
          // nothing else to do here
        }

      nsCOMPtr( const nsCOMPtr<T>& aSmartPtr )
            : NSCAP_CTOR_BASE(aSmartPtr.mRawPtr)
          // copy-constructor
        {
          if ( mRawPtr )
            NSCAP_ADDREF(mRawPtr);
        }

      nsCOMPtr( T* aRawPtr )
            : NSCAP_CTOR_BASE(aRawPtr)
          // construct from a raw pointer (of the right type)
        {
          if ( mRawPtr )
            NSCAP_ADDREF(mRawPtr);

          NSCAP_ASSERT_NO_QUERY_NEEDED();
        }

      nsCOMPtr( const already_AddRefed<T>& aSmartPtr )
            : NSCAP_CTOR_BASE(aSmartPtr.mRawPtr)
          // construct from |dont_AddRef(expr)|
        {
          NSCAP_ASSERT_NO_QUERY_NEEDED();
        }

      nsCOMPtr( const nsCOMPtr_helper& helper )
            : NSCAP_CTOR_BASE(0)
          // ...and finally, anything else we might need to construct from
          //  can exploit the |nsCOMPtr_helper| facility
        {
          assign_from_helper(helper, NS_GET_IID(T));
          NSCAP_ASSERT_NO_QUERY_NEEDED();
        }

#ifdef NSCAP_FEATURE_TEST_DONTQUERY_CASES
        // For debug only --- this particular helper doesn't need to do the
        //  |NSCAP_ASSERT_NO_QUERY_NEEDED()| test.
      nsCOMPtr( const nsQueryInterface& helper )
            : NSCAP_CTOR_BASE(0)
        {
          assign_from_helper(helper, NS_GET_IID(T));
        }
#endif


        // Assignment operators

      nsCOMPtr<T>&
      operator=( const nsCOMPtr<T>& rhs )
          // copy assignment operator
        {
          assign_with_AddRef(rhs.mRawPtr);
          return *this;
        }

      nsCOMPtr<T>&
      operator=( T* rhs )
          // assign from a raw pointer (of the right type)
        {
          assign_with_AddRef(rhs);
          NSCAP_ASSERT_NO_QUERY_NEEDED();
          return *this;
        }

      nsCOMPtr<T>&
      operator=( const already_AddRefed<T>& rhs )
          // assign from |dont_AddRef(expr)|
        {
          assign_assuming_AddRef(rhs.mRawPtr);
          NSCAP_ASSERT_NO_QUERY_NEEDED();
          return *this;
        }

      nsCOMPtr<T>&
      operator=( const nsCOMPtr_helper& rhs )
          // ...and finally, anything else we might need to assign from
          //  can exploit the |nsCOMPtr_helper| facility.
        {
          assign_from_helper(rhs, NS_GET_IID(T));
          NSCAP_ASSERT_NO_QUERY_NEEDED();
          return *this;
        }

#ifdef NSCAP_FEATURE_TEST_DONTQUERY_CASES
        // For debug only --- this particular helper doesn't need to do the
        //  |NSCAP_ASSERT_NO_QUERY_NEEDED()| test.
      nsCOMPtr<T>&
      operator=( const nsQueryInterface& rhs )
        {
          assign_from_helper(rhs, NS_GET_IID(T));
          return *this;
        }
#endif


        // Other pointer operators

      nsDerivedSafe<T>*
      get() const
          /*
            Prefer the implicit conversion provided automatically by |operator nsDerivedSafe<T>*() const|.
             Use |get()| _only_ to resolve ambiguity.

            Returns a |nsDerivedSafe<T>*| to deny clients the use of |AddRef| and |Release|.
          */
        {
          return NS_REINTERPRET_CAST(nsDerivedSafe<T>*, mRawPtr);
        }

      operator nsDerivedSafe<T>*() const
          /*
            ...makes an |nsCOMPtr| act like its underlying raw pointer type (except against |AddRef()|, |Release()|,
              and |delete|) whenever it is used in a context where a raw pointer is expected.  It is this operator
              that makes an |nsCOMPtr| substitutable for a raw pointer.

            Prefer the implicit use of this operator to calling |get()|, except where necessary to resolve ambiguity.
          */
        {
          return get();
        }

      nsDerivedSafe<T>*
      operator->() const
        {
          NS_PRECONDITION(mRawPtr != 0, "You can't dereference a NULL nsCOMPtr with operator->().");
          return get();
        }

      nsDerivedSafe<T>&
      operator*() const
        {
          NS_PRECONDITION(mRawPtr != 0, "You can't dereference a NULL nsCOMPtr with operator*().");
          return *get();
        }

#if 0
    private:
      friend class nsGetterAddRefs<T>;
#endif

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



  /*
    Specializing |nsCOMPtr| for |nsISupports| allows us to use |nsCOMPtr<nsISupports>| the
    same way people use |nsISupports*| and |void*|, i.e., as a `catch-all' pointer pointing
    to any valid [XP]COM interface.  Otherwise, an |nsCOMPtr<nsISupports>| would only be able
    to point to the single [XP]COM-correct |nsISupports| instance within an object; extra
    querying ensues.  Clients need to be able to pass around arbitrary interface pointers,
    without hassles, through intermediary code that doesn't know the exact type.
  */

NS_SPECIALIZE_TEMPLATE
class nsCOMPtr<nsISupports>
    : private nsCOMPtr_base
  {
    public:
      typedef nsISupports element_type;

#ifndef NSCAP_FEATURE_FACTOR_DESTRUCTOR
     ~nsCOMPtr()
         {
           if ( mRawPtr )
             NSCAP_RELEASE(mRawPtr);
         }
#endif


        // Constructors

      nsCOMPtr()
            : nsCOMPtr_base(0)
          // default constructor
        {
          // nothing else to do here
        }

      nsCOMPtr( const nsCOMPtr<nsISupports>& aSmartPtr )
            : nsCOMPtr_base(aSmartPtr.mRawPtr)
          // copy constructor
        {
          if ( mRawPtr )
            NSCAP_ADDREF(mRawPtr);
        }

      nsCOMPtr( nsISupports* aRawPtr )
            : nsCOMPtr_base(aRawPtr)
          // construct from a raw pointer (of the right type)
        {
          if ( mRawPtr )
            NSCAP_ADDREF(mRawPtr);
        }

      nsCOMPtr( const already_AddRefed<nsISupports>& aSmartPtr )
            : nsCOMPtr_base(aSmartPtr.mRawPtr)
          // construct from |dont_AddRef(expr)|
        {
          // nothing else to do here
        }

      nsCOMPtr( const nsCOMPtr_helper& helper )
            : nsCOMPtr_base(0)
          // ...and finally, anything else we might need to construct from
          //  can exploit the |nsCOMPtr_helper| facility
        {
          assign_from_helper(helper, NS_GET_IID(nsISupports));
        }


        // Assignment operators

      nsCOMPtr<nsISupports>&
      operator=( const nsCOMPtr<nsISupports>& rhs )
          // copy assignment operator
        {
          assign_with_AddRef(rhs.mRawPtr);
          return *this;
        }

      nsCOMPtr<nsISupports>&
      operator=( nsISupports* rhs )
          // assign from a raw pointer (of the right type)
        {
          assign_with_AddRef(rhs);
          return *this;
        }

      nsCOMPtr<nsISupports>&
      operator=( const already_AddRefed<nsISupports>& rhs )
          // assign from |dont_AddRef(expr)|
        {
          assign_assuming_AddRef(rhs.mRawPtr);
          return *this;
        }

      nsCOMPtr<nsISupports>&
      operator=( const nsCOMPtr_helper& rhs )
          // ...and finally, anything else we might need to assign from
          //  can exploit the |nsCOMPtr_helper| facility.
        {
          assign_from_helper(rhs, NS_GET_IID(nsISupports));
          return *this;
        }


        // Other pointer operators

      nsDerivedSafe<nsISupports>*
      get() const
          /*
            Prefer the implicit conversion provided automatically by |operator nsDerivedSafe<nsISupports>*() const|.
             Use |get()| _only_ to resolve ambiguity.

            Returns a |nsDerivedSafe<nsISupports>*| to deny clients the use of |AddRef| and |Release|.
          */
        {
          return NS_REINTERPRET_CAST(nsDerivedSafe<nsISupports>*, mRawPtr);
        }

      operator nsDerivedSafe<nsISupports>*() const
          /*
            ...makes an |nsCOMPtr| act like its underlying raw pointer type (except against |AddRef()|, |Release()|,
              and |delete|) whenever it is used in a context where a raw pointer is expected.  It is this operator
              that makes an |nsCOMPtr| substitutable for a raw pointer.

            Prefer the implicit use of this operator to calling |get()|, except where necessary to resolve ambiguity.
          */
        {
          return get();
        }

      nsDerivedSafe<nsISupports>*
      operator->() const
        {
          NS_PRECONDITION(mRawPtr != 0, "You can't dereference a NULL nsCOMPtr with operator->().");
          return get();
        }

      nsDerivedSafe<nsISupports>&
      operator*() const
        {
          NS_PRECONDITION(mRawPtr != 0, "You can't dereference a NULL nsCOMPtr with operator*().");
          return *get();
        }

#if 0
    private:
      friend class nsGetterAddRefs<nsISupports>;
#endif

      nsISupports**
      StartAssignment()
        {
#ifndef NSCAP_FEATURE_INLINE_STARTASSIGNMENT
          return NS_REINTERPRET_CAST(nsISupports**, begin_assignment());
#else
          assign_assuming_AddRef(0);
          return NS_REINTERPRET_CAST(nsISupports**, &mRawPtr);
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
    assign_assuming_AddRef(NS_REINTERPRET_CAST(T*, rawPtr));
  }

template <class T>
void
nsCOMPtr<T>::assign_from_helper( const nsCOMPtr_helper& helper, const nsIID& aIID )
  {
    T* newRawPtr;
    if ( !NS_SUCCEEDED( helper(aIID, NS_REINTERPRET_CAST(void**, &newRawPtr)) ) )
      newRawPtr = 0;
    assign_assuming_AddRef(newRawPtr);
  }

template <class T>
void**
nsCOMPtr<T>::begin_assignment()
  {
    assign_assuming_AddRef(0);
    return NS_REINTERPRET_CAST(void**, &mRawPtr);
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
      a |void**|, a |T**|, or an |nsISupports**| as needed, that the outer call (|QueryInterface| in this
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

#ifdef NSCAP_FEATURE_TEST_DONTQUERY_CASES
     ~nsGetterAddRefs()
         {
           mTargetSmartPtr.Assert_NoQueryNeeded();
         }
#endif

      operator void**()
        {
          return NS_REINTERPRET_CAST(void**, mTargetSmartPtr.StartAssignment());
        }

      operator nsISupports**()
        {
          return NS_REINTERPRET_CAST(nsISupports**, mTargetSmartPtr.StartAssignment());
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
      nsCOMPtr<T>& mTargetSmartPtr;
  };


NS_SPECIALIZE_TEMPLATE
class nsGetterAddRefs<nsISupports>
  {
    public:
      explicit
      nsGetterAddRefs( nsCOMPtr<nsISupports>& aSmartPtr )
          : mTargetSmartPtr(aSmartPtr)
        {
          // nothing else to do
        }

      operator void**()
        {
          return NS_REINTERPRET_CAST(void**, mTargetSmartPtr.StartAssignment());
        }

      operator nsISupports**()
        {
          return mTargetSmartPtr.StartAssignment();
        }

      nsISupports*&
      operator*()
        {
          return *(mTargetSmartPtr.StartAssignment());
        }

    private:
      nsCOMPtr<nsISupports>& mTargetSmartPtr;
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



  // Comparing two |nsCOMPtr|s

template <class T, class U>
inline
NSCAP_BOOL
operator==( const nsCOMPtr<T>& lhs, const nsCOMPtr<U>& rhs )
  {
    return NS_STATIC_CAST(const void*, lhs.get()) == NS_STATIC_CAST(const void*, rhs.get());
  }

template <class T, class U>
inline
NSCAP_BOOL
operator!=( const nsCOMPtr<T>& lhs, const nsCOMPtr<U>& rhs )
  {
    return NS_STATIC_CAST(const void*, lhs.get()) != NS_STATIC_CAST(const void*, rhs.get());
  }



#ifndef NSCAP_NSCOMPTR_TO_RAW_COMPARISONS_ARE_AMBIGUOUS

  // Comparing an |nsCOMPtr| to a raw pointer

template <class T, class U>
inline
NSCAP_BOOL
operator==( const nsCOMPtr<T>& lhs, const U* rhs )
  {
    return NS_STATIC_CAST(const void*, lhs.get()) == NS_STATIC_CAST(const void*, rhs);
  }

template <class T, class U>
inline
NSCAP_BOOL
operator==( const U* lhs, const nsCOMPtr<T>& rhs )
  {
    return NS_STATIC_CAST(const void*, lhs) == NS_STATIC_CAST(const void*, rhs.get());
  }

template <class T, class U>
inline
NSCAP_BOOL
operator!=( const nsCOMPtr<T>& lhs, const U* rhs )
  {
    return NS_STATIC_CAST(const void*, lhs.get()) != NS_STATIC_CAST(const void*, rhs);
  }

template <class T, class U>
inline
NSCAP_BOOL
operator!=( const U* lhs, const nsCOMPtr<T>& rhs )
  {
    return NS_STATIC_CAST(const void*, lhs) != NS_STATIC_CAST(const void*, rhs.get());
  }
#endif // !defined(NSCAP_NSCOMPTR_TO_RAW_COMPARISONS_ARE_AMBIGUOUS)



  // Comparing an |nsCOMPtr| to |0|

class NSCAP_Zero;

template <class T>
inline
NSCAP_BOOL
operator==( const nsCOMPtr<T>& lhs, NSCAP_Zero* rhs )
    // specifically to allow |smartPtr == 0|
  {
    return NS_STATIC_CAST(const void*, lhs.get()) == NS_REINTERPRET_CAST(const void*, rhs);
  }

template <class T>
inline
NSCAP_BOOL
operator==( NSCAP_Zero* lhs, const nsCOMPtr<T>& rhs )
    // specifically to allow |0 == smartPtr|
  {
    return NS_REINTERPRET_CAST(const void*, lhs) == NS_STATIC_CAST(const void*, rhs.get());
  }

template <class T>
inline
NSCAP_BOOL
operator!=( const nsCOMPtr<T>& lhs, NSCAP_Zero* rhs )
    // specifically to allow |smartPtr != 0|
  {
    return NS_STATIC_CAST(const void*, lhs.get()) != NS_REINTERPRET_CAST(const void*, rhs);
  }

template <class T>
inline
NSCAP_BOOL
operator!=( NSCAP_Zero* lhs, const nsCOMPtr<T>& rhs )
    // specifically to allow |0 != smartPtr|
  {
    return NS_REINTERPRET_CAST(const void*, lhs) != NS_STATIC_CAST(const void*, rhs.get());
  }



  // Comparing any two [XP]COM objects for identity

inline
NSCAP_BOOL
SameCOMIdentity( nsISupports* lhs, nsISupports* rhs )
  {
    return nsCOMPtr<nsISupports>( do_QueryInterface(lhs) ) == nsCOMPtr<nsISupports>( do_QueryInterface(rhs) );
  }



template <class SourceType, class DestinationType>
inline
nsresult
CallQueryInterface( nsCOMPtr<SourceType>& aSourcePtr, DestinationType** aDestPtr )
  {
    return CallQueryInterface(aSourcePtr.get(), aDestPtr);
  }

#endif // !defined(nsCOMPtr_h___)
