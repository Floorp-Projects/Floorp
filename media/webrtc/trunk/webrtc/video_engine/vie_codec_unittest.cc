/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/common_types.h"

namespace webrtc {

// Builds VP8 codec with 0 simulcast streams.
void BuildVP8Codec(webrtc::VideoCodec* video_codec) {
  video_codec->codecType = kVideoCodecVP8;
  strncpy(video_codec->plName, "VP8", 4);
  video_codec->plType = 100;
  video_codec->width = 1280;
  video_codec->height = 720;

  video_codec->startBitrate = 1000;  // kbps
  video_codec->maxBitrate = 2000;  // kbps
  video_codec->minBitrate = 1000;  // kbps
  video_codec->maxFramerate = 30;

  video_codec->qpMax = 50;
  video_codec->numberOfSimulcastStreams = 0;
  video_codec->mode = kRealtimeVideo;

  // Set VP8 codec specific info.
  video_codec->codecSpecific.VP8.pictureLossIndicationOn = true;
  video_codec->codecSpecific.VP8.feedbackModeOn = true;
  video_codec->codecSpecific.VP8.complexity = kComplexityNormal;
  video_codec->codecSpecific.VP8.resilience = kResilienceOff;
  video_codec->codecSpecific.VP8.numberOfTemporalLayers = 0;
  video_codec->codecSpecific.VP8.denoisingOn = true;
  video_codec->codecSpecific.VP8.errorConcealmentOn = true;
  video_codec->codecSpecific.VP8.automaticResizeOn = true;
  video_codec->codecSpecific.VP8.frameDroppingOn = true;
  video_codec->codecSpecific.VP8.keyFrameInterval = 200;
}


void SetSimulcastSettings(webrtc::VideoCodec* video_codec) {
  // Simulcast settings.
  video_codec->numberOfSimulcastStreams = 1;
  video_codec->simulcastStream[0].width = 320;
  video_codec->simulcastStream[0].height = 180;
  video_codec->simulcastStream[0].numberOfTemporalLayers = 0;
  video_codec->simulcastStream[0].maxBitrate = 100;
  video_codec->simulcastStream[0].targetBitrate = 100;
  video_codec->simulcastStream[0].minBitrate = 0;
  video_codec->simulcastStream[0].qpMax = video_codec->qpMax;
}


// This test compares two VideoCodecInst objects except codec specific and
// simulcast streams.
TEST(ViECodecTest, TestCompareCodecs) {
  VideoCodec codec1, codec2;
  memset(&codec1, 0, sizeof(VideoCodec));
  memset(&codec2, 0, sizeof(VideoCodec));

  BuildVP8Codec(&codec1);
  BuildVP8Codec(&codec2);

  EXPECT_TRUE(codec1 == codec2);
  EXPECT_FALSE(codec1 != codec2);

  // plname is case insensitive.
  strncpy(codec2.plName, "vp8", 4);
  EXPECT_TRUE(codec1 == codec2);

  codec2.codecType = kVideoCodecUnknown;
  EXPECT_FALSE(codec1 == codec2);

  // Modify pltype.
  BuildVP8Codec(&codec2);
  codec2.plType = 101;
  EXPECT_FALSE(codec1 == codec2);

  // Modifing height and width.
  BuildVP8Codec(&codec2);
  codec2.width = 640;
  codec2.height = 480;
  EXPECT_FALSE(codec1 == codec2);

  // Modify framerate, default value is 30.
  BuildVP8Codec(&codec2);
  codec2.maxFramerate = 15;
  EXPECT_FALSE(codec1 == codec2);

  // Modifying startBitrate, default value is 1000 kbps.
  BuildVP8Codec(&codec2);
  codec2.startBitrate = 2000;
  EXPECT_FALSE(codec1 == codec2);
  // maxBitrate
  BuildVP8Codec(&codec2);
  codec2.startBitrate = 3000;
  EXPECT_FALSE(codec1 == codec2);
  // minBirate
  BuildVP8Codec(&codec2);
  codec2.startBitrate = 500;
  EXPECT_FALSE(codec1 == codec2);

  // Modify qpMax.
  BuildVP8Codec(&codec2);
  codec2.qpMax = 100;
  EXPECT_FALSE(codec1 == codec2);

  // Modify mode
  BuildVP8Codec(&codec2);
  codec2.mode = kScreensharing;
  EXPECT_FALSE(codec1 == codec2);
}

// Test VP8 specific comparision.
TEST(ViECodecTest, TestCompareVP8CodecSpecific) {
  VideoCodec codec1, codec2;
  memset(&codec1, 0, sizeof(VideoCodec));
  memset(&codec2, 0, sizeof(VideoCodec));

  BuildVP8Codec(&codec1);
  BuildVP8Codec(&codec2);
  EXPECT_TRUE(codec1 == codec2);

  // pictureLossIndicationOn
  codec2.codecSpecific.VP8.pictureLossIndicationOn = false;
  EXPECT_FALSE(codec1 == codec2);

  // feedbackModeOn
  BuildVP8Codec(&codec2);
  codec2.codecSpecific.VP8.feedbackModeOn = false;
  EXPECT_FALSE(codec1 == codec2);

  // complexity
  BuildVP8Codec(&codec2);
  codec2.codecSpecific.VP8.complexity = kComplexityHigh;
  EXPECT_FALSE(codec1 == codec2);

  // resilience
  BuildVP8Codec(&codec2);
  codec2.codecSpecific.VP8.resilience = kResilientStream;
  EXPECT_FALSE(codec1 == codec2);

  // numberOfTemporalLayers
  BuildVP8Codec(&codec2);
  codec2.codecSpecific.VP8.numberOfTemporalLayers = 2;
  EXPECT_FALSE(codec1 == codec2);

  // denoisingOn
  BuildVP8Codec(&codec2);
  codec2.codecSpecific.VP8.denoisingOn = false;
  EXPECT_FALSE(codec1 == codec2);

  // errorConcealmentOn
  BuildVP8Codec(&codec2);
  codec2.codecSpecific.VP8.errorConcealmentOn = false;
  EXPECT_FALSE(codec1 == codec2);

  // pictureLossIndicationOn
  BuildVP8Codec(&codec2);
  codec2.codecSpecific.VP8.automaticResizeOn = false;
  EXPECT_FALSE(codec1 == codec2);

  // frameDroppingOn
  BuildVP8Codec(&codec2);
  codec2.codecSpecific.VP8.frameDroppingOn = false;
  EXPECT_FALSE(codec1 == codec2);

  // keyFrameInterval
  BuildVP8Codec(&codec2);
  codec2.codecSpecific.VP8.keyFrameInterval = 100;
  EXPECT_FALSE(codec1 == codec2);
}

// This test compares simulcast stream information in VideoCodec.
TEST(ViECodecTest, TestCompareSimulcastStreams) {
  VideoCodec codec1, codec2;
  memset(&codec1, 0, sizeof(VideoCodec));
  memset(&codec2, 0, sizeof(VideoCodec));

  BuildVP8Codec(&codec1);
  BuildVP8Codec(&codec2);
  // Set simulacast settings.
  SetSimulcastSettings(&codec1);
  SetSimulcastSettings(&codec2);
  EXPECT_TRUE(codec1 == codec2);

  // Modify number of streams.
  codec2.numberOfSimulcastStreams = 2;
  EXPECT_FALSE(codec1 == codec2);

  // Resetting steram count.
  codec2.numberOfSimulcastStreams = 1;
  // Modify height and width in codec2.
  codec2.simulcastStream[0].width = 640;
  codec2.simulcastStream[0].height = 480;
  EXPECT_FALSE(codec1 == codec2);

  // numberOfTemporalLayers
  SetSimulcastSettings(&codec2);
  codec2.simulcastStream[0].numberOfTemporalLayers = 2;
  EXPECT_FALSE(codec1 == codec2);

  // maxBitrate
  SetSimulcastSettings(&codec2);
  codec2.simulcastStream[0].maxBitrate = 1000;
  EXPECT_FALSE(codec1 == codec2);

  // targetBitrate
  SetSimulcastSettings(&codec2);
  codec2.simulcastStream[0].targetBitrate = 1000;
  EXPECT_FALSE(codec1 == codec2);

  // minBitrate
  SetSimulcastSettings(&codec2);
  codec2.simulcastStream[0].minBitrate = 50;
  EXPECT_FALSE(codec1 == codec2);

  // qpMax
  SetSimulcastSettings(&codec2);
  codec2.simulcastStream[0].qpMax = 100;
  EXPECT_FALSE(codec1 == codec2);
}

}  // namespace webrtc
