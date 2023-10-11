/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSWIFIAPSCANNERDBUS_H_
#define NSWIFIAPSCANNERDBUS_H_

#include "WifiScanner.h"

class nsIWifiAccessPoint;
class nsWifiAccessPoint;

namespace mozilla {

using AccessPointArray = nsTArray<RefPtr<nsIWifiAccessPoint>>;

class WifiScannerImpl final : public WifiScanner {
 public:
  explicit WifiScannerImpl();
  ~WifiScannerImpl();

  /**
   * GetAccessPointsFromWLAN
   *
   * Scans the available wireless interfaces for nearby access points and
   * populates the supplied collection with them
   *
   * @param accessPoints The collection to populate with available APs
   * @return NS_OK on success, failure codes on failure
   */
  nsresult GetAccessPointsFromWLAN(AccessPointArray& accessPoints);

 private:
  bool AddDevice(const char* aDevicePath, AccessPointArray& aAccessPoints);
  bool AddAPProperties(const char* aApPath, AccessPointArray& aAccessPoints);
  bool SetMac(char* aHwAddress, nsWifiAccessPoint* aAp);
};

}  // namespace mozilla

#endif  // NSWIFIAPSCANNERDBUS_H_
