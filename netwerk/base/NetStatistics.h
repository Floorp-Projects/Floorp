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
#include "nsINetworkManager.h"
#include "nsINetworkStatsServiceProxy.h"
#include "nsThreadUtils.h"
#include "nsProxyRelease.h"

namespace mozilla {
namespace net {

// The following members are used for network per-app metering.
const static uint64_t NETWORK_STATS_THRESHOLD = 65536;
const static char NETWORK_STATS_NO_SERVICE_TYPE[] = "";

inline nsresult
GetActiveNetworkInterface(nsCOMPtr<nsINetworkInterface> &aNetworkInterface)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv;
  nsCOMPtr<nsINetworkManager> networkManager =
    do_GetService("@mozilla.org/network/manager;1", &rv);

  if (NS_FAILED(rv) || !networkManager) {
    aNetworkInterface = nullptr;
    return rv;
  }

  networkManager->GetActive(getter_AddRefs(aNetworkInterface));

  return NS_OK;
}

class SaveNetworkStatsEvent : public nsRunnable {
public:
  SaveNetworkStatsEvent(uint32_t aAppId,
                        bool aIsInBrowser,
                        nsMainThreadPtrHandle<nsINetworkInterface> &aActiveNetwork,
                        uint64_t aCountRecv,
                        uint64_t aCountSent,
                        bool aIsAccumulative)
    : mAppId(aAppId),
      mIsInBrowser(aIsInBrowser),
      mActiveNetwork(aActiveNetwork),
      mCountRecv(aCountRecv),
      mCountSent(aCountSent),
      mIsAccumulative(aIsAccumulative)
  {
    MOZ_ASSERT(mAppId != NECKO_NO_APP_ID);
    MOZ_ASSERT(mActiveNetwork);
  }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsresult rv;
    nsCOMPtr<nsINetworkStatsServiceProxy> mNetworkStatsServiceProxy =
      do_GetService("@mozilla.org/networkstatsServiceProxy;1", &rv);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // save the network stats through NetworkStatsServiceProxy
    mNetworkStatsServiceProxy->SaveAppStats(mAppId,
                                            mIsInBrowser,
                                            mActiveNetwork,
                                            PR_Now() / 1000,
                                            mCountRecv,
                                            mCountSent,
                                            mIsAccumulative,
                                            nullptr);

    return NS_OK;
  }
private:
  uint32_t mAppId;
  bool     mIsInBrowser;
  nsMainThreadPtrHandle<nsINetworkInterface> mActiveNetwork;
  uint64_t mCountRecv;
  uint64_t mCountSent;
  bool mIsAccumulative;
};

} // namespace mozilla:net
} // namespace mozilla

#endif // !NetStatistics_h__
