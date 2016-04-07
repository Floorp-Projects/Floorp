/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWifiAccessPoint.h"
#include "win_wifiScanner.h"

// Moz headers (alphabetical)
#include "win_wlanLibrary.h"

#define DOT11_BSS_TYPE_UNUSED static_cast<DOT11_BSS_TYPE>(0)

class InterfaceScanCallbackData {
public:
  InterfaceScanCallbackData(uint32_t numInterfaces)
    : mCurrentlyScanningInterfaces(numInterfaces)
  {
    mAllInterfacesDoneScanningEvent =
      ::CreateEvent(nullptr,  // null security
                    TRUE,     // manual reset event
                    FALSE,    // initially nonsignaled
                    nullptr); // not named
    MOZ_ASSERT(NULL != mAllInterfacesDoneScanningEvent);
  }

  ~InterfaceScanCallbackData()
  {
    ::CloseHandle(mAllInterfacesDoneScanningEvent);
  }

  void
  OnInterfaceScanComplete()
  {
    uint32_t val = ::InterlockedDecrement(&mCurrentlyScanningInterfaces);
    if (!val) {
      ::SetEvent(mAllInterfacesDoneScanningEvent);
    }
  }

  void
  WaitForAllInterfacesToFinishScanning(uint32_t msToWait)
  {
    ::WaitForSingleObject(mAllInterfacesDoneScanningEvent,
                          msToWait);
  }

private:
  volatile uint32_t mCurrentlyScanningInterfaces;
  HANDLE mAllInterfacesDoneScanningEvent;
};

static void
OnScanComplete(PWLAN_NOTIFICATION_DATA data, PVOID context)
{
  if (WLAN_NOTIFICATION_SOURCE_ACM != data->NotificationSource) {
    return;
  }

  if (wlan_notification_acm_scan_complete != data->NotificationCode &&
      wlan_notification_acm_scan_fail != data->NotificationCode) {
    return;
  }

  InterfaceScanCallbackData* cbData =
    reinterpret_cast<InterfaceScanCallbackData*>(context);
  cbData->OnInterfaceScanComplete();
}

WinWifiScanner::WinWifiScanner()
{
  // NOTE: We assume that, if we were unable to load the WLAN library when
  // we initially tried, we will not be able to load it in the future.
  // Technically, on Windows XP SP2, a user could install the redistributable
  // and make our assumption incorrect. We opt to avoid making a bunch of
  // spurious LoadLibrary calls in the common case rather than load the
  // WLAN API in the edge case.
  mWlanLibrary = WinWLANLibrary::Load();
  if (!mWlanLibrary) {
    NS_WARNING("Could not initialize Windows Wi-Fi scanner");
  }
}

WinWifiScanner::~WinWifiScanner()
{
}

nsresult
WinWifiScanner::GetAccessPointsFromWLAN(nsCOMArray<nsWifiAccessPoint> &accessPoints)
{
  accessPoints.Clear();

  // NOTE: We do not try to load the WLAN library if we previously failed
  // to load it. See the note in WinWifiScanner constructor
  if (!mWlanLibrary) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Get the list of interfaces. WlanEnumInterfaces allocates interface_list.
  WLAN_INTERFACE_INFO_LIST *interface_list = nullptr;
  if (ERROR_SUCCESS !=
      (*mWlanLibrary->GetWlanEnumInterfacesPtr())(mWlanLibrary->GetWLANHandle(),
                                                  nullptr,
                                                  &interface_list)) {
    return NS_ERROR_FAILURE;
  }

  // This ensures we call WlanFreeMemory on interface_list
  ScopedWLANObject scopedInterfaceList(mWlanLibrary, interface_list);

  if (!interface_list->dwNumberOfItems) {
    return NS_OK;
  }

  InterfaceScanCallbackData cbData(interface_list->dwNumberOfItems);

  DWORD wlanNotifySource;
  if (ERROR_SUCCESS !=
      (*mWlanLibrary->GetWlanRegisterNotificationPtr())(
                                  mWlanLibrary->GetWLANHandle(),
                                  WLAN_NOTIFICATION_SOURCE_ACM,
                                  TRUE,
                                  (WLAN_NOTIFICATION_CALLBACK)OnScanComplete,
                                  &cbData,
                                  NULL,
                                  &wlanNotifySource)) {
    return NS_ERROR_FAILURE;
  }

  // Go through the list of interfaces and call `WlanScan` on each
  for (unsigned int i = 0; i < interface_list->dwNumberOfItems; ++i) {
    if (ERROR_SUCCESS !=
        (*mWlanLibrary->GetWlanScanPtr())(
                    mWlanLibrary->GetWLANHandle(),
                    &interface_list->InterfaceInfo[i].InterfaceGuid,
                    NULL,
                    NULL,
                    NULL)) {
      cbData.OnInterfaceScanComplete();
    }
  }

  // From the MSDN documentation:
  //   "Wireless network drivers that meet Windows logo requirements are
  // required to complete a WlanScan function request in 4 seconds"
  cbData.WaitForAllInterfacesToFinishScanning(5000);

  // Unregister for the notifications. The documentation mentions that,
  // if a callback is currently running, this will wait for the callback
  // to complete.
  (*mWlanLibrary->GetWlanRegisterNotificationPtr())(
                              mWlanLibrary->GetWLANHandle(),
                              WLAN_NOTIFICATION_SOURCE_NONE,
                              TRUE,
                              NULL,
                              NULL,
                              NULL,
                              &wlanNotifySource);

  // Go through the list of interfaces and get the data for each.
  for (uint32_t i = 0; i < interface_list->dwNumberOfItems; ++i) {
    WLAN_BSS_LIST *bss_list;
    if (ERROR_SUCCESS !=
        (*mWlanLibrary->GetWlanGetNetworkBssListPtr())(
                           mWlanLibrary->GetWLANHandle(),
                           &interface_list->InterfaceInfo[i].InterfaceGuid,
                           nullptr,  // Use all SSIDs.
                           DOT11_BSS_TYPE_UNUSED,
                           false,    // bSecurityEnabled - unused
                           nullptr,  // reserved
                           &bss_list)) {
      continue;
    }

    // This ensures we call WlanFreeMemory on bss_list
    ScopedWLANObject scopedBssList(mWlanLibrary, bss_list);

    // Store each discovered access point in our outparam
    for (int j = 0; j < static_cast<int>(bss_list->dwNumberOfItems); ++j) {
      nsWifiAccessPoint* ap = new nsWifiAccessPoint();
      if (!ap) {
        continue;
      }

      const WLAN_BSS_ENTRY bss_entry = bss_list->wlanBssEntries[j];
      ap->setMac(bss_entry.dot11Bssid);
      ap->setSignal(bss_entry.lRssi);
      ap->setSSID(reinterpret_cast<char const*>(bss_entry.dot11Ssid.ucSSID),
                  bss_entry.dot11Ssid.uSSIDLength);

      accessPoints.AppendObject(ap);
    }
  }

  return NS_OK;
}
