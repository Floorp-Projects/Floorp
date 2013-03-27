/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCycleCollector_h__
#define nsCycleCollector_h__

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
    uint32_t mVisitedRefCounted;
    uint32_t mVisitedGCed;
    uint32_t mFreedRefCounted;
    uint32_t mFreedGCed;
};

bool nsCycleCollector_init();

enum CCThreadingModel {
    CCSingleThread,
    CCWithTraverseThread,
};

nsresult nsCycleCollector_startup(CCThreadingModel aThreadingModel);

typedef void (*CC_BeforeUnlinkCallback)(void);
void nsCycleCollector_setBeforeUnlinkCallback(CC_BeforeUnlinkCallback aCB);

typedef void (*CC_ForgetSkippableCallback)(void);
void nsCycleCollector_setForgetSkippableCallback(CC_ForgetSkippableCallback aCB);

void nsCycleCollector_forgetSkippable(bool aRemoveChildlessNodes = false);

void nsCycleCollector_collect(bool aMergeCompartments,
                              nsCycleCollectorResults *aResults,
                              nsICycleCollectorListener *aListener);
uint32_t nsCycleCollector_suspectedCount();
void nsCycleCollector_shutdownThreads();
void nsCycleCollector_shutdown();

// Various methods the cycle collector needs to deal with Javascript.
struct nsCycleCollectionJSRuntime
{
    virtual nsresult BeginCycleCollection(nsCycleCollectionTraversalCallback &cb) = 0;

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
     * Unmark gray any weak map values, as needed.
     */
    virtual void FixWeakMappingGrayBits() = 0;

    /**
     * Should we force a JavaScript GC before a CC?
     */
    virtual bool NeedCollect() = 0;

    /**
     * Runs the JavaScript GC. |reason| is a gcreason::Reason from jsfriendapi.h.
     */
    virtual void Collect(uint32_t reason) = 0;

    /**
     * Get the JS cycle collection participant.
     */
    virtual nsCycleCollectionParticipant *GetParticipant() = 0;

#ifdef DEBUG
    virtual void SetObjectToUnlink(void* aObject) = 0;
    virtual void AssertNoObjectsToTrace(void* aPossibleJSHolder) = 0;
#endif
};

// Helpers for interacting with JS
void nsCycleCollector_registerJSRuntime(nsCycleCollectionJSRuntime *rt);
void nsCycleCollector_forgetJSRuntime();

#define NS_CYCLE_COLLECTOR_LOGGER_CID \
{ 0x58be81b4, 0x39d2, 0x437c, \
{ 0x94, 0xea, 0xae, 0xde, 0x2c, 0x62, 0x08, 0xd3 } }

extern nsresult
nsCycleCollectorLoggerConstructor(nsISupports* outer,
                                  const nsIID& aIID,
                                  void* *aInstancePtr);

#endif // nsCycleCollector_h__
