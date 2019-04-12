/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWindowsDHCPClient.h"

#include <vector>

#include "DHCPUtils.h"
#include "nsNetCID.h"
#include "nsString.h"
#include "mozilla/Logging.h"
#include "mozilla/Components.h"

namespace mozilla {
namespace toolkit {
namespace system {
namespace windowsDHCPClient {

LazyLogModule gDhcpLog("windowsDHCPClient");

#undef LOG
#define LOG(args) MOZ_LOG(gDhcpLog, LogLevel::Debug, args)

#define MOZ_MAX_DHCP_OPTION_LENGTH \
  255  // this is the maximum option length in DHCP V4 and 6

NS_IMPL_ISUPPORTS(nsWindowsDHCPClient, nsIDHCPClient)

NS_IMETHODIMP
nsWindowsDHCPClient::GetOption(uint8_t aOption, nsACString& aRetVal) {
  nsCString networkAdapterName;
  nsresult rv;
  rv = GetActiveDHCPNetworkAdapterName(networkAdapterName, mNetworkFunctions);
  if (rv != NS_OK) {
    LOG(
        ("Failed to get network adapter name in nsWindowsDHCPClient::GetOption "
         "due to error %d",
         rv));
    return rv;
  }

  uint32_t sizeOptionValue = MOZ_MAX_DHCP_OPTION_LENGTH;
  std::vector<char> optionValue;

  bool retryingAfterLossOfSignificantData = false;
  do {
    optionValue.resize(sizeOptionValue);
    rv = RetrieveOption(networkAdapterName, aOption, optionValue,
                        &sizeOptionValue, mNetworkFunctions);
    if (rv == NS_ERROR_LOSS_OF_SIGNIFICANT_DATA) {
      LOG((
          "In nsWindowsDHCPClient::GetOption, DHCP Option %d required %d bytes",
          aOption, sizeOptionValue));
      if (retryingAfterLossOfSignificantData) {
        break;
      }
      retryingAfterLossOfSignificantData = true;
    }
  } while (rv == NS_ERROR_LOSS_OF_SIGNIFICANT_DATA);
  if (rv != NS_OK) {
    LOG(
        ("Failed to get DHCP Option %d nsWindowsDHCPClient::GetOption due to "
         "error %d",
         aOption, rv));
    return rv;
  }
  aRetVal.Assign(optionValue.data(), sizeOptionValue);
  return NS_OK;
}

}  // namespace windowsDHCPClient
}  // namespace system
}  // namespace toolkit
}  // namespace mozilla
