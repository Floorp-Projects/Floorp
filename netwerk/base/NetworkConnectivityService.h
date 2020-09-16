/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NetworkConnectivityService_h_
#define NetworkConnectivityService_h_

#include "nsINetworkConnectivityService.h"
#include "nsIObserver.h"
#include "nsIDNSListener.h"
#include "nsIStreamListener.h"

namespace mozilla {
namespace net {

class NetworkConnectivityService : public nsINetworkConnectivityService,
                                   public nsIObserver,
                                   public nsIDNSListener,
                                   public nsIStreamListener {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSINETWORKCONNECTIVITYSERVICE
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIDNSLISTENER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIREQUESTOBSERVER

  static already_AddRefed<NetworkConnectivityService> GetSingleton();

 private:
  NetworkConnectivityService() = default;
  virtual ~NetworkConnectivityService() = default;

  nsresult Init();
  // Calls all the check methods
  void PerformChecks();

  // Will be set to OK if the DNS request returned in IP of this type,
  //                NOT_AVAILABLE if that type of resolution is not available
  //                UNKNOWN if the check wasn't performed
  ConnectivityState mDNSv4 = nsINetworkConnectivityService::UNKNOWN;
  ConnectivityState mDNSv6 = nsINetworkConnectivityService::UNKNOWN;

  ConnectivityState mIPv4 = nsINetworkConnectivityService::UNKNOWN;
  ConnectivityState mIPv6 = nsINetworkConnectivityService::UNKNOWN;

  nsCOMPtr<nsICancelable> mDNSv4Request;
  nsCOMPtr<nsICancelable> mDNSv6Request;

  nsCOMPtr<nsIChannel> mIPv4Channel;
  nsCOMPtr<nsIChannel> mIPv6Channel;

  bool mCheckedNetworkId = false;
  bool mHasNetworkId = false;
};

}  // namespace net
}  // namespace mozilla

#endif  // NetworkConnectivityService_h_
