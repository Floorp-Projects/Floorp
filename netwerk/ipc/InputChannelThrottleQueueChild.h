/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef InputChannelThrottleQueueChild_h__
#define InputChannelThrottleQueueChild_h__

#include "mozilla/net/PInputChannelThrottleQueueChild.h"
#include "mozilla/net/ThrottleQueue.h"
#include "nsISupportsImpl.h"

namespace mozilla {
namespace net {

class InputChannelThrottleQueueChild final
    : public PInputChannelThrottleQueueChild,
      public ThrottleQueue {
 public:
  friend class PInputChannelThrottleQueueChild;
  NS_DECL_ISUPPORTS_INHERITED

  explicit InputChannelThrottleQueueChild() = default;
  NS_IMETHOD RecordRead(uint32_t aBytesRead) override;

 private:
  virtual ~InputChannelThrottleQueueChild() = default;
};

}  // namespace net
}  // namespace mozilla

#endif  // InputChannelThrottleQueueChild_h__
