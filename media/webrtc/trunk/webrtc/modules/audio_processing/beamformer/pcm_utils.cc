/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/beamformer/pcm_utils.h"

#include "webrtc/base/checks.h"
#include "webrtc/common_audio/include/audio_util.h"
#include "webrtc/common_audio/channel_buffer.h"

namespace webrtc {

size_t PcmRead(FILE* file,
               size_t length,
               int num_channels,
               int16_t* const* buffer) {
  CHECK_GE(num_channels, 1);

  rtc::scoped_ptr<int16_t[]> interleaved_buffer(new int16_t[length]);
  size_t elements_read = fread(interleaved_buffer.get(), sizeof(int16_t),
                               length, file);
  if (elements_read != length) {
    // This is only an error if we haven't reached the end of the file.
    CHECK_NE(0, feof(file));
  }

  Deinterleave(interleaved_buffer.get(),
               static_cast<int>(elements_read) / num_channels,
               num_channels,
               buffer);
  return elements_read;
}

size_t PcmReadToFloat(FILE* file,
                      size_t length,
                      int num_channels,
                      float* const* buffer) {
  CHECK_GE(num_channels, 1);

  int num_frames = static_cast<int>(length) / num_channels;
  rtc::scoped_ptr<ChannelBuffer<int16_t> > deinterleaved_buffer(
      new ChannelBuffer<int16_t>(num_frames, num_channels));

  size_t elements_read =
      PcmRead(file, length, num_channels, deinterleaved_buffer->channels());

  for (int i = 0; i < num_channels; ++i) {
    S16ToFloat(deinterleaved_buffer->channels()[i], num_frames, buffer[i]);
  }
  return elements_read;
}

void PcmWrite(FILE* file,
              size_t length,
              int num_channels,
              const int16_t* const* buffer) {
  CHECK_GE(num_channels, 1);

  rtc::scoped_ptr<int16_t[]> interleaved_buffer(new int16_t[length]);
  Interleave(buffer,
             static_cast<int>(length) / num_channels,
             num_channels,
             interleaved_buffer.get());
  CHECK_EQ(length,
           fwrite(interleaved_buffer.get(), sizeof(int16_t), length, file));
}

void PcmWriteFromFloat(FILE* file,
                       size_t length,
                       int num_channels,
                       const float* const* buffer) {
  CHECK_GE(num_channels, 1);

  int num_frames = static_cast<int>(length) / num_channels;
  rtc::scoped_ptr<ChannelBuffer<int16_t> > deinterleaved_buffer(
      new ChannelBuffer<int16_t>(num_frames, num_channels));

  for (int i = 0; i < num_channels; ++i) {
    FloatToS16(buffer[i], num_frames, deinterleaved_buffer->channels()[i]);
  }
  PcmWrite(file, length, num_channels, deinterleaved_buffer->channels());
}

}  // namespace webrtc
