/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_ThrottleQueue_h
#define mozilla_net_ThrottleQueue_h

#include "mozilla/TimeStamp.h"
#include "nsINamed.h"
#include "nsIThrottledInputChannel.h"
#include "nsITimer.h"
#include "nsTArray.h"

namespace mozilla {
namespace net {

class ThrottleInputStream;

/**
 * An implementation of nsIInputChannelThrottleQueue that can be used
 * to throttle uploads.  This class is not thread-safe.
 * Initialization and calls to WrapStream may be done on any thread;
 * but otherwise, after creation, it can only be used on the socket
 * thread.  It currently throttles with a one second granularity, so
 * may be a bit choppy.
 */

class ThrottleQueue : public nsIInputChannelThrottleQueue,
                      public nsITimerCallback,
                      public nsINamed {
 public:
  static already_AddRefed<nsIInputChannelThrottleQueue> Create();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTCHANNELTHROTTLEQUEUE
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSINAMED

  void QueueStream(ThrottleInputStream* aStream);
  void DequeueStream(ThrottleInputStream* aStream);

 protected:
  ThrottleQueue();
  virtual ~ThrottleQueue();

  struct ThrottleEntry {
    TimeStamp mTime;
    uint32_t mBytesRead = 0;
  };

  nsTArray<ThrottleEntry> mReadEvents;
  uint32_t mMeanBytesPerSecond{0};
  uint32_t mMaxBytesPerSecond{0};
  uint64_t mBytesProcessed{0};

  nsTArray<RefPtr<ThrottleInputStream>> mAsyncEvents;
  nsCOMPtr<nsITimer> mTimer;
  bool mTimerArmed{false};
};

}  // namespace net
}  // namespace mozilla

#endif  //  mozilla_net_ThrottleQueue_h
