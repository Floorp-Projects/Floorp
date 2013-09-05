/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RedirectChannelRegistrar.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS1(RedirectChannelRegistrar, nsIRedirectChannelRegistrar)

RedirectChannelRegistrar::RedirectChannelRegistrar()
  : mRealChannels(64)
  , mParentChannels(64)
  , mId(1)
{
}

NS_IMETHODIMP
RedirectChannelRegistrar::RegisterChannel(nsIChannel *channel,
                                          uint32_t *_retval)
{
  mRealChannels.Put(mId, channel);
  *_retval = mId;

  ++mId;

  // Ensure we always provide positive ids
  if (!mId)
    mId = 1;

  return NS_OK;
}

NS_IMETHODIMP
RedirectChannelRegistrar::GetRegisteredChannel(uint32_t id,
                                               nsIChannel **_retval)
{
  if (!mRealChannels.Get(id, _retval))
    return NS_ERROR_NOT_AVAILABLE;

  return NS_OK;
}

NS_IMETHODIMP
RedirectChannelRegistrar::LinkChannels(uint32_t id,
                                       nsIParentChannel *channel,
                                       nsIChannel** _retval)
{
  if (!mRealChannels.Get(id, _retval))
    return NS_ERROR_NOT_AVAILABLE;

  mParentChannels.Put(id, channel);
  return NS_OK;
}

NS_IMETHODIMP
RedirectChannelRegistrar::GetParentChannel(uint32_t id,
                                           nsIParentChannel **_retval)
{
  if (!mParentChannels.Get(id, _retval))
    return NS_ERROR_NOT_AVAILABLE;

  return NS_OK;
}

NS_IMETHODIMP
RedirectChannelRegistrar::DeregisterChannels(uint32_t id)
{
  mRealChannels.Remove(id);
  mParentChannels.Remove(id);
  return NS_OK;
}

}
}
