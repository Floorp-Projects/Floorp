/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/BackgroundDataBridgeParent.h"
#include "mozilla/net/SocketProcessChild.h"
#include "mozilla/Unused.h"

namespace mozilla {
namespace net {

BackgroundDataBridgeParent::BackgroundDataBridgeParent(uint64_t aChannelID)
    : mChannelID(aChannelID), mBackgroundThread(NS_GetCurrentThread()) {
  if (SocketProcessChild* child = SocketProcessChild::GetSingleton()) {
    child->AddDataBridgeToMap(aChannelID, this);
  }
}

void BackgroundDataBridgeParent::ActorDestroy(ActorDestroyReason aWhy) {
  if (SocketProcessChild* child = SocketProcessChild::GetSingleton()) {
    child->RemoveDataBridgeFromMap(mChannelID);
  }
}

already_AddRefed<nsIThread> BackgroundDataBridgeParent::GetBackgroundThread() {
  nsCOMPtr<nsIThread> thread = mBackgroundThread;
  return thread.forget();
}

void BackgroundDataBridgeParent::Destroy() {
  RefPtr<BackgroundDataBridgeParent> self = this;
  MOZ_ALWAYS_SUCCEEDS(mBackgroundThread->Dispatch(
      NS_NewRunnableFunction("BackgroundDataBridgeParent::Destroy",
                             [self]() {
                               if (self->CanSend()) {
                                 Unused << self->Send__delete__(self);
                               }
                             }),
      NS_DISPATCH_NORMAL));
}

void BackgroundDataBridgeParent::OnStopRequest(
    nsresult aStatus, const ResourceTimingStructArgs& aTiming,
    const TimeStamp& aLastActiveTabOptHit,
    const nsHttpHeaderArray& aResponseTrailers) {
  RefPtr<BackgroundDataBridgeParent> self = this;
  MOZ_ALWAYS_SUCCEEDS(mBackgroundThread->Dispatch(
      NS_NewRunnableFunction(
          "BackgroundDataBridgeParent::OnStopRequest",
          [self, aStatus, aTiming, aLastActiveTabOptHit, aResponseTrailers]() {
            if (self->CanSend()) {
              Unused << self->SendOnStopRequest(
                  aStatus, aTiming, aLastActiveTabOptHit, aResponseTrailers);
              Unused << self->Send__delete__(self);
            }
          }),
      NS_DISPATCH_NORMAL));
}

}  // namespace net
}  // namespace mozilla
