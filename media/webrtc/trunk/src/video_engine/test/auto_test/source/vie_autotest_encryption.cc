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

#include "vie_autotest_defines.h"
#include "vie_autotest.h"
#include "engine_configurations.h"

#include "tb_capture_device.h"
#include "tb_external_transport.h"
#include "tb_interfaces.h"
#include "tb_video_channel.h"

class ViEAutotestEncryption: public webrtc::Encryption
{
public:
    ViEAutotestEncryption()
    {
    }
    ~ViEAutotestEncryption()
    {
    }

    virtual void encrypt(int channel_no, unsigned char * in_data,
                         unsigned char * out_data, int bytes_in, int* bytes_out)
    {
        for (int i = 0; i < bytes_in; i++)
        {
            out_data[i] = ~in_data[i];
        }
        *bytes_out = bytes_in + 2;
    }

    virtual void decrypt(int channel_no, unsigned char * in_data,
                         unsigned char * out_data, int bytes_in, int* bytes_out)
    {
        for (int i = 0; i < bytes_in - 2; i++)
        {
            out_data[i] = ~in_data[i];
        }
        *bytes_out = bytes_in - 2;
    }

    virtual void encrypt_rtcp(int channel_no, unsigned char * in_data,
                              unsigned char * out_data, int bytes_in,
                              int* bytes_out)
    {
        for (int i = 0; i < bytes_in; i++)
        {
            out_data[i] = ~in_data[i];
        }
        *bytes_out = bytes_in + 2;
    }

    virtual void decrypt_rtcp(int channel_no, unsigned char * in_data,
                              unsigned char * out_data, int bytes_in,
                              int* bytes_out)
    {
        for (int i = 0; i < bytes_in - 2; i++)
        {
            out_data[i] = ~in_data[i];
        }
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

#ifdef WEBRTC_SRTP
    //***************************************************************
    //	Engine ready. Begin testing class
    //***************************************************************

    //
    // SRTP
    //
    unsigned char srtpKey1[30] =
    {   0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3,
        4, 5, 6, 7, 8, 9};

    // Encryption only
    EXPECT_EQ(0, ViE.encryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthNull, 0, 0, webrtc::kEncryption, srtpKey1));
    EXPECT_EQ(0, ViE.encryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthNull, 0, 0, webrtc::kEncryption, srtpKey1));
    ViETest::Log("SRTP encryption only");
    AutoTestSleep(KAutoTestSleepTimeMs);
    EXPECT_EQ(0, ViE.encryption->DisableSRTPReceive(tbChannel.videoChannel));
    EXPECT_EQ(0, ViE.encryption->DisableSRTPSend(tbChannel.videoChannel));

    // Authentication only
    EXPECT_EQ(0, ViE.encryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherNull, 0, webrtc::kAuthHmacSha1,
        20, 4, webrtc::kAuthentication, srtpKey1));
    EXPECT_EQ(0, ViE.encryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherNull, 0, webrtc::kAuthHmacSha1,
        20, 4, webrtc::kAuthentication, srtpKey1));

    ViETest::Log("SRTP authentication only");
    AutoTestSleep(KAutoTestSleepTimeMs);
    EXPECT_EQ(0, ViE.encryption->DisableSRTPReceive(tbChannel.videoChannel));
    EXPECT_EQ(0, ViE.encryption->DisableSRTPSend(tbChannel.videoChannel));

    // Full protection
    EXPECT_EQ(0, ViE.encryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kEncryptionAndAuthentication,
        srtpKey1));
    EXPECT_EQ(0, ViE.encryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kEncryptionAndAuthentication,
        srtpKey1));

    ViETest::Log("SRTP full protection");
    AutoTestSleep(KAutoTestSleepTimeMs);
    EXPECT_EQ(0, ViE.encryption->DisableSRTPReceive(tbChannel.videoChannel));
    EXPECT_EQ(0, ViE.encryption->DisableSRTPSend(tbChannel.videoChannel));
#endif  // WEBRTC_SRTP

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
    AutoTestSleep(KAutoTestSleepTimeMs);
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

#ifdef WEBRTC_SRTP

    //
    // SRTP
    //
    unsigned char srtpKey1[30] =
    {   0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3,
        4, 5, 6, 7, 8, 9};
    unsigned char srtpKey2[30] =
    {   9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 9, 8, 7, 6,
        5, 4, 3, 2, 1, 0};
    // NULL
    EXPECT_EQ(0, ViE.encryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherNull, 0, webrtc::kAuthNull, 0, 0,
        webrtc::kNoProtection, srtpKey1));
    EXPECT_EQ(0, ViE.encryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherNull, 0, webrtc::kAuthNull, 0, 0,
        webrtc::kNoProtection, srtpKey1));

    ViETest::Log("SRTP NULL encryption/authentication");
    AutoTestSleep(KAutoTestSleepTimeMs);
    EXPECT_EQ(0, ViE.encryption->DisableSRTPReceive(tbChannel.videoChannel));
    EXPECT_EQ(0, ViE.encryption->DisableSRTPSend(tbChannel.videoChannel));

    // Encryption only
    EXPECT_EQ(0, ViE.encryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthNull, 0, 0, webrtc::kEncryption, srtpKey1));
    EXPECT_EQ(0, ViE.encryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthNull, 0, 0, webrtc::kEncryption, srtpKey1));

    ViETest::Log("SRTP encryption only");
    AutoTestSleep(KAutoTestSleepTimeMs);
    EXPECT_EQ(0, ViE.encryption->DisableSRTPReceive(tbChannel.videoChannel));
    EXPECT_EQ(0, ViE.encryption->DisableSRTPSend(tbChannel.videoChannel));

    // Authentication only
    EXPECT_EQ(0, ViE.encryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherNull, 0, webrtc::kAuthHmacSha1,
        20, 4, webrtc::kAuthentication, srtpKey1));
    EXPECT_EQ(0, ViE.encryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherNull, 0, webrtc::kAuthHmacSha1,
        20, 4, webrtc::kAuthentication, srtpKey1));

    ViETest::Log("SRTP authentication only");
    AutoTestSleep(KAutoTestSleepTimeMs);
    EXPECT_EQ(0, ViE.encryption->DisableSRTPReceive(tbChannel.videoChannel));
    EXPECT_EQ(0, ViE.encryption->DisableSRTPSend(tbChannel.videoChannel));

    // Full protection
    EXPECT_EQ(0, ViE.encryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kEncryptionAndAuthentication,
        srtpKey1));
    EXPECT_EQ(0, ViE.encryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kEncryptionAndAuthentication,
        srtpKey1));

    ViETest::Log("SRTP full protection");
    AutoTestSleep(KAutoTestSleepTimeMs);
    EXPECT_EQ(0, ViE.encryption->DisableSRTPReceive(tbChannel.videoChannel));
    EXPECT_EQ(0, ViE.encryption->DisableSRTPSend(tbChannel.videoChannel));

    // Change receive key, but not send key...
    EXPECT_EQ(0, ViE.encryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kEncryptionAndAuthentication,
        srtpKey2));

    EXPECT_EQ(0, ViE.encryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kEncryptionAndAuthentication,
        srtpKey1));

    ViETest::Log(
        "\nSRTP receive key changed, you should not see any remote images");
    AutoTestSleep(KAutoTestSleepTimeMs);

    // Change send key too
    EXPECT_EQ(0, ViE.encryption->DisableSRTPSend(tbChannel.videoChannel));
    EXPECT_EQ(0, ViE.encryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kEncryptionAndAuthentication,
        srtpKey2));

    ViETest::Log("\nSRTP send key changed too, you should see remote video "
                 "again with some decoding artefacts at start");
    AutoTestSleep(KAutoTestSleepTimeMs);
    EXPECT_EQ(0, ViE.encryption->DisableSRTPReceive(tbChannel.videoChannel));

    // Disable receive, keep send
    ViETest::Log("SRTP receive disabled , you shouldn't see any video");
    AutoTestSleep(KAutoTestSleepTimeMs);
    EXPECT_EQ(0, ViE.encryption->DisableSRTPSend(tbChannel.videoChannel));

#endif //WEBRTC_SRTP
    //
    // External encryption
    //
    ViEAutotestEncryption testEncryption;
    EXPECT_EQ(0, ViE.encryption->RegisterExternalEncryption(
        tbChannel.videoChannel, testEncryption));
    ViETest::Log(
        "External encryption/decryption added, you should still see video");
    AutoTestSleep(KAutoTestSleepTimeMs);
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

#ifdef WEBRTC_SRTP
    unsigned char srtpKey[30] =
    {   0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3,
        4, 5, 6, 7, 8, 9};

    //
    // EnableSRTPSend and DisableSRTPSend
    //

    // Incorrect input argument, complete protection not enabled
    EXPECT_NE(0, ViE.encryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kNoProtection, srtpKey));
    EXPECT_NE(0, ViE.encryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kEncryption, srtpKey));
    EXPECT_NE(0, ViE.encryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kAuthentication, srtpKey));

    // Incorrect cipher key length
    EXPECT_NE(0, ViE.encryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 15,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kEncryptionAndAuthentication,
        srtpKey));
    EXPECT_NE(0, ViE.encryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 257,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kEncryptionAndAuthentication,
        srtpKey));
    EXPECT_NE(0, ViE.encryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherNull, 15, webrtc::kAuthHmacSha1,
        20, 4, webrtc::kEncryptionAndAuthentication, srtpKey));

    EXPECT_NE(0, ViE.encryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherNull, 257, webrtc::kAuthHmacSha1,
        20, 4, webrtc::kEncryptionAndAuthentication, srtpKey));

    // Incorrect auth key length
    EXPECT_NE(0, ViE.encryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode,
        30, webrtc::kAuthHmacSha1, 21, 4, webrtc::kEncryptionAndAuthentication,
        srtpKey));
    EXPECT_NE(0, ViE.encryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthNull, 257, 4, webrtc::kEncryptionAndAuthentication,
        srtpKey));
    EXPECT_NE(0, ViE.encryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 21, webrtc::kEncryptionAndAuthentication,
        srtpKey));
    EXPECT_NE(0, ViE.encryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthNull, 20, 13, webrtc::kEncryptionAndAuthentication,
        srtpKey));

    // NULL input
    EXPECT_NE(0, ViE.encryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kEncryptionAndAuthentication,
        NULL));

    // Double enable and disable
    EXPECT_EQ(0, ViE.encryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kEncryptionAndAuthentication,
        srtpKey));
    EXPECT_NE(0, ViE.encryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kEncryptionAndAuthentication,
        srtpKey));

    // Note(qhogpat): the second check is likely incorrect.
    EXPECT_EQ(0, ViE.encryption->DisableSRTPSend(tbChannel.videoChannel));
    EXPECT_EQ(0, ViE.encryption->DisableSRTPSend(tbChannel.videoChannel));

    // No protection
    EXPECT_EQ(0, ViE.encryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherNull, 0, webrtc::kAuthNull, 0, 0,
        webrtc::kNoProtection, srtpKey));
    EXPECT_EQ(0, ViE.encryption->DisableSRTPSend(tbChannel.videoChannel));

    // Authentication only
    EXPECT_EQ(0, ViE.encryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherNull, 0, webrtc::kAuthHmacSha1,
        20, 4, webrtc::kAuthentication, srtpKey));
    EXPECT_EQ(0, ViE.encryption->DisableSRTPSend(tbChannel.videoChannel));
    EXPECT_EQ(0, ViE.encryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherNull, 0, webrtc::kAuthHmacSha1,
        1, 4, webrtc::kAuthentication, srtpKey));
    EXPECT_EQ(0, ViE.encryption->DisableSRTPSend(tbChannel.videoChannel));
    EXPECT_EQ(0, ViE.encryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherNull, 0, webrtc::kAuthHmacSha1,
        20, 20, webrtc::kAuthentication, srtpKey));
    EXPECT_EQ(0, ViE.encryption->DisableSRTPSend(tbChannel.videoChannel));
    EXPECT_EQ(0, ViE.encryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherNull, 0, webrtc::kAuthHmacSha1,
        1, 1, webrtc::kAuthentication, srtpKey));
    EXPECT_EQ(0, ViE.encryption->DisableSRTPSend(tbChannel.videoChannel));

    // Encryption only
    EXPECT_EQ(0, ViE.encryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthNull, 0, 0, webrtc::kEncryption, srtpKey));
    EXPECT_EQ(0, ViE.encryption->DisableSRTPSend(tbChannel.videoChannel));
    EXPECT_EQ(0, ViE.encryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 16,
        webrtc::kAuthNull, 0, 0, webrtc::kEncryption, srtpKey));
    EXPECT_EQ(0, ViE.encryption->DisableSRTPSend(tbChannel.videoChannel));

    // Full protection
    EXPECT_EQ(0, ViE.encryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kEncryptionAndAuthentication,
        srtpKey));
    EXPECT_EQ(0, ViE.encryption->DisableSRTPSend(tbChannel.videoChannel));

    //
    // EnableSRTPReceive and DisableSRTPReceive
    //

    // Incorrect input argument, complete protection not enabled
    EXPECT_NE(0, ViE.encryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kNoProtection, srtpKey));
    EXPECT_NE(0, ViE.encryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kEncryption, srtpKey));
    EXPECT_NE(0, ViE.encryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kAuthentication, srtpKey));

    // Incorrect cipher key length
    EXPECT_NE(0, ViE.encryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 15,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kEncryptionAndAuthentication,
        srtpKey));
    EXPECT_NE(0, ViE.encryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 257,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kEncryptionAndAuthentication,
        srtpKey));
    EXPECT_NE(0, ViE.encryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherNull, 15, webrtc::kAuthHmacSha1,
        20, 4, webrtc::kEncryptionAndAuthentication, srtpKey));
    EXPECT_NE(0, ViE.encryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherNull, 257, webrtc::kAuthHmacSha1,
        20, 4, webrtc::kEncryptionAndAuthentication, srtpKey));

    // Incorrect auth key length
    EXPECT_NE(0, ViE.encryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 21, 4, webrtc::kEncryptionAndAuthentication,
        srtpKey));
    EXPECT_NE(0, ViE.encryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthNull, 257, 4, webrtc::kEncryptionAndAuthentication,
        srtpKey));
    EXPECT_NE(0, ViE.encryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 21, webrtc::kEncryptionAndAuthentication,
        srtpKey));
    EXPECT_NE(0, ViE.encryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthNull, 20, 13, webrtc::kEncryptionAndAuthentication,
        srtpKey));

    // NULL input
    EXPECT_NE(0, ViE.encryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kEncryptionAndAuthentication,
        NULL));

    // Double enable and disable
    EXPECT_EQ(0, ViE.encryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kEncryptionAndAuthentication,
        srtpKey));
    EXPECT_NE(0, ViE.encryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kEncryptionAndAuthentication,
        srtpKey));
    EXPECT_EQ(0, ViE.encryption->DisableSRTPSend(tbChannel.videoChannel));
    EXPECT_EQ(0, ViE.encryption->DisableSRTPSend(tbChannel.videoChannel));

    // No protection
    EXPECT_EQ(0, ViE.encryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherNull, 0, webrtc::kAuthNull, 0, 0,
        webrtc::kNoProtection, srtpKey));
    EXPECT_EQ(0, ViE.encryption->DisableSRTPReceive(tbChannel.videoChannel));

    // Authentication only
    EXPECT_EQ(0, ViE.encryption->DisableSRTPReceive(tbChannel.videoChannel));
    EXPECT_EQ(0, ViE.encryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherNull, 0, webrtc::kAuthHmacSha1,
        1, 4, webrtc::kAuthentication, srtpKey));
    EXPECT_EQ(0, ViE.encryption->DisableSRTPReceive(tbChannel.videoChannel));
    EXPECT_EQ(0, ViE.encryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherNull, 0,
        webrtc::kAuthHmacSha1, 20, 20, webrtc::kAuthentication, srtpKey));
    EXPECT_EQ(0, ViE.encryption->DisableSRTPReceive(tbChannel.videoChannel));
    EXPECT_EQ(0, ViE.encryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherNull, 0, webrtc::kAuthHmacSha1,
        1, 1, webrtc::kAuthentication, srtpKey));
    EXPECT_EQ(0, ViE.encryption->DisableSRTPReceive(tbChannel.videoChannel));

    // Encryption only
    EXPECT_EQ(0, ViE.encryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthNull, 0, 0, webrtc::kEncryption, srtpKey));
    EXPECT_EQ(0, ViE.encryption->DisableSRTPReceive(tbChannel.videoChannel));
    EXPECT_EQ(0, ViE.encryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 16,
        webrtc::kAuthNull, 0, 0, webrtc::kEncryption, srtpKey));
    EXPECT_EQ(0, ViE.encryption->DisableSRTPReceive(tbChannel.videoChannel));

    // Full protection
    EXPECT_EQ(0, ViE.encryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kEncryptionAndAuthentication,
        srtpKey));
    EXPECT_EQ(0, ViE.encryption->DisableSRTPReceive(tbChannel.videoChannel));
#endif //WEBRTC_SRTP
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
