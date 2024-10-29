/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWifiAccessPoint.h"
#include "WinWifiScanner.h"

#define DOT11_BSS_TYPE_UNUSED static_cast<DOT11_BSS_TYPE>(0)

namespace mozilla {

LazyLogModule gWifiScannerLog("WifiScanner");
#define WIFISCAN_LOG(args) \
  MOZ_LOG(mozilla::gWifiScannerLog, mozilla::LogLevel::Debug, args)

class InterfaceScanCallbackData {
 public:
  explicit InterfaceScanCallbackData(uint32_t numInterfaces)
      : mCurrentlyScanningInterfaces(numInterfaces) {
    mAllInterfacesDoneScanningEvent =
        ::CreateEventW(nullptr,   // null security
                       TRUE,      // manual reset event
                       FALSE,     // initially nonsignaled
                       nullptr);  // not named
    MOZ_ASSERT(NULL != mAllInterfacesDoneScanningEvent);
  }

  ~InterfaceScanCallbackData() {
    ::CloseHandle(mAllInterfacesDoneScanningEvent);
  }

  void OnInterfaceScanComplete() {
    uint32_t val = ::InterlockedDecrement(&mCurrentlyScanningInterfaces);
    if (!val) {
      ::SetEvent(mAllInterfacesDoneScanningEvent);
    }
  }

  void WaitForAllInterfacesToFinishScanning(uint32_t msToWait) {
    ::WaitForSingleObject(mAllInterfacesDoneScanningEvent, msToWait);
  }

 private:
  volatile uint32_t mCurrentlyScanningInterfaces;
  HANDLE mAllInterfacesDoneScanningEvent;
};

static void WINAPI OnScanComplete(PWLAN_NOTIFICATION_DATA data, PVOID context) {
  WIFISCAN_LOG(("WinWifiScanner OnScanComplete"));
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

WifiScannerImpl::WifiScannerImpl() {
  // NOTE: We assume that, if we were unable to load the WLAN library when
  // we initially tried, we will not be able to load it in the future.
  // Technically, on Windows XP SP2, a user could install the redistributable
  // and make our assumption incorrect. We opt to avoid making a bunch of
  // spurious LoadLibrary calls in the common case rather than load the
  // WLAN API in the edge case.
  mWlanLibrary.reset(WinWLANLibrary::Load());
  if (!mWlanLibrary) {
    WIFISCAN_LOG(
        ("[%p] WinWifiScanner could not initialize Windows Wi-Fi scanner",
         this));
  }
  WIFISCAN_LOG(("[%p] WinWifiScanner created WifiScannerImpl", this));
}

WifiScannerImpl::~WifiScannerImpl() {
  WIFISCAN_LOG(("[%p] WinWifiScanner destroyed WifiScannerImpl", this));
}

nsresult WifiScannerImpl::GetAccessPointsFromWLAN(
    nsTArray<RefPtr<nsIWifiAccessPoint>>& accessPoints) {
  WIFISCAN_LOG(("[%p] WinWifiScanner::GetAccessPointsFromWLAN", this));
  accessPoints.Clear();

  // NOTE: We do not try to load the WLAN library if we previously failed
  // to load it. See the note in WifiScannerImpl constructor
  if (!mWlanLibrary) {
    WIFISCAN_LOG(
        ("[%p] WinWifiScanner::GetAccessPointsFromWLAN WLAN library is not "
         "available",
         this));
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Get the list of interfaces. WlanEnumInterfaces allocates interface_list.
  WLAN_INTERFACE_INFO_LIST* interface_list = nullptr;
  if (ERROR_SUCCESS !=
      (*mWlanLibrary->GetWlanEnumInterfacesPtr())(mWlanLibrary->GetWLANHandle(),
                                                  nullptr, &interface_list)) {
    WIFISCAN_LOG(
        ("[%p] WinWifiScanner::GetAccessPointsFromWLAN WlanEnumInterfaces "
         "failed",
         this));
    return NS_ERROR_FAILURE;
  }

  // This ensures we call WlanFreeMemory on interface_list
  ScopedWLANObject scopedInterfaceList(*mWlanLibrary, interface_list);
  WIFISCAN_LOG(
      ("[%p] WinWifiScanner::GetAccessPointsFromWLAN found %lu interfaces",
       this, interface_list->dwNumberOfItems));

  if (!interface_list->dwNumberOfItems) {
    return NS_OK;
  }

  InterfaceScanCallbackData cbData(interface_list->dwNumberOfItems);

  DWORD wlanNotifySource;
  if (ERROR_SUCCESS != (*mWlanLibrary->GetWlanRegisterNotificationPtr())(
                           mWlanLibrary->GetWLANHandle(),
                           WLAN_NOTIFICATION_SOURCE_ACM, TRUE,
                           (WLAN_NOTIFICATION_CALLBACK)OnScanComplete, &cbData,
                           NULL, &wlanNotifySource)) {
    WIFISCAN_LOG(
        ("[%p] WinWifiScanner::GetAccessPointsFromWLAN "
         "WlanRegisterNotification failed",
         this));
    return NS_ERROR_FAILURE;
  }

  // Go through the list of interfaces and call `WlanScan` on each
  for (unsigned int i = 0; i < interface_list->dwNumberOfItems; ++i) {
    if (ERROR_SUCCESS != (*mWlanLibrary->GetWlanScanPtr())(
                             mWlanLibrary->GetWLANHandle(),
                             &interface_list->InterfaceInfo[i].InterfaceGuid,
                             NULL, NULL, NULL)) {
      WIFISCAN_LOG(
          ("[%p] WinWifiScanner::GetAccessPointsFromWLAN WlanScan failed.",
           this));
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
      mWlanLibrary->GetWLANHandle(), WLAN_NOTIFICATION_SOURCE_NONE, TRUE, NULL,
      NULL, NULL, &wlanNotifySource);

  // Go through the list of interfaces and get the data for each.
  for (uint32_t i = 0; i < interface_list->dwNumberOfItems; ++i) {
    WLAN_BSS_LIST* bss_list;
    if (ERROR_SUCCESS != (*mWlanLibrary->GetWlanGetNetworkBssListPtr())(
                             mWlanLibrary->GetWLANHandle(),
                             &interface_list->InterfaceInfo[i].InterfaceGuid,
                             nullptr,  // Use all SSIDs.
                             DOT11_BSS_TYPE_UNUSED,
                             false,    // bSecurityEnabled -
                                       // unused
                             nullptr,  // reserved
                             &bss_list)) {
      WIFISCAN_LOG(
          ("[%p] WinWifiScanner::GetAccessPointsFromWLAN unable to get BSS "
           "list from interface %u",
           this, i));
      continue;
    }

    WIFISCAN_LOG(
        ("[%p] WinWifiScanner::GetAccessPointsFromWLAN BSS list has %lu access "
         "points",
         this, bss_list->dwNumberOfItems));

    // This ensures we call WlanFreeMemory on bss_list
    ScopedWLANObject scopedBssList(*mWlanLibrary, bss_list);

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

      accessPoints.AppendElement(ap);
    }
  }

  return NS_OK;
}

}  // namespace mozilla

#undef WIFISCAN_LOG
