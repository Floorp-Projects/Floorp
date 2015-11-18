/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_BEAMFORMER_PCM_UTILS_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_BEAMFORMER_PCM_UTILS_H_

#include <inttypes.h>
#include <stdio.h>

// Utilities for reading from and writing to multichannel pcm files.
// Assumes a bit depth of 16 and little-endian. Note that in these functions,
// length refers to the number of samples to read from/write to the file,
// such that length / num_channels is the number of frames.
namespace webrtc {

// Reads audio from a pcm into a 2D array: buffer[channel_index][frame_index].
// Returns the number of frames written. If this is less than |length|, it's
// safe to assume the end-of-file was reached, as otherwise this will crash.
// In PcmReadToFloat, the floats are within the range [-1, 1].
size_t PcmRead(FILE* file,
               size_t length,
               int num_channels,
               int16_t* const* buffer);
size_t PcmReadToFloat(FILE* file,
                      size_t length,
                      int num_channels,
                      float* const* buffer);

// Writes to a pcm file. The resulting file contains the channels interleaved.
// Crashes if the correct number of frames aren't written to the file. For
// PcmWriteFromFloat, floats must be within the range [-1, 1].
void PcmWrite(FILE* file,
              size_t length,
              int num_channels,
              const int16_t* const* buffer);
void PcmWriteFromFloat(FILE* file,
                       size_t length,
                       int num_channels,
                       const float* const* buffer);

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_BEAMFORMER_PCM_UTILS_H_
