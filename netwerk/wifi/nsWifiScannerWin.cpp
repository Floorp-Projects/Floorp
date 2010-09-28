/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Geolocation.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * This is a derivative of work done by Google under a BSD style License.
 * See: http://gears.googlecode.com/svn/trunk/gears/geolocation/
 *
 * Contributor(s):
 *  Doug Turner <dougt@meer.net>  (Original Author)
 *  Nino D'Aversa <ninodaversa@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#include "windows.h"
#include "wlanapi.h"

#include <ntddndis.h>
#include "winioctl.h"
#include "stdlib.h"

#include "nsAutoPtr.h"
#include "nsWifiMonitor.h"
#include "nsWifiAccessPoint.h"

#include "nsIProxyObjectManager.h"
#include "nsServiceManagerUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsIMutableArray.h"

nsresult
nsWifiMonitor::DoScan()
{
    HINSTANCE wlan_library = LoadLibrary("Wlanapi.dll");
    if (!wlan_library)
      return NS_ERROR_NOT_AVAILABLE;

    WlanOpenHandleFunction WlanOpenHandle = (WlanOpenHandleFunction) GetProcAddress(wlan_library, "WlanOpenHandle");
    WlanEnumInterfacesFunction WlanEnumInterfaces = (WlanEnumInterfacesFunction) GetProcAddress(wlan_library, "WlanEnumInterfaces");
    WlanGetNetworkBssListFunction WlanGetNetworkBssList = (WlanGetNetworkBssListFunction) GetProcAddress(wlan_library, "WlanGetNetworkBssList");
    WlanFreeMemoryFunction WlanFreeMemory = (WlanFreeMemoryFunction) GetProcAddress(wlan_library, "WlanFreeMemory");
    WlanCloseHandleFunction WlanCloseHandle = (WlanCloseHandleFunction) GetProcAddress(wlan_library, "WlanCloseHandle");

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
      HANDLE wlan_handle = NULL;
      // We could be executing on either Windows XP or Windows Vista, so use the
      // lower version of the client WLAN API. It seems that the negotiated version
      // is the Vista version irrespective of what we pass!
      static const int kXpWlanClientVersion = 1;
      if ((*WlanOpenHandle)(kXpWlanClientVersion,
                            NULL,
                            &negotiated_version,
                            &wlan_handle) != ERROR_SUCCESS) {
        return NS_ERROR_NOT_AVAILABLE;
      }

      // try again later.
      if (!wlan_handle)
        return NS_ERROR_FAILURE;

      // Get the list of interfaces. WlanEnumInterfaces allocates interface_list.
      WLAN_INTERFACE_INFO_LIST *interface_list = NULL;
      if ((*WlanEnumInterfaces)(wlan_handle, NULL, &interface_list) != ERROR_SUCCESS) {
        // try again later
        (*WlanCloseHandle)(wlan_handle, NULL);
        return NS_ERROR_FAILURE;
      }

      // Go through the list of interfaces and get the data for each.
      for (int i = 0; i < static_cast<int>(interface_list->dwNumberOfItems); ++i) {

        WLAN_BSS_LIST *bss_list;
        HRESULT rv = (*WlanGetNetworkBssList)(wlan_handle,
                                              &interface_list->InterfaceInfo[i].InterfaceGuid,
                                              NULL,   // Use all SSIDs.
                                              DOT11_BSS_TYPE_UNUSED,
                                              false,  // bSecurityEnabled - unused
                                              NULL,   // reserved
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
      (*WlanCloseHandle)(wlan_handle, NULL);


      PRBool accessPointsChanged = !AccessPointsEqual(accessPoints, lastAccessPoints);
      nsCOMArray<nsIWifiListener> currentListeners;

      {
        nsAutoMonitor mon(mMonitor);

        for (PRUint32 i = 0; i < mListeners.Length(); i++) {
          if (!mListeners[i].mHasSentData || accessPointsChanged) {
            mListeners[i].mHasSentData = PR_TRUE;
            currentListeners.AppendObject(mListeners[i].mListener);
          }
        }
      }

      ReplaceArray(lastAccessPoints, accessPoints);

      if (currentListeners.Count() > 0) {
        PRUint32 resultCount = lastAccessPoints.Count();
        nsIWifiAccessPoint** result = static_cast<nsIWifiAccessPoint**> (nsMemory::Alloc(sizeof(nsIWifiAccessPoint*) * resultCount));
        if (!result)
          return NS_ERROR_OUT_OF_MEMORY;

        for (PRUint32 i = 0; i < resultCount; i++)
          result[i] = lastAccessPoints[i];

        for (PRInt32 i = 0; i < currentListeners.Count(); i++) {

          LOG(("About to send data to the wifi listeners\n"));

          nsCOMPtr<nsIWifiListener> proxy;
          nsCOMPtr<nsIProxyObjectManager> proxyObjMgr = do_GetService("@mozilla.org/xpcomproxy;1");
          proxyObjMgr->GetProxyForObject(NS_PROXY_TO_MAIN_THREAD,
                                         NS_GET_IID(nsIWifiListener),
                                         currentListeners[i],
                                         NS_PROXY_SYNC | NS_PROXY_ALWAYS,
                                         getter_AddRefs(proxy));
          if (!proxy) {
            LOG(("There is no proxy available.  this should never happen\n"));
          }
          else
          {
            nsresult rv = proxy->OnChange(result, resultCount);
            LOG( ("... sent %d\n", rv));
          }
        }

        nsMemory::Free(result);
      }

      // wait for some reasonable amount of time.  pref?
      LOG(("waiting on monitor\n"));

      nsAutoMonitor mon(mMonitor);
      mon.Wait(PR_SecondsToInterval(60));
    }
    while (mKeepGoing);
  }
  return NS_OK;
}
