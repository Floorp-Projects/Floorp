/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaDataDecoderCodec.h"
#include "WebrtcMediaDataDecoderCodec.h"
#include "mozilla/StaticPrefs.h"

namespace mozilla {

/* static */ WebrtcVideoEncoder*
MediaDataDecoderCodec::CreateEncoder(
  webrtc::VideoCodecType aCodecType)
{
  return nullptr;
}

/* static */ WebrtcVideoDecoder*
MediaDataDecoderCodec::CreateDecoder(
  webrtc::VideoCodecType aCodecType)
{
  if (!StaticPrefs::MediaNavigatorMediadatadecoderEnabled()) {
    return nullptr;
  }

  switch (aCodecType) {
    case webrtc::VideoCodecType::kVideoCodecVP8:
    case webrtc::VideoCodecType::kVideoCodecVP9:
    case webrtc::VideoCodecType::kVideoCodecH264:
      break;
    default:
      return nullptr;
  }
  return new WebrtcMediaDataDecoder();
}

} // namespace mozilla
