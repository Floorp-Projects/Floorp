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
 *   Dan Mosedale <dmose@mozilla.org>
 */

#ifndef nsISupportsUtils_h__
#define nsISupportsUtils_h__

#ifndef nscore_h___
#include "nscore.h"
#endif

#ifndef nsISupportsBase_h__
#include "nsISupportsBase.h"
#endif

#ifndef nsError_h__
#include "nsError.h"
#endif

#ifndef nsDebug_h___
#include "nsDebug.h"
#endif

#ifndef nsISupportsImpl_h__
#include "nsISupportsImpl.h"
#endif

#ifndef nsISupportsObsolete_h__
#include "nsISupportsObsolete.h"
#endif

/**
 * Macro for instantiating a new object that implements nsISupports.
 * Use this in your factory methods to allow for refcnt tracing.
 * Note that you can only use this if you adhere to the no arguments
 * constructor com policy (which you really should!).
 * @param _result Where the new instance pointer is stored
 * @param _type The type of object to call "new" with.
 */
#define NS_NEWXPCOM(_result,_type)                                            \
  PR_BEGIN_MACRO                                                              \
    _result = new _type();                                                    \
    NS_LOG_NEW_XPCOM(_result, #_type, sizeof(_type), __FILE__, __LINE__);     \
  PR_END_MACRO

/**
 * Macro for deleting an object that implements nsISupports.
 * Use this in your Release methods to allow for refcnt tracing.
 * @param _ptr The object to delete.
 */
#define NS_DELETEXPCOM(_ptr)                                                  \
  PR_BEGIN_MACRO                                                              \
    NS_LOG_DELETE_XPCOM((_ptr), __FILE__, __LINE__);                          \
    delete (_ptr);                                                            \
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

/**
 * Macro for adding a reference to an interface that checks for NULL.
 * @param _expr The interface pointer.
 */
#ifdef NS_BUILD_REFCNT_LOGGING
#define NS_IF_ADDREF(_expr)                                                   \
  ((0 != (_expr))                                                             \
   ? NS_LOG_ADDREF_CALL((_expr), ns_if_addref(_expr), __FILE__, __LINE__)     \
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
#define NS_RELEASE(_ptr)                                                      \
  PR_BEGIN_MACRO                                                              \
    NS_LOG_RELEASE_CALL((_ptr), (_ptr)->Release(), __FILE__, __LINE__);       \
    (_ptr) = 0;                                                               \
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
#define NS_RELEASE2(_ptr,_rv)                                                 \
  PR_BEGIN_MACRO                                                              \
    _rv = NS_LOG_RELEASE_CALL((_ptr), (_ptr)->Release(),__FILE__,__LINE__);   \
    if (0 == (_rv)) (_ptr) = 0;                                               \
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
#define NS_IF_RELEASE(_ptr)                                                   \
  PR_BEGIN_MACRO                                                              \
    if (_ptr) {                                                               \
      NS_LOG_RELEASE_CALL((_ptr), (_ptr)->Release(), __FILE__, __LINE__);     \
      (_ptr) = 0;                                                             \
    }                                                                         \
  PR_END_MACRO

/*
 * Often you have to cast an implementation pointer, e.g., |this|, to an
 * |nsISupports*|, but because you have multiple inheritance, a simple cast
 * is ambiguous.  One could simply say, e.g., (given a base |nsIBase|),
 * |NS_STATIC_CAST(nsIBase*, this)|; but that disguises the fact that what
 * you are really doing is disambiguating the |nsISupports|.  You could make
 * that more obvious with a double cast, e.g., |NS_STATIC_CAST(nsISupports*,
 * NS_STATIC_CAST(nsIBase*, this))|, but that is bulky and harder to read...
 *
 * The following macro is clean, short, and obvious.  In the example above,
 * you would use it like this: |NS_ISUPPORTS_CAST(nsIBase*, this)|.
 */

#define NS_ISUPPORTS_CAST(__unambiguousBase, __expr) \
  NS_STATIC_CAST(nsISupports*, NS_STATIC_CAST(__unambiguousBase, __expr))




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
        static const nsIID iid = NS_ISUPPORTS_IID; return iid;
    }
  };

#define NS_GET_IID(T) nsCOMTypeInfo<T>::GetIID()

// a type-safe shortcut for calling the |QueryInterface()| member function
template <class T, class DestinationType>
inline
nsresult
CallQueryInterface( T* aSource, DestinationType** aDestination )
  {
    NS_PRECONDITION(aSource, "null parameter");
    NS_PRECONDITION(aDestination, "null parameter");

    return aSource->QueryInterface(NS_GET_IID(DestinationType),
                                   NS_REINTERPRET_CAST(void**, aDestination));
  }

} // extern "C++"

#endif /* __nsISupportsUtils_h */
