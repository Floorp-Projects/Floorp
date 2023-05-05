/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RedirectChannelRegistrar.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticPtr.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace net {

StaticRefPtr<RedirectChannelRegistrar> RedirectChannelRegistrar::gSingleton;

NS_IMPL_ISUPPORTS(RedirectChannelRegistrar, nsIRedirectChannelRegistrar)

RedirectChannelRegistrar::RedirectChannelRegistrar()
    : mRealChannels(32),
      mParentChannels(32),
      mLock("RedirectChannelRegistrar") {
  MOZ_ASSERT(!gSingleton);
}

// static
already_AddRefed<nsIRedirectChannelRegistrar>
RedirectChannelRegistrar::GetOrCreate() {
  MOZ_ASSERT(NS_IsMainThread());
  if (!gSingleton) {
    gSingleton = new RedirectChannelRegistrar();
    ClearOnShutdown(&gSingleton);
  }
  return do_AddRef(gSingleton);
}

NS_IMETHODIMP
RedirectChannelRegistrar::RegisterChannel(nsIChannel* channel, uint64_t id) {
  MutexAutoLock lock(mLock);

  mRealChannels.InsertOrUpdate(id, channel);

  return NS_OK;
}

NS_IMETHODIMP
RedirectChannelRegistrar::GetRegisteredChannel(uint64_t id,
                                               nsIChannel** _retval) {
  MutexAutoLock lock(mLock);

  if (!mRealChannels.Get(id, _retval)) return NS_ERROR_NOT_AVAILABLE;

  return NS_OK;
}

NS_IMETHODIMP
RedirectChannelRegistrar::LinkChannels(uint64_t id, nsIParentChannel* channel,
                                       nsIChannel** _retval) {
  MutexAutoLock lock(mLock);

  if (!mRealChannels.Get(id, _retval)) return NS_ERROR_NOT_AVAILABLE;

  mParentChannels.InsertOrUpdate(id, channel);
  return NS_OK;
}

NS_IMETHODIMP
RedirectChannelRegistrar::GetParentChannel(uint64_t id,
                                           nsIParentChannel** _retval) {
  MutexAutoLock lock(mLock);

  if (!mParentChannels.Get(id, _retval)) return NS_ERROR_NOT_AVAILABLE;

  return NS_OK;
}

NS_IMETHODIMP
RedirectChannelRegistrar::DeregisterChannels(uint64_t id) {
  MutexAutoLock lock(mLock);

  mRealChannels.Remove(id);
  mParentChannels.Remove(id);
  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
