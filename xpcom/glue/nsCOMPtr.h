/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCOMPtr_h___
#define nsCOMPtr_h___

/*
  Having problems?
  
  See the User Manual at:
    http://www.mozilla.org/projects/xpcom/nsCOMPtr.html


  nsCOMPtr
    better than a raw pointer
  for owning objects
                       -- scc
*/

#include "mozilla/Attributes.h"
#include "mozilla/TypeTraits.h"
#include "mozilla/Assertions.h"
#include "mozilla/NullPtr.h"
#include "mozilla/Move.h"

  // Wrapping includes can speed up compiles (see "Large Scale C++ Software Design")
#ifndef nsDebug_h___
#include "nsDebug.h"
  // for |NS_ABORT_IF_FALSE|, |NS_ASSERTION|
#endif

#ifndef nsISupportsUtils_h__
#include "nsISupportsUtils.h"
  // for |nsresult|, |NS_ADDREF|, |NS_GET_TEMPLATE_IID| et al
#endif

#ifndef nscore_h___
#include "nscore.h"
  // for |NS_COM_GLUE|
#endif

#include "nsCycleCollectionNoteChild.h"


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

#define NSCAP_FEATURE_USE_BASE

#ifdef DEBUG
  #define NSCAP_FEATURE_TEST_DONTQUERY_CASES
  #undef NSCAP_FEATURE_USE_BASE
//#define NSCAP_FEATURE_TEST_NONNULL_QUERY_SUCCEEDS
#endif

#ifdef __GNUC__
  // Our use of nsCOMPtr_base::mRawPtr violates the C++ standard's aliasing
  // rules. Mark it with the may_alias attribute so that gcc 3.3 and higher
  // don't reorder instructions based on aliasing assumptions for
  // this variable.  Fortunately, gcc versions < 3.3 do not do any
  // optimizations that break nsCOMPtr.

  #define NS_MAY_ALIAS_PTR(t)    t*  __attribute__((__may_alias__))
#else
  #define NS_MAY_ALIAS_PTR(t)    t*
#endif

#if defined(NSCAP_DISABLE_DEBUG_PTR_TYPES)
  #define NSCAP_FEATURE_USE_BASE
#endif

  /*
    The following three macros (|NSCAP_ADDREF|, |NSCAP_RELEASE|, and |NSCAP_LOG_ASSIGNMENT|)
      allow external clients the ability to add logging or other interesting debug facilities.
      In fact, if you want |nsCOMPtr| to participate in the standard logging facility, you
      provide (e.g., in "nsISupportsImpl.h") suitable definitions

        #define NSCAP_ADDREF(this, ptr)         NS_ADDREF(ptr)
        #define NSCAP_RELEASE(this, ptr)        NS_RELEASE(ptr)
  */

#ifndef NSCAP_ADDREF
  #define NSCAP_ADDREF(this, ptr)     (ptr)->AddRef()
#endif

#ifndef NSCAP_RELEASE
  #define NSCAP_RELEASE(this, ptr)    (ptr)->Release()
#endif

  // Clients can define |NSCAP_LOG_ASSIGNMENT| to perform logging.
#ifdef NSCAP_LOG_ASSIGNMENT
    // Remember that |NSCAP_LOG_ASSIGNMENT| was defined by some client so that we know
    //  to instantiate |~nsGetterAddRefs| in turn to note the external assignment into
    //  the |nsCOMPtr|.
  #define NSCAP_LOG_EXTERNAL_ASSIGNMENT
#else
    // ...otherwise, just strip it out of the code
  #define NSCAP_LOG_ASSIGNMENT(this, ptr)
#endif

#ifndef NSCAP_LOG_RELEASE
  #define NSCAP_LOG_RELEASE(this, ptr)
#endif

namespace mozilla {

struct unused_t;

} // namespace mozilla

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
    /*
     * Prohibit all one-argument overloads but already_AddRefed(T*) and
     * already_AddRefed(decltype(nullptr)), and funnel the nullptr case through
     * the T* constructor.
     */
    template<typename N>
    already_AddRefed(N,
                     typename mozilla::EnableIf<mozilla::IsNullPointer<N>::value,
                                                int>::Type dummy = 0)
      : mRawPtr(nullptr)
    {
      // nothing else to do here
    }

    already_AddRefed( T* aRawPtr )
      : mRawPtr(aRawPtr)
    {
      // nothing else to do here
    }

    // Disallowed.  Use move semantics instead.
    already_AddRefed(const already_AddRefed<T>& aOther) MOZ_DELETE;

    already_AddRefed(already_AddRefed<T>&& aOther)
      : mRawPtr(aOther.take())
    {
      // nothing else to do here
    }

    ~already_AddRefed()
    {
      MOZ_ASSERT(!mRawPtr);
    }

    // Specialize the unused operator<< for already_AddRefed, to allow
    // nsCOMPtr<nsIFoo> foo;
    // unused << foo.forget();
    friend void operator<<(const mozilla::unused_t& unused,
                           const already_AddRefed<T>& rhs)
    {
      auto mutableAlreadyAddRefed = const_cast<already_AddRefed<T>*>(&rhs);
      unused << mutableAlreadyAddRefed->take();
    }

    MOZ_WARN_UNUSED_RESULT T* take()
    {
      T* rawPtr = mRawPtr;
      mRawPtr = nullptr;
      return rawPtr;
    }

    /**
     * This helper is useful in cases like
     *
     *  already_AddRefed<BaseClass>
     *  Foo()
     *  {
     *    nsRefPtr<SubClass> x = ...;
     *    return x.forget();
     *  }
     *
     * The autoconversion allows one to omit the idiom
     *
     *    nsRefPtr<BaseClass> y = x.forget();
     *    return y.forget();
     */
    template<class U>
    operator already_AddRefed<U>()
    {
      U* tmp = mRawPtr;
      mRawPtr = nullptr;
      return already_AddRefed<U>(tmp);
    }

    /**
     * This helper provides a static_cast replacement for already_AddRefed, so
     * if you have
     *
     *   already_AddRefed<Parent> F();
     *
     * you can write
     *
     *   already_AddRefed<Child>
     *   G()
     *   {
     *     return F().downcast<Child>();
     *   }
     *
     * instead of
     *
     *     return dont_AddRef(static_cast<Child*>(F().get()));
     */
    template<class U>
    already_AddRefed<U> downcast()
    {
      U* tmp = static_cast<U*>(mRawPtr);
      mRawPtr = nullptr;
      return already_AddRefed<U>(tmp);
    }

  private:
    T* mRawPtr;
  };

template <class T>
inline
already_AddRefed<T>
dont_AddRef( T* aRawPtr )
  {
    return already_AddRefed<T>(aRawPtr);
  }

template <class T>
inline
already_AddRefed<T>&&
dont_AddRef( already_AddRefed<T>&& aAlreadyAddRefedPtr )
  {
    return mozilla::Move(aAlreadyAddRefedPtr);
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
          
      See |class nsGetInterface| for an example.
    */
  {
    public:
      virtual nsresult NS_FASTCALL operator()( const nsIID&, void** ) const = 0;
  };

/*
  |nsQueryInterface| could have been implemented as an |nsCOMPtr_helper| to
  avoid adding specialized machinery in |nsCOMPtr|, But |do_QueryInterface|
  is called often enough that the codesize savings are big enough to
  warrant the specialcasing.
*/

class
  NS_COM_GLUE
  MOZ_STACK_CLASS
nsQueryInterface MOZ_FINAL
  {
    public:
      explicit
      nsQueryInterface( nsISupports* aRawPtr )
          : mRawPtr(aRawPtr)
        {
          // nothing else to do here
        }

      nsresult NS_FASTCALL operator()( const nsIID& aIID, void** ) const;

    private:
      nsISupports*  mRawPtr;
  };

class NS_COM_GLUE nsQueryInterfaceWithError
  {
    public:
      nsQueryInterfaceWithError( nsISupports* aRawPtr, nsresult* error )
          : mRawPtr(aRawPtr),
            mErrorPtr(error)
        {
          // nothing else to do here
        }

      nsresult NS_FASTCALL operator()( const nsIID& aIID, void** ) const;

    private:
      nsISupports*  mRawPtr;
      nsresult*     mErrorPtr;
  };

inline
nsQueryInterface
do_QueryInterface( nsISupports* aRawPtr )
  {
    return nsQueryInterface(aRawPtr);
  }

inline
nsQueryInterfaceWithError
do_QueryInterface( nsISupports* aRawPtr, nsresult* error )
  {
    return nsQueryInterfaceWithError(aRawPtr, error);
  }

template <class T>
inline
void
do_QueryInterface( already_AddRefed<T>& )
  {
    // This signature exists solely to _stop_ you from doing the bad thing.
    //  Saying |do_QueryInterface()| on a pointer that is not otherwise owned by
    //  someone else is an automatic leak.  See <http://bugzilla.mozilla.org/show_bug.cgi?id=8221>.
  }

template <class T>
inline
void
do_QueryInterface( already_AddRefed<T>&, nsresult* )
  {
    // This signature exists solely to _stop_ you from doing the bad thing.
    //  Saying |do_QueryInterface()| on a pointer that is not otherwise owned by
    //  someone else is an automatic leak.  See <http://bugzilla.mozilla.org/show_bug.cgi?id=8221>.
  }


////////////////////////////////////////////////////////////////////////////
// Using servicemanager with COMPtrs
class NS_COM_GLUE nsGetServiceByCID
{
 public:
    explicit nsGetServiceByCID(const nsCID& aCID)
        : mCID(aCID)
        {
            // nothing else to do
        }
    
    nsresult NS_FASTCALL operator()( const nsIID&, void** ) const;
    
 private:
    const nsCID&                mCID;
};

class NS_COM_GLUE nsGetServiceByCIDWithError
{
 public:
    nsGetServiceByCIDWithError( const nsCID& aCID, nsresult* aErrorPtr )
        : mCID(aCID),
          mErrorPtr(aErrorPtr)
        {
            // nothing else to do
        }
    
    nsresult NS_FASTCALL operator()( const nsIID&, void** ) const;
    
 private:
    const nsCID&                mCID;
    nsresult*                   mErrorPtr;
};

class NS_COM_GLUE nsGetServiceByContractID
{
 public:
    explicit nsGetServiceByContractID(const char* aContractID)
        : mContractID(aContractID)
        {
            // nothing else to do
        }
    
    nsresult NS_FASTCALL operator()( const nsIID&, void** ) const;
    
 private:
    const char*                 mContractID;
};

class NS_COM_GLUE nsGetServiceByContractIDWithError
{
 public:
    nsGetServiceByContractIDWithError(const char* aContractID, nsresult* aErrorPtr)
        : mContractID(aContractID),
          mErrorPtr(aErrorPtr)
        {
            // nothing else to do
        }
    
    nsresult NS_FASTCALL operator()( const nsIID&, void** ) const;
    
 private:
    const char*                 mContractID;
    nsresult*                   mErrorPtr;
};

class
nsCOMPtr_base
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

      NS_COM_GLUE NS_CONSTRUCTOR_FASTCALL ~nsCOMPtr_base()
        {
          NSCAP_LOG_RELEASE(this, mRawPtr);
            if ( mRawPtr )
              NSCAP_RELEASE(this, mRawPtr);
        }

      NS_COM_GLUE void NS_FASTCALL   assign_with_AddRef( nsISupports* );
      NS_COM_GLUE void NS_FASTCALL   assign_from_qi( const nsQueryInterface, const nsIID& );
      NS_COM_GLUE void NS_FASTCALL   assign_from_qi_with_error( const nsQueryInterfaceWithError&, const nsIID& );
      NS_COM_GLUE void NS_FASTCALL   assign_from_gs_cid( const nsGetServiceByCID, const nsIID& );
      NS_COM_GLUE void NS_FASTCALL   assign_from_gs_cid_with_error( const nsGetServiceByCIDWithError&, const nsIID& );
      NS_COM_GLUE void NS_FASTCALL   assign_from_gs_contractid( const nsGetServiceByContractID, const nsIID& );
      NS_COM_GLUE void NS_FASTCALL   assign_from_gs_contractid_with_error( const nsGetServiceByContractIDWithError&, const nsIID& );
      NS_COM_GLUE void NS_FASTCALL   assign_from_helper( const nsCOMPtr_helper&, const nsIID& );
      NS_COM_GLUE void** NS_FASTCALL begin_assignment();

    protected:
      NS_MAY_ALIAS_PTR(nsISupports) mRawPtr;

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
          NSCAP_LOG_ASSIGNMENT(this, newPtr);
          NSCAP_LOG_RELEASE(this, oldPtr);
          if ( oldPtr )
            NSCAP_RELEASE(this, oldPtr);
        }
  };

// template <class T> class nsGetterAddRefs;

template <class T>
class nsCOMPtr MOZ_FINAL
#ifdef NSCAP_FEATURE_USE_BASE
    : private nsCOMPtr_base
#endif
  {

#ifdef NSCAP_FEATURE_USE_BASE
  #define NSCAP_CTOR_BASE(x) nsCOMPtr_base(x)
#else
  #define NSCAP_CTOR_BASE(x) mRawPtr(x)

    private:
      void    assign_with_AddRef( nsISupports* );
      void    assign_from_qi( const nsQueryInterface, const nsIID& );
      void    assign_from_qi_with_error( const nsQueryInterfaceWithError&, const nsIID& );
      void    assign_from_gs_cid( const nsGetServiceByCID, const nsIID& );
      void    assign_from_gs_cid_with_error( const nsGetServiceByCIDWithError&, const nsIID& );
      void    assign_from_gs_contractid( const nsGetServiceByContractID, const nsIID& );
      void    assign_from_gs_contractid_with_error( const nsGetServiceByContractIDWithError&, const nsIID& );
      void    assign_from_helper( const nsCOMPtr_helper&, const nsIID& );
      void**  begin_assignment();

      void
      assign_assuming_AddRef( T* newPtr )
        {
          T* oldPtr = mRawPtr;
          mRawPtr = newPtr;
          NSCAP_LOG_ASSIGNMENT(this, newPtr);
          NSCAP_LOG_RELEASE(this, oldPtr);
          if ( oldPtr )
            NSCAP_RELEASE(this, oldPtr);
        }

    private:
      T* mRawPtr;
#endif

    public:
      typedef T element_type;
      
#ifndef NSCAP_FEATURE_USE_BASE
     ~nsCOMPtr()
        {
          NSCAP_LOG_RELEASE(this, mRawPtr);
          if ( mRawPtr )
            NSCAP_RELEASE(this, mRawPtr);
        }
#endif

#ifdef NSCAP_FEATURE_TEST_DONTQUERY_CASES
      void
      Assert_NoQueryNeeded()
        {
          if ( mRawPtr )
            {
              nsCOMPtr<T> query_result( do_QueryInterface(mRawPtr) );
              NS_ASSERTION(query_result.get() == mRawPtr, "QueryInterface needed");
            }
        }

  #define NSCAP_ASSERT_NO_QUERY_NEEDED() Assert_NoQueryNeeded();
#else
  #define NSCAP_ASSERT_NO_QUERY_NEEDED()
#endif


        // Constructors

      nsCOMPtr()
            : NSCAP_CTOR_BASE(0)
          // default constructor
        {
          NSCAP_LOG_ASSIGNMENT(this, 0);
        }

      nsCOMPtr( const nsCOMPtr<T>& aSmartPtr )
            : NSCAP_CTOR_BASE(aSmartPtr.mRawPtr)
          // copy-constructor
        {
          if ( mRawPtr )
            NSCAP_ADDREF(this, mRawPtr);
          NSCAP_LOG_ASSIGNMENT(this, aSmartPtr.mRawPtr);
        }

      nsCOMPtr( T* aRawPtr )
            : NSCAP_CTOR_BASE(aRawPtr)
          // construct from a raw pointer (of the right type)
        {
          if ( mRawPtr )
            NSCAP_ADDREF(this, mRawPtr);
          NSCAP_LOG_ASSIGNMENT(this, aRawPtr);
          NSCAP_ASSERT_NO_QUERY_NEEDED();
        }

      nsCOMPtr( already_AddRefed<T>& aSmartPtr )
            : NSCAP_CTOR_BASE(aSmartPtr.take())
          // construct from |already_AddRefed|
        {
          NSCAP_LOG_ASSIGNMENT(this, mRawPtr);
          NSCAP_ASSERT_NO_QUERY_NEEDED();
        }

      nsCOMPtr( already_AddRefed<T>&& aSmartPtr )
            : NSCAP_CTOR_BASE(aSmartPtr.take())
          // construct from |otherComPtr.forget()|
        {
          NSCAP_LOG_ASSIGNMENT(this, mRawPtr);
          NSCAP_ASSERT_NO_QUERY_NEEDED();
        }

      template<typename U>
      nsCOMPtr( already_AddRefed<U>& aSmartPtr )
            : NSCAP_CTOR_BASE(static_cast<T*>(aSmartPtr.take()))
          // construct from |already_AddRefed|
        {
          // But make sure that U actually inherits from T
          static_assert(mozilla::IsBaseOf<T, U>::value,
                        "U is not a subclass of T");
          NSCAP_LOG_ASSIGNMENT(this, static_cast<T*>(mRawPtr));
          NSCAP_ASSERT_NO_QUERY_NEEDED();
        }

      template<typename U>
      nsCOMPtr( already_AddRefed<U>&& aSmartPtr )
            : NSCAP_CTOR_BASE(static_cast<T*>(aSmartPtr.take()))
          // construct from |otherComPtr.forget()|
        {
          // But make sure that U actually inherits from T
          static_assert(mozilla::IsBaseOf<T, U>::value,
                        "U is not a subclass of T");
          NSCAP_LOG_ASSIGNMENT(this, static_cast<T*>(mRawPtr));
          NSCAP_ASSERT_NO_QUERY_NEEDED();
        }

      nsCOMPtr( const nsQueryInterface qi )
            : NSCAP_CTOR_BASE(0)
          // construct from |do_QueryInterface(expr)|
        {
          NSCAP_LOG_ASSIGNMENT(this, 0);
          assign_from_qi(qi, NS_GET_TEMPLATE_IID(T));
        }

      nsCOMPtr( const nsQueryInterfaceWithError& qi )
            : NSCAP_CTOR_BASE(0)
          // construct from |do_QueryInterface(expr, &rv)|
        {
          NSCAP_LOG_ASSIGNMENT(this, 0);
          assign_from_qi_with_error(qi, NS_GET_TEMPLATE_IID(T));
        }

      nsCOMPtr( const nsGetServiceByCID gs )
            : NSCAP_CTOR_BASE(0)
          // construct from |do_GetService(cid_expr)|
        {
          NSCAP_LOG_ASSIGNMENT(this, 0);
          assign_from_gs_cid(gs, NS_GET_TEMPLATE_IID(T));
        }

      nsCOMPtr( const nsGetServiceByCIDWithError& gs )
            : NSCAP_CTOR_BASE(0)
          // construct from |do_GetService(cid_expr, &rv)|
        {
          NSCAP_LOG_ASSIGNMENT(this, 0);
          assign_from_gs_cid_with_error(gs, NS_GET_TEMPLATE_IID(T));
        }

      nsCOMPtr( const nsGetServiceByContractID gs )
            : NSCAP_CTOR_BASE(0)
          // construct from |do_GetService(contractid_expr)|
        {
          NSCAP_LOG_ASSIGNMENT(this, 0);
          assign_from_gs_contractid(gs, NS_GET_TEMPLATE_IID(T));
        }

      nsCOMPtr( const nsGetServiceByContractIDWithError& gs )
            : NSCAP_CTOR_BASE(0)
          // construct from |do_GetService(contractid_expr, &rv)|
        {
          NSCAP_LOG_ASSIGNMENT(this, 0);
          assign_from_gs_contractid_with_error(gs, NS_GET_TEMPLATE_IID(T));
        }

      nsCOMPtr( const nsCOMPtr_helper& helper )
            : NSCAP_CTOR_BASE(0)
          // ...and finally, anything else we might need to construct from
          //  can exploit the |nsCOMPtr_helper| facility
        {
          NSCAP_LOG_ASSIGNMENT(this, 0);
          assign_from_helper(helper, NS_GET_TEMPLATE_IID(T));
          NSCAP_ASSERT_NO_QUERY_NEEDED();
        }


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

      template<typename U>
      nsCOMPtr<T>&
      operator=( already_AddRefed<U>& rhs )
          // assign from |already_AddRefed|
        {
          // Make sure that U actually inherits from T
          static_assert(mozilla::IsBaseOf<T, U>::value,
                        "U is not a subclass of T");
          assign_assuming_AddRef(static_cast<T*>(rhs.take()));
          NSCAP_ASSERT_NO_QUERY_NEEDED();
          return *this;
        }

      template<typename U>
      nsCOMPtr<T>&
      operator=( already_AddRefed<U>&& rhs )
          // assign from |otherComPtr.forget()|
        {
          // Make sure that U actually inherits from T
          static_assert(mozilla::IsBaseOf<T, U>::value,
                        "U is not a subclass of T");
          assign_assuming_AddRef(static_cast<T*>(rhs.take()));
          NSCAP_ASSERT_NO_QUERY_NEEDED();
          return *this;
        }

      nsCOMPtr<T>&
      operator=( const nsQueryInterface rhs )
          // assign from |do_QueryInterface(expr)|
        {
          assign_from_qi(rhs, NS_GET_TEMPLATE_IID(T));
          return *this;
        }

      nsCOMPtr<T>&
      operator=( const nsQueryInterfaceWithError& rhs )
          // assign from |do_QueryInterface(expr, &rv)|
        {
          assign_from_qi_with_error(rhs, NS_GET_TEMPLATE_IID(T));
          return *this;
        }

      nsCOMPtr<T>&
      operator=( const nsGetServiceByCID rhs )
          // assign from |do_GetService(cid_expr)|
        {
          assign_from_gs_cid(rhs, NS_GET_TEMPLATE_IID(T));
          return *this;
        }

      nsCOMPtr<T>&
      operator=( const nsGetServiceByCIDWithError& rhs )
          // assign from |do_GetService(cid_expr, &rv)|
        {
          assign_from_gs_cid_with_error(rhs, NS_GET_TEMPLATE_IID(T));
          return *this;
        }

      nsCOMPtr<T>&
      operator=( const nsGetServiceByContractID rhs )
          // assign from |do_GetService(contractid_expr)|
        {
          assign_from_gs_contractid(rhs, NS_GET_TEMPLATE_IID(T));
          return *this;
        }

      nsCOMPtr<T>&
      operator=( const nsGetServiceByContractIDWithError& rhs )
          // assign from |do_GetService(contractid_expr, &rv)|
        {
          assign_from_gs_contractid_with_error(rhs, NS_GET_TEMPLATE_IID(T));
          return *this;
        }

      nsCOMPtr<T>&
      operator=( const nsCOMPtr_helper& rhs )
          // ...and finally, anything else we might need to assign from
          //  can exploit the |nsCOMPtr_helper| facility.
        {
          assign_from_helper(rhs, NS_GET_TEMPLATE_IID(T));
          NSCAP_ASSERT_NO_QUERY_NEEDED();
          return *this;
        }

      void
      swap( nsCOMPtr<T>& rhs )
          // ...exchange ownership with |rhs|; can save a pair of refcount operations
        {
#ifdef NSCAP_FEATURE_USE_BASE
          nsISupports* temp = rhs.mRawPtr;
#else
          T* temp = rhs.mRawPtr;
#endif
          NSCAP_LOG_ASSIGNMENT(&rhs, mRawPtr);
          NSCAP_LOG_ASSIGNMENT(this, temp);
          NSCAP_LOG_RELEASE(this, mRawPtr);
          NSCAP_LOG_RELEASE(&rhs, temp);
          rhs.mRawPtr = mRawPtr;
          mRawPtr = temp;
          // |rhs| maintains the same invariants, so we don't need to |NSCAP_ASSERT_NO_QUERY_NEEDED|
        }

      void
      swap( T*& rhs )
          // ...exchange ownership with |rhs|; can save a pair of refcount operations
        {
#ifdef NSCAP_FEATURE_USE_BASE
          nsISupports* temp = rhs;
#else
          T* temp = rhs;
#endif
          NSCAP_LOG_ASSIGNMENT(this, temp);
          NSCAP_LOG_RELEASE(this, mRawPtr);
          rhs = reinterpret_cast<T*>(mRawPtr);
          mRawPtr = temp;
          NSCAP_ASSERT_NO_QUERY_NEEDED();
        }


        // Other pointer operators

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
      forget( I** rhs )
          // Set the target of rhs to the value of mRawPtr and null out mRawPtr.
          // Useful to avoid unnecessary AddRef/Release pairs with "out"
          // parameters where rhs bay be a T** or an I** where I is a base class
          // of T.
        {
          NS_ASSERTION(rhs, "Null pointer passed to forget!");
          NSCAP_LOG_RELEASE(this, mRawPtr);
          *rhs = get();
          mRawPtr = 0;
        }

      T*
      get() const
          /*
            Prefer the implicit conversion provided automatically by |operator T*() const|.
            Use |get()| to resolve ambiguity or to get a castable pointer.
          */
        {
          return reinterpret_cast<T*>(mRawPtr);
        }

      operator T*() const
          /*
            ...makes an |nsCOMPtr| act like its underlying raw pointer type whenever it
            is used in a context where a raw pointer is expected.  It is this operator
            that makes an |nsCOMPtr| substitutable for a raw pointer.

            Prefer the implicit use of this operator to calling |get()|, except where
            necessary to resolve ambiguity.
          */
        {
          return get();
        }

      T*
      operator->() const
        {
          NS_ABORT_IF_FALSE(mRawPtr != 0, "You can't dereference a NULL nsCOMPtr with operator->().");
          return get();
        }

      nsCOMPtr<T>*
      get_address()
          // This is not intended to be used by clients.  See |address_of|
          // below.
        {
          return this;
        }

      const nsCOMPtr<T>*
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
          NS_ABORT_IF_FALSE(mRawPtr != 0, "You can't dereference a NULL nsCOMPtr with operator*().");
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



  /*
    Specializing |nsCOMPtr| for |nsISupports| allows us to use |nsCOMPtr<nsISupports>| the
    same way people use |nsISupports*| and |void*|, i.e., as a `catch-all' pointer pointing
    to any valid [XP]COM interface.  Otherwise, an |nsCOMPtr<nsISupports>| would only be able
    to point to the single [XP]COM-correct |nsISupports| instance within an object; extra
    querying ensues.  Clients need to be able to pass around arbitrary interface pointers,
    without hassles, through intermediary code that doesn't know the exact type.
  */

template <>
class nsCOMPtr<nsISupports>
    : private nsCOMPtr_base
  {
    public:
      typedef nsISupports element_type;

        // Constructors

      nsCOMPtr()
            : nsCOMPtr_base(0)
          // default constructor
        {
          NSCAP_LOG_ASSIGNMENT(this, 0);
        }

      nsCOMPtr( const nsCOMPtr<nsISupports>& aSmartPtr )
            : nsCOMPtr_base(aSmartPtr.mRawPtr)
          // copy constructor
        {
          if ( mRawPtr )
            NSCAP_ADDREF(this, mRawPtr);
          NSCAP_LOG_ASSIGNMENT(this, aSmartPtr.mRawPtr);
        }

      nsCOMPtr( nsISupports* aRawPtr )
            : nsCOMPtr_base(aRawPtr)
          // construct from a raw pointer (of the right type)
        {
          if ( mRawPtr )
            NSCAP_ADDREF(this, mRawPtr);
          NSCAP_LOG_ASSIGNMENT(this, aRawPtr);
        }

      nsCOMPtr( already_AddRefed<nsISupports>& aSmartPtr )
            : nsCOMPtr_base(aSmartPtr.take())
          // construct from |already_AddRefed|
        {
          NSCAP_LOG_ASSIGNMENT(this, mRawPtr);
        }

      nsCOMPtr( already_AddRefed<nsISupports>&& aSmartPtr )
            : nsCOMPtr_base(aSmartPtr.take())
          // construct from |otherComPtr.forget()|
        {
          NSCAP_LOG_ASSIGNMENT(this, mRawPtr);
        }

      nsCOMPtr( const nsQueryInterface qi )
            : nsCOMPtr_base(0)
          // assign from |do_QueryInterface(expr)|
        {
          NSCAP_LOG_ASSIGNMENT(this, 0);
          assign_from_qi(qi, NS_GET_IID(nsISupports));
        }

      nsCOMPtr( const nsQueryInterfaceWithError& qi )
            : nsCOMPtr_base(0)
          // assign from |do_QueryInterface(expr, &rv)|
        {
          NSCAP_LOG_ASSIGNMENT(this, 0);
          assign_from_qi_with_error(qi, NS_GET_IID(nsISupports));
        }

      nsCOMPtr( const nsGetServiceByCID gs )
            : nsCOMPtr_base(0)
          // assign from |do_GetService(cid_expr)|
        {
          NSCAP_LOG_ASSIGNMENT(this, 0);
          assign_from_gs_cid(gs, NS_GET_IID(nsISupports));
        }

      nsCOMPtr( const nsGetServiceByCIDWithError& gs )
            : nsCOMPtr_base(0)
          // assign from |do_GetService(cid_expr, &rv)|
        {
          NSCAP_LOG_ASSIGNMENT(this, 0);
          assign_from_gs_cid_with_error(gs, NS_GET_IID(nsISupports));
        }

      nsCOMPtr( const nsGetServiceByContractID gs )
            : nsCOMPtr_base(0)
          // assign from |do_GetService(contractid_expr)|
        {
          NSCAP_LOG_ASSIGNMENT(this, 0);
          assign_from_gs_contractid(gs, NS_GET_IID(nsISupports));
        }

      nsCOMPtr( const nsGetServiceByContractIDWithError& gs )
            : nsCOMPtr_base(0)
          // assign from |do_GetService(contractid_expr, &rv)|
        {
          NSCAP_LOG_ASSIGNMENT(this, 0);
          assign_from_gs_contractid_with_error(gs, NS_GET_IID(nsISupports));
        }

      nsCOMPtr( const nsCOMPtr_helper& helper )
            : nsCOMPtr_base(0)
          // ...and finally, anything else we might need to construct from
          //  can exploit the |nsCOMPtr_helper| facility
        {
          NSCAP_LOG_ASSIGNMENT(this, 0);
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
      operator=( already_AddRefed<nsISupports>& rhs )
          // assign from |already_AddRefed|
        {
          assign_assuming_AddRef(rhs.take());
          return *this;
        }

      nsCOMPtr<nsISupports>&
      operator=( already_AddRefed<nsISupports>&& rhs )
          // assign from |otherComPtr.forget()|
        {
          assign_assuming_AddRef(rhs.take());
          return *this;
        }

      nsCOMPtr<nsISupports>&
      operator=( const nsQueryInterface rhs )
          // assign from |do_QueryInterface(expr)|
        {
          assign_from_qi(rhs, NS_GET_IID(nsISupports));
          return *this;
        }

      nsCOMPtr<nsISupports>&
      operator=( const nsQueryInterfaceWithError& rhs )
          // assign from |do_QueryInterface(expr, &rv)|
        {
          assign_from_qi_with_error(rhs, NS_GET_IID(nsISupports));
          return *this;
        }

      nsCOMPtr<nsISupports>&
      operator=( const nsGetServiceByCID rhs )
          // assign from |do_GetService(cid_expr)|
        {
          assign_from_gs_cid(rhs, NS_GET_IID(nsISupports));
          return *this;
        }

      nsCOMPtr<nsISupports>&
      operator=( const nsGetServiceByCIDWithError& rhs )
          // assign from |do_GetService(cid_expr, &rv)|
        {
          assign_from_gs_cid_with_error(rhs, NS_GET_IID(nsISupports));
          return *this;
        }

      nsCOMPtr<nsISupports>&
      operator=( const nsGetServiceByContractID rhs )
          // assign from |do_GetService(contractid_expr)|
        {
          assign_from_gs_contractid(rhs, NS_GET_IID(nsISupports));
          return *this;
        }

      nsCOMPtr<nsISupports>&
      operator=( const nsGetServiceByContractIDWithError& rhs )
          // assign from |do_GetService(contractid_expr, &rv)|
        {
          assign_from_gs_contractid_with_error(rhs, NS_GET_IID(nsISupports));
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

      void
      swap( nsCOMPtr<nsISupports>& rhs )
          // ...exchange ownership with |rhs|; can save a pair of refcount operations
        {
          nsISupports* temp = rhs.mRawPtr;
          NSCAP_LOG_ASSIGNMENT(&rhs, mRawPtr);
          NSCAP_LOG_ASSIGNMENT(this, temp);
          NSCAP_LOG_RELEASE(this, mRawPtr);
          NSCAP_LOG_RELEASE(&rhs, temp);
          rhs.mRawPtr = mRawPtr;
          mRawPtr = temp;
        }

      void
      swap( nsISupports*& rhs )
          // ...exchange ownership with |rhs|; can save a pair of refcount operations
        {
          nsISupports* temp = rhs;
          NSCAP_LOG_ASSIGNMENT(this, temp);
          NSCAP_LOG_RELEASE(this, mRawPtr);
          rhs = mRawPtr;
          mRawPtr = temp;
        }

      already_AddRefed<nsISupports>
      forget()
          // return the value of mRawPtr and null out mRawPtr. Useful for
          // already_AddRefed return values.
        {
          nsISupports* temp = 0;
          swap(temp);
          return already_AddRefed<nsISupports>(temp);
        }

      void
      forget( nsISupports** rhs )
          // Set the target of rhs to the value of mRawPtr and null out mRawPtr.
          // Useful to avoid unnecessary AddRef/Release pairs with "out"
          // parameters.
        {
          NS_ASSERTION(rhs, "Null pointer passed to forget!");
          *rhs = 0;
          swap(*rhs);
        }

        // Other pointer operators

      nsISupports*
      get() const
          /*
            Prefer the implicit conversion provided automatically by
            |operator nsISupports*() const|.
            Use |get()| to resolve ambiguity or to get a castable pointer.
          */
        {
          return reinterpret_cast<nsISupports*>(mRawPtr);
        }

      operator nsISupports*() const
          /*
            ...makes an |nsCOMPtr| act like its underlying raw pointer type whenever it
            is used in a context where a raw pointer is expected.  It is this operator
            that makes an |nsCOMPtr| substitutable for a raw pointer.

            Prefer the implicit use of this operator to calling |get()|, except where
            necessary to resolve ambiguity.
          */
        {
          return get();
        }

      nsISupports*
      operator->() const
        {
          NS_ABORT_IF_FALSE(mRawPtr != 0, "You can't dereference a NULL nsCOMPtr with operator->().");
          return get();
        }

      nsCOMPtr<nsISupports>*
      get_address()
          // This is not intended to be used by clients.  See |address_of|
          // below.
        {
          return this;
        }

      const nsCOMPtr<nsISupports>*
      get_address() const
          // This is not intended to be used by clients.  See |address_of|
          // below.
        {
          return this;
        }

    public:

      nsISupports&
      operator*() const
        {
          NS_ABORT_IF_FALSE(mRawPtr != 0, "You can't dereference a NULL nsCOMPtr with operator*().");
          return *get();
        }

      nsISupports**
      StartAssignment()
        {
#ifndef NSCAP_FEATURE_INLINE_STARTASSIGNMENT
          return reinterpret_cast<nsISupports**>(begin_assignment());
#else
          assign_assuming_AddRef(0);
          return reinterpret_cast<nsISupports**>(&mRawPtr);
#endif
        }
  };

template <typename T>
inline void
ImplCycleCollectionUnlink(nsCOMPtr<T>& aField)
{
  aField = nullptr;
}

template <typename T>
inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            nsCOMPtr<T>& aField,
                            const char* aName,
                            uint32_t aFlags = 0)
{
  CycleCollectionNoteChild(aCallback, aField.get(), aName, aFlags);
}

#ifndef NSCAP_FEATURE_USE_BASE
template <class T>
void
nsCOMPtr<T>::assign_with_AddRef( nsISupports* rawPtr )
  {
    if ( rawPtr )
      NSCAP_ADDREF(this, rawPtr);
    assign_assuming_AddRef(reinterpret_cast<T*>(rawPtr));
  }

template <class T>
void
nsCOMPtr<T>::assign_from_qi( const nsQueryInterface qi, const nsIID& aIID )
  {
    void* newRawPtr;
    if ( NS_FAILED( qi(aIID, &newRawPtr) ) )
      newRawPtr = 0;
    assign_assuming_AddRef(static_cast<T*>(newRawPtr));
  }

template <class T>
void
nsCOMPtr<T>::assign_from_qi_with_error( const nsQueryInterfaceWithError& qi, const nsIID& aIID )
  {
    void* newRawPtr;
    if ( NS_FAILED( qi(aIID, &newRawPtr) ) )
      newRawPtr = 0;
    assign_assuming_AddRef(static_cast<T*>(newRawPtr));
  }

template <class T>
void
nsCOMPtr<T>::assign_from_gs_cid( const nsGetServiceByCID gs, const nsIID& aIID )
  {
    void* newRawPtr;
    if ( NS_FAILED( gs(aIID, &newRawPtr) ) )
      newRawPtr = 0;
    assign_assuming_AddRef(static_cast<T*>(newRawPtr));
  }

template <class T>
void
nsCOMPtr<T>::assign_from_gs_cid_with_error( const nsGetServiceByCIDWithError& gs, const nsIID& aIID )
  {
    void* newRawPtr;
    if ( NS_FAILED( gs(aIID, &newRawPtr) ) )
      newRawPtr = 0;
    assign_assuming_AddRef(static_cast<T*>(newRawPtr));
  }

template <class T>
void
nsCOMPtr<T>::assign_from_gs_contractid( const nsGetServiceByContractID gs, const nsIID& aIID )
  {
    void* newRawPtr;
    if ( NS_FAILED( gs(aIID, &newRawPtr) ) )
      newRawPtr = 0;
    assign_assuming_AddRef(static_cast<T*>(newRawPtr));
  }

template <class T>
void
nsCOMPtr<T>::assign_from_gs_contractid_with_error( const nsGetServiceByContractIDWithError& gs, const nsIID& aIID )
  {
    void* newRawPtr;
    if ( NS_FAILED( gs(aIID, &newRawPtr) ) )
      newRawPtr = 0;
    assign_assuming_AddRef(static_cast<T*>(newRawPtr));
  }

template <class T>
void
nsCOMPtr<T>::assign_from_helper( const nsCOMPtr_helper& helper, const nsIID& aIID )
  {
    void* newRawPtr;
    if ( NS_FAILED( helper(aIID, &newRawPtr) ) )
      newRawPtr = 0;
    assign_assuming_AddRef(static_cast<T*>(newRawPtr));
  }

template <class T>
void**
nsCOMPtr<T>::begin_assignment()
  {
    assign_assuming_AddRef(0);
    union { T** mT; void** mVoid; } result;
    result.mT = &mRawPtr;
    return result.mVoid;
  }
#endif

template <class T>
inline
nsCOMPtr<T>*
address_of( nsCOMPtr<T>& aPtr )
  {
    return aPtr.get_address();
  }

template <class T>
inline
const nsCOMPtr<T>*
address_of( const nsCOMPtr<T>& aPtr )
  {
    return aPtr.get_address();
  }

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

#if defined(NSCAP_FEATURE_TEST_DONTQUERY_CASES) || defined(NSCAP_LOG_EXTERNAL_ASSIGNMENT)
     ~nsGetterAddRefs()
        {
#ifdef NSCAP_LOG_EXTERNAL_ASSIGNMENT
          NSCAP_LOG_ASSIGNMENT(reinterpret_cast<void *>(address_of(mTargetSmartPtr)), mTargetSmartPtr.get());
#endif

#ifdef NSCAP_FEATURE_TEST_DONTQUERY_CASES
          mTargetSmartPtr.Assert_NoQueryNeeded();
#endif
        }
#endif

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
      nsCOMPtr<T>& mTargetSmartPtr;
  };


template <>
class nsGetterAddRefs<nsISupports>
  {
    public:
      explicit
      nsGetterAddRefs( nsCOMPtr<nsISupports>& aSmartPtr )
          : mTargetSmartPtr(aSmartPtr)
        {
          // nothing else to do
        }

#ifdef NSCAP_LOG_EXTERNAL_ASSIGNMENT
     ~nsGetterAddRefs()
        {
          NSCAP_LOG_ASSIGNMENT(reinterpret_cast<void *>(address_of(mTargetSmartPtr)), mTargetSmartPtr.get());
        }
#endif

      operator void**()
        {
          return reinterpret_cast<void**>(mTargetSmartPtr.StartAssignment());
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

template <class T, class DestinationType>
inline
nsresult
CallQueryInterface( T* aSource, nsGetterAddRefs<DestinationType> aDestination )
{
    return CallQueryInterface(aSource,
                              static_cast<DestinationType**>(aDestination));
}


  // Comparing two |nsCOMPtr|s

template <class T, class U>
inline
bool
operator==( const nsCOMPtr<T>& lhs, const nsCOMPtr<U>& rhs )
  {
    return static_cast<const T*>(lhs.get()) == static_cast<const U*>(rhs.get());
  }


template <class T, class U>
inline
bool
operator!=( const nsCOMPtr<T>& lhs, const nsCOMPtr<U>& rhs )
  {
    return static_cast<const T*>(lhs.get()) != static_cast<const U*>(rhs.get());
  }


  // Comparing an |nsCOMPtr| to a raw pointer

template <class T, class U>
inline
bool
operator==( const nsCOMPtr<T>& lhs, const U* rhs )
  {
    return static_cast<const T*>(lhs.get()) == rhs;
  }

template <class T, class U>
inline
bool
operator==( const U* lhs, const nsCOMPtr<T>& rhs )
  {
    return lhs == static_cast<const T*>(rhs.get());
  }

template <class T, class U>
inline
bool
operator!=( const nsCOMPtr<T>& lhs, const U* rhs )
  {
    return static_cast<const T*>(lhs.get()) != rhs;
  }

template <class T, class U>
inline
bool
operator!=( const U* lhs, const nsCOMPtr<T>& rhs )
  {
    return lhs != static_cast<const T*>(rhs.get());
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
operator==( const nsCOMPtr<T>& lhs, U* rhs )
  {
    return static_cast<const T*>(lhs.get()) == const_cast<const U*>(rhs);
  }

template <class T, class U>
inline
bool
operator==( U* lhs, const nsCOMPtr<T>& rhs )
  {
    return const_cast<const U*>(lhs) == static_cast<const T*>(rhs.get());
  }

template <class T, class U>
inline
bool
operator!=( const nsCOMPtr<T>& lhs, U* rhs )
  {
    return static_cast<const T*>(lhs.get()) != const_cast<const U*>(rhs);
  }

template <class T, class U>
inline
bool
operator!=( U* lhs, const nsCOMPtr<T>& rhs )
  {
    return const_cast<const U*>(lhs) != static_cast<const T*>(rhs.get());
  }
#endif



  // Comparing an |nsCOMPtr| to |0|

class NSCAP_Zero;

template <class T>
inline
bool
operator==( const nsCOMPtr<T>& lhs, NSCAP_Zero* rhs )
    // specifically to allow |smartPtr == 0|
  {
    return static_cast<const void*>(lhs.get()) == reinterpret_cast<const void*>(rhs);
  }

template <class T>
inline
bool
operator==( NSCAP_Zero* lhs, const nsCOMPtr<T>& rhs )
    // specifically to allow |0 == smartPtr|
  {
    return reinterpret_cast<const void*>(lhs) == static_cast<const void*>(rhs.get());
  }

template <class T>
inline
bool
operator!=( const nsCOMPtr<T>& lhs, NSCAP_Zero* rhs )
    // specifically to allow |smartPtr != 0|
  {
    return static_cast<const void*>(lhs.get()) != reinterpret_cast<const void*>(rhs);
  }

template <class T>
inline
bool
operator!=( NSCAP_Zero* lhs, const nsCOMPtr<T>& rhs )
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
operator==( const nsCOMPtr<T>& lhs, int rhs )
    // specifically to allow |smartPtr == 0|
  {
    return static_cast<const void*>(lhs.get()) == reinterpret_cast<const void*>(rhs);
  }

template <class T>
inline
bool
operator==( int lhs, const nsCOMPtr<T>& rhs )
    // specifically to allow |0 == smartPtr|
  {
    return reinterpret_cast<const void*>(lhs) == static_cast<const void*>(rhs.get());
  }

#endif // !defined(HAVE_CPP_TROUBLE_COMPARING_TO_ZERO)

  // Comparing any two [XP]COM objects for identity

inline
bool
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
