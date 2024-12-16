/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsWifiMonitor__
#define __nsWifiMonitor__

#include "nsIWifiMonitor.h"
#include "nsCOMPtr.h"
#include "nsProxyRelease.h"
#include "nsIThread.h"
#include "nsIRunnable.h"
#include "nsCOMArray.h"
#include "nsIWifiListener.h"
#include "mozilla/Atomics.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/Logging.h"
#include "nsIObserver.h"
#include "nsTArray.h"
#include "mozilla/Attributes.h"
#include "mozilla/Monitor.h"
#include "WifiScanner.h"
#include "nsTHashMap.h"

namespace mozilla {
class TestWifiMonitor;
}

extern mozilla::LazyLogModule gWifiMonitorLog;

class nsWifiAccessPoint;

// Period between scans when on mobile network.
#define WIFI_SCAN_INTERVAL_MS_PREF "network.wifi.scanning_period"

#ifdef XP_MACOSX
// Use a larger stack size for the wifi monitor thread of macOS, to
// accommodate Core WLAN making large stack allocations.
#  define kMacOSWifiMonitorStackSize (512 * 1024)
#endif

struct WifiListenerData {
  bool mShouldPoll;
  bool mHasSentData = false;

  explicit WifiListenerData(bool aShouldPoll = false)
      : mShouldPoll(aShouldPoll) {}
};

class nsWifiMonitor final : public nsIWifiMonitor, public nsIObserver {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIWIFIMONITOR
  NS_DECL_NSIOBSERVER

  explicit nsWifiMonitor(
      mozilla::UniquePtr<mozilla::WifiScanner>&& aScanner = nullptr);

 private:
  friend class mozilla::TestWifiMonitor;

  ~nsWifiMonitor();

  void EnsureWifiScanner();

  nsresult DispatchScanToBackgroundThread(uint64_t aPollingId = 0,
                                          uint32_t aWaitMs = 0);

  void Scan(uint64_t aPollingId);
  nsresult DoScan();

  nsresult CallWifiListeners(
      const nsTArray<RefPtr<nsIWifiAccessPoint>>& aAccessPoints,
      bool aAccessPointsChanged);

  nsresult PassErrorToWifiListeners(nsresult rv);

  void Close();

  bool IsBackgroundThread();

  bool ShouldPoll() {
    MOZ_ASSERT(!IsBackgroundThread());
    return (mShouldPollForCurrentNetwork && !mListeners.IsEmpty()) ||
           mNumPollingListeners > 0;
  };

  template <typename CallbackFn>
  nsresult NotifyListeners(CallbackFn&& aCallback);

#ifdef ENABLE_TESTS
  // Test-only function that confirms we "should" be polling.  May be wrong
  // if somehow the polling tasks are not set to run on the background
  // thread.
  bool IsPolling() { return mThread && mPollingId; }
#endif

  // Main thread only.
  nsCOMPtr<nsIThread> mThread;

  // Main thread only.
  nsTHashMap<RefPtr<nsIWifiListener>, WifiListenerData> mListeners;

  // Background thread only.
  mozilla::UniquePtr<mozilla::WifiScanner> mWifiScanner;

  // Background thread only.  Sorted.
  nsTArray<RefPtr<nsIWifiAccessPoint>> mLastAccessPoints;

  // Wifi-scanning requests may poll, meaning they will run repeatedly on
  // a scheduled time period.  If this value is 0 then polling is not running,
  // otherwise, it indicates the "ID" of the polling that is running.  if some
  // other polling (with different ID) is running, it will stop, not iterate.
  mozilla::Atomic<uint64_t> mPollingId;

  // Number of current listeners that requested that the wifi scan poll
  // periodically.
  // Main thread only.
  uint32_t mNumPollingListeners = 0;

  // True if the current network type is one that requires polling
  // (i.e. a "mobile" network type).
  // Main thread only.
  bool mShouldPollForCurrentNetwork = false;
};

#endif
