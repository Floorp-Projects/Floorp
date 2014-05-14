/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCycleCollector_h__
#define nsCycleCollector_h__

class nsICycleCollectorListener;
class nsICycleCollectorLogSink;
class nsISupports;
template<class T> struct already_AddRefed;

#include "nsError.h"
#include "nsID.h"

namespace mozilla {

class CycleCollectedJSRuntime;

// See the comments in nsContentUtils.h for explanations of these functions.
typedef void* (*DeferredFinalizeAppendFunction)(void* aPointers, void* aThing);
typedef bool (*DeferredFinalizeFunction)(uint32_t aSlice, void* aData);

}

bool nsCycleCollector_init();

void nsCycleCollector_startup();

typedef void (*CC_BeforeUnlinkCallback)(void);
void nsCycleCollector_setBeforeUnlinkCallback(CC_BeforeUnlinkCallback aCB);

typedef void (*CC_ForgetSkippableCallback)(void);
void nsCycleCollector_setForgetSkippableCallback(CC_ForgetSkippableCallback aCB);

void nsCycleCollector_forgetSkippable(bool aRemoveChildlessNodes = false,
                                      bool aAsyncSnowWhiteFreeing = false);

void nsCycleCollector_prepareForGarbageCollection();

// If an incremental cycle collection is in progress, finish it.
void nsCycleCollector_finishAnyCurrentCollection();

void nsCycleCollector_dispatchDeferredDeletion(bool aContinuation = false);
bool nsCycleCollector_doDeferredDeletion();

already_AddRefed<nsICycleCollectorLogSink> nsCycleCollector_createLogSink();

void nsCycleCollector_collect(nsICycleCollectorListener* aManualListener);

// If aSliceTime is negative, the CC will run to completion. Otherwise,
// aSliceTime will be used as the time budget for the slice, in ms.
void nsCycleCollector_collectSlice(int64_t aSliceTime);

// If aSliceTime is negative, the CC will run to completion. Otherwise,
// aSliceTime will be used as the work budget for the slice.
void nsCycleCollector_collectSliceWork(int64_t aSliceWork);

uint32_t nsCycleCollector_suspectedCount();
void nsCycleCollector_shutdown();

// Helpers for interacting with JS
void nsCycleCollector_registerJSRuntime(mozilla::CycleCollectedJSRuntime* aRt);
void nsCycleCollector_forgetJSRuntime();

#define NS_CYCLE_COLLECTOR_LOGGER_CID \
{ 0x58be81b4, 0x39d2, 0x437c, \
{ 0x94, 0xea, 0xae, 0xde, 0x2c, 0x62, 0x08, 0xd3 } }

extern nsresult
nsCycleCollectorLoggerConstructor(nsISupports* aOuter,
                                  const nsIID& aIID,
                                  void** aInstancePtr);

namespace mozilla {
namespace cyclecollector {

#ifdef DEBUG
bool IsJSHolder(void* aHolder);
#endif

void DeferredFinalize(DeferredFinalizeAppendFunction aAppendFunc,
                      DeferredFinalizeFunction aFunc,
                      void* aThing);
void DeferredFinalize(nsISupports* aSupports);


} // namespace cyclecollector
} // namespace mozilla

#endif // nsCycleCollector_h__
