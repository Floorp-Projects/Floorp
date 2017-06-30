/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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

#include "js/SliceBudget.h"

namespace mozilla {
class CycleCollectedJSContext;
} // namespace mozilla

bool nsCycleCollector_init();

void nsCycleCollector_startup();

typedef void (*CC_BeforeUnlinkCallback)(void);
void nsCycleCollector_setBeforeUnlinkCallback(CC_BeforeUnlinkCallback aCB);

typedef void (*CC_ForgetSkippableCallback)(void);
void nsCycleCollector_setForgetSkippableCallback(CC_ForgetSkippableCallback aCB);

void nsCycleCollector_forgetSkippable(js::SliceBudget& aBudget,
                                      bool aRemoveChildlessNodes = false,
                                      bool aAsyncSnowWhiteFreeing = false);

void nsCycleCollector_prepareForGarbageCollection();

// If an incremental cycle collection is in progress, finish it.
void nsCycleCollector_finishAnyCurrentCollection();

void nsCycleCollector_dispatchDeferredDeletion(bool aContinuation = false,
                                               bool aPurge = false);
bool nsCycleCollector_doDeferredDeletion();

already_AddRefed<nsICycleCollectorLogSink> nsCycleCollector_createLogSink();

void nsCycleCollector_collect(nsICycleCollectorListener* aManualListener);

void nsCycleCollector_collectSlice(js::SliceBudget& budget,
                                   bool aPreferShorterSlices = false);

uint32_t nsCycleCollector_suspectedCount();

// If aDoCollect is true, then run the GC and CC a few times before
// shutting down the CC completely.
void nsCycleCollector_shutdown(bool aDoCollect = true);

// Helpers for interacting with JS
void nsCycleCollector_registerJSContext(mozilla::CycleCollectedJSContext* aCx);
void nsCycleCollector_forgetJSContext();

// Helpers for cooperative threads.
void nsCycleCollector_registerNonPrimaryContext(mozilla::CycleCollectedJSContext* aCx);
void nsCycleCollector_forgetNonPrimaryContext();

#define NS_CYCLE_COLLECTOR_LOGGER_CID \
{ 0x58be81b4, 0x39d2, 0x437c, \
{ 0x94, 0xea, 0xae, 0xde, 0x2c, 0x62, 0x08, 0xd3 } }

extern nsresult
nsCycleCollectorLoggerConstructor(nsISupports* aOuter,
                                  const nsIID& aIID,
                                  void** aInstancePtr);

#endif // nsCycleCollector_h__
