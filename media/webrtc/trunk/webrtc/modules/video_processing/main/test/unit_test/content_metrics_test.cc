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
#include "webrtc/modules/video_processing/main/test/unit_test/unit_test.h"

namespace webrtc {

TEST_F(VideoProcessingModuleTest, ContentAnalysis)
{
    VPMContentAnalysis    _ca_c(false);
    VPMContentAnalysis    _ca_sse(true);
    VideoContentMetrics  *_cM_c, *_cM_SSE;

    _ca_c.Initialize(_width,_height);
    _ca_sse.Initialize(_width,_height);

    scoped_array<uint8_t> video_buffer(new uint8_t[_frame_length]);
    while (fread(video_buffer.get(), 1, _frame_length, _sourceFile)
           == _frame_length)
    {
        // Using ConvertToI420 to add stride to the image.
        EXPECT_EQ(0, ConvertToI420(kI420, video_buffer.get(), 0, 0,
                                   _width, _height,
                                   0, kRotateNone, &_videoFrame));
        _cM_c   = _ca_c.ComputeContentMetrics(_videoFrame);
        _cM_SSE = _ca_sse.ComputeContentMetrics(_videoFrame);

        ASSERT_EQ(_cM_c->spatial_pred_err, _cM_SSE->spatial_pred_err);
        ASSERT_EQ(_cM_c->spatial_pred_err_v, _cM_SSE->spatial_pred_err_v);
        ASSERT_EQ(_cM_c->spatial_pred_err_h, _cM_SSE->spatial_pred_err_h);
        ASSERT_EQ(_cM_c->motion_magnitude, _cM_SSE->motion_magnitude);
    }
    ASSERT_NE(0, feof(_sourceFile)) << "Error reading source file";
}

}  // namespace webrtc
