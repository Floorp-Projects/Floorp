/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWifiScannerDBus.h"
#include "mozilla/DBusHelpers.h"
#include "nsWifiAccessPoint.h"

namespace mozilla {

nsWifiScannerDBus::nsWifiScannerDBus(
    nsCOMArray<nsWifiAccessPoint>* aAccessPoints)
    : mAccessPoints(aAccessPoints) {
  MOZ_ASSERT(mAccessPoints);

  mConnection =
      already_AddRefed<DBusConnection>(dbus_bus_get(DBUS_BUS_SYSTEM, nullptr));

  if (mConnection) {
    dbus_connection_setup_with_g_main(mConnection, nullptr);
    dbus_connection_set_exit_on_disconnect(mConnection, false);
  }

  MOZ_COUNT_CTOR(nsWifiScannerDBus);
}

nsWifiScannerDBus::~nsWifiScannerDBus() { MOZ_COUNT_DTOR(nsWifiScannerDBus); }

nsresult nsWifiScannerDBus::Scan() {
  if (!mConnection) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  return SendGetDevices();
}

// http://dbus.freedesktop.org/doc/api/html/group__DBusConnection.html
// Refer to function dbus_connection_send_with_reply_and_block.
static const uint32_t DBUS_DEFAULT_TIMEOUT = -1;

nsresult nsWifiScannerDBus::SendGetDevices() {
  RefPtr<DBusMessage> msg =
      already_AddRefed<DBusMessage>(dbus_message_new_method_call(
          "org.freedesktop.NetworkManager", "/org/freedesktop/NetworkManager",
          "org.freedesktop.NetworkManager", "GetDevices"));
  if (!msg) {
    return NS_ERROR_FAILURE;
  }

  DBusError err;
  dbus_error_init(&err);

  RefPtr<DBusMessage> reply =
      already_AddRefed<DBusMessage>(dbus_connection_send_with_reply_and_block(
          mConnection, msg, DBUS_DEFAULT_TIMEOUT, &err));
  if (dbus_error_is_set(&err)) {
    dbus_error_free(&err);
    return NS_ERROR_FAILURE;
  }

  return IdentifyDevices(reply);
}

nsresult nsWifiScannerDBus::SendGetDeviceType(const char* aPath) {
  RefPtr<DBusMessage> msg = already_AddRefed<DBusMessage>(
      dbus_message_new_method_call("org.freedesktop.NetworkManager", aPath,
                                   "org.freedesktop.DBus.Properties", "Get"));
  if (!msg) {
    return NS_ERROR_FAILURE;
  }

  DBusMessageIter argsIter;
  dbus_message_iter_init_append(msg, &argsIter);

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

  DBusError err;
  dbus_error_init(&err);

  RefPtr<DBusMessage> reply =
      already_AddRefed<DBusMessage>(dbus_connection_send_with_reply_and_block(
          mConnection, msg, DBUS_DEFAULT_TIMEOUT, &err));
  if (dbus_error_is_set(&err)) {
    dbus_error_free(&err);
    return NS_ERROR_FAILURE;
  }

  return IdentifyDeviceType(reply, aPath);
}

nsresult nsWifiScannerDBus::SendGetAccessPoints(const char* aPath) {
  RefPtr<DBusMessage> msg =
      already_AddRefed<DBusMessage>(dbus_message_new_method_call(
          "org.freedesktop.NetworkManager", aPath,
          "org.freedesktop.NetworkManager.Device.Wireless", "GetAccessPoints"));
  if (!msg) {
    return NS_ERROR_FAILURE;
  }

  DBusError err;
  dbus_error_init(&err);

  RefPtr<DBusMessage> reply =
      already_AddRefed<DBusMessage>(dbus_connection_send_with_reply_and_block(
          mConnection, msg, DBUS_DEFAULT_TIMEOUT, &err));
  if (dbus_error_is_set(&err)) {
    dbus_error_free(&err);
    // In the GetAccessPoints case, if there are no access points, error is set.
    // We don't want to error out here.
    return NS_OK;
  }

  return IdentifyAccessPoints(reply);
}

nsresult nsWifiScannerDBus::SendGetAPProperties(const char* aPath) {
  RefPtr<DBusMessage> msg =
      already_AddRefed<DBusMessage>(dbus_message_new_method_call(
          "org.freedesktop.NetworkManager", aPath,
          "org.freedesktop.DBus.Properties", "GetAll"));
  if (!msg) {
    return NS_ERROR_FAILURE;
  }

  DBusMessageIter argsIter;
  dbus_message_iter_init_append(msg, &argsIter);

  const char* param = "org.freedesktop.NetworkManager.AccessPoint";
  if (!dbus_message_iter_append_basic(&argsIter, DBUS_TYPE_STRING, &param)) {
    return NS_ERROR_FAILURE;
  }

  DBusError err;
  dbus_error_init(&err);

  RefPtr<DBusMessage> reply =
      already_AddRefed<DBusMessage>(dbus_connection_send_with_reply_and_block(
          mConnection, msg, DBUS_DEFAULT_TIMEOUT, &err));
  if (dbus_error_is_set(&err)) {
    dbus_error_free(&err);
    return NS_ERROR_FAILURE;
  }

  return IdentifyAPProperties(reply);
}

nsresult nsWifiScannerDBus::IdentifyDevices(DBusMessage* aMsg) {
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

    rv = SendGetDeviceType(devicePath);
    NS_ENSURE_SUCCESS(rv, rv);
  } while (dbus_message_iter_next(&iter));

  return NS_OK;
}

nsresult nsWifiScannerDBus::IdentifyDeviceType(DBusMessage* aMsg,
                                               const char* aDevicePath) {
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
    rv = SendGetAccessPoints(aDevicePath);
  }

  return rv;
}

nsresult nsWifiScannerDBus::IdentifyAccessPoints(DBusMessage* aMsg) {
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

    rv = SendGetAPProperties(path);
    NS_ENSURE_SUCCESS(rv, rv);
  } while (dbus_message_iter_next(&iter));

  return NS_OK;
}

nsresult nsWifiScannerDBus::IdentifyAPProperties(DBusMessage* aMsg) {
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

        uint8_t strength;  // in %
        dbus_message_iter_get_basic(&variant, &strength);
        int signal_strength = (strength / 2) - 100;  // strength to dB
        ap->setSignal(signal_strength);
      }
    } while (dbus_message_iter_next(&dict));
  } while (dbus_message_iter_next(&arr));

  mAccessPoints->AppendObject(ap);
  return NS_OK;
}

nsresult nsWifiScannerDBus::StoreSsid(DBusMessageIter* aVariant,
                                      nsWifiAccessPoint* aAp) {
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

nsresult nsWifiScannerDBus::SetMac(DBusMessageIter* aVariant,
                                   nsWifiAccessPoint* aAp) {
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
  char* savedPtr = nullptr;
  char* token = strtok_r(hwAddress, ":", &savedPtr);
  for (unsigned char& macAddres : macAddress) {
    if (!token) {
      return NS_ERROR_FAILURE;
    }
    macAddres = strtoul(token, nullptr, 16);
    token = strtok_r(nullptr, ":", &savedPtr);
  }
  aAp->setMac(macAddress);
  return NS_OK;
}

nsresult nsWifiScannerDBus::GetDBusIterator(DBusMessage* aMsg,
                                            DBusMessageIter* aIterArray) {
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

}  // namespace mozilla

nsresult nsWifiMonitor::DoScan() {
  nsCOMArray<nsWifiAccessPoint> accessPoints;
  mozilla::nsWifiScannerDBus wifiScanner(&accessPoints);
  nsCOMArray<nsWifiAccessPoint> lastAccessPoints;

  while (mKeepGoing) {
    accessPoints.Clear();
    nsresult rv = wifiScanner.Scan();
    NS_ENSURE_SUCCESS(rv, rv);
    bool accessPointsChanged =
        !AccessPointsEqual(accessPoints, lastAccessPoints);
    ReplaceArray(lastAccessPoints, accessPoints);

    rv = CallWifiListeners(lastAccessPoints, accessPointsChanged);
    NS_ENSURE_SUCCESS(rv, rv);

    LOG(("waiting on monitor\n"));
    mozilla::ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    mon.Wait(PR_SecondsToInterval(kDefaultWifiScanInterval));
  }

  return NS_OK;
}
