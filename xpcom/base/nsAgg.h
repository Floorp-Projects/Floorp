/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
     * Returns true if this object is part of an aggregated object.      \
     */                                                                     \
    bool IsPartOfAggregated(void) { return fOuter != InnerObject(); }     \
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
  NS_IMETHOD_(void) Unlink(void *p);                                        \
  NS_IMETHOD Traverse(void *p, nsCycleCollectionTraversalCallback &cb);     \
  NS_IMETHOD_(void) DeleteCycleCollectable(void* p)                         \
  {                                                                         \
    NS_CYCLE_COLLECTION_CLASSNAME(_class)::                                 \
      Downcast(static_cast<nsISupports*>(p))->DeleteCycleCollectable();     \
  }                                                                         \
  static _class* Downcast(nsISupports* s)                                   \
  {                                                                         \
    return (_class*)((char*)(s) - offsetof(_class, fAggregated));           \
  }                                                                         \
  static nsISupports* Upcast(_class *p)                                     \
  {                                                                         \
    return p->InnerObject();                                                \
  }                                                                         \
  static nsXPCOMCycleCollectionParticipant* GetParticipant()                \
  {                                                                         \
    return &_class::NS_CYCLE_COLLECTION_INNERNAME;                          \
  }                                                                         \
};                                                                          \
NS_CHECK_FOR_RIGHT_PARTICIPANT_IMPL(_class);                                \
static NS_CYCLE_COLLECTION_INNERCLASS NS_CYCLE_COLLECTION_INNERNAME;

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
    MOZ_ASSERT(int32_t(agg->mRefCnt) >= 0, "illegal refcnt");               \
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
    MOZ_ASSERT(int32_t(agg->mRefCnt) > 0, "dup release");                   \
    NS_ASSERT_OWNINGTHREAD(_class);                                         \
    --agg->mRefCnt;                                                         \
    NS_LOG_RELEASE(this, agg->mRefCnt, #_class);                            \
    if (agg->mRefCnt == 0) {                                                \
        agg->mRefCnt = 1; /* stabilize */                                   \
        delete agg;                                                         \
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
    MOZ_ASSERT(int32_t(agg->mRefCnt) >= 0, "illegal refcnt");               \
    NS_ASSERT_OWNINGTHREAD_AGGREGATE(agg, _class);                          \
    nsrefcnt count = agg->mRefCnt.incr();                                   \
    NS_LOG_ADDREF(this, count, #_class, sizeof(*agg));                      \
    return count;                                                           \
}                                                                           \
NS_IMETHODIMP_(nsrefcnt)                                                    \
_class::Internal::Release(void)                                             \
{                                                                           \
    _class* agg = NS_CYCLE_COLLECTION_CLASSNAME(_class)::Downcast(this);    \
    MOZ_ASSERT(int32_t(agg->mRefCnt) > 0, "dup release");                   \
    NS_ASSERT_OWNINGTHREAD_AGGREGATE(agg, _class);                          \
    nsrefcnt count = agg->mRefCnt.decr(this);                               \
    NS_LOG_RELEASE(this, count, #_class);                                   \
    return count;                                                           \
}                                                                           \
NS_IMETHODIMP_(void)                                                        \
_class::DeleteCycleCollectable(void)                                        \
{                                                                           \
  delete this;                                                              \
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
    foundInterface = NS_CYCLE_COLLECTION_PARTICIPANT(_class);               \
  else

#define NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_AGGREGATED(_class)          \
  NS_IMETHODIMP                                                             \
  NS_CYCLE_COLLECTION_CLASSNAME(_class)::Traverse                           \
                         (void *p, nsCycleCollectionTraversalCallback &cb)  \
  {                                                                         \
    nsISupports *s = static_cast<nsISupports*>(p);                          \
    MOZ_ASSERT(CheckForRightISupports(s),                                   \
               "not the nsISupports pointer we expect");                    \
    _class *tmp = static_cast<_class*>(Downcast(s));                        \
    if (!tmp->IsPartOfAggregated())                                         \
        NS_IMPL_CYCLE_COLLECTION_DESCRIBE(_class, tmp->mRefCnt.get())

#define NS_GENERIC_AGGREGATED_CONSTRUCTOR(_InstanceClass)                   \
static nsresult                                                             \
_InstanceClass##Constructor(nsISupports *aOuter, REFNSIID aIID,             \
                            void **aResult)                                 \
{                                                                           \
    *aResult = nullptr;                                                     \
    if (NS_WARN_IF(aOuter && !aIID.Equals(NS_GET_IID(nsISupports))))        \
        return NS_ERROR_INVALID_ARG;                                        \
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
static nsresult                                                             \
_InstanceClass##Constructor(nsISupports *aOuter, REFNSIID aIID,             \
                            void **aResult)                                 \
{                                                                           \
    *aResult = nullptr;                                                     \
    if (NS_WARN_IF(aOuter && !aIID.Equals(NS_GET_IID(nsISupports))))        \
        return NS_ERROR_INVALID_ARG;                                        \
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
