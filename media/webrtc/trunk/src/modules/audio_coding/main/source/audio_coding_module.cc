/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "acm_dtmf_detection.h"
#include "audio_coding_module.h"
#include "audio_coding_module_impl.h"
#include "trace.h"

namespace webrtc
{

// Create module
AudioCodingModule*
AudioCodingModule::Create(
    const WebRtc_Word32 id)
{
    return new AudioCodingModuleImpl(id);
}

// Destroy module
void
AudioCodingModule::Destroy(
        AudioCodingModule* module)
{
    delete static_cast<AudioCodingModuleImpl*> (module);
}

// Get number of supported codecs
WebRtc_UWord8 AudioCodingModule::NumberOfCodecs()
{
    return static_cast<WebRtc_UWord8>(ACMCodecDB::kNumCodecs);
}

// Get supported codec param with id
WebRtc_Word32
AudioCodingModule::Codec(
    const WebRtc_UWord8 listId,
    CodecInst&          codec)
{
    // Get the codec settings for the codec with the given list ID
    return ACMCodecDB::Codec(listId, &codec);
}

// Get supported codec Param with name, frequency and number of channels.
WebRtc_Word32 AudioCodingModule::Codec(const char* payload_name,
                                       CodecInst& codec,
                                       int sampling_freq_hz,
                                       int channels) {
  int codec_id;

  // Get the id of the codec from the database.
  codec_id = ACMCodecDB::CodecId(payload_name, sampling_freq_hz, channels);
  if (codec_id < 0) {
    // We couldn't find a matching codec, set the parameterss to unacceptable
    // values and return.
    codec.plname[0] = '\0';
    codec.pltype    = -1;
    codec.pacsize   = 0;
    codec.rate      = 0;
    codec.plfreq    = 0;
    return -1;
  }

  // Get default codec settings.
  ACMCodecDB::Codec(codec_id, &codec);

  return 0;
}

// Get supported codec Index with name, frequency and number of channels.
WebRtc_Word32 AudioCodingModule::Codec(const char* payload_name,
                                       int sampling_freq_hz,
                                       int channels) {
  return ACMCodecDB::CodecId(payload_name, sampling_freq_hz, channels);
}

// Checks the validity of the parameters of the given codec
bool
AudioCodingModule::IsCodecValid(
    const CodecInst& codec)
{
    int mirrorID;
    char errMsg[500];

    int codecNumber = ACMCodecDB::CodecNumber(&codec, &mirrorID, errMsg, 500);

    if(codecNumber < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, -1, errMsg);
        return false;
    }
    else
    {
        return true;
    }
}

} // namespace webrtc
