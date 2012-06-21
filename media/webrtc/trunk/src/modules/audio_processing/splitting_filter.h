/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_MAIN_SOURCE_SPLITTING_FILTER_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_MAIN_SOURCE_SPLITTING_FILTER_H_

#include "typedefs.h"
#include "signal_processing_library.h"

namespace webrtc {
/*
 * SplittingFilterbank_analysisQMF(...)
 *
 * Splits a super-wb signal into two subbands: 0-8 kHz and 8-16 kHz.
 *
 * Input:
 *    - in_data  : super-wb audio signal
 *
 * Input & Output:
 *    - filt_state1: Filter state for first all-pass filter
 *    - filt_state2: Filter state for second all-pass filter
 *
 * Output:
 *    - low_band : The signal from the 0-4 kHz band
 *    - high_band  : The signal from the 4-8 kHz band
 */
void SplittingFilterAnalysis(const WebRtc_Word16* in_data,
                             WebRtc_Word16* low_band,
                             WebRtc_Word16* high_band,
                             WebRtc_Word32* filt_state1,
                             WebRtc_Word32* filt_state2);

/*
 * SplittingFilterbank_synthesisQMF(...)
 *
 * Combines the two subbands (0-8 and 8-16 kHz) into a super-wb signal.
 *
 * Input:
 *    - low_band : The signal with the 0-8 kHz band
 *    - high_band  : The signal with the 8-16 kHz band
 *
 * Input & Output:
 *    - filt_state1: Filter state for first all-pass filter
 *    - filt_state2: Filter state for second all-pass filter
 *
 * Output:
 *    - out_data : super-wb speech signal
 */
void SplittingFilterSynthesis(const WebRtc_Word16* low_band,
                              const WebRtc_Word16* high_band,
                              WebRtc_Word16* out_data,
                              WebRtc_Word32* filt_state1,
                              WebRtc_Word32* filt_state2);
}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_MAIN_SOURCE_SPLITTING_FILTER_H_
