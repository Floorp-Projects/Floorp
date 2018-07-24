/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ThreadEventTarget_h
#define mozilla_ThreadEventTarget_h

#include "mozilla/MemoryReporting.h"
#include "mozilla/Mutex.h"
#include "mozilla/SynchronizedEventQueue.h" // for ThreadTargetSink
#include "nsISerialEventTarget.h"

namespace mozilla {

// ThreadEventTarget handles the details of posting an event to a thread. It can
// be used with any ThreadTargetSink implementation.
class ThreadEventTarget final : public nsISerialEventTarget
{
public:
  ThreadEventTarget(ThreadTargetSink* aSink,
                    bool aIsMainThread);

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIEVENTTARGET_FULL

  // Disconnects the target so that it can no longer post events.
  void Disconnect(const MutexAutoLock& aProofOfLock) { mSink->Disconnect(aProofOfLock); }

  // Sets the thread for which IsOnCurrentThread returns true to the current thread.
  void SetCurrentThread();

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
  {
    size_t n = 0;
    if (mSink) {
      n += mSink->SizeOfIncludingThis(aMallocSizeOf);
    }
    return aMallocSizeOf(this) + n;
  }

private:
  ~ThreadEventTarget() {}

  RefPtr<ThreadTargetSink> mSink;
  bool mIsMainThread;
};

} // namespace mozilla

#endif // mozilla_ThreadEventTarget_h
