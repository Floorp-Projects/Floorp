/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This sub-API supports the following functionalities:
//
//  - Support of non-default codecs (e.g. iLBC, iSAC, etc.).
//  - Voice Activity Detection (VAD) on a per channel basis.
//  - Possibility to specify how to map received payload types to codecs.
//
// Usage example, omitting error checking:
//
//  using namespace webrtc;
//  VoiceEngine* voe = VoiceEngine::Create();
//  VoEBase* base = VoEBase::GetInterface(voe);
//  VoECodec* codec = VoECodec::GetInterface(voe);
//  base->Init();
//  int num_of_codecs = codec->NumOfCodecs()
//  ...
//  base->Terminate();
//  base->Release();
//  codec->Release();
//  VoiceEngine::Delete(voe);
//
#ifndef WEBRTC_VOICE_ENGINE_VOE_CODEC_H
#define WEBRTC_VOICE_ENGINE_VOE_CODEC_H

#include "common_types.h"

namespace webrtc {

class VoiceEngine;

class WEBRTC_DLLEXPORT VoECodec
{
public:
    // Factory for the VoECodec sub-API. Increases an internal
    // reference counter if successful. Returns NULL if the API is not
    // supported or if construction fails.
    static VoECodec* GetInterface(VoiceEngine* voiceEngine);

    // Releases the VoECodec sub-API and decreases an internal
    // reference counter. Returns the new reference count. This value should
    // be zero for all sub-API:s before the VoiceEngine object can be safely
    // deleted.
    virtual int Release() = 0;

    // Gets the number of supported codecs.
    virtual int NumOfCodecs() = 0;

    // Get the |codec| information for a specified list |index|.
    virtual int GetCodec(int index, CodecInst& codec) = 0;

    // Sets the |codec| for the |channel| to be used for sending.
    virtual int SetSendCodec(int channel, const CodecInst& codec) = 0;

    // Gets the |codec| parameters for the sending codec on a specified
    // |channel|.
    virtual int GetSendCodec(int channel, CodecInst& codec) = 0;

    // Gets the currently received |codec| for a specific |channel|.
    virtual int GetRecCodec(int channel, CodecInst& codec) = 0;

    // Sets the initial values of target rate and frame size for iSAC
    // for a specified |channel|. This API is only valid if iSAC is setup
    // to run in channel-adaptive mode
    virtual int SetISACInitTargetRate(int channel, int rateBps,
                                      bool useFixedFrameSize = false) = 0;

    // Sets the maximum allowed iSAC rate which the codec may not exceed
    // for a single packet for the specified |channel|. The maximum rate is
    // defined as payload size per frame size in bits per second.
    virtual int SetISACMaxRate(int channel, int rateBps) = 0;

    // Sets the maximum allowed iSAC payload size for a specified |channel|.
    // The maximum value is set independently of the frame size, i.e.
    // 30 ms and 60 ms packets have the same limit.
    virtual int SetISACMaxPayloadSize(int channel, int sizeBytes) = 0;

    // Sets the dynamic payload type number for a particular |codec| or
    // disables (ignores) a codec for receiving. For instance, when receiving
    // an invite from a SIP-based client, this function can be used to change
    // the dynamic payload type number to match that in the INVITE SDP-
    // message. The utilized parameters in the |codec| structure are:
    // plname, plfreq, pltype and channels.
    virtual int SetRecPayloadType(int channel, const CodecInst& codec) = 0;

    // Gets the actual payload type that is set for receiving a |codec| on a
    // |channel|. The value it retrieves will either be the default payload
    // type, or a value earlier set with SetRecPayloadType().
    virtual int GetRecPayloadType(int channel, CodecInst& codec) = 0;

    // Sets the payload |type| for the sending of SID-frames with background
    // noise estimation during silence periods detected by the VAD.
    virtual int SetSendCNPayloadType(
        int channel, int type, PayloadFrequencies frequency = kFreq16000Hz) = 0;


    // Sets the VAD/DTX (silence suppression) status and |mode| for a
    // specified |channel|. Disabling VAD (through |enable|) will also disable
    // DTX; it is not necessary to explictly set |disableDTX| in this case.
    virtual int SetVADStatus(int channel, bool enable,
                             VadModes mode = kVadConventional,
                             bool disableDTX = false) = 0;

    // Gets the VAD/DTX status and |mode| for a specified |channel|.
    virtual int GetVADStatus(int channel, bool& enabled, VadModes& mode,
                             bool& disabledDTX) = 0;

    // Not supported
    virtual int SetAMREncFormat(int channel, AmrMode mode) = 0;

    // Not supported
    virtual int SetAMRDecFormat(int channel, AmrMode mode) = 0;

    // Not supported
    virtual int SetAMRWbEncFormat(int channel, AmrMode mode) = 0;

    // Not supported
    virtual int SetAMRWbDecFormat(int channel, AmrMode mode) = 0;

protected:
    VoECodec() {}
    virtual ~VoECodec() {}
};

} // namespace webrtc

#endif  //  WEBRTC_VOICE_ENGINE_VOE_CODEC_H
