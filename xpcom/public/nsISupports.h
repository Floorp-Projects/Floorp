/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#ifndef nsISupports_h___
#define nsISupports_h___

#include "nsDebug.h"
#include "nsTraceRefcnt.h"
#include "nsID.h"
#include "nsError.h"

#if defined(NS_MT_SUPPORTED)
#include "prcmon.h"
#endif  /* NS_MT_SUPPORTED */

/*@{*/

/**
 * An "interface id" which can be used to uniquely identify a given
 * interface.
 */

typedef nsID nsIID;

/**
 * A macro shorthand for <tt>const nsIID&<tt>
 */

#define REFNSIID const nsIID&

/**
 * Define an IID (obsolete)
 */

#define NS_DEFINE_IID(_name, _iidspec) \
  const nsIID _name = _iidspec

//----------------------------------------------------------------------

/**
 * IID for the nsISupports interface
 * {00000000-0000-0000-c000-000000000046}
 *
 * NOTE: NEVER EVER EVER EVER EVER change this IID. Never. Not once.
 * No. Don't do it. Stop!
 */
#define NS_ISUPPORTS_IID      \
{ 0x00000000, 0x0000, 0x0000, \
  {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46} }

/**
 * Reference count values
 */
typedef PRUint32 nsrefcnt;

/**
 * Basic component object model interface. Objects which implement
 * this interface support runtime interface discovery (QueryInterface)
 * and a reference counted memory model (AddRef/Release). This is
 * modelled after the win32 IUnknown API.
 */
class nsISupports {
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

//----------------------------------------------------------------------

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

/**
 * Initialize the reference count variable. Add this to each and every
 * constructor you implement.
 */
#define NS_INIT_REFCNT() mRefCnt = 0

/**
 * Use this macro to implement the AddRef method for a given <i>_class</i>
 * @param _class The name of the class implementing the method
 */
#define NS_IMPL_ADDREF(_class)                               \
nsrefcnt _class::AddRef(void)                                \
{                                                            \
  return ++mRefCnt;                                          \
}

/**
 * Macro for instantiating a new object that implements nsISupports.
 * Use this in your factory methods to allow for refcnt tracing.
 * Note that you can only use this if you adhere to the no arguments
 * constructor com policy (which you really should!).
 * @param _result Where the new instance pointer is stored
 * @param _type The type of object to call "new" with.
 */
#ifdef MOZ_TRACE_XPCOM_REFCNT
#define NS_NEWXPCOM(_result,_type) \
  _result = new _type(); \
  nsTraceRefcnt::Create(_result, #_type, __FILE__, __LINE__)
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
#define NS_DELETEXPCOM(_ptr)                        \
  nsTraceRefcnt::Destroy(_ptr, __FILE__, __LINE__); \
  delete _ptr
#else
#define NS_DELETEXPCOM(_ptr)    \
  delete _ptr
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
#define NS_RELEASE(_ptr)                                                \
 nsTraceRefcnt::Release((_ptr), (_ptr)->Release(), __FILE__, __LINE__); \
 (_ptr) = nsnull
#else
#define NS_RELEASE(_ptr) \
 (_ptr)->Release();      \
 (_ptr) = nsnull
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
#define NS_RELEASE2(_ptr, _result)                                       \
 _result = ((nsrefcnt) nsTraceRefcnt::Release((_ptr), (_ptr)->Release(), \
                                              __FILE__, __LINE__));      \
 if (0 == (_result)) (_ptr) = nsnull
#else
#define NS_RELEASE2(_ptr, _result) \
 _result = (_ptr)->Release();      \
 if (0 == (_result)) (_ptr) = nsnull

#endif

/**
 * Macro for releasing a reference to an interface that checks for nsnull;
 *
 * Note that when MOZ_TRACE_XPCOM_REFCNT is defined that the release will
 * be done before the trace message is logged. If the reference count
 * goes to zero and implementation of Release logs a message, the two
 * messages will be logged out of order.
 *
 * @param _ptr The interface pointer.
 */
#ifdef MOZ_TRACE_XPCOM_REFCNT
#define NS_IF_RELEASE(_ptr)                                        \
  ((0 != (_ptr))                                                   \
   ? ((nsrefcnt) nsTraceRefcnt::Release((_ptr), (_ptr)->Release(), \
                                        __FILE__, __LINE__))       \
   : 0);                                                           \
  (_ptr) = nsnull
#else
#define NS_IF_RELEASE(_ptr)                \
  ((0 != (_ptr)) ? (_ptr)->Release() : 0); \
  (_ptr) = nsnull
#endif

/**
 * Use this macro to implement the Release method for a given <i>_class</i>
 * @param _class The name of the class implementing the method
 */
#define NS_IMPL_RELEASE(_class)                        \
nsrefcnt _class::Release(void)                         \
{                                                      \
  NS_PRECONDITION(0 != mRefCnt, "dup release");        \
  if (--mRefCnt == 0) {                                \
    NS_DELETEXPCOM(this);                              \
    return 0;                                          \
  }                                                    \
  return mRefCnt;                                      \
}

//----------------------------------------------------------------------

/*
 * Some convenience macros for implementing QueryInterface
 */

/** 
 * This implements query interface with two assumptions: First, the
 * class in question implements nsISupports and it's own interface and
 * nothing else. Second, the implementation of the class's primary
 * inheritance chain leads to it's own interface.
 *
 * @param _class The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
 */

#define NS_IMPL_QUERY_INTERFACE(_class,_classiiddef)                     \
nsresult _class::QueryInterface(REFNSIID aIID, void** aInstancePtr)      \
{                                                                        \
  if (nsnull == aInstancePtr) {                                            \
    return NS_ERROR_NULL_POINTER;                                        \
  }                                                                      \
                                                                         \
  *aInstancePtr = nsnull;                                                  \
                                                                         \
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 \
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
#if defined(XP_PC)
#define NS_IMPL_THREADSAFE_ADDREF(_class)                                   \
nsrefcnt _class::AddRef(void)                                               \
{                                                                           \
  return InterlockedIncrement((LONG*)&mRefCnt);                             \
}

#else /* ! XP_PC */
#define NS_IMPL_THREADSAFE_ADDREF(_class)                                   \
nsrefcnt _class::AddRef(void)                                               \
{                                                                           \
  nsrefcnt count;                                                           \
  NS_LOCK_INSTANCE();                                                       \
  count = ++mRefCnt;                                                        \
  NS_UNLOCK_INSTANCE();                                                     \
  return count;                                                             \
}
#endif /* ! XP_PC */

/**
 * Use this macro to implement the Release method for a given <i>_class</i>
 * @param _class The name of the class implementing the method
 */
#if defined(XP_PC)
#define NS_IMPL_THREADSAFE_RELEASE(_class)             \
nsrefcnt _class::Release(void)                         \
{                                                      \
  NS_PRECONDITION(0 != mRefCnt, "dup release");        \
  if (0 == InterlockedDecrement((LONG*)&mRefCnt)) {    \
    NS_DELETEXPCOM(this);                              \
    return 0;                                          \
  }                                                    \
  return mRefCnt; /* Not threadsafe but who cares. */  \
}

#else /* ! XP_PC */
#define NS_IMPL_THREADSAFE_RELEASE(_class)             \
nsrefcnt _class::Release(void)                         \
{                                                      \
  nsrefcnt count;                                      \
  NS_PRECONDITION(0 != mRefCnt, "dup release");        \
  NS_LOCK_INSTANCE();                                  \
  count = --mRefCnt;                                   \
  NS_UNLOCK_INSTANCE();                                \
  if (0 == count) {                                    \
    NS_DELETEXPCOM(this);                              \
    return 0;                                          \
  }                                                    \
  return count;                                        \
}
#endif /* ! XP_PC */

//----------------------------------------------------------------------

/*
 * Some convenience macros for implementing QueryInterface
 */

/** 
 * This implements query interface with two assumptions: First, the
 * class in question implements nsISupports and it's own interface and
 * nothing else. Second, the implementation of the class's primary
 * inheritance chain leads to it's own interface.
 *
 * @param _class The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
 */
#if defined(NS_DEBUG)
#define NS_VERIFY_THREADSAFE_INTERFACE(_iface)                              \
 if (nsnull != (_iface)) {                                                    \
   nsISupports* tmp;                                                        \
   static NS_DEFINE_IID(kIsThreadsafeIID, NS_ISTHREADSAFE_IID);             \
   NS_PRECONDITION((NS_OK == _iface->QueryInterface(kIsThreadsafeIID,       \
                                                    (void**)&tmp)),         \
                   "Interface is not threadsafe");                          \
 }

#define NS_IMPL_THREADSAFE_QUERY_INTERFACE(_class,_classiiddef)          \
nsresult _class::QueryInterface(REFNSIID aIID, void** aInstancePtr)      \
{                                                                        \
  if (nsnull == aInstancePtr) {                                            \
    return NS_ERROR_NULL_POINTER;                                        \
  }                                                                      \
                                                                         \
  *aInstancePtr = nsnull;                                                  \
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

/*@}*/

#endif /* nsISupports_h___ */
