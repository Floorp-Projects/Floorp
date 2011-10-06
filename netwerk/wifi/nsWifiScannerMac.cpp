/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Geolocation.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * This is a derivative of work done by Google under a BSD style License.
 * See: http://gears.googlecode.com/svn/trunk/gears/geolocation/
 *
 * Contributor(s):
 *  Doug Turner <dougt@meer.net>  (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
