/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DNSAdditionalInfo.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(DNSAdditionalInfo, nsIDNSAdditionalInfo)

NS_IMETHODIMP
DNSAdditionalInfo::GetPort(int32_t* aPort) {
  *aPort = mPort;
  return NS_OK;
}

NS_IMETHODIMP
DNSAdditionalInfo::GetResolverURL(nsACString& aURL) {
  aURL = mURL;
  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
