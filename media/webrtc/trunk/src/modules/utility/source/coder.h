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
    AudioCoder(WebRtc_UWord32 instanceID);
    ~AudioCoder();

    WebRtc_Word32 SetEncodeCodec(
        const CodecInst& codecInst,
	ACMAMRPackingFormat amrFormat = AMRBandwidthEfficient);

    WebRtc_Word32 SetDecodeCodec(
        const CodecInst& codecInst,
	ACMAMRPackingFormat amrFormat = AMRBandwidthEfficient);

    WebRtc_Word32 Decode(AudioFrame& decodedAudio, WebRtc_UWord32 sampFreqHz,
			 const WebRtc_Word8* incomingPayload,
			 WebRtc_Word32  payloadLength);

    WebRtc_Word32 PlayoutData(AudioFrame& decodedAudio,
			      WebRtc_UWord16& sampFreqHz);

    WebRtc_Word32 Encode(const AudioFrame& audio,
                         WebRtc_Word8*   encodedData,
			 WebRtc_UWord32& encodedLengthInBytes);

protected:
    virtual WebRtc_Word32 SendData(FrameType frameType,
				   WebRtc_UWord8 payloadType,
				   WebRtc_UWord32 timeStamp,
				   const WebRtc_UWord8* payloadData,
				   WebRtc_UWord16 payloadSize,
				   const RTPFragmentationHeader* fragmentation);

private:
    WebRtc_UWord32 _instanceID;

    AudioCodingModule* _acm;

    CodecInst _receiveCodec;

    WebRtc_UWord32 _encodeTimestamp;
    WebRtc_Word8*  _encodedData;
    WebRtc_UWord32 _encodedLengthInBytes;

    WebRtc_UWord32 _decodeTimestamp;
};
} // namespace webrtc

#endif // WEBRTC_MODULES_UTILITY_SOURCE_CODER_H_
