/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ThreadResponsiveness_h
#define ThreadResponsiveness_h

#include "nsISupports.h"
#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TimeStamp.h"

class CheckResponsivenessTask;

// This class should only be used for the main thread.
class ThreadResponsiveness {
public:
  explicit ThreadResponsiveness();

  ~ThreadResponsiveness();

  void Update();

  // The number of milliseconds that elapsed since the last
  // CheckResponsivenessTask was processed.
  double GetUnresponsiveDuration(double aStartToNow_ms) const {
    return aStartToNow_ms - *mStartToPrevTracer_ms;
  }

  bool HasData() const {
    return mStartToPrevTracer_ms.isSome();
  }
private:
  RefPtr<CheckResponsivenessTask> mActiveTracerEvent;
  // The time at which the last CheckResponsivenessTask was processed, in
  // milliseconds since process start (i.e. what you get from profiler_time()).
  mozilla::Maybe<double> mStartToPrevTracer_ms;
};

#endif

