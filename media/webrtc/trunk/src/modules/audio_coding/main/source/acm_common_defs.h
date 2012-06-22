/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_COMMON_DEFS_H_
#define WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_COMMON_DEFS_H_

#include <string.h>

#include "audio_coding_module_typedefs.h"
#include "common_types.h"
#include "engine_configurations.h"
#include "typedefs.h"

// Checks for enabled codecs, we prevent enabling codecs which are not
// compatible.
#if ((defined WEBRTC_CODEC_ISAC) && (defined WEBRTC_CODEC_ISACFX))
#error iSAC and iSACFX codecs cannot be enabled at the same time
#endif

#ifdef WIN32
// OS-dependent case-insensitive string comparison
#define STR_CASE_CMP(x,y) ::_stricmp(x,y)
#else
// OS-dependent case-insensitive string comparison
#define STR_CASE_CMP(x,y) ::strcasecmp(x,y)
#endif

namespace webrtc {

// 60 ms is the maximum block size we support. An extra 20 ms is considered
// for safety if process() method is not called when it should be, i.e. we
// accept 20 ms of jitter. 80 ms @ 32 kHz (super wide-band) is 2560 samples.
#define AUDIO_BUFFER_SIZE_W16  2560

// There is one timestamp per each 10 ms of audio
// the audio buffer, at max, may contain 32 blocks of 10ms
// audio if the sampling frequency is 8000 Hz (80 samples per block).
// Therefore, The size of the buffer where we keep timestamps
// is defined as follows
#define TIMESTAMP_BUFFER_SIZE_W32  (AUDIO_BUFFER_SIZE_W16/80)

// The maximum size of a payload, that is 60 ms of PCM-16 @ 32 kHz stereo
#define MAX_PAYLOAD_SIZE_BYTE   7680

// General codec specific defines
const int kIsacWbDefaultRate = 32000;
const int kIsacSwbDefaultRate = 56000;
const int kIsacPacSize480 = 480;
const int kIsacPacSize960 = 960;

// An encoded bit-stream is labeled by one of the following enumerators.
//
//   kNoEncoding              : There has been no encoding.
//   kActiveNormalEncoded     : Active audio frame coded by the codec.
//   kPassiveNormalEncoded    : Passive audio frame coded by the codec.
//   kPassiveDTXNB            : Passive audio frame coded by narrow-band CN.
//   kPassiveDTXWB            : Passive audio frame coded by wide-band CN.
//   kPassiveDTXSWB           : Passive audio frame coded by super-wide-band CN.
//
enum WebRtcACMEncodingType {
  kNoEncoding,
  kActiveNormalEncoded,
  kPassiveNormalEncoded,
  kPassiveDTXNB,
  kPassiveDTXWB,
  kPassiveDTXSWB
};

// A structure which contains codec parameters. For instance, used when
// initializing encoder and decoder.
//
//   codecInstant            : c.f. common_types.h
//   enableDTX               : set true to enable DTX. If codec does not have
//                             internal DTX, this will enable VAD.
//   enableVAD               : set true to enable VAD.
//   vadMode                 : VAD mode, c.f. audio_coding_module_typedefs.h
//                             for possible values.
struct WebRtcACMCodecParams {
  CodecInst codecInstant;
  bool enableDTX;
  bool enableVAD;
  ACMVADMode vadMode;
};

// A structure that encapsulates audio buffer and related parameters
// used for synchronization of audio of two ACMs.
//
//   inAudio                 : same as ACMGenericCodec::_inAudio
//   inAudioIxRead           : same as ACMGenericCodec::_inAudioIxRead
//   inAudioIxWrite          : same as ACMGenericCodec::_inAudioIxWrite
//   inTimestamp             : same as ACMGenericCodec::_inTimestamp
//   inTimestampIxWrite      : same as ACMGenericCodec::_inTImestampIxWrite
//   lastTimestamp           : same as ACMGenericCodec::_lastTimestamp
//   lastInTimestamp         : same as AudioCodingModuleImpl::_lastInTimestamp
//
struct WebRtcACMAudioBuff {
  WebRtc_Word16 inAudio[AUDIO_BUFFER_SIZE_W16];
  WebRtc_Word16 inAudioIxRead;
  WebRtc_Word16 inAudioIxWrite;
  WebRtc_UWord32 inTimestamp[TIMESTAMP_BUFFER_SIZE_W32];
  WebRtc_Word16 inTimestampIxWrite;
  WebRtc_UWord32 lastTimestamp;
  WebRtc_UWord32 lastInTimestamp;
};

} // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_COMMON_DEFS_H_
