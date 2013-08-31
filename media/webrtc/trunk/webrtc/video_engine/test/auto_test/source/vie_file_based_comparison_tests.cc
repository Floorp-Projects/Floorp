/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/test/auto_test/interface/vie_file_based_comparison_tests.h"

#include "webrtc/video_engine/test/auto_test/interface/vie_autotest_defines.h"
#include "webrtc/video_engine/test/auto_test/primitives/base_primitives.h"
#include "webrtc/video_engine/test/auto_test/primitives/framedrop_primitives.h"
#include "webrtc/video_engine/test/auto_test/primitives/general_primitives.h"
#include "webrtc/video_engine/test/libvietest/include/tb_external_transport.h"
#include "webrtc/video_engine/test/libvietest/include/tb_interfaces.h"
#include "webrtc/video_engine/test/libvietest/include/vie_external_render_filter.h"
#include "webrtc/video_engine/test/libvietest/include/vie_fake_camera.h"
#include "webrtc/video_engine/test/libvietest/include/vie_to_file_renderer.h"

bool ViEFileBasedComparisonTests::TestCallSetup(
    const std::string& i420_video_file,
    int width,
    int height,
    ViEToFileRenderer* local_file_renderer,
    ViEToFileRenderer* remote_file_renderer) {

  TbInterfaces interfaces("TestCallSetup");

  int video_channel = -1;
  EXPECT_EQ(0, interfaces.base->CreateChannel(video_channel));

  ViEFakeCamera fake_camera(interfaces.capture);
  if (!fake_camera.StartCameraInNewThread(i420_video_file,
                                          width,
                                          height)) {
    // No point in continuing if we have no proper video source
    ADD_FAILURE() << "Could not open input video " << i420_video_file <<
        ": aborting test...";
    return false;
  }
  int capture_id = fake_camera.capture_id();

  // Apparently, we need to connect external capture devices, but we should
  // not start them since the external device is not a proper device.
  EXPECT_EQ(0, interfaces.capture->ConnectCaptureDevice(
      capture_id, video_channel));

  ConfigureRtpRtcp(interfaces.rtp_rtcp, kNack, video_channel);

  webrtc::ViERender* render_interface = interfaces.render;
  webrtc::ViEImageProcess* image_process = interfaces.image_process;

  RenderToFile(render_interface, video_channel, remote_file_renderer);

  // We make a special hookup of the local renderer to use an effect filter
  // instead of using the render interface for the capture device. This way
  // we will only render frames that actually get sent.
  webrtc::ExternalRendererEffectFilter renderer_filter(local_file_renderer);
  EXPECT_EQ(0, image_process->RegisterSendEffectFilter(video_channel,
                                                       renderer_filter));

  // Run the test itself:
  const char* device_name = "Fake Capture Device";

  ::TestI420CallSetup(interfaces.codec, interfaces.video_engine,
                      interfaces.base, interfaces.network, video_channel,
                      device_name);

  EXPECT_EQ(0, render_interface->StopRender(video_channel));
  EXPECT_EQ(0, render_interface->RemoveRenderer(video_channel));

  interfaces.capture->DisconnectCaptureDevice(video_channel);

  // Stop sending data, clean up the camera thread and release the capture
  // device. Note that this all happens after StopEverything, so this
  // tests that the system doesn't mind that the external capture device sends
  // data after rendering has been stopped.
  fake_camera.StopCamera();
  EXPECT_EQ(0, image_process->DeregisterSendEffectFilter(video_channel));

  EXPECT_EQ(0, interfaces.base->DeleteChannel(video_channel));
  return true;
}

void ViEFileBasedComparisonTests::TestFullStack(
    const std::string& i420_video_file,
    int width,
    int height,
    int bit_rate_kbps,
    ProtectionMethod protection_method,
    const NetworkParameters& network,
    ViEToFileRenderer* local_file_renderer,
    ViEToFileRenderer* remote_file_renderer,
    FrameDropDetector* frame_drop_detector) {
  TbInterfaces interfaces("TestFullStack");

  // Setup camera capturing from file.
  ViEFakeCamera fake_camera(interfaces.capture);
  if (!fake_camera.StartCameraInNewThread(i420_video_file, width, height)) {
    // No point in continuing if we have no proper video source
    ADD_FAILURE() << "Could not open input video " << i420_video_file <<
        ": aborting test...";
    return;
  }
  int video_channel = -1;
  int capture_id = fake_camera.capture_id();
  EXPECT_EQ(0, interfaces.base->CreateChannel(video_channel));

  // Must set SSRC to avoid SSRC collision detection since we're sending and
  // receiving from the same machine (that would cause frames being discarded
  // and decoder reset).
  EXPECT_EQ(0, interfaces.rtp_rtcp->SetLocalSSRC(video_channel, 12345));

  EXPECT_EQ(0, interfaces.capture->ConnectCaptureDevice(
      capture_id, video_channel));
  ConfigureRtpRtcp(interfaces.rtp_rtcp, protection_method, video_channel);

  ::TestFullStack(interfaces, capture_id, video_channel, width, height,
                  bit_rate_kbps, network, frame_drop_detector,
                  remote_file_renderer, local_file_renderer);
  EXPECT_TRUE(fake_camera.StopCamera());
}
