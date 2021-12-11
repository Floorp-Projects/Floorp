/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/aec3/matched_filter_lag_aggregator.h"

#include "modules/audio_processing/logging/apm_data_dumper.h"

namespace webrtc {

MatchedFilterLagAggregator::MatchedFilterLagAggregator(
    ApmDataDumper* data_dumper,
    size_t max_filter_lag)
    : data_dumper_(data_dumper), histogram_(max_filter_lag + 1, 0) {
  RTC_DCHECK(data_dumper);
  histogram_data_.fill(0);
}

MatchedFilterLagAggregator::~MatchedFilterLagAggregator() = default;

void MatchedFilterLagAggregator::Reset() {
  std::fill(histogram_.begin(), histogram_.end(), 0);
  histogram_data_.fill(0);
  histogram_data_index_ = 0;
}

rtc::Optional<size_t> MatchedFilterLagAggregator::Aggregate(
    rtc::ArrayView<const MatchedFilter::LagEstimate> lag_estimates) {
  // Choose the strongest lag estimate as the best one.
  float best_accuracy = 0.f;
  int best_lag_estimate_index = -1;
  for (size_t k = 0; k < lag_estimates.size(); ++k) {
    if (lag_estimates[k].updated && lag_estimates[k].reliable) {
      if (lag_estimates[k].accuracy > best_accuracy) {
        best_accuracy = lag_estimates[k].accuracy;
        best_lag_estimate_index = static_cast<int>(k);
      }
    }
  }

  // TODO(peah): Remove this logging once all development is done.
  data_dumper_->DumpRaw("aec3_echo_path_delay_estimator_best_index",
                        best_lag_estimate_index);

  if (best_lag_estimate_index != -1) {
    RTC_DCHECK_GT(histogram_.size(), histogram_data_[histogram_data_index_]);
    RTC_DCHECK_LE(0, histogram_data_[histogram_data_index_]);
    --histogram_[histogram_data_[histogram_data_index_]];

    histogram_data_[histogram_data_index_] =
        lag_estimates[best_lag_estimate_index].lag;

    RTC_DCHECK_GT(histogram_.size(), histogram_data_[histogram_data_index_]);
    RTC_DCHECK_LE(0, histogram_data_[histogram_data_index_]);
    ++histogram_[histogram_data_[histogram_data_index_]];

    histogram_data_index_ =
        (histogram_data_index_ + 1) % histogram_data_.size();

    const int candidate =
        std::distance(histogram_.begin(),
                      std::max_element(histogram_.begin(), histogram_.end()));

    if (histogram_[candidate] > 25) {
      return candidate;
    }
  }
  return rtc::nullopt;
}

}  // namespace webrtc
