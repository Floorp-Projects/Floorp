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
// vie_autotest.cc
//

#include "webrtc/video_engine/test/auto_test/interface/vie_autotest.h"

#include <stdio.h>

#include "webrtc/engine_configurations.h"
#include "webrtc/modules/video_render/include/video_render.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/video_engine/test/auto_test/interface/vie_autotest_defines.h"
#include "webrtc/video_engine/test/auto_test/primitives/general_primitives.h"
#include "webrtc/video_engine/test/libvietest/include/tb_capture_device.h"
#include "webrtc/video_engine/test/libvietest/include/tb_interfaces.h"
#include "webrtc/video_engine/test/libvietest/include/tb_video_channel.h"

DEFINE_bool(include_timing_dependent_tests, true,
            "If true, we will include tests / parts of tests that are known "
            "to break in slow execution environments (such as valgrind).");

// ViETest implementation
FILE* ViETest::log_file_ = NULL;
char* ViETest::log_str_ = NULL;

std::string ViETest::GetResultOutputPath() {
  return webrtc::test::OutputPath();
}

// ViEAutoTest implementation
ViEAutoTest::ViEAutoTest(void* window1, void* window2) :
    _window1(window1),
    _window2(window2),
    _renderType(webrtc::kRenderDefault),
    _vrm1(webrtc::VideoRender::CreateVideoRender(
        4561, window1, false, _renderType)),
    _vrm2(webrtc::VideoRender::CreateVideoRender(
        4562, window2, false, _renderType))
{
    assert(_vrm1);
    assert(_vrm2);
}

ViEAutoTest::~ViEAutoTest()
{
    webrtc::VideoRender::DestroyVideoRender(_vrm1);
    _vrm1 = NULL;
    webrtc::VideoRender::DestroyVideoRender(_vrm2);
    _vrm2 = NULL;
}

void ViEAutoTest::ViEStandardTest()
{
    ViEBaseStandardTest();
    ViECaptureStandardTest();
    ViECodecStandardTest();
    ViEEncryptionStandardTest();
    ViEFileStandardTest();
    ViEImageProcessStandardTest();
    ViERenderStandardTest();
    ViERtpRtcpStandardTest();
}

void ViEAutoTest::ViEExtendedTest()
{
    ViEBaseExtendedTest();
    ViECaptureExtendedTest();
    ViECodecExtendedTest();
    ViEEncryptionExtendedTest();
    ViEFileExtendedTest();
    ViEImageProcessExtendedTest();
    ViERenderExtendedTest();
    ViERtpRtcpExtendedTest();
}

void ViEAutoTest::ViEAPITest()
{
    ViEBaseAPITest();
    ViECaptureAPITest();
    ViECodecAPITest();
    ViEEncryptionAPITest();
    ViEFileAPITest();
    ViEImageProcessAPITest();
    ViERenderAPITest();
    ViERtpRtcpAPITest();
}

void ViEAutoTest::PrintVideoCodec(const webrtc::VideoCodec videoCodec)
{
    ViETest::Log("Video Codec Information:");

    switch (videoCodec.codecType)
    {
        case webrtc::kVideoCodecVP8:
            ViETest::Log("\tcodecType: VP8");
            break;
        case webrtc::kVideoCodecI420:
            ViETest::Log("\tcodecType: I420");
            break;
        case webrtc::kVideoCodecRED:
            ViETest::Log("\tcodecType: RED");
            break;
        case webrtc::kVideoCodecULPFEC:
            ViETest::Log("\tcodecType: ULPFEC");
            break;
        case webrtc::kVideoCodecGeneric:
            ViETest::Log("\tcodecType: GENERIC");
            break;
        case webrtc::kVideoCodecUnknown:
            ViETest::Log("\tcodecType: UNKNOWN");
            break;
    }

    ViETest::Log("\theight: %u", videoCodec.height);
    ViETest::Log("\tmaxBitrate: %u", videoCodec.maxBitrate);
    ViETest::Log("\tmaxFramerate: %u", videoCodec.maxFramerate);
    ViETest::Log("\tminBitrate: %u", videoCodec.minBitrate);
    ViETest::Log("\tplName: %s", videoCodec.plName);
    ViETest::Log("\tplType: %u", videoCodec.plType);
    ViETest::Log("\tstartBitrate: %u", videoCodec.startBitrate);
    ViETest::Log("\twidth: %u", videoCodec.width);
    ViETest::Log("");
}

void ViEAutoTest::PrintAudioCodec(const webrtc::CodecInst audioCodec)
{
    ViETest::Log("Audio Codec Information:");
    ViETest::Log("\tchannels: %u", audioCodec.channels);
    ViETest::Log("\t: %u", audioCodec.pacsize);
    ViETest::Log("\t: %u", audioCodec.plfreq);
    ViETest::Log("\t: %s", audioCodec.plname);
    ViETest::Log("\t: %u", audioCodec.pltype);
    ViETest::Log("\t: %u", audioCodec.rate);
    ViETest::Log("");
}

void ViEAutoTest::RenderCaptureDeviceAndOutputStream(
    TbInterfaces* video_engine,
    TbVideoChannel* video_channel,
    TbCaptureDevice* capture_device) {
  RenderInWindow(
      video_engine->render, capture_device->captureId, _window1, 0);
  RenderInWindow(
      video_engine->render, video_channel->videoChannel, _window2, 1);
}
