/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "dual_decoder_test.h"

#include <assert.h>
#include <string.h> // memcmp
#include <time.h>

#include "testsupport/fileutils.h"

VP8DualDecoderTest::VP8DualDecoderTest(float bitRate)
:
VP8NormalAsyncTest(bitRate)
{
    _decoder2 = NULL;
}

VP8DualDecoderTest::VP8DualDecoderTest()
:
VP8NormalAsyncTest("VP8 Dual Decoder Test", "Tests VP8 dual decoder", 1),
_decoder2(NULL)
{}

VP8DualDecoderTest::~VP8DualDecoderTest()
{
    if(_decoder2)
    {
        _decoder2->Release();
        delete _decoder2;
    }

    _decodedVideoBuffer2.Free();
}

void
VP8DualDecoderTest::Perform()
{
    _inname = webrtc::test::ProjectRootPath() + "resources/foreman_cif.yuv";
    CodecSettings(352, 288, 30, _bitRate);
    Setup();
    _inputVideoBuffer.VerifyAndAllocate(_lengthSourceFrame);
    _decodedVideoBuffer.VerifyAndAllocate(_lengthSourceFrame);
    _decodedVideoBuffer2.VerifyAndAllocate(_lengthSourceFrame);
    if(_encoder->InitEncode(&_inst, 4, 1460) < 0)
    {
        exit(EXIT_FAILURE);
    }
    _decoder->InitDecode(&_inst,1);

    FrameQueue frameQueue;
    VideoEncodeCompleteCallback encCallback(_encodedFile, &frameQueue, *this);
    DualDecoderCompleteCallback decCallback(&_decodedVideoBuffer);
    DualDecoderCompleteCallback decCallback2(&_decodedVideoBuffer2);
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
    double starttime = clock()/(double)CLOCKS_PER_SEC;
    while (!complete)
    {
        if (_encFrameCnt == 10)
        {
            // initialize second decoder and copy state
            _decoder2 = static_cast<webrtc::VP8Decoder *>(_decoder->Copy());
            assert(_decoder2 != NULL);
            _decoder2->RegisterDecodeCompleteCallback(&decCallback2);
        }
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
    double endtime = clock()/(double)CLOCKS_PER_SEC;
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


int
VP8DualDecoderTest::Decode(int lossValue)
{
    _sumEncBytes += _frameToDecode->_frame->GetLength();
    webrtc::EncodedImage encodedImage;
    VideoEncodedBufferToEncodedImage(*(_frameToDecode->_frame), encodedImage);
    encodedImage._completeFrame = !lossValue;
    _decodeCompleteTime = 0;
    _decodeTimes[encodedImage._timeStamp] = clock()/(double)CLOCKS_PER_SEC;
    int ret = _decoder->Decode(encodedImage, _missingFrames, NULL,
                               _frameToDecode->_codecSpecificInfo);
    // second decoder
    if (_decoder2)
    {
        int ret2 = _decoder2->Decode(encodedImage, _missingFrames, NULL,
                                     _frameToDecode->_codecSpecificInfo,
                                     0 /* dummy */);

        // check return values
        if (ret < 0 || ret2 < 0 || ret2 != ret)
        {
            exit(EXIT_FAILURE);
        }

        // compare decoded images
        if (!CheckIfBitExact(_decodedVideoBuffer.GetBuffer(),
            _decodedVideoBuffer.GetLength(),
            _decodedVideoBuffer2.GetBuffer(), _decodedVideoBuffer.GetLength()))
        {
            fprintf(stderr,"\n\nClone output different from master.\n\n");
            exit(EXIT_FAILURE);
        }

    }

    _missingFrames = false;
    return ret;
}


bool
VP8DualDecoderTest::CheckIfBitExact(const void* ptrA, unsigned int aLengthBytes,
                                    const void* ptrB, unsigned int bLengthBytes)
{
    if (aLengthBytes != bLengthBytes)
    {
        return false;
    }

    return memcmp(ptrA, ptrB, aLengthBytes) == 0;
}

WebRtc_Word32 DualDecoderCompleteCallback::Decoded(webrtc::VideoFrame& image)
{
    _decodedVideoBuffer->VerifyAndAllocate(image.Length());
    _decodedVideoBuffer->CopyBuffer(image.Length(), image.Buffer());
    _decodedVideoBuffer->SetWidth(image.Width());
    _decodedVideoBuffer->SetHeight(image.Height());
    _decodedVideoBuffer->SetTimeStamp(image.TimeStamp());
    _decodeComplete = true;
    return 0;
}

bool DualDecoderCompleteCallback::DecodeComplete()
{
    if (_decodeComplete)
    {
        _decodeComplete = false;
        return true;
    }
    return false;
}

