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

#include "common_types.h"

#include "normal_test.h"
#include "rw_lock_wrapper.h"
#include <list>
#include <map>
#include <queue>

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
    fbSignal(int d, WebRtc_UWord8 pid) : delay(d), id(pid) {};
    int         delay;
    WebRtc_UWord8 id;
};

class NormalAsyncTest : public NormalTest
{
public:
    NormalAsyncTest();
    NormalAsyncTest(WebRtc_UWord32 bitRate);
    NormalAsyncTest(std::string name, std::string description,
                    unsigned int testNo);
    NormalAsyncTest(std::string name, std::string description,
                    WebRtc_UWord32 bitRate, unsigned int testNo);
    NormalAsyncTest(std::string name, std::string description,
                    WebRtc_UWord32 bitRate, unsigned int testNo,
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
    virtual WebRtc_Word32 ReceivedDecodedReferenceFrame(
        const WebRtc_UWord64 pictureId);
    virtual WebRtc_Word32 ReceivedDecodedFrame(const WebRtc_UWord64 pictureId);

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
    WebRtc_UWord32          _decodedWidth;
    WebRtc_UWord32          _decodedHeight;
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
    std::map<WebRtc_UWord32, double> _encodeTimes;
    std::map<WebRtc_UWord32, double> _decodeTimes;
    bool                    _missingFrames;
    std::list<fbSignal>     _signalSLI;
    int                     _rttFrames;
    mutable bool            _hasReceivedSLI;
    mutable bool            _hasReceivedRPSI;
    WebRtc_UWord8           _pictureIdSLI;
    WebRtc_UWord16          _pictureIdRPSI;
    WebRtc_UWord64          _lastDecRefPictureId;
    WebRtc_UWord64          _lastDecPictureId;
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

    WebRtc_Word32
    Encoded(webrtc::EncodedImage& encodedImage,
            const webrtc::CodecSpecificInfo* codecSpecificInfo = NULL,
            const webrtc::RTPFragmentationHeader* fragmentation = NULL);
    WebRtc_UWord32 EncodedBytes();
private:
    FILE*             _encodedFile;
    FrameQueue*       _frameQueue;
    NormalAsyncTest&  _test;
    WebRtc_UWord32    _encodedBytes;
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

    virtual WebRtc_Word32 Decoded(webrtc::I420VideoFrame& decodedImage);
    virtual WebRtc_Word32
    ReceivedDecodedReferenceFrame(const WebRtc_UWord64 pictureId);
    virtual WebRtc_Word32 ReceivedDecodedFrame(const WebRtc_UWord64 pictureId);

    WebRtc_UWord32 DecodedBytes();
private:
    FILE* _decodedFile;
    NormalAsyncTest& _test;
    WebRtc_UWord32    _decodedBytes;
};

#endif // WEBRTC_MODULES_VIDEO_CODING_CODECS_TEST_FRAMEWORK_NORMAL_ASYNC_TEST_H_
