/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsXPCOM.h"
#include "nsXPCOMCID.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsWifiMonitor.h"
#include "nsWifiAccessPoint.h"

#include "nsServiceManagerUtils.h"
#include "nsComponentManagerUtils.h"
#include "mozilla/Services.h"

#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"

using namespace mozilla;

#if defined(PR_LOGGING)
PRLogModuleInfo *gWifiMonitorLog;
#endif

NS_IMPL_ISUPPORTS(nsWifiMonitor,
                  nsIWifiMonitor,
                  nsIObserver,
                  nsIWifiScanResultsReady)

nsWifiMonitor::nsWifiMonitor()
{
#if defined(PR_LOGGING)
  gWifiMonitorLog = PR_NewLogModule("WifiMonitor");
#endif

  nsCOMPtr<nsIObserverService> obsSvc = mozilla::services::GetObserverService();
  if (obsSvc) {
    obsSvc->AddObserver(this, "xpcom-shutdown", false);
  }
  LOG(("@@@@@ wifimonitor created\n"));
}

nsWifiMonitor::~nsWifiMonitor()
{
}

NS_IMETHODIMP
nsWifiMonitor::StartWatching(nsIWifiListener *aListener)
{
  LOG(("@@@@@ nsWifiMonitor::StartWatching\n"));
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  if (!aListener) {
    return NS_ERROR_NULL_POINTER;
  }

  mListeners.AppendElement(nsWifiListener(new nsMainThreadPtrHolder<nsIWifiListener>(aListener)));

  if (!mTimer) {
    mTimer = do_CreateInstance("@mozilla.org/timer;1");
    mTimer->Init(this, 5000, nsITimer::TYPE_REPEATING_SLACK);
  }
  StartScan();
  return NS_OK;
}

NS_IMETHODIMP
nsWifiMonitor::StopWatching(nsIWifiListener *aListener)
{
  LOG(("@@@@@ nsWifiMonitor::StopWatching\n"));
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  if (!aListener) {
    return NS_ERROR_NULL_POINTER;
  }

  for (uint32_t i = 0; i < mListeners.Length(); i++) {
    if (mListeners[i].mListener == aListener) {
      mListeners.RemoveElementAt(i);
      break;
    }
  }

  if (mListeners.Length() == 0) {
    ClearTimer();
  }
  return NS_OK;
}

void
nsWifiMonitor::StartScan()
{
  nsCOMPtr<nsIInterfaceRequestor> ir = do_GetService("@mozilla.org/telephony/system-worker-manager;1");
  nsCOMPtr<nsIWifi> wifi = do_GetInterface(ir);
  if (!wifi) {
    return;
  }
  wifi->GetWifiScanResults(this);
}

NS_IMETHODIMP
nsWifiMonitor::Observe(nsISupports *subject, const char *topic,
                       const char16_t *data)
{
  if (!strcmp(topic, "timer-callback")) {
    LOG(("timer callback\n"));
    StartScan();
    return NS_OK;
  }

  if (!strcmp(topic, "xpcom-shutdown")) {
    LOG(("Shutting down\n"));
    ClearTimer();
    return NS_OK;
  }

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsWifiMonitor::Onready(uint32_t count, nsIWifiScanResult **results)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  LOG(("@@@@@ About to send data to the wifi listeners\n"));

  nsCOMArray<nsWifiAccessPoint> accessPoints;

  for (uint32_t i = 0; i < count; i++) {
    nsRefPtr<nsWifiAccessPoint> ap = new nsWifiAccessPoint();

    nsString temp;
    results[i]->GetBssid(temp);
    //   00:00:00:00:00:00 --> 00-00-00-00-00-00
    for (int32_t x=0; x<6; x++) {
      temp.ReplaceSubstring(NS_LITERAL_STRING(":"), NS_LITERAL_STRING("-")); // would it be too much to ask for a ReplaceAll()?
    }

    nsCString mac;
    mac.AssignWithConversion(temp);

    results[i]->GetSsid(temp);

    nsCString ssid;
    ssid.AssignWithConversion(temp);

    uint32_t signal;
    results[i]->GetSignalStrength(&signal);

    ap->setSignal(signal);
    ap->setMacRaw(mac.get());
    ap->setSSIDRaw(ssid.get(), ssid.Length());

    accessPoints.AppendObject(ap);
  }

  bool accessPointsChanged = !AccessPointsEqual(accessPoints, mLastAccessPoints);
  ReplaceArray(mLastAccessPoints, accessPoints);

  nsTArray<nsIWifiAccessPoint*> ac;
  uint32_t resultCount = mLastAccessPoints.Count();
  for (uint32_t i = 0; i < resultCount; i++) {
    ac.AppendElement(mLastAccessPoints[i]);
  }

  for (uint32_t i = 0; i < mListeners.Length(); i++) {
    if (!mListeners[i].mHasSentData || accessPointsChanged) {
      mListeners[i].mHasSentData = true;
      mListeners[i].mListener->OnChange(ac.Elements(), ac.Length());
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWifiMonitor::Onfailure()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  LOG(("@@@@@ About to send error to the wifi listeners\n"));
  for (uint32_t i = 0; i < mListeners.Length(); i++) {
    mListeners[i].mListener->OnError(NS_ERROR_UNEXPECTED);
  }

  ClearTimer();
  return NS_OK;
}
