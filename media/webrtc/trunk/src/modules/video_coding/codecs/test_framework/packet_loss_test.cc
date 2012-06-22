/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "packet_loss_test.h"
#include "video_source.h"
#include <sstream>
#include <cassert>
#include <string.h>

using namespace webrtc;

PacketLossTest::PacketLossTest()
:
NormalAsyncTest("PacketLossTest", "Encode, remove lost packets, decode", 300,
                5),
_lossRate(0.1),
_lossProbability(0.1),
_lastFrame(NULL),
_lastFrameLength(0)
{
}

PacketLossTest::PacketLossTest(std::string name, std::string description)
:
NormalAsyncTest(name, description, 300, 5),
_lossRate(0.1),
_lossProbability(0.1),
_lastFrame(NULL),
_lastFrameLength(0)
{
}

PacketLossTest::PacketLossTest(std::string name, std::string description, double lossRate, bool useNack, unsigned int rttFrames /* = 0*/)
:
NormalAsyncTest(name, description, 300, 5, rttFrames),
_lossRate(lossRate),
_lastFrame(NULL),
_lastFrameLength(0)
{
    assert(lossRate >= 0 && lossRate <= 1);
    if (useNack)
    {
        _lossProbability = 0;
    }
    else
    {
        _lossProbability = lossRate;
    }
}

void
PacketLossTest::Encoded(const EncodedImage& encodedImage)
{
    // push timestamp to queue
    _frameQueue.push_back(encodedImage._timeStamp);
    NormalAsyncTest::Encoded(encodedImage);
}

void
PacketLossTest::Decoded(const RawImage& decodedImage)
{
    // check the frame queue if any frames have gone missing
    assert(!_frameQueue.empty()); // decoded frame is not in the queue
    while(_frameQueue.front() < decodedImage._timeStamp)
    {
        // this frame is missing
        // write previous decoded frame again (frame freeze)
        if (_decodedFile && _lastFrame)
        {
            fwrite(_lastFrame, 1, _lastFrameLength, _decodedFile);
        }

        // remove frame from queue
        _frameQueue.pop_front();
    }
    assert(_frameQueue.front() == decodedImage._timeStamp); // decoded frame is not in the queue

    // pop the current frame
    _frameQueue.pop_front();

    // save image for future freeze-frame
    if (_lastFrameLength < decodedImage._length)
    {
        if (_lastFrame) delete [] _lastFrame;

        _lastFrame = new WebRtc_UWord8[decodedImage._length];
    }
    memcpy(_lastFrame, decodedImage._buffer, decodedImage._length);
    _lastFrameLength = decodedImage._length;

    NormalAsyncTest::Decoded(decodedImage);
}

void
PacketLossTest::Teardown()
{
    if (_totalKept + _totalThrown > 0)
    {
        printf("Target packet loss rate: %.4f\n", _lossProbability);
        printf("Actual packet loss rate: %.4f\n", (_totalThrown * 1.0f) / (_totalKept + _totalThrown));
        printf("Channel rate: %.2f kbps\n",
            0.001 * 8.0 * _sumChannelBytes / ((_framecnt * 1.0f) / _inst.maxFramerate));
    }
    else
    {
        printf("No packet losses inflicted\n");
    }

    NormalAsyncTest::Teardown();
}

void
PacketLossTest::Setup()
{
    const VideoSource source(_inname, _inst.width, _inst.height, _inst.maxFramerate);

    std::stringstream ss;
    std::string lossRateStr;
    ss << _lossRate;
    ss >> lossRateStr;
    _encodedName = source.GetName() + "-" + lossRateStr;
    _outname = "out-" + source.GetName() + "-" + lossRateStr;

    if (_lossProbability != _lossRate)
    {
        _encodedName += "-nack";
        _outname += "-nack";
    }
    _encodedName += ".vp8";
    _outname += ".yuv";

    _totalKept = 0;
    _totalThrown = 0;
    _sumChannelBytes = 0;

    NormalAsyncTest::Setup();
}

void
PacketLossTest::CodecSpecific_InitBitrate()
{
    assert(_bitRate > 0);
    WebRtc_UWord32 simulatedBitRate;
    if (_lossProbability != _lossRate)
    {
        // Simulating NACK
        simulatedBitRate = WebRtc_UWord32(_bitRate / (1 + _lossRate));
    }
    else
    {
        simulatedBitRate = _bitRate;
    }
    int rtt = 0;
    if (_inst.maxFramerate > 0)
      rtt = _rttFrames * (1000 / _inst.maxFramerate);
    _encoder->SetChannelParameters((WebRtc_UWord32)(_lossProbability * 255.0),
                                                    rtt);
    _encoder->SetRates(simulatedBitRate, _inst.maxFramerate);
}

int PacketLossTest::DoPacketLoss()
{
    // Only packet loss for delta frames
    if (_frameToDecode->_frame->GetLength() == 0 || _frameToDecode->_frame->GetFrameType() != kDeltaFrame)
    {
        _sumChannelBytes += _frameToDecode->_frame->GetLength();
        return 0;
    }
    unsigned char *packet = NULL;
    TestVideoEncodedBuffer newEncBuf;
    newEncBuf.VerifyAndAllocate(_lengthSourceFrame);
    _inBufIdx = 0;
    _outBufIdx = 0;
    int size = 1;
    int kept = 0;
    int thrown = 0;
    while ((size = NextPacket(1500, &packet)) > 0)
    {
        if (!PacketLoss(_lossProbability, thrown))
        {
            InsertPacket(&newEncBuf, packet, size);
            kept++;
        }
        else
        {
            // Use the ByteLoss function if you want to lose only
            // parts of a packet, and not the whole packet.

            //int size2 = ByteLoss(size, packet, 15);
            thrown++;
            //if (size2 != size)
            //{
            //    InsertPacket(&newEncBuf, packet, size2);
            //}
        }
    }
    int	lossResult  = (thrown!=0);	// 0 = no loss	1 = loss(es)
    if (lossResult)
    {
        lossResult += (kept==0);	// 2 = all lost = full frame
    }
    _frameToDecode->_frame->CopyBuffer(newEncBuf.GetLength(), newEncBuf.GetBuffer());
    _sumChannelBytes += newEncBuf.GetLength();
    _totalKept += kept;
    _totalThrown += thrown;

    return lossResult;
    //printf("Threw away: %d out of %d packets\n", thrown, thrown + kept);
    //printf("Encoded left: %d bytes\n", _encodedVideoBuffer.GetLength());
}

int PacketLossTest::NextPacket(int mtu, unsigned char **pkg)
{
    unsigned char *buf = _frameToDecode->_frame->GetBuffer();
    *pkg = buf + _inBufIdx;
    if (static_cast<long>(_frameToDecode->_frame->GetLength()) - _inBufIdx <= mtu)
    {
        int size = _frameToDecode->_frame->GetLength() - _inBufIdx;
        _inBufIdx = _frameToDecode->_frame->GetLength();
        return size;
    }
    _inBufIdx += mtu;
    return mtu;
}

int PacketLossTest::ByteLoss(int size, unsigned char *pkg, int bytesToLose)
{
    return size;
}

void PacketLossTest::InsertPacket(TestVideoEncodedBuffer *buf, unsigned char *pkg, int size)
{
    if (static_cast<long>(buf->GetSize()) - _outBufIdx < size)
    {
        printf("InsertPacket error!\n");
        return;
    }
    memcpy(buf->GetBuffer() + _outBufIdx, pkg, size);
    buf->UpdateLength(buf->GetLength() + size);
    _outBufIdx += size;
}
