/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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

#ifndef __nsISupportsUtils_h
#define __nsISupportsUtils_h

/***************************************************************************/
/* this section copied from the hand written nsISupports.h */

#include "nsDebug.h"
#include "nsID.h"
#include "nsIID.h"
#include "nsError.h"
#include "pratom.h" /* needed for PR_AtomicIncrement and PR_AtomicDecrement */

#if defined(NS_MT_SUPPORTED)
#include "prcmon.h"
#endif  /* NS_MT_SUPPORTED */

  // under Metrowerks (Mac), we don't have autoconf yet
#ifdef __MWERKS__
  #define HAVE_CPP_SPECIALIZATION
#endif

  // under VC++ (Windows), we don't have autoconf yet
#if defined(_MSC_VER) && (_MSC_VER>=1100)
		// VC++ 5.0 and greater implement template specialization, 4.2 is unknown
  #define HAVE_CPP_SPECIALIZATION
#endif

#ifdef HAVE_CPP_SPECIALIZATION
	#define NSCAP_FEATURE_HIDE_NSISUPPORTS_GETIID
#endif

/*@{*/

////////////////////////////////////////////////////////////////////////////////

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
#if defined(XP_PC) && PR_BYTES_PER_LONG == 4
typedef unsigned long nsrefcnt;
#else
typedef PRUint32 nsrefcnt;
#endif

#include "nsTraceRefcnt.h"

/**
 * Basic component object model interface. Objects which implement
 * this interface support runtime interface discovery (QueryInterface)
 * and a reference counted memory model (AddRef/Release). This is
 * modelled after the win32 IUnknown API.
 */
class nsISupports {
public:

#ifndef NSCAP_FEATURE_HIDE_NSISUPPORTS_GETIID
  static const nsIID& GetIID() { static nsIID iid = NS_ISUPPORTS_IID; return iid; }
#endif

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
public:

#define NS_DECL_ISUPPORTS_EXPORTED                                          \
public:                                                                     \
  NS_EXPORT NS_IMETHOD QueryInterface(REFNSIID aIID,                        \
                            void** aInstancePtr);                           \
  NS_EXPORT NS_IMETHOD_(nsrefcnt) AddRef(void);                             \
  NS_EXPORT NS_IMETHOD_(nsrefcnt) Release(void);                            \
protected:                                                                  \
  nsrefcnt mRefCnt;                                                         \
public:

////////////////////////////////////////////////////////////////////////////////

/**
 * Initialize the reference count variable. Add this to each and every
 * constructor you implement.
 */
#define NS_INIT_REFCNT() mRefCnt = 0
#define NS_INIT_ISUPPORTS() NS_INIT_REFCNT() // what it should have been called in the first place

/**
 * Use this macro to implement the AddRef method for a given <i>_class</i>
 * @param _class The name of the class implementing the method
 */
#define NS_IMPL_ADDREF(_class)                               \
NS_IMETHODIMP_(nsrefcnt) _class::AddRef(void)                \
{                                                            \
  NS_PRECONDITION(PRInt32(mRefCnt) >= 0, "illegal refcnt");  \
  ++mRefCnt;                                                 \
  NS_LOG_ADDREF(this, mRefCnt, __FILE__, __LINE__);          \
  return mRefCnt;                                            \
}

/**
 * Use this macro to implement the Release method for a given <i>_class</i>
 * @param _class The name of the class implementing the method
 */
#define NS_IMPL_RELEASE(_class)                        \
NS_IMETHODIMP_(nsrefcnt) _class::Release(void)         \
{                                                      \
  NS_PRECONDITION(0 != mRefCnt, "dup release");        \
  --mRefCnt;                                           \
  NS_LOG_RELEASE(this, mRefCnt, __FILE__, __LINE__);   \
  if (mRefCnt == 0) {                                  \
    NS_DELETEXPCOM(this);                              \
    return 0;                                          \
  }                                                    \
  return mRefCnt;                                      \
}

////////////////////////////////////////////////////////////////////////////////

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

#define NS_IMPL_QUERY_TAIL(_supports_interface)                          \
  if ( aIID.Equals(NS_GET_IID(nsISupports)) )                            \
    foundInterface = NS_STATIC_CAST(_supports_interface*, this);         \
  else                                                                   \
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


#define NS_IMPL_QUERY_INTERFACE0(_class)                                 \
  NS_IMPL_QUERY_HEAD(_class)                                             \
  NS_IMPL_QUERY_TAIL(nsISupports)

#define NS_IMPL_QUERY_INTERFACE1(_class, _interface)                     \
  NS_IMPL_QUERY_HEAD(_class)                                             \
  NS_IMPL_QUERY_BODY(_interface)                                         \
  NS_IMPL_QUERY_TAIL(nsISupports)

#define NS_IMPL_QUERY_INTERFACE2(_class, _i1, _i2)                       \
  NS_IMPL_QUERY_HEAD(_class)                                             \
  NS_IMPL_QUERY_BODY(_i1)                                                \
  NS_IMPL_QUERY_BODY(_i2)                                                \
  NS_IMPL_QUERY_TAIL(_i1)

#define NS_IMPL_QUERY_INTERFACE3(_class, _i1, _i2, _i3)                  \
  NS_IMPL_QUERY_HEAD(_class)                                             \
  NS_IMPL_QUERY_BODY(_i1)                                                \
  NS_IMPL_QUERY_BODY(_i2)                                                \
  NS_IMPL_QUERY_BODY(_i3)                                                \
  NS_IMPL_QUERY_TAIL(_i1)


	/*
		The following macro is deprecated.  We need to switch all instances
		to |NS_IMPL_QUERY_INTERFACE1|, or |NS_IMPL_QUERY_INTERFACE0| depending
		on how they were using it.
	*/

#define NS_IMPL_QUERY_INTERFACE(_class,_classiiddef)                     \
NS_IMETHODIMP _class::QueryInterface(REFNSIID aIID, void** aInstancePtr)      \
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
  if (aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID())) {               \
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

#define NS_IMPL_ADDREF_INHERITED(Class, Super)                                \
NS_IMETHODIMP_(nsrefcnt) Class::AddRef(void)                                                  \
{                                                                             \
  return Super::AddRef();                                                     \
}                                                                             \

#define NS_IMPL_RELEASE_INHERITED(Class, Super)                               \
NS_IMETHODIMP_(nsrefcnt) Class::Release(void)                                                 \
{                                                                             \
  return Super::Release();                                                    \
}                                                                             \

#define NS_IMPL_QUERY_INTERFACE_INHERITED(Class, Super, AdditionalInterface)  \
NS_IMETHODIMP Class::QueryInterface(REFNSIID aIID, void** aInstancePtr)            \
{                                                                             \
  if (!aInstancePtr) return NS_ERROR_NULL_POINTER;                            \
  if (aIID.Equals(AdditionalInterface::GetIID())) {                           \
    *aInstancePtr = NS_STATIC_CAST(AdditionalInterface*, this);               \
    NS_ADDREF_THIS();                                                         \
    return NS_OK;                                                             \
  }                                                                           \
  return Super::QueryInterface(aIID, aInstancePtr);                           \
}                                                                             \

#define NS_IMPL_ISUPPORTS_INHERITED(Class, Super, AdditionalInterface)        \
    NS_IMPL_QUERY_INTERFACE_INHERITED(Class, Super, AdditionalInterface)      \
    NS_IMPL_ADDREF_INHERITED(Class, Super)                                    \
    NS_IMPL_RELEASE_INHERITED(Class, Super)                                   \

////////////////////////////////////////////////////////////////////////////////

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
  return PR_AtomicIncrement((PRInt32*)&mRefCnt);                            \
}

/**
 * Use this macro to implement the Release method for a given <i>_class</i>
 * @param _class The name of the class implementing the method
 */

#define NS_IMPL_THREADSAFE_RELEASE(_class)             \
nsrefcnt _class::Release(void)                         \
{                                                      \
  nsrefcnt count;                                      \
  NS_PRECONDITION(0 != mRefCnt, "dup release");        \
  count = PR_AtomicDecrement((PRInt32 *)&mRefCnt);     \
  if (0 == count) {                                    \
    NS_DELETEXPCOM(this);                              \
    return 0;                                          \
  }                                                    \
  return count;                                        \
}

////////////////////////////////////////////////////////////////////////////////

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
NS_IMETHODIMP _class::QueryInterface(REFNSIID aIID, void** aInstancePtr)      \
{                                                                        \
  if (NULL == aInstancePtr) {                                            \
    return NS_ERROR_NULL_POINTER;                                        \
  }                                                                      \
                                                                         \
  *aInstancePtr = NULL;                                                  \
                                                                         \
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 \
  static NS_DEFINE_IID(kIsThreadsafeIID, NS_ISTHREADSAFE_IID);           \
  static NS_DEFINE_IID(kClassIID, _classiiddef);                         \
  if (aIID.Equals(kClassIID)) {                                          \
    *aInstancePtr = (void*) this;                                        \
    NS_ADDREF_THIS();                                                    \
    return NS_OK;                                                        \
  }                                                                      \
  if (aIID.Equals(kISupportsIID)) {                                      \
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
// Debugging Macros
////////////////////////////////////////////////////////////////////////////////

/**
 * Macro for instantiating a new object that implements nsISupports.
 * Use this in your factory methods to allow for refcnt tracing.
 * Note that you can only use this if you adhere to the no arguments
 * constructor com policy (which you really should!).
 * @param _result Where the new instance pointer is stored
 * @param _type The type of object to call "new" with.
 */
#ifdef MOZ_TRACE_XPCOM_REFCNT
#define NS_NEWXPCOM(_result,_type)                              \
  PR_BEGIN_MACRO                                                \
    _result = new _type();                                      \
    nsTraceRefcnt::Create(_result, #_type, __FILE__, __LINE__); \
  PR_END_MACRO
#else
#define NS_NEWXPCOM(_result,_type) \
  _result = new _type()
#endif

/**
 * Macro for deleting an object that implements nsISupports.
 * Use this in your Release methods to allow for refcnt tracing.
 * @param _ptr The object to delete.
 */
#ifdef MOZ_TRACE_XPCOM_REFCNT
#define NS_DELETEXPCOM(_ptr)                            \
  PR_BEGIN_MACRO                                        \
    nsTraceRefcnt::Destroy((_ptr), __FILE__, __LINE__); \
    delete (_ptr);                                      \
  PR_END_MACRO
#else
#define NS_DELETEXPCOM(_ptr)    \
  delete (_ptr)
#endif

/**
 * Macro for adding a reference to an interface.
 * @param _ptr The interface pointer.
 */
#ifdef MOZ_TRACE_XPCOM_REFCNT
#define NS_ADDREF(_ptr)                                       \
  ((nsrefcnt) nsTraceRefcnt::AddRef((_ptr), (_ptr)->AddRef(), \
                                    __FILE__, __LINE__))
#else
#define NS_ADDREF(_ptr) \
  (_ptr)->AddRef()
#endif

/**
 * Macro for adding a reference to this. This macro should be used
 * because NS_ADDREF (when tracing) may require an ambiguous cast
 * from the pointers primary type to nsISupports. This macro sidesteps
 * that entire problem.
 */
#ifdef MOZ_TRACE_XPCOM_REFCNT
#define NS_ADDREF_THIS() \
  ((nsrefcnt) nsTraceRefcnt::AddRef(this, AddRef(), __FILE__, __LINE__))
#else
#define NS_ADDREF_THIS() \
  AddRef()
#endif

/**
 * Macro for adding a reference to an interface that checks for NULL.
 * @param _ptr The interface pointer.
 */
#ifdef MOZ_TRACE_XPCOM_REFCNT
#define NS_IF_ADDREF(_ptr)                                                 \
  ((0 != (_ptr))                                                           \
   ? ((nsrefcnt) nsTraceRefcnt::AddRef((_ptr), (_ptr)->AddRef(), __FILE__, \
                                       __LINE__))                          \
   : 0)
#else
#define NS_IF_ADDREF(_ptr) \
  ((0 != (_ptr)) ? (_ptr)->AddRef() : 0)
#endif

/**
 * Macro for releasing a reference to an interface.
 *
 * Note that when MOZ_TRACE_XPCOM_REFCNT is defined that the release will
 * be done before the trace message is logged. If the reference count
 * goes to zero and implementation of Release logs a message, the two
 * messages will be logged out of order.
 *
 * @param _ptr The interface pointer.
 */
#ifdef MOZ_TRACE_XPCOM_REFCNT
#define NS_RELEASE(_ptr)                                                   \
  PR_BEGIN_MACRO                                                           \
    nsTraceRefcnt::Release((_ptr), (_ptr)->Release(), __FILE__, __LINE__); \
    (_ptr) = 0;                                                            \
  PR_END_MACRO
#else
#define NS_RELEASE(_ptr)    \
  PR_BEGIN_MACRO            \
    (_ptr)->Release();      \
    (_ptr) = 0;             \
  PR_END_MACRO
#endif

/**
 * Macro for releasing a reference to an interface.
 *
 * Note that when MOZ_TRACE_XPCOM_REFCNT is defined that the release will
 * be done before the trace message is logged. If the reference count
 * goes to zero and implementation of Release logs a message, the two
 * messages will be logged out of order.
 *
 * @param _ptr The interface pointer.
 */
#ifdef MOZ_TRACE_XPCOM_REFCNT
#define NS_RELEASE_THIS()                                                  \
    nsTraceRefcnt::Release(this, Release(), __FILE__, __LINE__)
#else
#define NS_RELEASE_THIS()    \
    Release()
#endif

/**
 * Macro for releasing a reference to an interface, except that this
 * macro preserves the return value from the underlying Release call.
 * The interface pointer argument will only be NULLed if the reference count
 * goes to zero.
 *
 * Note that when MOZ_TRACE_XPCOM_REFCNT is defined that the release will
 * be done before the trace message is logged. If the reference count
 * goes to zero and implementation of Release logs a message, the two
 * messages will be logged out of order.
 *
 * @param _ptr The interface pointer.
 */
#ifdef MOZ_TRACE_XPCOM_REFCNT
#define NS_RELEASE2(_ptr, _result)                                          \
  PR_BEGIN_MACRO                                                            \
    _result = ((nsrefcnt) nsTraceRefcnt::Release((_ptr), (_ptr)->Release(), \
                                                 __FILE__, __LINE__));      \
    if (0 == (_result)) (_ptr) = 0;                                         \
  PR_END_MACRO
#else
#define NS_RELEASE2(_ptr, _result)  \
  PR_BEGIN_MACRO                    \
    _result = (_ptr)->Release();    \
    if (0 == (_result)) (_ptr) = 0; \
  PR_END_MACRO
#endif

/**
 * Macro for releasing a reference to an interface that checks for NULL;
 *
 * Note that when MOZ_TRACE_XPCOM_REFCNT is defined that the release will
 * be done before the trace message is logged. If the reference count
 * goes to zero and implementation of Release logs a message, the two
 * messages will be logged out of order.
 *
 * @param _ptr The interface pointer.
 */
#ifdef MOZ_TRACE_XPCOM_REFCNT
#define NS_IF_RELEASE(_ptr)                                         \
  PR_BEGIN_MACRO                                                    \
    if (_ptr) {                                                     \
      (nsrefcnt) nsTraceRefcnt::Release((_ptr), (_ptr)->Release(),  \
                                        __FILE__, __LINE__);        \
      (_ptr) = 0;                                                   \
    }                                                               \
  PR_END_MACRO
#else
#define NS_IF_RELEASE(_ptr)                                         \
  PR_BEGIN_MACRO                                                    \
    if (_ptr) {                                                     \
      (_ptr)->Release();                                            \
      (_ptr) = 0;                                                   \
    }                                                               \
  PR_END_MACRO
#endif

#ifdef MOZ_LOG_REFCNT
#define NS_LOG_ADDREF(_ptr, _refcnt, _file, _line) \
  nsTraceRefcnt::LogAddRef((_ptr), (_refcnt), (_file), (_line))

#define NS_LOG_RELEASE(_ptr, _refcnt, _file, _line) \
  nsTraceRefcnt::LogRelease((_ptr), (_refcnt), (_file), (_line))

#else
#define NS_LOG_ADDREF(_file, _line, _ptr, _refcnt)
#define NS_LOG_RELEASE(_file, _line, _ptr, _refcnt)
#endif

////////////////////////////////////////////////////////////////////////////////

// A type-safe interface for calling |QueryInterface()|.  A similar implementation
//	exists in "nsCOMPtr.h" for use with |nsCOMPtr|s.

extern "C++" {
	// ...because some one is accidentally including this file inside an |extern "C"|

class nsISupports;

template <class T>
struct nsCOMTypeInfo
	{
		static const nsIID& GetIID() { return T::GetIID(); }
	};

#ifdef NSCAP_FEATURE_HIDE_NSISUPPORTS_GETIID
	template <>
	struct nsCOMTypeInfo<nsISupports>
		{
			static const nsIID& GetIID() { static nsIID iid = NS_ISUPPORTS_IID; return iid; }
		};
#endif

#define NS_GET_IID(T) nsCOMTypeInfo<T>::GetIID()

template <class DestinationType>
inline
nsresult
CallQueryInterface( nsISupports* aSource, DestinationType** aDestination )
		// a type-safe shortcut for calling the |QueryInterface()| member function
	{
		NS_PRECONDITION(aSource, "null parameter");
    NS_PRECONDITION(aDestination, "null parameter");

		return aSource->QueryInterface(nsCOMTypeInfo<DestinationType>::GetIID(), (void**)aDestination);
	}

} // extern "C++"

////////////////////////////////////////////////////////////////////////////////
// Macros for checking the trueness of an expression passed in	within an 
// interface implementation.
////////////////////////////////////////////////////////////////////////////////

#define NS_ENSURE(x, ret)							\
PR_BEGIN_MACRO											\
	if(!(x))												\
		{													\
		NS_ERROR("NS_ENSURE(" #x ") failed");	\
		return ret;										\
		}													\
PR_END_MACRO

#define NS_ENSURE_NOT(x, ret) \
NS_ENSURE(!(x), ret)

////////////////////////////////////////////////////////////////////////////////
// Macros for checking results
////////////////////////////////////////////////////////////////////////////////

#define NS_ENSURE_SUCCESS(res, ret)				\
NS_ENSURE(NS_SUCCEEDED(res), ret)

////////////////////////////////////////////////////////////////////////////////
// Macros for checking state and arguments upon entering interface boundaries
////////////////////////////////////////////////////////////////////////////////

#define NS_ENSURE_ARG(arg) \
NS_ENSURE(arg, NS_ERROR_INVALID_ARG)

#define NS_ENSURE_ARG_POINTER(arg) \
NS_ENSURE(arg, NS_ERROR_INVALID_POINTER)

#define NS_ENSURE_ARG_MIN(arg, min) \
NS_ENSURE((arg) >= min, NS_ERROR_INVALID_ARG)

#define NS_ENSURE_ARG_MAX(arg, max) \
NS_ENSURE((arg) <= min, NS_ERROR_INVALID_ARG)

#define NS_ENSURE_ARG_RANGE(arg, min, max) \
NS_ENSURE(((arg) >= min) && ((arg) <= max), NS_ERROR_INVALID_ARG)

#define NS_ENSURE_STATE(state) \
NS_ENSURE(state, NS_ERROR_UNEXPECTED)

#define NS_ENSURE_NO_AGGREGATION(outer) \
NS_ENSURE_NOT(outer, NS_ERROR_NO_AGGREGATION)

#define NS_ENSURE_PROPER_AGGREGATION(outer, iid) \
NS_ENSURE_NOT(outer && !iid.Equals(NS_GET_IID(nsISupports)), NS_ERROR_INVALID_ARG)


////////////////////////////////////////////////////////////////////////////////

#endif /* __nsISupportsUtils_h */
