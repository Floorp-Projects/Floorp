/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#import <Foundation/Foundation.h>

#import "RTCMacros.h"
#import "RTCVideoEncoderAV1.h"
#import "RTCWrappedNativeVideoEncoder.h"

#if defined(RTC_USE_LIBAOM_AV1_ENCODER)
#include "modules/video_coding/codecs/av1/libaom_av1_encoder.h"  // nogncheck
#endif

@implementation RTC_OBJC_TYPE (RTCVideoEncoderAV1)

+ (id<RTC_OBJC_TYPE(RTCVideoEncoder)>)av1Encoder {
#if defined(RTC_USE_LIBAOM_AV1_ENCODER)
  std::unique_ptr<webrtc::VideoEncoder> nativeEncoder(webrtc::CreateLibaomAv1Encoder());
  return [[RTC_OBJC_TYPE(RTCWrappedNativeVideoEncoder) alloc]
      initWithNativeEncoder:std::move(nativeEncoder)];
#else
  return nil;
#endif
}

+ (bool)isSupported {
#if defined(RTC_USE_LIBAOM_AV1_ENCODER)
  return true;
#else
  return false;
#endif
}

@end
