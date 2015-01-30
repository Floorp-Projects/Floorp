/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

//
// vie_autotest_image_process.cc
//

// Settings
#include "webrtc/engine_configurations.h"
#include "webrtc/video_engine/test/auto_test/interface/vie_autotest.h"
#include "webrtc/video_engine/test/auto_test/interface/vie_autotest_defines.h"

#include "webrtc/video_engine/test/libvietest/include/tb_capture_device.h"
#include "webrtc/video_engine/test/libvietest/include/tb_interfaces.h"
#include "webrtc/video_engine/test/libvietest/include/tb_video_channel.h"

class MyEffectFilter: public webrtc::ViEEffectFilter
{
public:
    MyEffectFilter() {}

    ~MyEffectFilter() {}

    virtual int Transform(int size,
                          unsigned char* frame_buffer,
                          int64_t ntp_time_ms,
                          unsigned int timestamp,
                          unsigned int width,
                          unsigned int height)
    {
        // Black and white
        memset(frame_buffer + (2 * size) / 3, 0x7f, size / 3);
        return 0;
    }
};

void ViEAutoTest::ViEImageProcessStandardTest()
{
    //***************************************************************
    //	Begin create/initialize WebRTC Video Engine for testing
    //***************************************************************
    int rtpPort = 6000;
    // Create VIE
    TbInterfaces ViE("ViEImageProcessAPITest");
    // Create a video channel
    TbVideoChannel tbChannel(ViE, webrtc::kVideoCodecVP8);
    // Create a capture device
    TbCaptureDevice tbCapture(ViE);

    tbCapture.ConnectTo(tbChannel.videoChannel);
    tbChannel.StartReceive(rtpPort);
    tbChannel.StartSend(rtpPort);

    MyEffectFilter effectFilter;

    RenderCaptureDeviceAndOutputStream(&ViE, &tbChannel, &tbCapture);

    ViETest::Log("Capture device is renderered in Window 1");
    ViETest::Log("Remote stream is renderered in Window 2");
    AutoTestSleep(kAutoTestSleepTimeMs);

    //***************************************************************
    //	Engine ready. Begin testing class
    //***************************************************************


    EXPECT_EQ(0, ViE.image_process->RegisterCaptureEffectFilter(
        tbCapture.captureId, effectFilter));

    ViETest::Log("Black and white filter registered for capture device, "
                 "affects both windows");
    AutoTestSleep(kAutoTestSleepTimeMs);

    EXPECT_EQ(0, ViE.image_process->DeregisterCaptureEffectFilter(
        tbCapture.captureId));

    EXPECT_EQ(0, ViE.image_process->RegisterRenderEffectFilter(
        tbChannel.videoChannel, effectFilter));

    ViETest::Log("Remove capture effect filter, adding filter for incoming "
                 "stream");
    ViETest::Log("Only Window 2 should be black and white");
    AutoTestSleep(kAutoTestSleepTimeMs);

    EXPECT_EQ(0, ViE.render->StopRender(tbCapture.captureId));
    EXPECT_EQ(0, ViE.render->RemoveRenderer(tbCapture.captureId));

    int rtpPort2 = rtpPort + 100;
    // Create a video channel
    TbVideoChannel tbChannel2(ViE, webrtc::kVideoCodecVP8);

    tbCapture.ConnectTo(tbChannel2.videoChannel);
    tbChannel2.StartReceive(rtpPort2);
    tbChannel2.StartSend(rtpPort2);

    EXPECT_EQ(0, ViE.render->AddRenderer(
        tbChannel2.videoChannel, _window1, 1, 0.0, 0.0, 1.0, 1.0));
    EXPECT_EQ(0, ViE.render->StartRender(tbChannel2.videoChannel));
    EXPECT_EQ(0, ViE.image_process->DeregisterRenderEffectFilter(
        tbChannel.videoChannel));

    ViETest::Log("Local renderer removed, added new channel and rendering in "
                 "Window1.");

    EXPECT_EQ(0, ViE.image_process->RegisterCaptureEffectFilter(
        tbCapture.captureId, effectFilter));

    ViETest::Log("Black and white filter registered for capture device, "
                 "affects both windows");
    AutoTestSleep(kAutoTestSleepTimeMs);

    EXPECT_EQ(0, ViE.image_process->DeregisterCaptureEffectFilter(
        tbCapture.captureId));

    EXPECT_EQ(0, ViE.image_process->RegisterSendEffectFilter(
        tbChannel.videoChannel, effectFilter));

    ViETest::Log("Capture filter removed.");
    ViETest::Log("Black and white filter registered for one channel, Window2 "
                 "should be black and white");
    AutoTestSleep(kAutoTestSleepTimeMs);

    EXPECT_EQ(0, ViE.image_process->DeregisterSendEffectFilter(
        tbChannel.videoChannel));

    //***************************************************************
    //	Testing finished. Tear down Video Engine
    //***************************************************************
}

void ViEAutoTest::ViEImageProcessExtendedTest()
{
}

void ViEAutoTest::ViEImageProcessAPITest()
{
    TbInterfaces ViE("ViEImageProcessAPITest");
    TbVideoChannel tbChannel(ViE, webrtc::kVideoCodecVP8);
    TbCaptureDevice tbCapture(ViE);

    tbCapture.ConnectTo(tbChannel.videoChannel);

    MyEffectFilter effectFilter;

    //
    // Capture effect filter
    //
    // Add effect filter
    EXPECT_EQ(0, ViE.image_process->RegisterCaptureEffectFilter(
        tbCapture.captureId, effectFilter));
    // Add again -> error
    EXPECT_NE(0, ViE.image_process->RegisterCaptureEffectFilter(
        tbCapture.captureId, effectFilter));
    EXPECT_EQ(0, ViE.image_process->DeregisterCaptureEffectFilter(
        tbCapture.captureId));
    EXPECT_EQ(0, ViE.image_process->DeregisterCaptureEffectFilter(
        tbCapture.captureId));

    // Non-existing capture device
    EXPECT_NE(0, ViE.image_process->RegisterCaptureEffectFilter(
        tbChannel.videoChannel, effectFilter));

    //
    // Render effect filter
    //
    EXPECT_EQ(0, ViE.image_process->RegisterRenderEffectFilter(
        tbChannel.videoChannel, effectFilter));
    EXPECT_NE(0, ViE.image_process->RegisterRenderEffectFilter(
        tbChannel.videoChannel, effectFilter));
    EXPECT_EQ(0, ViE.image_process->DeregisterRenderEffectFilter(
        tbChannel.videoChannel));
    EXPECT_EQ(0, ViE.image_process->DeregisterRenderEffectFilter(
        tbChannel.videoChannel));

    // Non-existing channel id
    EXPECT_NE(0, ViE.image_process->RegisterRenderEffectFilter(
        tbCapture.captureId, effectFilter));

    //
    // Send effect filter
    //
    EXPECT_EQ(0, ViE.image_process->RegisterSendEffectFilter(
        tbChannel.videoChannel, effectFilter));
    EXPECT_NE(0, ViE.image_process->RegisterSendEffectFilter(
        tbChannel.videoChannel, effectFilter));
    EXPECT_EQ(0, ViE.image_process->DeregisterSendEffectFilter(
        tbChannel.videoChannel));
    EXPECT_EQ(0, ViE.image_process->DeregisterSendEffectFilter(
        tbChannel.videoChannel));
    EXPECT_NE(0, ViE.image_process->RegisterSendEffectFilter(
        tbCapture.captureId, effectFilter));

    //
    // Deflickering
    //
    EXPECT_EQ(0, ViE.image_process->EnableDeflickering(
        tbCapture.captureId, true));
    EXPECT_NE(0, ViE.image_process->EnableDeflickering(
        tbCapture.captureId, true));
    EXPECT_EQ(0, ViE.image_process->EnableDeflickering(
        tbCapture.captureId, false));
    EXPECT_NE(0, ViE.image_process->EnableDeflickering(
        tbCapture.captureId, false));
    EXPECT_NE(0, ViE.image_process->EnableDeflickering(
        tbChannel.videoChannel, true));

    //
    // Color enhancement
    //
    EXPECT_EQ(0, ViE.image_process->EnableColorEnhancement(
        tbChannel.videoChannel, false));
    EXPECT_EQ(0, ViE.image_process->EnableColorEnhancement(
        tbChannel.videoChannel, true));
    EXPECT_EQ(0, ViE.image_process->EnableColorEnhancement(
        tbChannel.videoChannel, false));
    EXPECT_NE(0, ViE.image_process->EnableColorEnhancement(
        tbCapture.captureId, true));
}
