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
// basically replaces accesspoints in the passed reference
// it lives in a separate file so that we can use objective c.
extern nsresult GetAccessPointsFromWLAN(
    nsCOMArray<nsWifiAccessPoint>& accessPoints);

nsresult nsWifiMonitor::DoScan() {
  // Regularly get the access point data.

  nsCOMArray<nsWifiAccessPoint> lastAccessPoints;
  nsCOMArray<nsWifiAccessPoint> accessPoints;

  do {
    nsresult rv = GetAccessPointsFromWLAN(accessPoints);
    if (NS_FAILED(rv)) return rv;

    bool accessPointsChanged =
        !AccessPointsEqual(accessPoints, lastAccessPoints);
    ReplaceArray(lastAccessPoints, accessPoints);

    rv = CallWifiListeners(lastAccessPoints, accessPointsChanged);
    NS_ENSURE_SUCCESS(rv, rv);

    // wait for some reasonable amount of time.  pref?
    LOG(("waiting on monitor\n"));

    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    mon.Wait(PR_SecondsToInterval(kDefaultWifiScanInterval));
  } while (mKeepGoing);

  return NS_OK;
}
