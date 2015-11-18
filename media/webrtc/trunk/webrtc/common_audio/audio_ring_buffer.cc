/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_audio/audio_ring_buffer.h"

#include "webrtc/base/checks.h"
#include "webrtc/common_audio/ring_buffer.h"

// This is a simple multi-channel wrapper over the ring_buffer.h C interface.

namespace webrtc {

AudioRingBuffer::AudioRingBuffer(size_t channels, size_t max_frames) {
  for (size_t i = 0; i < channels; ++i)
    buffers_.push_back(WebRtc_CreateBuffer(max_frames, sizeof(float)));
}

AudioRingBuffer::~AudioRingBuffer() {
  for (auto buf : buffers_)
    WebRtc_FreeBuffer(buf);
}

void AudioRingBuffer::Write(const float* const* data, size_t channels,
                            size_t frames) {
  DCHECK_EQ(buffers_.size(), channels);
  for (size_t i = 0; i < channels; ++i) {
    size_t written = WebRtc_WriteBuffer(buffers_[i], data[i], frames);
    CHECK_EQ(written, frames);
  }
}

void AudioRingBuffer::Read(float* const* data, size_t channels, size_t frames) {
  DCHECK_EQ(buffers_.size(), channels);
  for (size_t i = 0; i < channels; ++i) {
    size_t read = WebRtc_ReadBuffer(buffers_[i], nullptr, data[i], frames);
    CHECK_EQ(read, frames);
  }
}

size_t AudioRingBuffer::ReadFramesAvailable() const {
  // All buffers have the same amount available.
  return WebRtc_available_read(buffers_[0]);
}

size_t AudioRingBuffer::WriteFramesAvailable() const {
  // All buffers have the same amount available.
  return WebRtc_available_write(buffers_[0]);
}

void AudioRingBuffer::MoveReadPosition(int frames) {
  for (auto buf : buffers_) {
    int moved = WebRtc_MoveReadPtr(buf, frames);
    CHECK_EQ(moved, frames);
  }
}

}  // namespace webrtc
