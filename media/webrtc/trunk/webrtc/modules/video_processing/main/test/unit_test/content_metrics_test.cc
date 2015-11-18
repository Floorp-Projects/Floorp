/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/video_processing/main/interface/video_processing.h"
#include "webrtc/modules/video_processing/main/source/content_analysis.h"
#include "webrtc/modules/video_processing/main/test/unit_test/video_processing_unittest.h"

namespace webrtc {

TEST_F(VideoProcessingModuleTest, ContentAnalysis) {
  VPMContentAnalysis    ca__c(false);
  VPMContentAnalysis    ca__sse(true);
  VideoContentMetrics  *_cM_c, *_cM_SSE;

  ca__c.Initialize(width_,height_);
  ca__sse.Initialize(width_,height_);

  rtc::scoped_ptr<uint8_t[]> video_buffer(new uint8_t[frame_length_]);
  while (fread(video_buffer.get(), 1, frame_length_, source_file_)
       == frame_length_) {
    // Using ConvertToI420 to add stride to the image.
    EXPECT_EQ(0, ConvertToI420(kI420, video_buffer.get(), 0, 0, width_, height_,
                               0, kVideoRotation_0, &video_frame_));
    _cM_c   = ca__c.ComputeContentMetrics(video_frame_);
    _cM_SSE = ca__sse.ComputeContentMetrics(video_frame_);

    ASSERT_EQ(_cM_c->spatial_pred_err, _cM_SSE->spatial_pred_err);
    ASSERT_EQ(_cM_c->spatial_pred_err_v, _cM_SSE->spatial_pred_err_v);
    ASSERT_EQ(_cM_c->spatial_pred_err_h, _cM_SSE->spatial_pred_err_h);
    ASSERT_EQ(_cM_c->motion_magnitude, _cM_SSE->motion_magnitude);
  }
  ASSERT_NE(0, feof(source_file_)) << "Error reading source file";
}

}  // namespace webrtc
