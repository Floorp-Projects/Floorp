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
// vie_autotest_encryption.cc
//

#include "webrtc/engine_configurations.h"
#include "webrtc/video_engine/test/auto_test/interface/vie_autotest.h"
#include "webrtc/video_engine/test/auto_test/interface/vie_autotest_defines.h"

#include "webrtc/video_engine/test/libvietest/include/tb_capture_device.h"
#include "webrtc/video_engine/test/libvietest/include/tb_external_transport.h"
#include "webrtc/video_engine/test/libvietest/include/tb_interfaces.h"
#include "webrtc/video_engine/test/libvietest/include/tb_video_channel.h"

class ViEAutotestEncryption: public webrtc::Encryption
{
public:
    ViEAutotestEncryption()
    {
    }
    ~ViEAutotestEncryption()
    {
    }

    virtual void encrypt(int channel_no, unsigned char* in_data,
                         unsigned char* out_data, int bytes_in, int* bytes_out)
    {
        for (int i = 0; i < bytes_in; i++)
        {
            out_data[i] = ~in_data[i];
        }
        assert(*bytes_out >= bytes_in + 2);
        *bytes_out = bytes_in + 2;
        out_data[bytes_in] = 'a';
        out_data[bytes_in + 1] = 'b';
    }

    virtual void decrypt(int channel_no, unsigned char* in_data,
                         unsigned char* out_data, int bytes_in, int* bytes_out)
    {
        for (int i = 0; i < bytes_in - 2; i++)
        {
            out_data[i] = ~in_data[i];
        }
        assert(*bytes_out >= bytes_in - 2);
        *bytes_out = bytes_in - 2;
    }

    virtual void encrypt_rtcp(int channel_no, unsigned char* in_data,
                              unsigned char* out_data, int bytes_in,
                              int* bytes_out)
    {
        for (int i = 0; i < bytes_in; i++)
        {
            out_data[i] = ~in_data[i];
        }
        assert(*bytes_out >= bytes_in + 2);
        *bytes_out = bytes_in + 2;
        out_data[bytes_in] = 'a';
        out_data[bytes_in + 1] = 'b';
    }

    virtual void decrypt_rtcp(int channel_no, unsigned char* in_data,
                              unsigned char* out_data, int bytes_in,
                              int* bytes_out)
    {
        for (int i = 0; i < bytes_in - 2; i++)
        {
            out_data[i] = ~in_data[i];
        }
        assert(*bytes_out >= bytes_in - 2);
        *bytes_out = bytes_in - 2;
    }
};

void ViEAutoTest::ViEEncryptionStandardTest()
{
    //***************************************************************
    //	Begin create/initialize WebRTC Video Engine for testing
    //***************************************************************

    // Create VIE
    TbInterfaces ViE("ViEEncryptionStandardTest");
    // Create a video channel
    TbVideoChannel tbChannel(ViE, webrtc::kVideoCodecVP8);

    // Create a capture device
    TbCaptureDevice tbCapture(ViE);
    tbCapture.ConnectTo(tbChannel.videoChannel);

    tbChannel.StartReceive();

    tbChannel.StartSend();

    RenderCaptureDeviceAndOutputStream(&ViE, &tbChannel, &tbCapture);

    //
    // External encryption
    //
    ViEAutotestEncryption testEncryption;
    // Note(qhogpat): StartSend fails, not sure if this is intentional.
    EXPECT_NE(0, ViE.base->StartSend(tbChannel.videoChannel));
    EXPECT_EQ(0, ViE.encryption->RegisterExternalEncryption(
        tbChannel.videoChannel, testEncryption));
    ViETest::Log(
        "External encryption/decryption added, you should still see video");
    AutoTestSleep(kAutoTestSleepTimeMs);
    EXPECT_EQ(0, ViE.encryption->DeregisterExternalEncryption(
        tbChannel.videoChannel));

    //***************************************************************
    //	Testing finished. Tear down Video Engine
    //***************************************************************
}

void ViEAutoTest::ViEEncryptionExtendedTest()
{
    //***************************************************************
    //	Begin create/initialize WebRTC Video Engine for testing
    //***************************************************************

    // Create VIE
    TbInterfaces ViE("ViEEncryptionExtendedTest");
    // Create a video channel
    TbVideoChannel tbChannel(ViE, webrtc::kVideoCodecVP8);

    // Create a capture device
    TbCaptureDevice tbCapture(ViE);
    tbCapture.ConnectTo(tbChannel.videoChannel);

    tbChannel.StartReceive();
    tbChannel.StartSend();

    RenderCaptureDeviceAndOutputStream(&ViE, &tbChannel, &tbCapture);

    //***************************************************************
    //	Engine ready. Begin testing class
    //***************************************************************

    //
    // External encryption
    //
    ViEAutotestEncryption testEncryption;
    EXPECT_EQ(0, ViE.encryption->RegisterExternalEncryption(
        tbChannel.videoChannel, testEncryption));
    ViETest::Log(
        "External encryption/decryption added, you should still see video");
    AutoTestSleep(kAutoTestSleepTimeMs);
    EXPECT_EQ(0, ViE.encryption->DeregisterExternalEncryption(
        tbChannel.videoChannel));

    //***************************************************************
    //	Testing finished. Tear down Video Engine
    //***************************************************************
}

void ViEAutoTest::ViEEncryptionAPITest()
{
    //***************************************************************
    //	Begin create/initialize WebRTC Video Engine for testing
    //***************************************************************

    //***************************************************************
    //	Engine ready. Begin testing class
    //***************************************************************

    // Create VIE
    TbInterfaces ViE("ViEEncryptionAPITest");
    // Create a video channel
    TbVideoChannel tbChannel(ViE, webrtc::kVideoCodecVP8);

    // Create a capture device
    TbCaptureDevice tbCapture(ViE);
    // Connect to channel
    tbCapture.ConnectTo(tbChannel.videoChannel);

    //
    // External encryption
    //

    ViEAutotestEncryption testEncryption;
    EXPECT_EQ(0, ViE.encryption->RegisterExternalEncryption(
        tbChannel.videoChannel, testEncryption));
    EXPECT_NE(0, ViE.encryption->RegisterExternalEncryption(
        tbChannel.videoChannel, testEncryption));
    EXPECT_EQ(0, ViE.encryption->DeregisterExternalEncryption(
        tbChannel.videoChannel));
    EXPECT_EQ(0, ViE.encryption->DeregisterExternalEncryption(
        tbChannel.videoChannel));

    //***************************************************************
    //	Testing finished. Tear down Video Engine
    //***************************************************************
}
