/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_toolkit_system_windowsDHCPClient_DHCPUtils_h
#define mozilla_toolkit_system_windowsDHCPClient_DHCPUtils_h

#include "WindowsNetworkFunctionsWrapper.h"
#include <vector>

namespace mozilla {
namespace toolkit {
namespace system {
namespace windowsDHCPClient {

nsresult GetActiveDHCPNetworkAdapterName(
  nsACString& aNetworkAdapterName,
  WindowsNetworkFunctionsWrapper* aWindowsNetworkFunctionsWrapper);

nsresult RetrieveOption(
  const nsACString&  aAdapterName,
  uint8_t           aOption,
  std::vector<char>& aOptionValueBuf,
  uint32_t*         aOptionSize,
  WindowsNetworkFunctionsWrapper* aWindowsNetworkFunctionsWrapper
);

} // namespace windowsDHCPClient
} // namespace system
} // namespace toolkit
} // namespace mozilla
#endif // mozilla_toolkit_system_windowsDHCPClient_DHCPUtils_h
