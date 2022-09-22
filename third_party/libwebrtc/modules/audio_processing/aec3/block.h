/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_BLOCK_H_
#define MODULES_AUDIO_PROCESSING_AEC3_BLOCK_H_

#include <array>
#include <vector>

#include "api/array_view.h"
#include "modules/audio_processing/aec3/aec3_common.h"

namespace webrtc {

class Block {
 public:
  Block(int num_bands, int num_channels, float default_value = 0.0f)
      : data_(num_bands,
              std::vector<std::array<float, kBlockSize>>(
                  num_channels,
                  std::array<float, kBlockSize>({default_value}))) {}

  // Returns the number of bands.
  int NumBands() const { return data_.size(); }

  // Returns the number of channels.
  int NumChannels() const { return data_[0].size(); }

  // Modifies the number of channels.
  void SetNumChannels(int num_channels) {
    for (std::vector<std::array<float, kBlockSize>>& block_band : data_) {
      block_band.resize(num_channels, std::array<float, kBlockSize>({0.0f}));
    }
  }

  // Iterators for accessing the data.
  auto begin(int band, int channel) { return data_[band][channel].begin(); }

  auto begin(int band, int channel) const {
    return data_[band][channel].begin();
  }

  auto end(int band, int channel) { return data_[band][channel].end(); }

  auto end(int band, int channel) const { return data_[band][channel].end(); }

  // Access data via ArrayView.
  rtc::ArrayView<float, kBlockSize> View(int band, int channel) {
    return rtc::ArrayView<float, kBlockSize>(data_[band][channel].data(),
                                             kBlockSize);
  }

  rtc::ArrayView<const float, kBlockSize> View(int band, int channel) const {
    return rtc::ArrayView<const float, kBlockSize>(data_[band][channel].data(),
                                                   kBlockSize);
  }

  // Lets two Blocks swap audio data.
  void Swap(Block& b) { data_.swap(b.data_); }

 private:
  std::vector<std::vector<std::array<float, kBlockSize>>> data_;
};

}  // namespace webrtc
#endif  // MODULES_AUDIO_PROCESSING_AEC3_BLOCK_H_
