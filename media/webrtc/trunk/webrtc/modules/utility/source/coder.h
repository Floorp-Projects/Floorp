/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_UTILITY_SOURCE_CODER_H_
#define WEBRTC_MODULES_UTILITY_SOURCE_CODER_H_

#include "audio_coding_module.h"
#include "common_types.h"
#include "typedefs.h"

namespace webrtc {
class AudioFrame;

class AudioCoder : public AudioPacketizationCallback
{
public:
    AudioCoder(uint32_t instanceID);
    ~AudioCoder();

    int32_t SetEncodeCodec(
        const CodecInst& codecInst,
	ACMAMRPackingFormat amrFormat = AMRBandwidthEfficient);

    int32_t SetDecodeCodec(
        const CodecInst& codecInst,
	ACMAMRPackingFormat amrFormat = AMRBandwidthEfficient);

    int32_t Decode(AudioFrame& decodedAudio, uint32_t sampFreqHz,
                   const int8_t* incomingPayload, int32_t payloadLength);

    int32_t PlayoutData(AudioFrame& decodedAudio, uint16_t& sampFreqHz);

    int32_t Encode(const AudioFrame& audio, int8_t* encodedData,
                   uint32_t& encodedLengthInBytes);

protected:
    virtual int32_t SendData(FrameType frameType,
                             uint8_t payloadType,
                             uint32_t timeStamp,
                             const uint8_t* payloadData,
                             uint16_t payloadSize,
                             const RTPFragmentationHeader* fragmentation);

private:
    AudioCodingModule* _acm;

    CodecInst _receiveCodec;

    uint32_t _encodeTimestamp;
    int8_t*  _encodedData;
    uint32_t _encodedLengthInBytes;

    uint32_t _decodeTimestamp;
};
} // namespace webrtc

#endif // WEBRTC_MODULES_UTILITY_SOURCE_CODER_H_
