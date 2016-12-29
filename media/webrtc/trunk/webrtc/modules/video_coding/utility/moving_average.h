/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_UTILITY_MOVING_AVERAGE_H_
#define WEBRTC_MODULES_VIDEO_CODING_UTILITY_MOVING_AVERAGE_H_

#include <list>

#include "webrtc/typedefs.h"

namespace webrtc {
template <class T>
class MovingAverage {
 public:
  MovingAverage();
  void AddSample(T sample);
  bool GetAverage(size_t num_samples, T* average);
  void Reset();
  int size();

 private:
  T sum_;
  std::list<T> samples_;
};

template <class T>
MovingAverage<T>::MovingAverage()
    : sum_(static_cast<T>(0)) {}

template <class T>
void MovingAverage<T>::AddSample(T sample) {
  samples_.push_back(sample);
  sum_ += sample;
}

template <class T>
bool MovingAverage<T>::GetAverage(size_t num_samples, T* avg) {
  if (num_samples > samples_.size())
    return false;

  // Remove old samples.
  while (num_samples < samples_.size()) {
    sum_ -= samples_.front();
    samples_.pop_front();
  }

  *avg = sum_ / static_cast<T>(num_samples);
  return true;
}

template <class T>
void MovingAverage<T>::Reset() {
  sum_ = static_cast<T>(0);
  samples_.clear();
}

template <class T>
int MovingAverage<T>::size() {
  return samples_.size();
}

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_UTILITY_MOVING_AVERAGE_H_
