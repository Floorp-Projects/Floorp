/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_BackgroundDataBridgeParent_h
#define mozilla_net_BackgroundDataBridgeParent_h

#include "mozilla/net/PBackgroundDataBridgeParent.h"

namespace mozilla {
namespace net {

class BackgroundDataBridgeParent final : public PBackgroundDataBridgeParent {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(BackgroundDataBridgeParent, override)

  explicit BackgroundDataBridgeParent(uint64_t aChannelID);
  void ActorDestroy(ActorDestroyReason aWhy) override;
  already_AddRefed<nsIThread> GetBackgroundThread();
  void Destroy();

 private:
  virtual ~BackgroundDataBridgeParent() = default;

  uint64_t mChannelID;
  nsCOMPtr<nsIThread> mBackgroundThread;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_BackgroundDataBridgeParent_h
