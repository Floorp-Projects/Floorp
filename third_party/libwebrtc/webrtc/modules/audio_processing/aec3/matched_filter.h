/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_MATCHED_FILTER_H_
#define MODULES_AUDIO_PROCESSING_AEC3_MATCHED_FILTER_H_

#include <array>
#include <memory>
#include <vector>

#include "api/optional.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/downsampled_render_buffer.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {
namespace aec3 {

#if defined(WEBRTC_HAS_NEON)

// Filter core for the matched filter that is optimized for NEON.
void MatchedFilterCore_NEON(size_t x_start_index,
                            float x2_sum_threshold,
                            rtc::ArrayView<const float> x,
                            rtc::ArrayView<const float> y,
                            rtc::ArrayView<float> h,
                            bool* filters_updated,
                            float* error_sum);

#endif

#if defined(WEBRTC_ARCH_X86_FAMILY)

// Filter core for the matched filter that is optimized for SSE2.
void MatchedFilterCore_SSE2(size_t x_start_index,
                            float x2_sum_threshold,
                            rtc::ArrayView<const float> x,
                            rtc::ArrayView<const float> y,
                            rtc::ArrayView<float> h,
                            bool* filters_updated,
                            float* error_sum);

#endif

// Filter core for the matched filter.
void MatchedFilterCore(size_t x_start_index,
                       float x2_sum_threshold,
                       rtc::ArrayView<const float> x,
                       rtc::ArrayView<const float> y,
                       rtc::ArrayView<float> h,
                       bool* filters_updated,
                       float* error_sum);

}  // namespace aec3

class ApmDataDumper;

// Produces recursively updated cross-correlation estimates for several signal
// shifts where the intra-shift spacing is uniform.
class MatchedFilter {
 public:
  // Stores properties for the lag estimate corresponding to a particular signal
  // shift.
  struct LagEstimate {
    LagEstimate() = default;
    LagEstimate(float accuracy, bool reliable, size_t lag, bool updated)
        : accuracy(accuracy), reliable(reliable), lag(lag), updated(updated) {}

    float accuracy = 0.f;
    bool reliable = false;
    size_t lag = 0;
    bool updated = false;
  };

  MatchedFilter(ApmDataDumper* data_dumper,
                Aec3Optimization optimization,
                size_t sub_block_size,
                size_t window_size_sub_blocks,
                int num_matched_filters,
                size_t alignment_shift_sub_blocks,
                float excitation_limit);

  ~MatchedFilter();

  // Updates the correlation with the values in the capture buffer.
  void Update(const DownsampledRenderBuffer& render_buffer,
              rtc::ArrayView<const float> capture);

  // Resets the matched filter.
  void Reset();

  // Returns the current lag estimates.
  rtc::ArrayView<const MatchedFilter::LagEstimate> GetLagEstimates() const {
    return lag_estimates_;
  }

  // Returns the maximum filter lag.
  size_t GetMaxFilterLag() const {
    return filters_.size() * filter_intra_lag_shift_ + filters_[0].size();
  }

  // Log matched filter properties.
  void LogFilterProperties(int sample_rate_hz,
                           size_t shift,
                           size_t downsampling_factor) const;

 private:
  ApmDataDumper* const data_dumper_;
  const Aec3Optimization optimization_;
  const size_t sub_block_size_;
  const size_t filter_intra_lag_shift_;
  std::vector<std::vector<float>> filters_;
  std::vector<LagEstimate> lag_estimates_;
  std::vector<size_t> filters_offsets_;
  const float excitation_limit_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(MatchedFilter);
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_MATCHED_FILTER_H_
