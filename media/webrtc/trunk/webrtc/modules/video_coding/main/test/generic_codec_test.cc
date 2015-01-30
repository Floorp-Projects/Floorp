/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/main/test/generic_codec_test.h"

#include <math.h>
#include <stdio.h>

#include "webrtc/common_video/interface/i420_video_frame.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "webrtc/modules/video_coding/main/interface/video_coding.h"
#include "webrtc/modules/video_coding/main/test/test_macros.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/test/testsupport/fileutils.h"

using namespace webrtc;

enum { kMaxWaitEncTimeMs = 100 };

int GenericCodecTest::RunTest(CmdArgs& args)
{
    SimulatedClock clock(0);
    NullEventFactory event_factory;
    VideoCodingModule* vcm = VideoCodingModule::Create(&clock, &event_factory);
    GenericCodecTest* get = new GenericCodecTest(vcm, &clock);
    Trace::CreateTrace();
    Trace::SetTraceFile(
        (test::OutputPath() + "genericCodecTestTrace.txt").c_str());
    Trace::set_level_filter(webrtc::kTraceAll);
    get->Perform(args);
    Trace::ReturnTrace();
    delete get;
    VideoCodingModule::Destroy(vcm);
    return 0;
}

GenericCodecTest::GenericCodecTest(VideoCodingModule* vcm,
                                   SimulatedClock* clock):
_clock(clock),
_vcm(vcm),
_width(0),
_height(0),
_frameRate(0),
_lengthSourceFrame(0),
_timeStamp(0)
{
}

GenericCodecTest::~GenericCodecTest()
{
}

void
GenericCodecTest::Setup(CmdArgs& args)
{
    _timeStamp = 0;

    /* Test Sequence parameters */

    _inname= args.inputFile;
    if (args.outputFile.compare(""))
        _outname = test::OutputPath() + "GCTest_decoded.yuv";
    else
        _outname = args.outputFile;
    _encodedName = test::OutputPath() + "GCTest_encoded.vp8";
    _width = args.width;
    _height = args.height;
    _frameRate = args.frameRate;
    _lengthSourceFrame  = 3*_width*_height/2;

    /* File settings */

    if ((_sourceFile = fopen(_inname.c_str(), "rb")) == NULL)
    {
        printf("Cannot read file %s.\n", _inname.c_str());
        exit(1);
    }
    if ((_encodedFile = fopen(_encodedName.c_str(), "wb")) == NULL)
    {
        printf("Cannot write encoded file.\n");
        exit(1);
    }
    if ((_decodedFile = fopen(_outname.c_str(),  "wb")) == NULL)
    {
        printf("Cannot write file %s.\n", _outname.c_str());
        exit(1);
    }

    return;
}
int32_t
GenericCodecTest::Perform(CmdArgs& args)
{
    int32_t ret;
    Setup(args);
    /*
    1. sanity checks
    2. encode/decoder individuality
    3. API testing
    4. Target bitrate (within a specific timespan)
    5. Pipeline Delay
    */

    /*******************************/
    /* sanity checks on inputs    */
    /*****************************/
    VideoCodec sendCodec, receiveCodec;
    sendCodec.maxBitrate = 8000;
    TEST(_vcm->NumberOfCodecs() > 0); // This works since we now initialize the list in the constructor
    TEST(_vcm->Codec(0, &sendCodec)  == VCM_OK);
    _vcm->InitializeSender();
    _vcm->InitializeReceiver();
    int32_t NumberOfCodecs = _vcm->NumberOfCodecs();
    // registration of first codec in the list
    int i = 0;
    _vcm->Codec(0, &_sendCodec);
    TEST(_vcm->RegisterSendCodec(&_sendCodec, 4, 1440) == VCM_OK);
    // sanity on encoder registration
    I420VideoFrame sourceFrame;
    _vcm->InitializeSender();
    TEST(_vcm->Codec(kVideoCodecVP8, &sendCodec) == 0);
    sendCodec.maxBitrate = 8000;
    _vcm->RegisterSendCodec(&sendCodec, 1, 1440);
    _vcm->InitializeSender();
    _vcm->Codec(kVideoCodecVP8, &sendCodec);
    sendCodec.height = 0;
    TEST(_vcm->RegisterSendCodec(&sendCodec, 1, 1440) < 0); // bad height
    _vcm->Codec(kVideoCodecVP8, &sendCodec);
    _vcm->Codec(kVideoCodecVP8, &sendCodec);
    _vcm->InitializeSender();
    // Setting rate when encoder uninitialized.
    TEST(_vcm->SetChannelParameters(100000, 0, 0) < 0);
    // register all availbale decoders -- need to have more for this test
    for (i=0; i< NumberOfCodecs; i++)
    {
        _vcm->Codec(i, &receiveCodec);
        _vcm->RegisterReceiveCodec(&receiveCodec, 1);
    }
    uint8_t* tmpBuffer = new uint8_t[_lengthSourceFrame];
    TEST(fread(tmpBuffer, 1, _lengthSourceFrame, _sourceFile) > 0);
    int half_width = (_width + 1) / 2;
    int half_height = (_height + 1) / 2;
    int size_y = _width * _height;
    int size_uv = half_width * half_height;
    sourceFrame.CreateFrame(size_y, tmpBuffer,
                            size_uv, tmpBuffer + size_y,
                            size_uv, tmpBuffer + size_y + size_uv,
                            _width, _height,
                            _width, half_width, half_width);
    sourceFrame.set_timestamp(_timeStamp++);
    TEST(_vcm->AddVideoFrame(sourceFrame) < 0 ); // encoder uninitialized
    _vcm->InitializeReceiver();
    // Setting rtt when receiver uninitialized.
    TEST(_vcm->SetChannelParameters(100000, 0, 0) < 0);

      /**************************************/
     /* encoder/decoder individuality test */
    /**************************************/
    //Register both encoder and decoder, reset decoder - encode, set up decoder, reset encoder - decode.
    rewind(_sourceFile);
    _vcm->InitializeReceiver();
    _vcm->InitializeSender();
    NumberOfCodecs = _vcm->NumberOfCodecs();
    // Register VP8
    _vcm->Codec(kVideoCodecVP8, &_sendCodec);
    _vcm->RegisterSendCodec(&_sendCodec, 4, 1440);
    _vcm->SendCodec(&sendCodec);
    sendCodec.startBitrate = 2000;

    // Set target frame rate to half of the incoming frame rate
    // to test the frame rate control in the VCM
    sendCodec.maxFramerate = (uint8_t)(_frameRate / 2);
    sendCodec.width = _width;
    sendCodec.height = _height;
    TEST(strncmp(_sendCodec.plName, "VP8", 3) == 0); // was VP8

    _decodeCallback = new VCMDecodeCompleteCallback(_decodedFile);
    _encodeCompleteCallback = new VCMEncodeCompleteCallback(_encodedFile);
    _vcm->RegisterReceiveCallback(_decodeCallback);
    _vcm->RegisterTransportCallback(_encodeCompleteCallback);
    _encodeCompleteCallback->RegisterReceiverVCM(_vcm);

    _vcm->RegisterSendCodec(&sendCodec, 4, 1440);
    _encodeCompleteCallback->SetCodecType(ConvertCodecType(sendCodec.plName));

    _vcm->InitializeReceiver();
    _vcm->Process();

    //encoding 1 second of video
    for (i = 0; i < _frameRate; i++)
    {
        TEST(fread(tmpBuffer, 1, _lengthSourceFrame, _sourceFile) > 0);
        sourceFrame.CreateFrame(size_y, tmpBuffer,
                                size_uv, tmpBuffer + size_y,
                                size_uv, tmpBuffer + size_y + size_uv,
                                _width, _height,
                                _width, half_width, half_width);
        _timeStamp += (uint32_t)(9e4 / static_cast<float>(_frameRate));
        sourceFrame.set_timestamp(_timeStamp);
        TEST(_vcm->AddVideoFrame(sourceFrame) == VCM_OK);
        IncrementDebugClock(_frameRate);
        _vcm->Process();
    }
    sendCodec.maxFramerate = (uint8_t)_frameRate;
    _vcm->InitializeSender();
    TEST(_vcm->RegisterReceiveCodec(&sendCodec, 1) == VCM_OK); // same codec for encode and decode
    ret = 0;
    i = 0;
    while ((i < 25) && (ret == 0) )
    {
        ret = _vcm->Decode();
        TEST(ret == VCM_OK);
        if (ret < 0)
        {
            printf("error in frame # %d \n", i);
        }
        IncrementDebugClock(_frameRate);
        i++;
    }
    //TEST((ret == 0) && (i = 50));
    if (ret == 0)
    {
        printf("Encoder/Decoder individuality test complete - View output files \n");
    }
    // last frame - not decoded
    _vcm->InitializeReceiver();
    TEST(_vcm->Decode() < 0); // frame to be encoded exists, decoder uninitialized


    // Test key frame request on packet loss mode.
    // This a frame as a key frame and fooling the receiver
    // that the last packet was lost. The decoding will succeed,
    // but the VCM will see a packet loss and request a new key frame.
    VCMEncComplete_KeyReqTest keyReqTest_EncCompleteCallback(*_vcm);
    KeyFrameReqTest frameTypeCallback;
    _vcm->RegisterTransportCallback(&keyReqTest_EncCompleteCallback);
    _encodeCompleteCallback->RegisterReceiverVCM(_vcm);
    _vcm->RegisterSendCodec(&sendCodec, 4, 1440);
    _encodeCompleteCallback->SetCodecType(ConvertCodecType(sendCodec.plName));
    TEST(_vcm->SetVideoProtection(kProtectionKeyOnKeyLoss, true) == VCM_OK);
    TEST(_vcm->RegisterFrameTypeCallback(&frameTypeCallback) == VCM_OK);
    TEST(_vcm->RegisterReceiveCodec(&sendCodec, 1) == VCM_OK);
    TEST(_vcm->AddVideoFrame(sourceFrame) == VCM_OK);
    _timeStamp += (uint32_t)(9e4 / static_cast<float>(_frameRate));
    sourceFrame.set_timestamp(_timeStamp);
    // First packet of a subsequent frame required before the jitter buffer
    // will allow decoding an incomplete frame.
    TEST(_vcm->AddVideoFrame(sourceFrame) == VCM_OK);
    TEST(_vcm->Decode() == VCM_OK);

    printf("API tests complete \n");

     /*******************/
    /* Bit Rate Tests */
    /*****************/
    /* Requirements:
    * 1. OneSecReq = 15 % above/below target over a time period of 1s (_frameRate number of frames)
    * 3. FullReq  = 10% for total seq. (for 300 frames/seq. coincides with #1)
    * 4. Test will go over all registered codecs
    //NOTE: time requirements are not part of the release tests
    */
    double FullReq   =  0.1;
    //double OneSecReq = 0.15;
    printf("\n RATE CONTROL TEST\n");
    // initializing....
    _vcm->InitializeSender();
    _vcm->InitializeReceiver();
    rewind(_sourceFile);
    sourceFrame.CreateEmptyFrame(_width, _height, _width,
                                 (_width + 1) / 2, (_width + 1) / 2);
    const float bitRate[] = {100, 400, 600, 1000, 2000};
    const float nBitrates = sizeof(bitRate)/sizeof(*bitRate);
    float _bitRate = 0;
    int _frameCnt = 0;
    float totalBytesOneSec = 0;//, totalBytesTenSec;
    float totalBytes, actualBitrate;
    VCMFrameCount frameCount; // testing frame type counters
    // start test
    NumberOfCodecs = _vcm->NumberOfCodecs();
    // going over all available codecs
    _encodeCompleteCallback->SetFrameDimensions(_width, _height);
    SendStatsTest sendStats;
    for (int k = 0; k < NumberOfCodecs; k++)
    //for (int k = NumberOfCodecs - 1; k >=0; k--)
    {// static list starts from 0
        //just checking
        _vcm->InitializeSender();
        _sendCodec.maxBitrate = 8000;
        TEST(_vcm->Codec(k, &_sendCodec)== VCM_OK);
        _vcm->RegisterSendCodec(&_sendCodec, 1, 1440);
        _vcm->RegisterTransportCallback(_encodeCompleteCallback);
        _encodeCompleteCallback->SetCodecType(ConvertCodecType(_sendCodec.plName));
        printf (" \n\n Codec type = %s \n\n",_sendCodec.plName);
        for (i = 0; i < nBitrates; i++)
        {
             _bitRate = static_cast<float>(bitRate[i]);
            // just testing
            _vcm->InitializeSender();
            _sendCodec.startBitrate = (int)_bitRate;
            _sendCodec.maxBitrate = 8000;
            _sendCodec.maxFramerate = _frameRate;
            _vcm->RegisterSendCodec(&_sendCodec, 1, 1440);
            _vcm->RegisterTransportCallback(_encodeCompleteCallback);
            // up to here
            _vcm->SetChannelParameters(static_cast<uint32_t>(1000 * _bitRate),
                                       0, 20);
            _frameCnt = 0;
            totalBytes = 0;
            _encodeCompleteCallback->Initialize();
            sendStats.set_framerate(static_cast<uint32_t>(_frameRate));
            sendStats.set_bitrate(1000 * _bitRate);
            _vcm->RegisterSendStatisticsCallback(&sendStats);
            while (fread(tmpBuffer, 1, _lengthSourceFrame, _sourceFile) ==
                _lengthSourceFrame)
            {
                _frameCnt++;
                sourceFrame.CreateFrame(size_y, tmpBuffer,
                                        size_uv, tmpBuffer + size_y,
                                        size_uv, tmpBuffer + size_y + size_uv,
                                        _width, _height,
                                        _width, (_width + 1) / 2,
                                        (_width + 1) / 2);
                _timeStamp += (uint32_t)(9e4 / static_cast<float>(_frameRate));
                sourceFrame.set_timestamp(_timeStamp);

                ret = _vcm->AddVideoFrame(sourceFrame);
                IncrementDebugClock(_frameRate);
                // The following should be uncommneted for timing tests. Release tests only include
                // compliance with full sequence bit rate.
                if (_frameCnt == _frameRate)// @ 1sec
                {
                    totalBytesOneSec =  _encodeCompleteCallback->EncodedBytes();//totalBytes;
                }
                TEST(_vcm->TimeUntilNextProcess() >= 0);
            }  // video seq. encode done
            TEST(_vcm->TimeUntilNextProcess() == 0);
            _vcm->Process(); // Let the module calculate its send bit rate estimate
            // estimating rates
            // complete sequence
            // bit rate assumes input frame rate is as specified
            totalBytes = _encodeCompleteCallback->EncodedBytes();
            actualBitrate = (float)(8.0/1000)*(totalBytes / (_frameCnt / _frameRate));

            printf("Complete Seq.: target bitrate: %.0f kbps, actual bitrate: %.1f kbps\n", _bitRate, actualBitrate);
            TEST((fabs(actualBitrate - _bitRate) < FullReq * _bitRate) ||
                 (strncmp(_sendCodec.plName, "I420", 4) == 0));

            // 1 Sec.
            actualBitrate = (float)(8.0/1000)*(totalBytesOneSec);
            //actualBitrate = (float)(8.0*totalBytesOneSec)/(oneSecTime - startTime);
            //printf("First 1Sec: target bitrate: %.0f kbps, actual bitrate: %.1f kbps\n", _bitRate, actualBitrate);
            //TEST(fabs(actualBitrate - _bitRate) < OneSecReq * _bitRate);
            rewind(_sourceFile);

            //checking key/delta frame count
            _vcm->SentFrameCount(frameCount);
            printf("frame count: %d delta, %d key\n", frameCount.numDeltaFrames, frameCount.numKeyFrames);
        }// end per codec

    }  // end rate control test
    /********************************/
    /* Encoder Pipeline Delay Test */
    /******************************/
    _vcm->InitializeSender();
    NumberOfCodecs = _vcm->NumberOfCodecs();
    bool encodeComplete = false;
    // going over all available codecs
    for (int k = 0; k < NumberOfCodecs; k++)
    {
        _vcm->Codec(k, &_sendCodec);
        _vcm->InitializeSender();
        _sendCodec.maxBitrate = 8000;
        _vcm->RegisterSendCodec(&_sendCodec, 4, 1440);
        _vcm->RegisterTransportCallback(_encodeCompleteCallback);

        _frameCnt = 0;
        encodeComplete = false;
        while (encodeComplete == false)
        {
            TEST(fread(tmpBuffer, 1, _lengthSourceFrame, _sourceFile) > 0);
            _frameCnt++;
            sourceFrame.CreateFrame(size_y, tmpBuffer,
                                    size_uv, tmpBuffer + size_y,
                                    size_uv, tmpBuffer + size_y + size_uv,
                                    _width, _height,
                                    _width, half_width, half_width);
            _timeStamp += (uint32_t)(9e4 / static_cast<float>(_frameRate));
            sourceFrame.set_timestamp(_timeStamp);
            _vcm->AddVideoFrame(sourceFrame);
            encodeComplete = _encodeCompleteCallback->EncodeComplete();
        }  // first frame encoded
        printf ("\n Codec type = %s \n", _sendCodec.plName);
        printf(" Encoder pipeline delay = %d frames\n", _frameCnt - 1);
    }  // end for all codecs

    /********************************/
    /* Encoder Packet Size Test     */
    /********************************/
    RTPSendCallback_SizeTest sendCallback;

    RtpRtcp::Configuration configuration;
    configuration.id = 1;
    configuration.audio = false;
    configuration.outgoing_transport = &sendCallback;

    RtpRtcp& rtpModule = *RtpRtcp::CreateRtpRtcp(configuration);

    VCMRTPEncodeCompleteCallback encCompleteCallback(&rtpModule);
    _vcm->InitializeSender();

    // Test temporal decimation settings
    for (int k = 0; k < NumberOfCodecs; k++)
    {
        _vcm->Codec(k, &_sendCodec);
        if (strncmp(_sendCodec.plName, "I420", 4) == 0)
        {
            // Only test with I420
            break;
        }
    }
    TEST(strncmp(_sendCodec.plName, "I420", 4) == 0);
    _vcm->InitializeSender();
    _sendCodec.maxFramerate = static_cast<uint8_t>(_frameRate / 2.0 + 0.5f);
    _vcm->RegisterSendCodec(&_sendCodec, 4, 1440);
    _vcm->SetChannelParameters(2000000, 0, 0);
    _vcm->RegisterTransportCallback(_encodeCompleteCallback);
    // up to here
    _vcm->SetChannelParameters(static_cast<uint32_t>(1000 * _bitRate), 0, 20);
    _encodeCompleteCallback->Initialize();
    sendStats.set_framerate(static_cast<uint32_t>(_frameRate));
    sendStats.set_bitrate(1000 * _bitRate);
    _vcm->RegisterSendStatisticsCallback(&sendStats);
    rewind(_sourceFile);
    while (fread(tmpBuffer, 1, _lengthSourceFrame, _sourceFile) ==
        _lengthSourceFrame) {
        sourceFrame.CreateFrame(size_y, tmpBuffer,
                                size_uv, tmpBuffer + size_y,
                                size_uv, tmpBuffer + size_y + size_uv,
                                _width, _height,
                                _width, half_width, half_width);
        _timeStamp += (uint32_t)(9e4 / static_cast<float>(_frameRate));
        sourceFrame.set_timestamp(_timeStamp);
        ret = _vcm->AddVideoFrame(sourceFrame);
        if (_vcm->TimeUntilNextProcess() <= 0)
        {
            _vcm->Process();
        }
        IncrementDebugClock(_frameRate);
    }  // first frame encoded

    delete &rtpModule;
    Print();
    delete tmpBuffer;
    delete _decodeCallback;
    delete _encodeCompleteCallback;
    return 0;
}


void
GenericCodecTest::Print()
{
    printf(" \n\n VCM Generic Encoder Test: \n\n%i tests completed\n", vcmMacrosTests);
    if (vcmMacrosErrors > 0)
    {
        printf("%i FAILED\n\n", vcmMacrosErrors);
    }
    else
    {
        printf("ALL PASSED\n\n");
    }
}

float
GenericCodecTest::WaitForEncodedFrame() const
{
    int64_t startTime = _clock->TimeInMilliseconds();
    while (_clock->TimeInMilliseconds() - startTime < kMaxWaitEncTimeMs*10)
    {
        if (_encodeCompleteCallback->EncodeComplete())
        {
            return _encodeCompleteCallback->EncodedBytes();
        }
    }
    return 0;
}

void
GenericCodecTest::IncrementDebugClock(float frameRate)
{
    _clock->AdvanceTimeMilliseconds(1000/frameRate);
}

int
RTPSendCallback_SizeTest::SendPacket(int channel, const void *data, int len)
{
    _nPackets++;
    _payloadSizeSum += len;
    // Make sure no payloads (len - header size) are larger than maxPayloadSize
    TEST(len > 0 && static_cast<uint32_t>(len - 12) <= _maxPayloadSize);
    return 0;
}

void
RTPSendCallback_SizeTest::SetMaxPayloadSize(uint32_t maxPayloadSize)
{
    _maxPayloadSize = maxPayloadSize;
}

void
RTPSendCallback_SizeTest::Reset()
{
    _nPackets = 0;
    _payloadSizeSum = 0;
}

float
RTPSendCallback_SizeTest::AveragePayloadSize() const
{
    if (_nPackets > 0)
    {
        return _payloadSizeSum / static_cast<float>(_nPackets);
    }
    return 0;
}

int32_t
VCMEncComplete_KeyReqTest::SendData(
        const FrameType frameType,
        const uint8_t payloadType,
        const uint32_t timeStamp,
        int64_t capture_time_ms,
        const uint8_t* payloadData,
        const uint32_t payloadSize,
        const RTPFragmentationHeader& /*fragmentationHeader*/,
        const webrtc::RTPVideoHeader* /*videoHdr*/)
{
    WebRtcRTPHeader rtpInfo;
    rtpInfo.header.markerBit = true; // end of frame
    rtpInfo.type.Video.codecHeader.VP8.InitRTPVideoHeaderVP8();
    rtpInfo.type.Video.codec = kRtpVideoVp8;
    rtpInfo.header.payloadType = payloadType;
    rtpInfo.header.sequenceNumber = _seqNo;
    _seqNo += 2;
    rtpInfo.header.ssrc = 0;
    rtpInfo.header.timestamp = _timeStamp;
    _timeStamp += 3000;
    rtpInfo.type.Video.isFirstPacket = false;
    rtpInfo.frameType = kVideoFrameKey;
    return _vcm.IncomingPacket(payloadData, payloadSize, rtpInfo);
}
