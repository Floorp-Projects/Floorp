/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_TEST_FRAMEWORK_NORMAL_ASYNC_TEST_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_TEST_FRAMEWORK_NORMAL_ASYNC_TEST_H_

#include "webrtc/common_types.h"

#include <list>
#include <map>
#include <queue>
#include "webrtc/modules/video_coding/codecs/test_framework/normal_test.h"
#include "webrtc/system_wrappers/interface/rw_lock_wrapper.h"

class FrameQueueTuple
{
public:
    FrameQueueTuple(webrtc::VideoFrame *frame,
                    const webrtc::CodecSpecificInfo* codecSpecificInfo = NULL)
    :
        _frame(frame),
        _codecSpecificInfo(codecSpecificInfo)
    {};
    ~FrameQueueTuple();
    webrtc::VideoFrame*          _frame;
    const webrtc::CodecSpecificInfo* _codecSpecificInfo;
};

class FrameQueue
{
public:
    FrameQueue()
    :
        _queueRWLock(*webrtc::RWLockWrapper::CreateRWLock())
    {
    }

    ~FrameQueue()
    {
        delete &_queueRWLock;
    }

    void PushFrame(webrtc::VideoFrame *frame,
                   webrtc::CodecSpecificInfo* codecSpecificInfo = NULL);
    FrameQueueTuple* PopFrame();
    bool Empty();

private:
    webrtc::RWLockWrapper&                       _queueRWLock;
    std::queue<FrameQueueTuple *>     _frameBufferQueue;
};

// feedback signal to encoder
struct fbSignal
{
    fbSignal(int d, uint8_t pid) : delay(d), id(pid) {};
    int         delay;
    uint8_t id;
};

class NormalAsyncTest : public NormalTest
{
public:
    NormalAsyncTest();
    NormalAsyncTest(uint32_t bitRate);
    NormalAsyncTest(std::string name, std::string description,
                    unsigned int testNo);
    NormalAsyncTest(std::string name, std::string description,
                    uint32_t bitRate, unsigned int testNo);
    NormalAsyncTest(std::string name, std::string description,
                    uint32_t bitRate, unsigned int testNo,
                    unsigned int rttFrames);
    virtual ~NormalAsyncTest() {};
    virtual void Perform();
    virtual void Encoded(const webrtc::EncodedImage& encodedImage);
    virtual void Decoded(const webrtc::I420VideoFrame& decodedImage);
    virtual webrtc::CodecSpecificInfo*
    CopyCodecSpecificInfo(
        const webrtc::CodecSpecificInfo* codecSpecificInfo) const;
    virtual void CopyEncodedImage(webrtc::VideoFrame& dest,
                                  webrtc::EncodedImage& src,
                                  void* /*codecSpecificInfo*/) const;
    virtual webrtc::CodecSpecificInfo* CreateEncoderSpecificInfo() const
    {
        return NULL;
    };
    virtual int32_t ReceivedDecodedReferenceFrame(
        const uint64_t pictureId);
    virtual int32_t ReceivedDecodedFrame(const uint64_t pictureId);

protected:
    virtual void Setup();
    virtual void Teardown();
    virtual bool Encode();
    virtual int Decode(int lossValue = 0);
    virtual void CodecSpecific_InitBitrate();
    virtual int SetCodecSpecificParameters() {return 0;};
    double tGetTime();// return time in sec

    FILE*                   _sourceFile;
    FILE*                   _decodedFile;
    uint32_t          _decodedWidth;
    uint32_t          _decodedHeight;
    double                  _totalEncodeTime;
    double                  _totalDecodeTime;
    double                  _decodeCompleteTime;
    double                  _encodeCompleteTime;
    double                  _totalEncodePipeTime;
    double                  _totalDecodePipeTime;
    int                     _framecnt;
    int                     _encFrameCnt;
    int                     _decFrameCnt;
    bool                    _requestKeyFrame;
    unsigned int            _testNo;
    unsigned int            _lengthEncFrame;
    FrameQueueTuple*        _frameToDecode;
    bool                    _appendNext;
    std::map<uint32_t, double> _encodeTimes;
    std::map<uint32_t, double> _decodeTimes;
    bool                    _missingFrames;
    std::list<fbSignal>     _signalSLI;
    int                     _rttFrames;
    mutable bool            _hasReceivedSLI;
    mutable bool            _hasReceivedRPSI;
    uint8_t           _pictureIdSLI;
    uint16_t          _pictureIdRPSI;
    uint64_t          _lastDecRefPictureId;
    uint64_t          _lastDecPictureId;
    std::list<fbSignal>     _signalPLI;
    bool                    _hasReceivedPLI;
    bool                    _waitForKey;
};

class VideoEncodeCompleteCallback : public webrtc::EncodedImageCallback
{
public:
    VideoEncodeCompleteCallback(FILE* encodedFile, FrameQueue *frameQueue,
                                NormalAsyncTest& test)
    :
      _encodedFile(encodedFile),
      _frameQueue(frameQueue),
      _test(test),
      _encodedBytes(0)
    {}

    int32_t
    Encoded(webrtc::EncodedImage& encodedImage,
            const webrtc::CodecSpecificInfo* codecSpecificInfo = NULL,
            const webrtc::RTPFragmentationHeader* fragmentation = NULL);
    uint32_t EncodedBytes();
private:
    FILE*             _encodedFile;
    FrameQueue*       _frameQueue;
    NormalAsyncTest&  _test;
    uint32_t    _encodedBytes;
};

class VideoDecodeCompleteCallback : public webrtc::DecodedImageCallback
{
public:
    VideoDecodeCompleteCallback(FILE* decodedFile, NormalAsyncTest& test)
    :
        _decodedFile(decodedFile),
        _test(test),
        _decodedBytes(0)
    {}

    virtual int32_t Decoded(webrtc::I420VideoFrame& decodedImage);
    virtual int32_t
    ReceivedDecodedReferenceFrame(const uint64_t pictureId);
    virtual int32_t ReceivedDecodedFrame(const uint64_t pictureId);

    uint32_t DecodedBytes();
private:
    FILE* _decodedFile;
    NormalAsyncTest& _test;
    uint32_t    _decodedBytes;
};

#endif // WEBRTC_MODULES_VIDEO_CODING_CODECS_TEST_FRAMEWORK_NORMAL_ASYNC_TEST_H_
