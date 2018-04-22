/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 #include "WindowsNetworkFunctionsWrapper.h"


#pragma comment(lib, "IPHLPAPI.lib")
#pragma comment(lib, "dhcpcsvc.lib" )

namespace mozilla {
namespace toolkit {
namespace system {
namespace windowsDHCPClient {

NS_IMPL_ISUPPORTS(WindowsNetworkFunctionsWrapper, nsISupports)

ULONG WindowsNetworkFunctionsWrapper::GetAdaptersAddressesWrapped(
      _In_    ULONG                 aFamily,
      _In_    ULONG                 aFlags,
      _In_    PVOID                 aReserved,
      _Inout_ PIP_ADAPTER_ADDRESSES aAdapterAddresses,
      _Inout_ PULONG                aSizePointer)
{
  return GetAdaptersAddresses(aFamily, aFlags, aReserved, aAdapterAddresses, aSizePointer);
}

DWORD WindowsNetworkFunctionsWrapper::DhcpRequestParamsWrapped(
    _In_    DWORD                 aFlags,
    _In_    LPVOID                aReserved,
    _In_    LPWSTR                aAdapterName,
    _In_    LPDHCPCAPI_CLASSID    aClassId,
    _In_    DHCPCAPI_PARAMS_ARRAY aSendParams,
    _Inout_ DHCPCAPI_PARAMS_ARRAY aRecdParams,
    _In_    LPBYTE                aBuffer,
    _Inout_ LPDWORD               apSize,
    _In_    LPWSTR                aRequestIdStr)
{
  return DhcpRequestParams(aFlags, aReserved, aAdapterName, aClassId, aSendParams, aRecdParams, aBuffer, apSize, aRequestIdStr);
}
} // namespace windowsDHCPClient
} // namespace system
} // namespace toolkit
} // namespace mozilla
