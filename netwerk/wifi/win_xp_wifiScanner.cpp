// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Windows Vista uses the Native Wifi (WLAN) API for accessing WiFi cards. See
// http://msdn.microsoft.com/en-us/library/ms705945(VS.85).aspx. Windows XP
// Service Pack 3 (and Windows XP Service Pack 2, if upgraded with a hot fix)
// also support a limited version of the WLAN API. See
// http://msdn.microsoft.com/en-us/library/bb204766.aspx. The WLAN API uses
// wlanapi.h, which is not part of the SDK used by Gears, so is replicated
// locally using data from the MSDN.
//\
// Windows XP from Service Pack 2 onwards supports the Wireless Zero
// Configuration (WZC) programming interface. See
// http://msdn.microsoft.com/en-us/library/ms706587(VS.85).aspx.
//
// The MSDN recommends that one use the WLAN API where available, and WZC
// otherwise.
//
// However, it seems that WZC fails for some wireless cards. Also, WLAN seems
// not to work on XP SP3. So we use WLAN on Vista, and use NDIS directly
// otherwise.

// MOZILLA NOTE:
// This code is ported from chromium:
// https://chromium.googlesource.com/chromium/src/+/master/content/browser/geolocation/wifi_data_provider_win.cc
// Based on changeset 42c5878

#include "win_xp_wifiScanner.h"
#include "nsWifiAccessPoint.h"
#include <windows.h>
#include <winioctl.h>
#include <wlanapi.h>
#include <string>
#include <vector>

// Taken from ndis.h for WinCE.
#define NDIS_STATUS_INVALID_LENGTH   ((NDIS_STATUS)0xC0010014L)
#define NDIS_STATUS_BUFFER_TOO_SHORT ((NDIS_STATUS)0xC0010016L)

namespace {
// The limits on the size of the buffer used for the OID query.
const int kInitialBufferSize = 2 << 12;  // Good for about 50 APs.
const int kMaximumBufferSize = 2 << 20;  // 2MB

// Length for generic string buffers passed to Win32 APIs.
const int kStringLength = 512;

// WlanOpenHandle
typedef DWORD (WINAPI* WlanOpenHandleFunction)(DWORD dwClientVersion,
                                               PVOID pReserved,
                                               PDWORD pdwNegotiatedVersion,
                                               PHANDLE phClientHandle);

// WlanEnumInterfaces
typedef DWORD (WINAPI* WlanEnumInterfacesFunction)(
    HANDLE hClientHandle,
    PVOID pReserved,
    PWLAN_INTERFACE_INFO_LIST* ppInterfaceList);

// WlanGetNetworkBssList
typedef DWORD (WINAPI* WlanGetNetworkBssListFunction)(
    HANDLE hClientHandle,
    const GUID* pInterfaceGuid,
    const  PDOT11_SSID pDot11Ssid,
    DOT11_BSS_TYPE dot11BssType,
    BOOL bSecurityEnabled,
    PVOID pReserved,
    PWLAN_BSS_LIST* ppWlanBssList
);

// WlanFreeMemory
typedef VOID (WINAPI* WlanFreeMemoryFunction)(PVOID pMemory);

// WlanCloseHandle
typedef DWORD (WINAPI* WlanCloseHandleFunction)(HANDLE hClientHandle,
                                                PVOID pReserved);

// Extracts data for an access point and converts to Gears format.
bool UndefineDosDevice(const std::string& device_name);
bool DefineDosDeviceIfNotExists(const std::string& device_name);
HANDLE GetFileHandle(const std::string& device_name);
// Makes the OID query and returns a Win32 error code.
int PerformQuery(HANDLE adapter_handle, std::vector<char>& buffer, DWORD* bytes_out);
bool ResizeBuffer(size_t requested_size, std::vector<char>& buffer);
// Gets the system directory and appends a trailing slash if not already
// present.
bool GetSystemDirectory(std::string* path);

bool ConvertToAccessPointData(const NDIS_WLAN_BSSID& data, nsWifiAccessPoint* access_point_data);
int GetDataFromBssIdList(const NDIS_802_11_BSSID_LIST& bss_id_list,
                         int list_size,
                         nsCOMArray<nsWifiAccessPoint>& outData);
} // namespace

class WindowsNdisApi
{
public:
  virtual ~WindowsNdisApi();
  static WindowsNdisApi* Create();
  virtual bool GetAccessPointData(nsCOMArray<nsWifiAccessPoint>& outData);

private:
  static bool GetInterfacesNDIS(std::vector<std::string>& interface_service_names_out);
  // Swaps in content of the vector passed
  explicit WindowsNdisApi(std::vector<std::string>* interface_service_names);
  bool GetInterfaceDataNDIS(HANDLE adapter_handle, nsCOMArray<nsWifiAccessPoint>& outData);
  // NDIS variables.
  std::vector<std::string> interface_service_names_;
  std::vector<char> _buffer;
};

// WindowsNdisApi
WindowsNdisApi::WindowsNdisApi(
    std::vector<std::string>* interface_service_names)
    : _buffer(kInitialBufferSize) {
  interface_service_names_.swap(*interface_service_names);
}

WindowsNdisApi::~WindowsNdisApi() {
}

WindowsNdisApi* WindowsNdisApi::Create() {
  std::vector<std::string> interface_service_names;
  if (GetInterfacesNDIS(interface_service_names)) {
    return new WindowsNdisApi(&interface_service_names);
  }
  return NULL;
}

bool WindowsNdisApi::GetAccessPointData(nsCOMArray<nsWifiAccessPoint>& outData) {
  int interfaces_failed = 0;
  int interfaces_succeeded = 0;

  for (int i = 0; i < static_cast<int>(interface_service_names_.size()); ++i) {
    // First, check that we have a DOS device for this adapter.
    if (!DefineDosDeviceIfNotExists(interface_service_names_[i])) {
      continue;
    }

    // Get the handle to the device. This will fail if the named device is not
    // valid.
    HANDLE adapter_handle = GetFileHandle(interface_service_names_[i]);
    if (adapter_handle == INVALID_HANDLE_VALUE) {
      continue;
    }

    // Get the data.
    if (GetInterfaceDataNDIS(adapter_handle, outData)) {
      ++interfaces_succeeded;
    } else {
      ++interfaces_failed;
    }

    // Clean up.
    CloseHandle(adapter_handle);
    UndefineDosDevice(interface_service_names_[i]);
  }

  // Return true if at least one interface succeeded, or at the very least none
  // failed.
  return interfaces_succeeded > 0 || interfaces_failed == 0;
}

bool WindowsNdisApi::GetInterfacesNDIS(std::vector<std::string>& interface_service_names_out) {
  HKEY network_cards_key = NULL;
  if (RegOpenKeyEx(
      HKEY_LOCAL_MACHINE,
      "Software\\Microsoft\\Windows NT\\CurrentVersion\\NetworkCards",
      0,
      KEY_READ,
      &network_cards_key) != ERROR_SUCCESS) {
    return false;
  }
  if (!network_cards_key) {
    return false;
  }

  for (int i = 0; ; ++i) {
    TCHAR name[kStringLength];
    DWORD name_size = kStringLength;
    FILETIME time;
    if (RegEnumKeyEx(network_cards_key,
                     i,
                     name,
                     &name_size,
                     NULL,
                     NULL,
                     NULL,
                     &time) != ERROR_SUCCESS) {
      break;
    }
    HKEY hardware_key = NULL;
    if (RegOpenKeyEx(network_cards_key, name, 0, KEY_READ, &hardware_key) !=
        ERROR_SUCCESS) {
      break;
    }
    if (!hardware_key) {
      return false;
    }

    TCHAR service_name[kStringLength];
    DWORD service_name_size = kStringLength;
    DWORD type = 0;
    if (RegQueryValueEx(hardware_key,
                        "ServiceName",
                        NULL,
                        &type,
                        reinterpret_cast<LPBYTE>(service_name),
                        &service_name_size) == ERROR_SUCCESS) {
      interface_service_names_out.push_back(service_name);
    }
    RegCloseKey(hardware_key);
  }

  RegCloseKey(network_cards_key);
  return true;
}

bool WindowsNdisApi::GetInterfaceDataNDIS(HANDLE adapter_handle,
                                          nsCOMArray<nsWifiAccessPoint>& outData) {
  DWORD bytes_out;
  int result;

  while (true) {
    bytes_out = 0;
    result = PerformQuery(adapter_handle, _buffer, &bytes_out);
    if (result == ERROR_GEN_FAILURE ||  // Returned by some Intel cards.
        result == ERROR_INSUFFICIENT_BUFFER ||
        result == ERROR_MORE_DATA ||
        result == NDIS_STATUS_INVALID_LENGTH ||
        result == NDIS_STATUS_BUFFER_TOO_SHORT) {
      // The buffer we supplied is too small, so increase it. bytes_out should
      // provide the required buffer size, but this is not always the case.
      size_t newSize;
      if (bytes_out > static_cast<DWORD>(_buffer.size())) {
        newSize = bytes_out;
      } else {
        newSize = _buffer.size() * 2;
      }
      if (!ResizeBuffer(newSize, _buffer)) {
        return false;
      }
    } else {
      // The buffer is not too small.
      break;
    }
  }

  if (result == ERROR_SUCCESS) {
    NDIS_802_11_BSSID_LIST* bssid_list =
        reinterpret_cast<NDIS_802_11_BSSID_LIST*>(&_buffer[0]);
    GetDataFromBssIdList(*bssid_list, _buffer.size(), outData);
  }

  return true;
}

namespace {
#define uint8 unsigned char

bool ConvertToAccessPointData(const NDIS_WLAN_BSSID& data, nsWifiAccessPoint* access_point_data)
{
  access_point_data->setMac(data.MacAddress);
  access_point_data->setSignal(data.Rssi);
  // Note that _NDIS_802_11_SSID::Ssid::Ssid is not null-terminated.
  const unsigned char* ssid = data.Ssid.Ssid;
  size_t len = data.Ssid.SsidLength;
  access_point_data->setSSID(reinterpret_cast<const char*>(ssid), len);
  return true;
}

int GetDataFromBssIdList(const NDIS_802_11_BSSID_LIST& bss_id_list,
                         int list_size,
                         nsCOMArray<nsWifiAccessPoint>& outData)
{
  // Walk through the BSS IDs.
  int found = 0;
  const uint8* iterator = reinterpret_cast<const uint8*>(&bss_id_list.Bssid[0]);
  const uint8* end_of_buffer =
      reinterpret_cast<const uint8*>(&bss_id_list) + list_size;
  for (int i = 0; i < static_cast<int>(bss_id_list.NumberOfItems); ++i) {
    const NDIS_WLAN_BSSID *bss_id =
        reinterpret_cast<const NDIS_WLAN_BSSID*>(iterator);
    // Check that the length of this BSS ID is reasonable.
    if (bss_id->Length < sizeof(NDIS_WLAN_BSSID) ||
        iterator + bss_id->Length > end_of_buffer) {
      break;
    }
    nsWifiAccessPoint* ap = new nsWifiAccessPoint();
    if (ConvertToAccessPointData(*bss_id, ap)) {
      outData.AppendObject(ap);
      ++found;
    }
    // Move to the next BSS ID.
    iterator += bss_id->Length;
  }
  return found;
}


bool UndefineDosDevice(const std::string& device_name) {
  // We remove only the mapping we use, that is \Device\<device_name>.
  std::string target_path = "\\Device\\" + device_name;
  return DefineDosDevice(
      DDD_RAW_TARGET_PATH | DDD_REMOVE_DEFINITION | DDD_EXACT_MATCH_ON_REMOVE,
      device_name.c_str(),
      target_path.c_str()) == TRUE;
}

bool DefineDosDeviceIfNotExists(const std::string& device_name) {
  // We create a DOS device name for the device at \Device\<device_name>.
  std::string target_path = "\\Device\\" + device_name;

  TCHAR target[kStringLength];
  if (QueryDosDevice(device_name.c_str(), target, kStringLength) > 0 &&
      target_path.compare(target) == 0) {
    // Device already exists.
    return true;
  }

  if (GetLastError() != ERROR_FILE_NOT_FOUND) {
    return false;
  }

  if (!DefineDosDevice(DDD_RAW_TARGET_PATH,
                       device_name.c_str(),
                       target_path.c_str())) {
    return false;
  }

  // Check that the device is really there.
  return QueryDosDevice(device_name.c_str(), target, kStringLength) > 0 &&
      target_path.compare(target) == 0;
}

HANDLE GetFileHandle(const std::string& device_name) {
  // We access a device with DOS path \Device\<device_name> at
  // \\.\<device_name>.
  std::string formatted_device_name = "\\\\.\\" + device_name;

  return CreateFile(formatted_device_name.c_str(),
                    GENERIC_READ,
                    FILE_SHARE_READ | FILE_SHARE_WRITE,  // share mode
                    0,  // security attributes
                    OPEN_EXISTING,
                    0,  // flags and attributes
                    INVALID_HANDLE_VALUE);
}

int PerformQuery(HANDLE adapter_handle,
                 std::vector<char>& buffer,
                 DWORD* bytes_out) {
  DWORD oid = OID_802_11_BSSID_LIST;
  if (!DeviceIoControl(adapter_handle,
                       IOCTL_NDIS_QUERY_GLOBAL_STATS,
                       &oid,
                       sizeof(oid),
                       &buffer[0],
                       buffer.size(),
                       bytes_out,
                       NULL)) {
    return GetLastError();
  }
  return ERROR_SUCCESS;
}

bool ResizeBuffer(size_t requested_size, std::vector<char>& buffer) {
  if (requested_size > kMaximumBufferSize) {
    buffer.resize(kInitialBufferSize);
    return false;
  }

  buffer.resize(requested_size);
  return true;
}

} // namespace


nsresult
WinXPWifiScanner::GetAccessPointsFromWLAN(nsCOMArray<nsWifiAccessPoint> &accessPoints)
{
  if (!mImplementation) {
    mImplementation = WindowsNdisApi::Create();
    if (!mImplementation) {
      return NS_ERROR_FAILURE;
    }
  }

  accessPoints.Clear();
  bool isOk = mImplementation->GetAccessPointData(accessPoints);
  if (!isOk) {
    mImplementation = 0;
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}
