/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_VIDEO_ENGINE_MOCK_MOCK_VIE_FRAME_PROVIDER_BASE_H_
#define WEBRTC_VIDEO_ENGINE_MOCK_MOCK_VIE_FRAME_PROVIDER_BASE_H_

#include "webrtc/video_engine/vie_frame_provider_base.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace webrtc {

class MockViEFrameCallback : public ViEFrameCallback {
 public:
  MOCK_METHOD3(DeliverFrame,
               void(int id,
                    I420VideoFrame* video_frame,
                    const std::vector<uint32_t>& csrcs));
  MOCK_METHOD2(DelayChanged, void(int id, int frame_delay));
  MOCK_METHOD3(GetPreferedFrameSettings,
               int(int* width, int* height, int* frame_rate));
  MOCK_METHOD1(ProviderDestroyed, void(int id));
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_MOCK_MOCK_VIE_FRAME_PROVIDER_BASE_H_
