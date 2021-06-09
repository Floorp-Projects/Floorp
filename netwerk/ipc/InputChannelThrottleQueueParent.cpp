/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InputChannelThrottleQueueParent.h"
#include "mozilla/net/SocketProcessParent.h"
#include "nsIOService.h"

namespace mozilla {
namespace net {

NS_IMPL_ADDREF(InputChannelThrottleQueueParent)
NS_INTERFACE_MAP_BEGIN(InputChannelThrottleQueueParent)
  NS_INTERFACE_MAP_ENTRY(nsIInputChannelThrottleQueue)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY_CONCRETE(InputChannelThrottleQueueParent)
NS_INTERFACE_MAP_END

NS_IMETHODIMP_(MozExternalRefCountType)
InputChannelThrottleQueueParent::Release(void) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(int32_t(mRefCnt) > 0, "dup release");

  if (!nsAutoRefCnt::isThreadSafe) {
    NS_ASSERT_OWNINGTHREAD(InputChannelThrottleQueueParent);
  }

  nsrefcnt count = --mRefCnt;
  NS_LOG_RELEASE(this, count, "InputChannelThrottleQueueParent");

  if (count == 0) {
    if (!nsAutoRefCnt::isThreadSafe) {
      NS_ASSERT_OWNINGTHREAD(InputChannelThrottleQueueParent);
    }

    mRefCnt = 1; /* stabilize */
    delete (this);
    return 0;
  }

  // When ref count goes down to 1 (held internally by IPDL), it means that
  // we are done with this ThrottleQueue. We should send a delete message
  // to delete the InputChannelThrottleQueueChild in socket process.
  if (count == 1 && CanSend()) {
    mozilla::Unused << Send__delete__(this);
    return 1;
  }
  return count;
}

mozilla::ipc::IPCResult InputChannelThrottleQueueParent::RecvRecordRead(
    const uint32_t& aBytesRead) {
  mBytesProcessed += aBytesRead;
  return IPC_OK();
}

NS_IMETHODIMP
InputChannelThrottleQueueParent::RecordRead(uint32_t aBytesRead) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
InputChannelThrottleQueueParent::Available(uint32_t aRemaining,
                                           uint32_t* aAvailable) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
InputChannelThrottleQueueParent::Init(uint32_t aMeanBytesPerSecond,
                                      uint32_t aMaxBytesPerSecond) {
  // Can be called on any thread.
  if (aMeanBytesPerSecond == 0 || aMaxBytesPerSecond == 0 ||
      aMaxBytesPerSecond < aMeanBytesPerSecond) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  mMeanBytesPerSecond = aMeanBytesPerSecond;
  mMaxBytesPerSecond = aMaxBytesPerSecond;

  RefPtr<InputChannelThrottleQueueParent> self = this;
  gIOService->CallOrWaitForSocketProcess(
      [self, meanBytesPerSecond(mMeanBytesPerSecond),
       maxBytesPerSecond(mMaxBytesPerSecond)] {
        Unused << SocketProcessParent::GetSingleton()
                      ->SendPInputChannelThrottleQueueConstructor(
                          self, meanBytesPerSecond, maxBytesPerSecond);
      });

  return NS_OK;
}

NS_IMETHODIMP
InputChannelThrottleQueueParent::BytesProcessed(uint64_t* aResult) {
  *aResult = mBytesProcessed;
  return NS_OK;
}

NS_IMETHODIMP
InputChannelThrottleQueueParent::WrapStream(nsIInputStream* aInputStream,
                                            nsIAsyncInputStream** aResult) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
InputChannelThrottleQueueParent::GetMeanBytesPerSecond(
    uint32_t* aMeanBytesPerSecond) {
  NS_ENSURE_ARG(aMeanBytesPerSecond);

  *aMeanBytesPerSecond = mMeanBytesPerSecond;
  return NS_OK;
}

NS_IMETHODIMP
InputChannelThrottleQueueParent::GetMaxBytesPerSecond(
    uint32_t* aMaxBytesPerSecond) {
  NS_ENSURE_ARG(aMaxBytesPerSecond);

  *aMaxBytesPerSecond = mMaxBytesPerSecond;
  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
