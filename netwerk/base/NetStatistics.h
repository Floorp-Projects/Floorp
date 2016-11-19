/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NetStatistics_h__
#define NetStatistics_h__

#include "mozilla/Assertions.h"

#include "nsCOMPtr.h"
#include "nsError.h"
#include "nsINetworkInterface.h"
#include "nsINetworkManager.h"
#include "nsThreadUtils.h"
#include "nsProxyRelease.h"

namespace mozilla {
namespace net {

// The following members are used for network per-app metering.
const static uint64_t NETWORK_STATS_THRESHOLD = 65536;
const static char NETWORK_STATS_NO_SERVICE_TYPE[] = "";

inline nsresult
GetActiveNetworkInfo(nsCOMPtr<nsINetworkInfo> &aNetworkInfo)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv;
  nsCOMPtr<nsINetworkManager> networkManager =
    do_GetService("@mozilla.org/network/manager;1", &rv);

  if (NS_FAILED(rv) || !networkManager) {
    aNetworkInfo = nullptr;
    return rv;
  }

  networkManager->GetActiveNetworkInfo(getter_AddRefs(aNetworkInfo));

  return NS_OK;
}

} // namespace mozilla:net
} // namespace mozilla

#endif // !NetStatistics_h__
