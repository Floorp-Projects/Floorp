/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWifiMonitor.h"

// moz headers (alphabetical)
#include "nsAutoPtr.h"
#include "nsCOMArray.h"
#include "nsComponentManagerUtils.h"
#include "nsIMutableArray.h"
#include "nsServiceManagerUtils.h"
#include "nsWifiAccessPoint.h"
#include "win_wifiScanner.h"
#include "win_xp_wifiScanner.h"
#include "mozilla/WindowsVersion.h"

using namespace mozilla;

/**
 * `nsWifiMonitor` is declared in the cross-platform nsWifiMonitor.h and
 * is mostly defined in the cross-platform nsWifiMonitor.cpp. This function
 * is implemented in various platform-specific files but the implementation
 * is almost identical in each file. We relegate the Windows-specific
 * work to the `WinWifiScanner` class and deal with non-Windows-specific
 * issues like calling listeners here. Hopefully this file can be merged
 * with the other implementations of `nsWifiMonitor::DoScan` since a lot
 * of the code is identical
 */
nsresult
nsWifiMonitor::DoScan()
{
    if (!mWinWifiScanner) {
      if (IsWin2003OrLater()) {
        mWinWifiScanner = new WinWifiScanner();
        LOG(("Using Windows 2003+ wifi scanner."));
      } else {
        mWinWifiScanner = new WinXPWifiScanner();
        LOG(("Using Windows XP wifi scanner."));
      }

      if (!mWinWifiScanner) {
        // TODO: Probably return OOM error
        return NS_ERROR_FAILURE;
      }
    }

    // Regularly get the access point data.

    nsCOMArray<nsWifiAccessPoint> lastAccessPoints;
    nsCOMArray<nsWifiAccessPoint> accessPoints;

    do {
      accessPoints.Clear();
      nsresult rv = mWinWifiScanner->GetAccessPointsFromWLAN(accessPoints);
      if (NS_FAILED(rv)) {
        return rv;
      }

      bool accessPointsChanged = !AccessPointsEqual(accessPoints, lastAccessPoints);
      ReplaceArray(lastAccessPoints, accessPoints);

      rv = CallWifiListeners(lastAccessPoints, accessPointsChanged);
      NS_ENSURE_SUCCESS(rv, rv);

      // wait for some reasonable amount of time.  pref?
      LOG(("waiting on monitor\n"));

      ReentrantMonitorAutoEnter mon(mReentrantMonitor);
      if (mKeepGoing) {
          mon.Wait(PR_SecondsToInterval(kDefaultWifiScanInterval));
      }
    }
    while (mKeepGoing);

    return NS_OK;
}
