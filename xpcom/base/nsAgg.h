/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 */

#ifndef nsAgg_h___
#define nsAgg_h___

#include "nsISupports.h"


////////////////////////////////////////////////////////////////////////////////

// Put this in your class's declaration:
#define NS_DECL_AGGREGATED                                                  \
    NS_DECL_ISUPPORTS                                                       \
                                                                            \
public:                                                                     \
                                                                            \
    /* You must implement this operation instead of the nsISupports */      \
    /* methods if you inherit from nsAggregated. */                         \
    NS_IMETHOD                                                              \
    AggregatedQueryInterface(const nsIID& aIID, void** aInstancePtr);       \
                                                                            \
protected:                                                                  \
                                                                            \
    class Internal : public nsISupports {                                   \
    public:                                                                 \
                                                                            \
        Internal() {}                                                       \
                                                                            \
        NS_IMETHOD QueryInterface(const nsIID& aIID,                        \
                                        void** aInstancePtr);               \
        NS_IMETHOD_(nsrefcnt) AddRef(void);                                 \
        NS_IMETHOD_(nsrefcnt) Release(void);                                \
                                                                            \
    };                                                                      \
                                                                            \
    friend class Internal;                                                  \
                                                                            \
    nsISupports*        fOuter;                                             \
    Internal            fAggregated;                                        \
                                                                            \
    nsISupports* GetInner(void) { return &fAggregated; }                    \
                                                                            \
public:                                                                     \


// Put this in your class's constructor:
#define NS_INIT_AGGREGATED(outer)                                           \
  PR_BEGIN_MACRO                                                            \
    NS_INIT_REFCNT();                                                       \
    fOuter = outer ? outer : &fAggregated;                                  \
  PR_END_MACRO


// Put this in your class's implementation file:
#define NS_IMPL_AGGREGATED(_class)                                          \
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
                                                                            \
NS_IMETHODIMP_(nsrefcnt)                                                    \
_class::Internal::AddRef(void)                                              \
{                                                                           \
    _class* agg = (_class*)((char*)(this) - offsetof(_class, fAggregated)); \
    NS_PRECONDITION(PRInt32(agg->mRefCnt) >= 0, "illegal refcnt");          \
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
    --agg->mRefCnt;                                                         \
    NS_LOG_RELEASE(this, agg->mRefCnt, #_class);                            \
    if (agg->mRefCnt == 0) {                                                \
        agg->mRefCnt = 1; /* stabilize */                                   \
        NS_DELETEXPCOM(agg);                                                \
        return 0;                                                           \
    }                                                                       \
    return agg->mRefCnt;                                                    \
}                                                                           \

// for use with QI macros in nsISupportsUtils.h:

#define NS_IMPL_AGGREGATED_QUERY_HEAD(_class)                               \
NS_IMETHODIMP                                                               \
_class::AggregatedQueryInterface(REFNSIID aIID, void** aInstancePtr)        \
{                                                                           \
  NS_ASSERTION(aInstancePtr,                                                \
               "AggregatedQueryInterface requires a non-NULL result ptr!"); \
  if ( !aInstancePtr )                                                      \
    return NS_ERROR_NULL_POINTER;                                           \
  nsISupports* foundInterface;

#endif /* nsAgg_h___ */
