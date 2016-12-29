/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#include "webrtc/modules/video_coding/codecs/h264/include/h264.h"

#if defined(WEBRTC_IOS)
#include "webrtc/modules/video_coding/codecs/h264/h264_video_toolbox_decoder.h"
#include "webrtc/modules/video_coding/codecs/h264/h264_video_toolbox_encoder.h"
#endif

#include "webrtc/base/checks.h"

namespace webrtc {

// We need this file to be C++ only so it will compile properly for all
// platforms. In order to write ObjC specific implementations we use private
// externs. This function is defined in h264.mm.
#if defined(WEBRTC_IOS)
extern bool IsH264CodecSupportedObjC();
#endif

bool IsH264CodecSupported() {
#if defined(WEBRTC_IOS)
  return IsH264CodecSupportedObjC();
#else
  return false;
#endif
}

H264Encoder* H264Encoder::Create() {
  RTC_DCHECK(H264Encoder::IsSupported());
#if defined(WEBRTC_IOS) && defined(WEBRTC_VIDEO_TOOLBOX_SUPPORTED)
  return new H264VideoToolboxEncoder();
#else
  RTC_NOTREACHED();
  return nullptr;
#endif
}

bool H264Encoder::IsSupported() {
  return IsH264CodecSupported();
}

H264Decoder* H264Decoder::Create() {
  RTC_DCHECK(H264Decoder::IsSupported());
#if defined(WEBRTC_IOS) && defined(WEBRTC_VIDEO_TOOLBOX_SUPPORTED)
  return new H264VideoToolboxDecoder();
#else
  RTC_NOTREACHED();
  return nullptr;
#endif
}

bool H264Decoder::IsSupported() {
  return IsH264CodecSupported();
}

}  // namespace webrtc
