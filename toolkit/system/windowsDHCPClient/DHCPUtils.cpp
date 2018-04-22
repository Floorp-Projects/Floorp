/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DHCPUtils.h"
#include <vector>
#include "mozilla\Logging.h"
#include "nsString.h"


#define MOZ_WORKING_BUFFER_SIZE_NETWORK_ADAPTERS 15000
#define MOZ_WORKING_BUFFER_SIZE_DHCP_PARAMS 1000
#define MOZ_MAX_TRIES 3
namespace mozilla {
namespace toolkit {
namespace system {
namespace windowsDHCPClient {

//
// The comments on this page reference the following Microsoft documentation pages (both retrieved 2017-06-27)
// [1] https://msdn.microsoft.com/en-us/library/windows/desktop/aa365915(v=vs.85).aspx
// [2] https://msdn.microsoft.com/en-us/library/windows/desktop/aa363298(v=vs.85).aspx
mozilla::LazyLogModule gDhcpUtilsLog("dhcpUtils");

#undef LOG
#define LOG(args) MOZ_LOG(gDhcpUtilsLog, LogLevel::Debug, args)

bool
IsCurrentAndHasDHCP(PIP_ADAPTER_ADDRESSES aAddresses)
{
  return aAddresses->OperStatus == 1 &&
      (aAddresses->Dhcpv4Server.iSockaddrLength ||
      aAddresses->Dhcpv6Server.iSockaddrLength);
}

nsresult
GetActiveDHCPNetworkAdapterName(nsACString& aNetworkAdapterName,
    WindowsNetworkFunctionsWrapper* aWindowsNetworkFunctionsWrapper)
{
  /* Declare and initialize variables */

  uint32_t dwSize = 0;
  uint32_t dwRetVal = 0;
  nsresult rv = NS_ERROR_FAILURE;

  // Set the flags to pass to GetAdaptersAddresses
  uint32_t flags = GAA_FLAG_INCLUDE_PREFIX;

  // default to unspecified address family (both)
  uint32_t family = AF_UNSPEC;

  // Allocate a 15 KB buffer to start with.
  uint32_t outBufLen = MOZ_WORKING_BUFFER_SIZE_NETWORK_ADAPTERS;
  uint32_t iterations = 0;

  aNetworkAdapterName.Truncate();

  // Now we try calling the GetAdaptersAddresses method until the return value
  // is not ERROR_BUFFER_OVERFLOW. According to [1]
  //
  //
  // > When the return value is ERROR_BUFFER_OVERFLOW, the SizePointer parameter returned
  // > points to the required size of the buffer to hold the adapter information.
  // > Note that it is possible for the buffer size required for the IP_ADAPTER_ADDRESSES
  // > structures pointed to by the AdapterAddresses parameter to change between
  // > subsequent calls to the GetAdaptersAddresses function if an adapter address
  // > is added or removed. However, this method of using the GetAdaptersAddresses
  // > function is strongly discouraged. This method requires calling the
  // > GetAdaptersAddresses function multiple times.
  // >
  // > The recommended method of calling the GetAdaptersAddresses function is
  // > to pre-allocate a 15KB working buffer pointed to by the AdapterAddresses parameter.
  // > On typical computers, this dramatically reduces the chances that the
  // > GetAdaptersAddresses function returns ERROR_BUFFER_OVERFLOW, which would require
  // > calling GetAdaptersAddresses function multiple times.
  //
  //
  // The possibility of the buffer size changing between calls to
  // GetAdaptersAddresses is why we allow the following code to be called several times,
  // rather than just the two that would be neccessary if we could rely on the
  // value returned in outBufLen being the true size needed.

  std::vector<IP_ADAPTER_ADDRESSES> pAddresses;
  do {
    pAddresses.resize(outBufLen/sizeof(IP_ADAPTER_ADDRESSES));

    dwRetVal =
      aWindowsNetworkFunctionsWrapper->GetAdaptersAddressesWrapped(
        family, flags, nullptr, pAddresses.data(), (PULONG)&outBufLen);

    if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
      iterations++;
    }
  } while (dwRetVal == ERROR_BUFFER_OVERFLOW && iterations < MOZ_MAX_TRIES);

  switch(dwRetVal) {
    case NO_ERROR:
      {
        // set default return value if we don't find a suitable network adapter
        rv = NS_ERROR_NOT_AVAILABLE;
        PIP_ADAPTER_ADDRESSES pCurrAddresses = pAddresses.data();
        while (pCurrAddresses) {
          if (IsCurrentAndHasDHCP(pCurrAddresses)) {
            rv = NS_OK;
            aNetworkAdapterName.Assign(pCurrAddresses->AdapterName);
            break;
          }
          pCurrAddresses = pCurrAddresses->Next;
        }
      }
      break;
    case ERROR_NO_DATA:
      rv = NS_ERROR_NOT_AVAILABLE;
      break;
    default:
      MOZ_LOG(gDhcpUtilsLog, mozilla::LogLevel::Warning,
              ("GetAdaptersAddresses returned %d", dwRetVal));
      rv = NS_ERROR_FAILURE;
      break;
  }
  return rv;
}


DWORD
IterateDHCPInformRequestsUntilBufferLargeEnough(
    DHCPCAPI_PARAMS&     aDhcpRequestedOptionParams,
     wchar_t*            aWideNetworkAdapterName,
     std::vector<char>&  aBuffer,
     WindowsNetworkFunctionsWrapper* aWindowsNetworkFunctionsWrapper)
{
  uint32_t iterations = 0;
  uint32_t outBufLen = MOZ_WORKING_BUFFER_SIZE_DHCP_PARAMS;

  DHCPCAPI_PARAMS_ARRAY RequestParams = {
    1,  // only one option to request
    &aDhcpRequestedOptionParams
  };

  // According to [2],
  // the following is for 'Optional data to be requested,
  // in addition to the data requested in the RecdParams array.'
  // We are not requesting anything in addition, so this is empty.
  DHCPCAPI_PARAMS_ARRAY SendParams = {
    0,
    nullptr
  };

  DWORD winAPIResponse;
  // Now we try calling the DHCPRequestParams method until the return value
  // is not ERROR_MORE_DATA. According to [2]:
  //
  //
  // > Note that the required size of Buffer may increase during the time that elapses
  // > between the initial function call's return and a subsequent call;
  // > therefore, the required size of Buffer (indicated in pSize)
  // > provides an indication of the approximate size required of Buffer,
  // > rather than guaranteeing that subsequent calls will return successfully
  // > if Buffer is set to the size indicated in pSize.
  //
  //
  // This is why we allow this DHCPRequestParams to be called several times,
  // rather than just the two that would be neccessary if we could rely on the
  // value returned in outBufLen being the true size needed.
  do {
    aBuffer.resize(outBufLen);

    winAPIResponse = aWindowsNetworkFunctionsWrapper->DhcpRequestParamsWrapped(
      DHCPCAPI_REQUEST_SYNCHRONOUS, // Flags
      nullptr,                         // Reserved
      aWideNetworkAdapterName,               // Adapter Name
      nullptr,                         // not using class id
      SendParams,                         // sent parameters
      RequestParams,                // requesting params
      (PBYTE)aBuffer.data(),            // buffer for the output of RequestParams
      (PULONG)&outBufLen,                      // buffer size
      nullptr                          // Request ID for persistent requests - not needed here
    );

    if (winAPIResponse == ERROR_MORE_DATA) {
      iterations++;
    }
  } while (winAPIResponse == ERROR_MORE_DATA && iterations < MOZ_MAX_TRIES);
  return winAPIResponse;
}

nsresult
RetrieveOption(
  const nsACString&   aAdapterName,
  uint8_t            aOption,
  std::vector<char>& aOptionValueBuf,
  uint32_t*          aOptionSize,
  WindowsNetworkFunctionsWrapper* aWindowsNetworkFunctionsWrapper)
{

  nsresult rv;
  nsAutoString wideNetworkAdapterName = NS_ConvertUTF8toUTF16(aAdapterName);

  DHCPCAPI_PARAMS DhcpRequestedOptionParams = {
    0,                //  Flags - Reserved, must be set to zero [2]
    aOption, // OptionId
    false,            // whether this is vendor specific - let's assume not
    nullptr,             // data filled in on return
    0                // nBytes used by return data
  };

  std::vector<char> tmpBuffer(MOZ_WORKING_BUFFER_SIZE_DHCP_PARAMS);  // a buffer for the DHCP response object
  DWORD winAPIResponse = IterateDHCPInformRequestsUntilBufferLargeEnough(DhcpRequestedOptionParams,
     wideNetworkAdapterName.get(),
     tmpBuffer,
     aWindowsNetworkFunctionsWrapper);

  switch (winAPIResponse){
    case NO_ERROR:
      {
        if (DhcpRequestedOptionParams.nBytesData == 0) {
          *aOptionSize = 0;
          rv = NS_ERROR_NOT_AVAILABLE;
          break;
        }

        if (*aOptionSize >= DhcpRequestedOptionParams.nBytesData) {
          rv = NS_OK;
        } else {
          rv = NS_ERROR_LOSS_OF_SIGNIFICANT_DATA;
        }

        uint32_t actualSizeReturned =
              *aOptionSize > DhcpRequestedOptionParams.nBytesData?
              DhcpRequestedOptionParams.nBytesData: *aOptionSize;

        memcpy(aOptionValueBuf.data(),
              DhcpRequestedOptionParams.Data, actualSizeReturned);
        *aOptionSize = DhcpRequestedOptionParams.nBytesData;
        break;
      }
    case ERROR_INVALID_PARAMETER:
      MOZ_LOG(gDhcpUtilsLog, mozilla::LogLevel::Warning,
          ("RetrieveOption returned %d (ERROR_INVALID_PARAMETER) when option %d requested",
           winAPIResponse, aOption));
      rv = NS_ERROR_INVALID_ARG;
      break;
    default:
      MOZ_LOG(gDhcpUtilsLog, mozilla::LogLevel::Warning,
          ("RetrieveOption returned %d when option %d requested", winAPIResponse, aOption));
      rv = NS_ERROR_FAILURE;
  }
  return rv;
}

} // namespace windowsDHCPClient
} // namespace system
} // namespace toolkit
} // namespace mozilla
