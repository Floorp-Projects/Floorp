/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

//
// vie_autotest_loopback.cc
//
// This code is also used as sample code for ViE 3.0
//

// ===================================================================
//
// BEGIN: VideoEngine 3.0 Sample Code
//

#include <iostream>

#include "common_types.h"
#include "tb_external_transport.h"
#include "voe_base.h"
#include "vie_autotest_defines.h"
#include "vie_autotest.h"
#include "vie_base.h"
#include "vie_capture.h"
#include "vie_codec.h"
#include "vie_network.h"
#include "vie_render.h"
#include "vie_rtp_rtcp.h"

#define VCM_RED_PAYLOAD_TYPE        96
#define VCM_ULPFEC_PAYLOAD_TYPE     97

int VideoEngineSampleCode(void* window1, void* window2)
{
    //********************************************************
    //  Begin create/initialize Video Engine for testing
    //********************************************************

    int error = 0;

    //
    // Create a VideoEngine instance
    //
    webrtc::VideoEngine* ptrViE = NULL;
    ptrViE = webrtc::VideoEngine::Create();
    if (ptrViE == NULL)
    {
        printf("ERROR in VideoEngine::Create\n");
        return -1;
    }

    error = ptrViE->SetTraceFilter(webrtc::kTraceAll);
    if (error == -1)
    {
        printf("ERROR in VideoEngine::SetTraceLevel\n");
        return -1;
    }

    std::string trace_file =
        ViETest::GetResultOutputPath() + "ViELoopbackCall_trace.txt";
    error = ptrViE->SetTraceFile(trace_file.c_str());
    if (error == -1)
    {
        printf("ERROR in VideoEngine::SetTraceFile\n");
        return -1;
    }

    //
    // Init VideoEngine and create a channel
    //
    webrtc::ViEBase* ptrViEBase = webrtc::ViEBase::GetInterface(ptrViE);
    if (ptrViEBase == NULL)
    {
        printf("ERROR in ViEBase::GetInterface\n");
        return -1;
    }

    error = ptrViEBase->Init();
    if (error == -1)
    {
        printf("ERROR in ViEBase::Init\n");
        return -1;
    }

    int videoChannel = -1;
    error = ptrViEBase->CreateChannel(videoChannel);
    if (error == -1)
    {
        printf("ERROR in ViEBase::CreateChannel\n");
        return -1;
    }

    //
    // List available capture devices, allocate and connect.
    //
    webrtc::ViECapture* ptrViECapture =
        webrtc::ViECapture::GetInterface(ptrViE);
    if (ptrViEBase == NULL)
    {
        printf("ERROR in ViECapture::GetInterface\n");
        return -1;
    }

    const unsigned int KMaxDeviceNameLength = 128;
    const unsigned int KMaxUniqueIdLength = 256;
    char deviceName[KMaxDeviceNameLength];
    memset(deviceName, 0, KMaxDeviceNameLength);
    char uniqueId[KMaxUniqueIdLength];
    memset(uniqueId, 0, KMaxUniqueIdLength);

    printf("Available capture devices:\n");
    int captureIdx = 0;
    for (captureIdx = 0;
         captureIdx < ptrViECapture->NumberOfCaptureDevices();
         captureIdx++)
    {
        memset(deviceName, 0, KMaxDeviceNameLength);
        memset(uniqueId, 0, KMaxUniqueIdLength);

        error = ptrViECapture->GetCaptureDevice(captureIdx, deviceName,
                                                KMaxDeviceNameLength, uniqueId,
                                                KMaxUniqueIdLength);
        if (error == -1)
        {
            printf("ERROR in ViECapture::GetCaptureDevice\n");
            return -1;
        }
        printf("\t %d. %s\n", captureIdx + 1, deviceName);
    }
    printf("\nChoose capture device: ");
#ifdef WEBRTC_ANDROID
    captureIdx = 0;
    printf("0\n");
#else
    if (scanf("%d", &captureIdx) != 1)
    {
        printf("Error in scanf()\n");
        return -1;
    }
    getchar();
    captureIdx = captureIdx - 1; // Compensate for idx start at 1.
#endif
    error = ptrViECapture->GetCaptureDevice(captureIdx, deviceName,
                                            KMaxDeviceNameLength, uniqueId,
                                            KMaxUniqueIdLength);
    if (error == -1)
    {
        printf("ERROR in ViECapture::GetCaptureDevice\n");
        return -1;
    }

    int captureId = 0;
    error = ptrViECapture->AllocateCaptureDevice(uniqueId, KMaxUniqueIdLength,
                                                 captureId);
    if (error == -1)
    {
        printf("ERROR in ViECapture::AllocateCaptureDevice\n");
        return -1;
    }

    error = ptrViECapture->ConnectCaptureDevice(captureId, videoChannel);
    if (error == -1)
    {
        printf("ERROR in ViECapture::ConnectCaptureDevice\n");
        return -1;
    }

    error = ptrViECapture->StartCapture(captureId);
    if (error == -1)
    {
        printf("ERROR in ViECapture::StartCapture\n");
        return -1;
    }

    //
    // RTP/RTCP settings
    //
    webrtc::ViERTP_RTCP* ptrViERtpRtcp =
        webrtc::ViERTP_RTCP::GetInterface(ptrViE);
    if (ptrViERtpRtcp == NULL)
    {
        printf("ERROR in ViERTP_RTCP::GetInterface\n");
        return -1;
    }

    error = ptrViERtpRtcp->SetRTCPStatus(videoChannel,
                                         webrtc::kRtcpCompound_RFC4585);
    if (error == -1)
    {
        printf("ERROR in ViERTP_RTCP::SetRTCPStatus\n");
        return -1;
    }

    error = ptrViERtpRtcp->SetKeyFrameRequestMethod(
        videoChannel, webrtc::kViEKeyFrameRequestPliRtcp);
    if (error == -1)
    {
        printf("ERROR in ViERTP_RTCP::SetKeyFrameRequestMethod\n");
        return -1;
    }

    error = ptrViERtpRtcp->SetRembStatus(videoChannel, true, true);
    if (error == -1)
    {
        printf("ERROR in ViERTP_RTCP::SetTMMBRStatus\n");
        return -1;
    }

    //
    // Set up rendering
    //
    webrtc::ViERender* ptrViERender = webrtc::ViERender::GetInterface(ptrViE);
    if (ptrViERender == NULL)
    {
        printf("ERROR in ViERender::GetInterface\n");
        return -1;
    }

    error
        = ptrViERender->AddRenderer(captureId, window1, 0, 0.0, 0.0, 1.0, 1.0);
    if (error == -1)
    {
        printf("ERROR in ViERender::AddRenderer\n");
        return -1;
    }

    error = ptrViERender->StartRender(captureId);
    if (error == -1)
    {
        printf("ERROR in ViERender::StartRender\n");
        return -1;
    }

    error = ptrViERender->AddRenderer(videoChannel, window2, 1, 0.0, 0.0, 1.0,
                                      1.0);
    if (error == -1)
    {
        printf("ERROR in ViERender::AddRenderer\n");
        return -1;
    }

    error = ptrViERender->StartRender(videoChannel);
    if (error == -1)
    {
        printf("ERROR in ViERender::StartRender\n");
        return -1;
    }

    //
    // Setup codecs
    //
    webrtc::ViECodec* ptrViECodec = webrtc::ViECodec::GetInterface(ptrViE);
    if (ptrViECodec == NULL)
    {
        printf("ERROR in ViECodec::GetInterface\n");
        return -1;
    }

    // Check available codecs and prepare receive codecs
    printf("\nAvailable codecs:\n");
    webrtc::VideoCodec videoCodec;
    memset(&videoCodec, 0, sizeof(webrtc::VideoCodec));
    int codecIdx = 0;
    for (codecIdx = 0; codecIdx < ptrViECodec->NumberOfCodecs(); codecIdx++)
    {
        error = ptrViECodec->GetCodec(codecIdx, videoCodec);
        if (error == -1)
        {
            printf("ERROR in ViECodec::GetCodec\n");
            return -1;
        }

        // try to keep the test frame size small when I420
        if (videoCodec.codecType == webrtc::kVideoCodecI420)
        {
            videoCodec.width = 176;
            videoCodec.height = 144;
        }

        error = ptrViECodec->SetReceiveCodec(videoChannel, videoCodec);
        if (error == -1)
        {
            printf("ERROR in ViECodec::SetReceiveCodec\n");
            return -1;
        }
        if (videoCodec.codecType != webrtc::kVideoCodecRED
            && videoCodec.codecType != webrtc::kVideoCodecULPFEC)
        {
            printf("\t %d. %s\n", codecIdx + 1, videoCodec.plName);
        }
    }
    printf("Choose codec: ");
#ifdef WEBRTC_ANDROID
    codecIdx = 0;
    printf("0\n");
#else
    if (scanf("%d", &codecIdx) != 1)
    {
        printf("Error in scanf()\n");
        return -1;
    }
    getchar();
    codecIdx = codecIdx - 1; // Compensate for idx start at 1.
#endif

    error = ptrViECodec->GetCodec(codecIdx, videoCodec);
    if (error == -1)
    {
        printf("ERROR in ViECodec::GetCodec\n");
        return -1;
    }

    // Set spatial resolution option
    std::string str;
    std::cout << std::endl;
    std::cout << "Enter frame size option (default is CIF):" << std::endl;
    std::cout << "1. QCIF (176X144) " << std::endl;
    std::cout << "2. CIF  (352X288) " << std::endl;
    std::cout << "3. VGA  (640X480) " << std::endl;
    std::cout << "4. 4CIF (704X576) " << std::endl;
    std::cout << "5. WHD  (1280X720) " << std::endl;
    std::getline(std::cin, str);
    int resolnOption = atoi(str.c_str());
    // Try to keep the test frame size small when I420
    if (videoCodec.codecType == webrtc::kVideoCodecI420)
    {
       resolnOption = 1;
    }
    switch (resolnOption)
    {
        case 1:
            videoCodec.width = 176;
            videoCodec.height = 144;
            break;
        case 2:
            videoCodec.width = 352;
            videoCodec.height = 288;
            break;
        case 3:
            videoCodec.width = 640;
            videoCodec.height = 480;
            break;
        case 4:
            videoCodec.width = 704;
            videoCodec.height = 576;
            break;
        case 5:
            videoCodec.width = 1280;
            videoCodec.height = 720;
            break;
    }

    // Set number of temporal layers.
    std::cout << std::endl;
    std::cout << "Choose number of temporal layers (1 to 4).";
    std::cout << "Press enter for default: \n";
    std::getline(std::cin, str);
    int numTemporalLayers = atoi(str.c_str());
    if(numTemporalLayers != 0)
    {
        videoCodec.codecSpecific.VP8.numberOfTemporalLayers = numTemporalLayers;
    }

    // Set start bit rate
    std::cout << std::endl;
    std::cout << "Choose start rate (in kbps). Press enter for default:  ";
    std::getline(std::cin, str);
    int startRate = atoi(str.c_str());
    if(startRate != 0)
    {
        videoCodec.startBitrate=startRate;
    }

    error = ptrViECodec->SetSendCodec(videoChannel, videoCodec);
    if (error == -1)
    {
        printf("ERROR in ViECodec::SetSendCodec\n");
        return -1;
    }

    //
    // Choose Protection Mode
    //
    std::cout << std::endl;
    std::cout << "Enter Protection Method:" << std::endl;
    std::cout << "0. None" << std::endl;
    std::cout << "1. FEC" << std::endl;
    std::cout << "2. NACK" << std::endl;
    std::cout << "3. NACK+FEC" << std::endl;
    std::getline(std::cin, str);
    int protectionMethod = atoi(str.c_str());
    error = 0;
    bool temporalToggling = true;
    switch (protectionMethod)
    {
        case 0: // None: default is no protection
            break;

        case 1: // FEC only
            error = ptrViERtpRtcp->SetFECStatus(videoChannel,
                                                true,
                                                VCM_RED_PAYLOAD_TYPE,
                                                VCM_ULPFEC_PAYLOAD_TYPE);
            temporalToggling = false;
            break;

        case 2: // Nack only
            error = ptrViERtpRtcp->SetNACKStatus(videoChannel, true);

            break;

        case 3: // Hybrid NAck and FEC
            error = ptrViERtpRtcp->SetHybridNACKFECStatus(
                videoChannel,
                true,
                VCM_RED_PAYLOAD_TYPE,
                VCM_ULPFEC_PAYLOAD_TYPE);
            temporalToggling = false;
            break;
     }

    if (error < 0)
    {
        printf("ERROR in ViERTP_RTCP::SetProtectionStatus\n");
    }


    //
    // Address settings
    //
    webrtc::ViENetwork* ptrViENetwork =
        webrtc::ViENetwork::GetInterface(ptrViE);
    if (ptrViENetwork == NULL)
    {
        printf("ERROR in ViENetwork::GetInterface\n");
        return -1;
    }

    // Setting External transport
    TbExternalTransport extTransport(*(ptrViENetwork));

    int testMode = 0;
    std::cout << std::endl;
    std::cout << "Enter 1 for testing packet loss and delay with "
        "external transport: ";
    std::string test_str;
    std::getline(std::cin, test_str);
    testMode = atoi(test_str.c_str());
    if (testMode == 1)
    {
        // Avoid changing SSRC due to collision.
        error = ptrViERtpRtcp->SetLocalSSRC(videoChannel, 1);

        error = ptrViENetwork->RegisterSendTransport(videoChannel,
                                                     extTransport);
        if (error == -1)
        {
            printf("ERROR in ViECodec::RegisterSendTransport \n");
            return -1;
        }

        // Set up packet loss value
        std::cout << "Enter Packet Loss Percentage" << std::endl;
        std::string rate_str;
        std::getline(std::cin, rate_str);
        int rate = atoi(rate_str.c_str());
        extTransport.SetPacketLoss(rate);
        if (rate) {
          temporalToggling = false;
        }

        // Set network delay value
        std::cout << "Enter network delay value [mS]" << std::endl;
        std::string delay_str;
        std::getline(std::cin, delay_str);
        int delayMs = atoi(delay_str.c_str());
        extTransport.SetNetworkDelay(delayMs);

        if (numTemporalLayers > 1 && temporalToggling) {
          extTransport.SetTemporalToggle(numTemporalLayers);
        } else {
          // Disabled
          extTransport.SetTemporalToggle(0);
        }
    }
    else
    {
        const char* ipAddress = "127.0.0.1";
        const unsigned short rtpPort = 6000;
        std::cout << std::endl;
        std::cout << "Using rtp port: " << rtpPort << std::endl;
        std::cout << std::endl;
        error = ptrViENetwork->SetLocalReceiver(videoChannel, rtpPort);
        if (error == -1)
        {
            printf("ERROR in ViENetwork::SetLocalReceiver\n");
            return -1;
        }
        error = ptrViENetwork->SetSendDestination(videoChannel,
                                                  ipAddress, rtpPort);
        if (error == -1)
        {
            printf("ERROR in ViENetwork::SetSendDestination\n");
            return -1;
        }
    }

    error = ptrViEBase->StartReceive(videoChannel);
    if (error == -1)
    {
        printf("ERROR in ViENetwork::StartReceive\n");
        return -1;
    }

    error = ptrViEBase->StartSend(videoChannel);
    if (error == -1)
    {
        printf("ERROR in ViENetwork::StartSend\n");
        return -1;
    }

    //********************************************************
    //  Engine started
    //********************************************************


    // Call started
    printf("\nLoopback call started\n\n");
    printf("Press enter to stop...");
    while ((getchar()) != '\n')
        ;

    //********************************************************
    //  Testing finished. Tear down Video Engine
    //********************************************************

    error = ptrViEBase->StopReceive(videoChannel);
    if (error == -1)
    {
        printf("ERROR in ViEBase::StopReceive\n");
        return -1;
    }

    error = ptrViEBase->StopSend(videoChannel);
    if (error == -1)
    {
        printf("ERROR in ViEBase::StopSend\n");
        return -1;
    }

    error = ptrViERender->StopRender(captureId);
    if (error == -1)
    {
        printf("ERROR in ViERender::StopRender\n");
        return -1;
    }

    error = ptrViERender->RemoveRenderer(captureId);
    if (error == -1)
    {
        printf("ERROR in ViERender::RemoveRenderer\n");
        return -1;
    }

    error = ptrViERender->StopRender(videoChannel);
    if (error == -1)
    {
        printf("ERROR in ViERender::StopRender\n");
        return -1;
    }

    error = ptrViERender->RemoveRenderer(videoChannel);
    if (error == -1)
    {
        printf("ERROR in ViERender::RemoveRenderer\n");
        return -1;
    }

    error = ptrViECapture->StopCapture(captureId);
    if (error == -1)
    {
        printf("ERROR in ViECapture::StopCapture\n");
        return -1;
    }

    error = ptrViECapture->DisconnectCaptureDevice(videoChannel);
    if (error == -1)
    {
        printf("ERROR in ViECapture::DisconnectCaptureDevice\n");
        return -1;
    }

    error = ptrViECapture->ReleaseCaptureDevice(captureId);
    if (error == -1)
    {
        printf("ERROR in ViECapture::ReleaseCaptureDevice\n");
        return -1;
    }

    error = ptrViEBase->DeleteChannel(videoChannel);
    if (error == -1)
    {
        printf("ERROR in ViEBase::DeleteChannel\n");
        return -1;
    }

    int remainingInterfaces = 0;
    remainingInterfaces = ptrViECodec->Release();
    remainingInterfaces += ptrViECapture->Release();
    remainingInterfaces += ptrViERtpRtcp->Release();
    remainingInterfaces += ptrViERender->Release();
    remainingInterfaces += ptrViENetwork->Release();
    remainingInterfaces += ptrViEBase->Release();
    if (remainingInterfaces > 0)
    {
        printf("ERROR: Could not release all interfaces\n");
        return -1;
    }

    bool deleted = webrtc::VideoEngine::Delete(ptrViE);
    if (deleted == false)
    {
        printf("ERROR in VideoEngine::Delete\n");
        return -1;
    }

    return 0;

    //
    // END:  VideoEngine 3.0 Sample Code
    //
    // ===================================================================
}

int ViEAutoTest::ViELoopbackCall()
{
    ViETest::Log(" ");
    ViETest::Log("========================================");
    ViETest::Log(" ViE Autotest Loopback Call\n");

    if (VideoEngineSampleCode(_window1, _window2) == 0)
    {
        ViETest::Log(" ");
        ViETest::Log(" ViE Autotest Loopback Call Done");
        ViETest::Log("========================================");
        ViETest::Log(" ");

        return 0;
    }

    ViETest::Log(" ");
    ViETest::Log(" ViE Autotest Loopback Call Failed");
    ViETest::Log("========================================");
    ViETest::Log(" ");
    return 1;

}
