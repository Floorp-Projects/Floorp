/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCycleCollector_h__
#define nsCycleCollector_h__

//#define DEBUG_CC

class nsISupports;
class nsICycleCollectorListener;
class nsCycleCollectionParticipant;
class nsCycleCollectionTraversalCallback;

// Contains various stats about the cycle collection.
class nsCycleCollectorResults
{
public:
    nsCycleCollectorResults() :
        mForcedGC(false), mVisitedRefCounted(0), mVisitedGCed(0),
        mFreedRefCounted(0), mFreedGCed(0) {}
    bool mForcedGC;
    PRUint32 mVisitedRefCounted;
    PRUint32 mVisitedGCed;
    PRUint32 mFreedRefCounted;
    PRUint32 mFreedGCed;
};

nsresult nsCycleCollector_startup();

typedef void (*CC_BeforeUnlinkCallback)(void);
void nsCycleCollector_setBeforeUnlinkCallback(CC_BeforeUnlinkCallback aCB);

typedef void (*CC_ForgetSkippableCallback)(void);
void nsCycleCollector_setForgetSkippableCallback(CC_ForgetSkippableCallback aCB);

void nsCycleCollector_forgetSkippable(bool aRemoveChildlessNodes = false);

#ifdef DEBUG_CC
void nsCycleCollector_logPurpleRemoval(void* aObject);
#endif

void nsCycleCollector_collect(nsCycleCollectorResults *aResults,
                              nsICycleCollectorListener *aListener);
PRUint32 nsCycleCollector_suspectedCount();
void nsCycleCollector_shutdownThreads();
void nsCycleCollector_shutdown();

// Various methods the cycle collector needs to deal with Javascript.
struct nsCycleCollectionJSRuntime
{
    virtual nsresult BeginCycleCollection(nsCycleCollectionTraversalCallback &cb) = 0;
    virtual nsresult FinishTraverse() = 0;

    /**
     * Called before/after transitioning to/from the main thread.
     *
     * NotifyLeaveMainThread may return 'false' to prevent the cycle collector
     * from leaving the main thread.
     */
    virtual bool NotifyLeaveMainThread() = 0;
    virtual void NotifyEnterCycleCollectionThread() = 0;
    virtual void NotifyLeaveCycleCollectionThread() = 0;
    virtual void NotifyEnterMainThread() = 0;

    /**
     * Should we force a JavaScript GC before a CC?
     */
    virtual bool NeedCollect() = 0;

    /**
     * Runs the JavaScript GC. |reason| is a gcreason::Reason from jsfriendapi.h.
     * |kind| is a nsGCType from nsIXPConnect.idl.
     */
    virtual void Collect(PRUint32 reason, PRUint32 kind) = 0;

    /**
     * Get the JS cycle collection participant.
     */
    virtual nsCycleCollectionParticipant *GetParticipant() = 0;
};

// Helpers for interacting with JS
void nsCycleCollector_registerJSRuntime(nsCycleCollectionJSRuntime *rt);
void nsCycleCollector_forgetJSRuntime();

#ifdef DEBUG
void nsCycleCollector_DEBUG_shouldBeFreed(nsISupports *n);
void nsCycleCollector_DEBUG_wasFreed(nsISupports *n);
#endif

#define NS_CYCLE_COLLECTOR_LOGGER_CID \
{ 0x58be81b4, 0x39d2, 0x437c, \
{ 0x94, 0xea, 0xae, 0xde, 0x2c, 0x62, 0x08, 0xd3 } }

extern nsresult
nsCycleCollectorLoggerConstructor(nsISupports* outer,
                                  const nsIID& aIID,
                                  void* *aInstancePtr);

#endif // nsCycleCollector_h__
