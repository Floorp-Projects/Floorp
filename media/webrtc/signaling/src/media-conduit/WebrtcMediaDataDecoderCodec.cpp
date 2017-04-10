/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebrtcMediaDataDecoderCodec.h"
#include "PlatformDecoderModule.h"

namespace mozilla {

class MediaDataDecoder;

WebrtcMediaDataDecoder::WebrtcMediaDataDecoder()
{
}

WebrtcMediaDataDecoder::~WebrtcMediaDataDecoder()
{
}

int32_t
WebrtcMediaDataDecoder::InitDecode(const webrtc::VideoCodec* codecSettings,
                                   int32_t numberOfCores)
{
  return 0;
}

int32_t
WebrtcMediaDataDecoder::Decode(
  const webrtc::EncodedImage& inputImage,
  bool missingFrames,
  const webrtc::RTPFragmentationHeader* fragmentation,
  const webrtc::CodecSpecificInfo* codecSpecificInfo,
  int64_t renderTimeMs)
{
  return 0;
}

int32_t
WebrtcMediaDataDecoder::RegisterDecodeCompleteCallback(
  webrtc::DecodedImageCallback* callback)
{
  return 0;
}

int32_t
WebrtcMediaDataDecoder::Release()
{
  return 0;
}

} // namespace mozilla
