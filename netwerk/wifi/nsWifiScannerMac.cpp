/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <mach-o/dyld.h>
#include <dlfcn.h>
#include <unistd.h>

#include "osx_wifi.h"

#include "nsAutoPtr.h"
#include "nsCOMArray.h"
#include "nsWifiMonitor.h"
#include "nsWifiAccessPoint.h"

#include "nsServiceManagerUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsIMutableArray.h"

using namespace mozilla;

// defined in osx_corewlan.mm
// basically relaces accesspoints in the passed reference
// it lives in a separate file so that we can use objective c.
extern nsresult GetAccessPointsFromWLAN(nsCOMArray<nsWifiAccessPoint> &accessPoints);

nsresult
nsWifiMonitor::DoScanWithCoreWLAN()
{
  // Regularly get the access point data.

  nsCOMArray<nsWifiAccessPoint> lastAccessPoints;
  nsCOMArray<nsWifiAccessPoint> accessPoints;

  do {
    nsresult rv = GetAccessPointsFromWLAN(accessPoints);
    if (NS_FAILED(rv))
      return rv;

    bool accessPointsChanged = !AccessPointsEqual(accessPoints, lastAccessPoints);
    ReplaceArray(lastAccessPoints, accessPoints);

    rv = CallWifiListeners(lastAccessPoints, accessPointsChanged);
    NS_ENSURE_SUCCESS(rv, rv);

    // wait for some reasonable amount of time.  pref?
    LOG(("waiting on monitor\n"));

    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    mon.Wait(PR_SecondsToInterval(60));
  }
  while (mKeepGoing);

  return NS_OK;
}

nsresult
nsWifiMonitor::DoScanOld()
{
  void *apple_80211_library = dlopen(
      "/System/Library/PrivateFrameworks/Apple80211.framework/Apple80211",
      RTLD_LAZY);
  if (!apple_80211_library)
    return NS_ERROR_NOT_AVAILABLE;

  WirelessContextPtr wifi_context_;

  WirelessAttachFunction WirelessAttach_function_ = reinterpret_cast<WirelessAttachFunction>(dlsym(apple_80211_library, "WirelessAttach"));
  WirelessScanSplitFunction WirelessScanSplit_function_ = reinterpret_cast<WirelessScanSplitFunction>(dlsym(apple_80211_library, "WirelessScanSplit"));
  WirelessDetachFunction WirelessDetach_function_ = reinterpret_cast<WirelessDetachFunction>(dlsym(apple_80211_library, "WirelessDetach"));

  if (!WirelessAttach_function_ || !WirelessScanSplit_function_ || !WirelessDetach_function_) {
    dlclose(apple_80211_library);
    return NS_ERROR_NOT_AVAILABLE;
  }

  WIErr err = 0;

  err = (*WirelessAttach_function_)(&wifi_context_, 0);
  if (err != noErr) {
    printf("Error: WirelessAttach: %d\n", (int) err);
    dlclose(apple_80211_library);
    return NS_ERROR_FAILURE;
  }

  // Regularly get the access point data.

  nsCOMArray<nsWifiAccessPoint> lastAccessPoints;
  nsCOMArray<nsWifiAccessPoint> accessPoints;

  do {
    accessPoints.Clear();

    CFArrayRef managed_access_points = NULL;
    CFArrayRef adhoc_access_points = NULL;

    if ((*WirelessScanSplit_function_)(wifi_context_,
                                      &managed_access_points,
                                      &adhoc_access_points,
                                      0) != noErr) {
      return NS_ERROR_FAILURE;
    }

    if (managed_access_points == NULL) {
      return NS_ERROR_FAILURE;
    }

    int accessPointsCount = CFArrayGetCount(managed_access_points);

    for (int i = 0; i < accessPointsCount; ++i) {

      nsWifiAccessPoint* ap = new nsWifiAccessPoint();
      if (!ap)
        continue;

      const WirelessNetworkInfo *access_point_info =
        reinterpret_cast<const WirelessNetworkInfo*>(CFDataGetBytePtr(reinterpret_cast<const CFDataRef>(CFArrayGetValueAtIndex(managed_access_points, i))));

      ap->setMac(access_point_info->macAddress);

      // WirelessNetworkInfo::signal appears to be signal strength in dBm.
      ap->setSignal(access_point_info->signal);

      ap->setSSID(reinterpret_cast<const char*>(access_point_info->name),
                  access_point_info->nameLen);

      accessPoints.AppendObject(ap);
    }

    bool accessPointsChanged = !AccessPointsEqual(accessPoints, lastAccessPoints);
    ReplaceArray(lastAccessPoints, accessPoints);

    nsresult rv = CallWifiListeners(lastAccessPoints, accessPointsChanged);
    if (NS_FAILED(rv)) {
        dlclose(apple_80211_library);
        return rv;
    }

    // wait for some reasonable amount of time.  pref?
    LOG(("waiting on monitor\n"));

    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    mon.Wait(PR_SecondsToInterval(60));
  }
  while (mKeepGoing);

  (*WirelessDetach_function_)(wifi_context_);

  dlclose(apple_80211_library);

  return NS_OK;
}

nsresult
nsWifiMonitor::DoScan()
{
  nsresult rv = DoScanWithCoreWLAN();

  // we can remove this once we stop caring about 10.5.
  if (NS_FAILED(rv))
    rv = DoScanOld();

  return rv;
}
