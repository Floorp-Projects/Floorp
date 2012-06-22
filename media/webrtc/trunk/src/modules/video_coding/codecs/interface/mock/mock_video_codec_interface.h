/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_INTERFACE_MOCK_MOCK_VIDEO_CODEC_INTERFACE_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_INTERFACE_MOCK_MOCK_VIDEO_CODEC_INTERFACE_H_

#include <string>

#include "gmock/gmock.h"
#include "modules/video_coding/codecs/interface/video_codec_interface.h"
#include "typedefs.h"

namespace webrtc {

class MockEncodedImageCallback : public EncodedImageCallback {
 public:
  MOCK_METHOD3(Encoded,
               WebRtc_Word32(EncodedImage& encodedImage,
                             const CodecSpecificInfo* codecSpecificInfo,
                             const RTPFragmentationHeader* fragmentation));
};

class MockVideoEncoder : public VideoEncoder {
 public:
  MOCK_CONST_METHOD2(Version,
                     WebRtc_Word32(WebRtc_Word8 *version,
                                   WebRtc_Word32 length));
  MOCK_METHOD3(InitEncode,
               WebRtc_Word32(const VideoCodec* codecSettings,
                             WebRtc_Word32 numberOfCores,
                             WebRtc_UWord32 maxPayloadSize));
  MOCK_METHOD3(Encode,
               WebRtc_Word32(const RawImage& inputImage,
                             const CodecSpecificInfo* codecSpecificInfo,
                             const VideoFrameType frameType));
  MOCK_METHOD1(RegisterEncodeCompleteCallback,
               WebRtc_Word32(EncodedImageCallback* callback));
  MOCK_METHOD0(Release, WebRtc_Word32());
  MOCK_METHOD0(Reset, WebRtc_Word32());
  MOCK_METHOD2(SetChannelParameters, WebRtc_Word32(WebRtc_UWord32 packetLoss,
                                                   int rtt));
  MOCK_METHOD2(SetRates,
               WebRtc_Word32(WebRtc_UWord32 newBitRate,
                             WebRtc_UWord32 frameRate));
  MOCK_METHOD1(SetPeriodicKeyFrames, WebRtc_Word32(bool enable));
  MOCK_METHOD2(CodecConfigParameters,
               WebRtc_Word32(WebRtc_UWord8* /*buffer*/, WebRtc_Word32));
};

class MockDecodedImageCallback : public DecodedImageCallback {
 public:
  MOCK_METHOD1(Decoded,
               WebRtc_Word32(RawImage& decodedImage));
  MOCK_METHOD1(ReceivedDecodedReferenceFrame,
               WebRtc_Word32(const WebRtc_UWord64 pictureId));
  MOCK_METHOD1(ReceivedDecodedFrame,
               WebRtc_Word32(const WebRtc_UWord64 pictureId));
};

class MockVideoDecoder : public VideoDecoder {
 public:
  MOCK_METHOD2(InitDecode,
      WebRtc_Word32(const VideoCodec* codecSettings,
                    WebRtc_Word32 numberOfCores));
  MOCK_METHOD5(Decode,
               WebRtc_Word32(const EncodedImage& inputImage,
                             bool missingFrames,
                             const RTPFragmentationHeader* fragmentation,
                             const CodecSpecificInfo* codecSpecificInfo,
                             WebRtc_Word64 renderTimeMs));
  MOCK_METHOD1(RegisterDecodeCompleteCallback,
               WebRtc_Word32(DecodedImageCallback* callback));
  MOCK_METHOD0(Release, WebRtc_Word32());
  MOCK_METHOD0(Reset, WebRtc_Word32());
  MOCK_METHOD2(SetCodecConfigParameters,
               WebRtc_Word32(const WebRtc_UWord8* /*buffer*/, WebRtc_Word32));
  MOCK_METHOD0(Copy, VideoDecoder*());
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_CODECS_INTERFACE_MOCK_MOCK_VIDEO_CODEC_INTERFACE_H_
