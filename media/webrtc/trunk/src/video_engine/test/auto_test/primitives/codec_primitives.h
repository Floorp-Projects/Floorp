/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_TEST_AUTO_TEST_PRIMITIVES_CODEC_PRIMITIVES_H_
#define WEBRTC_VIDEO_ENGINE_TEST_AUTO_TEST_PRIMITIVES_CODEC_PRIMITIVES_H_

#include "video_engine/include/vie_codec.h"
#include "video_engine/include/vie_image_process.h"
#include "video_engine/test/auto_test/interface/vie_autotest_defines.h"
#include "video_engine/test/auto_test/primitives/general_primitives.h"

class TbInterfaces;

// Tests that a codec actually renders frames by registering a basic
// render effect filter on the codec and then running it. This test is
// quite lenient on the number of frames that get rendered, so it should not
// be seen as a end-user-visible quality measure - it is more a sanity check
// that the codec at least gets some frames through.

// The codec resolution can be forced by specifying the forced* variables
// (pass in kDoNotForceResolution if you don't care).
void TestCodecs(const TbInterfaces& interfaces,
                int capture_id,
                int video_channel,
                int forced_codec_width,
                int forced_codec_height);

// This helper function will set the send codec in the codec interface to a
// codec of the specified type. It will generate a test failure if we do not
// support the provided codec type.

// The codec resolution can be forced by specifying the forced* variables
// (pass in kDoNotForceResolution if you don't care).
void SetSendCodec(webrtc::VideoCodecType of_type,
                  webrtc::ViECodec* codec_interface,
                  int video_channel,
                  int forced_codec_width,
                  int forced_codec_height);

class ViEAutotestCodecObserver: public webrtc::ViEEncoderObserver,
                                public webrtc::ViEDecoderObserver {
 public:
  int incomingCodecCalled;
  int incomingRatecalled;
  int outgoingRatecalled;

  unsigned char lastPayloadType;
  unsigned short lastWidth;
  unsigned short lastHeight;

  unsigned int lastOutgoingFramerate;
  unsigned int lastOutgoingBitrate;
  unsigned int lastIncomingFramerate;
  unsigned int lastIncomingBitrate;

  webrtc::VideoCodec incomingCodec;

  ViEAutotestCodecObserver() {
    incomingCodecCalled = 0;
    incomingRatecalled = 0;
    outgoingRatecalled = 0;
    lastPayloadType = 0;
    lastWidth = 0;
    lastHeight = 0;
    lastOutgoingFramerate = 0;
    lastOutgoingBitrate = 0;
    lastIncomingFramerate = 0;
    lastIncomingBitrate = 0;
    memset(&incomingCodec, 0, sizeof(incomingCodec));
  }
  virtual void IncomingCodecChanged(const int videoChannel,
                                    const webrtc::VideoCodec& videoCodec) {
    incomingCodecCalled++;
    lastPayloadType = videoCodec.plType;
    lastWidth = videoCodec.width;
    lastHeight = videoCodec.height;

    memcpy(&incomingCodec, &videoCodec, sizeof(videoCodec));
  }

  virtual void IncomingRate(const int videoChannel,
                            const unsigned int framerate,
                            const unsigned int bitrate) {
    incomingRatecalled++;
    lastIncomingFramerate += framerate;
    lastIncomingBitrate += bitrate;
  }

  virtual void OutgoingRate(const int videoChannel,
                            const unsigned int framerate,
                            const unsigned int bitrate) {
    outgoingRatecalled++;
    lastOutgoingFramerate += framerate;
    lastOutgoingBitrate += bitrate;
  }

  virtual void RequestNewKeyFrame(const int videoChannel) {
  }
};

class FrameCounterEffectFilter : public webrtc::ViEEffectFilter
{
 public:
  int numFrames;
  FrameCounterEffectFilter() {
    numFrames = 0;
  }
  virtual ~FrameCounterEffectFilter() {
  }

  virtual int Transform(int size, unsigned char* frameBuffer,
                        unsigned int timeStamp90KHz, unsigned int width,
                        unsigned int height) {
    numFrames++;
    return 0;
  }
};

#endif  // WEBRTC_VIDEO_ENGINE_TEST_AUTO_TEST_PRIMITIVES_CODEC_PRIMITIVES_H_
