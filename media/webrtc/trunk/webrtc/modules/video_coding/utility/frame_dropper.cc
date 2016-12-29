/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/utility/frame_dropper.h"

#include "webrtc/system_wrappers/include/trace.h"

namespace webrtc {

const float kDefaultKeyFrameSizeAvgKBits = 0.9f;
const float kDefaultKeyFrameRatio = 0.99f;
const float kDefaultDropRatioAlpha = 0.9f;
const float kDefaultDropRatioMax = 0.96f;
const float kDefaultMaxTimeToDropFrames = 4.0f;  // In seconds.

FrameDropper::FrameDropper()
    : _keyFrameSizeAvgKbits(kDefaultKeyFrameSizeAvgKBits),
      _keyFrameRatio(kDefaultKeyFrameRatio),
      _dropRatio(kDefaultDropRatioAlpha, kDefaultDropRatioMax),
      _enabled(true),
      _max_time_drops(kDefaultMaxTimeToDropFrames) {
  Reset();
}

FrameDropper::FrameDropper(float max_time_drops)
    : _keyFrameSizeAvgKbits(kDefaultKeyFrameSizeAvgKBits),
      _keyFrameRatio(kDefaultKeyFrameRatio),
      _dropRatio(kDefaultDropRatioAlpha, kDefaultDropRatioMax),
      _enabled(true),
      _max_time_drops(max_time_drops) {
  Reset();
}

void FrameDropper::Reset() {
  _keyFrameRatio.Reset(0.99f);
  _keyFrameRatio.Apply(
      1.0f, 1.0f / 300.0f);  // 1 key frame every 10th second in 30 fps
  _keyFrameSizeAvgKbits.Reset(0.9f);
  _keyFrameCount = 0;
  _accumulator = 0.0f;
  _accumulatorMax = 150.0f;  // assume 300 kb/s and 0.5 s window
  _targetBitRate = 300.0f;
  _incoming_frame_rate = 30;
  _keyFrameSpreadFrames = 0.5f * _incoming_frame_rate;
  _dropNext = false;
  _dropRatio.Reset(0.9f);
  _dropRatio.Apply(0.0f, 0.0f);  // Initialize to 0
  _dropCount = 0;
  _windowSize = 0.5f;
  _wasBelowMax = true;
  _fastMode = false;  // start with normal (non-aggressive) mode
  // Cap for the encoder buffer level/accumulator, in secs.
  _cap_buffer_size = 3.0f;
  // Cap on maximum amount of dropped frames between kept frames, in secs.
  _max_time_drops = 4.0f;
}

void FrameDropper::Enable(bool enable) {
  _enabled = enable;
}

void FrameDropper::Fill(size_t frameSizeBytes, bool deltaFrame) {
  if (!_enabled) {
    return;
  }
  float frameSizeKbits = 8.0f * static_cast<float>(frameSizeBytes) / 1000.0f;
  if (!deltaFrame &&
      !_fastMode) {  // fast mode does not treat key-frames any different
    _keyFrameSizeAvgKbits.Apply(1, frameSizeKbits);
    _keyFrameRatio.Apply(1.0, 1.0);
    if (frameSizeKbits > _keyFrameSizeAvgKbits.filtered()) {
      // Remove the average key frame size since we
      // compensate for key frames when adding delta
      // frames.
      frameSizeKbits -= _keyFrameSizeAvgKbits.filtered();
    } else {
      // Shouldn't be negative, so zero is the lower bound.
      frameSizeKbits = 0;
    }
    if (_keyFrameRatio.filtered() > 1e-5 &&
        1 / _keyFrameRatio.filtered() < _keyFrameSpreadFrames) {
      // We are sending key frames more often than our upper bound for
      // how much we allow the key frame compensation to be spread
      // out in time. Therefor we must use the key frame ratio rather
      // than keyFrameSpreadFrames.
      _keyFrameCount =
          static_cast<int32_t>(1 / _keyFrameRatio.filtered() + 0.5);
    } else {
      // Compensate for the key frame the following frames
      _keyFrameCount = static_cast<int32_t>(_keyFrameSpreadFrames + 0.5);
    }
  } else {
    // Decrease the keyFrameRatio
    _keyFrameRatio.Apply(1.0, 0.0);
  }
  // Change the level of the accumulator (bucket)
  _accumulator += frameSizeKbits;
  CapAccumulator();
}

void FrameDropper::Leak(uint32_t inputFrameRate) {
  if (!_enabled) {
    return;
  }
  if (inputFrameRate < 1) {
    return;
  }
  if (_targetBitRate < 0.0f) {
    return;
  }
  _keyFrameSpreadFrames = 0.5f * inputFrameRate;
  // T is the expected bits per frame (target). If all frames were the same
  // size,
  // we would get T bits per frame. Notice that T is also weighted to be able to
  // force a lower frame rate if wanted.
  float T = _targetBitRate / inputFrameRate;
  if (_keyFrameCount > 0) {
    // Perform the key frame compensation
    if (_keyFrameRatio.filtered() > 0 &&
        1 / _keyFrameRatio.filtered() < _keyFrameSpreadFrames) {
      T -= _keyFrameSizeAvgKbits.filtered() * _keyFrameRatio.filtered();
    } else {
      T -= _keyFrameSizeAvgKbits.filtered() / _keyFrameSpreadFrames;
    }
    _keyFrameCount--;
  }
  _accumulator -= T;
  if (_accumulator < 0.0f) {
    _accumulator = 0.0f;
  }
  UpdateRatio();
}

void FrameDropper::UpdateNack(uint32_t nackBytes) {
  if (!_enabled) {
    return;
  }
  _accumulator += static_cast<float>(nackBytes) * 8.0f / 1000.0f;
}

void FrameDropper::FillBucket(float inKbits, float outKbits) {
  _accumulator += (inKbits - outKbits);
}

void FrameDropper::UpdateRatio() {
  if (_accumulator > 1.3f * _accumulatorMax) {
    // Too far above accumulator max, react faster
    _dropRatio.UpdateBase(0.8f);
  } else {
    // Go back to normal reaction
    _dropRatio.UpdateBase(0.9f);
  }
  if (_accumulator > _accumulatorMax) {
    // We are above accumulator max, and should ideally
    // drop a frame. Increase the dropRatio and drop
    // the frame later.
    if (_wasBelowMax) {
      _dropNext = true;
    }
    if (_fastMode) {
      // always drop in aggressive mode
      _dropNext = true;
    }

    _dropRatio.Apply(1.0f, 1.0f);
    _dropRatio.UpdateBase(0.9f);
  } else {
    _dropRatio.Apply(1.0f, 0.0f);
  }
  _wasBelowMax = _accumulator < _accumulatorMax;
}

// This function signals when to drop frames to the caller. It makes use of the
// dropRatio
// to smooth out the drops over time.
bool FrameDropper::DropFrame() {
  if (!_enabled) {
    return false;
  }
  if (_dropNext) {
    _dropNext = false;
    _dropCount = 0;
  }

  if (_dropRatio.filtered() >= 0.5f) {  // Drops per keep
    // limit is the number of frames we should drop between each kept frame
    // to keep our drop ratio. limit is positive in this case.
    float denom = 1.0f - _dropRatio.filtered();
    if (denom < 1e-5) {
      denom = 1e-5f;
    }
    int32_t limit = static_cast<int32_t>(1.0f / denom - 1.0f + 0.5f);
    // Put a bound on the max amount of dropped frames between each kept
    // frame, in terms of frame rate and window size (secs).
    int max_limit = static_cast<int>(_incoming_frame_rate * _max_time_drops);
    if (limit > max_limit) {
      limit = max_limit;
    }
    if (_dropCount < 0) {
      // Reset the _dropCount since it was negative and should be positive.
      if (_dropRatio.filtered() > 0.4f) {
        _dropCount = -_dropCount;
      } else {
        _dropCount = 0;
      }
    }
    if (_dropCount < limit) {
      // As long we are below the limit we should drop frames.
      _dropCount++;
      return true;
    } else {
      // Only when we reset _dropCount a frame should be kept.
      _dropCount = 0;
      return false;
    }
  } else if (_dropRatio.filtered() > 0.0f &&
             _dropRatio.filtered() < 0.5f) {  // Keeps per drop
    // limit is the number of frames we should keep between each drop
    // in order to keep the drop ratio. limit is negative in this case,
    // and the _dropCount is also negative.
    float denom = _dropRatio.filtered();
    if (denom < 1e-5) {
      denom = 1e-5f;
    }
    int32_t limit = -static_cast<int32_t>(1.0f / denom - 1.0f + 0.5f);
    if (_dropCount > 0) {
      // Reset the _dropCount since we have a positive
      // _dropCount, and it should be negative.
      if (_dropRatio.filtered() < 0.6f) {
        _dropCount = -_dropCount;
      } else {
        _dropCount = 0;
      }
    }
    if (_dropCount > limit) {
      if (_dropCount == 0) {
        // Drop frames when we reset _dropCount.
        _dropCount--;
        return true;
      } else {
        // Keep frames as long as we haven't reached limit.
        _dropCount--;
        return false;
      }
    } else {
      _dropCount = 0;
      return false;
    }
  }
  _dropCount = 0;
  return false;

  // A simpler version, unfiltered and quicker
  // bool dropNext = _dropNext;
  // _dropNext = false;
  // return dropNext;
}

void FrameDropper::SetRates(float bitRate, float incoming_frame_rate) {
  // Bit rate of -1 means infinite bandwidth.
  _accumulatorMax = bitRate * _windowSize;  // bitRate * windowSize (in seconds)
  if (_targetBitRate > 0.0f && bitRate < _targetBitRate &&
      _accumulator > _accumulatorMax) {
    // Rescale the accumulator level if the accumulator max decreases
    _accumulator = bitRate / _targetBitRate * _accumulator;
  }
  _targetBitRate = bitRate;
  CapAccumulator();
  _incoming_frame_rate = incoming_frame_rate;
}

float FrameDropper::ActualFrameRate(uint32_t inputFrameRate) const {
  if (!_enabled) {
    return static_cast<float>(inputFrameRate);
  }
  return inputFrameRate * (1.0f - _dropRatio.filtered());
}

// Put a cap on the accumulator, i.e., don't let it grow beyond some level.
// This is a temporary fix for screencasting where very large frames from
// encoder will cause very slow response (too many frame drops).
void FrameDropper::CapAccumulator() {
  float max_accumulator = _targetBitRate * _cap_buffer_size;
  if (_accumulator > max_accumulator) {
    _accumulator = max_accumulator;
  }
}
}  // namespace webrtc
