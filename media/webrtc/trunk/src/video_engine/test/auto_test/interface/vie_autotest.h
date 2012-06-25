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
// vie_autotest.h
//

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_VIE_AUTOTEST_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_VIE_AUTOTEST_H_

#include "common_types.h"

#include "voe_base.h"
#include "voe_codec.h"
#include "voe_hardware.h"
#include "voe_audio_processing.h"

#include "vie_base.h"
#include "vie_capture.h"
#include "vie_codec.h"
#include "vie_file.h"
#include "vie_network.h"
#include "vie_render.h"
#include "vie_rtp_rtcp.h"
#include "vie_defines.h"
#include "vie_errors.h"
#include "video_render_defines.h"

#include "vie_autotest_defines.h"

#ifndef WEBRTC_ANDROID
#include <string>
#endif

class TbCaptureDevice;
class TbInterfaces;
class TbVideoChannel;
class ViEToFileRenderer;

// This class provides a bunch of methods, implemented across several .cc
// files, which runs tests on the video engine. All methods will report
// errors using standard googletest macros, except when marked otherwise.
class ViEAutoTest
{
public:
    ViEAutoTest(void* window1, void* window2);
    ~ViEAutoTest();

    // These three are special and should not be run in a googletest harness.
    // They keep track of their errors by themselves and return the number
    // of errors.
    int ViELoopbackCall();
    int ViESimulcastCall();
    int ViECustomCall();

    // All functions except the three above are meant to run in a
    // googletest harness.
    void ViEStandardTest();
    void ViEExtendedTest();
    void ViEAPITest();

    // vie_autotest_base.cc
    void ViEBaseStandardTest();
    void ViEBaseExtendedTest();
    void ViEBaseAPITest();

    // vie_autotest_capture.cc
    void ViECaptureStandardTest();
    void ViECaptureExtendedTest();
    void ViECaptureAPITest();
    void ViECaptureExternalCaptureTest();

    // vie_autotest_codec.cc
    void ViECodecStandardTest();
    void ViECodecExtendedTest();
    void ViECodecExternalCodecTest();
    void ViECodecAPITest();

    // vie_autotest_encryption.cc
    void ViEEncryptionStandardTest();
    void ViEEncryptionExtendedTest();
    void ViEEncryptionAPITest();

    // vie_autotest_file.ccs
    void ViEFileStandardTest();
    void ViEFileExtendedTest();
    void ViEFileAPITest();

    // vie_autotest_image_process.cc
    void ViEImageProcessStandardTest();
    void ViEImageProcessExtendedTest();
    void ViEImageProcessAPITest();

    // vie_autotest_network.cc
    void ViENetworkStandardTest();
    void ViENetworkExtendedTest();
    void ViENetworkAPITest();

    // vie_autotest_render.cc
    void ViERenderStandardTest();
    void ViERenderExtendedTest();
    void ViERenderAPITest();

    // vie_autotest_rtp_rtcp.cc
    void ViERtpRtcpStandardTest();
    void ViERtpRtcpExtendedTest();
    void ViERtpRtcpAPITest();

    // vie_autotest_rtp_fuzz.cc
    void ViERtpTryInjectingRandomPacketsIntoRtpStream(long rand_seed);

private:
    void PrintAudioCodec(const webrtc::CodecInst audioCodec);
    void PrintVideoCodec(const webrtc::VideoCodec videoCodec);

    // Sets up rendering so the capture device output goes to window 1 and
    // the video engine output goes to window 2.
    void RenderCaptureDeviceAndOutputStream(TbInterfaces* video_engine,
                                            TbVideoChannel* video_channel,
                                            TbCaptureDevice* capture_device);

    void* _window1;
    void* _window2;

    webrtc::VideoRenderType _renderType;
    webrtc::VideoRender* _vrm1;
    webrtc::VideoRender* _vrm2;
};

#endif  // WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_VIE_AUTOTEST_H_
