/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_DNSAdditionalInfo_h__
#define mozilla_net_DNSAdditionalInfo_h__

#include "nsIDNSAdditionalInfo.h"
#include "nsString.h"

namespace mozilla {
namespace net {

class DNSAdditionalInfo : public nsIDNSAdditionalInfo {
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIDNSADDITIONALINFO
 public:
  explicit DNSAdditionalInfo(const nsACString& aURL, int32_t aPort)
      : mURL(aURL), mPort(aPort){};
  static nsCString URL(nsIDNSAdditionalInfo* aInfo) {
    nsCString url;
    if (aInfo) {
      MOZ_ALWAYS_SUCCEEDS(aInfo->GetResolverURL(url));
    }
    return url;
  }
  static int32_t Port(nsIDNSAdditionalInfo* aInfo) {
    int32_t port = -1;
    if (aInfo) {
      MOZ_ALWAYS_SUCCEEDS(aInfo->GetPort(&port));
    }
    return port;
  }

 private:
  virtual ~DNSAdditionalInfo() = default;
  nsCString mURL;
  int32_t mPort;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_DNSAdditionalInfo_h__
