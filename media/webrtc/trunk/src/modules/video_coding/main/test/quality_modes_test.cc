/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "quality_modes_test.h"

#include <iostream>
#include <string>
#include <time.h>

#include "../source/event.h"
#include "modules/video_coding/main/source/tick_time_base.h"
#include "test_callbacks.h"
#include "test_macros.h"
#include "testsupport/metrics/video_metrics.h"
#include "common_video/libyuv/include/libyuv.h"

using namespace webrtc;

int qualityModeTest()
{
    // Don't run this test with debug events.
#if defined(EVENT_DEBUG)
    return -1;
#endif
    TickTimeBase clock;
    VideoCodingModule* vcm = VideoCodingModule::Create(1, &clock);
    QualityModesTest QMTest(vcm, &clock);
    QMTest.Perform();
    VideoCodingModule::Destroy(vcm);
    return 0;
}


QualityModesTest::QualityModesTest(VideoCodingModule* vcm,
                                   TickTimeBase* clock):
NormalTest(vcm, clock),
_vpm()
{
    //
}


QualityModesTest::~QualityModesTest()
{
    //
}

void
QualityModesTest::Setup()
{


    _inname= test::ProjectRootPath() + "resources/crew_30f_4CIF.yuv";
    _outname = test::OutputPath() + "out_qmtest.yuv";
    _encodedName = test::OutputPath() + "encoded_qmtest.yuv";

    //NATIVE/SOURCE VALUES
    _nativeWidth = 2*352;
    _nativeHeight = 2*288;
    _nativeFrameRate = 30;


    //TARGET/ENCODER VALUES
     _width = 2*352;
     _height = 2*288;
    _frameRate = 30;
    //
    _bitRate = 400;

    _flagSSIM = false;

    _lengthSourceFrame  = 3*_nativeWidth*_nativeHeight/2;

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

    _log.open((test::OutputPath() + "TestLog.txt").c_str(),
              std::fstream::out | std::fstream::app);
    return;
}


void
QualityModesTest::Print()
{
    std::cout << "Quality Modes Test Completed!" << std::endl;
    (_log) << "Quality Modes Test Completed!" << std::endl;
    (_log) << "Input file: " << _inname << std::endl;
    (_log) << "Output file: " << _outname << std::endl;
    (_log) << "Total run time: " << _testTotalTime << std::endl;
    printf("Total run time: %f s \n", _testTotalTime);
    double ActualBitRate =  8.0 *( _sumEncBytes / (_frameCnt / _nativeFrameRate));
    double actualBitRate = ActualBitRate / 1000.0;
    double avgEncTime = _totalEncodeTime / _frameCnt;
    double avgDecTime = _totalDecodeTime / _frameCnt;
    webrtc::test::QualityMetricsResult psnr,ssim;
    I420PSNRFromFiles(_inname.c_str(), _outname.c_str(), _nativeWidth,
                      _nativeHeight, &psnr);
    printf("Actual bitrate: %f kbps\n", actualBitRate);
    printf("Target bitrate: %f kbps\n", _bitRate);
    ( _log) << "Actual bitrate: " << actualBitRate<< " kbps\tTarget: " << _bitRate << " kbps" << std::endl;
    printf("Average encode time: %f s\n", avgEncTime);
    ( _log) << "Average encode time: " << avgEncTime << " s" << std::endl;
    printf("Average decode time: %f s\n", avgDecTime);
    ( _log) << "Average decode time: " << avgDecTime << " s" << std::endl;
    printf("PSNR: %f \n", psnr.average);
    printf("**Number of frames dropped in VPM***%d \n",_numFramesDroppedVPM);
    ( _log) << "PSNR: " << psnr.average << std::endl;
    if (_flagSSIM == 1)
    {
        printf("***computing SSIM***\n");
        I420SSIMFromFiles(_inname.c_str(), _outname.c_str(), _nativeWidth,
                          _nativeHeight, &ssim);
        printf("SSIM: %f \n", ssim.average);
    }
    (_log) << std::endl;

    printf("\nVCM Qualit Modes Test: \n\n%i tests completed\n", vcmMacrosTests);
    if (vcmMacrosErrors > 0)
    {
        printf("%i FAILED\n\n", vcmMacrosErrors);
    }
    else
    {
        printf("ALL PASSED\n\n");
    }
}
void
QualityModesTest::Teardown()
{
    _log.close();
    fclose(_sourceFile);
    fclose(_decodedFile);
    fclose(_encodedFile);
    return;
}


WebRtc_Word32
QualityModesTest::Perform()
{
    Setup();
    // changing bit/frame rate during the test
    const float bitRateUpdate[] = {1000};
    const float frameRateUpdate[] = {30};
    const int updateFrameNum[] = {10000}; // frame numbers at which an update will occur

    WebRtc_UWord32 numChanges = sizeof(updateFrameNum)/sizeof(*updateFrameNum);
    WebRtc_UWord8 change = 0;// change counter

    _vpm = VideoProcessingModule::Create(1);

    EventWrapper* waitEvent = EventWrapper::Create();
    VideoCodec codec;//both send and receive
    _vcm->InitializeReceiver();
    _vcm->InitializeSender();
    WebRtc_Word32 NumberOfCodecs = _vcm->NumberOfCodecs();
    for (int i = 0; i < NumberOfCodecs; i++)
    {
        _vcm->Codec(i, &codec);
        if(strncmp(codec.plName,"VP8" , 5) == 0)
        {
             codec.startBitrate = (int)_bitRate;
             codec.maxFramerate = (WebRtc_UWord8) _frameRate;
             codec.width = (WebRtc_UWord16)_width;
             codec.height = (WebRtc_UWord16)_height;
             TEST(_vcm->RegisterSendCodec(&codec, 2, 1440) == VCM_OK);// will also set and init the desired codec
             i = NumberOfCodecs;
        }
    }

    // register a decoder (same codec for decoder and encoder )
    TEST(_vcm->RegisterReceiveCodec(&codec, 2) == VCM_OK);
    /* Callback Settings */
    VCMQMDecodeCompleCallback  _decodeCallback(_decodedFile);
    _vcm->RegisterReceiveCallback(&_decodeCallback);
    VCMNTEncodeCompleteCallback   _encodeCompleteCallback(_encodedFile, *this);
    _vcm->RegisterTransportCallback(&_encodeCompleteCallback);
    // encode and decode with the same vcm
    _encodeCompleteCallback.RegisterReceiverVCM(_vcm);

    //quality modes callback
    QMTestVideoSettingsCallback QMCallback;
    QMCallback.RegisterVCM(_vcm);
    QMCallback.RegisterVPM(_vpm);
    _vcm->RegisterVideoQMCallback(&QMCallback);

    ///////////////////////
    /// Start Test
    ///////////////////////
    _vpm->EnableTemporalDecimation(true);
    _vpm->EnableContentAnalysis(true);
    _vpm->SetInputFrameResampleMode(kFastRescaling);

    // disabling internal VCM frame dropper
    _vcm->EnableFrameDropper(false);

    VideoFrame sourceFrame;
    VideoFrame *decimatedFrame = NULL;
    sourceFrame.VerifyAndAllocate(_lengthSourceFrame);
    WebRtc_UWord8* tmpBuffer = new WebRtc_UWord8[_lengthSourceFrame];
    double startTime = clock()/(double)CLOCKS_PER_SEC;
    _vcm->SetChannelParameters((WebRtc_UWord32)_bitRate, 0, 0);

    SendStatsTest sendStats;
    sendStats.SetTargetFrameRate(static_cast<WebRtc_UWord32>(_frameRate));
    _vcm->RegisterSendStatisticsCallback(&sendStats);

    VideoContentMetrics* contentMetrics = NULL;
    // setting user frame rate
    _vpm->SetMaxFrameRate((WebRtc_UWord32)(_nativeFrameRate+ 0.5f));
    // for starters: keeping native values:
    _vpm->SetTargetResolution(_width, _height, (WebRtc_UWord32)(_frameRate+ 0.5f));
    _decodeCallback.SetOriginalFrameDimensions(_nativeWidth, _nativeHeight);

    //tmp  - disabling VPM frame dropping
    _vpm->EnableTemporalDecimation(false);


    WebRtc_Word32 ret = 0;
      _numFramesDroppedVPM = 0;

    while (feof(_sourceFile)== 0)
    {
        TEST(fread(tmpBuffer, 1, _lengthSourceFrame, _sourceFile) > 0);
        _frameCnt++;
        sourceFrame.CopyFrame(_lengthSourceFrame, tmpBuffer);
        sourceFrame.SetHeight(_nativeHeight);
        sourceFrame.SetWidth(_nativeWidth);

        _timeStamp += (WebRtc_UWord32)(9e4 / static_cast<float>(codec.maxFramerate));
        sourceFrame.SetTimeStamp(_timeStamp);

        ret = _vpm->PreprocessFrame(&sourceFrame, &decimatedFrame);
        if (ret  == 1)
        {
            printf("VD: frame drop %d \n",_frameCnt);
            _numFramesDroppedVPM += 1;
            continue; // frame drop
        }
        else if (ret < 0)
        {
            printf("Error in PreprocessFrame: %d\n", ret);
            //exit(1);
        }
        contentMetrics = _vpm->ContentMetrics();
        if (contentMetrics == NULL)
        {
            printf("error: contentMetrics = NULL\n");
        }

        // counting only encoding time
        _encodeTimes[int(sourceFrame.TimeStamp())] = clock()/(double)CLOCKS_PER_SEC;

        WebRtc_Word32 ret = _vcm->AddVideoFrame(*decimatedFrame, contentMetrics);

        _totalEncodeTime += clock()/(double)CLOCKS_PER_SEC - _encodeTimes[int(sourceFrame.TimeStamp())];

        if (ret < 0)
        {
            printf("Error in AddFrame: %d\n", ret);
            //exit(1);
        }
        _decodeTimes[int(sourceFrame.TimeStamp())] = clock()/(double)CLOCKS_PER_SEC; // same timestamp value for encode and decode
        ret = _vcm->Decode();
        _totalDecodeTime += clock()/(double)CLOCKS_PER_SEC - _decodeTimes[int(sourceFrame.TimeStamp())];
        if (ret < 0)
        {
            printf("Error in Decode: %d\n", ret);
            //exit(1);
        }
        if (_vcm->TimeUntilNextProcess() <= 0)
        {
            _vcm->Process();
        }
        // mimicking setTargetRates - update every 1 sec
        // this will trigger QMSelect
        if (_frameCnt%((int)_frameRate) == 0)
        {
            _vcm->SetChannelParameters((WebRtc_UWord32)_bitRate, 0, 1);
            waitEvent->Wait(33);
        }
        waitEvent->Wait(33);
        // check for bit rate update
        if (change < numChanges && _frameCnt == updateFrameNum[change])
        {
            _bitRate = bitRateUpdate[change];
            _frameRate = frameRateUpdate[change];
            codec.startBitrate = (int)_bitRate;
            codec.maxFramerate = (WebRtc_UWord8) _frameRate;
            TEST(_vcm->RegisterSendCodec(&codec, 2, 1440) == VCM_OK);// will also set and init the desired codec
            change++;
        }
    }

    double endTime = clock()/(double)CLOCKS_PER_SEC;
    _testTotalTime = endTime - startTime;
    _sumEncBytes = _encodeCompleteCallback.EncodedBytes();

    delete tmpBuffer;
    delete waitEvent;
    _vpm->Reset();
    Teardown();
    Print();
    VideoProcessingModule::Destroy(_vpm);
    return 0;
}


// implementing callback to be called from VCM to update VPM of frame rate and size
QMTestVideoSettingsCallback::QMTestVideoSettingsCallback():
_vpm(NULL),
_vcm(NULL)
{
    //
}

void
QMTestVideoSettingsCallback::RegisterVPM(VideoProcessingModule *vpm)
{
    _vpm = vpm;
}
void
QMTestVideoSettingsCallback::RegisterVCM(VideoCodingModule *vcm)
{
    _vcm = vcm;
}

bool
QMTestVideoSettingsCallback::Updated()
{
    if (_updated)
    {
        _updated = false;
        return true;
    }
    return false;
}

WebRtc_Word32
QMTestVideoSettingsCallback::SetVideoQMSettings(const WebRtc_UWord32 frameRate,
                                                const WebRtc_UWord32 width,
                                                const WebRtc_UWord32 height)
{
    WebRtc_Word32 retVal = 0;
    printf("QM updates: W = %d, H = %d, FR = %d, \n", width, height, frameRate);
    retVal = _vpm->SetTargetResolution(width, height, frameRate);
    //Initialize codec with new values - is this the best place to do it?
    if (!retVal)
    {
        // first get current settings
        VideoCodec currentCodec;
        _vcm->SendCodec(&currentCodec);
        // now set new values:
        currentCodec.height = (WebRtc_UWord16)height;
        currentCodec.width = (WebRtc_UWord16)width;
        currentCodec.maxFramerate = (WebRtc_UWord8)frameRate;

        // re-register encoder
        retVal = _vcm->RegisterSendCodec(&currentCodec, 2, 1440);
        _updated = true;
    }

    return retVal;
}


// Decoded Frame Callback Implmentation
VCMQMDecodeCompleCallback::VCMQMDecodeCompleCallback(FILE* decodedFile):
_decodedFile(decodedFile),
_decodedBytes(0),
//_test(test),
_origWidth(0),
_origHeight(0),
_decWidth(0),
_decHeight(0),
//_interpolator(NULL),
_decBuffer(NULL),
_frameCnt(0)
{
    //
}

VCMQMDecodeCompleCallback::~VCMQMDecodeCompleCallback()
 {
//     if (_interpolator != NULL)
//     {
//         deleteInterpolator(_interpolator);
//         _interpolator = NULL;
//     }
     if (_decBuffer != NULL)
     {
         delete [] _decBuffer;
         _decBuffer = NULL;
     }
 }
WebRtc_Word32
VCMQMDecodeCompleCallback::FrameToRender(VideoFrame& videoFrame)
{
    if ((_origWidth == videoFrame.Width()) && (_origHeight == videoFrame.Height()))
    {
        fwrite(videoFrame.Buffer(), 1, videoFrame.Length(), _decodedFile);
        _frameCnt++;
        //printf("frame dec # %d", _frameCnt);
        // no need for interpolator and decBuffer
        if (_decBuffer != NULL)
        {
            delete [] _decBuffer;
            _decBuffer = NULL;
        }
//        if (_interpolator != NULL)
//        {
//            deleteInterpolator(_interpolator);
//            _interpolator = NULL;
//        }
        _decWidth = 0;
        _decHeight = 0;
    }
    else
    {
        if ((_decWidth != videoFrame.Width()) || (_decHeight != videoFrame.Height()))
        {
            _decWidth = videoFrame.Width();
            _decHeight = videoFrame.Height();
            buildInterpolator();
        }

//        interpolateFrame(_interpolator, videoFrame.Buffer(),_decBuffer);
        fwrite(_decBuffer, 1, _origWidth*_origHeight*3/2, _decodedFile);
        _frameCnt++;
    }

    _decodedBytes += videoFrame.Length();
    return VCM_OK;
}

WebRtc_Word32
VCMQMDecodeCompleCallback::DecodedBytes()
{
    return _decodedBytes;
}

void
VCMQMDecodeCompleCallback::SetOriginalFrameDimensions(WebRtc_Word32 width, WebRtc_Word32 height)
{
    _origWidth = width;
    _origHeight = height;
}

WebRtc_Word32
VCMQMDecodeCompleCallback::buildInterpolator()
{
    WebRtc_UWord32 decFrameLength  = _origWidth*_origHeight*3 >> 1;
    if (_decBuffer != NULL)
    {
        delete [] _decBuffer;
    }
    _decBuffer = new WebRtc_UWord8[decFrameLength];
    if (_decBuffer == NULL)
    {
        return -1;
    }

    return 0;
}
