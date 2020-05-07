/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/BackgroundDataBridgeChild.h"
#include "mozilla/net/HttpBackgroundChannelChild.h"

namespace mozilla {
namespace net {

BackgroundDataBridgeChild::BackgroundDataBridgeChild(
    HttpBackgroundChannelChild* aBgChild)
    : mBgChild(aBgChild), mBackgroundThread(NS_GetCurrentThread()) {
  MOZ_ASSERT(aBgChild);
}

BackgroundDataBridgeChild::~BackgroundDataBridgeChild() = default;

void BackgroundDataBridgeChild::Destroy() {
  RefPtr<BackgroundDataBridgeChild> self = this;
  MOZ_ALWAYS_SUCCEEDS(mBackgroundThread->Dispatch(
      NS_NewRunnableFunction("BackgroundDataBridgeParent::Destroy",
                             [self]() {
                               if (self->CanSend()) {
                                 Unused << self->Send__delete__(self);
                               }
                             }),
      NS_DISPATCH_NORMAL));
}

mozilla::ipc::IPCResult BackgroundDataBridgeChild::RecvOnTransportAndData(
    const uint64_t& offset, const uint32_t& count, const nsCString& data) {
  MOZ_ASSERT(mBgChild);
  return mBgChild->RecvOnTransportAndData(NS_OK, NS_NET_STATUS_RECEIVING_FROM,
                                          offset, count, data, true);
}

}  // namespace net
}  // namespace mozilla
