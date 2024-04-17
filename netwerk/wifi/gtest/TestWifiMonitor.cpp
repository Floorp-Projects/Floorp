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
using ::testing::Cardinality;
using ::testing::Exactly;
using ::testing::MockFunction;
using ::testing::Sequence;

static mozilla::LazyLogModule gLog("TestWifiMonitor");
#define LOGI(x) MOZ_LOG(gLog, mozilla::LogLevel::Info, x)
#define LOGD(x) MOZ_LOG(gLog, mozilla::LogLevel::Debug, x)

namespace mozilla {

// Timeout if update not received from wifi scanner thread.
static const uint32_t kWifiScanTestResultTimeoutMs = 100;
static const uint32_t kTestWifiScanIntervalMs = 10;

// ID counter, used to make sure each call to GetAccessPointsFromWLAN
// returns "new" access points.
static int gCurrentId = 0;

static uint32_t gNumScanResults = 0;

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
    MOZ_RELEASE_ASSERT(mObs);

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
    Preferences::SetInt(WIFI_SCAN_INTERVAL_MS_PREF, kTestWifiScanIntervalMs);
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

 protected:
  bool WaitForScanResults() {
    // Wait for kWifiScanTestResultTimeoutMs to allow async calls to complete.
    bool timedout = false;
    RefPtr<CancelableRunnable> timer = NS_NewCancelableRunnableFunction(
        "WaitForScanResults Timeout", [&] { timedout = true; });
    NS_DelayedDispatchToCurrentThread(do_AddRef(timer),
                                      kWifiScanTestResultTimeoutMs);

    mozilla::SpinEventLoopUntil("TestWifiMonitor::WaitForScanResults"_ns,
                                [&]() { return timedout; });

    timer->Cancel();
    return true;
  }

  void CreateObjects() {
    mWifiMonitor = MakeRefPtr<nsWifiMonitor>(MakeUnique<MockWifiScanner>());
    EXPECT_TRUE(!mWifiMonitor->IsPolling());

    // Start with ETHERNET network type to avoid always polling at test start.
    mObs->NotifyObservers(
        nullptr, NS_NETWORK_LINK_TYPE_TOPIC,
        NS_ConvertUTF8toUTF16(NS_NETWORK_LINK_TYPE_ETHERNET).get());

    mWifiListener = new MockWifiListener();
    LOGI(("monitor: %p | scanner: %p | listener: %p", mWifiMonitor.get(),
          mWifiMonitor->mWifiScanner.get(), mWifiListener.get()));
  }

  void DestroyObjects() {
    ::testing::Mock::VerifyAndClearExpectations(
        mWifiMonitor->mWifiScanner.get());
    ::testing::Mock::VerifyAndClearExpectations(mWifiListener.get());

    // Manually disconnect observers so that the monitor can be destroyed.
    // In the browser, this would be done on xpcom-shutdown but that is sent
    // after the tests run, which is too late to avoid a gtest memory-leak
    // error.
    mWifiMonitor->Close();

    mWifiMonitor = nullptr;
    mWifiListener = nullptr;
    gCurrentId = 0;
  }

  void StartWatching(bool aRequestPolling) {
    LOGD(("StartWatching | aRequestPolling: %s | nScanResults: %u",
          aRequestPolling ? "true" : "false", gNumScanResults));
    EXPECT_TRUE(NS_SUCCEEDED(
        mWifiMonitor->StartWatching(mWifiListener, aRequestPolling)));
    WaitForScanResults();
  }

  void NotifyOfNetworkEvent(const char* aTopic, const char16_t* aData) {
    LOGD(("NotifyOfNetworkEvent: (%s, %s) | nScanResults: %u", aTopic,
          NS_ConvertUTF16toUTF8(aData).get(), gNumScanResults));
    EXPECT_TRUE(NS_SUCCEEDED(mObs->NotifyObservers(nullptr, aTopic, aData)));
    WaitForScanResults();
  }

  void StopWatching() {
    LOGD(("StopWatching | nScanResults: %u", gNumScanResults));
    EXPECT_TRUE(NS_SUCCEEDED(mWifiMonitor->StopWatching(mWifiListener)));
    WaitForScanResults();
  }

  struct MockCallSequences {
    Sequence mGetAccessPointsSeq;
    Sequence mOnChangeSeq;
    Sequence mOnErrorSeq;
  };

  void AddMockObjectChecks(const Cardinality& aScanCardinality,
                           MockCallSequences& aSeqs) {
    // Only add WillRepeatedly handler if scans is more than 0, to avoid a
    // VERY LOUD gtest warning.
    if (aScanCardinality.IsSaturatedByCallCount(0)) {
      EXPECT_CALL(
          *static_cast<MockWifiScanner*>(mWifiMonitor->mWifiScanner.get()),
          GetAccessPointsFromWLAN)
          .Times(aScanCardinality)
          .InSequence(aSeqs.mGetAccessPointsSeq);

      EXPECT_CALL(*mWifiListener, OnChange)
          .Times(aScanCardinality)
          .InSequence(aSeqs.mOnChangeSeq);
    } else {
      EXPECT_CALL(
          *static_cast<MockWifiScanner*>(mWifiMonitor->mWifiScanner.get()),
          GetAccessPointsFromWLAN)
          .Times(aScanCardinality)
          .InSequence(aSeqs.mGetAccessPointsSeq)
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

      EXPECT_CALL(*mWifiListener, OnChange)
          .Times(aScanCardinality)
          .InSequence(aSeqs.mOnChangeSeq)
          .WillRepeatedly(
              [](const nsTArray<RefPtr<nsIWifiAccessPoint>>& aAccessPoints) {
                EXPECT_TRUE(NS_IsMainThread());
                EXPECT_EQ(aAccessPoints.Length(), 1u);
                ++gNumScanResults;
                return NS_OK;
              });
    }

    EXPECT_CALL(*mWifiListener, OnError).Times(0).InSequence(aSeqs.mOnErrorSeq);
  }

  void AddStartWatchingCheck(bool aShouldPoll, MockCallSequences& aSeqs) {
    AddMockObjectChecks(aShouldPoll ? AtLeast(1) : Exactly(1), aSeqs);
  }

  void AddNetworkEventCheck(const Cardinality& aScanCardinality,
                            MockCallSequences& aSeqs) {
    AddMockObjectChecks(aScanCardinality, aSeqs);
  }

  void AddStopWatchingCheck(bool aShouldPoll, MockCallSequences& aSeqs) {
    // When polling, we may get stray scan + OnChange calls asynchronously
    // before stopping.  We may also get scan calls after stopping.
    // We check that the calls actually stopped in ConfirmStoppedCheck.
    AddMockObjectChecks(aShouldPoll ? AtLeast(0) : Exactly(0), aSeqs);
  }

  void AddConfirmStoppedCheck(MockCallSequences& aSeqs) {
    AddMockObjectChecks(Exactly(0), aSeqs);
  }

  // A Checkpoint is just a mocked function taking an int.  It will serve
  // as a temporal barrier that requires all expectations before it to be
  // satisfied and retired (meaning they won't be used in matches anymore).
  class Checkpoint {
   public:
    void Check(uint32_t aId, MockCallSequences& aSeqs) {
      EXPECT_CALL(mFn, Call(aId))
          .InSequence(aSeqs.mGetAccessPointsSeq, aSeqs.mOnChangeSeq,
                      aSeqs.mOnErrorSeq);
    }

    void Reach(uint32_t aId) { mFn.Call(aId); }

   private:
    MockFunction<void(uint32_t)> mFn;
  };

  // A single test is StartWatching, NotifyOfNetworkEvent, and StopWatching.
  void RunSingleTest(bool aRequestPolling, bool aShouldPoll,
                     const Cardinality& aScanCardinality, const char* aTopic,
                     const char16_t* aData) {
    LOGI(("RunSingleTest: <%s, %s> | requestPolling: %s | shouldPoll: %s",
          aTopic, NS_ConvertUTF16toUTF8(aData).get(),
          aRequestPolling ? "true" : "false", aShouldPoll ? "true" : "false"));
    MOZ_RELEASE_ASSERT(aShouldPoll || !aRequestPolling);

    CreateObjects();

    Checkpoint checkpoint;
    {
      // gmock expectations are asynchronous by default.  Sequence objects
      // are used here to require that expectations occur in the specified
      // (partial) order.
      MockCallSequences seqs;

      AddStartWatchingCheck(aShouldPoll, seqs);
      checkpoint.Check(1, seqs);

      AddNetworkEventCheck(aScanCardinality, seqs);
      checkpoint.Check(2, seqs);

      AddStopWatchingCheck(aShouldPoll, seqs);
      checkpoint.Check(3, seqs);

      AddConfirmStoppedCheck(seqs);
    }

    // Now run the test on the mock objects.
    StartWatching(aRequestPolling);
    checkpoint.Reach(1);
    EXPECT_EQ(mWifiMonitor->IsPolling(), aRequestPolling);

    NotifyOfNetworkEvent(aTopic, aData);
    checkpoint.Reach(2);
    EXPECT_EQ(mWifiMonitor->IsPolling(), aShouldPoll);

    StopWatching();
    checkpoint.Reach(3);
    EXPECT_TRUE(!mWifiMonitor->IsPolling());

    // Wait for extraneous calls as a way to confirm it has stopped.
    WaitForScanResults();

    DestroyObjects();
  }

  void CheckMessages(bool aRequestPolling) {
    // NS_NETWORK_LINK_TOPIC messages should cause a new scan.
    const char* kLinkTopicDatas[] = {
        NS_NETWORK_LINK_DATA_UP, NS_NETWORK_LINK_DATA_DOWN,
        NS_NETWORK_LINK_DATA_CHANGED, NS_NETWORK_LINK_DATA_UNKNOWN};

    for (const auto& data : kLinkTopicDatas) {
      RunSingleTest(aRequestPolling, aRequestPolling,
                    aRequestPolling ? AtLeast(2) : Exactly(1),
                    NS_NETWORK_LINK_TOPIC, NS_ConvertUTF8toUTF16(data).get());
    }

    // NS_NETWORK_LINK_TYPE_TOPIC should cause wifi scan polling iff the topic
    // says we have switched to a mobile network (LINK_TYPE_MOBILE or
    // LINK_TYPE_WIMAX) or we are polling the wifi-scanner (aShouldPoll).
    const LinkTypeMobility kLinkTypeTopicDatas[] = {
        {NS_NETWORK_LINK_TYPE_UNKNOWN, true /* mIsMobile */},
        {NS_NETWORK_LINK_TYPE_ETHERNET, false},
        {NS_NETWORK_LINK_TYPE_USB, false},
        {NS_NETWORK_LINK_TYPE_WIFI, false},
        {NS_NETWORK_LINK_TYPE_WIMAX, true},
        {NS_NETWORK_LINK_TYPE_MOBILE, true}};

    for (const auto& data : kLinkTypeTopicDatas) {
      bool shouldPoll = (aRequestPolling || data.mIsMobile);
      RunSingleTest(aRequestPolling, shouldPoll,
                    shouldPoll ? AtLeast(2) : Exactly(0),
                    NS_NETWORK_LINK_TYPE_TOPIC,
                    NS_ConvertUTF8toUTF16(data.mLinkType).get());
    }
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
