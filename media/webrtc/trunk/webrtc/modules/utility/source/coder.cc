/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_types.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/utility/source/coder.h"

namespace webrtc {
AudioCoder::AudioCoder(uint32_t instanceID)
    : _acm(AudioCodingModule::Create(instanceID)),
      _receiveCodec(),
      _encodeTimestamp(0),
      _encodedData(NULL),
      _encodedLengthInBytes(0),
      _decodeTimestamp(0)
{
    _acm->InitializeReceiver();
    _acm->RegisterTransportCallback(this);
}

AudioCoder::~AudioCoder()
{
}

int32_t AudioCoder::SetEncodeCodec(const CodecInst& codecInst)
{
    if(_acm->RegisterSendCodec((CodecInst&)codecInst) == -1)
    {
        return -1;
    }
    return 0;
}

int32_t AudioCoder::SetDecodeCodec(const CodecInst& codecInst)
{
    if(_acm->RegisterReceiveCodec((CodecInst&)codecInst) == -1)
    {
        return -1;
    }
    memcpy(&_receiveCodec,&codecInst,sizeof(CodecInst));
    return 0;
}

int32_t AudioCoder::Decode(AudioFrame& decodedAudio,
                           uint32_t sampFreqHz,
                           const int8_t*  incomingPayload,
                           size_t  payloadLength)
{
    if (payloadLength > 0)
    {
        const uint8_t payloadType = _receiveCodec.pltype;
        _decodeTimestamp += _receiveCodec.pacsize;
        if(_acm->IncomingPayload((const uint8_t*) incomingPayload,
                                 payloadLength,
                                 payloadType,
                                 _decodeTimestamp) == -1)
        {
            return -1;
        }
    }
    return _acm->PlayoutData10Ms((uint16_t)sampFreqHz, &decodedAudio);
}

int32_t AudioCoder::PlayoutData(AudioFrame& decodedAudio,
                                uint16_t& sampFreqHz)
{
    return _acm->PlayoutData10Ms(sampFreqHz, &decodedAudio);
}

int32_t AudioCoder::Encode(const AudioFrame& audio,
                           int8_t* encodedData,
                           size_t& encodedLengthInBytes)
{
    // Fake a timestamp in case audio doesn't contain a correct timestamp.
    // Make a local copy of the audio frame since audio is const
    AudioFrame audioFrame;
    audioFrame.CopyFrom(audio);
    audioFrame.timestamp_ = _encodeTimestamp;
    _encodeTimestamp += static_cast<uint32_t>(audioFrame.samples_per_channel_);

    // For any codec with a frame size that is longer than 10 ms the encoded
    // length in bytes should be zero until a a full frame has been encoded.
    _encodedLengthInBytes = 0;
    if(_acm->Add10MsData((AudioFrame&)audioFrame) == -1)
    {
        return -1;
    }
    _encodedData = encodedData;
    encodedLengthInBytes = _encodedLengthInBytes;
    return 0;
}

int32_t AudioCoder::SendData(
    FrameType /* frameType */,
    uint8_t   /* payloadType */,
    uint32_t  /* timeStamp */,
    const uint8_t*  payloadData,
    size_t  payloadSize,
    const RTPFragmentationHeader* /* fragmentation*/)
{
    memcpy(_encodedData,payloadData,sizeof(uint8_t) * payloadSize);
    _encodedLengthInBytes = payloadSize;
    return 0;
}
}  // namespace webrtc
