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
#include "nsINetworkStatsServiceProxy.h"
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

class SaveNetworkStatsEvent : public Runnable {
public:
  SaveNetworkStatsEvent(uint32_t aAppId,
                        bool aIsInIsolatedMozBrowser,
                        nsMainThreadPtrHandle<nsINetworkInfo> &aActiveNetworkInfo,
                        uint64_t aCountRecv,
                        uint64_t aCountSent,
                        bool aIsAccumulative)
    : mAppId(aAppId),
      mIsInIsolatedMozBrowser(aIsInIsolatedMozBrowser),
      mActiveNetworkInfo(aActiveNetworkInfo),
      mCountRecv(aCountRecv),
      mCountSent(aCountSent),
      mIsAccumulative(aIsAccumulative)
  {
    MOZ_ASSERT(mAppId != NECKO_NO_APP_ID);
    MOZ_ASSERT(mActiveNetworkInfo);
  }

  NS_IMETHOD Run() override
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
                                            mIsInIsolatedMozBrowser,
                                            mActiveNetworkInfo,
                                            PR_Now() / 1000,
                                            mCountRecv,
                                            mCountSent,
                                            mIsAccumulative,
                                            nullptr);

    return NS_OK;
  }
private:
  uint32_t mAppId;
  bool     mIsInIsolatedMozBrowser;
  nsMainThreadPtrHandle<nsINetworkInfo> mActiveNetworkInfo;
  uint64_t mCountRecv;
  uint64_t mCountSent;
  bool mIsAccumulative;
};

} // namespace mozilla:net
} // namespace mozilla

#endif // !NetStatistics_h__
