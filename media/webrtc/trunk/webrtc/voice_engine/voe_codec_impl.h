/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_VOE_CODEC_IMPL_H
#define WEBRTC_VOICE_ENGINE_VOE_CODEC_IMPL_H

#include "webrtc/voice_engine/include/voe_codec.h"

#include "webrtc/voice_engine/shared_data.h"

namespace webrtc
{

class VoECodecImpl: public VoECodec
{
public:
    virtual int NumOfCodecs();

    virtual int GetCodec(int index, CodecInst& codec);

    virtual int SetSendCodec(int channel, const CodecInst& codec);

    virtual int GetSendCodec(int channel, CodecInst& codec);

    virtual int GetRecCodec(int channel, CodecInst& codec);

    virtual int SetSendCNPayloadType(
        int channel, int type,
        PayloadFrequencies frequency = kFreq16000Hz);

    virtual int SetRecPayloadType(int channel,
                                  const CodecInst& codec);

    virtual int GetRecPayloadType(int channel, CodecInst& codec);

    virtual int SetFECStatus(int channel, bool enable);

    virtual int GetFECStatus(int channel, bool& enabled);

    virtual int SetVADStatus(int channel,
                             bool enable,
                             VadModes mode = kVadConventional,
                             bool disableDTX = false);

    virtual int GetVADStatus(int channel,
                             bool& enabled,
                             VadModes& mode,
                             bool& disabledDTX);

    virtual int SetOpusMaxPlaybackRate(int channel, int frequency_hz);

    virtual int SetOpusDtx(int channel, bool enable_dtx);

protected:
    VoECodecImpl(voe::SharedData* shared);
    virtual ~VoECodecImpl();

private:
    void ACMToExternalCodecRepresentation(CodecInst& toInst,
                                          const CodecInst& fromInst);

    void ExternalToACMCodecRepresentation(CodecInst& toInst,
                                          const CodecInst& fromInst);

    voe::SharedData* _shared;
};

}  // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_VOE_CODEC_IMPL_H
