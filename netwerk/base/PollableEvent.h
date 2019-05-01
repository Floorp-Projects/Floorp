/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PollableEvent_h__
#define PollableEvent_h__

#include "mozilla/Mutex.h"
#include "mozilla/TimeStamp.h"

namespace mozilla {
namespace net {

// class must be called locked
class PollableEvent {
 public:
  PollableEvent();
  ~PollableEvent();

  // Signal/Clear return false only if they fail
  bool Signal();
  // This is called only when we get non-null out_flags for the socket pair
  bool Clear();
  bool Valid() { return mWriteFD && mReadFD; }

  // We want to detect if writing to one of the socket pair sockets takes
  // too long to be received by the other socket from the pair.
  // Hence, we remember the timestamp of the earliest write by a call to
  // MarkFirstSignalTimestamp() from Signal().  After waking up from poll()
  // we check how long it took get the 'readable' signal on the socket pair.
  void MarkFirstSignalTimestamp();
  // Called right before we enter poll() to exclude any possible delay between
  // the earlist call to Signal() and entering poll() caused by processing
  // of events dispatched to the socket transport thread.
  void AdjustFirstSignalTimestamp();
  // This returns false on following conditions:
  // - PR_Write has failed
  // - no out_flags were signalled on the socket pair for too long after
  //   the earliest Signal()
  bool IsSignallingAlive(TimeDuration const& timeout);

  PRFileDesc* PollableFD() { return mReadFD; }

 private:
  PRFileDesc* mWriteFD;
  PRFileDesc* mReadFD;
  bool mSignaled;
  // true when PR_Write to the socket pair has failed (status < 1)
  bool mWriteFailed;
  // Set true after AdjustFirstSignalTimestamp() was called
  // Set false after Clear() was called
  // Ensures shifting the timestamp before entering poll() only once
  // between Clear()'ings.
  bool mSignalTimestampAdjusted;
  // Timestamp of the first call to Signal() (or time we enter poll())
  // that happened after the last Clear() call
  TimeStamp mFirstSignalAfterClear;
};

}  // namespace net
}  // namespace mozilla

#endif
