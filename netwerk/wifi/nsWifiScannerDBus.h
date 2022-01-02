/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSWIFIAPSCANNERDBUS_H_
#define NSWIFIAPSCANNERDBUS_H_

#include "nsCOMArray.h"

#define DBUS_API_SUBJECT_TO_CHANGE
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>

class nsWifiAccessPoint;

namespace mozilla {

class nsWifiScannerDBus final {
 public:
  explicit nsWifiScannerDBus(nsCOMArray<nsWifiAccessPoint>* aAccessPoints);
  ~nsWifiScannerDBus();

  nsresult Scan();

 private:
  nsresult SendGetDevices();
  nsresult SendGetDeviceType(const char* aPath);
  nsresult SendGetAccessPoints(const char* aPath);
  nsresult SendGetAPProperties(const char* aPath);
  nsresult IdentifyDevices(DBusMessage* aMsg);
  nsresult IdentifyDeviceType(DBusMessage* aMsg, const char* aDevicePath);
  nsresult IdentifyAccessPoints(DBusMessage* aMsg);
  nsresult IdentifyAPProperties(DBusMessage* aMsg);
  nsresult StoreSsid(DBusMessageIter* aVariant, nsWifiAccessPoint* aAp);
  nsresult SetMac(DBusMessageIter* aVariant, nsWifiAccessPoint* aAp);
  nsresult GetDBusIterator(DBusMessage* aMsg, DBusMessageIter* aIterArray);

  RefPtr<DBusConnection> mConnection;
  nsCOMArray<nsWifiAccessPoint>* mAccessPoints;
};

}  // namespace mozilla

#endif  // NSWIFIAPSCANNERDBUS_H_
