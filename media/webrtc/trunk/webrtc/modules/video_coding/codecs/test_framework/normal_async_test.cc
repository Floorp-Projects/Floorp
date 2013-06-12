/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "normal_async_test.h"

#include <assert.h>
#include <string.h>
#include <queue>
#include <sstream>
#include <vector>

#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "gtest/gtest.h"
#include "tick_util.h"
#include "testsupport/fileutils.h"
#include "typedefs.h"

using namespace webrtc;

NormalAsyncTest::NormalAsyncTest()
:
NormalTest("Async Normal Test 1", "A test of normal execution of the codec",
           _testNo),
_decodeCompleteTime(0),
_encodeCompleteTime(0),
_encFrameCnt(0),
_decFrameCnt(0),
_requestKeyFrame(false),
_testNo(1),
_appendNext(false),
_missingFrames(false),
_rttFrames(0),
_hasReceivedSLI(false),
_hasReceivedRPSI(false),
_hasReceivedPLI(false),
_waitForKey(false)
{
}

NormalAsyncTest::NormalAsyncTest(WebRtc_UWord32 bitRate)
:
NormalTest("Async Normal Test 1", "A test of normal execution of the codec",
           bitRate, _testNo),
_decodeCompleteTime(0),
_encodeCompleteTime(0),
_encFrameCnt(0),
_decFrameCnt(0),
_requestKeyFrame(false),
_testNo(1),
_appendNext(false),
_missingFrames(false),
_rttFrames(0),
_hasReceivedSLI(false),
_hasReceivedRPSI(false),
_hasReceivedPLI(false),
_waitForKey(false)
{
}

NormalAsyncTest::NormalAsyncTest(std::string name, std::string description,
                                 unsigned int testNo)
:
NormalTest(name, description, _testNo),
_decodeCompleteTime(0),
_encodeCompleteTime(0),
_encFrameCnt(0),
_decFrameCnt(0),
_requestKeyFrame(false),
_testNo(testNo),
_lengthEncFrame(0),
_appendNext(false),
_missingFrames(false),
_rttFrames(0),
_hasReceivedSLI(false),
_hasReceivedRPSI(false),
_hasReceivedPLI(false),
_waitForKey(false)
{
}

NormalAsyncTest::NormalAsyncTest(std::string name, std::string description,
                                 WebRtc_UWord32 bitRate, unsigned int testNo)
:
NormalTest(name, description, bitRate, _testNo),
_decodeCompleteTime(0),
_encodeCompleteTime(0),
_encFrameCnt(0),
_decFrameCnt(0),
_requestKeyFrame(false),
_testNo(testNo),
_lengthEncFrame(0),
_appendNext(false),
_missingFrames(false),
_rttFrames(0),
_hasReceivedSLI(false),
_hasReceivedRPSI(false),
_hasReceivedPLI(false),
_waitForKey(false)
{
}

NormalAsyncTest::NormalAsyncTest(std::string name, std::string description,
                                 WebRtc_UWord32 bitRate, unsigned int testNo,
                                 unsigned int rttFrames)
:
NormalTest(name, description, bitRate, _testNo),
_decodeCompleteTime(0),
_encodeCompleteTime(0),
_encFrameCnt(0),
_decFrameCnt(0),
_requestKeyFrame(false),
_testNo(testNo),
_lengthEncFrame(0),
_appendNext(false),
_missingFrames(false),
_rttFrames(rttFrames),
_hasReceivedSLI(false),
_hasReceivedRPSI(false),
_hasReceivedPLI(false),
_waitForKey(false)
{
}

void
NormalAsyncTest::Setup()
{
    CodecTest::Setup();
    std::stringstream ss;
    std::string strTestNo;
    ss << _testNo;
    ss >> strTestNo;

    // Check if settings exist. Otherwise use defaults.
    if (_outname == "")
    {
        _outname = webrtc::test::OutputPath() + "out_normaltest" + strTestNo +
            ".yuv";
    }

    if (_encodedName == "")
    {
        _encodedName = webrtc::test::OutputPath() + "encoded_normaltest" +
            strTestNo + ".yuv";
    }

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

    char mode[3] = "wb";
    if (_appendNext)
    {
        strncpy(mode, "ab", 3);
    }

    if ((_decodedFile = fopen(_outname.c_str(), mode)) == NULL)
    {
        printf("Cannot write file %s.\n", _outname.c_str());
        exit(1);
    }

    _appendNext = true;
}

void
NormalAsyncTest::Teardown()
{
    CodecTest::Teardown();
    fclose(_sourceFile);
    fclose(_encodedFile);
    fclose(_decodedFile);
}

FrameQueueTuple::~FrameQueueTuple()
{
    if (_codecSpecificInfo != NULL)
    {
        delete _codecSpecificInfo;
    }
    if (_frame != NULL)
    {
        delete _frame;
    }
}

void FrameQueue::PushFrame(VideoFrame *frame,
                           webrtc::CodecSpecificInfo* codecSpecificInfo)
{
    WriteLockScoped cs(_queueRWLock);
    _frameBufferQueue.push(new FrameQueueTuple(frame, codecSpecificInfo));
}

FrameQueueTuple* FrameQueue::PopFrame()
{
    WriteLockScoped cs(_queueRWLock);
    if (_frameBufferQueue.empty())
    {
        return NULL;
    }
    FrameQueueTuple* tuple = _frameBufferQueue.front();
    _frameBufferQueue.pop();
    return tuple;
}

bool FrameQueue::Empty()
{
    ReadLockScoped cs(_queueRWLock);
    return _frameBufferQueue.empty();
}

WebRtc_UWord32 VideoEncodeCompleteCallback::EncodedBytes()
{
    return _encodedBytes;
}

WebRtc_Word32
VideoEncodeCompleteCallback::Encoded(EncodedImage& encodedImage,
                                     const webrtc::CodecSpecificInfo* codecSpecificInfo,
                                     const webrtc::RTPFragmentationHeader*
                                     fragmentation)
{
    _test.Encoded(encodedImage);
    VideoFrame *newBuffer = new VideoFrame();
    newBuffer->VerifyAndAllocate(encodedImage._size);
    _encodedBytes += encodedImage._length;
    // If _frameQueue would have been a fixed sized buffer we could have asked
    // it for an empty frame and then just do:
    // emptyFrame->SwapBuffers(encodedBuffer);
    // This is how it should be done in Video Engine to save in on memcpys
    webrtc::CodecSpecificInfo* codecSpecificInfoCopy =
        _test.CopyCodecSpecificInfo(codecSpecificInfo);
    _test.CopyEncodedImage(*newBuffer, encodedImage, codecSpecificInfoCopy);
    if (_encodedFile != NULL)
    {
      if (fwrite(newBuffer->Buffer(), 1, newBuffer->Length(),
                 _encodedFile) !=  newBuffer->Length()) {
        return -1;
      }
    }
    _frameQueue->PushFrame(newBuffer, codecSpecificInfoCopy);
    return 0;
}

WebRtc_UWord32 VideoDecodeCompleteCallback::DecodedBytes()
{
    return _decodedBytes;
}

WebRtc_Word32
VideoDecodeCompleteCallback::Decoded(I420VideoFrame& image)
{
    _test.Decoded(image);
    _decodedBytes += CalcBufferSize(kI420, image.width(), image.height());
    if (_decodedFile != NULL)
    {
       return PrintI420VideoFrame(image, _decodedFile);
    }
    return 0;
}

WebRtc_Word32
VideoDecodeCompleteCallback::ReceivedDecodedReferenceFrame(
    const WebRtc_UWord64 pictureId)
{
    return _test.ReceivedDecodedReferenceFrame(pictureId);
}

WebRtc_Word32
VideoDecodeCompleteCallback::ReceivedDecodedFrame(
    const WebRtc_UWord64 pictureId)
{
    return _test.ReceivedDecodedFrame(pictureId);
}

void
NormalAsyncTest::Encoded(const EncodedImage& encodedImage)
{
    _encodeCompleteTime = tGetTime();
    _encFrameCnt++;
    _totalEncodePipeTime += _encodeCompleteTime -
        _encodeTimes[encodedImage._timeStamp];
}

void
NormalAsyncTest::Decoded(const I420VideoFrame& decodedImage)
{
    _decodeCompleteTime = tGetTime();
    _decFrameCnt++;
    _totalDecodePipeTime += _decodeCompleteTime -
        _decodeTimes[decodedImage.timestamp()];
    _decodedWidth = decodedImage.width();
    _decodedHeight = decodedImage.height();
}

void
NormalAsyncTest::Perform()
{
    _inname = webrtc::test::ProjectRootPath() + "resources/foreman_cif.yuv";
    CodecSettings(352, 288, 30, _bitRate);
    Setup();
    if(_encoder->InitEncode(&_inst, 1, 1440) < 0)
    {
        exit(EXIT_FAILURE);
    }
    _decoder->InitDecode(&_inst, 1);
    FrameQueue frameQueue;
    VideoEncodeCompleteCallback encCallback(_encodedFile, &frameQueue, *this);
    VideoDecodeCompleteCallback decCallback(_decodedFile, *this);
    _encoder->RegisterEncodeCompleteCallback(&encCallback);
    _decoder->RegisterDecodeCompleteCallback(&decCallback);
    if (SetCodecSpecificParameters() != WEBRTC_VIDEO_CODEC_OK)
    {
        exit(EXIT_FAILURE);
    }
    _totalEncodeTime = _totalDecodeTime = 0;
    _totalEncodePipeTime = _totalDecodePipeTime = 0;
    bool complete = false;
    _framecnt = 0;
    _encFrameCnt = 0;
    _decFrameCnt = 0;
    _sumEncBytes = 0;
    _lengthEncFrame = 0;
    double starttime = tGetTime();
    while (!complete)
    {
        CodecSpecific_InitBitrate();
        complete = Encode();
        if (!frameQueue.Empty() || complete)
        {
            while (!frameQueue.Empty())
            {
                _frameToDecode =
                    static_cast<FrameQueueTuple *>(frameQueue.PopFrame());
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
                    exit(EXIT_FAILURE);
                }
                else if (ret == 0)
                {
                    _framecnt++;
                }
                else
                {
                    fprintf(stderr,
                        "\n\nPositive return value from decode!\n\n");
                }
            }
        }
    }
    double endtime = tGetTime();
    double totalExecutionTime = endtime - starttime;
    printf("Total execution time: %.1f s\n", totalExecutionTime);
    _sumEncBytes = encCallback.EncodedBytes();
    double actualBitRate = ActualBitRate(_encFrameCnt) / 1000.0;
    double avgEncTime = _totalEncodeTime / _encFrameCnt;
    double avgDecTime = _totalDecodeTime / _decFrameCnt;
    printf("Actual bitrate: %f kbps\n", actualBitRate);
    printf("Average encode time: %.1f ms\n", 1000 * avgEncTime);
    printf("Average decode time: %.1f ms\n", 1000 * avgDecTime);
    printf("Average encode pipeline time: %.1f ms\n",
           1000 * _totalEncodePipeTime / _encFrameCnt);
    printf("Average decode pipeline  time: %.1f ms\n",
           1000 * _totalDecodePipeTime / _decFrameCnt);
    printf("Number of encoded frames: %u\n", _encFrameCnt);
    printf("Number of decoded frames: %u\n", _decFrameCnt);
    (*_log) << "Actual bitrate: " << actualBitRate << " kbps\tTarget: " <<
        _bitRate << " kbps" << std::endl;
    (*_log) << "Average encode time: " << avgEncTime << " s" << std::endl;
    (*_log) << "Average decode time: " << avgDecTime << " s" << std::endl;
    _encoder->Release();
    _decoder->Release();
    Teardown();
}

bool
NormalAsyncTest::Encode()
{
    _lengthEncFrame = 0;
    if (feof(_sourceFile) != 0)
    {
        return true;
    }
    EXPECT_GT(fread(_sourceBuffer, 1, _lengthSourceFrame, _sourceFile), 0u);
    EXPECT_EQ(0, _inputVideoBuffer.CreateFrame(_sizeY, _sourceBuffer,
                                  _sizeUv, _sourceBuffer + _sizeY,
                                  _sizeUv, _sourceBuffer + _sizeY + _sizeUv,
                                  _width, _height,
                                  _width, _halfWidth, _halfWidth));
    _inputVideoBuffer.set_timestamp((unsigned int)
        (_encFrameCnt * 9e4 / _inst.maxFramerate));
    _encodeCompleteTime = 0;
    _encodeTimes[_inputVideoBuffer.timestamp()] = tGetTime();
    std::vector<VideoFrameType> frame_types(1, kDeltaFrame);

    // check SLI queue
    _hasReceivedSLI = false;
    while (!_signalSLI.empty() && _signalSLI.front().delay == 0)
    {
        // SLI message has arrived at sender side
        _hasReceivedSLI = true;
        _pictureIdSLI = _signalSLI.front().id;
        _signalSLI.pop_front();
    }
    // decrement SLI queue times
    for (std::list<fbSignal>::iterator it = _signalSLI.begin();
        it !=_signalSLI.end(); it++)
    {
        (*it).delay--;
    }

    // check PLI queue
    _hasReceivedPLI = false;
    while (!_signalPLI.empty() && _signalPLI.front().delay == 0)
    {
        // PLI message has arrived at sender side
        _hasReceivedPLI = true;
        _signalPLI.pop_front();
    }
    // decrement PLI queue times
    for (std::list<fbSignal>::iterator it = _signalPLI.begin();
        it != _signalPLI.end(); it++)
    {
        (*it).delay--;
    }

    if (_hasReceivedPLI)
    {
        // respond to PLI by encoding a key frame
        frame_types[0] = kKeyFrame;
        _hasReceivedPLI = false;
        _hasReceivedSLI = false; // don't trigger both at once
    }

    webrtc::CodecSpecificInfo* codecSpecificInfo = CreateEncoderSpecificInfo();
    int ret = _encoder->Encode(_inputVideoBuffer,
                               codecSpecificInfo, &frame_types);
    EXPECT_EQ(ret, WEBRTC_VIDEO_CODEC_OK);
    if (codecSpecificInfo != NULL)
    {
        delete codecSpecificInfo;
        codecSpecificInfo = NULL;
    }
    if (_encodeCompleteTime > 0)
    {
        _totalEncodeTime += _encodeCompleteTime -
            _encodeTimes[_inputVideoBuffer.timestamp()];
    }
    else
    {
        _totalEncodeTime += tGetTime() -
            _encodeTimes[_inputVideoBuffer.timestamp()];
    }
    assert(ret >= 0);
    return false;
}

int
NormalAsyncTest::Decode(int lossValue)
{
    _sumEncBytes += _frameToDecode->_frame->Length();
    EncodedImage encodedImage;
    VideoEncodedBufferToEncodedImage(*(_frameToDecode->_frame), encodedImage);
    encodedImage._completeFrame = !lossValue;
    _decodeCompleteTime = 0;
    _decodeTimes[encodedImage._timeStamp] = tGetTime();
    int ret = WEBRTC_VIDEO_CODEC_OK;
    // TODO(mikhal):  Update frame type.
    //if (!_waitForKey || encodedImage._frameType == kKeyFrame)
    {
        _waitForKey = false;
        ret = _decoder->Decode(encodedImage, _missingFrames, NULL,
                               _frameToDecode->_codecSpecificInfo);

        if (ret >= 0)
        {
            _missingFrames = false;
        }
    }

    // check for SLI
    if (ret == WEBRTC_VIDEO_CODEC_REQUEST_SLI)
    {
        // add an SLI feedback to the feedback "queue"
        // to be delivered to encoder with _rttFrames delay
        _signalSLI.push_back(fbSignal(_rttFrames,
            static_cast<WebRtc_UWord8>((_lastDecPictureId) & 0x3f))); // 6 lsb

        ret = WEBRTC_VIDEO_CODEC_OK;
    }
    else if (ret == WEBRTC_VIDEO_CODEC_ERR_REQUEST_SLI)
    {
        // add an SLI feedback to the feedback "queue"
        // to be delivered to encoder with _rttFrames delay
        _signalSLI.push_back(fbSignal(_rttFrames,
            static_cast<WebRtc_UWord8>((_lastDecPictureId + 1) & 0x3f)));//6 lsb

        ret = WEBRTC_VIDEO_CODEC_OK;
    }
    else if (ret == WEBRTC_VIDEO_CODEC_ERROR)
    {
        // wait for new key frame
        // add an PLI feedback to the feedback "queue"
        // to be delivered to encoder with _rttFrames delay
        _signalPLI.push_back(fbSignal(_rttFrames, 0 /* picId not used*/));
        _waitForKey = true;

        ret = WEBRTC_VIDEO_CODEC_OK;
    }

    if (_decodeCompleteTime > 0)
    {
        _totalDecodeTime += _decodeCompleteTime -
            _decodeTimes[encodedImage._timeStamp];
    }
    else
    {
        _totalDecodeTime += tGetTime() - _decodeTimes[encodedImage._timeStamp];
    }
    return ret;
}

webrtc::CodecSpecificInfo*
NormalAsyncTest::CopyCodecSpecificInfo(
        const webrtc::CodecSpecificInfo* codecSpecificInfo) const
{
    webrtc::CodecSpecificInfo* info = new webrtc::CodecSpecificInfo;
    *info = *codecSpecificInfo;
    return info;
}

void NormalAsyncTest::CodecSpecific_InitBitrate()
{
    if (_bitRate == 0)
    {
        _encoder->SetRates(600, _inst.maxFramerate);
    }
    else
    {
        _encoder->SetRates(_bitRate, _inst.maxFramerate);
    }
}

void NormalAsyncTest::CopyEncodedImage(VideoFrame& dest,
                                       EncodedImage& src,
                                       void* /*codecSpecificInfo*/) const
{
    dest.CopyFrame(src._length, src._buffer);
    //dest.SetFrameType(src._frameType);
    dest.SetWidth((WebRtc_UWord16)src._encodedWidth);
    dest.SetHeight((WebRtc_UWord16)src._encodedHeight);
    dest.SetTimeStamp(src._timeStamp);
}

WebRtc_Word32 NormalAsyncTest::ReceivedDecodedReferenceFrame(
    const WebRtc_UWord64 pictureId) {
  _lastDecRefPictureId = pictureId;
  return 0;
}

WebRtc_Word32 NormalAsyncTest::ReceivedDecodedFrame(
    const WebRtc_UWord64 pictureId) {
  _lastDecPictureId = pictureId;
  return 0;
}

double
NormalAsyncTest::tGetTime()
{// return time in sec
    return ((double) (TickTime::MillisecondTimestamp())/1000);
 }
