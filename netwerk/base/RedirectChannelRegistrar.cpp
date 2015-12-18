/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RedirectChannelRegistrar.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(RedirectChannelRegistrar, nsIRedirectChannelRegistrar)

RedirectChannelRegistrar::RedirectChannelRegistrar()
  : mRealChannels(32)
  , mParentChannels(32)
  , mId(1)
  , mLock("RedirectChannelRegistrar")
{
}

NS_IMETHODIMP
RedirectChannelRegistrar::RegisterChannel(nsIChannel *channel,
                                          uint32_t *_retval)
{
  MutexAutoLock lock(mLock);

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
  MutexAutoLock lock(mLock);

  if (!mRealChannels.Get(id, _retval))
    return NS_ERROR_NOT_AVAILABLE;

  return NS_OK;
}

NS_IMETHODIMP
RedirectChannelRegistrar::LinkChannels(uint32_t id,
                                       nsIParentChannel *channel,
                                       nsIChannel** _retval)
{
  MutexAutoLock lock(mLock);

  if (!mRealChannels.Get(id, _retval))
    return NS_ERROR_NOT_AVAILABLE;

  mParentChannels.Put(id, channel);
  return NS_OK;
}

NS_IMETHODIMP
RedirectChannelRegistrar::GetParentChannel(uint32_t id,
                                           nsIParentChannel **_retval)
{
  MutexAutoLock lock(mLock);

  if (!mParentChannels.Get(id, _retval))
    return NS_ERROR_NOT_AVAILABLE;

  return NS_OK;
}

NS_IMETHODIMP
RedirectChannelRegistrar::DeregisterChannels(uint32_t id)
{
  MutexAutoLock lock(mLock);

  mRealChannels.Remove(id);
  mParentChannels.Remove(id);
  return NS_OK;
}

} // namespace net
} // namespace mozilla
