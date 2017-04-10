/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WebrtcMediaDataDecoderCodec_h__
#define WebrtcMediaDataDecoderCodec_h__

#include "MediaConduitInterface.h"
#include "mozilla/RefPtr.h"

#include "webrtc/modules/video_coding/include/video_codec_interface.h"

namespace mozilla {

class MediaDataDecoder;

class WebrtcMediaDataDecoder : public WebrtcVideoDecoder
{
public:
  WebrtcMediaDataDecoder();

  virtual ~WebrtcMediaDataDecoder();

  // Implement VideoDecoder interface.
  uint64_t PluginID() const override { return 0; }

  int32_t InitDecode(const webrtc::VideoCodec* codecSettings,
                     int32_t numberOfCores) override;

  int32_t Decode(const webrtc::EncodedImage& inputImage,
                 bool missingFrames,
                 const webrtc::RTPFragmentationHeader* fragmentation,
                 const webrtc::CodecSpecificInfo* codecSpecificInfo = NULL,
                 int64_t renderTimeMs = -1) override;

  int32_t RegisterDecodeCompleteCallback(
    webrtc::DecodedImageCallback* callback) override;

  int32_t Release() override;
};

} // namespace mozilla

#endif // WebrtcMediaDataDecoderCodec_h__
