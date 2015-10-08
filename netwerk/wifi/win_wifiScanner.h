/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

// Moz headers (alphabetical)
#include "nsAutoPtr.h"
#include "nsCOMArray.h"
#include "win_wlanLibrary.h"

class nsWifiAccessPoint;

// This class allows the wifi monitor to use WinWifiScanner and WinXPWifiScanner interchangeably.
class WindowsWifiScannerInterface {
public:
  virtual ~WindowsWifiScannerInterface() {}

  virtual nsresult GetAccessPointsFromWLAN(nsCOMArray<nsWifiAccessPoint> &accessPoints) = 0;
};


class WinWifiScanner : public WindowsWifiScannerInterface {
 public:
  WinWifiScanner();
  virtual ~WinWifiScanner();

  /**
   * GetAccessPointsFromWLAN
   *
   * Scans the available wireless interfaces for nearby access points and
   * populates the supplied collection with them
   *
   * @param accessPoints The collection to populate with available APs
   * @return NS_OK on success, failure codes on failure
   */
  nsresult GetAccessPointsFromWLAN(nsCOMArray<nsWifiAccessPoint> &accessPoints);

 private:
  nsAutoPtr<WinWLANLibrary> mWlanLibrary;
};
