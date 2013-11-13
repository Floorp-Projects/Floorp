/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*************************************************
 *
 * Testing multi thread - receive and send sides
 *
 **************************************************/

#include <string.h>

#include "webrtc/modules/rtp_rtcp/interface/rtp_payload_registry.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_receiver.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "webrtc/modules/video_coding/main/interface/video_coding.h"
#include "webrtc/modules/video_coding/main/test/media_opt_test.h"
#include "webrtc/modules/video_coding/main/test/mt_test_common.h"
#include "webrtc/modules/video_coding/main/test/receiver_tests.h"
#include "webrtc/modules/video_coding/main/test/test_macros.h"
#include "webrtc/modules/video_coding/main/test/test_util.h"
#include "webrtc/system_wrappers/interface/thread_wrapper.h"
#include "webrtc/test/testsupport/fileutils.h"

using namespace webrtc;

bool
MainSenderThread(void* obj)
{
    SendSharedState* state = static_cast<SendSharedState*>(obj);
    EventWrapper& waitEvent = *EventWrapper::Create();
    // preparing a frame for encoding
    I420VideoFrame sourceFrame;
    int32_t width = state->_args.width;
    int32_t height = state->_args.height;
    float frameRate = state->_args.frameRate;
    int32_t lengthSourceFrame  = 3*width*height/2;
    uint8_t* tmpBuffer = new uint8_t[lengthSourceFrame];

    if (state->_sourceFile == NULL)
    {
        state->_sourceFile = fopen(state->_args.inputFile.c_str(), "rb");
        if (state->_sourceFile == NULL)
        {
            printf ("Error when opening file \n");
            delete &waitEvent;
            delete [] tmpBuffer;
            return false;
        }
    }
    if (feof(state->_sourceFile) == 0)
    {
        TEST(fread(tmpBuffer, 1, lengthSourceFrame,state->_sourceFile) > 0 ||
             feof(state->_sourceFile));
        state->_frameCnt++;
        int size_y = width * height;
        int half_width = (width + 1) / 2;
        int half_height = (height + 1) / 2;
        int size_uv = half_width * half_height;
        sourceFrame.CreateFrame(size_y, tmpBuffer,
                                size_uv, tmpBuffer + size_y,
                                size_uv, tmpBuffer + size_y + size_uv,
                                width, height,
                                width, half_width, half_width);
        state->_timestamp += (uint32_t)(9e4 / frameRate);
        sourceFrame.set_timestamp(state->_timestamp);

        int32_t ret = state->_vcm.AddVideoFrame(sourceFrame);
        if (ret < 0)
        {
            printf("Add Frame error: %d\n", ret);
            delete &waitEvent;
            delete [] tmpBuffer;
            return false;
        }
        waitEvent.Wait(33);
    }

    delete &waitEvent;
    delete [] tmpBuffer;

    return true;
}

bool
IntSenderThread(void* obj)
{
    SendSharedState* state = static_cast<SendSharedState*>(obj);
    state->_vcm.SetChannelParameters(1000000,30,0);

    return true;
}


int MTRxTxTest(CmdArgs& args)
{
    /* TEST SETTINGS */
    std::string   inname = args.inputFile;
    std::string outname;
    if (args.outputFile == "")
        outname = test::OutputPath() + "MTRxTxTest_decoded.yuv";
    else
        outname = args.outputFile;

    uint16_t  width = args.width;
    uint16_t  height = args.height;

    float         frameRate = args.frameRate;
    float         bitRate = args.bitRate;
    int32_t   numberOfCores = 1;

    // error resilience/network
    // Nack support is currently not implemented in this test.
    bool          nackEnabled = false;
    bool          fecEnabled = false;
    uint8_t   rttMS = 20;
    float         lossRate = 0.0*255; // no packet loss
    uint32_t  renderDelayMs = 0;
    uint32_t  minPlayoutDelayMs = 0;

    /* TEST SET-UP */

    // Set up trace
    Trace::CreateTrace();
    Trace::SetTraceFile((test::OutputPath() + "MTRxTxTestTrace.txt").c_str());
    Trace::set_level_filter(webrtc::kTraceAll);

    FILE* sourceFile;
    FILE* decodedFile;

    if ((sourceFile = fopen(inname.c_str(), "rb")) == NULL)
    {
        printf("Cannot read file %s.\n", inname.c_str());
        return -1;
    }

    if ((decodedFile = fopen(outname.c_str(), "wb")) == NULL)
    {
        printf("Cannot read file %s.\n", outname.c_str());
        return -1;
    }
    VideoCodingModule* vcm = VideoCodingModule::Create(1);
    RtpDataCallback dataCallback(vcm);

    RTPSendCompleteCallback* outgoingTransport =
        new RTPSendCompleteCallback(Clock::GetRealTimeClock(), "dump.rtp");

    RtpRtcp::Configuration configuration;
    configuration.id = 1;
    configuration.audio = false;
    configuration.outgoing_transport = outgoingTransport;
    RtpRtcp* rtp = RtpRtcp::CreateRtpRtcp(configuration);
    scoped_ptr<RTPPayloadRegistry> registry(new RTPPayloadRegistry(
        -1, RTPPayloadStrategy::CreateStrategy(false)));
    scoped_ptr<RtpReceiver> rtp_receiver(
        RtpReceiver::CreateVideoReceiver(-1, Clock::GetRealTimeClock(),
                                         &dataCallback, NULL, registry.get()));

    // registering codecs for the RTP module
    VideoCodec video_codec;
    strncpy(video_codec.plName, "ULPFEC", 32);
    video_codec.plType = VCM_ULPFEC_PAYLOAD_TYPE;
    TEST(rtp_receiver->RegisterReceivePayload(video_codec.plName,
                                              video_codec.plType,
                                              90000,
                                              0,
                                              video_codec.maxBitrate) == 0);

    strncpy(video_codec.plName, "RED", 32);
    video_codec.plType = VCM_RED_PAYLOAD_TYPE;
    TEST(rtp_receiver->RegisterReceivePayload(video_codec.plName,
                                              video_codec.plType,
                                              90000,
                                              0,
                                              video_codec.maxBitrate) == 0);

    strncpy(video_codec.plName, args.codecName.c_str(), 32);
    video_codec.plType = VCM_VP8_PAYLOAD_TYPE;
    video_codec.maxBitrate = 10000;
    video_codec.codecType = args.codecType;
    TEST(rtp_receiver->RegisterReceivePayload(video_codec.plName,
                                              video_codec.plType,
                                              90000,
                                              0,
                                              video_codec.maxBitrate) == 0);
    TEST(rtp->RegisterSendPayload(video_codec) == 0);

    // inform RTP Module of error resilience features
    TEST(rtp->SetGenericFECStatus(fecEnabled, VCM_RED_PAYLOAD_TYPE,
                                  VCM_ULPFEC_PAYLOAD_TYPE) == 0);

    //VCM
    if (vcm->InitializeReceiver() < 0)
    {
        return -1;
    }
    if (vcm->InitializeSender())
    {
        return -1;
    }
    // registering codecs for the VCM module
    VideoCodec sendCodec;
    vcm->InitializeSender();
    int32_t numberOfCodecs = vcm->NumberOfCodecs();
    if (numberOfCodecs < 1)
    {
        return -1;
    }

    if (vcm->Codec(args.codecType, &sendCodec) != 0)
    {
        // desired codec unavailable
        printf("Codec not registered\n");
        return -1;
    }
    // register codec
    sendCodec.startBitrate = (int) bitRate;
    sendCodec.height = height;
    sendCodec.width = width;
    sendCodec.maxFramerate = (uint8_t)frameRate;
    vcm->RegisterSendCodec(&sendCodec, numberOfCores, 1440);
    vcm->RegisterReceiveCodec(&sendCodec, numberOfCores); // same settings for encode and decode

    vcm->SetRenderDelay(renderDelayMs);
    vcm->SetMinimumPlayoutDelay(minPlayoutDelayMs);

    // Callback Settings

    PacketRequester packetRequester(*rtp);
    vcm->RegisterPacketRequestCallback(&packetRequester);

    VCMRTPEncodeCompleteCallback* encodeCompleteCallback = new VCMRTPEncodeCompleteCallback(rtp);
    vcm->RegisterTransportCallback(encodeCompleteCallback);
    encodeCompleteCallback->SetCodecType(ConvertCodecType(args.codecName.c_str()));
    encodeCompleteCallback->SetFrameDimensions(width, height);
    // frame ready to be sent to network

    VCMDecodeCompleteCallback receiveCallback(decodedFile);
    vcm->RegisterReceiveCallback(&receiveCallback);

    VideoProtectionCallback protectionCallback;
    vcm->RegisterProtectionCallback(&protectionCallback);

    outgoingTransport->SetLossPct(lossRate);
    // Nack support is currently not implemented in this test
    assert(nackEnabled == false);
    vcm->SetVideoProtection(kProtectionNack, nackEnabled);
    vcm->SetVideoProtection(kProtectionFEC, fecEnabled);

    // inform RTP Module of error resilience features
    FecProtectionParams delta_params = protectionCallback.DeltaFecParameters();
    FecProtectionParams key_params = protectionCallback.KeyFecParameters();
    rtp->SetFecParameters(&delta_params, &key_params);
    rtp_receiver->SetNACKStatus(nackEnabled ? kNackRtcp : kNackOff);

    vcm->SetChannelParameters(static_cast<uint32_t>(1000 * bitRate),
                              (uint8_t) lossRate, rttMS);

    SharedRTPState mtState(*vcm, *rtp); // receive side
    SendSharedState mtSendState(*vcm, *rtp, args); // send side

    /*START TEST*/

    // Create and start all threads
    // send side threads
    ThreadWrapper* mainSenderThread = ThreadWrapper::CreateThread(MainSenderThread,
            &mtSendState, kNormalPriority, "MainSenderThread");
    ThreadWrapper* intSenderThread = ThreadWrapper::CreateThread(IntSenderThread,
            &mtSendState, kNormalPriority, "IntThread");

    if (mainSenderThread != NULL)
    {
        unsigned int tid;
        mainSenderThread->Start(tid);
    }
    else
    {
        printf("Unable to start main sender thread\n");
        return -1;
    }

    if (intSenderThread != NULL)
    {
        unsigned int tid;
        intSenderThread->Start(tid);
    }
    else
    {
        printf("Unable to start sender interference thread\n");
        return -1;
    }

    // Receive side threads
    ThreadWrapper* processingThread = ThreadWrapper::CreateThread(ProcessingThread,
            &mtState, kNormalPriority, "ProcessingThread");
    ThreadWrapper* decodeThread = ThreadWrapper::CreateThread(DecodeThread,
            &mtState, kNormalPriority, "DecodeThread");

    if (processingThread != NULL)
    {
        unsigned int tid;
        processingThread->Start(tid);
    }
    else
    {
        printf("Unable to start processing thread\n");
        return -1;
    }

    if (decodeThread != NULL)
    {
        unsigned int tid;
        decodeThread->Start(tid);
    }
    else
    {
        printf("Unable to start decode thread\n");
        return -1;
    }

    EventWrapper& waitEvent = *EventWrapper::Create();

    // Decode for 10 seconds and then tear down and exit.
    waitEvent.Wait(30000);

    // Tear down

    while (!mainSenderThread->Stop())
    {
        ;
    }

    while (!intSenderThread->Stop())
    {
        ;
    }


    while (!processingThread->Stop())
    {
        ;
    }

    while (!decodeThread->Stop())
    {
        ;
    }

    printf("\nVCM MT RX/TX Test: \n\n%i tests completed\n", vcmMacrosTests);
    if (vcmMacrosErrors > 0)
    {
        printf("%i FAILED\n\n", vcmMacrosErrors);
    }
    else
    {
        printf("ALL PASSED\n\n");
    }

    delete &waitEvent;
    delete mainSenderThread;
    delete intSenderThread;
    delete processingThread;
    delete decodeThread;
    delete encodeCompleteCallback;
    delete outgoingTransport;
    VideoCodingModule::Destroy(vcm);
    delete rtp;
    rtp = NULL;
    vcm = NULL;
    Trace::ReturnTrace();
    fclose(decodedFile);
    printf("Multi-Thread test Done: View output file \n");
    return 0;

}
