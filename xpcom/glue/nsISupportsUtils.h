/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Scott Collins <scc@ScottCollins.net>
 */

#ifndef __nsISupportsUtils_h
#define __nsISupportsUtils_h

/***************************************************************************/
/* this section copied from the hand written nsISupports.h */

#include "nscore.h"
  // for |NS_SPECIALIZE_TEMPLATE|, and the casts, et al

#include "nsDebug.h"
#include "nsID.h"
#include "nsIID.h"
#include "nsError.h"
#include "pratom.h" /* needed for PR_AtomicIncrement and PR_AtomicDecrement */

#if defined(NS_MT_SUPPORTED)
#include "prcmon.h"
#endif  /* NS_MT_SUPPORTED */

/*@{*/

///////////////////////////////////////////////////////////////////////////////

/**
 * IID for the nsISupports interface
 * {00000000-0000-0000-c000-000000000046}
 *
 * To maintain binary compatibility with COM's nsIUnknown, we define the IID
 * of nsISupports to be the same as that of COM's nsIUnknown.
 */
#define NS_ISUPPORTS_IID      \
{ 0x00000000, 0x0000, 0x0000, \
  {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46} }

/**
 * Reference count values
 *
 * This is type of return value from Addref() and Release() in nsISupports.
 * nsIUnknown of COM returns a unsigned long from equivalent functions.
 * To maintain binary compatibility of nsISupports with nsIUnknown, we are
 * doing this ifdeffing.
 */
#if defined(XP_PC) && !defined(XP_OS2) && PR_BYTES_PER_LONG == 4
typedef unsigned long nsrefcnt;
#else
typedef PRUint32 nsrefcnt;
#endif

/**
 * NS_NO_VTABLE is emitted by xpidl in interface declarations whenever xpidl
 * can determine that the interface can't contain a constructor.  This results
 * in some space savings and possible runtime savings - see bug 49416.
 */
#if defined(_MSC_VER) && _MSC_VER >= 1100
#define NS_NO_VTABLE __declspec(novtable)
#else
#define NS_NO_VTABLE
#endif

#include "nsTraceRefcnt.h"

/**
 * Basic component object model interface. Objects which implement
 * this interface support runtime interface discovery (QueryInterface)
 * and a reference counted memory model (AddRef/Release). This is
 * modelled after the win32 IUnknown API.
 */
class NS_NO_VTABLE nsISupports {
public:

  /**
   * @name Methods
   */

  //@{
  /**
   * A run time mechanism for interface discovery.
   * @param aIID [in] A requested interface IID
   * @param aInstancePtr [out] A pointer to an interface pointer to
   * receive the result.
   * @return <b>NS_OK</b> if the interface is supported by the associated
   * instance, <b>NS_NOINTERFACE</b> if it is not.
   * <b>NS_ERROR_INVALID_POINTER</b> if <i>aInstancePtr</i> is <b>NULL</b>.
   */
  NS_IMETHOD QueryInterface(REFNSIID aIID,
                            void** aInstancePtr) = 0;
  /**
   * Increases the reference count for this interface.
   * The associated instance will not be deleted unless
   * the reference count is returned to zero.
   *
   * @return The resulting reference count.
   */
  NS_IMETHOD_(nsrefcnt) AddRef(void) = 0;

  /**
   * Decreases the reference count for this interface.
   * Generally, if the reference count returns to zero,
   * the associated instance is deleted.
   *
   * @return The resulting reference count.
   */
  NS_IMETHOD_(nsrefcnt) Release(void) = 0;

  //@}
};

/*@}*/

/***************************************************************************/

/**
 * A macro to build the static const IID accessor method
 */

#define NS_DEFINE_STATIC_IID_ACCESSOR(the_iid) \
  static const nsIID& GetIID() {static nsIID iid = the_iid; return iid;}

/**
 * A macro to build the static const CID accessor method
 */

#define NS_DEFINE_STATIC_CID_ACCESSOR(the_cid) \
  static const nsID& GetCID() {static nsID cid = the_cid; return cid;}

////////////////////////////////////////////////////////////////////////////////
// Macros to help detect thread-safety:

#if defined(NS_DEBUG) && defined(NS_MT_SUPPORTED)

extern "C" NS_EXPORT void* NS_CurrentThread(void);
extern "C" NS_EXPORT void NS_CheckThreadSafe(void* owningThread, const char* msg);

#define NS_DECL_OWNINGTHREAD            void* _mOwningThread;
#define NS_IMPL_OWNINGTHREAD()          (_mOwningThread = NS_CurrentThread())
#define NS_ASSERT_OWNINGTHREAD(_class)  NS_CheckThreadSafe(_mOwningThread, #_class " not thread-safe")

#else // !(defined(NS_DEBUG) && defined(NS_MT_SUPPORTED))

#define NS_DECL_OWNINGTHREAD            /* nothing */
#define NS_IMPL_OWNINGTHREAD()          ((void)0)
#define NS_ASSERT_OWNINGTHREAD(_class)  ((void)0)

#endif // !(defined(NS_DEBUG) && defined(NS_MT_SUPPORTED))

////////////////////////////////////////////////////////////////////////////////

/**
 * Some convenience macros for implementing AddRef and Release
 */

/**
 * Declare the reference count variable and the implementations of the
 * AddRef and QueryInterface methods.
 */

#define NS_DECL_ISUPPORTS                                                   \
public:                                                                     \
  NS_IMETHOD QueryInterface(REFNSIID aIID,                                  \
                            void** aInstancePtr);                           \
  NS_IMETHOD_(nsrefcnt) AddRef(void);                                       \
  NS_IMETHOD_(nsrefcnt) Release(void);                                      \
protected:                                                                  \
  nsrefcnt mRefCnt;                                                         \
  NS_DECL_OWNINGTHREAD                                                      \
public:

#define NS_DECL_ISUPPORTS_EXPORTED                                          \
public:                                                                     \
  NS_EXPORT NS_IMETHOD QueryInterface(REFNSIID aIID,                        \
                            void** aInstancePtr);                           \
  NS_EXPORT NS_IMETHOD_(nsrefcnt) AddRef(void);                             \
  NS_EXPORT NS_IMETHOD_(nsrefcnt) Release(void);                            \
protected:                                                                  \
  nsrefcnt mRefCnt;                                                         \
  NS_DECL_OWNINGTHREAD                                                      \
public:

///////////////////////////////////////////////////////////////////////////////

/**
 * Initialize the reference count variable. Add this to each and every
 * constructor you implement.
 */
#define NS_INIT_REFCNT() (mRefCnt = 0, NS_IMPL_OWNINGTHREAD())
#define NS_INIT_ISUPPORTS() NS_INIT_REFCNT() // what it should have been called in the first place

/**
 * Use this macro to implement the AddRef method for a given <i>_class</i>
 * @param _class The name of the class implementing the method
 */
#define NS_IMPL_ADDREF(_class)                               \
NS_IMETHODIMP_(nsrefcnt) _class::AddRef(void)                \
{                                                            \
  NS_PRECONDITION(PRInt32(mRefCnt) >= 0, "illegal refcnt");  \
  NS_ASSERT_OWNINGTHREAD(_class);                            \
  ++mRefCnt;                                                 \
  NS_LOG_ADDREF(this, mRefCnt, #_class, sizeof(*this));      \
  return mRefCnt;                                            \
}

/**
 * Use this macro to implement the Release method for a given <i>_class</i>
 * @param _class The name of the class implementing the method
 *
 * A note on the 'stabilization' of the refcnt to one. At that point,
 * the object's refcount will have gone to zero. The object's
 * destructor may trigger code that attempts to QueryInterface() and
 * Release() 'this' again. Doing so will temporarily increment and
 * decrement the refcount. (Only a logic error would make one try to
 * keep a permanent hold on 'this'.)  To prevent re-entering the
 * destructor, we make sure that no balanced refcounting can return
 * the refcount to |0|.
 */
#define NS_IMPL_RELEASE(_class)                              \
NS_IMETHODIMP_(nsrefcnt) _class::Release(void)               \
{                                                            \
  NS_PRECONDITION(0 != mRefCnt, "dup release");              \
  NS_ASSERT_OWNINGTHREAD(_class);                            \
  --mRefCnt;                                                 \
  NS_LOG_RELEASE(this, mRefCnt, #_class);                    \
  if (mRefCnt == 0) {                                        \
    mRefCnt = 1; /* stabilize */                             \
    NS_DELETEXPCOM(this);                                    \
    return 0;                                                \
  }                                                          \
  return mRefCnt;                                            \
}

///////////////////////////////////////////////////////////////////////////////

/*
 * Often you have to cast an implementation pointer, e.g., |this|, to an |nsISupports*|,
 * but because you have multiple inheritance, a simple cast is ambiguous.  One could
 * simply say, e.g., (given a base |nsIBase|), |NS_STATIC_CAST(nsIBase*, this)|; but that
 * disguises the fact that what you are really doing is disambiguating the |nsISupports|.
 * You could make that more obvious with a double cast, e.g.,
 * |NS_STATIC_CAST(nsISupports*, NS_STATIC_CAST(nsIBase*, this))|, but that is bulky and
 * harder to read...
 *
 * The following macro is clean, short, and obvious.  In the example above, you would use it
 * like this: |NS_ISUPPORTS_CAST(nsIBase*, this)|.
 */

#define NS_ISUPPORTS_CAST(__unambiguousBase, __expr)    NS_STATIC_CAST(nsISupports*, NS_STATIC_CAST(__unambiguousBase, __expr))

///////////////////////////////////////////////////////////////////////////////

#ifdef NS_DEBUG

/*
 * Adding this debug-only function as per bug #26803.  If you are debugging and
 * this function returns wrong refcounts, fix the objects |AddRef()| and |Release()|
 * to do the right thing.
 *
 * Of course, this function is only available for debug builds.
 */

inline
nsrefcnt
NS_DebugGetRefCount( nsISupports* obj )
    // Warning: don't apply this to an object whose refcount is
    //  |0| or not yet initialized ... it may be destroyed.
  {
    nsrefcnt ref_count = 0;

    if ( obj )
      {
          // |AddRef()| and |Release()| are supposed to return
          //  the new refcount of the object
        obj->AddRef();
        ref_count = obj->Release();
          // Can't use |NS_[ADDREF|RELEASE]| since (a) they _don't_ return
          //  the refcount, and (b) we don't want to log these guaranteed
          //  balanced calls.

        NS_ASSERTION(ref_count, "Oops! Calling |NS_DebugGetRefCount()| probably just destroyed this object.");
      }

     return ref_count;
  }

#endif


///////////////////////////////////////////////////////////////////////////////

/*
 * Some convenience macros for implementing QueryInterface
 */

/**
 * This implements query interface with two assumptions: First, the
 * class in question implements nsISupports and its own interface and
 * nothing else. Second, the implementation of the class's primary
 * inheritance chain leads to its own interface.
 *
 * @param _class The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
 */

#define NS_IMPL_QUERY_HEAD(_class)                                       \
NS_IMETHODIMP _class::QueryInterface(REFNSIID aIID, void** aInstancePtr) \
{                                                                        \
  NS_ASSERTION(aInstancePtr, "QueryInterface requires a non-NULL destination!"); \
  if ( !aInstancePtr )                                                   \
    return NS_ERROR_NULL_POINTER;                                        \
  nsISupports* foundInterface;

#define NS_IMPL_QUERY_BODY(_interface)                                   \
  if ( aIID.Equals(NS_GET_IID(_interface)) )                             \
    foundInterface = NS_STATIC_CAST(_interface*, this);                  \
  else

#define NS_IMPL_QUERY_BODY_AMBIGUOUS(_interface, _implClass)             \
  if ( aIID.Equals(NS_GET_IID(_interface)) )                             \
    foundInterface = NS_STATIC_CAST(_interface*, NS_STATIC_CAST(_implClass*, this)); \
  else

#define NS_IMPL_QUERY_TAIL_GUTS                                          \
    foundInterface = 0;                                                  \
  nsresult status;                                                       \
  if ( !foundInterface )                                                 \
    status = NS_NOINTERFACE;                                             \
  else                                                                   \
    {                                                                    \
      NS_ADDREF(foundInterface);                                         \
      status = NS_OK;                                                    \
    }                                                                    \
  *aInstancePtr = foundInterface;                                        \
  return status;                                                         \
}

#define NS_IMPL_QUERY_TAIL_INHERITING(_baseclass)                        \
    foundInterface = 0;                                                  \
  nsresult status;                                                       \
  if ( !foundInterface )                                                 \
    status = _baseclass::QueryInterface(aIID, (void**)&foundInterface);  \
  else                                                                   \
    {                                                                    \
      NS_ADDREF(foundInterface);                                         \
      status = NS_OK;                                                    \
    }                                                                    \
  *aInstancePtr = foundInterface;                                        \
  return status;                                                         \
}

#define NS_IMPL_QUERY_TAIL(_supports_interface)                          \
  NS_IMPL_QUERY_BODY_AMBIGUOUS(nsISupports, _supports_interface)         \
  NS_IMPL_QUERY_TAIL_GUTS


  /*
    This is the new scheme.  Using this notation now will allow us to switch to
    a table driven mechanism when it's ready.  Note the difference between this
    and the (currently) underlying NS_IMPL_QUERY_INTERFACE mechanism.  You must
    explicitly mention |nsISupports| when using the interface maps.
  */
#define NS_INTERFACE_MAP_BEGIN(_implClass)                         NS_IMPL_QUERY_HEAD(_implClass)
#define NS_INTERFACE_MAP_ENTRY(_interface)                         NS_IMPL_QUERY_BODY(_interface)
#define NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(_interface, _implClass)   NS_IMPL_QUERY_BODY_AMBIGUOUS(_interface, _implClass)
#define NS_INTERFACE_MAP_END                                       NS_IMPL_QUERY_TAIL_GUTS
#define NS_INTERFACE_MAP_END_INHERITING(_baseClass)                NS_IMPL_QUERY_TAIL_INHERITING(_baseClass)

#define NS_IMPL_QUERY_INTERFACE0(_class)                                                      \
  NS_INTERFACE_MAP_BEGIN(_class)                                                              \
    NS_INTERFACE_MAP_ENTRY(nsISupports)                                                       \
  NS_INTERFACE_MAP_END

#define NS_IMPL_QUERY_INTERFACE1(_class, _i1)                                                 \
  NS_INTERFACE_MAP_BEGIN(_class)                                                              \
    NS_INTERFACE_MAP_ENTRY(_i1)                                                               \
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, _i1)                                        \
  NS_INTERFACE_MAP_END

#define NS_IMPL_QUERY_INTERFACE2(_class, _i1, _i2)                                            \
  NS_INTERFACE_MAP_BEGIN(_class)                                                              \
    NS_INTERFACE_MAP_ENTRY(_i1)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i2)                                                               \
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, _i1)                                        \
  NS_INTERFACE_MAP_END

#define NS_IMPL_QUERY_INTERFACE3(_class, _i1, _i2, _i3)                                       \
  NS_INTERFACE_MAP_BEGIN(_class)                                                              \
    NS_INTERFACE_MAP_ENTRY(_i1)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i2)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i3)                                                               \
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, _i1)                                        \
  NS_INTERFACE_MAP_END

#define NS_IMPL_QUERY_INTERFACE4(_class, _i1, _i2, _i3, _i4)                                  \
  NS_INTERFACE_MAP_BEGIN(_class)                                                              \
    NS_INTERFACE_MAP_ENTRY(_i1)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i2)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i3)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i4)                                                               \
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, _i1)                                        \
  NS_INTERFACE_MAP_END

#define NS_IMPL_QUERY_INTERFACE5(_class, _i1, _i2, _i3, _i4, _i5)                             \
  NS_INTERFACE_MAP_BEGIN(_class)                                                              \
    NS_INTERFACE_MAP_ENTRY(_i1)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i2)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i3)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i4)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i5)                                                               \
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, _i1)                                        \
  NS_INTERFACE_MAP_END

#define NS_IMPL_QUERY_INTERFACE6(_class, _i1, _i2, _i3, _i4, _i5, _i6)                        \
  NS_INTERFACE_MAP_BEGIN(_class)                                                              \
    NS_INTERFACE_MAP_ENTRY(_i1)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i2)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i3)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i4)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i5)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i6)                                                               \
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, _i1)                                        \
  NS_INTERFACE_MAP_END

#define NS_IMPL_QUERY_INTERFACE7(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7)                   \
  NS_INTERFACE_MAP_BEGIN(_class)                                                              \
    NS_INTERFACE_MAP_ENTRY(_i1)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i2)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i3)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i4)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i5)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i6)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i7)                                                               \
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, _i1)                                        \
  NS_INTERFACE_MAP_END

#define NS_IMPL_QUERY_INTERFACE8(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7, _i8)              \
  NS_INTERFACE_MAP_BEGIN(_class)                                                              \
    NS_INTERFACE_MAP_ENTRY(_i1)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i2)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i3)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i4)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i5)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i6)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i7)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i8)                                                               \
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, _i1)                                        \
  NS_INTERFACE_MAP_END

#define NS_IMPL_QUERY_INTERFACE9(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7, _i8, _i9)         \
  NS_INTERFACE_MAP_BEGIN(_class)                                                              \
    NS_INTERFACE_MAP_ENTRY(_i1)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i2)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i3)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i4)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i5)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i6)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i7)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i8)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i9)                                                               \
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, _i1)                                        \
  NS_INTERFACE_MAP_END

#define NS_IMPL_QUERY_INTERFACE10(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7, _i8, _i9, _i10)  \
  NS_INTERFACE_MAP_BEGIN(_class)                                                              \
    NS_INTERFACE_MAP_ENTRY(_i1)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i2)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i3)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i4)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i5)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i6)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i7)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i8)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i9)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i10)                                                              \
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, _i1)                                        \
  NS_INTERFACE_MAP_END

/*
 The following macro is deprecated.  We need to switch all instances
 to |NS_IMPL_QUERY_INTERFACE1|, or |NS_IMPL_QUERY_INTERFACE0| depending
 on how they were using it.
*/

#define NS_IMPL_QUERY_INTERFACE(_class,_classiiddef)                     \
NS_IMETHODIMP _class::QueryInterface(REFNSIID aIID, void** aInstancePtr) \
{                                                                        \
  if (NULL == aInstancePtr) {                                            \
    return NS_ERROR_NULL_POINTER;                                        \
  }                                                                      \
                                                                         \
  *aInstancePtr = NULL;                                                  \
                                                                         \
  static NS_DEFINE_IID(kClassIID, _classiiddef);                         \
  if (aIID.Equals(kClassIID)) {                                          \
    *aInstancePtr = (void*) this;                                        \
    NS_ADDREF_THIS();                                                    \
    return NS_OK;                                                        \
  }                                                                      \
  if (aIID.Equals(NS_GET_IID(nsISupports))) {               \
    *aInstancePtr = (void*) ((nsISupports*)this);                        \
    NS_ADDREF_THIS();                                                    \
    return NS_OK;                                                        \
  }                                                                      \
  return NS_NOINTERFACE;                                                 \
}

/**
 * Convenience macro for implementing all nsISupports methods for
 * a simple class.
 * @param _class The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
 */

#define NS_IMPL_ISUPPORTS(_class,_classiiddef) \
  NS_IMPL_ADDREF(_class)                       \
  NS_IMPL_RELEASE(_class)                      \
  NS_IMPL_QUERY_INTERFACE(_class,_classiiddef)

#define NS_IMPL_ISUPPORTS0(_class)             \
  NS_IMPL_ADDREF(_class)                       \
  NS_IMPL_RELEASE(_class)                      \
  NS_IMPL_QUERY_INTERFACE0(_class)

#define NS_IMPL_ISUPPORTS1(_class, _interface) \
  NS_IMPL_ADDREF(_class)                       \
  NS_IMPL_RELEASE(_class)                      \
  NS_IMPL_QUERY_INTERFACE1(_class, _interface)

#define NS_IMPL_ISUPPORTS2(_class, _i1, _i2)   \
  NS_IMPL_ADDREF(_class)                       \
  NS_IMPL_RELEASE(_class)                      \
  NS_IMPL_QUERY_INTERFACE2(_class, _i1, _i2)

#define NS_IMPL_ISUPPORTS3(_class, _i1, _i2, _i3)   \
  NS_IMPL_ADDREF(_class)                            \
  NS_IMPL_RELEASE(_class)                           \
  NS_IMPL_QUERY_INTERFACE3(_class, _i1, _i2, _i3)

#define NS_IMPL_ISUPPORTS4(_class, _i1, _i2, _i3, _i4)   \
  NS_IMPL_ADDREF(_class)                                 \
  NS_IMPL_RELEASE(_class)                                \
  NS_IMPL_QUERY_INTERFACE4(_class, _i1, _i2, _i3, _i4)

#define NS_IMPL_ISUPPORTS5(_class, _i1, _i2, _i3, _i4, _i5)   \
  NS_IMPL_ADDREF(_class)                                      \
  NS_IMPL_RELEASE(_class)                                     \
  NS_IMPL_QUERY_INTERFACE5(_class, _i1, _i2, _i3, _i4, _i5)

#define NS_IMPL_ISUPPORTS6(_class, _i1, _i2, _i3, _i4, _i5, _i6)   \
  NS_IMPL_ADDREF(_class)                                      \
  NS_IMPL_RELEASE(_class)                                     \
  NS_IMPL_QUERY_INTERFACE6(_class, _i1, _i2, _i3, _i4, _i5, _i6)

#define NS_IMPL_ISUPPORTS7(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7)   \
  NS_IMPL_ADDREF(_class)                                      \
  NS_IMPL_RELEASE(_class)                                     \
  NS_IMPL_QUERY_INTERFACE7(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7)

#define NS_IMPL_ISUPPORTS8(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7, _i8)   \
  NS_IMPL_ADDREF(_class)                                      \
  NS_IMPL_RELEASE(_class)                                     \
  NS_IMPL_QUERY_INTERFACE8(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7, _i8)

#define NS_IMPL_ISUPPORTS9(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7, _i8, _i9)   \
  NS_IMPL_ADDREF(_class)                                      \
  NS_IMPL_RELEASE(_class)                                     \
  NS_IMPL_QUERY_INTERFACE9(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7, _i8, _i9)

#define NS_IMPL_ISUPPORTS10(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7, _i8, _i9, _i10)   \
  NS_IMPL_ADDREF(_class)                                      \
  NS_IMPL_RELEASE(_class)                                     \
  NS_IMPL_QUERY_INTERFACE10(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7, _i8, _i9, _i10)

////////////////////////////////////////////////////////////////////////////////

/**
 * Declare that you're going to inherit from something that already
 * implements nsISupports, but also implements an additional interface, thus
 * causing an ambiguity. In this case you don't need another mRefCnt, you
 * just need to forward the definitions to the appropriate superclass. E.g.
 *
 * class Bar : public Foo, public nsIBar {  // both provide nsISupports
 * public:
 *   NS_DECL_ISUPPORTS_INHERITED
 *   ...other nsIBar and Bar methods...
 * };
 */
#define NS_DECL_ISUPPORTS_INHERITED                                         \
public:                                                                     \
  NS_IMETHOD QueryInterface(REFNSIID aIID,                                  \
                            void** aInstancePtr);                           \
  NS_IMETHOD_(nsrefcnt) AddRef(void);                                       \
  NS_IMETHOD_(nsrefcnt) Release(void);                                      \

/**
 * These macros can be used in conjunction with NS_DECL_ISUPPORTS_INHERITED
 * to implement the nsISupports methods, forwarding the invocations to a
 * superclass that already implements nsISupports.
 *
 * Note that I didn't make these inlined because they're virtual methods.
 */

#define NS_IMPL_ADDREF_INHERITED(Class, Super)                              \
NS_IMETHODIMP_(nsrefcnt) Class::AddRef(void)                                \
{                                                                           \
  return Super::AddRef();                                                   \
}                                                                           \

#define NS_IMPL_RELEASE_INHERITED(Class, Super)                             \
NS_IMETHODIMP_(nsrefcnt) Class::Release(void)                               \
{                                                                           \
  return Super::Release();                                                  \
}                                                                           \

#define NS_IMPL_QUERY_INTERFACE_INHERITED(Class, Super, i1)                 \
  NS_IMPL_QUERY_INTERFACE_INHERITED1(Class, Super, i1)                      \

#define NS_IMPL_QUERY_INTERFACE_INHERITED0(Class, Super)                    \
  NS_IMPL_QUERY_HEAD(Class)                                       \
  NS_IMPL_QUERY_TAIL_INHERITING(Super)                                \

#define NS_IMPL_QUERY_INTERFACE_INHERITED1(Class, Super, i1)                \
  NS_IMPL_QUERY_HEAD(Class)                                       \
  NS_IMPL_QUERY_BODY(i1)                                                    \
  NS_IMPL_QUERY_TAIL_INHERITING(Super)                                \

#define NS_IMPL_QUERY_INTERFACE_INHERITED2(Class, Super, i1, i2)            \
  NS_IMPL_QUERY_HEAD(Class)                                       \
  NS_IMPL_QUERY_BODY(i1)                                                    \
  NS_IMPL_QUERY_BODY(i2)                                                    \
  NS_IMPL_QUERY_TAIL_INHERITING(Super)                                \

#define NS_IMPL_QUERY_INTERFACE_INHERITED3(Class, Super, i1, i2, i3)        \
  NS_IMPL_QUERY_HEAD(Class)                                       \
  NS_IMPL_QUERY_BODY(i1)                                                    \
  NS_IMPL_QUERY_BODY(i2)                                                    \
  NS_IMPL_QUERY_BODY(i3)                                                    \
  NS_IMPL_QUERY_TAIL_INHERITING(Super)                                \

#define NS_IMPL_ISUPPORTS_INHERITED(Class, Super, i1)                       \
  NS_IMPL_ISUPPORTS_INHERITED1(Class, Super, i1)                            \

#define NS_IMPL_ISUPPORTS_INHERITED0(Class, Super)                          \
    NS_IMPL_QUERY_INTERFACE_INHERITED0(Class, Super)                        \
    NS_IMPL_ADDREF_INHERITED(Class, Super)                                  \
    NS_IMPL_RELEASE_INHERITED(Class, Super)                                 \

#define NS_IMPL_ISUPPORTS_INHERITED1(Class, Super, i1)                      \
    NS_IMPL_QUERY_INTERFACE_INHERITED1(Class, Super, i1)                    \
    NS_IMPL_ADDREF_INHERITED(Class, Super)                                  \
    NS_IMPL_RELEASE_INHERITED(Class, Super)                                 \

#define NS_IMPL_ISUPPORTS_INHERITED2(Class, Super, i1, i2)                  \
    NS_IMPL_QUERY_INTERFACE_INHERITED2(Class, Super, i1, i2)                \
    NS_IMPL_ADDREF_INHERITED(Class, Super)                                  \
    NS_IMPL_RELEASE_INHERITED(Class, Super)                                 \

#define NS_IMPL_ISUPPORTS_INHERITED3(Class, Super, i1, i2, i3)              \
    NS_IMPL_QUERY_INTERFACE_INHERITED3(Class, Super, i1, i2, i3)            \
    NS_IMPL_ADDREF_INHERITED(Class, Super)                                  \
    NS_IMPL_RELEASE_INHERITED(Class, Super)                                 \

///////////////////////////////////////////////////////////////////////////////

/**
 *
 * Threadsafe implementations of the ISupports convenience macros
 *
 */

/**
 * IID for the nsIsThreadsafe interface
 * {88210890-47a6-11d2-bec3-00805f8a66dc}
 *
 * This interface is *only* used for debugging purposes to determine if
 * a given component is threadsafe.
 */
#define NS_ISTHREADSAFE_IID      \
{ 0x88210890, 0x47a6, 0x11d2,    \
  {0xbe, 0xc3, 0x00, 0x80, 0x5f, 0x8a, 0x66, 0xdc} }

#if defined(NS_MT_SUPPORTED)

#define NS_LOCK_INSTANCE()                                                  \
  PR_CEnterMonitor((void*)this)

#define NS_UNLOCK_INSTANCE()                                                \
  PR_CExitMonitor((void*)this)

/**
 * Use this macro to implement the AddRef method for a given <i>_class</i>
 * @param _class The name of the class implementing the method
 */

#define NS_IMPL_THREADSAFE_ADDREF(_class)                                   \
NS_IMETHODIMP_(nsrefcnt) _class::AddRef(void)                               \
{                                                                           \
  NS_PRECONDITION(PRInt32(mRefCnt) >= 0, "illegal refcnt");                 \
  nsrefcnt count;                                                           \
  count = PR_AtomicIncrement((PRInt32*)&mRefCnt);                           \
  NS_LOG_ADDREF(this, count, #_class, sizeof(*this));                       \
  return count;                                                             \
}

/**
 * Use this macro to implement the Release method for a given <i>_class</i>
 * @param _class The name of the class implementing the method
 */

#define NS_IMPL_THREADSAFE_RELEASE(_class)                                  \
nsrefcnt _class::Release(void)                                              \
{                                                                           \
  nsrefcnt count;                                                           \
  NS_PRECONDITION(0 != mRefCnt, "dup release");                             \
  count = PR_AtomicDecrement((PRInt32 *)&mRefCnt);                          \
  NS_LOG_RELEASE(this, count, #_class);                                     \
  if (0 == count) {                                                         \
    mRefCnt = 1; /* stabilize */                                            \
    /* enable this to find non-threadsafe destructors: */                   \
    /* NS_ASSERT_OWNINGTHREAD(_class); */                                   \
    NS_DELETEXPCOM(this);                                                   \
    return 0;                                                               \
  }                                                                         \
  return count;                                                             \
}

///////////////////////////////////////////////////////////////////////////////

/*
 * Some convenience macros for implementing QueryInterface
 */

/**
 * This implements query interface with two assumptions: First, the
 * class in question implements nsISupports and its own interface and
 * nothing else. Second, the implementation of the class's primary
 * inheritance chain leads to its own interface.
 *
 * @param _class The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
 */
#if defined(NS_DEBUG)
#define NS_VERIFY_THREADSAFE_INTERFACE(_iface)                              \
 if (NULL != (_iface)) {                                                    \
   nsISupports* tmp;                                                        \
   static NS_DEFINE_IID(kIsThreadsafeIID, NS_ISTHREADSAFE_IID);             \
   NS_PRECONDITION((NS_OK == _iface->QueryInterface(kIsThreadsafeIID,       \
                                                    (void**)&tmp)),         \
                   "Interface is not threadsafe");                          \
 }

#define NS_IMPL_THREADSAFE_QUERY_INTERFACE(_class,_classiiddef)          \
NS_IMETHODIMP _class::QueryInterface(REFNSIID aIID, void** aInstancePtr) \
{                                                                        \
  if (NULL == aInstancePtr) {                                            \
    return NS_ERROR_NULL_POINTER;                                        \
  }                                                                      \
                                                                         \
  *aInstancePtr = NULL;                                                  \
                                                                         \
  static NS_DEFINE_IID(kIsThreadsafeIID, NS_ISTHREADSAFE_IID);           \
  static NS_DEFINE_IID(kClassIID, _classiiddef);                         \
  if (aIID.Equals(kClassIID)) {                                          \
    *aInstancePtr = (void*) this;                                        \
    NS_ADDREF_THIS();                                                    \
    return NS_OK;                                                        \
  }                                                                      \
  if (aIID.Equals(NS_GET_IID(nsISupports))) {                            \
    *aInstancePtr = (void*) ((nsISupports*)this);                        \
    NS_ADDREF_THIS();                                                    \
    return NS_OK;                                                        \
  }                                                                      \
  if (aIID.Equals(kIsThreadsafeIID)) {                                   \
    return NS_OK;                                                        \
  }                                                                      \
  return NS_NOINTERFACE;                                                 \
}

#else   /* !NS_DEBUG */

#define NS_VERIFY_THREADSAFE_INTERFACE(_iface)
#define NS_IMPL_THREADSAFE_QUERY_INTERFACE(_class,_classiiddef)          \
 NS_IMPL_QUERY_INTERFACE(_class, _classiiddef)

#endif /* !NS_DEBUG */

/**
 * Convenience macro for implementing all nsISupports methods for
 * a simple class.
 * @param _class The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
 */

#define NS_IMPL_THREADSAFE_ISUPPORTS(_class,_classiiddef) \
  NS_IMPL_THREADSAFE_ADDREF(_class)                       \
  NS_IMPL_THREADSAFE_RELEASE(_class)                      \
  NS_IMPL_THREADSAFE_QUERY_INTERFACE(_class,_classiiddef)

#else /* !NS_MT_SUPPORTED */

#define NS_LOCK_INSTANCE()

#define NS_UNLOCK_INSTANCE()

#define NS_IMPL_THREADSAFE_ADDREF(_class)  NS_IMPL_ADDREF(_class)

#define NS_IMPL_THREADSAFE_RELEASE(_class) NS_IMPL_RELEASE(_class)

#define NS_VERIFY_THREADSAFE_INTERFACE(_iface)

#define NS_IMPL_THREADSAFE_QUERY_INTERFACE(_class,_classiiddef) \
 NS_IMPL_QUERY_INTERFACE(_class, _classiiddef)

#define NS_IMPL_THREADSAFE_ISUPPORTS(_class,_classiiddef) \
  NS_IMPL_ADDREF(_class)                                  \
  NS_IMPL_RELEASE(_class)                                 \
  NS_IMPL_QUERY_INTERFACE(_class,_classiiddef)

#endif /* !NS_MT_SUPPORTED */

////////////////////////////////////////////////////////////////////////////////

#define NS_IMPL_QUERY_TAIL_GUTS_THREADSAFE                               \
    foundInterface = 0;                                                  \
  nsresult status;                                                       \
  if ( !foundInterface )                                                 \
    {                                                                    \
      static NS_DEFINE_IID(kIsThreadsafeIID, NS_ISTHREADSAFE_IID);       \
      status = aIID.Equals(kIsThreadsafeIID) ? NS_OK : NS_NOINTERFACE;   \
    }                                                                    \
  else                                                                   \
    {                                                                    \
      NS_ADDREF(foundInterface);                                         \
      status = NS_OK;                                                    \
    }                                                                    \
  *aInstancePtr = foundInterface;                                        \
  return status;                                                         \
}

#ifdef NS_DEBUG
#define NS_INTERFACE_MAP_END_THREADSAFE                            NS_IMPL_QUERY_TAIL_GUTS_THREADSAFE
#else
#define NS_INTERFACE_MAP_END_THREADSAFE                            NS_IMPL_QUERY_TAIL_GUTS
#endif

////////////////////////////////////////////////////////////////////////////////

#define NS_IMPL_THREADSAFE_QUERY_INTERFACE0(_class)                                           \
  NS_INTERFACE_MAP_BEGIN(_class)                                                              \
    NS_INTERFACE_MAP_ENTRY(nsISupports)                                                       \
  NS_INTERFACE_MAP_END_THREADSAFE

#define NS_IMPL_THREADSAFE_QUERY_INTERFACE1(_class, _i1)                                      \
  NS_INTERFACE_MAP_BEGIN(_class)                                                              \
    NS_INTERFACE_MAP_ENTRY(_i1)                                                               \
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, _i1)                                        \
  NS_INTERFACE_MAP_END_THREADSAFE

#define NS_IMPL_THREADSAFE_QUERY_INTERFACE2(_class, _i1, _i2)                                 \
  NS_INTERFACE_MAP_BEGIN(_class)                                                              \
    NS_INTERFACE_MAP_ENTRY(_i1)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i2)                                                               \
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, _i1)                                        \
  NS_INTERFACE_MAP_END_THREADSAFE

#define NS_IMPL_THREADSAFE_QUERY_INTERFACE3(_class, _i1, _i2, _i3)                            \
  NS_INTERFACE_MAP_BEGIN(_class)                                                              \
    NS_INTERFACE_MAP_ENTRY(_i1)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i2)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i3)                                                               \
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, _i1)                                        \
  NS_INTERFACE_MAP_END_THREADSAFE

#define NS_IMPL_THREADSAFE_QUERY_INTERFACE4(_class, _i1, _i2, _i3, _i4)                       \
  NS_INTERFACE_MAP_BEGIN(_class)                                                              \
    NS_INTERFACE_MAP_ENTRY(_i1)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i2)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i3)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i4)                                                               \
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, _i1)                                        \
  NS_INTERFACE_MAP_END_THREADSAFE

#define NS_IMPL_THREADSAFE_QUERY_INTERFACE5(_class, _i1, _i2, _i3, _i4, _i5)                  \
  NS_INTERFACE_MAP_BEGIN(_class)                                                              \
    NS_INTERFACE_MAP_ENTRY(_i1)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i2)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i3)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i4)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i5)                                                               \
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, _i1)                                        \
  NS_INTERFACE_MAP_END_THREADSAFE

#define NS_IMPL_THREADSAFE_QUERY_INTERFACE6(_class, _i1, _i2, _i3, _i4, _i5, _i6)             \
  NS_INTERFACE_MAP_BEGIN(_class)                                                              \
    NS_INTERFACE_MAP_ENTRY(_i1)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i2)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i3)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i4)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i5)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i6)                                                               \
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, _i1)                                        \
  NS_INTERFACE_MAP_END_THREADSAFE

#define NS_IMPL_THREADSAFE_QUERY_INTERFACE7(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7)        \
  NS_INTERFACE_MAP_BEGIN(_class)                                                              \
    NS_INTERFACE_MAP_ENTRY(_i1)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i2)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i3)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i4)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i5)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i6)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i7)                                                               \
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, _i1)                                        \
  NS_INTERFACE_MAP_END_THREADSAFE

#define NS_IMPL_THREADSAFE_QUERY_INTERFACE8(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7, _i8)   \
  NS_INTERFACE_MAP_BEGIN(_class)                                                              \
    NS_INTERFACE_MAP_ENTRY(_i1)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i2)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i3)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i4)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i5)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i6)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i7)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i8)                                                               \
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, _i1)                                        \
  NS_INTERFACE_MAP_END_THREADSAFE

#define NS_IMPL_THREADSAFE_QUERY_INTERFACE9(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7, _i8, _i9) \
  NS_INTERFACE_MAP_BEGIN(_class)                                                              \
    NS_INTERFACE_MAP_ENTRY(_i1)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i2)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i3)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i4)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i5)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i6)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i7)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i8)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i9)                                                               \
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, _i1)                                        \
  NS_INTERFACE_MAP_END_THREADSAFE

#define NS_IMPL_THREADSAFE_QUERY_INTERFACE10(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7, _i8, _i9, _i10)  \
  NS_INTERFACE_MAP_BEGIN(_class)                                                              \
    NS_INTERFACE_MAP_ENTRY(_i1)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i2)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i3)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i4)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i5)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i6)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i7)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i8)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i9)                                                               \
    NS_INTERFACE_MAP_ENTRY(_i10)                                                              \
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, _i1)                                        \
  NS_INTERFACE_MAP_END_THREADSAFE

////////////////////////////////////////////////////////////////////////////////

#define NS_IMPL_THREADSAFE_ISUPPORTS0(_class)             \
  NS_IMPL_THREADSAFE_ADDREF(_class)                       \
  NS_IMPL_THREADSAFE_RELEASE(_class)                      \
  NS_IMPL_THREADSAFE_QUERY_INTERFACE0(_class)

#define NS_IMPL_THREADSAFE_ISUPPORTS1(_class, _interface) \
  NS_IMPL_THREADSAFE_ADDREF(_class)                       \
  NS_IMPL_THREADSAFE_RELEASE(_class)                      \
  NS_IMPL_THREADSAFE_QUERY_INTERFACE1(_class, _interface)

#define NS_IMPL_THREADSAFE_ISUPPORTS2(_class, _i1, _i2)   \
  NS_IMPL_THREADSAFE_ADDREF(_class)                       \
  NS_IMPL_THREADSAFE_RELEASE(_class)                      \
  NS_IMPL_THREADSAFE_QUERY_INTERFACE2(_class, _i1, _i2)

#define NS_IMPL_THREADSAFE_ISUPPORTS3(_class, _i1, _i2, _i3)   \
  NS_IMPL_THREADSAFE_ADDREF(_class)                            \
  NS_IMPL_THREADSAFE_RELEASE(_class)                           \
  NS_IMPL_THREADSAFE_QUERY_INTERFACE3(_class, _i1, _i2, _i3)

#define NS_IMPL_THREADSAFE_ISUPPORTS4(_class, _i1, _i2, _i3, _i4)   \
  NS_IMPL_THREADSAFE_ADDREF(_class)                                 \
  NS_IMPL_THREADSAFE_RELEASE(_class)                                \
  NS_IMPL_THREADSAFE_QUERY_INTERFACE4(_class, _i1, _i2, _i3, _i4)

#define NS_IMPL_THREADSAFE_ISUPPORTS5(_class, _i1, _i2, _i3, _i4, _i5)   \
  NS_IMPL_THREADSAFE_ADDREF(_class)                                      \
  NS_IMPL_THREADSAFE_RELEASE(_class)                                     \
  NS_IMPL_THREADSAFE_QUERY_INTERFACE5(_class, _i1, _i2, _i3, _i4, _i5)

#define NS_IMPL_THREADSAFE_ISUPPORTS6(_class, _i1, _i2, _i3, _i4, _i5, _i6)   \
  NS_IMPL_THREADSAFE_ADDREF(_class)                                      \
  NS_IMPL_THREADSAFE_RELEASE(_class)                                     \
  NS_IMPL_THREADSAFE_QUERY_INTERFACE6(_class, _i1, _i2, _i3, _i4, _i5, _i6)

#define NS_IMPL_THREADSAFE_ISUPPORTS7(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7)   \
  NS_IMPL_THREADSAFE_ADDREF(_class)                                      \
  NS_IMPL_THREADSAFE_RELEASE(_class)                                     \
  NS_IMPL_THREADSAFE_QUERY_INTERFACE7(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7)

#define NS_IMPL_THREADSAFE_ISUPPORTS8(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7, _i8)   \
  NS_IMPL_THREADSAFE_ADDREF(_class)                                      \
  NS_IMPL_THREADSAFE_RELEASE(_class)                                     \
  NS_IMPL_THREADSAFE_QUERY_INTERFACE8(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7, _i8)

#define NS_IMPL_THREADSAFE_ISUPPORTS9(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7, _i8, _i9)   \
  NS_IMPL_THREADSAFE_ADDREF(_class)                                      \
  NS_IMPL_THREADSAFE_RELEASE(_class)                                     \
  NS_IMPL_THREADSAFE_QUERY_INTERFACE9(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7, _i8, _i9)

#define NS_IMPL_THREADSAFE_ISUPPORTS10(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7, _i8, _i9, _i10)   \
  NS_IMPL_THREADSAFE_ADDREF(_class)                                      \
  NS_IMPL_THREADSAFE_RELEASE(_class)                                     \
  NS_IMPL_THREADSAFE_QUERY_INTERFACE10(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7, _i8, _i9, _i10)

///////////////////////////////////////////////////////////////////////////////
// Debugging Macros
///////////////////////////////////////////////////////////////////////////////

/**
 * Macro for instantiating a new object that implements nsISupports.
 * Use this in your factory methods to allow for refcnt tracing.
 * Note that you can only use this if you adhere to the no arguments
 * constructor com policy (which you really should!).
 * @param _result Where the new instance pointer is stored
 * @param _type The type of object to call "new" with.
 */
#define NS_NEWXPCOM(_result,_type)                                         \
  PR_BEGIN_MACRO                                                           \
    _result = new _type();                                                 \
    NS_LOG_NEW_XPCOM(_result, #_type, sizeof(_type), __FILE__, __LINE__);  \
  PR_END_MACRO

/**
 * Macro for deleting an object that implements nsISupports.
 * Use this in your Release methods to allow for refcnt tracing.
 * @param _ptr The object to delete.
 */
#define NS_DELETEXPCOM(_ptr)                                               \
  PR_BEGIN_MACRO                                                           \
    NS_LOG_DELETE_XPCOM((_ptr), __FILE__, __LINE__);                       \
    delete (_ptr);                                                         \
  PR_END_MACRO



/**
 * Macro for adding a reference to an interface.
 * @param _ptr The interface pointer.
 */
#define NS_ADDREF(_ptr) \
  NS_LOG_ADDREF_CALL((_ptr), (_ptr)->AddRef(), __FILE__, __LINE__)

/**
 * Macro for adding a reference to this. This macro should be used
 * because NS_ADDREF (when tracing) may require an ambiguous cast
 * from the pointers primary type to nsISupports. This macro sidesteps
 * that entire problem.
 */
#define NS_ADDREF_THIS() \
  NS_LOG_ADDREF_CALL(this, AddRef(), __FILE__, __LINE__)



extern "C++" {
// ...because some one is accidentally including this file inside
// an |extern "C"|

template <class T>
inline
nsrefcnt
ns_if_addref( T expr )
    // Making this a |inline| |template| allows |expr| to be evaluated only once,
    //  yet still denies you the ability to |AddRef()| an |nsCOMPtr|.
    // Note that |NS_ADDREF()| already has this property in the non-logging case.
  {
    return expr ? expr->AddRef() : 0;
  }

 } /* extern "C++" */

#ifdef NS_BUILD_REFCNT_LOGGING

/**
 * Macro for adding a reference to an interface that checks for NULL.
 * @param _expr The interface pointer.
 */
#define NS_IF_ADDREF(_expr)                                                 \
  ((0 != (_expr))                                                           \
   ? NS_LOG_ADDREF_CALL((_expr), ns_if_addref(_expr), __FILE__, __LINE__)   \
   : 0)

#else

#define NS_IF_ADDREF(_expr) ns_if_addref(_expr)

#endif

/*
 * Given these declarations, it explicitly OK and (in the non-logging build) efficient
 * to end a `getter' with:
 *
 *    NS_IF_ADDREF(*result = mThing);
 *
 * even if |mThing| is an |nsCOMPtr|.  If |mThing| is an |nsCOMPtr|, however, it is still
 * _illegal_ to say |NS_IF_ADDREF(mThing)|.
 */




/**
 * Macro for releasing a reference to an interface.
 *
 * Note that when NS_LOSING_ARCHITECTURE is defined that the release will
 * be done before the trace message is logged. If the reference count
 * goes to zero and implementation of Release logs a message, the two
 * messages will be logged out of order.
 *
 * @param _ptr The interface pointer.
 */
#define NS_RELEASE(_ptr)                                                   \
  PR_BEGIN_MACRO                                                           \
    NS_LOG_RELEASE_CALL((_ptr), (_ptr)->Release(), __FILE__, __LINE__);   \
    (_ptr) = 0;                                                             \
  PR_END_MACRO

/**
 * Macro for releasing a reference to an interface.
 *
 * Note that when NS_LOSING_ARCHITECTURE is defined that the release will
 * be done before the trace message is logged. If the reference count
 * goes to zero and implementation of Release logs a message, the two
 * messages will be logged out of order.
 *
 * @param _ptr The interface pointer.
 */
#define NS_RELEASE_THIS() \
    NS_LOG_RELEASE_CALL(this, Release(), __FILE__, __LINE__)

/**
 * Macro for releasing a reference to an interface, except that this
 * macro preserves the return value from the underlying Release call.
 * The interface pointer argument will only be NULLed if the reference count
 * goes to zero.
 *
 * Note that when NS_LOSING_ARCHITECTURE is defined that the release will
 * be done before the trace message is logged. If the reference count
 * goes to zero and implementation of Release logs a message, the two
 * messages will be logged out of order.
 *
 * @param _ptr The interface pointer.
 */
#define NS_RELEASE2(_ptr,_rv)                                                \
  PR_BEGIN_MACRO                                                             \
    _rv = NS_LOG_RELEASE_CALL((_ptr), (_ptr)->Release(),__FILE__,__LINE__);  \
    if (0 == (_rv)) (_ptr) = 0;                                              \
  PR_END_MACRO

/**
 * Macro for releasing a reference to an interface that checks for NULL;
 *
 * Note that when NS_LOSING_ARCHITECTURE is defined that the release will
 * be done before the trace message is logged. If the reference count
 * goes to zero and implementation of Release logs a message, the two
 * messages will be logged out of order.
 *
 * @param _ptr The interface pointer.
 */
#define NS_IF_RELEASE(_ptr)                                                 \
  PR_BEGIN_MACRO                                                            \
    if (_ptr) {                                                             \
      NS_LOG_RELEASE_CALL((_ptr), (_ptr)->Release(), __FILE__, __LINE__);   \
      (_ptr) = 0;                                                           \
    }                                                                       \
  PR_END_MACRO

///////////////////////////////////////////////////////////////////////////////

extern "C++" {
// ...because some one is accidentally including this file inside
// an |extern "C"|

class nsISupports;

template <class T>
struct nsCOMTypeInfo
  {
    static const nsIID& GetIID() { return T::GetIID(); }
  };

NS_SPECIALIZE_TEMPLATE
struct nsCOMTypeInfo<nsISupports>
  {
    static const nsIID& GetIID() {
        static nsIID iid = NS_ISUPPORTS_IID; return iid;
    }
  };

#define NS_GET_IID(T) nsCOMTypeInfo<T>::GetIID()

// a type-safe shortcut for calling the |QueryInterface()| member function
template <class DestinationType>
inline
nsresult
CallQueryInterface( nsISupports* aSource, DestinationType** aDestination )
  {
    NS_PRECONDITION(aSource, "null parameter");
    NS_PRECONDITION(aDestination, "null parameter");

    return aSource->QueryInterface(NS_GET_IID(DestinationType), (void**)aDestination);
  }

} // extern "C++"

///////////////////////////////////////////////////////////////////////////////
// Macros for checking the trueness of an expression passed in within an 
// interface implementation.
///////////////////////////////////////////////////////////////////////////////

#define NS_ENSURE_TRUE(x, ret)                         \
  PR_BEGIN_MACRO                                       \
   if(NS_WARN_IF_FALSE(x, "NS_ENSURE_TRUE(" #x ") failed")) \
     return ret;                                       \
  PR_END_MACRO

#define NS_ENSURE_FALSE(x, ret) \
  NS_ENSURE_TRUE(!(x), ret)

///////////////////////////////////////////////////////////////////////////////
// Macros for checking results
///////////////////////////////////////////////////////////////////////////////

#define NS_ENSURE_SUCCESS(res, ret) \
  NS_ENSURE_TRUE(NS_SUCCEEDED(res), ret)

///////////////////////////////////////////////////////////////////////////////
// Macros for checking state and arguments upon entering interface boundaries
///////////////////////////////////////////////////////////////////////////////

#define NS_ENSURE_ARG(arg) \
  NS_ENSURE_TRUE(arg, NS_ERROR_INVALID_ARG)

#define NS_ENSURE_ARG_POINTER(arg) \
  NS_ENSURE_TRUE(arg, NS_ERROR_INVALID_POINTER)

#define NS_ENSURE_ARG_MIN(arg, min) \
  NS_ENSURE_TRUE((arg) >= min, NS_ERROR_INVALID_ARG)

#define NS_ENSURE_ARG_MAX(arg, max) \
  NS_ENSURE_TRUE((arg) <= max, NS_ERROR_INVALID_ARG)

#define NS_ENSURE_ARG_RANGE(arg, min, max) \
  NS_ENSURE_TRUE(((arg) >= min) && ((arg) <= max), NS_ERROR_INVALID_ARG)

#define NS_ENSURE_STATE(state) \
  NS_ENSURE_TRUE(state, NS_ERROR_UNEXPECTED)

#define NS_ENSURE_NO_AGGREGATION(outer) \
  NS_ENSURE_FALSE(outer, NS_ERROR_NO_AGGREGATION)

#define NS_ENSURE_PROPER_AGGREGATION(outer, iid) \
  NS_ENSURE_FALSE(outer && !iid.Equals(NS_GET_IID(nsISupports)), NS_ERROR_INVALID_ARG)


///////////////////////////////////////////////////////////////////////////////

#endif /* __nsISupportsUtils_h */
