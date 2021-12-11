/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(InputEventStatistics_h_)
#  define InputEventStatistics_h_

#  include "mozilla/TimeStamp.h"
#  include "mozilla/UniquePtr.h"
#  include "nsTArray.h"

namespace mozilla {

class InputEventStatistics {
  // The default amount of time (milliseconds) required for handling a input
  // event.
  static const uint16_t sDefaultInputDuration = 1;

  // The number of processed input events we use to predict the amount of time
  // required to process the following input events.
  static const uint16_t sInputCountForPrediction = 9;

  // The default maximum and minimum time (milliseconds) we reserve for handling
  // input events in each frame.
  static const uint16_t sMaxReservedTimeForHandlingInput = 8;
  static const uint16_t sMinReservedTimeForHandlingInput = 1;

  class TimeDurationCircularBuffer {
    int16_t mSize;
    int16_t mCurrentIndex;
    nsTArray<TimeDuration> mBuffer;
    TimeDuration mTotal;

   public:
    TimeDurationCircularBuffer(uint32_t aSize, TimeDuration& aDefaultValue)
        : mSize(aSize), mCurrentIndex(0) {
      mSize = mSize == 0 ? sInputCountForPrediction : mSize;
      for (int16_t index = 0; index < mSize; ++index) {
        mBuffer.AppendElement(aDefaultValue);
        mTotal += aDefaultValue;
      }
    }

    void Insert(TimeDuration& aDuration) {
      mTotal += (aDuration - mBuffer[mCurrentIndex]);
      mBuffer[mCurrentIndex++] = aDuration;
      if (mCurrentIndex == mSize) {
        mCurrentIndex = 0;
      }
    }

    TimeDuration GetMean();
  };

  UniquePtr<TimeDurationCircularBuffer> mLastInputDurations;
  TimeDuration mMaxInputDuration;
  TimeDuration mMinInputDuration;
  bool mEnable;

  // We'd like to have our constructor and destructor be private to enforce our
  // singleton, but because UniquePtr needs to be able to destruct our class we
  // can't. This is a trick that ensures that we're the only code that can
  // construct ourselves: nobody else can access ConstructorCookie and therefore
  // nobody else can construct an InputEventStatistics.
  struct ConstructorCookie {};

 public:
  explicit InputEventStatistics(ConstructorCookie&&);
  ~InputEventStatistics() = default;

  static InputEventStatistics& Get();

  void UpdateInputDuration(TimeDuration aDuration) {
    if (!mEnable) {
      return;
    }
    mLastInputDurations->Insert(aDuration);
  }

  TimeStamp GetInputHandlingStartTime(uint32_t aInputCount);
  TimeDuration GetMaxInputHandlingDuration() const;

  void SetEnable(bool aEnable) { mEnable = aEnable; }
};

class MOZ_RAII AutoTimeDurationHelper final {
 public:
  AutoTimeDurationHelper() { mStartTime = TimeStamp::Now(); }

  ~AutoTimeDurationHelper() {
    InputEventStatistics::Get().UpdateInputDuration(TimeStamp::Now() -
                                                    mStartTime);
  }

 private:
  TimeStamp mStartTime;
};

}  // namespace mozilla

#endif  // InputEventStatistics_h_
