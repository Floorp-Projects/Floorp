/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "performance_test.h"

#include <assert.h>

#include "gtest/gtest.h"
#include "testsupport/fileutils.h"
#include "tick_util.h"

using namespace webrtc;

#define NUM_FRAMES 300

PerformanceTest::PerformanceTest(WebRtc_UWord32 bitRate)
:
NormalAsyncTest(bitRate),
_numCodecs(0),
_tests(NULL),
_encoders(NULL),
_decoders(NULL),
_threads(NULL),
_rawImageLock(NULL),
_encodeEvents(new EventWrapper*[1]),
_stopped(true),
_encodeCompleteCallback(NULL),
_decodeCompleteCallback(NULL)
{
}

PerformanceTest::PerformanceTest(WebRtc_UWord32 bitRate, WebRtc_UWord8 numCodecs)
:
NormalAsyncTest(bitRate),
_numCodecs(numCodecs),
_tests(new PerformanceTest*[_numCodecs]),
_encoders(new VideoEncoder*[_numCodecs]),
_decoders(new VideoDecoder*[_numCodecs]),
_threads(new ThreadWrapper*[_numCodecs]),
_rawImageLock(RWLockWrapper::CreateRWLock()),
_encodeEvents(new EventWrapper*[_numCodecs]),
_stopped(true),
_encodeCompleteCallback(NULL),
_decodeCompleteCallback(NULL)
{
    for (int i=0; i < _numCodecs; i++)
    {
        _tests[i] = new PerformanceTest(bitRate);
        _encodeEvents[i] = EventWrapper::Create();
    }
}

PerformanceTest::~PerformanceTest()
{
    if (_encoders != NULL)
    {
        delete [] _encoders;
    }
    if (_decoders != NULL)
    {
        delete [] _decoders;
    }
    if (_tests != NULL)
    {
        delete [] _tests;
    }
    if (_threads != NULL)
    {
        delete [] _threads;
    }
    if (_rawImageLock != NULL)
    {
        delete _rawImageLock;
    }
    if (_encodeEvents != NULL)
    {
        delete [] _encodeEvents;
    }
}

void
PerformanceTest::Setup()
{
    _inname = webrtc::test::ProjectRootPath() + "resources/foreman_cif.yuv";
    NormalAsyncTest::Setup(); // Setup input and output files
    CodecSettings(352, 288, 30, _bitRate); // common to all codecs
    for (int i=0; i < _numCodecs; i++)
    {
        _encoders[i] = CreateEncoder();
        _decoders[i] = CreateDecoder();
        if (_encoders[i] == NULL)
        {
            printf("Must create a codec specific test!\n");
            exit(EXIT_FAILURE);
        }
        if(_encoders[i]->InitEncode(&_inst, 4, 1440) < 0)
        {
            exit(EXIT_FAILURE);
        }
        if (_decoders[i]->InitDecode(&_inst, 1))
        {
            exit(EXIT_FAILURE);
        }
        _tests[i]->SetEncoder(_encoders[i]);
        _tests[i]->SetDecoder(_decoders[i]);
        _tests[i]->_rawImageLock = _rawImageLock;
        _encodeEvents[i]->Reset();
        _tests[i]->_encodeEvents[0] = _encodeEvents[i];
        _tests[i]->_inst = _inst;
        _threads[i] = ThreadWrapper::CreateThread(PerformanceTest::RunThread, _tests[i]);
        unsigned int id = 0;
        _tests[i]->_stopped = false;
        _threads[i]->Start(id);
    }
}

void
PerformanceTest::Perform()
{
    Setup();
    EventWrapper& sleepEvent = *EventWrapper::Create();
    const WebRtc_Word64 startTime = TickTime::MillisecondTimestamp();
    for (int i=0; i < NUM_FRAMES; i++)
    {
        {
            // Read a new frame from file
            WriteLockScoped imageLock(*_rawImageLock);
            _lengthEncFrame = 0;
            EXPECT_GT(fread(_sourceBuffer, 1, _lengthSourceFrame, _sourceFile),
                      0u);
            if (feof(_sourceFile) != 0)
            {
                rewind(_sourceFile);
            }
            _inputVideoBuffer.VerifyAndAllocate(_inst.width*_inst.height*3/2);
            _inputVideoBuffer.CopyBuffer(_lengthSourceFrame, _sourceBuffer);
            _inputVideoBuffer.SetTimeStamp((unsigned int) (_encFrameCnt * 9e4 / static_cast<float>(_inst.maxFramerate)));
            _inputVideoBuffer.SetWidth(_inst.width);
            _inputVideoBuffer.SetHeight(_inst.height);
            for (int i=0; i < _numCodecs; i++)
            {
                _tests[i]->_inputVideoBuffer.CopyPointer(_inputVideoBuffer);
                _encodeEvents[i]->Set();
            }
        }
        if (i < NUM_FRAMES - 1)
        {
            sleepEvent.Wait(33);
        }
    }
    for (int i=0; i < _numCodecs; i++)
    {
        _tests[i]->_stopped = true;
        _encodeEvents[i]->Set();
        _threads[i]->Stop();
    }
    const WebRtc_UWord32 totalTime =
            static_cast<WebRtc_UWord32>(TickTime::MillisecondTimestamp() - startTime);
    printf("Total time: %u\n", totalTime);
    delete &sleepEvent;
    Teardown();
}

void PerformanceTest::Teardown()
{
    if (_encodeCompleteCallback != NULL)
    {
        delete _encodeCompleteCallback;
    }
    if (_decodeCompleteCallback != NULL)
    {
        delete _decodeCompleteCallback;
    }
    // main test only, all others have numCodecs = 0:
    if (_numCodecs > 0)
    {
        WriteLockScoped imageLock(*_rawImageLock);
        _inputVideoBuffer.Free();
        NormalAsyncTest::Teardown();
    }
    for (int i=0; i < _numCodecs; i++)
    {
        _encoders[i]->Release();
        delete _encoders[i];
        _decoders[i]->Release();
        delete _decoders[i];
        _tests[i]->_inputVideoBuffer.ClearPointer();
        _tests[i]->_rawImageLock = NULL;
        _tests[i]->Teardown();
        delete _tests[i];
        delete _encodeEvents[i];
        delete _threads[i];
    }
}

bool
PerformanceTest::RunThread(void* obj)
{
    PerformanceTest& test = *static_cast<PerformanceTest*>(obj);
    return test.PerformSingleTest();
}

bool
PerformanceTest::PerformSingleTest()
{
    if (_encodeCompleteCallback == NULL)
    {
        _encodeCompleteCallback = new VideoEncodeCompleteCallback(NULL, &_frameQueue, *this);
        _encoder->RegisterEncodeCompleteCallback(_encodeCompleteCallback);
    }
    if (_decodeCompleteCallback == NULL)
    {
        _decodeCompleteCallback = new VideoDecodeCompleteCallback(NULL, *this);
        _decoder->RegisterDecodeCompleteCallback(_decodeCompleteCallback);
    }
    (*_encodeEvents)->Wait(WEBRTC_EVENT_INFINITE); // The first event is used for every single test
    CodecSpecific_InitBitrate();
    bool complete = false;
    {
        ReadLockScoped imageLock(*_rawImageLock);
        complete = Encode();
    }
    if (!_frameQueue.Empty() || complete)
    {
        while (!_frameQueue.Empty())
        {
            _frameToDecode = static_cast<FrameQueueTuple *>(_frameQueue.PopFrame());
            int lost = DoPacketLoss();
            if (lost == 2)
            {
                // Lost the whole frame, continue
                _missingFrames = true;
                delete _frameToDecode;
                _frameToDecode = NULL;
                continue;
            }
            int ret = Decode(lost);
            delete _frameToDecode;
            _frameToDecode = NULL;
            if (ret < 0)
            {
                fprintf(stderr,"\n\nError in decoder: %d\n\n", ret);
                return false;
            }
            else if (ret < 0)
            {
                fprintf(stderr, "\n\nPositive return value from decode!\n\n");
                return false;
            }
        }
    }
    if (_stopped)
    {
        return false;
    }
    return true;
}

bool PerformanceTest::Encode()
{
    RawImage rawImage;
    VideoBufferToRawImage(_inputVideoBuffer, rawImage);
    VideoFrameType frameType = kDeltaFrame;
    if (_requestKeyFrame && !(_encFrameCnt%50))
    {
        frameType = kKeyFrame;
    }
    webrtc::CodecSpecificInfo* codecSpecificInfo = CreateEncoderSpecificInfo();
    int ret = _encoder->Encode(rawImage, codecSpecificInfo, frameType);
    EXPECT_EQ(ret, WEBRTC_VIDEO_CODEC_OK);
    if (codecSpecificInfo != NULL)
    {
        delete codecSpecificInfo;
        codecSpecificInfo = NULL;
    }
    assert(ret >= 0);
    return false;
}

int PerformanceTest::Decode(int lossValue)
{
    EncodedImage encodedImage;
    VideoEncodedBufferToEncodedImage(*(_frameToDecode->_frame), encodedImage);
    encodedImage._completeFrame = !lossValue;
    int ret = _decoder->Decode(encodedImage, _missingFrames, NULL,
                               _frameToDecode->_codecSpecificInfo);
    _missingFrames = false;
    return ret;
}
