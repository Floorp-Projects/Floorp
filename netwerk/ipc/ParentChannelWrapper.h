/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_ParentChannelWrapper_h
#define mozilla_net_ParentChannelWrapper_h

#include "nsIParentChannel.h"
#include "nsIStreamListener.h"

namespace mozilla {
namespace net {

class ParentChannelWrapper : public nsIParentChannel {
 public:
  ParentChannelWrapper(nsIChannel* aChannel, nsIStreamListener* aListener)
      : mChannel(aChannel), mListener(aListener) {}

  // Registers this nsIParentChannel wrapper with the RedirectChannelRegistrar
  // and holds a reference.
  void Register(uint64_t aRegistrarId);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIPARENTCHANNEL
  NS_FORWARD_NSISTREAMLISTENER(mListener->)
  NS_FORWARD_NSIREQUESTOBSERVER(mListener->)

 private:
  virtual ~ParentChannelWrapper() = default;
  const nsCOMPtr<nsIChannel> mChannel;
  const nsCOMPtr<nsIStreamListener> mListener;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_ParentChannelWrapper_h
