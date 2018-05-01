/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_NullHttpChannel_h
#define mozilla_net_NullHttpChannel_h

#include "nsINullChannel.h"
#include "nsIHttpChannel.h"
#include "nsITimedChannel.h"
#include "nsIURI.h"
#include "nsCOMPtr.h"
#include "mozilla/TimeStamp.h"
#include "nsString.h"
#include "prtime.h"

namespace mozilla {
namespace net {

class nsProxyInfo;

class NullHttpChannel final
  : public nsINullChannel
  , public nsIHttpChannel
  , public nsITimedChannel
{
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSINULLCHANNEL
  NS_DECL_NSIHTTPCHANNEL
  NS_DECL_NSITIMEDCHANNEL
  NS_DECL_NSIREQUEST
  NS_DECL_NSICHANNEL

  NullHttpChannel();

  // Copies the URI, Principal and Timing-Allow-Origin headers from the
  // passed channel to this object, to be used for resource timing checks
  explicit NullHttpChannel(nsIHttpChannel * chan);

  // Same signature as nsHttpChannel::Init
  MOZ_MUST_USE nsresult Init(nsIURI *aURI, uint32_t aCaps,
                             nsProxyInfo *aProxyInfo,
                             uint32_t aProxyResolveFlags, nsIURI *aProxyURI);
private:
  ~NullHttpChannel() = default;

protected:
  nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsIURI> mOriginalURI;

  nsString  mInitiatorType;
  PRTime    mChannelCreationTime;
  TimeStamp mAsyncOpenTime;
  TimeStamp mChannelCreationTimestamp;
  nsCOMPtr<nsIPrincipal> mResourcePrincipal;
  nsCString mTimingAllowOriginHeader;
  bool      mAllRedirectsSameOrigin;
  bool      mAllRedirectsPassTimingAllowCheck;
};

} // namespace net
} // namespace mozilla



#endif // mozilla_net_NullHttpChannel_h
