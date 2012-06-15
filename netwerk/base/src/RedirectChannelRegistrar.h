/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RedirectChannelRegistrar_h__
#define RedirectChannelRegistrar_h__

#include "nsIRedirectChannelRegistrar.h"

#include "nsIChannel.h"
#include "nsIParentChannel.h"
#include "nsClassHashtable.h"
#include "mozilla/Attributes.h"

namespace mozilla {
namespace net {

class RedirectChannelRegistrar MOZ_FINAL : public nsIRedirectChannelRegistrar
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREDIRECTCHANNELREGISTRAR

  RedirectChannelRegistrar();

protected:
  template<class KeyClass, class T>
  class nsCOMPtrHashtable :
    public nsBaseHashtable< KeyClass, nsCOMPtr<T>, T* >
  {
  public:
    typedef typename KeyClass::KeyType KeyType;
    typedef T* UserDataType;
    typedef nsBaseHashtable< KeyClass, nsCOMPtr<T>, T* > base_type;

    bool Get(KeyType aKey, UserDataType* pData) const;
  };

  typedef nsCOMPtrHashtable<nsUint32HashKey, nsIChannel>
          ChannelHashtable;
  typedef nsCOMPtrHashtable<nsUint32HashKey, nsIParentChannel>
          ParentChannelHashtable;

  ChannelHashtable mRealChannels;
  ParentChannelHashtable mParentChannels;
  PRUint32 mId;
};

}
}

#endif
