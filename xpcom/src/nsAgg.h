/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

/**
 * Outer objects can implement nsIOuter if they choose, allowing them to 
 * get notification if their inner objects (children) are effectively freed.
 * This allows them to reset any state associated with the inner object and
 * potentially unload it.
 */
class nsIOuter : public nsISupports {
public:

    /**
     * This method is called whenever an inner object's refcount is about to
     * become zero and the inner object should be released by the outer. This
     * allows the outer to clean up any state associated with the inner and 
     * potentially unload the inner object. This method should call 
     * inner->Release().
     */
    NS_IMETHOD
    ReleaseInner(nsISupports* inner) = 0;

};

#define NS_IOUTER_IID                                \
{ /* ea0bf9f0-3d67-11d2-8163-006008119d7a */         \
    0xea0bf9f0,                                      \
    0x3d67,                                          \
    0x11d2,                                          \
    {0x81, 0x63, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

////////////////////////////////////////////////////////////////////////////////

// Put this in your class's declaration:
#define NS_DECL_AGGREGATED                                                  \
    NS_DECL_ISUPPORTS                                                       \
                                                                            \
protected:                                                                  \
                                                                            \
    /* You must implement this operation instead of the nsISupports */      \
    /* methods if you inherit from nsAggregated. */                         \
    NS_IMETHOD                                                              \
    AggregatedQueryInterface(const nsIID& aIID, void** aInstancePtr);       \
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
    NS_INIT_REFCNT();                                                       \
    fOuter = outer;                                                         \


// Put this in your class's implementation file:
#define NS_IMPL_AGGREGATED(_class)                                          \
NS_IMETHODIMP                                                               \
_class::QueryInterface(const nsIID& aIID, void** aInstancePtr)              \
{                                                                           \
    /* try our own interfaces first before delegating to outer */           \
    nsresult rslt = AggregatedQueryInterface(aIID, aInstancePtr);           \
    if (rslt != NS_OK && fOuter)                                            \
        return fOuter->QueryInterface(aIID, aInstancePtr);                  \
    else                                                                    \
        return rslt;                                                        \
}                                                                           \
                                                                            \
NS_IMETHODIMP_(nsrefcnt)                                                    \
_class::AddRef(void)                                                        \
{                                                                           \
    ++mRefCnt; /* keep track of our refcount as well as outer's */          \
    if (fOuter)                                                             \
        return NS_ADDREF(fOuter);                                           \
    else                                                                    \
        return mRefCnt;                                                     \
}                                                                           \
                                                                            \
NS_IMETHODIMP_(nsrefcnt)                                                    \
_class::Release(void)                                                       \
{                                                                           \
    if (fOuter) {                                                           \
        nsISupports* outer = fOuter;    /* in case we release ourself */    \
        nsIOuter* outerIntf;                                                \
        static NS_DEFINE_IID(kIOuterIID, NS_IOUTER_IID);                    \
        if (mRefCnt == 1 &&                                                 \
            outer->QueryInterface(kIOuterIID,                               \
                                  (void**)&outerIntf) == NS_OK) {           \
            outerIntf->ReleaseInner(GetInner());                            \
            outerIntf->Release();                                           \
        }                                                                   \
        else                                                                \
            --mRefCnt; /* keep track of our refcount as well as outer's */  \
        return outer->Release();                                            \
    }                                                                       \
    else {                                                                  \
        if (--mRefCnt == 0) {                                               \
            delete this;                                                    \
            return 0;                                                       \
        }                                                                   \
        return mRefCnt;                                                     \
    }                                                                       \
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
    return ++agg->mRefCnt;                                                  \
}                                                                           \
                                                                            \
NS_IMETHODIMP_(nsrefcnt)                                                    \
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
