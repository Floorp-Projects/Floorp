/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "mozilla/UniquePtr.h"
#include "WlanLibrary.h"
#include "WifiScanner.h"

class nsIWifiAccessPoint;

namespace mozilla {

class WifiScannerImpl final : public WifiScanner {
 public:
  WifiScannerImpl();
  ~WifiScannerImpl();

  /**
   * True if there is a wifi adapter present that can perform a scan.
   */
  bool HasWifiAdapter() { return !!mWlanLibrary; }

  /**
   * GetAccessPointsFromWLAN
   *
   * Scans the available wireless interfaces for nearby access points and
   * populates the supplied collection with them
   *
   * @param accessPoints The collection to populate with available APs
   * @return NS_OK on success, failure codes on failure
   */
  nsresult GetAccessPointsFromWLAN(
      nsTArray<RefPtr<nsIWifiAccessPoint>>& accessPoints);

 private:
  mozilla::UniquePtr<WinWLANLibrary> mWlanLibrary;
};

}  // namespace mozilla
