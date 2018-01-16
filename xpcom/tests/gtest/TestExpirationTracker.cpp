/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdlib.h>
#include <stdio.h>
#include <prthread.h>
#include "nsExpirationTracker.h"
#include "nsMemory.h"
#include "nsAutoPtr.h"
#include "nsString.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsXPCOM.h"
#include "nsIFile.h"
#include "prinrval.h"
#include "nsThreadUtils.h"
#include "mozilla/UniquePtr.h"
#include "gtest/gtest.h"

namespace TestExpirationTracker {

struct Object {
  Object() : mExpired(false) { Touch(); }
  void Touch() { mLastUsed = PR_IntervalNow(); mExpired = false; }

  nsExpirationState mExpiration;
  nsExpirationState* GetExpirationState() { return &mExpiration; }

  PRIntervalTime mLastUsed;
  bool           mExpired;
};

static bool error;
static uint32_t periodMS = 100;
static uint32_t ops = 1000;
static uint32_t iterations = 2;
static bool logging = 0;
static uint32_t sleepPeriodMS = 50;
static uint32_t slackMS = 30; // allow this much error

template <uint32_t K> class Tracker : public nsExpirationTracker<Object,K> {
public:
  Tracker() : nsExpirationTracker<Object,K>(periodMS, "Tracker") {
    Object* obj = new Object();
    mUniverse.AppendElement(obj);
    LogAction(obj, "Created");
  }

  nsTArray<mozilla::UniquePtr<Object>> mUniverse;

  void LogAction(Object* aObj, const char* aAction) {
    if (logging) {
      printf("%d %p(%d): %s\n", PR_IntervalNow(),
             static_cast<void*>(aObj), aObj->mLastUsed, aAction);
    }
  }

  void DoRandomOperation() {
    using mozilla::UniquePtr;

    Object* obj;
    switch (rand() & 0x7) {
    case 0: {
      if (mUniverse.Length() < 50) {
        obj = new Object();
        mUniverse.AppendElement(obj);
        nsExpirationTracker<Object,K>::AddObject(obj);
        LogAction(obj, "Created and added");
      }
      break;
    }
    case 4: {
      if (mUniverse.Length() < 50) {
        obj = new Object();
        mUniverse.AppendElement(obj);
        LogAction(obj, "Created");
      }
      break;
    }
    case 1: {
      UniquePtr<Object>& objref = mUniverse[uint32_t(rand())%mUniverse.Length()];
      if (objref->mExpiration.IsTracked()) {
        nsExpirationTracker<Object,K>::RemoveObject(objref.get());
        LogAction(objref.get(), "Removed");
      }
      break;
    }
    case 2: {
      UniquePtr<Object>& objref = mUniverse[uint32_t(rand())%mUniverse.Length()];
      if (!objref->mExpiration.IsTracked()) {
        objref->Touch();
        nsExpirationTracker<Object,K>::AddObject(objref.get());
        LogAction(objref.get(), "Added");
      }
      break;
    }
    case 3: {
      UniquePtr<Object>& objref = mUniverse[uint32_t(rand())%mUniverse.Length()];
      if (objref->mExpiration.IsTracked()) {
        objref->Touch();
        nsExpirationTracker<Object,K>::MarkUsed(objref.get());
        LogAction(objref.get(), "Marked used");
      }
      break;
    }
    }
  }

protected:
  void NotifyExpired(Object* aObj) override {
    LogAction(aObj, "Expired");
    PRIntervalTime now = PR_IntervalNow();
    uint32_t timeDiffMS = (now - aObj->mLastUsed)*1000/PR_TicksPerSecond();
    // See the comment for NotifyExpired in nsExpirationTracker.h for these
    // bounds
    uint32_t lowerBoundMS = (K-1)*periodMS - slackMS;
    uint32_t upperBoundMS = K*(periodMS + sleepPeriodMS) + slackMS;
    if (logging) {
      printf("Checking: %d-%d = %d [%d,%d]\n",
             now, aObj->mLastUsed, timeDiffMS, lowerBoundMS, upperBoundMS);
    }
    if (timeDiffMS < lowerBoundMS || timeDiffMS > upperBoundMS) {
      EXPECT_TRUE(timeDiffMS < periodMS && aObj->mExpired);
    }
    aObj->Touch();
    aObj->mExpired = true;
    DoRandomOperation();
    DoRandomOperation();
    DoRandomOperation();
  }
};

template <uint32_t K> static bool test_random() {
  srand(K);
  error = false;

  for (uint32_t j = 0; j < iterations; ++j) {
    Tracker<K> tracker;

    uint32_t i = 0;
    for (i = 0; i < ops; ++i) {
      if ((rand() & 0xF) == 0) {
        // Simulate work that takes time
        if (logging) {
          printf("SLEEPING for %dms (%d)\n", sleepPeriodMS, PR_IntervalNow());
        }
        PR_Sleep(PR_MillisecondsToInterval(sleepPeriodMS));
        // Process pending timer events
        NS_ProcessPendingEvents(nullptr);
      }
      tracker.DoRandomOperation();
    }
  }

  return !error;
}

static bool test_random3() { return test_random<3>(); }
static bool test_random4() { return test_random<4>(); }
static bool test_random8() { return test_random<8>(); }

typedef bool (*TestFunc)();
#define DECL_TEST(name) { #name, name }

static const struct Test {
  const char* name;
  TestFunc    func;
} tests[] = {
  DECL_TEST(test_random3),
  DECL_TEST(test_random4),
  DECL_TEST(test_random8),
  { nullptr, nullptr }
};

TEST(ExpirationTracker, main)
{
  for (const TestExpirationTracker::Test* t = tests;
       t->name != nullptr; ++t) {
    EXPECT_TRUE(t->func());
  }
}

} // namespace TestExpirationTracker
