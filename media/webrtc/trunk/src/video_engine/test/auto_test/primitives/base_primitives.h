/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_SOURCE_BASE_PRIMITIVES_H_
#define SRC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_SOURCE_BASE_PRIMITIVES_H_

namespace webrtc {
class VideoEngine;
class ViEBase;
class ViECodec;
class ViENetwork;
}

// Tests a I420-to-I420 call. This test exercises the most basic WebRTC
// functionality by training the codec interface to recognize the most common
// codecs, and the initiating a I420 call. A video channel with a capture device
// must be set up prior to this call.
void TestI420CallSetup(webrtc::ViECodec* codec_interface,
                       webrtc::VideoEngine* video_engine,
                       webrtc::ViEBase* base_interface,
                       webrtc::ViENetwork* network_interface,
                       int video_channel,
                       const char* device_name);

#endif  // SRC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_SOURCE_BASE_PRIMITIVES_H_
