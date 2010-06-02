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

#ifdef WINCE
#include <Iphlpapi.h>  // For GetAdaptersInfo()
#include <nuiouser.h>  // For NDISUIO stuff
// GetAdaptersInfo
typedef DWORD (*GETADAPTERSINFO)(PIP_ADAPTER_INFO pAdapterInfo, PULONG pOutBufLen);
#endif


// Taken from ndis.h
#define NDIS_STATUS_INVALID_LENGTH   ((NDIS_STATUS)0xC0010014L)
#define NDIS_STATUS_BUFFER_TOO_SHORT ((NDIS_STATUS)0xC0010016L)

static const int kStringLength = 512;

// The limits on the size of the buffer used for the OID query.
static const int kInitialBufferSize = 2 << 12;  // Good for about 50 APs.
static const int kMaximumBufferSize = 2 << 20;  // 2MB

static int oid_buffer_size_ = kStringLength;

PRBool ResizeBuffer(int requested_size, BYTE **buffer)
{
  if (requested_size > kMaximumBufferSize) {
    free(*buffer);
    *buffer = NULL;
    return false;
  }

  BYTE *new_buffer = (BYTE*)realloc(*buffer, requested_size);
  if (new_buffer == NULL) {
    free(*buffer);
    *buffer = NULL;
    return false;
  }

  *buffer = new_buffer;
  return true;
}


#ifdef WINCE
int PerformQuery(HANDLE &ndis_handle,
                 const TCHAR *device_name,
                 BYTE *buffer,
                 DWORD buffer_size,
                 BYTE *&data,
                 DWORD *bytes_out) {

  NS_ASSERTION(buffer, "Buffer is null.  OOM?");

  if (!buffer)
    return ERROR_INSUFFICIENT_BUFFER;

  // Form the query parameters.
  NDISUIO_QUERY_OID *query = (NDISUIO_QUERY_OID*)(buffer);
  query->ptcDeviceName = const_cast<PTCHAR>(device_name);
  query->Oid = OID_802_11_BSSID_LIST;

  if (!DeviceIoControl(ndis_handle,
                       IOCTL_NDISUIO_QUERY_OID_VALUE,
                       query,
                       sizeof(NDISUIO_QUERY_OID),
                       query,
                       buffer_size,
                       bytes_out,
                       NULL)) {
    return GetLastError();
  }
  // The start of the NDIS_802_11_BSSID_LIST is at Data[0]
  data = &query->Data[0];
  return ERROR_SUCCESS;
}

 	

void GetNetworkInterfaces(GETADAPTERSINFO pGetAdaptersInfo, nsStringArray& interfaces){
  // Get the list of adapters. First determine the buffer size.
  ULONG buffer_size = 0;
  // since buffer_size is zero before this, we should get ERROR_BUFFER_OVERFLOW
  // after this error the value of buffer_size will reflect the size needed
  if (pGetAdaptersInfo(NULL, &buffer_size) != ERROR_BUFFER_OVERFLOW)
    return;

  // Allocate adapter_info with correct size.
  IP_ADAPTER_INFO *adapter_info = (IP_ADAPTER_INFO*)malloc(buffer_size);
  if (adapter_info == NULL){
    free (adapter_info);
    return;
  }

  if (pGetAdaptersInfo(adapter_info, &buffer_size) != ERROR_SUCCESS){
    free (adapter_info);
    return;
  }

  // Walk through the list of adapters.
  while (adapter_info) {
    // AdapterName e.g. TNETW12511
    nsString adapterName;
    // AdapterName is in ASCII
    adapterName.AppendWithConversion(adapter_info->AdapterName);
    interfaces.AppendString(adapterName);
    adapter_info = adapter_info->Next;
  }
  free (adapter_info);
}

nsresult SetupWince(HANDLE& ndis_handle, GETADAPTERSINFO& pGetAdaptersInfo){
  // Get the Network Driver Interface Specification (NDIS) handle for wireless
  // devices. NDIS defines a standard API for Network Interface Cards (NICs).
  // NDIS User Mode I/O (NDISUIO) is an NDIS protocol driver which offers
  // support for wireless devices.

  ndis_handle = CreateFile(NDISUIO_DEVICE_NAME, GENERIC_READ,
			   FILE_SHARE_READ, NULL, OPEN_EXISTING,
			   FILE_ATTRIBUTE_READONLY, NULL);
  if (INVALID_HANDLE_VALUE == ndis_handle)
    return NS_ERROR_FAILURE;

  HINSTANCE hIpDLL = LoadLibraryW(L"Iphlpapi.dll");
  if (!hIpDLL)
    return NS_ERROR_NOT_AVAILABLE;

  pGetAdaptersInfo = (GETADAPTERSINFO) GetProcAddress(hIpDLL, "GetAdaptersInfo");

  if (!pGetAdaptersInfo)
    return NS_ERROR_FAILURE;

  return NS_OK;
}

#else
int PerformQuery(HANDLE adapter_handle,
                 BYTE *buffer,
                 DWORD buffer_size,
                 DWORD *bytes_out) {

  DWORD oid = OID_802_11_BSSID_LIST;

  if (!DeviceIoControl(adapter_handle,
                       IOCTL_NDIS_QUERY_GLOBAL_STATS,
                       &oid,
                       sizeof(oid),
                       buffer,
                       buffer_size,
                       bytes_out,
                       NULL)) {
    return GetLastError();
  }
  return ERROR_SUCCESS;
}

HANDLE GetFileHandle(const PRUnichar* device_name) {
  // We access a device with DOS path \Device\<device_name> at
  // \\.\<device_name>.

  nsString formatted_device_name;
  formatted_device_name.Assign(L"\\\\.\\");
  formatted_device_name.Append(device_name);

  return CreateFileW(formatted_device_name.get(),
                     GENERIC_READ,
                     FILE_SHARE_READ | FILE_SHARE_WRITE, // share mode
                     0,  // security attributes
                     OPEN_EXISTING,
                     0,  // flags and attributes
                     INVALID_HANDLE_VALUE);
}


bool UndefineDosDevice(const PRUnichar* device_name) {
  // We remove only the mapping we use, that is \Device\<device_name>.
  nsString target_path;
  target_path.Assign(L"\\Device\\");
  target_path.Append(device_name);

  return DefineDosDeviceW(DDD_RAW_TARGET_PATH | DDD_REMOVE_DEFINITION | DDD_EXACT_MATCH_ON_REMOVE,
                          device_name,
                          target_path.get()) == TRUE;
}

bool DefineDosDeviceIfNotExists(const PRUnichar* device_name, bool* dosDeviceDefined) {

  // We create a DOS device name for the device at \Device\<device_name>.
  nsString target_path;
  target_path.Assign(L"\\Device\\");
  target_path.Append(device_name);

  WCHAR target[kStringLength];

  if (QueryDosDeviceW(device_name, target, kStringLength) > 0 && target_path.Equals(target)) {
    // Device already exists.
    return true;
  }

  DWORD error = GetLastError();
  if (error != ERROR_FILE_NOT_FOUND) {
    return false;
  }

  if (!DefineDosDeviceW(DDD_RAW_TARGET_PATH,
                        device_name,
                        target_path.get())) {
    return false;
  }
  *dosDeviceDefined = true;
  // Check that the device is really there.
  return QueryDosDeviceW(device_name, target, kStringLength) > 0 &&
    target_path.Equals(target);
}

void GetNetworkInterfaces(nsStringArray& interfaces)
{
  HKEY network_cards_key = NULL;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                    L"Software\\Microsoft\\Windows NT\\CurrentVersion\\NetworkCards",
                    0,
                    KEY_READ,
                    &network_cards_key) != ERROR_SUCCESS)
    {
      return;
    }

  for (int i = 0; ; ++i) {
    WCHAR name[kStringLength];
    DWORD name_size = kStringLength;
    FILETIME time;

    if (RegEnumKeyExW(network_cards_key,
                      i,
                      name,
                      &name_size,
                      NULL,
                      NULL,
                      NULL,
                      &time) != ERROR_SUCCESS)
      {
        break;
      }

    HKEY hardware_key = NULL;
    if (RegOpenKeyExW(network_cards_key,
                      name,
                      0,
                      KEY_READ,
                      &hardware_key) != ERROR_SUCCESS)
      {
        break;
      }

    PRUnichar service_name[kStringLength];
    DWORD service_name_size = kStringLength;
    DWORD type = 0;

    if (RegQueryValueExW(hardware_key,
                         L"ServiceName",
                         NULL,
                         &type,
                         (LPBYTE)service_name,
                         &service_name_size) == ERROR_SUCCESS) {

      interfaces.AppendString(nsString(service_name));  // is this allowed?
    }
    RegCloseKey(hardware_key);
  }
  RegCloseKey(network_cards_key);
}


PRBool IsRunningOnVista() {
  static DWORD os_major_version = 0;
  static DWORD platform_id = 0;

  if (0 == os_major_version) {
    OSVERSIONINFO os_version = {0};
    os_version.dwOSVersionInfoSize = sizeof(os_version);
    GetVersionEx(&os_version);
    os_major_version = os_version.dwMajorVersion;
    platform_id = os_version.dwPlatformId;
  }

  return (6 == os_major_version) && (VER_PLATFORM_WIN32_NT == platform_id);
}
#endif

nsresult
nsWifiMonitor::DoScan()
{
#ifndef WINCE
  if (!IsRunningOnVista()) {
#else
    HANDLE ndis_handle;
    GETADAPTERSINFO pGetAdaptersInfo;
    nsresult rc = SetupWince(ndis_handle, pGetAdaptersInfo);
    if (rc != NS_OK)
      return rc;
#endif

    nsCOMArray<nsWifiAccessPoint> lastAccessPoints;
    nsCOMArray<nsWifiAccessPoint> accessPoints;

    do {
      accessPoints.Clear();

      nsStringArray interfaces;
#ifdef WINCE
      GetNetworkInterfaces(pGetAdaptersInfo, interfaces);
#else
      GetNetworkInterfaces(interfaces);
#endif

      for (int i = 0; i < interfaces.Count(); i++) {
        nsString *s = interfaces.StringAt(i);
        const PRUnichar *service_name = s->get();

#ifndef WINCE
        bool dosDeviceDefined = false;
        if (!DefineDosDeviceIfNotExists(service_name, &dosDeviceDefined))
          continue;

        // Get the handle to the device. This will fail if the named device is not
        // valid.

        HANDLE adapter_handle = GetFileHandle(service_name);
        if (adapter_handle == INVALID_HANDLE_VALUE)
          continue;
#else
        BYTE *data; // will store address of  NDIS_802_11_BSSID_LIST data
#endif

        // Get the data.

        BYTE *buffer = (BYTE*)malloc(oid_buffer_size_);
        if (buffer == NULL) {
#ifdef WINCE
          CloseHandle(ndis_handle);
#endif
          return NS_ERROR_OUT_OF_MEMORY;
        }

        DWORD bytes_out;
        int result;

        while (true) {

          NS_ASSERTION(buffer && oid_buffer_size_ > 0, "buffer must not be null, and the size must be larger than 0");

          bytes_out = 0;
#ifdef WINCE
          result = PerformQuery(ndis_handle, service_name, buffer, oid_buffer_size_, data, &bytes_out);
#else
          result = PerformQuery(adapter_handle, buffer, oid_buffer_size_, &bytes_out);
#endif

          if (result == ERROR_GEN_FAILURE ||  // Returned by some Intel cards.
              result == ERROR_INVALID_USER_BUFFER || // Returned on the Samsung Omnia II.
              result == ERROR_INSUFFICIENT_BUFFER ||
              result == ERROR_MORE_DATA ||
              result == NDIS_STATUS_INVALID_LENGTH ||
              result == NDIS_STATUS_BUFFER_TOO_SHORT) {

            // The buffer we supplied is too small, so increase it. bytes_out should
            // provide the required buffer size, but this is not always the case.

            if (bytes_out > static_cast<DWORD>(oid_buffer_size_)) {
              oid_buffer_size_ = bytes_out;
            } else {
              oid_buffer_size_ *= 2;
            }

            if (!ResizeBuffer(oid_buffer_size_, &buffer)) {
              oid_buffer_size_ = kInitialBufferSize;  // Reset for next time.
              continue;
            }
          } else {
            // The buffer is not too small.
            break;
          }
        }

        if (result == ERROR_SUCCESS) {
#ifdef WINCE
          NDIS_802_11_BSSID_LIST* bssid_list = (NDIS_802_11_BSSID_LIST*)data;
#else
          NDIS_802_11_BSSID_LIST* bssid_list = (NDIS_802_11_BSSID_LIST*)buffer;
#endif

          // Walk through the BSS IDs.
          const uint8 *iterator = (const uint8*)&bssid_list->Bssid[0];
          const uint8 *end_of_buffer = (const uint8*)buffer + oid_buffer_size_;
          for (int i = 0; i < static_cast<int>(bssid_list->NumberOfItems); ++i) {

            const NDIS_WLAN_BSSID *bss_id = (const NDIS_WLAN_BSSID*)iterator;

            // Check that the length of this BSS ID is reasonable.
            if (bss_id->Length < sizeof(NDIS_WLAN_BSSID) ||
                iterator + bss_id->Length > end_of_buffer) {
              break;
            }

            nsWifiAccessPoint* ap = new nsWifiAccessPoint();
            if (!ap)
              continue;

            ap->setMac(bss_id->MacAddress);
            ap->setSignal(bss_id->Rssi);
            ap->setSSID((char*) bss_id->Ssid.Ssid, bss_id->Ssid.SsidLength);

            accessPoints.AppendObject(ap);

            // Move to the next BSS ID.
            iterator += bss_id->Length;
          }

          free(buffer);

          // Clean up.
#ifndef WINCE
          CloseHandle(adapter_handle);
#endif
        }

#ifndef WINCE
        if (dosDeviceDefined)
          UndefineDosDevice(service_name);
#endif
      }

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
      if (!result) {
#ifdef WINCE
        CloseHandle(ndis_handle);
#endif
        return NS_ERROR_OUT_OF_MEMORY;
      }

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



#ifdef WINCE
    //Clean up
    CloseHandle(ndis_handle);
#else
  }
  else {

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
#endif
  return NS_OK;
}
