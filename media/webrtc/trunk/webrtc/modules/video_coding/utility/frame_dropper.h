/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_UTILITY_FRAME_DROPPER_H_
#define WEBRTC_MODULES_VIDEO_CODING_UTILITY_FRAME_DROPPER_H_

#include <cstddef>

#include "webrtc/base/exp_filter.h"
#include "webrtc/typedefs.h"

namespace webrtc {

// The Frame Dropper implements a variant of the leaky bucket algorithm
// for keeping track of when to drop frames to avoid bit rate
// over use when the encoder can't keep its bit rate.
class FrameDropper {
 public:
  FrameDropper();
  explicit FrameDropper(float max_time_drops);
  virtual ~FrameDropper() {}

  // Resets the FrameDropper to its initial state.
  // This means that the frameRateWeight is set to its
  // default value as well.
  virtual void Reset();

  virtual void Enable(bool enable);
  // Answers the question if it's time to drop a frame
  // if we want to reach a given frame rate. Must be
  // called for every frame.
  //
  // Return value     : True if we should drop the current frame
  virtual bool DropFrame();
  // Updates the FrameDropper with the size of the latest encoded
  // frame. The FrameDropper calculates a new drop ratio (can be
  // seen as the probability to drop a frame) and updates its
  // internal statistics.
  //
  // Input:
  //          - frameSizeBytes    : The size of the latest frame
  //                                returned from the encoder.
  //          - deltaFrame        : True if the encoder returned
  //                                a key frame.
  virtual void Fill(size_t frameSizeBytes, bool deltaFrame);

  virtual void Leak(uint32_t inputFrameRate);

  void UpdateNack(uint32_t nackBytes);

  // Sets the target bit rate and the frame rate produced by
  // the camera.
  //
  // Input:
  //          - bitRate       : The target bit rate
  virtual void SetRates(float bitRate, float incoming_frame_rate);

  // Return value     : The current average frame rate produced
  //                    if the DropFrame() function is used as
  //                    instruction of when to drop frames.
  virtual float ActualFrameRate(uint32_t inputFrameRate) const;

 private:
  void FillBucket(float inKbits, float outKbits);
  void UpdateRatio();
  void CapAccumulator();

  rtc::ExpFilter _keyFrameSizeAvgKbits;
  rtc::ExpFilter _keyFrameRatio;
  float _keyFrameSpreadFrames;
  int32_t _keyFrameCount;
  float _accumulator;
  float _accumulatorMax;
  float _targetBitRate;
  bool _dropNext;
  rtc::ExpFilter _dropRatio;
  int32_t _dropCount;
  float _windowSize;
  float _incoming_frame_rate;
  bool _wasBelowMax;
  bool _enabled;
  bool _fastMode;
  float _cap_buffer_size;
  float _max_time_drops;
};  // end of VCMFrameDropper class

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_UTILITY_FRAME_DROPPER_H_
