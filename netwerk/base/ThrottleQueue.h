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

class ThrottleQueue final
  : public nsIInputChannelThrottleQueue
  , public nsITimerCallback
  , public nsINamed
{
public:

  ThrottleQueue();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTCHANNELTHROTTLEQUEUE
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSINAMED

  void QueueStream(ThrottleInputStream* aStream);
  void DequeueStream(ThrottleInputStream* aStream);

private:

  ~ThrottleQueue();

  struct ThrottleEntry {
    TimeStamp mTime;
    uint32_t mBytesRead;
  };

  nsTArray<ThrottleEntry> mReadEvents;
  uint32_t mMeanBytesPerSecond;
  uint32_t mMaxBytesPerSecond;
  uint64_t mBytesProcessed;

  nsTArray<RefPtr<ThrottleInputStream>> mAsyncEvents;
  nsCOMPtr<nsITimer> mTimer;
  bool mTimerArmed;
};

}
}

#endif //  mozilla_net_ThrottleQueue_h
