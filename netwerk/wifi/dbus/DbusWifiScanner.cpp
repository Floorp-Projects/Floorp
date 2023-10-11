/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <gio/gio.h>
#include "DbusWifiScanner.h"
#include "nsWifiAccessPoint.h"
#include "mozilla/GUniquePtr.h"
#include "mozilla/RefPtr.h"
#include "mozilla/GRefPtr.h"

namespace mozilla {

WifiScannerImpl::WifiScannerImpl() { MOZ_COUNT_CTOR(WifiScannerImpl); }

WifiScannerImpl::~WifiScannerImpl() { MOZ_COUNT_DTOR(WifiScannerImpl); }

nsresult WifiScannerImpl::GetAccessPointsFromWLAN(
    AccessPointArray& aAccessPoints) {
  RefPtr<GDBusProxy> proxy = dont_AddRef(g_dbus_proxy_new_for_bus_sync(
      G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE, nullptr,
      "org.freedesktop.NetworkManager", "/org/freedesktop/NetworkManager",
      "org.freedesktop.NetworkManager", nullptr, nullptr));
  if (!proxy) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<GVariant> result =
      dont_AddRef(g_dbus_proxy_get_cached_property(proxy, "Devices"));
  if (!result ||
      !g_variant_is_of_type(result, G_VARIANT_TYPE_OBJECT_PATH_ARRAY)) {
    return NS_ERROR_FAILURE;
  }

  gsize num = g_variant_n_children(result);
  for (gsize i = 0; i < num; i++) {
    const char* devicePath =
        g_variant_get_string(g_variant_get_child_value(result, i), nullptr);
    if (!devicePath || !AddDevice(devicePath, aAccessPoints)) {
      return NS_ERROR_FAILURE;
    }
  }

  return NS_OK;
}

bool WifiScannerImpl::AddDevice(const char* aDevicePath,
                                AccessPointArray& aAccessPoints) {
  RefPtr<GDBusProxy> proxy = dont_AddRef(g_dbus_proxy_new_for_bus_sync(
      G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE, nullptr,
      "org.freedesktop.NetworkManager", aDevicePath,
      "org.freedesktop.NetworkManager.Device", nullptr, nullptr));
  if (!proxy) {
    return false;
  }

  RefPtr<GVariant> deviceType =
      dont_AddRef(g_dbus_proxy_get_cached_property(proxy, "DeviceType"));
  if (!deviceType || !g_variant_is_of_type(deviceType, G_VARIANT_TYPE_UINT32)) {
    return false;
  }

  // http://projects.gnome.org/NetworkManager/developers/api/07/spec-07.html
  // Refer to NM_DEVICE_TYPE_WIFI under NM_DEVICE_TYPE.
  const uint32_t NM_DEVICE_TYPE_WIFI = 2;
  if (g_variant_get_uint32(deviceType) != NM_DEVICE_TYPE_WIFI) {
    // Don't probe non-wifi devices
    return true;
  }

  proxy = dont_AddRef(g_dbus_proxy_new_for_bus_sync(
      G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE, nullptr,
      "org.freedesktop.NetworkManager", aDevicePath,
      "org.freedesktop.NetworkManager.Device.Wireless", nullptr, nullptr));
  if (!proxy) {
    return false;
  }

  RefPtr<GVariant> points =
      dont_AddRef(g_dbus_proxy_get_cached_property(proxy, "AccessPoints"));
  if (!points ||
      !g_variant_is_of_type(points, G_VARIANT_TYPE_OBJECT_PATH_ARRAY)) {
    return false;
  }

  gsize num = g_variant_n_children(points);
  for (gsize i = 0; i < num; i++) {
    const char* ap =
        g_variant_get_string(g_variant_get_child_value(points, i), nullptr);
    if (!ap || !AddAPProperties(ap, aAccessPoints)) {
      return false;
    }
  }
  return true;
}

bool WifiScannerImpl::AddAPProperties(const char* aApPath,
                                      AccessPointArray& aAccessPoints) {
  RefPtr<GDBusProxy> proxy = dont_AddRef(g_dbus_proxy_new_for_bus_sync(
      G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE, nullptr,
      "org.freedesktop.NetworkManager", aApPath,
      "org.freedesktop.NetworkManager.AccessPoint", nullptr, nullptr));
  if (!proxy) {
    return false;
  }

  RefPtr<nsWifiAccessPoint> ap = new nsWifiAccessPoint();

  RefPtr<GVariant> ssid =
      dont_AddRef(g_dbus_proxy_get_cached_property(proxy, "Ssid"));
  if (!ssid || !g_variant_is_of_type(ssid, G_VARIANT_TYPE_BYTESTRING)) {
    return false;
  }
  const uint32_t MAX_SSID_LEN = 32;
  gsize len = 0;
  const char* ssidString = static_cast<const char*>(
      g_variant_get_fixed_array(ssid, &len, sizeof(guint8)));
  if (ssidString && len && len <= MAX_SSID_LEN) {
    ap->setSSID(ssidString, len);
  }

  RefPtr<GVariant> hwAddress =
      dont_AddRef(g_dbus_proxy_get_cached_property(proxy, "HwAddress"));
  if (!hwAddress || !g_variant_is_of_type(hwAddress, G_VARIANT_TYPE_STRING)) {
    return false;
  }
  GUniquePtr<gchar> address(g_variant_dup_string(hwAddress, nullptr));
  SetMac(address.get(), ap);

  RefPtr<GVariant> st =
      dont_AddRef(g_dbus_proxy_get_cached_property(proxy, "Strength"));
  if (!st || !g_variant_is_of_type(st, G_VARIANT_TYPE_BYTE)) {
    return false;
  }

  uint8_t strength = g_variant_get_byte(st);   // in %
  int signal_strength = (strength / 2) - 100;  // strength to dB
  ap->setSignal(signal_strength);

  aAccessPoints.AppendElement(ap);
  return true;
}

bool WifiScannerImpl::SetMac(char* aHwAddress, nsWifiAccessPoint* aAp) {
  // hwAddress format is XX:XX:XX:XX:XX:XX. Need to convert to XXXXXX format.
  const uint32_t MAC_LEN = 6;
  uint8_t macAddress[MAC_LEN];
  char* savedPtr = nullptr;
  char* token = strtok_r(aHwAddress, ":", &savedPtr);
  for (unsigned char& macAddres : macAddress) {
    if (!token) {
      return false;
    }
    macAddres = strtoul(token, nullptr, 16);
    token = strtok_r(nullptr, ":", &savedPtr);
  }
  aAp->setMac(macAddress);
  return true;
}

}  // namespace mozilla
