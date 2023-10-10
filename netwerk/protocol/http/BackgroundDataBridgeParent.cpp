/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/BackgroundDataBridgeParent.h"
#include "mozilla/net/SocketProcessChild.h"
#include "mozilla/Unused.h"

namespace mozilla {
namespace net {

BackgroundDataBridgeParent::BackgroundDataBridgeParent(uint64_t aChannelID)
    : mChannelID(aChannelID), mBackgroundThread(GetCurrentSerialEventTarget()) {
  if (SocketProcessChild* child = SocketProcessChild::GetSingleton()) {
    child->AddDataBridgeToMap(aChannelID, this);
  }
}

void BackgroundDataBridgeParent::ActorDestroy(ActorDestroyReason aWhy) {
  if (SocketProcessChild* child = SocketProcessChild::GetSingleton()) {
    child->RemoveDataBridgeFromMap(mChannelID);
  }
}

already_AddRefed<nsISerialEventTarget>
BackgroundDataBridgeParent::GetBackgroundThread() {
  return do_AddRef(mBackgroundThread);
}

void BackgroundDataBridgeParent::Destroy() {
  RefPtr<BackgroundDataBridgeParent> self = this;
  MOZ_ALWAYS_SUCCEEDS(mBackgroundThread->Dispatch(
      NS_NewRunnableFunction("BackgroundDataBridgeParent::Destroy",
                             [self]() {
                               if (self->CanSend()) {
                                 self->Close();
                               }
                             }),
      NS_DISPATCH_NORMAL));
}

void BackgroundDataBridgeParent::OnStopRequest(
    nsresult aStatus, const ResourceTimingStructArgs& aTiming,
    const TimeStamp& aLastActiveTabOptHit,
    const nsHttpHeaderArray& aResponseTrailers,
    const TimeStamp& aOnStopRequestStart) {
  RefPtr<BackgroundDataBridgeParent> self = this;
  MOZ_ALWAYS_SUCCEEDS(mBackgroundThread->Dispatch(
      NS_NewRunnableFunction("BackgroundDataBridgeParent::OnStopRequest",
                             [self, aStatus, aTiming, aLastActiveTabOptHit,
                              aResponseTrailers, aOnStopRequestStart]() {
                               if (self->CanSend()) {
                                 Unused << self->SendOnStopRequest(
                                     aStatus, aTiming, aLastActiveTabOptHit,
                                     aResponseTrailers, aOnStopRequestStart);
                                 self->Close();
                               }
                             }),
      NS_DISPATCH_NORMAL));
}

}  // namespace net
}  // namespace mozilla
