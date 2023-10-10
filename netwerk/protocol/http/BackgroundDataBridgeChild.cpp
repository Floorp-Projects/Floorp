/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/BackgroundDataBridgeChild.h"
#include "mozilla/net/HttpBackgroundChannelChild.h"

namespace mozilla {
namespace net {

BackgroundDataBridgeChild::BackgroundDataBridgeChild(
    HttpBackgroundChannelChild* aBgChild)
    : mBgChild(aBgChild) {
  MOZ_ASSERT(aBgChild);
}

BackgroundDataBridgeChild::~BackgroundDataBridgeChild() = default;

void BackgroundDataBridgeChild::ActorDestroy(ActorDestroyReason aWhy) {
  mBgChild = nullptr;
}

mozilla::ipc::IPCResult BackgroundDataBridgeChild::RecvOnTransportAndData(
    const uint64_t& offset, const uint32_t& count, const nsACString& data,
    const TimeStamp& aOnDataAvailableStartTime) {
  if (!mBgChild) {
    return IPC_OK();
  }

  if (mBgChild->ChannelClosed()) {
    Close();
    return IPC_OK();
  }

  return mBgChild->RecvOnTransportAndData(NS_OK, NS_NET_STATUS_RECEIVING_FROM,
                                          offset, count, data, true,
                                          aOnDataAvailableStartTime);
}

mozilla::ipc::IPCResult BackgroundDataBridgeChild::RecvOnStopRequest(
    nsresult aStatus, const ResourceTimingStructArgs& aTiming,
    const TimeStamp& aLastActiveTabOptHit,
    const nsHttpHeaderArray& aResponseTrailers,
    const TimeStamp& aOnStopRequestStartTime) {
  if (!mBgChild) {
    return IPC_OK();
  }

  if (mBgChild->ChannelClosed()) {
    Close();
    return IPC_OK();
  }

  return mBgChild->RecvOnStopRequest(
      aStatus, aTiming, aLastActiveTabOptHit, aResponseTrailers,
      nsTArray<ConsoleReportCollected>(), true, aOnStopRequestStartTime);
}

}  // namespace net
}  // namespace mozilla
