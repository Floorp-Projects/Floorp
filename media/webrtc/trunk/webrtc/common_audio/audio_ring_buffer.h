/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stddef.h>
#include <vector>

struct RingBuffer;

namespace webrtc {

// A ring buffer tailored for float deinterleaved audio. Any operation that
// cannot be performed as requested will cause a crash (e.g. insufficient data
// in the buffer to fulfill a read request.)
class AudioRingBuffer final {
 public:
  // Specify the number of channels and maximum number of frames the buffer will
  // contain.
  AudioRingBuffer(size_t channels, size_t max_frames);
  ~AudioRingBuffer();

  // Copy |data| to the buffer and advance the write pointer. |channels| must
  // be the same as at creation time.
  void Write(const float* const* data, size_t channels, size_t frames);

  // Copy from the buffer to |data| and advance the read pointer. |channels|
  // must be the same as at creation time.
  void Read(float* const* data, size_t channels, size_t frames);

  size_t ReadFramesAvailable() const;
  size_t WriteFramesAvailable() const;

  // Positive values advance the read pointer and negative values withdraw
  // the read pointer (i.e. flush and stuff the buffer respectively.)
  void MoveReadPosition(int frames);

 private:
  // We don't use a ScopedVector because it doesn't support a specialized
  // deleter (like scoped_ptr for instance.)
  std::vector<RingBuffer*> buffers_;
};

}  // namespace webrtc
