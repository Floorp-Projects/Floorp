/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_TEST_AUTO_TEST_PRIMITIVES_GENERAL_PRIMITIVES_H_
#define WEBRTC_VIDEO_ENGINE_TEST_AUTO_TEST_PRIMITIVES_GENERAL_PRIMITIVES_H_

class ViEToFileRenderer;

#include "common_types.h"

namespace webrtc {
class VideoCaptureModule;
class ViEBase;
class ViECapture;
class ViECodec;
class ViERender;
class ViERTP_RTCP;
struct VideoCodec;
}

enum ProtectionMethod {
  kNack,
  kHybridNackFec,
};

// This constant can be used as input to various functions to not force the
// codec resolution.
const int kDoNotForceResolution = 0;

// Finds a suitable capture device (e.g. camera) on the current system
// and allocates it. Details about the found device are filled into the out
// parameters. If this operation fails, device_id is assigned a negative value
// and number_of_errors is incremented.
void FindCaptureDeviceOnSystem(webrtc::ViECapture* capture,
                               char* device_name,
                               const unsigned int kDeviceNameLength,
                               int* device_id,
                               webrtc::VideoCaptureModule** device_video);

// Sets up rendering in a window previously created using a Window Manager
// (See vie_window_manager_factory.h for more details on how to make one of
// those). The frame provider id is a source of video frames, for instance
// a capture device or a video channel.
void RenderInWindow(webrtc::ViERender* video_render_interface,
                    int  frame_provider_id,
                    void* os_window,
                    float z_index);

// Similar in function to RenderInWindow, this function instead renders to
// a file using a to-file-renderer. The frame provider id is a source of
// video frames, for instance a capture device or a video channel.
void RenderToFile(webrtc::ViERender* renderer_interface,
                  int frame_provider_id,
                  ViEToFileRenderer* to_file_renderer);

// Configures RTP-RTCP.
void ConfigureRtpRtcp(webrtc::ViERTP_RTCP* rtcp_interface,
                      ProtectionMethod protection_method,
                      int video_channel);

// Finds a codec in the codec list. Returns true on success, false otherwise.
// The resulting codec is filled into result on success but is zeroed out
// on failure.
bool FindSpecificCodec(webrtc::VideoCodecType of_type,
                       webrtc::ViECodec* codec_interface,
                       webrtc::VideoCodec* result);

// Sets up the provided codec with a resolution that takes individual codec
// quirks into account (except if the forced* variables are
// != kDoNotForceResolution)
void SetSuitableResolution(webrtc::VideoCodec* video_codec,
                           int forced_codec_width,
                           int forced_codec_height);

#endif  // WEBRTC_VIDEO_ENGINE_TEST_AUTO_TEST_PRIMITIVES_GENERAL_PRIMITIVES_H_
