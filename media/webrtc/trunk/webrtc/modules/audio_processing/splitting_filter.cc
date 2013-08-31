/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_audio/signal_processing/include/signal_processing_library.h"
#include "webrtc/modules/audio_processing/splitting_filter.h"

namespace webrtc {

void SplittingFilterAnalysis(const int16_t* in_data,
                             int16_t* low_band,
                             int16_t* high_band,
                             int32_t* filter_state1,
                             int32_t* filter_state2)
{
    WebRtcSpl_AnalysisQMF(in_data, low_band, high_band, filter_state1, filter_state2);
}

void SplittingFilterSynthesis(const int16_t* low_band,
                              const int16_t* high_band,
                              int16_t* out_data,
                              int32_t* filt_state1,
                              int32_t* filt_state2)
{
    WebRtcSpl_SynthesisQMF(low_band, high_band, out_data, filt_state1, filt_state2);
}
}  // namespace webrtc
