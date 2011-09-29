/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Honza Bambas <honzab@firemni.cz>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

    return PR_TRUE;
  }

  if (retVal)
    *retVal = nsnull;

  return PR_FALSE;
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
                                          PRUint32 *_retval NS_OUTPARAM)
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
                                               nsIChannel **_retval NS_OUTPARAM)
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
                                           nsIParentChannel **_retval NS_OUTPARAM)
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
