/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIWifiMonitor.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsIThread.h"
#include "nsIRunnable.h"
#include "nsCOMArray.h"
#include "nsIWifiMonitor.h"
#include "mozilla/ReentrantMonitor.h"
#include "prlog.h"
#include "nsIObserver.h"
#include "nsTArray.h"
#include "mozilla/Attributes.h"

#ifndef __nsWifiMonitor__
#define __nsWifiMonitor__

#if defined(PR_LOGGING)
extern PRLogModuleInfo *gWifiMonitorLog;
#endif
#define LOG(args)     PR_LOG(gWifiMonitorLog, PR_LOG_DEBUG, args)

class nsWifiAccessPoint;

class nsWifiListener
{
 public:

  nsWifiListener(nsIWifiListener* aListener)
  {
    mListener = aListener;
    mHasSentData = false;
  }
  ~nsWifiListener() {}

  nsCOMPtr<nsIWifiListener> mListener;
  bool mHasSentData;
};

class nsWifiMonitor MOZ_FINAL : nsIRunnable, nsIWifiMonitor, nsIObserver
{
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIWIFIMONITOR
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSIOBSERVER

  nsWifiMonitor();

 private:
  ~nsWifiMonitor();

  nsresult DoScan();

#if defined(XP_MACOSX)
  nsresult DoScanWithCoreWLAN();
  nsresult DoScanOld();
#endif

  nsresult CallWifiListeners(const nsCOMArray<nsWifiAccessPoint> &aAccessPoints,
                             bool aAccessPointsChanged);

  bool mKeepGoing;
  nsCOMPtr<nsIThread> mThread;

  nsTArray<nsWifiListener> mListeners;

  mozilla::ReentrantMonitor mReentrantMonitor;

};

#endif
