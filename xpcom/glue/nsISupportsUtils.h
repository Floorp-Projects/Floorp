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
NS_IMETHODIMP_(nsrefcnt) _class::AddRef(void)                                \
{                                                            \
  NS_PRECONDITION(PRInt32(mRefCnt) >= 0, "illegal refcnt");  \
  return ++mRefCnt;                                          \
}

/**
 * Use this macro to implement the Release method for a given <i>_class</i>
 * @param _class The name of the class implementing the method
 */
#define NS_IMPL_RELEASE(_class)                        \
NS_IMETHODIMP_(nsrefcnt) _class::Release(void)                         \
{                                                      \
  NS_PRECONDITION(0 != mRefCnt, "dup release");        \
  if (--mRefCnt == 0) {                                \
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
 * class in question implements nsISupports and it's own interface and
 * nothing else. Second, the implementation of the class's primary
 * inheritance chain leads to it's own interface.
 *
 * @param _class The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
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
#if defined(XP_PC)
#define NS_IMPL_THREADSAFE_ADDREF(_class)                                   \
NS_IMETHODIMP_(nsrefcnt) _class::AddRef(void)                                               \
{                                                                           \
  NS_PRECONDITION(PRInt32(mRefCnt) >= 0, "illegal refcnt");                 \
  return InterlockedIncrement((LONG*)&mRefCnt);                             \
}

#else /* ! XP_PC */
#define NS_IMPL_THREADSAFE_ADDREF(_class)                                   \
nsrefcnt _class::AddRef(void)                                               \
{                                                                           \
  nsrefcnt count;                                                           \
  NS_LOCK_INSTANCE();                                                       \
  NS_PRECONDITION(PRInt32(mRefCnt) >= 0, "illegal refcnt");                 \
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
NS_IMETHODIMP_(nsrefcnt) _class::Release(void)                         \
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

////////////////////////////////////////////////////////////////////////////////

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
    ((0 != (_ptr))                                                  \
    ? ((nsrefcnt) nsTraceRefcnt::Release((_ptr), (_ptr)->Release(), \
                                          __FILE__, __LINE__))      \
    : 0);                                                           \
    (_ptr) = 0;                                                     \
  PR_END_MACRO
#else
#define NS_IF_RELEASE(_ptr)                   \
  PR_BEGIN_MACRO                              \
    ((0 != (_ptr)) ? (_ptr)->Release() : 0);  \
    (_ptr) = 0;                               \
  PR_END_MACRO
#endif

////////////////////////////////////////////////////////////////////////////////

#endif /* __nsISupportsUtils_h */
