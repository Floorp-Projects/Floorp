/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSWIFIAPSCANNERDBUS_H_
#define NSWIFIAPSCANNERDBUS_H_

#define DBUS_API_SUBJECT_TO_CHANGE
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>

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
  nsresult SendGetDevices(AccessPointArray& accessPoints);
  nsresult SendGetDeviceType(const char* aPath, AccessPointArray& accessPoints);
  nsresult SendGetAccessPoints(const char* aPath,
                               AccessPointArray& accessPoints);
  nsresult SendGetAPProperties(const char* aPath,
                               AccessPointArray& accessPoints);
  nsresult IdentifyDevices(DBusMessage* aMsg, AccessPointArray& accessPoints);
  nsresult IdentifyDeviceType(DBusMessage* aMsg, const char* aDevicePath,
                              AccessPointArray& accessPoints);
  nsresult IdentifyAccessPoints(DBusMessage* aMsg,
                                AccessPointArray& accessPoints);
  nsresult IdentifyAPProperties(DBusMessage* aMsg,
                                AccessPointArray& accessPoints);
  nsresult StoreSsid(DBusMessageIter* aVariant, nsWifiAccessPoint* aAp);
  nsresult SetMac(DBusMessageIter* aVariant, nsWifiAccessPoint* aAp);
  nsresult GetDBusIterator(DBusMessage* aMsg, DBusMessageIter* aIterArray);

  RefPtr<DBusConnection> mConnection;
};

}  // namespace mozilla

#endif  // NSWIFIAPSCANNERDBUS_H_
