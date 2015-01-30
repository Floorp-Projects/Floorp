/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/neteq/tools/input_audio_file.h"

namespace webrtc {
namespace test {

InputAudioFile::InputAudioFile(const std::string file_name) {
  fp_ = fopen(file_name.c_str(), "rb");
}

InputAudioFile::~InputAudioFile() { fclose(fp_); }

bool InputAudioFile::Read(size_t samples, int16_t* destination) {
  if (!fp_) {
    return false;
  }
  size_t samples_read = fread(destination, sizeof(int16_t), samples, fp_);
  if (samples_read < samples) {
    // Rewind and read the missing samples.
    rewind(fp_);
    size_t missing_samples = samples - samples_read;
    if (fread(destination, sizeof(int16_t), missing_samples, fp_) <
        missing_samples) {
      // Could not read enough even after rewinding the file.
      return false;
    }
  }
  return true;
}

void InputAudioFile::DuplicateInterleaved(const int16_t* source, size_t samples,
                                          size_t channels,
                                          int16_t* destination) {
  // Start from the end of |source| and |destination|, and work towards the
  // beginning. This is to allow in-place interleaving of the same array (i.e.,
  // |source| and |destination| are the same array).
  for (int i = static_cast<int>(samples - 1); i >= 0; --i) {
    for (int j = static_cast<int>(channels - 1); j >= 0; --j) {
      destination[i * channels + j] = source[i];
    }
  }
}

}  // namespace test
}  // namespace webrtc
