/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RedirectChannelRegistrar_h__
#define RedirectChannelRegistrar_h__

#include "nsIRedirectChannelRegistrar.h"

#include "nsIChannel.h"
#include "nsIParentChannel.h"
#include "nsInterfaceHashtable.h"
#include "mozilla/Attributes.h"
#include "mozilla/Mutex.h"

namespace mozilla {
namespace net {

class RedirectChannelRegistrar final : public nsIRedirectChannelRegistrar {
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREDIRECTCHANNELREGISTRAR

  RedirectChannelRegistrar();

 private:
  ~RedirectChannelRegistrar() = default;

 public:
  // Singleton accessor
  static already_AddRefed<nsIRedirectChannelRegistrar> GetOrCreate();

 protected:
  using ChannelHashtable = nsInterfaceHashtable<nsUint64HashKey, nsIChannel>;
  using ParentChannelHashtable =
      nsInterfaceHashtable<nsUint64HashKey, nsIParentChannel>;

  ChannelHashtable mRealChannels;
  ParentChannelHashtable mParentChannels;
  Mutex mLock MOZ_UNANNOTATED;

  static StaticRefPtr<RedirectChannelRegistrar> gSingleton;
};

}  // namespace net
}  // namespace mozilla

#endif
