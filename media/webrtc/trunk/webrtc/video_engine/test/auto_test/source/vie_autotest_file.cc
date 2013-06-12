/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "vie_autotest_defines.h"
#include "vie_autotest.h"
#include "engine_configurations.h"

#include "testsupport/fileutils.h"
#include "tb_interfaces.h"
#include "tb_capture_device.h"

#include "voe_codec.h"

#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/test/channel_transport/include/channel_transport.h"

class ViEAutotestFileObserver: public webrtc::ViEFileObserver
{
public:
    ViEAutotestFileObserver() {};
    ~ViEAutotestFileObserver() {};

    void PlayFileEnded(const int32_t fileId)
    {
        ViETest::Log("PlayFile ended");
    }
};

void ViEAutoTest::ViEFileStandardTest()
{
#ifdef WEBRTC_VIDEO_ENGINE_FILE_API
    if (!FLAGS_include_timing_dependent_tests) {
      ViETest::Log("Running in slow execution environment: skipping test...\n");
      return;
    }

    //***************************************************************
    // Begin create/initialize WebRTC Video Engine for testing
    //***************************************************************
    {
        ViETest::Log("Starting a loopback call...");

        TbInterfaces interfaces("ViEFileStandardTest");

        webrtc::VideoEngine* ptrViE = interfaces.video_engine;
        webrtc::ViEBase* ptrViEBase = interfaces.base;
        webrtc::ViECapture* ptrViECapture = interfaces.capture;
        webrtc::ViERender* ptrViERender = interfaces.render;
        webrtc::ViECodec* ptrViECodec = interfaces.codec;
        webrtc::ViERTP_RTCP* ptrViERtpRtcp = interfaces.rtp_rtcp;
        webrtc::ViENetwork* ptrViENetwork = interfaces.network;

        TbCaptureDevice captureDevice = TbCaptureDevice(interfaces);
        int captureId = captureDevice.captureId;

        int videoChannel = -1;
        EXPECT_EQ(0, ptrViEBase->CreateChannel(videoChannel));
        EXPECT_EQ(0, ptrViECapture->ConnectCaptureDevice(
            captureId, videoChannel));

        EXPECT_EQ(0, ptrViERtpRtcp->SetRTCPStatus(
            videoChannel, webrtc::kRtcpCompound_RFC4585));
        EXPECT_EQ(0, ptrViERtpRtcp->SetKeyFrameRequestMethod(
            videoChannel, webrtc::kViEKeyFrameRequestPliRtcp));
        EXPECT_EQ(0, ptrViERtpRtcp->SetTMMBRStatus(videoChannel, true));

        EXPECT_EQ(0, ptrViERender->AddRenderer(
            captureId, _window1, 0, 0.0, 0.0, 1.0, 1.0));
        EXPECT_EQ(0, ptrViERender->AddRenderer(
            videoChannel, _window2, 1, 0.0, 0.0, 1.0, 1.0));
        EXPECT_EQ(0, ptrViERender->StartRender(captureId));
        EXPECT_EQ(0, ptrViERender->StartRender(videoChannel));

        webrtc::VideoCodec videoCodec;
        memset(&videoCodec, 0, sizeof(webrtc::VideoCodec));
        for (int idx = 0; idx < ptrViECodec->NumberOfCodecs(); idx++)
        {
            EXPECT_EQ(0, ptrViECodec->GetCodec(idx, videoCodec));
            EXPECT_EQ(0, ptrViECodec->SetReceiveCodec(videoChannel,
                                                      videoCodec));
        }

        // Find the codec used for encoding the channel
        for (int idx = 0; idx < ptrViECodec->NumberOfCodecs(); idx++)
        {
            EXPECT_EQ(0, ptrViECodec->GetCodec(idx, videoCodec));
            if (videoCodec.codecType == webrtc::kVideoCodecVP8)
            {
                EXPECT_EQ(0, ptrViECodec->SetSendCodec(videoChannel, videoCodec));
                break;
            }
        }
        // Find the codec used for recording.
        for (int idx = 0; idx < ptrViECodec->NumberOfCodecs(); idx++)
        {
            EXPECT_EQ(0, ptrViECodec->GetCodec(idx, videoCodec));
            if (videoCodec.codecType == webrtc::kVideoCodecI420)
            {
                break;
            }
        }
        const char* ipAddress = "127.0.0.1";
        const unsigned short rtpPort = 6000;

        webrtc::scoped_ptr<webrtc::test::VideoChannelTransport>
            video_channel_transport(
                new webrtc::test::VideoChannelTransport(ptrViENetwork,
                                                        videoChannel));

        EXPECT_EQ(0, video_channel_transport->SetSendDestination(ipAddress,
                                                                 rtpPort));
        EXPECT_EQ(0, video_channel_transport->SetLocalReceiver(rtpPort));

        EXPECT_EQ(0, ptrViEBase->StartReceive(videoChannel));
        EXPECT_EQ(0, ptrViEBase->StartSend(videoChannel));
        webrtc::ViEFile* ptrViEFile = webrtc::ViEFile::GetInterface(ptrViE);
        EXPECT_TRUE(ptrViEFile != NULL);

        webrtc::VoiceEngine* ptrVEEngine = webrtc::VoiceEngine::Create();
        webrtc::VoEBase* ptrVEBase = webrtc::VoEBase::GetInterface(ptrVEEngine);
        ptrVEBase->Init();

        int audioChannel = ptrVEBase->CreateChannel();
        ptrViEBase->SetVoiceEngine(ptrVEEngine);
        ptrViEBase->ConnectAudioChannel(videoChannel, audioChannel);

        webrtc::CodecInst audioCodec;
        webrtc::VoECodec* ptrVECodec =
            webrtc::VoECodec::GetInterface(ptrVEEngine);
        for (int index = 0; index < ptrVECodec->NumOfCodecs(); index++)
        {
            ptrVECodec->GetCodec(index, audioCodec);
            if (0 == strcmp(audioCodec.plname, "PCMU") || 0
                == strcmp(audioCodec.plname, "PCMA"))
            {
                break; // these two types are allowed as avi recording formats
            }
        }

        webrtc::CodecInst audioCodec2;

        //***************************************************************
        // Engine ready. Begin testing class
        //***************************************************************

        // Call started
        ViETest::Log("Call started\nYou should see local preview from camera\n"
                     "in window 1 and the remote video in window 2.");
        AutoTestSleep(2000);

        const int RENDER_TIMEOUT = 1000;
        const int TEST_SPACING = 1000;
        const int VIDEO_LENGTH = 5000;

        const std::string renderStartImage = webrtc::test::ResourcePath(
            "video_engine/renderStartImage", "jpg");
        const std::string renderTimeoutFile = webrtc::test::ResourcePath(
            "video_engine/renderTimeoutImage", "jpg");

        const std::string output = webrtc::test::OutputPath();
        const std::string snapshotCaptureDeviceFileName =
            output + "snapshotCaptureDevice.jpg";
        const std::string incomingVideo = output + "incomingVideo.avi";
        const std::string outgoingVideo = output + "outgoingVideo.avi";
        const std::string snapshotRenderFileName =
            output + "snapshotRenderer.jpg";

        webrtc::ViEPicture capturePicture;
        webrtc::ViEPicture renderPicture;

        ViEAutotestFileObserver fileObserver;
        int fileId;

        AutoTestSleep(TEST_SPACING);

        // Test debug information recording.
        EXPECT_EQ(0, ptrViEFile->StartDebugRecording(videoChannel,
            (webrtc::test::OutputPath() + "vie_autotest_debug.yuv").c_str()));

        // testing StartRecordIncomingVideo and StopRecordIncomingVideo
        {
            ViETest::Log("Recording incoming video (currently no audio) for %d "
                         "seconds", VIDEO_LENGTH);

            EXPECT_EQ(0, ptrViEFile->StartRecordIncomingVideo(
                videoChannel, incomingVideo.c_str(), webrtc::NO_AUDIO,
                audioCodec2, videoCodec));

            AutoTestSleep(VIDEO_LENGTH);
            ViETest::Log("Stop recording incoming video");

            EXPECT_EQ(0, ptrViEFile->StopRecordIncomingVideo(videoChannel));
            ViETest::Log("Done\n");
        }

        AutoTestSleep(TEST_SPACING);

        // testing GetFileInformation
        {
            webrtc::VideoCodec fileVideoCodec;
            webrtc::CodecInst fileAudioCodec;
            ViETest::Log("Reading video file information");

            EXPECT_EQ(0, ptrViEFile->GetFileInformation(
                incomingVideo.c_str(), fileVideoCodec, fileAudioCodec));
            PrintAudioCodec(fileAudioCodec);
            PrintVideoCodec(fileVideoCodec);
        }

        // testing StartPlayFile and RegisterObserver
        {
            ViETest::Log("Start playing file: %s with observer",
                         incomingVideo.c_str());
            EXPECT_EQ(0, ptrViEFile->StartPlayFile(incomingVideo.c_str(),
                                                   fileId));

            ViETest::Log("Registering file observer");
            EXPECT_EQ(0, ptrViEFile->RegisterObserver(fileId, fileObserver));
            ViETest::Log("Done\n");
        }

        // testing SendFileOnChannel and StopSendFileOnChannel
        {
            ViETest::Log("Sending video on channel");
            // should fail since we are sending the capture device.
            EXPECT_NE(0, ptrViEFile->SendFileOnChannel(fileId, videoChannel));

            // Disconnect the camera
            EXPECT_EQ(0, ptrViECapture->DisconnectCaptureDevice(videoChannel));

            // And try playing the file again.
            EXPECT_EQ(0, ptrViEFile->SendFileOnChannel(fileId, videoChannel));

            AutoTestSleep(VIDEO_LENGTH);
            ViETest::Log("Stopped sending video on channel");
            EXPECT_EQ(0, ptrViEFile->StopSendFileOnChannel(videoChannel));
            ViETest::Log("Done\n");
        }

        AutoTestSleep(TEST_SPACING);

        // stop playing the file
        {
            ViETest::Log("Stop playing the file.");
            EXPECT_EQ(0, ptrViEFile->StopPlayFile(fileId));
            ViETest::Log("Done\n");
        }

        // testing StartRecordOutgoingVideo and StopRecordOutgoingVideo
        {
            // connect the camera to the output.
            EXPECT_EQ(0, ptrViECapture->ConnectCaptureDevice(
              captureId, videoChannel));

            ViETest::Log("Recording outgoing video (currently no audio) for %d "
                         "seconds", VIDEO_LENGTH);
            EXPECT_EQ(0, ptrViEFile->StartRecordOutgoingVideo(
                videoChannel, outgoingVideo.c_str(), webrtc::NO_AUDIO,
                audioCodec2, videoCodec));

            AutoTestSleep(VIDEO_LENGTH);
            ViETest::Log("Stop recording outgoing video");
            EXPECT_EQ(0, ptrViEFile->StopRecordOutgoingVideo(videoChannel));
            ViETest::Log("Done\n");
        }

        // again testing GetFileInformation
        {
            EXPECT_EQ(0, ptrViEFile->GetFileInformation(
                incomingVideo.c_str(), videoCodec, audioCodec2));
            PrintAudioCodec(audioCodec2);
            PrintVideoCodec(videoCodec);
        }

        AutoTestSleep(TEST_SPACING);

        // GetCaptureDeviceSnapshot
        {
            ViETest::Log("Testing GetCaptureDeviceSnapshot(int, ViEPicture)");
            ViETest::Log("Taking a picture to use for displaying ViEPictures "
                         "for the rest of file test");
            ViETest::Log("Hold an object to the camera. Ready?...");
            AutoTestSleep(1000);
            ViETest::Log("3");
            AutoTestSleep(1000);
            ViETest::Log("...2");
            AutoTestSleep(1000);
            ViETest::Log("...1");
            AutoTestSleep(1000);
            ViETest::Log("...Taking picture!");
            EXPECT_EQ(0, ptrViEFile->GetCaptureDeviceSnapshot(
                captureId, capturePicture));
            ViETest::Log("Picture has been taken.");
            AutoTestSleep(TEST_SPACING);

            ViETest::Log("Done\n");
        }

        AutoTestSleep(TEST_SPACING);

        // GetRenderSnapshot
        {
            ViETest::Log("Testing GetRenderSnapshot(int, char*)");

            ViETest::Log("Taking snapshot of videoChannel %d", captureId);
            EXPECT_EQ(0, ptrViEFile->GetRenderSnapshot(
                captureId, snapshotRenderFileName.c_str()));
            ViETest::Log("Wrote image to file %s",
                         snapshotRenderFileName.c_str());
            ViETest::Log("Done\n");
            AutoTestSleep(TEST_SPACING);
        }

        // GetRenderSnapshot
        {
            ViETest::Log("Testing GetRenderSnapshot(int, ViEPicture)");
            EXPECT_EQ(0, ptrViEFile->GetRenderSnapshot(
                captureId, renderPicture));
            ViETest::Log("Done\n");
        }

        AutoTestSleep(TEST_SPACING);

        // GetCaptureDeviceSnapshot
        {
            ViETest::Log("Testing GetCaptureDeviceSnapshot(int, char*)");
            ViETest::Log("Taking snapshot from capture device %d", captureId);
            EXPECT_EQ(0, ptrViEFile->GetCaptureDeviceSnapshot(
                captureId, snapshotCaptureDeviceFileName.c_str()));
            ViETest::Log("Wrote image to file %s",
                         snapshotCaptureDeviceFileName.c_str());
            ViETest::Log("Done\n");
        }

        AutoTestSleep(TEST_SPACING);

        // testing SetRenderStartImage(videoChannel, renderStartImage);
        {
            ViETest::Log("Testing SetRenderStartImage(int, char*)");
            // set render image, then stop capture and stop render to display it
            ViETest::Log("Stoping renderer, setting start image, then "
                         "restarting");
            EXPECT_EQ(0, ptrViEFile->SetRenderStartImage(
                videoChannel, renderStartImage.c_str()));
            EXPECT_EQ(0, ptrViECapture->StopCapture(captureId));
            EXPECT_EQ(0, ptrViERender->StopRender(videoChannel));

            ViETest::Log("Render start image should be displayed.");
            AutoTestSleep(RENDER_TIMEOUT);

            // restarting capture and render
            EXPECT_EQ(0, ptrViECapture->StartCapture(captureId));
            EXPECT_EQ(0, ptrViERender->StartRender(videoChannel));
            ViETest::Log("Done\n");
        }

        AutoTestSleep(TEST_SPACING);

        // testing SetRenderStartImage(videoChannel, renderStartImage);
        {
            ViETest::Log("Testing SetRenderStartImage(int, ViEPicture)");
            // set render image, then stop capture and stop render to display it
            ViETest::Log("Stoping renderer, setting start image, then "
                         "restarting");
            EXPECT_EQ(0, ptrViEFile->GetCaptureDeviceSnapshot(
                      captureId, capturePicture));
            EXPECT_EQ(0, ptrViEFile->SetRenderStartImage(
                videoChannel, capturePicture));
            EXPECT_EQ(0, ptrViECapture->StopCapture(captureId));
            EXPECT_EQ(0, ptrViERender->StopRender(videoChannel));

            ViETest::Log("Render start image should be displayed.");
            AutoTestSleep(RENDER_TIMEOUT);

            // restarting capture and render
            EXPECT_EQ(0, ptrViECapture->StartCapture(captureId));
            EXPECT_EQ(0, ptrViERender->StartRender(videoChannel));
            ViETest::Log("Done\n");
        }

        AutoTestSleep(TEST_SPACING);

        // testing SetRenderTimeoutImage(videoChannel, renderTimeoutFile,
        // RENDER_TIMEOUT);
        {
            ViETest::Log("Testing SetRenderTimeoutImage(int, char*)");
            ViETest::Log("Stopping capture device to induce timeout of %d ms",
                         RENDER_TIMEOUT);
            EXPECT_EQ(0, ptrViEFile->SetRenderTimeoutImage(
                videoChannel, renderTimeoutFile.c_str(), RENDER_TIMEOUT));

            // now stop sending frames to the remote renderer and wait for
            // timeout
            EXPECT_EQ(0, ptrViECapture->StopCapture(captureId));
            AutoTestSleep(RENDER_TIMEOUT);
            ViETest::Log("Timeout image should be displayed now for %d ms",
                         RENDER_TIMEOUT * 2);
            AutoTestSleep(RENDER_TIMEOUT * 2);

            // restart the capture device to undo the timeout
            EXPECT_EQ(0, ptrViECapture->StartCapture(captureId));
            ViETest::Log("Restarting capture device");
            AutoTestSleep(RENDER_TIMEOUT);
            ViETest::Log("Done\n");
        }

        AutoTestSleep(TEST_SPACING);

        // Need to create a ViEPicture object to pass into this function.
        // SetRenderTimeoutImage(videoChannel, renderTimeoutFile,
        // RENDER_TIMEOUT);
        {
            ViETest::Log("Testing SetRenderTimeoutImage(int, ViEPicture)");
            ViETest::Log("Stopping capture device to induce timeout of %d",
                         RENDER_TIMEOUT);
            EXPECT_EQ(0, ptrViEFile->SetRenderTimeoutImage(
                videoChannel, capturePicture, RENDER_TIMEOUT));

            // now stop sending frames to the remote renderer and wait for
            // timeout
            EXPECT_EQ(0, ptrViECapture->StopCapture(captureId));
            AutoTestSleep(RENDER_TIMEOUT);
            ViETest::Log("Timeout image should be displayed now for %d",
                         RENDER_TIMEOUT * 2);
            AutoTestSleep(RENDER_TIMEOUT * 2);

            // restart the capture device to undo the timeout
            EXPECT_EQ(0, ptrViECapture->StartCapture(captureId));
            ViETest::Log("Restarting capture device");
            ViETest::Log("Done\n");
        }

        // testing DeregisterObserver
        {
            ViETest::Log("Deregistering file observer");
            // Should fail since we don't observe this file.
            EXPECT_NE(0, ptrViEFile->DeregisterObserver(fileId, fileObserver));
        }

        // Stop debug record.
        EXPECT_EQ(0, ptrViEFile->StopDebugRecording(videoChannel));

        //***************************************************************
        //	Testing finished. Tear down Video Engine
        //***************************************************************

        EXPECT_EQ(0, ptrViEBase->DisconnectAudioChannel(videoChannel));
        EXPECT_EQ(0, ptrViEBase->SetVoiceEngine(NULL));
        EXPECT_EQ(0, ptrVEBase->DeleteChannel(audioChannel));
        // VoE reference counting is per-object, so we use EXPECT_NE
        EXPECT_NE(0, ptrVEBase->Release());
        EXPECT_NE(0, ptrVECodec->Release());
        EXPECT_TRUE(webrtc::VoiceEngine::Delete(ptrVEEngine));

        EXPECT_EQ(0, ptrViEBase->StopReceive(videoChannel));
        EXPECT_EQ(0, ptrViEBase->StopSend(videoChannel));
        EXPECT_EQ(0, ptrViERender->StopRender(videoChannel));
        EXPECT_EQ(0, ptrViERender->RemoveRenderer(captureId));
        EXPECT_EQ(0, ptrViERender->RemoveRenderer(videoChannel));
        EXPECT_EQ(0, ptrViECapture->DisconnectCaptureDevice(videoChannel));
        EXPECT_EQ(0, ptrViEFile->FreePicture(capturePicture));
        EXPECT_EQ(0, ptrViEFile->FreePicture(renderPicture));
        EXPECT_EQ(0, ptrViEBase->DeleteChannel(videoChannel));

        EXPECT_EQ(0, ptrViEFile->Release());
    }
#endif
}

void ViEAutoTest::ViEFileExtendedTest()
{
}

void ViEAutoTest::ViEFileAPITest()
{
}
