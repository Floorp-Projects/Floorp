/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef InputChannelThrottleQueueParent_h__
#define InputChannelThrottleQueueParent_h__

#include "nsISupportsImpl.h"
#include "nsIThrottledInputChannel.h"
#include "mozilla/net/PInputChannelThrottleQueueParent.h"

namespace mozilla {
namespace net {

#define INPUT_CHANNEL_THROTTLE_QUEUE_PARENT_IID      \
  {                                                  \
    0x4f151655, 0x70b3, 0x4350, {                    \
      0x9b, 0xd9, 0xe3, 0x2b, 0xe5, 0xeb, 0xb2, 0x9e \
    }                                                \
  }

class InputChannelThrottleQueueParent final
    : public PInputChannelThrottleQueueParent,
      public nsIInputChannelThrottleQueue {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIINPUTCHANNELTHROTTLEQUEUE
  NS_DECLARE_STATIC_IID_ACCESSOR(INPUT_CHANNEL_THROTTLE_QUEUE_PARENT_IID)

  friend class PInputChannelThrottleQueueParent;

  explicit InputChannelThrottleQueueParent() = default;
  mozilla::ipc::IPCResult RecvRecordRead(const uint32_t& aBytesRead);
  void ActorDestroy(ActorDestroyReason aWhy) override {}

 private:
  virtual ~InputChannelThrottleQueueParent() = default;

  uint64_t mBytesProcessed{0};
  uint32_t mMeanBytesPerSecond{0};
  uint32_t mMaxBytesPerSecond{0};
};

NS_DEFINE_STATIC_IID_ACCESSOR(InputChannelThrottleQueueParent,
                              INPUT_CHANNEL_THROTTLE_QUEUE_PARENT_IID)

}  // namespace net
}  // namespace mozilla

#endif  // InputChannelThrottleQueueParent_h__
