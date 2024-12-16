/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "nsCOMPtr.h"
#include "nsIWifiListener.h"
#include "nsWifiMonitor.h"
#include "nsWifiAccessPoint.h"
#include "WifiScanner.h"

#if defined(XP_WIN) && defined(_M_IX86)
#  include <objbase.h>  // STDMETHODCALLTYPE
#endif

// Test that removing wifi scan listeners in a wifi scan listener does
// not crash.

// Windows x86 calling convention
#if defined(XP_WIN) && defined(_M_IX86)
#  define MOCKCALLTYPE STDMETHODCALLTYPE
#else
#  define MOCKCALLTYPE
#endif

static mozilla::LazyLogModule gLog("TestWifiMonitorListenerRemoval");
#define LOGI(x) MOZ_LOG(gLog, mozilla::LogLevel::Info, x)
#define LOGD(x) MOZ_LOG(gLog, mozilla::LogLevel::Debug, x)

namespace mozilla {

// Number of callbacks called
uint32_t sCalled;

class MockWifiScanner : public mozilla::WifiScanner {
 public:
  explicit MockWifiScanner(bool aExpectOnChange)
      : mExpectOnChange(aExpectOnChange) {}

  nsresult GetAccessPointsFromWLAN(
      nsTArray<RefPtr<nsIWifiAccessPoint>>& aAccessPoints) override {
    if (!mExpectOnChange) {
      // Tell monitor to report OnError instead of OnChange.
      return NS_ERROR_FAILURE;
    }

    // Signal will be unique so we won't match the prior access
    // point list.
    static int sCurrentId = 0;
    nsWifiAccessPoint* ap = new nsWifiAccessPoint();
    ap->mSignal = sCurrentId++;
    aAccessPoints.AppendElement(RefPtr(ap));
    return NS_OK;
  }

 private:
  bool mExpectOnChange;
};

class MockWifiListener : public nsIWifiListener {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  MockWifiListener(nsWifiMonitor* aWifiMonitor, bool aExpectOnChange)
      : mWifiMonitor(aWifiMonitor), mExpectOnChange(aExpectOnChange) {}

  MOCKCALLTYPE nsresult
  OnChange(const nsTArray<RefPtr<nsIWifiAccessPoint>>& accessPoints) override {
    return Check(true);
  }

  MOCKCALLTYPE nsresult OnError(nsresult error) override {
    return Check(false);
  }

 private:
  nsresult Check(bool aWasOnChange) {
    bool gotCorrectCallback = !(mExpectOnChange ^ aWasOnChange);
    EXPECT_TRUE(gotCorrectCallback);
    if (!gotCorrectCallback) {
      return NS_OK;
    }
    // Each callback removes itself.  Test that these removals don't
    // break iteration.
    mWifiMonitor->StopWatching(this);
    ++sCalled;
    LOGI(("sCalled = %u", sCalled));
    return NS_OK;
  }

  virtual ~MockWifiListener() = default;
  RefPtr<nsWifiMonitor> mWifiMonitor;
  // If true, expect test to call OnChange.  Otherwise expect OnError.
  bool mExpectOnChange;
};

NS_IMPL_ISUPPORTS(MockWifiListener, nsIWifiListener)

// This class has friend privileges with nsWifiMonitor.
class TestWifiMonitor {
 public:
  explicit TestWifiMonitor(bool aExpectOnChange) {
    // Add two listeners so the one can stopWatching before we notify the
    // other one.
    mWifiMonitor =
        MakeRefPtr<nsWifiMonitor>(MakeUnique<MockWifiScanner>(aExpectOnChange));
    mWifiMonitor->StartWatching(
        new MockWifiListener(mWifiMonitor, aExpectOnChange),
        false /* forcePolling */);
    mWifiMonitor->StartWatching(
        new MockWifiListener(mWifiMonitor, aExpectOnChange),
        false /* forcePolling */);
  }

  ~TestWifiMonitor() {
    // Manually disconnect observers so that the monitor can be destroyed.
    // In the browser, this would be done on xpcom-shutdown but that is sent
    // after the tests run, which is too late to avoid a gtest memory-leak
    // error.
    mWifiMonitor->Close();
  }

 private:
  RefPtr<nsWifiMonitor> mWifiMonitor;
};

TEST(TestWifiMonitorListenerRemoval, RemoveDuringOnChange)
{
  sCalled = 0;
  TestWifiMonitor testWifiMonitor(true /* expectOnChange */);

  // Give monitor a chance to do a scan and report results, so that we remove
  // the listener during the results callback iteration.  Then we are done.
  mozilla::SpinEventLoopUntil(
      "TestWifiMonitorListenerRemoval::WaitForScan_OnChange"_ns, [&]() {
        MOZ_ASSERT(sCalled == 0 || sCalled == 2);
        return sCalled == 2;
      });
  EXPECT_EQ(sCalled, 2u);
}

TEST(TestWifiMonitorListenerRemoval, RemoveDuringOnError)
{
  sCalled = 0;
  TestWifiMonitor testWifiMonitor(false /* expectOnChange */);

  // Give monitor a chance to do a scan and report results, so that we remove
  // the listener during the results callback iteration.  Then we are done.
  mozilla::SpinEventLoopUntil(
      "TestWifiMonitorListenerRemoval::WaitForScan_OnError"_ns, [&]() {
        MOZ_ASSERT(sCalled == 0 || sCalled == 2);
        return sCalled == 2;
      });
  EXPECT_EQ(sCalled, 2u);
}

}  // namespace mozilla
