/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "windows.h"
#include "wlanapi.h"

#include "stdlib.h"

#include "nsWifiMonitor.h"
#include "nsWifiAccessPoint.h"

#include "nsServiceManagerUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsIMutableArray.h"

#define DOT11_BSS_TYPE_UNUSED static_cast<DOT11_BSS_TYPE>(0)

using namespace mozilla;

nsresult
nsWifiMonitor::DoScan()
{
    HINSTANCE wlan_library = LoadLibrary("Wlanapi.dll");
    if (!wlan_library)
      return NS_ERROR_NOT_AVAILABLE;

    decltype(::WlanOpenHandle)* WlanOpenHandle = (decltype(::WlanOpenHandle)*) GetProcAddress(wlan_library, "WlanOpenHandle");
    decltype(::WlanEnumInterfaces)* WlanEnumInterfaces = (decltype(::WlanEnumInterfaces)*) GetProcAddress(wlan_library, "WlanEnumInterfaces");
    decltype(::WlanGetNetworkBssList)* WlanGetNetworkBssList = (decltype(::WlanGetNetworkBssList)*) GetProcAddress(wlan_library, "WlanGetNetworkBssList");
    decltype(::WlanFreeMemory)* WlanFreeMemory = (decltype(::WlanFreeMemory)*) GetProcAddress(wlan_library, "WlanFreeMemory");
    decltype(::WlanCloseHandle)* WlanCloseHandle = (decltype(::WlanCloseHandle)*) GetProcAddress(wlan_library, "WlanCloseHandle");

    if (!WlanOpenHandle ||
        !WlanEnumInterfaces ||
        !WlanGetNetworkBssList ||
        !WlanFreeMemory ||
        !WlanCloseHandle)
      return NS_ERROR_FAILURE;

    // Regularly get the access point data.

    nsCOMArray<nsWifiAccessPoint> lastAccessPoints;
    nsCOMArray<nsWifiAccessPoint> accessPoints;

    do {
      accessPoints.Clear();

      // Get the handle to the WLAN API.
      DWORD negotiated_version;
      HANDLE wlan_handle = nullptr;
      // We could be executing on either Windows XP or Windows Vista, so use the
      // lower version of the client WLAN API. It seems that the negotiated version
      // is the Vista version irrespective of what we pass!
      static const int kXpWlanClientVersion = 1;
      if ((*WlanOpenHandle)(kXpWlanClientVersion,
                            nullptr,
                            &negotiated_version,
                            &wlan_handle) != ERROR_SUCCESS) {
        return NS_ERROR_NOT_AVAILABLE;
      }

      // try again later.
      if (!wlan_handle)
        return NS_ERROR_FAILURE;

      // Get the list of interfaces. WlanEnumInterfaces allocates interface_list.
      WLAN_INTERFACE_INFO_LIST *interface_list = nullptr;
      if ((*WlanEnumInterfaces)(wlan_handle, nullptr, &interface_list) != ERROR_SUCCESS) {
        // try again later
        (*WlanCloseHandle)(wlan_handle, nullptr);
        return NS_ERROR_FAILURE;
      }

      // Go through the list of interfaces and get the data for each.
      for (int i = 0; i < static_cast<int>(interface_list->dwNumberOfItems); ++i) {

        WLAN_BSS_LIST *bss_list;
        HRESULT rv = (*WlanGetNetworkBssList)(wlan_handle,
                                              &interface_list->InterfaceInfo[i].InterfaceGuid,
                                              nullptr,  // Use all SSIDs.
                                              DOT11_BSS_TYPE_UNUSED,
                                              false,    // bSecurityEnabled - unused
                                              nullptr,  // reserved
                                              &bss_list);
        if (rv != ERROR_SUCCESS) {
          continue;
        }

        for (int j = 0; j < static_cast<int>(bss_list->dwNumberOfItems); ++j) {

          nsWifiAccessPoint* ap = new nsWifiAccessPoint();
          if (!ap)
            continue;

          const WLAN_BSS_ENTRY bss_entry = bss_list->wlanBssEntries[j];

          ap->setMac(bss_entry.dot11Bssid);
          ap->setSignal(bss_entry.lRssi);
          ap->setSSID((char*) bss_entry.dot11Ssid.ucSSID,
                      bss_entry.dot11Ssid.uSSIDLength);

          accessPoints.AppendObject(ap);
        }
        (*WlanFreeMemory)(bss_list);
      }

      // Free interface_list.
      (*WlanFreeMemory)(interface_list);

      // Close the handle.
      (*WlanCloseHandle)(wlan_handle, nullptr);


      bool accessPointsChanged = !AccessPointsEqual(accessPoints, lastAccessPoints);
      ReplaceArray(lastAccessPoints, accessPoints);

      nsresult rv = CallWifiListeners(lastAccessPoints, accessPointsChanged);
      NS_ENSURE_SUCCESS(rv, rv);

      // wait for some reasonable amount of time.  pref?
      LOG(("waiting on monitor\n"));

      ReentrantMonitorAutoEnter mon(mReentrantMonitor);
      mon.Wait(PR_SecondsToInterval(kDefaultWifiScanInterval));
    }
    while (mKeepGoing);

    return NS_OK;
}
