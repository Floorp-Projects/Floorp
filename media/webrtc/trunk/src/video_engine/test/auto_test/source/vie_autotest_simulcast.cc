/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

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

int VideoEngineSimulcastTest(void* window1, void* window2)
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
        ViETest::GetResultOutputPath() + "ViESimulcast_trace.txt";
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
        if (videoCodec.codecType != webrtc::kVideoCodecVP8)
        {
            continue;
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
        break;
    }
    error = ptrViECodec->GetCodec(codecIdx, videoCodec);
    if (error == -1)
    {
        printf("ERROR in ViECodec::GetCodec\n");
        return -1;
    }

    // Set spatial resolution option
    videoCodec.width = 1280;
    videoCodec.height = 720;

    // simulcast settings
    videoCodec.numberOfSimulcastStreams = 3;
    videoCodec.simulcastStream[0].width = 320;
    videoCodec.simulcastStream[0].height = 180;
    videoCodec.simulcastStream[0].numberOfTemporalLayers = 0;
    videoCodec.simulcastStream[0].maxBitrate = 100;
    videoCodec.simulcastStream[0].qpMax = videoCodec.qpMax;

    videoCodec.simulcastStream[1].width = 640;
    videoCodec.simulcastStream[1].height = 360;
    videoCodec.simulcastStream[1].numberOfTemporalLayers = 0;
    videoCodec.simulcastStream[1].maxBitrate = 500;
    videoCodec.simulcastStream[1].qpMax = videoCodec.qpMax;

    videoCodec.simulcastStream[2].width = 1280;
    videoCodec.simulcastStream[2].height = 720;
    videoCodec.simulcastStream[2].numberOfTemporalLayers = 0;
    videoCodec.simulcastStream[2].maxBitrate = 1200;
    videoCodec.simulcastStream[2].qpMax = videoCodec.qpMax;

    // Set start bit rate
    std::string str;
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

    error = ptrViENetwork->RegisterSendTransport(videoChannel,
                                                 extTransport);
    if (error == -1)
    {
        printf("ERROR in ViECodec::RegisterSendTransport \n");
        return -1;
    }

    extTransport.SetPacketLoss(0);

    // Set network delay value
    extTransport.SetNetworkDelay(10);

    extTransport.SetSSRCFilter(3);

    for (int idx = 0; idx < 3; idx++)
    {
        error = ptrViERtpRtcp->SetLocalSSRC(videoChannel,
                                            idx+1, // SSRC
                                            webrtc::kViEStreamTypeNormal,
                                            idx);
        if (error == -1)
        {
            printf("ERROR in ViERTP_RTCP::SetLocalSSRC(idx:%d)\n", idx);
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

    printf("\nSimulcast call started\n\n");
    do
    {
        printf("Enter new SSRC filter 1,2 or 3\n");
        printf("Press enter to stop...");
        str.clear();
        std::getline(std::cin, str);
        if (!str.empty())
        {
            int ssrc = atoi(str.c_str());
            if (ssrc > 0 && ssrc < 4)
            {
                extTransport.SetSSRCFilter(ssrc);
            } else
            {
                printf("Invalid SSRC\n");
            }
        } else
        {
            break;
        }
    } while (true);

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

int ViEAutoTest::ViESimulcastCall()
{
    ViETest::Log(" ");
    ViETest::Log("========================================");
    ViETest::Log(" ViE Autotest Simulcast Call\n");

    if (VideoEngineSimulcastTest(_window1, _window2) == 0)
    {
        ViETest::Log(" ");
        ViETest::Log(" ViE Autotest Simulcast Call Done");
        ViETest::Log("========================================");
        ViETest::Log(" ");

        return 0;
    }
    ViETest::Log(" ");
    ViETest::Log(" ViE Autotest Simulcast Call Failed");
    ViETest::Log("========================================");
    ViETest::Log(" ");
    return 1;
}
