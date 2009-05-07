/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#ifndef nsAgg_h___
#define nsAgg_h___

#include "nsISupports.h"
#include "nsCycleCollectionParticipant.h"


////////////////////////////////////////////////////////////////////////////////

// Put NS_DECL_AGGREGATED or NS_DECL_CYCLE_COLLECTING_AGGREGATED in your class's
// declaration.
#define NS_DECL_AGGREGATED                                                  \
    NS_DECL_ISUPPORTS                                                       \
    NS_DECL_AGGREGATED_HELPER

#define NS_DECL_CYCLE_COLLECTING_AGGREGATED                                 \
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS                                      \
    NS_DECL_AGGREGATED_HELPER

#define NS_DECL_AGGREGATED_HELPER                                           \
public:                                                                     \
                                                                            \
    /**                                                                     \
     * Returns the nsISupports pointer of the inner object (aka the         \
     * aggregatee). This pointer is really only useful to the outer object  \
     * (aka the aggregator), which can use it to hold on to the inner       \
     * object. Anything else wants the nsISupports pointer of the outer     \
     * object (gotten by QI'ing inner or outer to nsISupports). This method \
     * returns a non-addrefed pointer.                                      \
     * @return the nsISupports pointer of the inner object                  \
     */                                                                     \
    nsISupports* InnerObject(void) { return &fAggregated; }                 \
                                                                            \
    /**                                                                     \
     * Returns PR_TRUE if this object is part of an aggregated object.      \
     */                                                                     \
    PRBool IsPartOfAggregated(void) { return fOuter != InnerObject(); }     \
                                                                            \
private:                                                                    \
                                                                            \
    /* You must implement this operation instead of the nsISupports */      \
    /* methods. */                                                          \
    nsresult                                                                \
    AggregatedQueryInterface(const nsIID& aIID, void** aInstancePtr);       \
                                                                            \
    class Internal : public nsISupports {                                   \
    public:                                                                 \
                                                                            \
        Internal() {}                                                       \
                                                                            \
        NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);  \
        NS_IMETHOD_(nsrefcnt) AddRef(void);                                 \
        NS_IMETHOD_(nsrefcnt) Release(void);                                \
                                                                            \
        NS_DECL_OWNINGTHREAD                                                \
    };                                                                      \
                                                                            \
    friend class Internal;                                                  \
                                                                            \
    nsISupports*        fOuter;                                             \
    Internal            fAggregated;                                        \
                                                                            \
public:                                                                     \

#define NS_DECL_AGGREGATED_CYCLE_COLLECTION_CLASS(_class)                   \
class NS_CYCLE_COLLECTION_INNERCLASS                                        \
 : public nsXPCOMCycleCollectionParticipant                                 \
{                                                                           \
public:                                                                     \
  NS_IMETHOD Unlink(void *p);                                               \
  NS_IMETHOD Traverse(void *p,                                              \
                      nsCycleCollectionTraversalCallback &cb);              \
  NS_IMETHOD_(void) UnmarkPurple(nsISupports *p)                            \
  {                                                                         \
    Downcast(p)->UnmarkPurple();                                            \
  }                                                                         \
  static _class* Downcast(nsISupports* s)                                   \
  {                                                                         \
    return (_class*)((char*)(s) - offsetof(_class, fAggregated));           \
  }                                                                         \
  static nsISupports* Upcast(_class *p)                                     \
  {                                                                         \
    return p->InnerObject();                                                \
  }                                                                         \
};                                                                          \
NS_CYCLE_COLLECTION_PARTICIPANT_INSTANCE

// Put this in your class's constructor:
#define NS_INIT_AGGREGATED(outer)                                           \
  PR_BEGIN_MACRO                                                            \
    fOuter = outer ? outer : &fAggregated;                                  \
  PR_END_MACRO


// Put this in your class's implementation file:
#define NS_IMPL_AGGREGATED(_class)                                          \
                                                                            \
NS_IMPL_AGGREGATED_HELPER(_class)                                           \
                                                                            \
NS_IMETHODIMP_(nsrefcnt)                                                    \
_class::Internal::AddRef(void)                                              \
{                                                                           \
    _class* agg = (_class*)((char*)(this) - offsetof(_class, fAggregated)); \
    NS_PRECONDITION(PRInt32(agg->mRefCnt) >= 0, "illegal refcnt");          \
    NS_ASSERT_OWNINGTHREAD(_class);                                         \
    ++agg->mRefCnt;                                                         \
    NS_LOG_ADDREF(this, agg->mRefCnt, #_class, sizeof(*this));              \
    return agg->mRefCnt;                                                    \
}                                                                           \
                                                                            \
NS_IMETHODIMP_(nsrefcnt)                                                    \
_class::Internal::Release(void)                                             \
{                                                                           \
    _class* agg = (_class*)((char*)(this) - offsetof(_class, fAggregated)); \
    NS_PRECONDITION(0 != agg->mRefCnt, "dup release");                      \
    NS_ASSERT_OWNINGTHREAD(_class);                                         \
    --agg->mRefCnt;                                                         \
    NS_LOG_RELEASE(this, agg->mRefCnt, #_class);                            \
    if (agg->mRefCnt == 0) {                                                \
        agg->mRefCnt = 1; /* stabilize */                                   \
        NS_DELETEXPCOM(agg);                                                \
        return 0;                                                           \
    }                                                                       \
    return agg->mRefCnt;                                                    \
}                                                                           \

#define NS_IMPL_CYCLE_COLLECTING_AGGREGATED(_class)                         \
                                                                            \
NS_IMPL_AGGREGATED_HELPER(_class)                                           \
                                                                            \
NS_IMETHODIMP_(nsrefcnt)                                                    \
_class::Internal::AddRef(void)                                              \
{                                                                           \
    _class* agg = NS_CYCLE_COLLECTION_CLASSNAME(_class)::Downcast(this);    \
    NS_PRECONDITION(PRInt32(agg->mRefCnt) >= 0, "illegal refcnt");          \
    NS_CheckThreadSafe(agg->_mOwningThread.GetThread(),                     \
                       #_class " not thread-safe");                         \
    nsrefcnt count = agg->mRefCnt.incr(this);                               \
    NS_LOG_ADDREF(this, count, #_class, sizeof(*agg));                      \
    return count;                                                           \
}                                                                           \
                                                                            \
NS_IMETHODIMP_(nsrefcnt)                                                    \
_class::Internal::Release(void)                                             \
{                                                                           \
    _class* agg = NS_CYCLE_COLLECTION_CLASSNAME(_class)::Downcast(this);    \
    NS_PRECONDITION(0 != agg->mRefCnt, "dup release");                      \
    NS_CheckThreadSafe(agg->_mOwningThread.GetThread(),                     \
                       #_class " not thread-safe");                         \
    nsrefcnt count = agg->mRefCnt.decr(this);                               \
    NS_LOG_RELEASE(this, count, #_class);                                   \
    if (count == 0) {                                                       \
        agg->mRefCnt.stabilizeForDeletion(this);                            \
        NS_DELETEXPCOM(agg);                                                \
        return 0;                                                           \
    }                                                                       \
    return count;                                                           \
}

#define NS_IMPL_AGGREGATED_HELPER(_class)                                   \
NS_IMETHODIMP                                                               \
_class::QueryInterface(const nsIID& aIID, void** aInstancePtr)              \
{                                                                           \
    return fOuter->QueryInterface(aIID, aInstancePtr);                      \
}                                                                           \
                                                                            \
NS_IMETHODIMP_(nsrefcnt)                                                    \
_class::AddRef(void)                                                        \
{                                                                           \
    return fOuter->AddRef();                                                \
}                                                                           \
                                                                            \
NS_IMETHODIMP_(nsrefcnt)                                                    \
_class::Release(void)                                                       \
{                                                                           \
    return fOuter->Release();                                               \
}                                                                           \
                                                                            \
NS_IMETHODIMP                                                               \
_class::Internal::QueryInterface(const nsIID& aIID, void** aInstancePtr)    \
{                                                                           \
    _class* agg = (_class*)((char*)(this) - offsetof(_class, fAggregated)); \
    return agg->AggregatedQueryInterface(aIID, aInstancePtr);               \
}                                                                           \

/**
 * To make aggregated objects participate in cycle collection we need to enable
 * the outer object (aggregator) to traverse/unlink the objects held by the
 * inner object (the aggregatee). We can't just make the inner object QI'able to
 * NS_CYCLECOLLECTIONPARTICIPANT_IID, we don't want to return the inner object's
 * nsCycleCollectionParticipant for the outer object (which will happen if the
 * outer object doesn't participate in cycle collection itself).
 * NS_AGGREGATED_CYCLECOLLECTIONPARTICIPANT_IID enables the outer object to get
 * the inner objects nsCycleCollectionParticipant.
 *
 * There are three cases:
 *   - No aggregation
 *     QI'ing to NS_CYCLECOLLECTIONPARTICIPANT_IID will return the inner
 *     object's nsCycleCollectionParticipant.
 *
 *   - Aggregation and outer object does not participate in cycle collection
 *     QI'ing to NS_CYCLECOLLECTIONPARTICIPANT_IID will not return anything.
 *
 *   - Aggregation and outer object does participate in cycle collection
 *     QI'ing to NS_CYCLECOLLECTIONPARTICIPANT_IID will return the outer
 *     object's nsCycleCollectionParticipant. The outer object's
 *     nsCycleCollectionParticipant can then QI the inner object to
 *     NS_AGGREGATED_CYCLECOLLECTIONPARTICIPANT_IID to get the inner object's
 *     nsCycleCollectionParticipant, which it can use to traverse/unlink the
 *     objects reachable from the inner object.
 */
#define NS_AGGREGATED_CYCLECOLLECTIONPARTICIPANT_IID                        \
{                                                                           \
    0x32889b7e,                                                             \
    0xe4fe,                                                                 \
    0x43f4,                                                                 \
    { 0x85, 0x31, 0xb5, 0x28, 0x23, 0xa2, 0xe9, 0xfc }                      \
}

/**
 * Just holds the IID so NS_GET_IID works.
 */
class nsAggregatedCycleCollectionParticipant
{
public:
    NS_DECLARE_STATIC_IID_ACCESSOR(NS_AGGREGATED_CYCLECOLLECTIONPARTICIPANT_IID)
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsAggregatedCycleCollectionParticipant, 
                              NS_AGGREGATED_CYCLECOLLECTIONPARTICIPANT_IID)

// for use with QI macros in nsISupportsUtils.h:

#define NS_INTERFACE_MAP_BEGIN_AGGREGATED(_class)                           \
  NS_IMPL_AGGREGATED_QUERY_HEAD(_class)

#define NS_INTERFACE_MAP_ENTRY_CYCLE_COLLECTION_AGGREGATED(_class)          \
  NS_IMPL_QUERY_CYCLE_COLLECTION(_class)

#define NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION_AGGREGATED(_class)        \
  NS_INTERFACE_MAP_ENTRY_CYCLE_COLLECTION_AGGREGATED(_class)                \
  NS_INTERFACE_MAP_ENTRY_CYCLE_COLLECTION_ISUPPORTS(_class)

#define NS_IMPL_AGGREGATED_QUERY_HEAD(_class)                               \
nsresult                                                                    \
_class::AggregatedQueryInterface(REFNSIID aIID, void** aInstancePtr)        \
{                                                                           \
  NS_ASSERTION(aInstancePtr,                                                \
               "AggregatedQueryInterface requires a non-NULL result ptr!"); \
  if ( !aInstancePtr )                                                      \
    return NS_ERROR_NULL_POINTER;                                           \
  nsISupports* foundInterface;                                              \
  if ( aIID.Equals(NS_GET_IID(nsISupports)) )                               \
    foundInterface = InnerObject();                                         \
  else

#define NS_IMPL_AGGREGATED_QUERY_CYCLE_COLLECTION(_class)                   \
  if (aIID.Equals(IsPartOfAggregated() ?                                    \
                  NS_GET_IID(nsCycleCollectionParticipant) :                \
                  NS_GET_IID(nsAggregatedCycleCollectionParticipant)))      \
    foundInterface = & NS_CYCLE_COLLECTION_NAME(_class);                    \
  else

#define NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_AGGREGATED(_class)          \
  NS_IMETHODIMP                                                             \
  NS_CYCLE_COLLECTION_CLASSNAME(_class)::Traverse                           \
                         (void *p,                                          \
                          nsCycleCollectionTraversalCallback &cb)           \
  {                                                                         \
    nsISupports *s = static_cast<nsISupports*>(p);                          \
    NS_ASSERTION(CheckForRightISupports(s),                                 \
                 "not the nsISupports pointer we expect");                  \
    _class *tmp = static_cast<_class*>(Downcast(s));                        \
    if (!tmp->IsPartOfAggregated())                                         \
        NS_IMPL_CYCLE_COLLECTION_DESCRIBE(_class, tmp->mRefCnt.get())

#define NS_GENERIC_AGGREGATED_CONSTRUCTOR(_InstanceClass)                   \
static NS_METHOD                                                            \
_InstanceClass##Constructor(nsISupports *aOuter, REFNSIID aIID,             \
                            void **aResult)                                 \
{                                                                           \
    *aResult = nsnull;                                                      \
                                                                            \
    NS_ENSURE_PROPER_AGGREGATION(aOuter, aIID);                             \
                                                                            \
    _InstanceClass* inst = new _InstanceClass(aOuter);                      \
    if (!inst) {                                                            \
        return NS_ERROR_OUT_OF_MEMORY;                                      \
    }                                                                       \
                                                                            \
    nsISupports* inner = inst->InnerObject();                               \
    nsresult rv = inner->QueryInterface(aIID, aResult);                     \
    if (NS_FAILED(rv)) {                                                    \
        delete inst;                                                        \
    }                                                                       \
                                                                            \
    return rv;                                                              \
}                                                                           \

#define NS_GENERIC_AGGREGATED_CONSTRUCTOR_INIT(_InstanceClass, _InitMethod) \
static NS_METHOD                                                            \
_InstanceClass##Constructor(nsISupports *aOuter, REFNSIID aIID,             \
                            void **aResult)                                 \
{                                                                           \
    *aResult = nsnull;                                                      \
                                                                            \
    NS_ENSURE_PROPER_AGGREGATION(aOuter, aIID);                             \
                                                                            \
    _InstanceClass* inst = new _InstanceClass(aOuter);                      \
    if (!inst) {                                                            \
        return NS_ERROR_OUT_OF_MEMORY;                                      \
    }                                                                       \
                                                                            \
    nsISupports* inner = inst->InnerObject();                               \
    NS_ADDREF(inner);                                                       \
    nsresult rv = inst->_InitMethod();                                      \
    if (NS_SUCCEEDED(rv)) {                                                 \
        rv = inner->QueryInterface(aIID, aResult);                          \
    }                                                                       \
    NS_RELEASE(inner);                                                      \
                                                                            \
    return rv;                                                              \
}                                                                           \

#endif /* nsAgg_h___ */
