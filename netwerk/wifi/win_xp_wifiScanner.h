/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WINXPWIFISCANNER_H_
#define WINXPWIFISCANNER_H_

#include "nsAutoPtr.h"
#include "nsCOMArray.h"
#include "win_wifiScanner.h"

class nsWifiAccessPoint;
class WindowsNdisApi;

// This class is wrapper into the Chromium WindowNdisApi class for scanning wifis
// on Windows XP. When Firefox drops XP support, this code can go.
class WinXPWifiScanner : public WindowsWifiScannerInterface {
public:
  nsresult GetAccessPointsFromWLAN(nsCOMArray<nsWifiAccessPoint> &accessPoints);
  virtual ~WinXPWifiScanner() {}
private:
  nsAutoPtr<WindowsNdisApi> mImplementation;
};

#endif