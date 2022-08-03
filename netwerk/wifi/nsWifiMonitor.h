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

#ifdef XP_WIN
#  include "win_wifiScanner.h"
#endif

extern mozilla::LazyLogModule gWifiMonitorLog;
#define LOG(args) MOZ_LOG(gWifiMonitorLog, mozilla::LogLevel::Debug, args)

class nsWifiAccessPoint;

#define kDefaultWifiScanInterval 5 /* seconds */

#ifdef XP_MACOSX
// Use a larger stack size for the monitor thread on macOS 13+
// to accommodate Core WLAN making large stack allocations.
#  define kMacOS13MonitorStackSize (512 * 1024)
#endif

class nsWifiListener {
 public:
  explicit nsWifiListener(nsMainThreadPtrHolder<nsIWifiListener>* aListener) {
    mListener = aListener;
    mHasSentData = false;
  }
  ~nsWifiListener() = default;

  nsMainThreadPtrHandle<nsIWifiListener> mListener;
  bool mHasSentData;
};

class nsWifiMonitor final : nsIRunnable, nsIWifiMonitor, nsIObserver {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIWIFIMONITOR
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSIOBSERVER

  nsWifiMonitor();

 private:
  ~nsWifiMonitor() = default;

  nsresult DoScan();

  nsresult CallWifiListeners(const nsCOMArray<nsWifiAccessPoint>& aAccessPoints,
                             bool aAccessPointsChanged);

  uint32_t GetMonitorThreadStackSize();

  mozilla::Atomic<bool> mKeepGoing;
  mozilla::Atomic<bool> mThreadComplete;
  nsCOMPtr<nsIThread> mThread;  // only accessed on MainThread

  nsTArray<nsWifiListener> mListeners MOZ_GUARDED_BY(mReentrantMonitor);

  mozilla::ReentrantMonitor mReentrantMonitor;

#ifdef XP_WIN
  mozilla::UniquePtr<WinWifiScanner> mWinWifiScanner;
#endif
};

#endif
