/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "nsIWifiListener.h"
#include "nsWifiMonitor.h"
#include "nsWifiAccessPoint.h"
#include "WifiScanner.h"
#include "nsCOMPtr.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "nsIObserverService.h"
#include "nsINetworkLinkService.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"

#if defined(XP_WIN) && defined(_M_IX86)
#  include <objbase.h>  // STDMETHODCALLTYPE
#endif

// Tests that wifi scanning happens on the right network change events,
// and that wifi-scan polling is operable on mobile networks.

using ::testing::AtLeast;
using ::testing::Exactly;

static mozilla::LazyLogModule gLog("TestWifiMonitor");
#define LOGI(x) MOZ_LOG(gLog, mozilla::LogLevel::Info, x)
#define LOGD(x) MOZ_LOG(gLog, mozilla::LogLevel::Debug, x)

namespace mozilla {

// Timeout if no updates are received for 1s.
static const uint32_t WIFI_SCAN_TEST_RESULT_TIMEOUT_MS = 1000;

// ID counter, used to make sure each call to GetAccessPointsFromWLAN
// returns "new" access points.
static int gCurrentId = 0;

static uint32_t gNumScanResults = 0;

template <typename T>
bool IsNonZero(const T&) {
  return true;
}

template <>
bool IsNonZero<int>(const int& aVal) {
  return aVal != 0;
}

struct LinkTypeMobility {
  const char* mLinkType;
  bool mIsMobile;
};

class MockWifiScanner : public WifiScanner {
 public:
  MOCK_METHOD(nsresult, GetAccessPointsFromWLAN,
              (nsTArray<RefPtr<nsIWifiAccessPoint>> & aAccessPoints),
              (override));
};

class MockWifiListener : public nsIWifiListener {
  virtual ~MockWifiListener() = default;

 public:
  NS_DECL_THREADSAFE_ISUPPORTS
#if defined(XP_WIN) && defined(_M_IX86)
  MOCK_METHOD(nsresult, OnChange,
              (const nsTArray<RefPtr<nsIWifiAccessPoint>>& accessPoints),
              (override, Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(nsresult, OnError, (nsresult error),
              (override, Calltype(STDMETHODCALLTYPE)));
#else
  MOCK_METHOD(nsresult, OnChange,
              (const nsTArray<RefPtr<nsIWifiAccessPoint>>& accessPoints),
              (override));
  MOCK_METHOD(nsresult, OnError, (nsresult error), (override));
#endif
};

NS_IMPL_ISUPPORTS(MockWifiListener, nsIWifiListener)

class TestWifiMonitor : public ::testing::Test {
 public:
  TestWifiMonitor() {
    mObs = mozilla::services::GetObserverService();
    MOZ_ASSERT(mObs);

    nsresult rv;
    nsCOMPtr<nsINetworkLinkService> nls =
        do_GetService(NS_NETWORK_LINK_SERVICE_CONTRACTID, &rv);
    EXPECT_TRUE(NS_SUCCEEDED(rv));
    EXPECT_TRUE(nls);
    rv = nls->GetLinkType(&mOrigLinkType);
    EXPECT_TRUE(NS_SUCCEEDED(rv));
    rv = nls->GetIsLinkUp(&mOrigIsLinkUp);
    EXPECT_TRUE(NS_SUCCEEDED(rv));
    rv = nls->GetLinkStatusKnown(&mOrigLinkStatusKnown);
    EXPECT_TRUE(NS_SUCCEEDED(rv));

    // Reduce wifi-polling interval.  0 turns polling off.
    mOldScanInterval = Preferences::GetInt(WIFI_SCAN_INTERVAL_MS_PREF);
    Preferences::SetInt(WIFI_SCAN_INTERVAL_MS_PREF, 10);
  }

  ~TestWifiMonitor() {
    Preferences::SetInt(WIFI_SCAN_INTERVAL_MS_PREF, mOldScanInterval);

    // Restore network link type
    const char* linkType = nullptr;
    switch (mOrigLinkType) {
      case nsINetworkLinkService::LINK_TYPE_UNKNOWN:
        linkType = NS_NETWORK_LINK_TYPE_UNKNOWN;
        break;
      case nsINetworkLinkService::LINK_TYPE_ETHERNET:
        linkType = NS_NETWORK_LINK_TYPE_ETHERNET;
        break;
      case nsINetworkLinkService::LINK_TYPE_USB:
        linkType = NS_NETWORK_LINK_TYPE_USB;
        break;
      case nsINetworkLinkService::LINK_TYPE_WIFI:
        linkType = NS_NETWORK_LINK_TYPE_WIFI;
        break;
      case nsINetworkLinkService::LINK_TYPE_MOBILE:
        linkType = NS_NETWORK_LINK_TYPE_MOBILE;
        break;
      case nsINetworkLinkService::LINK_TYPE_WIMAX:
        linkType = NS_NETWORK_LINK_TYPE_WIMAX;
        break;
    }
    EXPECT_TRUE(linkType);
    mObs->NotifyObservers(nullptr, NS_NETWORK_LINK_TYPE_TOPIC,
                          NS_ConvertUTF8toUTF16(linkType).get());

    const char* linkStatus = nullptr;
    if (mOrigLinkStatusKnown) {
      if (mOrigIsLinkUp) {
        linkStatus = NS_NETWORK_LINK_DATA_UP;
      } else {
        linkStatus = NS_NETWORK_LINK_DATA_DOWN;
      }
    } else {
      linkStatus = NS_NETWORK_LINK_DATA_UNKNOWN;
    }
    EXPECT_TRUE(linkStatus);
    mObs->NotifyObservers(nullptr, NS_NETWORK_LINK_TOPIC,
                          NS_ConvertUTF8toUTF16(linkStatus).get());
  }

  void SetUp() override {
    mWifiMonitor = MakeRefPtr<nsWifiMonitor>(MakeUnique<MockWifiScanner>());
    EXPECT_TRUE(!mWifiMonitor->IsPolling());

    mWifiListener = new MockWifiListener();
    LOGI(("monitor: %p | scanner: %p | listener: %p", mWifiMonitor.get(),
          mWifiMonitor->mWifiScanner.get(), mWifiListener.get()));
  }

  void TearDown() override {
    ::testing::Mock::VerifyAndClearExpectations(
        mWifiMonitor->mWifiScanner.get());
    ::testing::Mock::VerifyAndClearExpectations(mWifiListener.get());

    // Manually disconnect observers so that the monitor can be destroyed.
    // This would normally be done on xpcom-shutdown but that is sent after
    // the tests run, which is too late to avoid a gtest memory-leak error.
    mWifiMonitor->Close();

    mWifiMonitor = nullptr;
    mWifiListener = nullptr;
    gCurrentId = 0;
  }

 protected:
  // Fn must be the type of a parameterless function that returns a boolean
  // indicating success.
  template <typename Card1, typename Card2, typename Card3, typename Fn>
  void CheckAsync(Card1&& nScans, Card2&& nChanges, Card3&& nErrors, Fn fn) {
    // Only add WillRepeatedly handler if scans is more than 0, to avoid a
    // VERY LOUD gtest warning.
    if (IsNonZero(nScans)) {
      EXPECT_CALL(
          *static_cast<MockWifiScanner*>(mWifiMonitor->mWifiScanner.get()),
          GetAccessPointsFromWLAN)
          .Times(nScans)
          .WillRepeatedly(
              [](nsTArray<RefPtr<nsIWifiAccessPoint>>& aAccessPoints) {
                EXPECT_TRUE(!NS_IsMainThread());
                EXPECT_TRUE(aAccessPoints.IsEmpty());
                nsWifiAccessPoint* ap = new nsWifiAccessPoint();
                // Signal will be unique so we won't match the prior access
                // point list.
                ap->mSignal = gCurrentId++;
                aAccessPoints.AppendElement(RefPtr(ap));
                return NS_OK;
              });
    } else {
      EXPECT_CALL(
          *static_cast<MockWifiScanner*>(mWifiMonitor->mWifiScanner.get()),
          GetAccessPointsFromWLAN)
          .Times(nScans);
    }

    if (IsNonZero(nChanges)) {
      EXPECT_CALL(*mWifiListener, OnChange)
          .Times(nChanges)
          .WillRepeatedly(
              [](const nsTArray<RefPtr<nsIWifiAccessPoint>>& aAccessPoints) {
                EXPECT_TRUE(NS_IsMainThread());
                EXPECT_EQ(aAccessPoints.Length(), 1u);
                ++gNumScanResults;
                return NS_OK;
              });
    } else {
      EXPECT_CALL(*mWifiListener, OnChange).Times(nChanges);
    }

    if (IsNonZero(nErrors)) {
      EXPECT_CALL(*mWifiListener, OnError)
          .Times(nErrors)
          .WillRepeatedly([](nsresult aError) {
            EXPECT_TRUE(NS_IsMainThread());
            return NS_OK;
          });
    } else {
      EXPECT_CALL(*mWifiListener, OnError).Times(nErrors);
    }

    EXPECT_TRUE(fn());
  }

  void CheckStartWatching(bool aShouldPoll) {
    LOGI(("CheckStartWatching | aShouldPoll: %s",
          aShouldPoll ? "true" : "false"));
    CheckAsync(aShouldPoll ? AtLeast(1) : Exactly(1) /* nScans */,
               aShouldPoll ? AtLeast(1) : Exactly(1) /* nChanges */,
               0 /* nErrors */, [&] {
                 return NS_SUCCEEDED(mWifiMonitor->StartWatching(
                            mWifiListener, aShouldPoll)) &&
                        WaitForScanResult(1);
               });
  }

  void CheckStopWatching() {
    LOGI(("CheckStopWatching"));
    CheckAsync(0 /* nScans */, 0 /* nChanges */, 0 /* nErrors */, [&] {
      return NS_SUCCEEDED(mWifiMonitor->StopWatching(mWifiListener));
    });
  }

  void CheckNotifyAndNoScanResults(const char* aTopic, const char16_t* aData) {
    LOGI(("CheckNotifyAndNoScanResults: (%s, %s)", aTopic,
          NS_ConvertUTF16toUTF8(aData).get()));
    CheckAsync(0 /* nScans */, 0 /* nChanges */, 0 /* nErrors */, [&] {
      return NS_SUCCEEDED(mObs->NotifyObservers(nullptr, aTopic, aData));
    });
  }

  void CheckNotifyAndWaitForScanResult(const char* aTopic,
                                       const char16_t* aData) {
    LOGI(("CheckNotifyAndWaitForScanResult: (%s, %s)", aTopic,
          NS_ConvertUTF16toUTF8(aData).get()));
    CheckAsync(
        AtLeast(1) /* nScans */, AtLeast(1) /* nChanges */, 0 /* nErrors */,
        [&] {
          return NS_SUCCEEDED(mObs->NotifyObservers(nullptr, aTopic, aData)) &&
                 WaitForScanResult(1);
        });
  }

  void CheckNotifyAndWaitForPollingScans(const char* aTopic,
                                         const char16_t* aData) {
    LOGI(("CheckNotifyAndWaitForPollingScans: (%s, %s)", aTopic,
          NS_ConvertUTF16toUTF8(aData).get()));
    CheckAsync(
        AtLeast(2) /* nScans */, AtLeast(2) /* nChanges */, 0 /* nErrors */,
        [&] {
          // Wait for more than one scan.
          return NS_SUCCEEDED(mObs->NotifyObservers(nullptr, aTopic, aData)) &&
                 WaitForScanResult(2);
        });
  }

  void CheckMessages(bool aShouldPoll) {
    // NS_NETWORK_LINK_TOPIC messages should cause a new scan.
    const char* kLinkTopicDatas[] = {
        NS_NETWORK_LINK_DATA_UP, NS_NETWORK_LINK_DATA_DOWN,
        NS_NETWORK_LINK_DATA_CHANGED, NS_NETWORK_LINK_DATA_UNKNOWN};

    auto checkFn = aShouldPoll
                       ? &TestWifiMonitor::CheckNotifyAndWaitForPollingScans
                       : &TestWifiMonitor::CheckNotifyAndWaitForScanResult;

    for (const auto& data : kLinkTopicDatas) {
      CheckStartWatching(aShouldPoll);
      EXPECT_EQ(mWifiMonitor->IsPolling(), aShouldPoll);
      (this->*checkFn)(NS_NETWORK_LINK_TOPIC,
                       NS_ConvertUTF8toUTF16(data).get());
      EXPECT_EQ(mWifiMonitor->IsPolling(), aShouldPoll);
      CheckStopWatching();
      EXPECT_TRUE(!mWifiMonitor->IsPolling());
    }

    // NS_NETWORK_LINK_TYPE_TOPIC should cause wifi scan polling iff the topic
    // says we have switched to a mobile network (LINK_TYPE_MOBILE or
    // LINK_TYPE_WIMAX) or we are polling the wifi-scanner (aShouldPoll).
    const LinkTypeMobility kLinkTypeTopicDatas[] = {
        {NS_NETWORK_LINK_TYPE_UNKNOWN, false /* mIsMobile */},
        {NS_NETWORK_LINK_TYPE_ETHERNET, false},
        {NS_NETWORK_LINK_TYPE_USB, false},
        {NS_NETWORK_LINK_TYPE_WIFI, false},
        {NS_NETWORK_LINK_TYPE_WIMAX, true},
        {NS_NETWORK_LINK_TYPE_MOBILE, true}};

    for (const auto& data : kLinkTypeTopicDatas) {
      bool shouldPoll = (aShouldPoll || data.mIsMobile);
      checkFn = shouldPoll ? &TestWifiMonitor::CheckNotifyAndWaitForPollingScans
                           : &TestWifiMonitor::CheckNotifyAndNoScanResults;

      CheckStartWatching(aShouldPoll);
      (this->*checkFn)(NS_NETWORK_LINK_TYPE_TOPIC,
                       NS_ConvertUTF8toUTF16(data.mLinkType).get());
      EXPECT_EQ(mWifiMonitor->IsPolling(), shouldPoll);
      CheckStopWatching();
      EXPECT_TRUE(!mWifiMonitor->IsPolling());
    }
  }

  bool WaitForScanResult(uint32_t nScansToWaitFor) {
    uint32_t oldScanResults = gNumScanResults;
    auto startTime = TimeStamp::Now();

    return mozilla::SpinEventLoopUntil(
        "TestWifiMonitor::WaitForScanResult"_ns, [&]() {
          LOGD(
              ("gNumScanResults: %d | oldScanResults: %d | "
               "nScansToWaitFor: %d | timeMs: %d",
               (int)gNumScanResults, (int)oldScanResults, (int)nScansToWaitFor,
               (int)(TimeStamp::Now() - startTime).ToMilliseconds()));
          return gNumScanResults >= (oldScanResults + nScansToWaitFor) ||
                 (TimeStamp::Now() - startTime).ToMilliseconds() >
                     WIFI_SCAN_TEST_RESULT_TIMEOUT_MS * nScansToWaitFor;
        });
  }

  RefPtr<nsWifiMonitor> mWifiMonitor;
  nsCOMPtr<nsIObserverService> mObs;

  RefPtr<MockWifiListener> mWifiListener;

  int mOldScanInterval;
  uint32_t mOrigLinkType = 0;
  bool mOrigIsLinkUp = false;
  bool mOrigLinkStatusKnown = false;
};

TEST_F(TestWifiMonitor, WifiScanNoPolling) { CheckMessages(false); }

TEST_F(TestWifiMonitor, WifiScanPolling) { CheckMessages(true); }

}  // namespace mozilla
