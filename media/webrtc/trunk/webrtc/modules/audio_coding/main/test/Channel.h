/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CHANNEL_H
#define CHANNEL_H

#include <stdio.h>

#include "audio_coding_module.h"
#include "critical_section_wrapper.h"
#include "rw_lock_wrapper.h"
#include "webrtc/modules/interface/module_common_types.h"

namespace webrtc {

#define MAX_NUM_PAYLOADS   50
#define MAX_NUM_FRAMESIZES  6


struct ACMTestFrameSizeStats
{
    uint16_t frameSizeSample;
    int16_t  maxPayloadLen;
    uint32_t numPackets;
    uint64_t totalPayloadLenByte;
    uint64_t totalEncodedSamples;
    double         rateBitPerSec;
    double         usageLenSec;

};

struct ACMTestPayloadStats
{
    bool                  newPacket;
    int16_t         payloadType;
    int16_t         lastPayloadLenByte;
    uint32_t        lastTimestamp;
    ACMTestFrameSizeStats frameSizeStats[MAX_NUM_FRAMESIZES];
};

class Channel: public AudioPacketizationCallback
{
public:

    Channel(
        int16_t chID = -1);
    ~Channel();

    int32_t SendData(
        const FrameType       frameType,
        const uint8_t   payloadType,
        const uint32_t  timeStamp,
        const uint8_t*  payloadData,
        const uint16_t  payloadSize,
        const RTPFragmentationHeader* fragmentation);

    void RegisterReceiverACM(
        AudioCodingModule *acm);

    void ResetStats();

    int16_t Stats(
        CodecInst&           codecInst,
        ACMTestPayloadStats& payloadStats);

    void Stats(
        uint32_t* numPackets);

    void Stats(
        uint8_t*  payloadLenByte,
        uint32_t* payloadType);

    void PrintStats(
        CodecInst& codecInst);

    void SetIsStereo(bool isStereo)
    {
        _isStereo = isStereo;
    }

    uint32_t LastInTimestamp();

    void SetFECTestWithPacketLoss(bool usePacketLoss)
    {
        _useFECTestWithPacketLoss = usePacketLoss;
    }

    double BitRate();

private:
    void CalcStatistics(
        WebRtcRTPHeader& rtpInfo,
        uint16_t   payloadSize);

    AudioCodingModule*      _receiverACM;
    uint16_t          _seqNo;
    // 60msec * 32 sample(max)/msec * 2 description (maybe) * 2 bytes/sample
    uint8_t           _payloadData[60 * 32 * 2 * 2];

    CriticalSectionWrapper* _channelCritSect;
    FILE*                   _bitStreamFile;
    bool                    _saveBitStream;
    int16_t           _lastPayloadType;
    ACMTestPayloadStats     _payloadStats[MAX_NUM_PAYLOADS];
    bool                    _isStereo;
    WebRtcRTPHeader         _rtpInfo;
    bool                    _leftChannel;
    uint32_t          _lastInTimestamp;
    // FEC Test variables
    int16_t           _packetLoss;
    bool                    _useFECTestWithPacketLoss;
    uint64_t          _beginTime;
    uint64_t          _totalBytes;
};

} // namespace webrtc

#endif
