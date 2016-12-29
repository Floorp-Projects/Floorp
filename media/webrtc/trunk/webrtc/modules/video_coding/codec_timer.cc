/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/codec_timer.h"

#include <assert.h>

namespace webrtc {

// The first kIgnoredSampleCount samples will be ignored.
static const int32_t kIgnoredSampleCount = 5;

VCMCodecTimer::VCMCodecTimer()
    : _filteredMax(0), _ignoredSampleCount(0), _shortMax(0), _history() {
  Reset();
}

void VCMCodecTimer::Reset() {
  _filteredMax = 0;
  _ignoredSampleCount = 0;
  _shortMax = 0;
  for (int i = 0; i < MAX_HISTORY_SIZE; i++) {
    _history[i].shortMax = 0;
    _history[i].timeMs = -1;
  }
}

// Update the max-value filter
void VCMCodecTimer::MaxFilter(int32_t decodeTime, int64_t nowMs) {
  if (_ignoredSampleCount >= kIgnoredSampleCount) {
    UpdateMaxHistory(decodeTime, nowMs);
    ProcessHistory(nowMs);
  } else {
    _ignoredSampleCount++;
  }
}

void VCMCodecTimer::UpdateMaxHistory(int32_t decodeTime, int64_t now) {
  if (_history[0].timeMs >= 0 && now - _history[0].timeMs < SHORT_FILTER_MS) {
    if (decodeTime > _shortMax) {
      _shortMax = decodeTime;
    }
  } else {
    // Only add a new value to the history once a second
    if (_history[0].timeMs == -1) {
      // First, no shift
      _shortMax = decodeTime;
    } else {
      // Shift
      for (int i = (MAX_HISTORY_SIZE - 2); i >= 0; i--) {
        _history[i + 1].shortMax = _history[i].shortMax;
        _history[i + 1].timeMs = _history[i].timeMs;
      }
    }
    if (_shortMax == 0) {
      _shortMax = decodeTime;
    }

    _history[0].shortMax = _shortMax;
    _history[0].timeMs = now;
    _shortMax = 0;
  }
}

void VCMCodecTimer::ProcessHistory(int64_t nowMs) {
  _filteredMax = _shortMax;
  if (_history[0].timeMs == -1) {
    return;
  }
  for (int i = 0; i < MAX_HISTORY_SIZE; i++) {
    if (_history[i].timeMs == -1) {
      break;
    }
    if (nowMs - _history[i].timeMs > MAX_HISTORY_SIZE * SHORT_FILTER_MS) {
      // This sample (and all samples after this) is too old
      break;
    }
    if (_history[i].shortMax > _filteredMax) {
      // This sample is the largest one this far into the history
      _filteredMax = _history[i].shortMax;
    }
  }
}

// Get the maximum observed time within a time window
int32_t VCMCodecTimer::RequiredDecodeTimeMs(FrameType /*frameType*/) const {
  return _filteredMax;
}
}  // namespace webrtc
