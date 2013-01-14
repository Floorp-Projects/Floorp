/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "coder.h"
#include "common_types.h"
#include "module_common_types.h"

// OS independent case insensitive string comparison.
#ifdef WIN32
    #define STR_CASE_CMP(x,y) ::_stricmp(x,y)
#else
    #define STR_CASE_CMP(x,y) ::strcasecmp(x,y)
#endif

namespace webrtc {
AudioCoder::AudioCoder(WebRtc_UWord32 instanceID)
    : _acm(AudioCodingModule::Create(instanceID)),
      _receiveCodec(),
      _encodeTimestamp(0),
      _encodedData(NULL),
      _encodedLengthInBytes(0),
      _decodeTimestamp(0)
{
    _acm->InitializeSender();
    _acm->InitializeReceiver();
    _acm->RegisterTransportCallback(this);
}

AudioCoder::~AudioCoder()
{
    AudioCodingModule::Destroy(_acm);
}

WebRtc_Word32 AudioCoder::SetEncodeCodec(const CodecInst& codecInst,
					 ACMAMRPackingFormat amrFormat)
{
    if(_acm->RegisterSendCodec((CodecInst&)codecInst) == -1)
    {
        return -1;
    }
    return 0;
}

WebRtc_Word32 AudioCoder::SetDecodeCodec(const CodecInst& codecInst,
					 ACMAMRPackingFormat amrFormat)
{
    if(_acm->RegisterReceiveCodec((CodecInst&)codecInst) == -1)
    {
        return -1;
    }
    memcpy(&_receiveCodec,&codecInst,sizeof(CodecInst));
    return 0;
}

WebRtc_Word32 AudioCoder::Decode(AudioFrame& decodedAudio,
				 WebRtc_UWord32 sampFreqHz,
				 const WebRtc_Word8*  incomingPayload,
				 WebRtc_Word32  payloadLength)
{
    if (payloadLength > 0)
    {
        const WebRtc_UWord8 payloadType = _receiveCodec.pltype;
        _decodeTimestamp += _receiveCodec.pacsize;
        if(_acm->IncomingPayload((const WebRtc_UWord8*) incomingPayload,
                                 payloadLength,
                                 payloadType,
                                 _decodeTimestamp) == -1)
        {
            return -1;
        }
    }
    return _acm->PlayoutData10Ms((WebRtc_UWord16)sampFreqHz,
				 (AudioFrame&)decodedAudio);
}

WebRtc_Word32 AudioCoder::PlayoutData(AudioFrame& decodedAudio,
				      WebRtc_UWord16& sampFreqHz)
{
    return _acm->PlayoutData10Ms(sampFreqHz, (AudioFrame&)decodedAudio);
}

WebRtc_Word32 AudioCoder::Encode(const AudioFrame& audio,
				 WebRtc_Word8* encodedData,
				 WebRtc_UWord32& encodedLengthInBytes)
{
    // Fake a timestamp in case audio doesn't contain a correct timestamp.
    // Make a local copy of the audio frame since audio is const
    AudioFrame audioFrame = audio;
    audioFrame.timestamp_ = _encodeTimestamp;
    _encodeTimestamp += audioFrame.samples_per_channel_;

    // For any codec with a frame size that is longer than 10 ms the encoded
    // length in bytes should be zero until a a full frame has been encoded.
    _encodedLengthInBytes = 0;
    if(_acm->Add10MsData((AudioFrame&)audioFrame) == -1)
    {
        return -1;
    }
    _encodedData = encodedData;
    if(_acm->Process() == -1)
    {
        return -1;
    }
    encodedLengthInBytes = _encodedLengthInBytes;
    return 0;
}

WebRtc_Word32 AudioCoder::SendData(
    FrameType /* frameType */,
    WebRtc_UWord8   /* payloadType */,
    WebRtc_UWord32  /* timeStamp */,
    const WebRtc_UWord8*  payloadData,
    WebRtc_UWord16  payloadSize,
    const RTPFragmentationHeader* /* fragmentation*/)
{
    memcpy(_encodedData,payloadData,sizeof(WebRtc_UWord8) * payloadSize);
    _encodedLengthInBytes = payloadSize;
    return 0;
}
} // namespace webrtc
