/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "general_primitives.h"

#include "video_capture_factory.h"
#include "vie_autotest.h"
#include "vie_autotest_defines.h"
#include "vie_to_file_renderer.h"

void FindCaptureDeviceOnSystem(webrtc::ViECapture* capture,
                               char* device_name,
                               unsigned int device_name_length,
                               int* device_id,
                               webrtc::VideoCaptureModule** device_video) {

  bool capture_device_set = false;
  webrtc::VideoCaptureModule::DeviceInfo *dev_info =
      webrtc::VideoCaptureFactory::CreateDeviceInfo(0);

  const unsigned int kMaxUniqueIdLength = 256;
  char unique_id[kMaxUniqueIdLength];
  memset(unique_id, 0, kMaxUniqueIdLength);

  for (unsigned int i = 0; i < dev_info->NumberOfDevices(); i++) {
    EXPECT_EQ(0, dev_info->GetDeviceName(i, device_name, device_name_length,
                                         unique_id, kMaxUniqueIdLength));

    *device_video = webrtc::VideoCaptureFactory::Create(4571, unique_id);
    EXPECT_TRUE(*device_video != NULL);

    (*device_video)->AddRef();

    int error = capture->AllocateCaptureDevice(**device_video, *device_id);
    if (error == 0) {
      ViETest::Log("Using capture device: %s, captureId: %d.",
                   device_name, *device_id);
      capture_device_set = true;
      break;
    } else {
      (*device_video)->Release();
      (*device_video) = NULL;
    }
  }
  delete dev_info;
  EXPECT_TRUE(capture_device_set) << "Found no suitable camera on your system.";
}

void RenderInWindow(webrtc::ViERender* video_render_interface,
                    int frame_provider_id,
                    void* os_window,
                    float z_index) {
  EXPECT_EQ(0,
            video_render_interface->AddRenderer(frame_provider_id, os_window,
                                                z_index, 0.0, 0.0, 1.0, 1.0));
  EXPECT_EQ(0, video_render_interface->StartRender(frame_provider_id));
}

void RenderToFile(webrtc::ViERender* renderer_interface,
                  int frame_provider_id,
                  ViEToFileRenderer *to_file_renderer) {
  EXPECT_EQ(0, renderer_interface->AddRenderer(
      frame_provider_id, webrtc::kVideoI420, to_file_renderer));
  EXPECT_EQ(0, renderer_interface->StartRender(frame_provider_id));
}

void StopAndRemoveRenderers(webrtc::ViEBase* base_interface,
                            webrtc::ViERender* render_interface,
                            int channel_id,
                            int capture_id) {
  EXPECT_EQ(0, render_interface->StopRender(channel_id));
  EXPECT_EQ(0, render_interface->RemoveRenderer(channel_id));
  EXPECT_EQ(0, render_interface->RemoveRenderer(capture_id));
}

void ConfigureRtpRtcp(webrtc::ViERTP_RTCP* rtcp_interface,
                      int video_channel) {
  EXPECT_EQ(0, rtcp_interface->SetRTCPStatus(video_channel,
                                             webrtc::kRtcpCompound_RFC4585));
  EXPECT_EQ(0, rtcp_interface->SetKeyFrameRequestMethod(
      video_channel, webrtc::kViEKeyFrameRequestPliRtcp));
  EXPECT_EQ(0, rtcp_interface->SetTMMBRStatus(video_channel, true));
}

bool FindSpecificCodec(webrtc::VideoCodecType of_type,
                       webrtc::ViECodec* codec_interface,
                       webrtc::VideoCodec* result) {

  memset(result, 0, sizeof(webrtc::VideoCodec));

  for (int i = 0; i < codec_interface->NumberOfCodecs(); i++) {
    webrtc::VideoCodec codec;
    memset(&codec, 0, sizeof(webrtc::VideoCodec));
    if (codec_interface->GetCodec(i, codec) != 0) {
      return false;
    }
    if (codec.codecType == of_type) {
      // Done
      *result = codec;
      return true;
    }
  }
  // Didn't find it
  return false;
}

void SetSuitableResolution(webrtc::VideoCodec* video_codec,
                           int forced_codec_width,
                           int forced_codec_height) {
  if (forced_codec_width != kDoNotForceResolution &&
      forced_codec_height != kDoNotForceResolution) {
    video_codec->width = forced_codec_width;
    video_codec->height = forced_codec_height;
  } else if (video_codec->codecType == webrtc::kVideoCodecI420) {
    // I420 is very bandwidth heavy, so limit it here.
    video_codec->width = 176;
    video_codec->height = 144;
  } else {
    // Otherwise go with 640x480.
    video_codec->width = 640;
    video_codec->height = 480;
  }
}
