/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ThreadResponsiveness_h
#define ThreadResponsiveness_h

#include "nsISupports.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TimeStamp.h"

class CheckResponsivenessTask;

class ThreadResponsiveness {
public:
  explicit ThreadResponsiveness();

  ~ThreadResponsiveness();

  void Update(bool aIsMainThread, nsIThread* aThread);

  mozilla::TimeDuration GetUnresponsiveDuration(const mozilla::TimeStamp& now) const {
    return now - mLastTracerTime;
  }

  bool HasData() const {
    return !mLastTracerTime.IsNull();
  }
private:
  RefPtr<CheckResponsivenessTask> mActiveTracerEvent;
  mozilla::TimeStamp mLastTracerTime;
};

#endif

