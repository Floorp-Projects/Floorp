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

#include "voe_codec.h"

#include "shared_data.h"

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

    virtual int SetAMREncFormat(int channel,
                                AmrMode mode = kRfc3267BwEfficient);

    virtual int SetAMRDecFormat(int channel,
                                AmrMode mode = kRfc3267BwEfficient);

    virtual int SetAMRWbEncFormat(int channel,
                                  AmrMode mode = kRfc3267BwEfficient);

    virtual int SetAMRWbDecFormat(int channel,
                                  AmrMode mode = kRfc3267BwEfficient);

    virtual int SetSendCNPayloadType(
        int channel, int type,
        PayloadFrequencies frequency = kFreq16000Hz);

    virtual int SetRecPayloadType(int channel,
                                  const CodecInst& codec);

    virtual int GetRecPayloadType(int channel, CodecInst& codec);

    virtual int SetISACInitTargetRate(int channel,
                                      int rateBps,
                                      bool useFixedFrameSize = false);

    virtual int SetISACMaxRate(int channel, int rateBps);

    virtual int SetISACMaxPayloadSize(int channel, int sizeBytes);

    virtual int SetVADStatus(int channel,
                             bool enable,
                             VadModes mode = kVadConventional,
                             bool disableDTX = false);

    virtual int GetVADStatus(int channel,
                             bool& enabled,
                             VadModes& mode,
                             bool& disabledDTX);

    // Dual-streaming
    virtual int SetSecondarySendCodec(int channel, const CodecInst& codec,
                                      int red_payload_type);

    virtual int RemoveSecondarySendCodec(int channel);

    virtual int GetSecondarySendCodec(int channel, CodecInst& codec);

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

} // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_VOE_CODEC_IMPL_H
