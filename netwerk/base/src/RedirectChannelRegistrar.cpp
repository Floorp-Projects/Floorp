/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RedirectChannelRegistrar.h"

namespace mozilla {
namespace net {

template<class KeyClass, class T>
bool
RedirectChannelRegistrar::nsCOMPtrHashtable<KeyClass,T>::Get(KeyType aKey, T** retVal) const
{
  typename base_type::EntryType* ent = this->GetEntry(aKey);

  if (ent) {
    if (retVal)
      NS_IF_ADDREF(*retVal = ent->mData);

    return true;
  }

  if (retVal)
    *retVal = nsnull;

  return false;
}

NS_IMPL_ISUPPORTS1(RedirectChannelRegistrar, nsIRedirectChannelRegistrar)

RedirectChannelRegistrar::RedirectChannelRegistrar()
  : mId(1)
{
  mRealChannels.Init(64);
  mParentChannels.Init(64);
}

NS_IMETHODIMP
RedirectChannelRegistrar::RegisterChannel(nsIChannel *channel,
                                          PRUint32 *_retval)
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
RedirectChannelRegistrar::GetRegisteredChannel(PRUint32 id,
                                               nsIChannel **_retval)
{
  if (!mRealChannels.Get(id, _retval))
    return NS_ERROR_NOT_AVAILABLE;

  return NS_OK;
}

NS_IMETHODIMP
RedirectChannelRegistrar::LinkChannels(PRUint32 id,
                                       nsIParentChannel *channel,
                                       nsIChannel** _retval)
{
  if (!mRealChannels.Get(id, _retval))
    return NS_ERROR_NOT_AVAILABLE;

  mParentChannels.Put(id, channel);
  return NS_OK;
}

NS_IMETHODIMP
RedirectChannelRegistrar::GetParentChannel(PRUint32 id,
                                           nsIParentChannel **_retval)
{
  if (!mParentChannels.Get(id, _retval))
    return NS_ERROR_NOT_AVAILABLE;

  return NS_OK;
}

NS_IMETHODIMP
RedirectChannelRegistrar::DeregisterChannels(PRUint32 id)
{
  mRealChannels.Remove(id);
  mParentChannels.Remove(id);
  return NS_OK;
}

}
}
