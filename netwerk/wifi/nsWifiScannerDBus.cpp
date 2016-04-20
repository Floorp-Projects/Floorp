/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWifiScannerDBus.h"
#include "mozilla/ipc/DBusMessageRefPtr.h"
#include "nsWifiAccessPoint.h"

namespace mozilla {

nsWifiScannerDBus::nsWifiScannerDBus(nsCOMArray<nsWifiAccessPoint> *aAccessPoints)
: mAccessPoints(aAccessPoints)
{
  MOZ_ASSERT(mAccessPoints);

  mConnection =
    already_AddRefed<DBusConnection>(dbus_bus_get(DBUS_BUS_SYSTEM, nullptr));
  MOZ_ASSERT(mConnection);
  dbus_connection_set_exit_on_disconnect(mConnection, false);

  MOZ_COUNT_CTOR(nsWifiScannerDBus);
}

nsWifiScannerDBus::~nsWifiScannerDBus()
{
  MOZ_COUNT_DTOR(nsWifiScannerDBus);
}

nsresult
nsWifiScannerDBus::Scan()
{
  return SendMessage("org.freedesktop.NetworkManager",
                     "/org/freedesktop/NetworkManager",
                     "GetDevices");
}

nsresult
nsWifiScannerDBus::SendMessage(const char* aInterface,
                               const char* aPath,
                               const char* aFuncCall)
{
  RefPtr<DBusMessage> msg = already_AddRefed<DBusMessage>(
    dbus_message_new_method_call("org.freedesktop.NetworkManager",
                                 aPath, aInterface, aFuncCall));
  if (!msg) {
    return NS_ERROR_FAILURE;
  }

  DBusMessageIter argsIter;
  dbus_message_iter_init_append(msg, &argsIter);

  if (!strcmp(aFuncCall, "Get")) {
    const char* paramInterface = "org.freedesktop.NetworkManager.Device";
    if (!dbus_message_iter_append_basic(&argsIter, DBUS_TYPE_STRING,
                                        &paramInterface)) {
      return NS_ERROR_FAILURE;
    }

    const char* paramDeviceType = "DeviceType";
    if (!dbus_message_iter_append_basic(&argsIter, DBUS_TYPE_STRING,
                                        &paramDeviceType)) {
      return NS_ERROR_FAILURE;
    }
  } else if (!strcmp(aFuncCall, "GetAll")) {
    const char* param = "";
    if (!dbus_message_iter_append_basic(&argsIter, DBUS_TYPE_STRING, &param)) {
      return NS_ERROR_FAILURE;
    }
  }

  DBusError err;
  dbus_error_init(&err);

  // http://dbus.freedesktop.org/doc/api/html/group__DBusConnection.html
  // Refer to function dbus_connection_send_with_reply_and_block.
  const uint32_t DBUS_DEFAULT_TIMEOUT = -1;
  RefPtr<DBusMessage> reply = already_AddRefed<DBusMessage>(
    dbus_connection_send_with_reply_and_block(mConnection, msg,
                                              DBUS_DEFAULT_TIMEOUT, &err));
  if (dbus_error_is_set(&err)) {
    dbus_error_free(&err);

    // In the GetAccessPoints case, if there are no access points, error is set.
    // We don't want to error out here.
    if (!strcmp(aFuncCall, "GetAccessPoints")) {
      return NS_OK;
    }
    return NS_ERROR_FAILURE;
  }

  nsresult rv;
  if (!strcmp(aFuncCall, "GetDevices")) {
    rv = IdentifyDevices(reply);
  } else if (!strcmp(aFuncCall, "Get")) {
    rv = IdentifyDeviceType(reply, aPath);
  } else if (!strcmp(aFuncCall, "GetAccessPoints")) {
    rv = IdentifyAccessPoints(reply);
  } else if (!strcmp(aFuncCall, "GetAll")) {
    rv = IdentifyAPProperties(reply);
  } else {
    rv = NS_ERROR_FAILURE;
  }
  return rv;
}

nsresult
nsWifiScannerDBus::IdentifyDevices(DBusMessage* aMsg)
{
  DBusMessageIter iter;
  nsresult rv = GetDBusIterator(aMsg, &iter);
  NS_ENSURE_SUCCESS(rv, rv);

  const char* devicePath;
  do {
    if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_OBJECT_PATH) {
      return NS_ERROR_FAILURE;
    }

    dbus_message_iter_get_basic(&iter, &devicePath);
    if (!devicePath) {
      return NS_ERROR_FAILURE;
    }

    rv = SendMessage("org.freedesktop.DBus.Properties", devicePath, "Get");
    NS_ENSURE_SUCCESS(rv, rv);
  } while (dbus_message_iter_next(&iter));

  return NS_OK;
}

nsresult
nsWifiScannerDBus::IdentifyDeviceType(DBusMessage* aMsg, const char* aDevicePath)
{
  DBusMessageIter args;
  if (!dbus_message_iter_init(aMsg, &args)) {
    return NS_ERROR_FAILURE;
  }

  if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_VARIANT) {
    return NS_ERROR_FAILURE;
  }

  DBusMessageIter variantIter;
  dbus_message_iter_recurse(&args, &variantIter);
  if (dbus_message_iter_get_arg_type(&variantIter) != DBUS_TYPE_UINT32) {
    return NS_ERROR_FAILURE;
  }

  uint32_t deviceType;
  dbus_message_iter_get_basic(&variantIter, &deviceType);

  // http://projects.gnome.org/NetworkManager/developers/api/07/spec-07.html
  // Refer to NM_DEVICE_TYPE_WIFI under NM_DEVICE_TYPE.
  const uint32_t NM_DEVICE_TYPE_WIFI = 2;
  nsresult rv = NS_OK;
  if (deviceType == NM_DEVICE_TYPE_WIFI) {
    rv = SendMessage("org.freedesktop.NetworkManager.Device.Wireless",
                     aDevicePath, "GetAccessPoints");
  }

  return rv;
}

nsresult
nsWifiScannerDBus::IdentifyAccessPoints(DBusMessage* aMsg)
{
  DBusMessageIter iter;
  nsresult rv = GetDBusIterator(aMsg, &iter);
  NS_ENSURE_SUCCESS(rv, rv);

  const char* path;
  do {
    if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_OBJECT_PATH) {
      return NS_ERROR_FAILURE;
    }
    dbus_message_iter_get_basic(&iter, &path);
    if (!path) {
      return NS_ERROR_FAILURE;
    }

    rv = SendMessage("org.freedesktop.DBus.Properties", path, "GetAll");
    NS_ENSURE_SUCCESS(rv, rv);
  } while (dbus_message_iter_next(&iter));

  return NS_OK;
}

nsresult
nsWifiScannerDBus::IdentifyAPProperties(DBusMessage* aMsg)
{
  DBusMessageIter arr;
  nsresult rv = GetDBusIterator(aMsg, &arr);
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<nsWifiAccessPoint> ap = new nsWifiAccessPoint();
  do {
    DBusMessageIter dict;
    dbus_message_iter_recurse(&arr, &dict);

    do {
      const char* key;
      dbus_message_iter_get_basic(&dict, &key);
      if (!key) {
        return NS_ERROR_FAILURE;
      }
      dbus_message_iter_next(&dict);

      DBusMessageIter variant;
      dbus_message_iter_recurse(&dict, &variant);

      if (!strncmp(key, "Ssid", strlen("Ssid"))) {
        nsresult rv = StoreSsid(&variant, ap);
        NS_ENSURE_SUCCESS(rv, rv);
        break;
      }

      if (!strncmp(key, "HwAddress", strlen("HwAddress"))) {
        nsresult rv = SetMac(&variant, ap);
        NS_ENSURE_SUCCESS(rv, rv);
        break;
      }

      if (!strncmp(key, "Strength", strlen("Strength"))) {
        if (dbus_message_iter_get_arg_type(&variant) != DBUS_TYPE_BYTE) {
          return NS_ERROR_FAILURE;
        }

        uint8_t strength;
        dbus_message_iter_get_basic(&variant, &strength);
        ap->setSignal(strength);
      }
    } while (dbus_message_iter_next(&dict));
  } while (dbus_message_iter_next(&arr));

  mAccessPoints->AppendObject(ap);
  return NS_OK;
}

nsresult
nsWifiScannerDBus::StoreSsid(DBusMessageIter* aVariant, nsWifiAccessPoint* aAp)
{
  if (dbus_message_iter_get_arg_type(aVariant) != DBUS_TYPE_ARRAY) {
    return NS_ERROR_FAILURE;
  }

  DBusMessageIter variantMember;
  dbus_message_iter_recurse(aVariant, &variantMember);

  const uint32_t MAX_SSID_LEN = 32;
  char ssid[MAX_SSID_LEN];
  memset(ssid, '\0', ArrayLength(ssid));
  uint32_t i = 0;
  do {
    if (dbus_message_iter_get_arg_type(&variantMember) != DBUS_TYPE_BYTE) {
      return NS_ERROR_FAILURE;
    }

    dbus_message_iter_get_basic(&variantMember, &ssid[i]);
    i++;
  } while (dbus_message_iter_next(&variantMember) && i < MAX_SSID_LEN);

  aAp->setSSID(ssid, i);
  return NS_OK;
}

nsresult
nsWifiScannerDBus::SetMac(DBusMessageIter* aVariant, nsWifiAccessPoint* aAp)
{
  if (dbus_message_iter_get_arg_type(aVariant) != DBUS_TYPE_STRING) {
    return NS_ERROR_FAILURE;
  }

  // hwAddress format is XX:XX:XX:XX:XX:XX. Need to convert to XXXXXX format.
  char* hwAddress;
  dbus_message_iter_get_basic(aVariant, &hwAddress);
  if (!hwAddress) {
    return NS_ERROR_FAILURE;
  }

  const uint32_t MAC_LEN = 6;
  uint8_t macAddress[MAC_LEN];
  char* token = strtok(hwAddress, ":");
  for (uint32_t i = 0; i < ArrayLength(macAddress); i++) {
    if (!token) {
      return NS_ERROR_FAILURE;
    }
    macAddress[i] = strtoul(token, nullptr, 16);
    token = strtok(nullptr, ":");
  }
  aAp->setMac(macAddress);
  return NS_OK;
}

nsresult
nsWifiScannerDBus::GetDBusIterator(DBusMessage* aMsg,
                                   DBusMessageIter* aIterArray)
{
  DBusMessageIter iter;
  if (!dbus_message_iter_init(aMsg, &iter)) {
    return NS_ERROR_FAILURE;
  }

  if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY) {
    return NS_ERROR_FAILURE;
  }

  dbus_message_iter_recurse(&iter, aIterArray);
  return NS_OK;
}

} // mozilla

nsresult
nsWifiMonitor::DoScan()
{
  nsCOMArray<nsWifiAccessPoint> accessPoints;
  mozilla::nsWifiScannerDBus wifiScanner(&accessPoints);
  nsCOMArray<nsWifiAccessPoint> lastAccessPoints;

  while (mKeepGoing) {
    accessPoints.Clear();
    nsresult rv = wifiScanner.Scan();
    NS_ENSURE_SUCCESS(rv, rv);
    bool accessPointsChanged = !AccessPointsEqual(accessPoints,
                                                  lastAccessPoints);
    ReplaceArray(lastAccessPoints, accessPoints);

    rv = CallWifiListeners(lastAccessPoints, accessPointsChanged);
    NS_ENSURE_SUCCESS(rv, rv);

    LOG(("waiting on monitor\n"));
    mozilla::ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    mon.Wait(PR_SecondsToInterval(kDefaultWifiScanInterval));
  }

  return NS_OK;
}
