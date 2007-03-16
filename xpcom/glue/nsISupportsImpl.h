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
 * The Original Code is XPCOM.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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


#ifndef nsISupportsImpl_h__
#define nsISupportsImpl_h__

#ifndef nscore_h___
#include "nscore.h"
#endif

#ifndef nsISupportsBase_h__
#include "nsISupportsBase.h"
#endif

#if defined(XPCOM_GLUE) && !defined(XPCOM_GLUE_USE_NSPR)
// If we're being linked as standalone glue, we don't want a dynamic dependency
// on NSPR libs, so we skip the debug thread-safety checks, and we cannot use
// the THREADSAFE_ISUPPORTS macros.

#define XPCOM_GLUE_AVOID_NSPR

#endif


#if !defined(XPCOM_GLUE_AVOID_NSPR)
#include "prthread.h" /* needed for thread-safety checks */
#include "pratom.h"   /* needed for PR_AtomicIncrement and PR_AtomicDecrement */
#endif

#include "nsDebug.h"
#include "nsTraceRefcnt.h"
#include "nsCycleCollector.h"

////////////////////////////////////////////////////////////////////////////////
// Macros to help detect thread-safety:

#if defined(NS_DEBUG) && !defined(XPCOM_GLUE_AVOID_NSPR)

class nsAutoOwningThread {
public:
    nsAutoOwningThread() { mThread = PR_GetCurrentThread(); }
    void *GetThread() const { return mThread; }

private:
    void *mThread;
};

#define NS_DECL_OWNINGTHREAD            nsAutoOwningThread _mOwningThread;
#define NS_ASSERT_OWNINGTHREAD(_class) \
  NS_CheckThreadSafe(_mOwningThread.GetThread(), #_class " not thread-safe")

#else // !NS_DEBUG

#define NS_DECL_OWNINGTHREAD            /* nothing */
#define NS_ASSERT_OWNINGTHREAD(_class)  ((void)0)

#endif // NS_DEBUG

#define NS_PURPLE_BIT ((PRUint32)(1 << 31))

#define NS_PURPLE_MASK (~NS_PURPLE_BIT)
#define NS_PURPLE_BIT_SET(x) ((x) & (NS_PURPLE_BIT))
#define NS_SET_PURPLE_BIT(x) ((x) |= (NS_PURPLE_BIT))
#define NS_CLEAR_PURPLE_BIT(x) ((x) &= (NS_PURPLE_MASK))
#define NS_VALUE_WITHOUT_PURPLE_BIT(x) ((x) & (NS_PURPLE_MASK))


// Support for ISupports classes which interact with cycle collector.

class nsCycleCollectingAutoRefCnt {

public:
  nsCycleCollectingAutoRefCnt()
    : mValue(0)
  {}

  nsCycleCollectingAutoRefCnt(nsrefcnt aValue)
    : mValue(aValue)
  {
    NS_CLEAR_PURPLE_BIT(mValue);
  }

  nsrefcnt incr(nsISupports *owner)
  {
    if (NS_UNLIKELY(mValue == NS_PURPLE_BIT)) {
      // The sentinel value "purple bit alone, refcount 0" means
      // that we're stabilized, during finalization. In this
      // state we lie about our actual refcount if anyone asks
      // and say it's 2, which is basically true: the caller who
      // is incrementing has a reference, as does the decr() frame
      // that stabilized-and-is-deleting us.
      return 2;
    }

    nsrefcnt tmp = get();
    PRBool purple = NS_STATIC_CAST(PRBool, NS_PURPLE_BIT_SET(mValue));

    if (NS_UNLIKELY(purple)) {
      NS_ASSERTION(tmp != 0, "purple ISupports pointer with zero refcnt");
      nsCycleCollector_forget(owner);
    }

    mValue = tmp + 1;
    return mValue;
  }

  void stabilizeForDeletion(nsISupports *owner)
  {
    mValue = NS_PURPLE_BIT;
  }

  nsrefcnt decr(nsISupports *owner)
  {
    if (NS_UNLIKELY(mValue == NS_PURPLE_BIT))
      return 1;

    nsrefcnt tmp = get();
    PRBool purple = NS_STATIC_CAST(PRBool, NS_PURPLE_BIT_SET(mValue));

    if (NS_UNLIKELY(tmp > 1 && !purple)) {
      nsCycleCollector_suspect(owner);
      purple = PR_TRUE;

    } else if (NS_UNLIKELY(tmp == 1 && purple)) {
      nsCycleCollector_forget(owner);
      purple = PR_FALSE;

    } else {
      NS_ASSERTION(tmp >= 1, "decr() called with zero refcnt");
    }

    mValue = tmp - 1;

    if (purple)
      NS_SET_PURPLE_BIT(mValue);

    return get();
  }

  void unmarkPurple()
  {
    if (NS_LIKELY(mValue != NS_PURPLE_BIT))
      NS_CLEAR_PURPLE_BIT(mValue);
  }

  nsrefcnt get() const
  {
    if (NS_UNLIKELY(mValue == NS_PURPLE_BIT))
      return 1;

    return NS_VALUE_WITHOUT_PURPLE_BIT(mValue);
  }

  operator nsrefcnt() const
  {
    return get();
  }

 private:
  nsrefcnt mValue;
};

class nsAutoRefCnt {

 public:
    nsAutoRefCnt() : mValue(0) {}
    nsAutoRefCnt(nsrefcnt aValue) : mValue(aValue) {}

    // only support prefix increment/decrement
    nsrefcnt operator++() { return ++mValue; }
    nsrefcnt operator--() { return --mValue; }

    nsrefcnt operator=(nsrefcnt aValue) { return (mValue = aValue); }
    operator nsrefcnt() const { return mValue; }
    nsrefcnt get() const { return mValue; }
 private:
    // do not define these to enforce the faster prefix notation
    nsrefcnt operator++(int);
    nsrefcnt operator--(int);
    nsrefcnt mValue;
};

///////////////////////////////////////////////////////////////////////////////

/**
 * Declare the reference count variable and the implementations of the
 * AddRef and QueryInterface methods.
 */

#define NS_DECL_ISUPPORTS                                                     \
public:                                                                       \
  NS_IMETHOD QueryInterface(REFNSIID aIID,                                    \
                            void** aInstancePtr);                             \
  NS_IMETHOD_(nsrefcnt) AddRef(void);                                         \
  NS_IMETHOD_(nsrefcnt) Release(void);                                        \
protected:                                                                    \
  nsAutoRefCnt mRefCnt;                                                       \
  NS_DECL_OWNINGTHREAD                                                        \
public:

#define NS_DECL_CYCLE_COLLECTING_ISUPPORTS                                    \
public:                                                                       \
  NS_IMETHOD QueryInterface(REFNSIID aIID,                                    \
                            void** aInstancePtr);                             \
  NS_IMETHOD_(nsrefcnt) AddRef(void);                                         \
  NS_IMETHOD_(nsrefcnt) Release(void);                                        \
  void UnmarkPurple()                                                         \
  {                                                                           \
    mRefCnt.unmarkPurple();                                                   \
  }                                                                           \
protected:                                                                    \
  nsCycleCollectingAutoRefCnt mRefCnt;                                        \
  NS_DECL_OWNINGTHREAD                                                        \
public:


///////////////////////////////////////////////////////////////////////////////

/**
 * Previously used to initialize the reference count, but no longer needed.
 *
 * DEPRECATED.
 */
#define NS_INIT_ISUPPORTS() ((void)0)

/**
 * Use this macro to implement the AddRef method for a given <i>_class</i>
 * @param _class The name of the class implementing the method
 */
#define NS_IMPL_ADDREF(_class)                                                \
NS_IMETHODIMP_(nsrefcnt) _class::AddRef(void)                                 \
{                                                                             \
  NS_PRECONDITION(PRInt32(mRefCnt) >= 0, "illegal refcnt");                   \
  NS_ASSERT_OWNINGTHREAD(_class);                                             \
  ++mRefCnt;                                                                  \
  NS_LOG_ADDREF(this, mRefCnt, #_class, sizeof(*this));                       \
  return mRefCnt;                                                             \
}

/**
 * Use this macro to implement the AddRef method for a given <i>_class</i>
 * implemented as a wholly owned aggregated object intended to implement
 * interface(s) for its owner
 * @param _class The name of the class implementing the method
 * @param _aggregator the owning/containing object
 */
#define NS_IMPL_ADDREF_USING_AGGREGATOR(_class, _aggregator)                  \
NS_IMETHODIMP_(nsrefcnt) _class::AddRef(void)                                 \
{                                                                             \
  NS_PRECONDITION(_aggregator, "null aggregator");                            \
  return (_aggregator)->AddRef();                                             \
}

/**
 * Use this macro to implement the Release method for a given
 * <i>_class</i>.
 * @param _class The name of the class implementing the method
 * @param _destroy A statement that is executed when the object's
 *   refcount drops to zero.
 *
 * For example,
 *
 *   NS_IMPL_RELEASE_WITH_DESTROY(Foo, Destroy(this))
 *
 * will cause
 *
 *   Destroy(this);
 *
 * to be invoked when the object's refcount drops to zero. This
 * allows for arbitrary teardown activity to occur (e.g., deallocation
 * of object allocated with placement new).
 */
#define NS_IMPL_RELEASE_WITH_DESTROY(_class, _destroy)                        \
NS_IMETHODIMP_(nsrefcnt) _class::Release(void)                                \
{                                                                             \
  NS_PRECONDITION(0 != mRefCnt, "dup release");                               \
  NS_ASSERT_OWNINGTHREAD(_class);                                             \
  --mRefCnt;                                                                  \
  NS_LOG_RELEASE(this, mRefCnt, #_class);                                     \
  if (mRefCnt == 0) {                                                         \
    mRefCnt = 1; /* stabilize */                                              \
    _destroy;                                                                 \
    return 0;                                                                 \
  }                                                                           \
  return mRefCnt;                                                             \
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
#define NS_IMPL_RELEASE(_class) \
  NS_IMPL_RELEASE_WITH_DESTROY(_class, NS_DELETEXPCOM(this))

/**
 * Use this macro to implement the Release method for a given <i>_class</i>
 * implemented as a wholly owned aggregated object intended to implement
 * interface(s) for its owner
 * @param _class The name of the class implementing the method
 * @param _aggregator the owning/containing object
 */
#define NS_IMPL_RELEASE_USING_AGGREGATOR(_class, _aggregator)                 \
NS_IMETHODIMP_(nsrefcnt) _class::Release(void)                                \
{                                                                             \
  NS_PRECONDITION(_aggregator, "null aggregator");                            \
  return (_aggregator)->Release();                                            \
}


#define NS_IMPL_CYCLE_COLLECTING_ADDREF_AMBIGUOUS(_class, _basetype)          \
NS_IMETHODIMP_(nsrefcnt) _class::AddRef(void)                                 \
{                                                                             \
  NS_PRECONDITION(PRInt32(mRefCnt) >= 0, "illegal refcnt");                   \
  NS_ASSERT_OWNINGTHREAD(_class);                                             \
  nsrefcnt count =                                                            \
    mRefCnt.incr(NS_CYCLE_COLLECTION_CLASSNAME(_class)::Upcast(this));        \
  NS_LOG_ADDREF(this, count, #_class, sizeof(*this));                         \
  return count;                                                               \
}

#define NS_IMPL_CYCLE_COLLECTING_ADDREF(_class)      \
  NS_IMPL_CYCLE_COLLECTING_ADDREF_AMBIGUOUS(_class, _class)

#define NS_IMPL_CYCLE_COLLECTING_RELEASE_FULL(_class, _basetype, _destroy)    \
NS_IMETHODIMP_(nsrefcnt) _class::Release(void)                                \
{                                                                             \
  NS_PRECONDITION(0 != mRefCnt, "dup release");                               \
  NS_ASSERT_OWNINGTHREAD(_class);                                             \
  nsISupports *base = NS_CYCLE_COLLECTION_CLASSNAME(_class)::Upcast(this);    \
  nsrefcnt count = mRefCnt.decr(base);                                        \
  NS_LOG_RELEASE(this, count, #_class);                                       \
  if (count == 0) {                                                           \
    mRefCnt.stabilizeForDeletion(base);                                       \
    _destroy;                                                                 \
    return 0;                                                                 \
  }                                                                           \
  return count;                                                               \
}

#define NS_IMPL_CYCLE_COLLECTING_RELEASE_WITH_DESTROY(_class, _destroy)       \
  NS_IMPL_CYCLE_COLLECTING_RELEASE_FULL(_class, _class, _destroy)

#define NS_IMPL_CYCLE_COLLECTING_RELEASE_AMBIGUOUS_WITH_DESTROY(_class, _basetype, _destroy)         \
  NS_IMPL_CYCLE_COLLECTING_RELEASE_FULL(_class, _basetype, _destroy)

#define NS_IMPL_CYCLE_COLLECTING_RELEASE_AMBIGUOUS(_class, _basetype)         \
  NS_IMPL_CYCLE_COLLECTING_RELEASE_FULL(_class, _basetype, NS_DELETEXPCOM(this))

#define NS_IMPL_CYCLE_COLLECTING_RELEASE(_class)       \
  NS_IMPL_CYCLE_COLLECTING_RELEASE_FULL(_class, _class, NS_DELETEXPCOM(this))


///////////////////////////////////////////////////////////////////////////////

/**
 * There are two ways of implementing QueryInterface, and we use both:
 *
 * Table-driven QueryInterface uses a static table of IID->offset mappings
 * and a shared helper function. Using it tends to reduce codesize and improve
 * runtime performance (due to processor cache hits).
 *
 * Macro-driven QueryInterface generates a QueryInterface function directly
 * using common macros. This is necessary if special QueryInterface features
 * are being used (such as tearoffs and conditional interfaces).
 *
 * These methods can be combined into a table-driven function call followed
 * by custom code for tearoffs and conditionals.
 */

struct QITableEntry
{
  const nsIID *iid;     // null indicates end of the QITableEntry array
  PROffset32   offset;
};

NS_COM_GLUE nsresult NS_FASTCALL
NS_TableDrivenQI(void* aThis, const QITableEntry* entries,
                 REFNSIID aIID, void **aInstancePtr);

/**
 * Implement table-driven queryinterface
 */

#define NS_INTERFACE_TABLE_HEAD(_class)                                       \
NS_IMETHODIMP _class::QueryInterface(REFNSIID aIID, void** aInstancePtr)      \
{                                                                             \
  NS_ASSERTION(aInstancePtr,                                                  \
               "QueryInterface requires a non-NULL destination!");            \
  nsresult rv = NS_ERROR_FAILURE;;

#define NS_INTERFACE_TABLE_BEGIN                                              \
  static const QITableEntry table[] = {

#define NS_INTERFACE_TABLE_ENTRY(_class, _interface)                          \
  { &_interface::COMTypeInfo<int>::kIID,                                      \
    NS_REINTERPRET_CAST(char*,                                                \
                        NS_STATIC_CAST(_interface*, (_class*) 0x1000)) -      \
    NS_REINTERPRET_CAST(char*, (_class*) 0x1000)                              \
  },

#define NS_INTERFACE_TABLE_ENTRY_AMBIGUOUS(_class, _interface, _implClass)    \
  { &_interface::COMTypeInfo<int>::kIID,                                      \
    NS_REINTERPRET_CAST(char*,                                                \
                        NS_STATIC_CAST(_interface*,                           \
                                       NS_STATIC_CAST(_implClass*,            \
                                                      (_class*) 0x1000))) -   \
    NS_REINTERPRET_CAST(char*, (_class*) 0x1000)                              \
  },

#define NS_INTERFACE_TABLE_END                                                \
  { nsnull, 0 } };                                                            \
  rv = NS_TableDrivenQI(NS_STATIC_CAST(void*, this),                          \
                        table, aIID, aInstancePtr);

#define NS_INTERFACE_TABLE_TAIL                                               \
  return rv;                                                                  \
}

#define NS_INTERFACE_TABLE_TAIL_INHERITING(_baseclass)                        \
  if (NS_SUCCEEDED(rv))                                                       \
    return rv;                                                                \
  return _baseclass::QueryInterface(aIID, aInstancePtr);                      \
}

#define NS_INTERFACE_TABLE_TAIL_USING_AGGREGATOR(_aggregator)                 \
  if (NS_SUCCEEDED(rv))                                                       \
    return rv;                                                                \
  NS_ASSERTION(_aggregator, "null aggregator");                               \
  return _aggregator->QueryInterface(aIID, aInstancePtr)                      \
}

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

#define NS_IMPL_QUERY_HEAD(_class)                                            \
NS_IMETHODIMP _class::QueryInterface(REFNSIID aIID, void** aInstancePtr)      \
{                                                                             \
  NS_ASSERTION(aInstancePtr,                                                  \
               "QueryInterface requires a non-NULL destination!");            \
  nsISupports* foundInterface;

#define NS_IMPL_QUERY_BODY(_interface)                                        \
  if ( aIID.Equals(NS_GET_IID(_interface)) )                                  \
    foundInterface = NS_STATIC_CAST(_interface*, this);                       \
  else

#define NS_IMPL_QUERY_BODY_CONDITIONAL(_interface, condition)                 \
  if ( (condition) && aIID.Equals(NS_GET_IID(_interface)))                    \
    foundInterface = NS_STATIC_CAST(_interface*, this);                       \
  else

#define NS_IMPL_QUERY_BODY_AMBIGUOUS(_interface, _implClass)                  \
  if ( aIID.Equals(NS_GET_IID(_interface)) )                                  \
    foundInterface = NS_STATIC_CAST(_interface*,                              \
                                    NS_STATIC_CAST(_implClass*, this));       \
  else

#define NS_IMPL_QUERY_BODY_AGGREGATED(_interface, _aggregate)                 \
  if ( aIID.Equals(NS_GET_IID(_interface)) )                                  \
    foundInterface = NS_STATIC_CAST(_interface*, _aggregate);                 \
  else

#define NS_IMPL_QUERY_TAIL_GUTS                                               \
    foundInterface = 0;                                                       \
  nsresult status;                                                            \
  if ( !foundInterface )                                                      \
    status = NS_NOINTERFACE;                                                  \
  else                                                                        \
    {                                                                         \
      NS_ADDREF(foundInterface);                                              \
      status = NS_OK;                                                         \
    }                                                                         \
  *aInstancePtr = foundInterface;                                             \
  return status;                                                              \
}

#define NS_IMPL_QUERY_TAIL_INHERITING(_baseclass)                             \
    foundInterface = 0;                                                       \
  nsresult status;                                                            \
  if ( !foundInterface )                                                      \
    status = _baseclass::QueryInterface(aIID, (void**)&foundInterface);       \
  else                                                                        \
    {                                                                         \
      NS_ADDREF(foundInterface);                                              \
      status = NS_OK;                                                         \
    }                                                                         \
  *aInstancePtr = foundInterface;                                             \
  return status;                                                              \
}

#define NS_IMPL_QUERY_TAIL_USING_AGGREGATOR(_aggregator)                      \
    foundInterface = 0;                                                       \
  nsresult status;                                                            \
  if ( !foundInterface ) {                                                    \
    NS_ASSERTION(_aggregator, "null aggregator");                             \
    status = _aggregator->QueryInterface(aIID, (void**)&foundInterface);      \
  } else                                                                      \
    {                                                                         \
      NS_ADDREF(foundInterface);                                              \
      status = NS_OK;                                                         \
    }                                                                         \
  *aInstancePtr = foundInterface;                                             \
  return status;                                                              \
}

#define NS_IMPL_QUERY_TAIL(_supports_interface)                               \
  NS_IMPL_QUERY_BODY_AMBIGUOUS(nsISupports, _supports_interface)              \
  NS_IMPL_QUERY_TAIL_GUTS


  /*
    This is the new scheme.  Using this notation now will allow us to switch to
    a table driven mechanism when it's ready.  Note the difference between this
    and the (currently) underlying NS_IMPL_QUERY_INTERFACE mechanism.  You must
    explicitly mention |nsISupports| when using the interface maps.
  */
#define NS_INTERFACE_MAP_BEGIN(_implClass)      NS_IMPL_QUERY_HEAD(_implClass)
#define NS_INTERFACE_MAP_ENTRY(_interface)      NS_IMPL_QUERY_BODY(_interface)
#define NS_INTERFACE_MAP_ENTRY_CONDITIONAL(_interface, condition)             \
  NS_IMPL_QUERY_BODY_CONDITIONAL(_interface, condition)
#define NS_INTERFACE_MAP_ENTRY_AGGREGATED(_interface,_aggregate)              \
  NS_IMPL_QUERY_BODY_AGGREGATED(_interface,_aggregate)

#define NS_INTERFACE_MAP_END                    NS_IMPL_QUERY_TAIL_GUTS
#define NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(_interface, _implClass)              \
  NS_IMPL_QUERY_BODY_AMBIGUOUS(_interface, _implClass)
#define NS_INTERFACE_MAP_END_INHERITING(_baseClass)                           \
  NS_IMPL_QUERY_TAIL_INHERITING(_baseClass)
#define NS_INTERFACE_MAP_END_AGGREGATED(_aggregator)                          \
  NS_IMPL_QUERY_TAIL_USING_AGGREGATOR(_aggregator)

#define NS_IMPL_QUERY_INTERFACE0(_class)                                      \
  NS_INTERFACE_TABLE_HEAD(_class)                                             \
  NS_INTERFACE_TABLE_BEGIN                                                    \
    NS_INTERFACE_TABLE_ENTRY(_class, nsISupports)                             \
  NS_INTERFACE_TABLE_END                                                      \
  NS_INTERFACE_TABLE_TAIL

#define NS_IMPL_QUERY_INTERFACE1(_class, _i1)                                 \
  NS_INTERFACE_TABLE_HEAD(_class)                                             \
  NS_INTERFACE_TABLE_BEGIN                                                    \
    NS_INTERFACE_TABLE_ENTRY(_class, _i1)                                     \
    NS_INTERFACE_TABLE_ENTRY_AMBIGUOUS(_class, nsISupports, _i1)              \
  NS_INTERFACE_TABLE_END                                                      \
  NS_INTERFACE_TABLE_TAIL

#define NS_IMPL_QUERY_INTERFACE2(_class, _i1, _i2)                            \
  NS_INTERFACE_TABLE_HEAD(_class)                                             \
  NS_INTERFACE_TABLE_BEGIN                                                    \
    NS_INTERFACE_TABLE_ENTRY(_class, _i1)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i2)                                     \
    NS_INTERFACE_TABLE_ENTRY_AMBIGUOUS(_class, nsISupports, _i1)              \
  NS_INTERFACE_TABLE_END                                                      \
  NS_INTERFACE_TABLE_TAIL

#define NS_IMPL_QUERY_INTERFACE3(_class, _i1, _i2, _i3)                       \
  NS_INTERFACE_TABLE_HEAD(_class)                                             \
  NS_INTERFACE_TABLE_BEGIN                                                    \
    NS_INTERFACE_TABLE_ENTRY(_class, _i1)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i2)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i3)                                     \
    NS_INTERFACE_TABLE_ENTRY_AMBIGUOUS(_class, nsISupports, _i1)              \
  NS_INTERFACE_TABLE_END                                                      \
  NS_INTERFACE_TABLE_TAIL

#define NS_IMPL_QUERY_INTERFACE4(_class, _i1, _i2, _i3, _i4)                  \
  NS_INTERFACE_TABLE_HEAD(_class)                                             \
  NS_INTERFACE_TABLE_BEGIN                                                    \
    NS_INTERFACE_TABLE_ENTRY(_class, _i1)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i2)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i3)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i4)                                     \
    NS_INTERFACE_TABLE_ENTRY_AMBIGUOUS(_class, nsISupports, _i1)              \
  NS_INTERFACE_TABLE_END                                                      \
  NS_INTERFACE_TABLE_TAIL

#define NS_IMPL_QUERY_INTERFACE5(_class, _i1, _i2, _i3, _i4, _i5)             \
  NS_INTERFACE_TABLE_HEAD(_class)                                             \
  NS_INTERFACE_TABLE_BEGIN                                                    \
    NS_INTERFACE_TABLE_ENTRY(_class, _i1)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i2)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i3)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i4)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i5)                                     \
    NS_INTERFACE_TABLE_ENTRY_AMBIGUOUS(_class, nsISupports, _i1)              \
  NS_INTERFACE_TABLE_END                                                      \
  NS_INTERFACE_TABLE_TAIL

#define NS_IMPL_QUERY_INTERFACE6(_class, _i1, _i2, _i3, _i4, _i5, _i6)        \
  NS_INTERFACE_TABLE_HEAD(_class)                                             \
  NS_INTERFACE_TABLE_BEGIN                                                    \
    NS_INTERFACE_TABLE_ENTRY(_class, _i1)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i2)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i3)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i4)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i5)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i6)                                     \
    NS_INTERFACE_TABLE_ENTRY_AMBIGUOUS(_class, nsISupports, _i1)              \
  NS_INTERFACE_TABLE_END                                                      \
  NS_INTERFACE_TABLE_TAIL

#define NS_IMPL_QUERY_INTERFACE7(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7)   \
  NS_INTERFACE_TABLE_HEAD(_class)                                             \
  NS_INTERFACE_TABLE_BEGIN                                                    \
    NS_INTERFACE_TABLE_ENTRY(_class, _i1)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i2)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i3)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i4)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i5)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i6)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i7)                                     \
    NS_INTERFACE_TABLE_ENTRY_AMBIGUOUS(_class, nsISupports, _i1)              \
  NS_INTERFACE_TABLE_END                                                      \
  NS_INTERFACE_TABLE_TAIL

#define NS_IMPL_QUERY_INTERFACE8(_class, _i1, _i2, _i3, _i4, _i5, _i6,        \
                                 _i7, _i8)                                    \
  NS_INTERFACE_TABLE_HEAD(_class)                                             \
  NS_INTERFACE_TABLE_BEGIN                                                    \
    NS_INTERFACE_TABLE_ENTRY(_class, _i1)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i2)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i3)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i4)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i5)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i6)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i7)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i8)                                     \
    NS_INTERFACE_TABLE_ENTRY_AMBIGUOUS(_class, nsISupports, _i1)              \
  NS_INTERFACE_TABLE_END                                                      \
  NS_INTERFACE_TABLE_TAIL

#define NS_IMPL_QUERY_INTERFACE9(_class, _i1, _i2, _i3, _i4, _i5, _i6,        \
                                 _i7, _i8, _i9)                               \
  NS_INTERFACE_TABLE_HEAD(_class)                                             \
  NS_INTERFACE_TABLE_BEGIN                                                    \
    NS_INTERFACE_TABLE_ENTRY(_class, _i1)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i2)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i3)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i4)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i5)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i6)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i7)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i8)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i9)                                     \
    NS_INTERFACE_TABLE_ENTRY_AMBIGUOUS(_class, nsISupports, _i1)              \
  NS_INTERFACE_TABLE_END                                                      \
  NS_INTERFACE_TABLE_TAIL

#define NS_IMPL_QUERY_INTERFACE10(_class, _i1, _i2, _i3, _i4, _i5, _i6,       \
                                  _i7, _i8, _i9, _i10)                        \
  NS_INTERFACE_TABLE_HEAD(_class)                                             \
  NS_INTERFACE_TABLE_BEGIN                                                    \
    NS_INTERFACE_TABLE_ENTRY(_class, _i1)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i2)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i3)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i4)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i5)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i6)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i7)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i8)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i9)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i10)                                    \
    NS_INTERFACE_TABLE_ENTRY_AMBIGUOUS(_class, nsISupports, _i1)              \
  NS_INTERFACE_TABLE_END                                                      \
  NS_INTERFACE_TABLE_TAIL

#define NS_IMPL_QUERY_INTERFACE11(_class, _i1, _i2, _i3, _i4, _i5, _i6,       \
                                  _i7, _i8, _i9, _i10, _i11)                  \
  NS_INTERFACE_TABLE_HEAD(_class)                                             \
  NS_INTERFACE_TABLE_BEGIN                                                    \
    NS_INTERFACE_TABLE_ENTRY(_class, _i1)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i2)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i3)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i4)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i5)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i6)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i7)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i8)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i9)                                     \
    NS_INTERFACE_TABLE_ENTRY(_class, _i10)                                    \
    NS_INTERFACE_TABLE_ENTRY(_class, _i12)                                    \
    NS_INTERFACE_TABLE_ENTRY_AMBIGUOUS(_class, nsISupports, _i1)              \
  NS_INTERFACE_TABLE_END                                                      \
  NS_INTERFACE_TABLE_TAIL


#define NS_IMPL_THREADSAFE_QUERY_INTERFACE0  NS_IMPL_QUERY_INTERFACE0
#define NS_IMPL_THREADSAFE_QUERY_INTERFACE1  NS_IMPL_QUERY_INTERFACE1
#define NS_IMPL_THREADSAFE_QUERY_INTERFACE2  NS_IMPL_QUERY_INTERFACE2
#define NS_IMPL_THREADSAFE_QUERY_INTERFACE3  NS_IMPL_QUERY_INTERFACE3
#define NS_IMPL_THREADSAFE_QUERY_INTERFACE4  NS_IMPL_QUERY_INTERFACE4
#define NS_IMPL_THREADSAFE_QUERY_INTERFACE5  NS_IMPL_QUERY_INTERFACE5
#define NS_IMPL_THREADSAFE_QUERY_INTERFACE6  NS_IMPL_QUERY_INTERFACE6
#define NS_IMPL_THREADSAFE_QUERY_INTERFACE7  NS_IMPL_QUERY_INTERFACE7
#define NS_IMPL_THREADSAFE_QUERY_INTERFACE8  NS_IMPL_QUERY_INTERFACE8
#define NS_IMPL_THREADSAFE_QUERY_INTERFACE9  NS_IMPL_QUERY_INTERFACE9
#define NS_IMPL_THREADSAFE_QUERY_INTERFACE10  NS_IMPL_QUERY_INTERFACE10
#define NS_IMPL_THREADSAFE_QUERY_INTERFACE11  NS_IMPL_QUERY_INTERFACE11

#define NS_IMPL_QUERY_INTERFACE_INHERITED7(Class,Super,i1,i2,i3,i4,i5,i6,i7)  \
  NS_IMPL_QUERY_HEAD(Class)                                                   \
  NS_IMPL_QUERY_BODY(i1)                                                      \
  NS_IMPL_QUERY_BODY(i2)                                                      \
  NS_IMPL_QUERY_BODY(i3)                                                      \
  NS_IMPL_QUERY_BODY(i4)                                                      \
  NS_IMPL_QUERY_BODY(i5)                                                      \
  NS_IMPL_QUERY_BODY(i6)                                                      \
  NS_IMPL_QUERY_BODY(i7)                                                      \
  NS_IMPL_QUERY_TAIL_INHERITING(Super)                                        \

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
#define NS_DECL_ISUPPORTS_INHERITED                                           \
public:                                                                       \
  NS_IMETHOD QueryInterface(REFNSIID aIID,                                    \
                            void** aInstancePtr);                             \
  NS_IMETHOD_(nsrefcnt) AddRef(void);                                         \
  NS_IMETHOD_(nsrefcnt) Release(void);                                        \

/**
 * These macros can be used in conjunction with NS_DECL_ISUPPORTS_INHERITED
 * to implement the nsISupports methods, forwarding the invocations to a
 * superclass that already implements nsISupports.
 *
 * Note that I didn't make these inlined because they're virtual methods.
 */

#define NS_IMPL_ADDREF_INHERITED(Class, Super)                                \
NS_IMETHODIMP_(nsrefcnt) Class::AddRef(void)                                  \
{                                                                             \
  return Super::AddRef();                                                     \
}                                                                             \

#define NS_IMPL_RELEASE_INHERITED(Class, Super)                               \
NS_IMETHODIMP_(nsrefcnt) Class::Release(void)                                 \
{                                                                             \
  return Super::Release();                                                    \
}                                                                             \

#define NS_IMPL_QUERY_INTERFACE_INHERITED0(Class, Super)                      \
  NS_INTERFACE_TABLE_HEAD(Class)                                              \
  NS_INTERFACE_TABLE_TAIL_INHERITING(Super)

#define NS_IMPL_QUERY_INTERFACE_INHERITED1(Class, Super, i1)                  \
  NS_INTERFACE_TABLE_HEAD(Class)                                              \
  NS_INTERFACE_TABLE_BEGIN                                                    \
    NS_INTERFACE_TABLE_ENTRY(Class, i1)                                       \
  NS_INTERFACE_TABLE_END                                                      \
  NS_INTERFACE_TABLE_TAIL_INHERITING(Super)

#define NS_IMPL_QUERY_INTERFACE_INHERITED2(Class, Super, i1, i2)              \
  NS_INTERFACE_TABLE_HEAD(Class)                                              \
  NS_INTERFACE_TABLE_BEGIN                                                    \
    NS_INTERFACE_TABLE_ENTRY(Class, i1)                                       \
    NS_INTERFACE_TABLE_ENTRY(Class, i2)                                       \
  NS_INTERFACE_TABLE_END                                                      \
  NS_INTERFACE_TABLE_TAIL_INHERITING(Super)

#define NS_IMPL_QUERY_INTERFACE_INHERITED3(Class, Super, i1, i2, i3)          \
  NS_INTERFACE_TABLE_HEAD(Class)                                              \
  NS_INTERFACE_TABLE_BEGIN                                                    \
    NS_INTERFACE_TABLE_ENTRY(Class, i1)                                       \
    NS_INTERFACE_TABLE_ENTRY(Class, i2)                                       \
    NS_INTERFACE_TABLE_ENTRY(Class, i3)                                       \
  NS_INTERFACE_TABLE_END                                                      \
  NS_INTERFACE_TABLE_TAIL_INHERITING(Super)

#define NS_IMPL_QUERY_INTERFACE_INHERITED4(Class, Super, i1, i2, i3, i4)      \
  NS_INTERFACE_TABLE_HEAD(Class)                                              \
  NS_INTERFACE_TABLE_BEGIN                                                    \
    NS_INTERFACE_TABLE_ENTRY(Class, i1)                                       \
    NS_INTERFACE_TABLE_ENTRY(Class, i2)                                       \
    NS_INTERFACE_TABLE_ENTRY(Class, i3)                                       \
    NS_INTERFACE_TABLE_ENTRY(Class, i4)                                       \
  NS_INTERFACE_TABLE_END                                                      \
  NS_INTERFACE_TABLE_TAIL_INHERITING(Super)

#define NS_IMPL_QUERY_INTERFACE_INHERITED5(Class,Super,i1,i2,i3,i4,i5)        \
  NS_INTERFACE_TABLE_HEAD(Class)                                              \
  NS_INTERFACE_TABLE_BEGIN                                                    \
    NS_INTERFACE_TABLE_ENTRY(Class, i1)                                       \
    NS_INTERFACE_TABLE_ENTRY(Class, i2)                                       \
    NS_INTERFACE_TABLE_ENTRY(Class, i3)                                       \
    NS_INTERFACE_TABLE_ENTRY(Class, i4)                                       \
    NS_INTERFACE_TABLE_ENTRY(Class, i5)                                       \
  NS_INTERFACE_TABLE_END                                                      \
  NS_INTERFACE_TABLE_TAIL_INHERITING(Super)

#define NS_IMPL_QUERY_INTERFACE_INHERITED6(Class,Super,i1,i2,i3,i4,i5,i6)     \
  NS_INTERFACE_TABLE_HEAD(Class)                                              \
  NS_INTERFACE_TABLE_BEGIN                                                    \
    NS_INTERFACE_TABLE_ENTRY(Class, i1)                                       \
    NS_INTERFACE_TABLE_ENTRY(Class, i2)                                       \
    NS_INTERFACE_TABLE_ENTRY(Class, i3)                                       \
    NS_INTERFACE_TABLE_ENTRY(Class, i4)                                       \
    NS_INTERFACE_TABLE_ENTRY(Class, i5)                                       \
    NS_INTERFACE_TABLE_ENTRY(Class, i6)                                       \
  NS_INTERFACE_TABLE_END                                                      \
  NS_INTERFACE_TABLE_TAIL_INHERITING(Super)

/**
 * Convenience macros for implementing all nsISupports methods for
 * a simple class.
 * @param _class The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
 */

#define NS_IMPL_ISUPPORTS0(_class)                                            \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE0(_class)

#define NS_IMPL_ISUPPORTS1(_class, _interface)                                \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE1(_class, _interface)

#define NS_IMPL_ISUPPORTS2(_class, _i1, _i2)                                  \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE2(_class, _i1, _i2)

#define NS_IMPL_ISUPPORTS3(_class, _i1, _i2, _i3)                             \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE3(_class, _i1, _i2, _i3)

#define NS_IMPL_ISUPPORTS4(_class, _i1, _i2, _i3, _i4)                        \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE4(_class, _i1, _i2, _i3, _i4)

#define NS_IMPL_ISUPPORTS5(_class, _i1, _i2, _i3, _i4, _i5)                   \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE5(_class, _i1, _i2, _i3, _i4, _i5)

#define NS_IMPL_ISUPPORTS6(_class, _i1, _i2, _i3, _i4, _i5, _i6)              \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE6(_class, _i1, _i2, _i3, _i4, _i5, _i6)

#define NS_IMPL_ISUPPORTS7(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7)         \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE7(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7)

#define NS_IMPL_ISUPPORTS8(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7, _i8)    \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE8(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7, _i8)

#define NS_IMPL_ISUPPORTS9(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7, _i8,    \
                           _i9)                                               \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE9(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7, _i8, _i9)

#define NS_IMPL_ISUPPORTS10(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7, _i8,   \
                            _i9, _i10)                                        \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE10(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7, _i8,   \
                            _i9, _i10)

#define NS_IMPL_ISUPPORTS11(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7, _i8,   \
                            _i9, _i10, _i11)                                  \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE11(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7, _i8,   \
                            _i9, _i10, _i11)

#define NS_IMPL_ISUPPORTS_INHERITED0(Class, Super)                            \
    NS_IMPL_QUERY_INTERFACE_INHERITED0(Class, Super)                          \
    NS_IMPL_ADDREF_INHERITED(Class, Super)                                    \
    NS_IMPL_RELEASE_INHERITED(Class, Super)                                   \

#define NS_IMPL_ISUPPORTS_INHERITED1(Class, Super, i1)                        \
    NS_IMPL_QUERY_INTERFACE_INHERITED1(Class, Super, i1)                      \
    NS_IMPL_ADDREF_INHERITED(Class, Super)                                    \
    NS_IMPL_RELEASE_INHERITED(Class, Super)                                   \

#define NS_IMPL_ISUPPORTS_INHERITED2(Class, Super, i1, i2)                    \
    NS_IMPL_QUERY_INTERFACE_INHERITED2(Class, Super, i1, i2)                  \
    NS_IMPL_ADDREF_INHERITED(Class, Super)                                    \
    NS_IMPL_RELEASE_INHERITED(Class, Super)                                   \

#define NS_IMPL_ISUPPORTS_INHERITED3(Class, Super, i1, i2, i3)                \
    NS_IMPL_QUERY_INTERFACE_INHERITED3(Class, Super, i1, i2, i3)              \
    NS_IMPL_ADDREF_INHERITED(Class, Super)                                    \
    NS_IMPL_RELEASE_INHERITED(Class, Super)                                   \

#define NS_IMPL_ISUPPORTS_INHERITED4(Class, Super, i1, i2, i3, i4)            \
    NS_IMPL_QUERY_INTERFACE_INHERITED4(Class, Super, i1, i2, i3, i4)          \
    NS_IMPL_ADDREF_INHERITED(Class, Super)                                    \
    NS_IMPL_RELEASE_INHERITED(Class, Super)                                   \

#define NS_IMPL_ISUPPORTS_INHERITED5(Class, Super, i1, i2, i3, i4, i5)        \
    NS_IMPL_QUERY_INTERFACE_INHERITED5(Class, Super, i1, i2, i3, i4, i5)      \
    NS_IMPL_ADDREF_INHERITED(Class, Super)                                    \
    NS_IMPL_RELEASE_INHERITED(Class, Super)                                   \

#define NS_IMPL_ISUPPORTS_INHERITED6(Class, Super, i1, i2, i3, i4, i5, i6)    \
    NS_IMPL_QUERY_INTERFACE_INHERITED6(Class, Super, i1, i2, i3, i4, i5, i6)  \
    NS_IMPL_ADDREF_INHERITED(Class, Super)                                    \
    NS_IMPL_RELEASE_INHERITED(Class, Super)                                   \

#define NS_IMPL_ISUPPORTS_INHERITED7(Class, Super, i1, i2, i3, i4, i5, i6, i7) \
    NS_IMPL_QUERY_INTERFACE_INHERITED7(Class, Super, i1, i2, i3, i4, i5, i6, i7) \
    NS_IMPL_ADDREF_INHERITED(Class, Super)                                    \
    NS_IMPL_RELEASE_INHERITED(Class, Super)                                   \

///////////////////////////////////////////////////////////////////////////////
/**
 *
 * Threadsafe implementations of the ISupports convenience macros.
 *
 * @note  These are not available when linking against the standalone glue,
 *        because the implementation requires PR_ symbols.
 */

#if !defined(XPCOM_GLUE_AVOID_NSPR)

/**
 * Use this macro to implement the AddRef method for a given <i>_class</i>
 * @param _class The name of the class implementing the method
 */

#define NS_IMPL_THREADSAFE_ADDREF(_class)                                     \
NS_IMETHODIMP_(nsrefcnt) _class::AddRef(void)                                 \
{                                                                             \
  NS_PRECONDITION(PRInt32(mRefCnt) >= 0, "illegal refcnt");                   \
  nsrefcnt count;                                                             \
  count = PR_AtomicIncrement((PRInt32*)&mRefCnt);                             \
  NS_LOG_ADDREF(this, count, #_class, sizeof(*this));                         \
  return count;                                                               \
}

/**
 * Use this macro to implement the Release method for a given <i>_class</i>
 * @param _class The name of the class implementing the method
 */

#define NS_IMPL_THREADSAFE_RELEASE(_class)                                    \
NS_IMETHODIMP_(nsrefcnt) _class::Release(void)                                \
{                                                                             \
  nsrefcnt count;                                                             \
  NS_PRECONDITION(0 != mRefCnt, "dup release");                               \
  count = PR_AtomicDecrement((PRInt32 *)&mRefCnt);                            \
  NS_LOG_RELEASE(this, count, #_class);                                       \
  if (0 == count) {                                                           \
    mRefCnt = 1; /* stabilize */                                              \
    /* enable this to find non-threadsafe destructors: */                     \
    /* NS_ASSERT_OWNINGTHREAD(_class); */                                     \
    NS_DELETEXPCOM(this);                                                     \
    return 0;                                                                 \
  }                                                                           \
  return count;                                                               \
}

#else // XPCOM_GLUE_AVOID_NSPR

#define NS_IMPL_THREADSAFE_ADDREF(_class)                                     \
  THREADSAFE_ISUPPORTS_NOT_AVAILABLE_IN_STANDALONE_GLUE;

#define NS_IMPL_THREADSAFE_RELEASE(_class)                                    \
  THREADSAFE_ISUPPORTS_NOT_AVAILABLE_IN_STANDALONE_GLUE;

#endif

#define NS_IMPL_THREADSAFE_ISUPPORTS0(_class)                                 \
  NS_IMPL_THREADSAFE_ADDREF(_class)                                           \
  NS_IMPL_THREADSAFE_RELEASE(_class)                                          \
  NS_IMPL_THREADSAFE_QUERY_INTERFACE0(_class)

#define NS_IMPL_THREADSAFE_ISUPPORTS1(_class, _interface)                     \
  NS_IMPL_THREADSAFE_ADDREF(_class)                                           \
  NS_IMPL_THREADSAFE_RELEASE(_class)                                          \
  NS_IMPL_THREADSAFE_QUERY_INTERFACE1(_class, _interface)

#define NS_IMPL_THREADSAFE_ISUPPORTS2(_class, _i1, _i2)                       \
  NS_IMPL_THREADSAFE_ADDREF(_class)                                           \
  NS_IMPL_THREADSAFE_RELEASE(_class)                                          \
  NS_IMPL_THREADSAFE_QUERY_INTERFACE2(_class, _i1, _i2)

#define NS_IMPL_THREADSAFE_ISUPPORTS3(_class, _i1, _i2, _i3)                  \
  NS_IMPL_THREADSAFE_ADDREF(_class)                                           \
  NS_IMPL_THREADSAFE_RELEASE(_class)                                          \
  NS_IMPL_THREADSAFE_QUERY_INTERFACE3(_class, _i1, _i2, _i3)

#define NS_IMPL_THREADSAFE_ISUPPORTS4(_class, _i1, _i2, _i3, _i4)             \
  NS_IMPL_THREADSAFE_ADDREF(_class)                                           \
  NS_IMPL_THREADSAFE_RELEASE(_class)                                          \
  NS_IMPL_THREADSAFE_QUERY_INTERFACE4(_class, _i1, _i2, _i3, _i4)

#define NS_IMPL_THREADSAFE_ISUPPORTS5(_class, _i1, _i2, _i3, _i4, _i5)        \
  NS_IMPL_THREADSAFE_ADDREF(_class)                                           \
  NS_IMPL_THREADSAFE_RELEASE(_class)                                          \
  NS_IMPL_THREADSAFE_QUERY_INTERFACE5(_class, _i1, _i2, _i3, _i4, _i5)

#define NS_IMPL_THREADSAFE_ISUPPORTS6(_class, _i1, _i2, _i3, _i4, _i5, _i6)   \
  NS_IMPL_THREADSAFE_ADDREF(_class)                                           \
  NS_IMPL_THREADSAFE_RELEASE(_class)                                          \
  NS_IMPL_THREADSAFE_QUERY_INTERFACE6(_class, _i1, _i2, _i3, _i4, _i5, _i6)

#define NS_IMPL_THREADSAFE_ISUPPORTS7(_class, _i1, _i2, _i3, _i4, _i5, _i6,   \
                                      _i7)                                    \
  NS_IMPL_THREADSAFE_ADDREF(_class)                                           \
  NS_IMPL_THREADSAFE_RELEASE(_class)                                          \
  NS_IMPL_THREADSAFE_QUERY_INTERFACE7(_class, _i1, _i2, _i3, _i4, _i5, _i6,   \
                                      _i7)

#define NS_IMPL_THREADSAFE_ISUPPORTS8(_class, _i1, _i2, _i3, _i4, _i5, _i6,   \
                                      _i7, _i8)                               \
  NS_IMPL_THREADSAFE_ADDREF(_class)                                           \
  NS_IMPL_THREADSAFE_RELEASE(_class)                                          \
  NS_IMPL_THREADSAFE_QUERY_INTERFACE8(_class, _i1, _i2, _i3, _i4, _i5, _i6,   \
                                      _i7, _i8)

#define NS_IMPL_THREADSAFE_ISUPPORTS9(_class, _i1, _i2, _i3, _i4, _i5, _i6,   \
                                      _i7, _i8, _i9)                          \
  NS_IMPL_THREADSAFE_ADDREF(_class)                                           \
  NS_IMPL_THREADSAFE_RELEASE(_class)                                          \
  NS_IMPL_THREADSAFE_QUERY_INTERFACE9(_class, _i1, _i2, _i3, _i4, _i5, _i6,   \
                                      _i7, _i8, _i9)

#define NS_IMPL_THREADSAFE_ISUPPORTS10(_class, _i1, _i2, _i3, _i4, _i5, _i6,  \
                                       _i7, _i8, _i9, _i10)                   \
  NS_IMPL_THREADSAFE_ADDREF(_class)                                           \
  NS_IMPL_THREADSAFE_RELEASE(_class)                                          \
  NS_IMPL_THREADSAFE_QUERY_INTERFACE10(_class, _i1, _i2, _i3, _i4, _i5, _i6,  \
                                       _i7, _i8, _i9, _i10)

#define NS_IMPL_THREADSAFE_ISUPPORTS11(_class, _i1, _i2, _i3, _i4, _i5, _i6,  \
                                       _i7, _i8, _i9, _i10, _i11)             \
  NS_IMPL_THREADSAFE_ADDREF(_class)                                           \
  NS_IMPL_THREADSAFE_RELEASE(_class)                                          \
  NS_IMPL_THREADSAFE_QUERY_INTERFACE11(_class, _i1, _i2, _i3, _i4, _i5, _i6,  \
                                       _i7, _i8, _i9, _i10, _i11)

///////////////////////////////////////////////////////////////////////////////
// Macros for implementing nsIClassInfo-related stuff.
///////////////////////////////////////////////////////////////////////////////

// include here instead of at the top because it requires the nsISupport decl
#include "nsIClassInfo.h"

#define NS_CLASSINFO_NAME(_class) _class##_classInfoGlobal
#define NS_CI_INTERFACE_GETTER_NAME(_class) _class##_GetInterfacesHelper

#define NS_DECL_CI_INTERFACE_GETTER(_class)                                   \
  extern NS_IMETHODIMP NS_CI_INTERFACE_GETTER_NAME(_class)(PRUint32 *,        \
                                                           nsIID ***);

#define NS_DECL_CLASSINFO(_class)                                             \
  NS_DECL_CI_INTERFACE_GETTER(_class)                                         \
  nsIClassInfo *NS_CLASSINFO_NAME(_class);

#define NS_IMPL_QUERY_CLASSINFO(_class)                                       \
  if ( aIID.Equals(NS_GET_IID(nsIClassInfo)) ) {                              \
    extern nsIClassInfo *NS_CLASSINFO_NAME(_class);                           \
    foundInterface = NS_STATIC_CAST(nsIClassInfo*, NS_CLASSINFO_NAME(_class));\
  } else

#define NS_CLASSINFO_HELPER_BEGIN(_class, _c)                                 \
NS_IMETHODIMP                                                                 \
NS_CI_INTERFACE_GETTER_NAME(_class)(PRUint32 *count, nsIID ***array)          \
{                                                                             \
    *count = _c;                                                              \
    *array = (nsIID **)nsMemory::Alloc(sizeof (nsIID *) * _c);

#define NS_CLASSINFO_HELPER_ENTRY(_i, _interface)                             \
    (*array)[_i] = (nsIID *)nsMemory::Clone(&NS_GET_IID(_interface),          \
                                            sizeof(nsIID));

#define NS_CLASSINFO_HELPER_END                                               \
    return NS_OK;                                                             \
}

#define NS_IMPL_CI_INTERFACE_GETTER1(_class, _interface)                      \
   NS_CLASSINFO_HELPER_BEGIN(_class, 1)                                       \
     NS_CLASSINFO_HELPER_ENTRY(0, _interface)                                 \
   NS_CLASSINFO_HELPER_END

#define NS_IMPL_QUERY_INTERFACE1_CI(_class, _i1)                              \
  NS_INTERFACE_MAP_BEGIN(_class)                                              \
    NS_INTERFACE_MAP_ENTRY(_i1)                                               \
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, _i1)                        \
    NS_IMPL_QUERY_CLASSINFO(_class)                                           \
  NS_INTERFACE_MAP_END

#define NS_IMPL_ISUPPORTS1_CI(_class, _interface)                             \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE1_CI(_class, _interface)                             \
  NS_IMPL_CI_INTERFACE_GETTER1(_class, _interface)

#define NS_IMPL_CI_INTERFACE_GETTER2(_class, _i1, _i2)                        \
   NS_CLASSINFO_HELPER_BEGIN(_class, 2)                                       \
     NS_CLASSINFO_HELPER_ENTRY(0, _i1)                                        \
     NS_CLASSINFO_HELPER_ENTRY(1, _i2)                                        \
   NS_CLASSINFO_HELPER_END

#define NS_IMPL_QUERY_INTERFACE2_CI(_class, _i1, _i2)                         \
  NS_INTERFACE_MAP_BEGIN(_class)                                              \
    NS_INTERFACE_MAP_ENTRY(_i1)                                               \
    NS_INTERFACE_MAP_ENTRY(_i2)                                               \
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, _i1)                        \
    NS_IMPL_QUERY_CLASSINFO(_class)                                           \
  NS_INTERFACE_MAP_END

#define NS_IMPL_ISUPPORTS2_CI(_class, _i1, _i2)                               \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE2_CI(_class, _i1, _i2)                               \
  NS_IMPL_CI_INTERFACE_GETTER2(_class, _i1, _i2)

#define NS_IMPL_CI_INTERFACE_GETTER3(_class, _i1, _i2, _i3)                   \
   NS_CLASSINFO_HELPER_BEGIN(_class, 3)                                       \
     NS_CLASSINFO_HELPER_ENTRY(0, _i1)                                        \
     NS_CLASSINFO_HELPER_ENTRY(1, _i2)                                        \
     NS_CLASSINFO_HELPER_ENTRY(2, _i3)                                        \
   NS_CLASSINFO_HELPER_END

#define NS_IMPL_QUERY_INTERFACE3_CI(_class, _i1, _i2, _i3)                    \
  NS_INTERFACE_MAP_BEGIN(_class)                                              \
    NS_INTERFACE_MAP_ENTRY(_i1)                                               \
    NS_INTERFACE_MAP_ENTRY(_i2)                                               \
    NS_INTERFACE_MAP_ENTRY(_i3)                                               \
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, _i1)                        \
    NS_IMPL_QUERY_CLASSINFO(_class)                                           \
  NS_INTERFACE_MAP_END

#define NS_IMPL_ISUPPORTS3_CI(_class, _i1, _i2, _i3)                          \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE3_CI(_class, _i1, _i2, _i3)                          \
  NS_IMPL_CI_INTERFACE_GETTER3(_class, _i1, _i2, _i3)

#define NS_IMPL_CI_INTERFACE_GETTER4(_class, _i1, _i2, _i3, _i4)              \
   NS_CLASSINFO_HELPER_BEGIN(_class, 4)                                       \
     NS_CLASSINFO_HELPER_ENTRY(0, _i1)                                        \
     NS_CLASSINFO_HELPER_ENTRY(1, _i2)                                        \
     NS_CLASSINFO_HELPER_ENTRY(2, _i3)                                        \
     NS_CLASSINFO_HELPER_ENTRY(3, _i4)                                        \
   NS_CLASSINFO_HELPER_END

#define NS_IMPL_QUERY_INTERFACE4_CI(_class, _i1, _i2, _i3, _i4)               \
  NS_INTERFACE_MAP_BEGIN(_class)                                              \
    NS_INTERFACE_MAP_ENTRY(_i1)                                               \
    NS_INTERFACE_MAP_ENTRY(_i2)                                               \
    NS_INTERFACE_MAP_ENTRY(_i3)                                               \
    NS_INTERFACE_MAP_ENTRY(_i4)                                               \
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, _i1)                        \
    NS_IMPL_QUERY_CLASSINFO(_class)                                           \
  NS_INTERFACE_MAP_END

#define NS_IMPL_ISUPPORTS4_CI(_class, _i1, _i2, _i3, _i4)                     \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE4_CI(_class, _i1, _i2, _i3, _i4)                     \
  NS_IMPL_CI_INTERFACE_GETTER4(_class, _i1, _i2, _i3, _i4)

#define NS_IMPL_CI_INTERFACE_GETTER5(_class, _i1, _i2, _i3, _i4, _i5)         \
   NS_CLASSINFO_HELPER_BEGIN(_class, 5)                                       \
     NS_CLASSINFO_HELPER_ENTRY(0, _i1)                                        \
     NS_CLASSINFO_HELPER_ENTRY(1, _i2)                                        \
     NS_CLASSINFO_HELPER_ENTRY(2, _i3)                                        \
     NS_CLASSINFO_HELPER_ENTRY(3, _i4)                                        \
     NS_CLASSINFO_HELPER_ENTRY(4, _i5)                                        \
   NS_CLASSINFO_HELPER_END

#define NS_IMPL_QUERY_INTERFACE5_CI(_class, _i1, _i2, _i3, _i4, _i5)          \
  NS_INTERFACE_MAP_BEGIN(_class)                                              \
    NS_INTERFACE_MAP_ENTRY(_i1)                                               \
    NS_INTERFACE_MAP_ENTRY(_i2)                                               \
    NS_INTERFACE_MAP_ENTRY(_i3)                                               \
    NS_INTERFACE_MAP_ENTRY(_i4)                                               \
    NS_INTERFACE_MAP_ENTRY(_i5)                                               \
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, _i1)                        \
    NS_IMPL_QUERY_CLASSINFO(_class)                                           \
  NS_INTERFACE_MAP_END

#define NS_IMPL_ISUPPORTS5_CI(_class, _i1, _i2, _i3, _i4, _i5)                \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE5_CI(_class, _i1, _i2, _i3, _i4, _i5)                \
  NS_IMPL_CI_INTERFACE_GETTER5(_class, _i1, _i2, _i3, _i4, _i5)

#define NS_IMPL_CI_INTERFACE_GETTER6(_class, _i1, _i2, _i3, _i4, _i5, _i6)    \
   NS_CLASSINFO_HELPER_BEGIN(_class, 6)                                       \
     NS_CLASSINFO_HELPER_ENTRY(0, _i1)                                        \
     NS_CLASSINFO_HELPER_ENTRY(1, _i2)                                        \
     NS_CLASSINFO_HELPER_ENTRY(2, _i3)                                        \
     NS_CLASSINFO_HELPER_ENTRY(3, _i4)                                        \
     NS_CLASSINFO_HELPER_ENTRY(4, _i5)                                        \
     NS_CLASSINFO_HELPER_ENTRY(5, _i6)                                        \
   NS_CLASSINFO_HELPER_END

#define NS_IMPL_QUERY_INTERFACE6_CI(_class, _i1, _i2, _i3, _i4, _i5, _i6)     \
  NS_INTERFACE_MAP_BEGIN(_class)                                              \
    NS_INTERFACE_MAP_ENTRY(_i1)                                               \
    NS_INTERFACE_MAP_ENTRY(_i2)                                               \
    NS_INTERFACE_MAP_ENTRY(_i3)                                               \
    NS_INTERFACE_MAP_ENTRY(_i4)                                               \
    NS_INTERFACE_MAP_ENTRY(_i5)                                               \
    NS_INTERFACE_MAP_ENTRY(_i6)                                               \
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, _i1)                        \
    NS_IMPL_QUERY_CLASSINFO(_class)                                           \
  NS_INTERFACE_MAP_END

#define NS_IMPL_ISUPPORTS6_CI(_class, _i1, _i2, _i3, _i4, _i5, _i6)           \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE6_CI(_class, _i1, _i2, _i3, _i4, _i5, _i6)           \
  NS_IMPL_CI_INTERFACE_GETTER6(_class, _i1, _i2, _i3, _i4, _i5, _i6)

#define NS_IMPL_CI_INTERFACE_GETTER7(_class, _i1, _i2, _i3, _i4, _i5, _i6,    \
                                     _i7)                                     \
   NS_CLASSINFO_HELPER_BEGIN(_class, 7)                                       \
     NS_CLASSINFO_HELPER_ENTRY(0, _i1)                                        \
     NS_CLASSINFO_HELPER_ENTRY(1, _i2)                                        \
     NS_CLASSINFO_HELPER_ENTRY(2, _i3)                                        \
     NS_CLASSINFO_HELPER_ENTRY(3, _i4)                                        \
     NS_CLASSINFO_HELPER_ENTRY(4, _i5)                                        \
     NS_CLASSINFO_HELPER_ENTRY(5, _i6)                                        \
     NS_CLASSINFO_HELPER_ENTRY(6, _i7)                                        \
   NS_CLASSINFO_HELPER_END

#define NS_IMPL_QUERY_INTERFACE7_CI(_class, _i1, _i2, _i3, _i4, _i5, _i6,     \
                                    _i7)                                      \
  NS_INTERFACE_MAP_BEGIN(_class)                                              \
    NS_INTERFACE_MAP_ENTRY(_i1)                                               \
    NS_INTERFACE_MAP_ENTRY(_i2)                                               \
    NS_INTERFACE_MAP_ENTRY(_i3)                                               \
    NS_INTERFACE_MAP_ENTRY(_i4)                                               \
    NS_INTERFACE_MAP_ENTRY(_i5)                                               \
    NS_INTERFACE_MAP_ENTRY(_i6)                                               \
    NS_INTERFACE_MAP_ENTRY(_i7)                                               \
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, _i1)                        \
    NS_IMPL_QUERY_CLASSINFO(_class)                                           \
  NS_INTERFACE_MAP_END

#define NS_IMPL_ISUPPORTS7_CI(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7)      \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE7_CI(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7)      \
  NS_IMPL_CI_INTERFACE_GETTER7(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7)

#define NS_IMPL_CI_INTERFACE_GETTER8(_class, _i1, _i2, _i3, _i4, _i5, _i6,    \
                                     _i7, _i8)                                \
   NS_CLASSINFO_HELPER_BEGIN(_class, 8)                                       \
     NS_CLASSINFO_HELPER_ENTRY(0, _i1)                                        \
     NS_CLASSINFO_HELPER_ENTRY(1, _i2)                                        \
     NS_CLASSINFO_HELPER_ENTRY(2, _i3)                                        \
     NS_CLASSINFO_HELPER_ENTRY(3, _i4)                                        \
     NS_CLASSINFO_HELPER_ENTRY(4, _i5)                                        \
     NS_CLASSINFO_HELPER_ENTRY(5, _i6)                                        \
     NS_CLASSINFO_HELPER_ENTRY(6, _i7)                                        \
     NS_CLASSINFO_HELPER_ENTRY(7, _i8)                                        \
   NS_CLASSINFO_HELPER_END

#define NS_IMPL_QUERY_INTERFACE8_CI(_class, _i1, _i2, _i3, _i4, _i5, _i6,     \
                                    _i7, _i8)                                 \
  NS_INTERFACE_MAP_BEGIN(_class)                                              \
    NS_INTERFACE_MAP_ENTRY(_i1)                                               \
    NS_INTERFACE_MAP_ENTRY(_i2)                                               \
    NS_INTERFACE_MAP_ENTRY(_i3)                                               \
    NS_INTERFACE_MAP_ENTRY(_i4)                                               \
    NS_INTERFACE_MAP_ENTRY(_i5)                                               \
    NS_INTERFACE_MAP_ENTRY(_i6)                                               \
    NS_INTERFACE_MAP_ENTRY(_i7)                                               \
    NS_INTERFACE_MAP_ENTRY(_i8)                                               \
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, _i1)                        \
    NS_IMPL_QUERY_CLASSINFO(_class)                                           \
  NS_INTERFACE_MAP_END

#define NS_IMPL_ISUPPORTS8_CI(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7, _i8) \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE8_CI(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7, _i8) \
  NS_IMPL_CI_INTERFACE_GETTER8(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7, _i8)

#define NS_IMPL_CI_INTERFACE_GETTER9(_class, _i1, _i2, _i3, _i4, _i5, _i6,    \
                                     _i7, _i8, _i9)                           \
   NS_CLASSINFO_HELPER_BEGIN(_class, 9)                                       \
     NS_CLASSINFO_HELPER_ENTRY(0, _i1)                                        \
     NS_CLASSINFO_HELPER_ENTRY(1, _i2)                                        \
     NS_CLASSINFO_HELPER_ENTRY(2, _i3)                                        \
     NS_CLASSINFO_HELPER_ENTRY(3, _i4)                                        \
     NS_CLASSINFO_HELPER_ENTRY(4, _i5)                                        \
     NS_CLASSINFO_HELPER_ENTRY(5, _i6)                                        \
     NS_CLASSINFO_HELPER_ENTRY(6, _i7)                                        \
     NS_CLASSINFO_HELPER_ENTRY(7, _i8)                                        \
     NS_CLASSINFO_HELPER_ENTRY(8, _i9)                                        \
   NS_CLASSINFO_HELPER_END

#define NS_IMPL_QUERY_INTERFACE9_CI(_class, _i1, _i2, _i3, _i4, _i5, _i6,     \
                                    _i7, _i8, _i9)                            \
  NS_INTERFACE_MAP_BEGIN(_class)                                              \
    NS_INTERFACE_MAP_ENTRY(_i1)                                               \
    NS_INTERFACE_MAP_ENTRY(_i2)                                               \
    NS_INTERFACE_MAP_ENTRY(_i3)                                               \
    NS_INTERFACE_MAP_ENTRY(_i4)                                               \
    NS_INTERFACE_MAP_ENTRY(_i5)                                               \
    NS_INTERFACE_MAP_ENTRY(_i6)                                               \
    NS_INTERFACE_MAP_ENTRY(_i7)                                               \
    NS_INTERFACE_MAP_ENTRY(_i8)                                               \
    NS_INTERFACE_MAP_ENTRY(_i9)                                               \
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, _i1)                        \
    NS_IMPL_QUERY_CLASSINFO(_class)                                           \
  NS_INTERFACE_MAP_END

#define NS_IMPL_ISUPPORTS9_CI(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7,      \
                              _i8, _i9)                                       \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE9_CI(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7,      \
                              _i8, _i9)                                       \
  NS_IMPL_CI_INTERFACE_GETTER9(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7,     \
                               _i8, _i9)

#define NS_IMPL_CI_INTERFACE_GETTER10(_class, _i1, _i2, _i3, _i4, _i5, _i6,   \
                                      _i7, _i8, _i9, _i10)                    \
   NS_CLASSINFO_HELPER_BEGIN(_class, 10)                                      \
     NS_CLASSINFO_HELPER_ENTRY(0, _i1)                                        \
     NS_CLASSINFO_HELPER_ENTRY(1, _i2)                                        \
     NS_CLASSINFO_HELPER_ENTRY(2, _i3)                                        \
     NS_CLASSINFO_HELPER_ENTRY(3, _i4)                                        \
     NS_CLASSINFO_HELPER_ENTRY(4, _i5)                                        \
     NS_CLASSINFO_HELPER_ENTRY(5, _i6)                                        \
     NS_CLASSINFO_HELPER_ENTRY(6, _i7)                                        \
     NS_CLASSINFO_HELPER_ENTRY(7, _i8)                                        \
     NS_CLASSINFO_HELPER_ENTRY(8, _i9)                                        \
     NS_CLASSINFO_HELPER_ENTRY(9, _i10)                                       \
   NS_CLASSINFO_HELPER_END

#define NS_IMPL_CI_INTERFACE_GETTER11(_class, _i1, _i2, _i3, _i4, _i5, _i6,   \
                                      _i7, _i8, _i9, _i10, _i11)              \
   NS_CLASSINFO_HELPER_BEGIN(_class, 11)                                      \
     NS_CLASSINFO_HELPER_ENTRY(0, _i1)                                        \
     NS_CLASSINFO_HELPER_ENTRY(1, _i2)                                        \
     NS_CLASSINFO_HELPER_ENTRY(2, _i3)                                        \
     NS_CLASSINFO_HELPER_ENTRY(3, _i4)                                        \
     NS_CLASSINFO_HELPER_ENTRY(4, _i5)                                        \
     NS_CLASSINFO_HELPER_ENTRY(5, _i6)                                        \
     NS_CLASSINFO_HELPER_ENTRY(6, _i7)                                        \
     NS_CLASSINFO_HELPER_ENTRY(7, _i8)                                        \
     NS_CLASSINFO_HELPER_ENTRY(8, _i9)                                        \
     NS_CLASSINFO_HELPER_ENTRY(9, _i10)                                       \
     NS_CLASSINFO_HELPER_ENTRY(10, _i11)                                      \
   NS_CLASSINFO_HELPER_END

#define NS_IMPL_QUERY_INTERFACE10_CI(_class, _i1, _i2, _i3, _i4, _i5, _i6,    \
                                     _i7, _i8, _i9, _i10)                     \
  NS_INTERFACE_MAP_BEGIN(_class)                                              \
    NS_INTERFACE_MAP_ENTRY(_i1)                                               \
    NS_INTERFACE_MAP_ENTRY(_i2)                                               \
    NS_INTERFACE_MAP_ENTRY(_i3)                                               \
    NS_INTERFACE_MAP_ENTRY(_i4)                                               \
    NS_INTERFACE_MAP_ENTRY(_i5)                                               \
    NS_INTERFACE_MAP_ENTRY(_i6)                                               \
    NS_INTERFACE_MAP_ENTRY(_i7)                                               \
    NS_INTERFACE_MAP_ENTRY(_i8)                                               \
    NS_INTERFACE_MAP_ENTRY(_i9)                                               \
    NS_INTERFACE_MAP_ENTRY(_i10)                                              \
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, _i1)                        \
    NS_IMPL_QUERY_CLASSINFO(_class)                                           \
  NS_INTERFACE_MAP_END

#define NS_IMPL_QUERY_INTERFACE11_CI(_class, _i1, _i2, _i3, _i4, _i5, _i6,    \
                                     _i7, _i8, _i9, _i10, _i11)               \
  NS_INTERFACE_MAP_BEGIN(_class)                                              \
    NS_INTERFACE_MAP_ENTRY(_i1)                                               \
    NS_INTERFACE_MAP_ENTRY(_i2)                                               \
    NS_INTERFACE_MAP_ENTRY(_i3)                                               \
    NS_INTERFACE_MAP_ENTRY(_i4)                                               \
    NS_INTERFACE_MAP_ENTRY(_i5)                                               \
    NS_INTERFACE_MAP_ENTRY(_i6)                                               \
    NS_INTERFACE_MAP_ENTRY(_i7)                                               \
    NS_INTERFACE_MAP_ENTRY(_i8)                                               \
    NS_INTERFACE_MAP_ENTRY(_i9)                                               \
    NS_INTERFACE_MAP_ENTRY(_i10)                                              \
    NS_INTERFACE_MAP_ENTRY(_i11)                                              \
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, _i1)                        \
    NS_IMPL_QUERY_CLASSINFO(_class)                                           \
  NS_INTERFACE_MAP_END

#define NS_IMPL_ISUPPORTS10_CI(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7,     \
                               _i8, _i9, _i10)                                \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE10_CI(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7,     \
                               _i8, _i9, _i10)                                \
  NS_IMPL_CI_INTERFACE_GETTER10(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7,    \
                                _i8, _i9, _i10)

#define NS_IMPL_ISUPPORTS11_CI(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7,     \
                               _i8, _i9, _i10, _i11)                          \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE11_CI(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7,     \
                               _i8, _i9, _i10, _i11)                          \
  NS_IMPL_CI_INTERFACE_GETTER11(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7,    \
                                _i8, _i9, _i10, _i11)

#define NS_INTERFACE_MAP_END_THREADSAFE NS_IMPL_QUERY_TAIL_GUTS

#endif
