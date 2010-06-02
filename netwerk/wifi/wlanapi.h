// Copyright 2008, Google Inc.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//  2. Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//  3. Neither the name of Google Inc. nor the names of its contributors may be
//     used to endorse or promote products derived from this software without
//     specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
// EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Replicates missing wlanapi.h. Taken from
// http://msdn.microsoft.com/en-us/library/bb204766.aspx.

// TODO(steveblock): Change naming convention to follow correct style.


// WlanOpenHandle

typedef DWORD (WINAPI *WlanOpenHandleFunction)(DWORD dwClientVersion,
                                               PVOID pReserved,
                                               PDWORD pdwNegotiatedVersion,
                                               PHANDLE phClientHandle);

// WlanEnumInterfaces

typedef enum _WLAN_INTERFACE_STATE {
  WLAN_INTERFACE_STATE_UNUSED
} WLAN_INTERFACE_STATE;

typedef struct _WLAN_INTERFACE_INFO {
  GUID InterfaceGuid;
  WCHAR strInterfaceDescription[256];
  WLAN_INTERFACE_STATE isState;
} WLAN_INTERFACE_INFO, *PWLAN_INTERFACE_INFO;

typedef struct _WLAN_INTERFACE_INFO_LIST {
  DWORD dwNumberOfItems;
  DWORD dwIndex;
  WLAN_INTERFACE_INFO InterfaceInfo[1];
} WLAN_INTERFACE_INFO_LIST, *PWLAN_INTERFACE_INFO_LIST;

typedef DWORD (WINAPI *WlanEnumInterfacesFunction)(
    HANDLE hClientHandle,
    PVOID pReserved,
    PWLAN_INTERFACE_INFO_LIST *ppInterfaceList);

// WlanGetNetworkBssList

#define DOT11_SSID_MAX_LENGTH 32

typedef struct _DOT11_SSID {
  ULONG uSSIDLength;
  UCHAR ucSSID[DOT11_SSID_MAX_LENGTH];
} DOT11_SSID, *PDOT11_SSID;

typedef UCHAR DOT11_MAC_ADDRESS[6];

typedef enum _DOT11_BSS_TYPE {
  DOT11_BSS_TYPE_UNUSED
} DOT11_BSS_TYPE;

typedef enum _DOT11_PHY_TYPE {
  DOT11_PHY_TYPE_UNUSED
} DOT11_PHY_TYPE;

#define DOT11_RATE_SET_MAX_LENGTH (126)

typedef struct _WLAN_RATE_SET {
  ULONG uRateSetLength;
  USHORT usRateSet[DOT11_RATE_SET_MAX_LENGTH];
} WLAN_RATE_SET, *PWLAN_RATE_SET;

typedef struct _WLAN_BSS_ENTRY {
  DOT11_SSID dot11Ssid;
  ULONG uPhyId;
  DOT11_MAC_ADDRESS dot11Bssid;
  DOT11_BSS_TYPE dot11BssType;
  DOT11_PHY_TYPE dot11BssPhyType;
  LONG lRssi;
  ULONG uLinkQuality;
  BOOLEAN bInRegDomain;
  USHORT usBeaconPeriod;
  ULONGLONG ullTimestamp;
  ULONGLONG ullHostTimestamp;
  USHORT usCapabilityInformation;
  ULONG ulChCenterFrequency;
  WLAN_RATE_SET wlanRateSet;
  ULONG ulIeOffset;
  ULONG ulIeSize;
} WLAN_BSS_ENTRY, *PWLAN_BSS_ENTRY;

typedef struct _WLAN_BSS_LIST {
  DWORD dwTotalSize;
  DWORD dwNumberOfItems;
  // Following data is an array of WLAN_BSS_ENTRY objects of length
  // dwNumberOfItems.
  WLAN_BSS_ENTRY wlanBssEntries[1];
} WLAN_BSS_LIST, *PWLAN_BSS_LIST;

typedef DWORD (WINAPI *WlanGetNetworkBssListFunction)(
    HANDLE hClientHandle,
    const GUID *pInterfaceGuid,
    const  PDOT11_SSID pDot11Ssid,
    DOT11_BSS_TYPE dot11BssType,
    BOOL bSecurityEnabled,
    PVOID pReserved,
    PWLAN_BSS_LIST *ppWlanBssList
);

// WlanFreeMemory

typedef VOID (WINAPI *WlanFreeMemoryFunction)(PVOID pMemory);

// WlanCloseHandle

typedef DWORD (WINAPI *WlanCloseHandleFunction)(HANDLE hClientHandle,
                                                PVOID pReserved);


