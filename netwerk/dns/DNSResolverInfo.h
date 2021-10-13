/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_DNSResolverInfo_h__
#define mozilla_net_DNSResolverInfo_h__

#include "nsIDNSResolverInfo.h"

namespace mozilla {
namespace net {

class DNSResolverInfo : public nsIDNSResolverInfo {
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIDNSRESOLVERINFO
 public:
  explicit DNSResolverInfo(const nsACString& aURL) : mURL(aURL){};
  static nsCString URL(nsIDNSResolverInfo* aResolver) {
    nsCString url;
    if (aResolver) {
      MOZ_ALWAYS_SUCCEEDS(aResolver->GetURL(url));
    }
    return url;
  }

 private:
  virtual ~DNSResolverInfo() = default;
  nsCString mURL;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_DNSResolverInfo_h__
