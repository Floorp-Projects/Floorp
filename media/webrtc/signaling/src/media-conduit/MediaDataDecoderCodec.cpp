/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaDataDecoderCodec.h"

namespace mozilla {

/* static */ WebrtcVideoEncoder*
MediaDataDecoderCodec::CreateEncoder(
  webrtc::VideoCodecType aCodecType)
{
  return nullptr;
}

/* static */ WebrtcVideoDecoder*
MediaDataDecoderCodec::CreateDecoder(
  webrtc::VideoCodecType aCodecbType)
{
  return nullptr;
}

} // namespace mozilla