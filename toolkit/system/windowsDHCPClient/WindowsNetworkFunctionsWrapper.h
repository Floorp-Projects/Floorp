/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_toolkit_system_windowsDHCPClient_windowsNetworkFunctionsWrapper_h
#define mozilla_toolkit_system_windowsDHCPClient_windowsNetworkFunctionsWrapper_h

#include <winsock2.h>  // there is a compilation error if Winsock.h is not
                       // declared before dhcpcsdk.h
#include <dhcpcsdk.h>
#include <iphlpapi.h>

#include "nsISupports.h"

// Thin wrapper around low-level network functions needed for DHCP querying for
// web proxy
namespace mozilla {
namespace toolkit {
namespace system {
namespace windowsDHCPClient {

class WindowsNetworkFunctionsWrapper : nsISupports {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  WindowsNetworkFunctionsWrapper(){};

  virtual ULONG GetAdaptersAddressesWrapped(
      _In_ ULONG aFamily, _In_ ULONG aFlags, _In_ PVOID aReserved,
      _Inout_ PIP_ADAPTER_ADDRESSES aAdapterAddresses,
      _Inout_ PULONG aSizePointer);

  virtual DWORD DhcpRequestParamsWrapped(
      _In_ DWORD aFlags, _In_ LPVOID aReserved, _In_ LPWSTR aAdapterName,
      _In_ LPDHCPCAPI_CLASSID aClassId, _In_ DHCPCAPI_PARAMS_ARRAY aSendParams,
      _Inout_ DHCPCAPI_PARAMS_ARRAY aRecdParams, _In_ LPBYTE aBuffer,
      _Inout_ LPDWORD apSize, _In_ LPWSTR aRequestIdStr);

 protected:
  virtual ~WindowsNetworkFunctionsWrapper(){};
};

}  // namespace windowsDHCPClient
}  // namespace system
}  // namespace toolkit
}  // namespace mozilla
#endif  // mozilla_toolkit_system_windowsDHCPClient_windowsNetworkFunctionsWrapper_h
