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

#ifndef nsAgg_h___
#define nsAgg_h___

#include "nsISupports.h"

// Put this in your class's declaration:
#define NS_DECL_AGGREGATED                                                  \
    NS_DECL_ISUPPORTS                                                       \
                                                                            \
protected:                                                                  \
                                                                            \
    /* You must implement this operation instead of the nsISupports */      \
    /* methods if you inherit from nsAggregated. */                         \
    NS_IMETHOD AggregatedQueryInterface(const nsIID& aIID,                  \
                                        void** aInstancePtr);               \
                                                                            \
    class Internal : public nsISupports {                                   \
    public:                                                                 \
                                                                            \
        Internal() {}                                                       \
                                                                            \
        NS_IMETHOD QueryInterface(const nsIID& aIID,                        \
                                  void** aInstancePtr);                     \
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
public:                                                                     \


// Put this in your class's constructor:
#define NS_INIT_AGGREGATED(outer)                                           \
    NS_INIT_REFCNT();                                                       \
    fOuter = outer;                                                         \


// Put this in your class's implementation file:
#define NS_IMPL_AGGREGATED(_class)                                          \
NS_METHOD                                                                   \
_class::QueryInterface(const nsIID& aIID, void** aInstancePtr)              \
{                                                                           \
    static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                  \
    if (aIID.Equals(kISupportsIID)) {                                       \
        *aInstancePtr = &fAggregated;                                       \
        return NS_OK;                                                       \
    }                                                                       \
    else if (fOuter)                                                        \
        return fOuter->QueryInterface(aIID, aInstancePtr);                  \
    else                                                                    \
        return AggregatedQueryInterface(aIID, aInstancePtr);                \
}                                                                           \
                                                                            \
NS_METHOD_(nsrefcnt)                                                        \
_class::AddRef(void)                                                        \
{                                                                           \
    if (fOuter)                                                             \
        return fOuter->AddRef();                                            \
    else                                                                    \
        return ++mRefCnt;                                                   \
}                                                                           \
                                                                            \
NS_METHOD_(nsrefcnt)                                                        \
_class::Release(void)                                                       \
{                                                                           \
    if (fOuter)                                                             \
        return fOuter->Release();                                           \
    else {                                                                  \
        if (--mRefCnt == 0) {                                               \
            delete this;                                                    \
            return 0;                                                       \
        }                                                                   \
        return mRefCnt;                                                     \
    }                                                                       \
}                                                                           \
                                                                            \
NS_METHOD                                                                   \
_class::Internal::QueryInterface(const nsIID& aIID, void** aInstancePtr)    \
{                                                                           \
    _class* agg = (_class*)((char*)(this) - offsetof(_class, fAggregated)); \
    return agg->AggregatedQueryInterface(aIID, aInstancePtr);               \
}                                                                           \
                                                                            \
NS_METHOD                                                                   \
_class::Internal::AddRef(void)                                              \
{                                                                           \
    _class* agg = (_class*)((char*)(this) - offsetof(_class, fAggregated)); \
    return ++agg->mRefCnt;                                                  \
}                                                                           \
                                                                            \
NS_METHOD                                                                   \
_class::Internal::Release(void)                                             \
{                                                                           \
    _class* agg = (_class*)((char*)(this) - offsetof(_class, fAggregated)); \
    if (--agg->mRefCnt == 0) {                                              \
        delete agg;                                                         \
        return 0;                                                           \
    }                                                                       \
    return agg->mRefCnt;                                                    \
}                                                                           \

#endif /* nsAgg_h___ */
