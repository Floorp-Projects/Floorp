/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <map>
#include <string>

#include "modules/audio_coding/neteq/neteq_decoder_enum.h"

namespace webrtc {

rtc::Optional<SdpAudioFormat> NetEqDecoderToSdpAudioFormat(NetEqDecoder nd) {
  switch (nd) {
    case NetEqDecoder::kDecoderPCMu:
      return SdpAudioFormat("pcmu", 8000, 1);
    case NetEqDecoder::kDecoderPCMa:
      return SdpAudioFormat("pcma", 8000, 1);
    case NetEqDecoder::kDecoderPCMu_2ch:
      return SdpAudioFormat("pcmu", 8000, 2);
    case NetEqDecoder::kDecoderPCMa_2ch:
      return SdpAudioFormat("pcma", 8000, 2);
    case NetEqDecoder::kDecoderILBC:
      return SdpAudioFormat("ilbc", 8000, 1);
    case NetEqDecoder::kDecoderISAC:
      return SdpAudioFormat("isac", 16000, 1);
    case NetEqDecoder::kDecoderISACswb:
      return SdpAudioFormat("isac", 32000, 1);
    case NetEqDecoder::kDecoderPCM16B:
      return SdpAudioFormat("l16", 8000, 1);
    case NetEqDecoder::kDecoderPCM16Bwb:
      return SdpAudioFormat("l16", 16000, 1);
    case NetEqDecoder::kDecoderPCM16Bswb32kHz:
      return SdpAudioFormat("l16", 32000, 1);
    case NetEqDecoder::kDecoderPCM16Bswb48kHz:
      return SdpAudioFormat("l16", 48000, 1);
    case NetEqDecoder::kDecoderPCM16B_2ch:
      return SdpAudioFormat("l16", 8000, 2);
    case NetEqDecoder::kDecoderPCM16Bwb_2ch:
      return SdpAudioFormat("l16", 16000, 2);
    case NetEqDecoder::kDecoderPCM16Bswb32kHz_2ch:
      return SdpAudioFormat("l16", 32000, 2);
    case NetEqDecoder::kDecoderPCM16Bswb48kHz_2ch:
      return SdpAudioFormat("l16", 48000, 2);
    case NetEqDecoder::kDecoderPCM16B_5ch:
      return SdpAudioFormat("l16", 8000, 5);
    case NetEqDecoder::kDecoderG722:
      return SdpAudioFormat("g722", 8000, 1);
    case NetEqDecoder::kDecoderG722_2ch:
      return SdpAudioFormat("g722", 8000, 2);
    case NetEqDecoder::kDecoderOpus:
      return SdpAudioFormat("opus", 48000, 2);
    case NetEqDecoder::kDecoderOpus_2ch:
      return SdpAudioFormat(
          "opus", 48000, 2,
          std::map<std::string, std::string>{{"stereo", "1"}});
    case NetEqDecoder::kDecoderRED:
      return SdpAudioFormat("red", 8000, 1);
    case NetEqDecoder::kDecoderAVT:
      return SdpAudioFormat("telephone-event", 8000, 1);
    case NetEqDecoder::kDecoderAVT16kHz:
      return SdpAudioFormat("telephone-event", 16000, 1);
    case NetEqDecoder::kDecoderAVT32kHz:
      return SdpAudioFormat("telephone-event", 32000, 1);
    case NetEqDecoder::kDecoderAVT48kHz:
      return SdpAudioFormat("telephone-event", 48000, 1);
    case NetEqDecoder::kDecoderCNGnb:
      return SdpAudioFormat("cn", 8000, 1);
    case NetEqDecoder::kDecoderCNGwb:
      return SdpAudioFormat("cn", 16000, 1);
    case NetEqDecoder::kDecoderCNGswb32kHz:
      return SdpAudioFormat("cn", 32000, 1);
    case NetEqDecoder::kDecoderCNGswb48kHz:
      return SdpAudioFormat("cn", 48000, 1);
    default:
      return rtc::nullopt;
  }
}

}  // namespace webrtc
