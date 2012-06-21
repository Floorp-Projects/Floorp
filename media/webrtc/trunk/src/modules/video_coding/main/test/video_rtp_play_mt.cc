/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "receiver_tests.h"
#include "video_coding.h"
#include "rtp_rtcp.h"
#include "trace.h"
#include "thread_wrapper.h"
#include "../source/event.h"
#include "test_macros.h"
#include "rtp_player.h"

#include <string.h>

using namespace webrtc;

bool ProcessingThread(void* obj)
{
    SharedState* state = static_cast<SharedState*>(obj);
    if (state->_vcm.TimeUntilNextProcess() <= 0)
    {
        if (state->_vcm.Process() < 0)
        {
            return false;
        }
    }
    return true;
}

bool RtpReaderThread(void* obj)
{
    SharedState* state = static_cast<SharedState*>(obj);
    EventWrapper& waitEvent = *EventWrapper::Create();
    // RTP stream main loop
    TickTimeBase clock;
    if (state->_rtpPlayer.NextPacket(clock.MillisecondTimestamp()) < 0)
    {
        return false;
    }
    waitEvent.Wait(state->_rtpPlayer.TimeUntilNextPacket());
    delete &waitEvent;
    return true;
}

bool DecodeThread(void* obj)
{
    SharedState* state = static_cast<SharedState*>(obj);
    state->_vcm.Decode(10000);
    while (state->_vcm.DecodeDualFrame(0) == 1);
    return true;
}

int RtpPlayMT(CmdArgs& args, int releaseTestNo, webrtc::VideoCodecType releaseTestVideoType)
{
    // Don't run these tests with debug events.
#if defined(EVENT_DEBUG)
    return -1;
#endif

    // BEGIN Settings

    bool protectionEnabled = true;
    VCMVideoProtection protection = kProtectionDualDecoder;
    WebRtc_UWord8 rttMS = 50;
    float lossRate = 0.05f;
    WebRtc_UWord32 renderDelayMs = 0;
    WebRtc_UWord32 minPlayoutDelayMs = 0;
    const WebRtc_Word64 MAX_RUNTIME_MS = 10000;
    std::string outFilename = args.outputFile;
    if (outFilename == "")
        outFilename = test::OutputPath() + "RtpPlayMT_decoded.yuv";

    bool nackEnabled = (protectionEnabled &&
                (protection == kProtectionDualDecoder ||
                protection == kProtectionNack ||
                kProtectionNackFEC));
    TickTimeBase clock;
    VideoCodingModule* vcm =
            VideoCodingModule::Create(1, &clock);
    RtpDataCallback dataCallback(vcm);
    std::string rtpFilename;
    rtpFilename = args.inputFile;
    if (releaseTestNo > 0)
    {
        // Setup a release test
        switch (releaseTestVideoType)
        {
        case webrtc::kVideoCodecVP8:
            rtpFilename = args.inputFile;
            outFilename = test::OutputPath() + "MTReceiveTest_VP8";
            break;
        default:
            return -1;
        }
        switch (releaseTestNo)
        {
        case 1:
            // Normal execution
            protectionEnabled = false;
            nackEnabled = false;
            rttMS = 0;
            lossRate = 0.0f;
            outFilename += "_Normal.yuv";
            break;
        case 2:
            // Packet loss
            protectionEnabled = false;
            nackEnabled = false;
            rttMS = 0;
            lossRate = 0.05f;
            outFilename += "_0.05.yuv";
            break;
        case 3:
            // Packet loss and NACK
            protection = kProtectionNack;
            nackEnabled = true;
            protectionEnabled = true;
            rttMS = 100;
            lossRate = 0.05f;
            outFilename += "_0.05_NACK_100ms.yuv";
            break;
        case 4:
            // Packet loss and dual decoder
            // Not implemented
            return 0;
            break;
        default:
            return -1;
        }
        printf("Watch %s to verify that the output is reasonable\n", outFilename.c_str());
    }
    RTPPlayer rtpStream(rtpFilename.c_str(), &dataCallback, &clock);
    PayloadTypeList payloadTypes;
    payloadTypes.push_front(new PayloadCodecTuple(VCM_VP8_PAYLOAD_TYPE, "VP8",
                                                  kVideoCodecVP8));
    Trace::CreateTrace();
    Trace::SetTraceFile("receiverTestTrace.txt");
    Trace::SetLevelFilter(webrtc::kTraceAll);

    // END Settings

    // Set up

    SharedState mtState(*vcm, rtpStream);

    if (rtpStream.Initialize(&payloadTypes) < 0)
    {
        return -1;
    }
    rtpStream.SimulatePacketLoss(lossRate, nackEnabled, rttMS);

    WebRtc_Word32 ret = vcm->InitializeReceiver();
    if (ret < 0)
    {
        return -1;
    }

    // Create and start all threads
    ThreadWrapper* processingThread = ThreadWrapper::CreateThread(ProcessingThread,
            &mtState, kNormalPriority, "ProcessingThread");
    ThreadWrapper* rtpReaderThread = ThreadWrapper::CreateThread(RtpReaderThread,
            &mtState, kNormalPriority, "RtpReaderThread");
    ThreadWrapper* decodeThread = ThreadWrapper::CreateThread(DecodeThread,
            &mtState, kNormalPriority, "DecodeThread");

    // Register receive codecs in VCM
    for (PayloadTypeList::iterator it = payloadTypes.begin();
        it != payloadTypes.end(); ++it) {
        PayloadCodecTuple* payloadType = *it;
        if (payloadType != NULL)
        {
            VideoCodec codec;
            VideoCodingModule::Codec(payloadType->codecType, &codec);
            codec.plType = payloadType->payloadType;
            if (vcm->RegisterReceiveCodec(&codec, 1) < 0)
            {
                return -1;
            }
        }
    }

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
    if (rtpReaderThread != NULL)
    {
        unsigned int tid;
        rtpReaderThread->Start(tid);
    }
    else
    {
        printf("Unable to start RTP reader thread\n");
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

    FrameReceiveCallback receiveCallback(outFilename);
    vcm->RegisterReceiveCallback(&receiveCallback);
    vcm->RegisterPacketRequestCallback(&rtpStream);

    vcm->SetChannelParameters(0, 0, rttMS);
    vcm->SetVideoProtection(protection, protectionEnabled);
    vcm->SetRenderDelay(renderDelayMs);
    vcm->SetMinimumPlayoutDelay(minPlayoutDelayMs);

    EventWrapper& waitEvent = *EventWrapper::Create();

    // Decode for 10 seconds and then tear down and exit.
    waitEvent.Wait(MAX_RUNTIME_MS);

    // Tear down
    while (!payloadTypes.empty())
    {
        delete payloadTypes.front();
        payloadTypes.pop_front();
    }
    while (!processingThread->Stop())
    {
        ;
    }
    while (!rtpReaderThread->Stop())
    {
        ;
    }
    while (!decodeThread->Stop())
    {
        ;
    }
    VideoCodingModule::Destroy(vcm);
    vcm = NULL;
    delete &waitEvent;
    delete processingThread;
    delete decodeThread;
    delete rtpReaderThread;
    rtpStream.Print();
    Trace::ReturnTrace();
    return 0;
}
