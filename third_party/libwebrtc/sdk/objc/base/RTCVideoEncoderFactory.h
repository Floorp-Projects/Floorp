/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#import "RTCMacros.h"
#import "RTCVideoCodecInfo.h"
#import "RTCVideoEncoder.h"

NS_ASSUME_NONNULL_BEGIN

/** RTCVideoEncoderFactory is an Objective-C version of
 webrtc::VideoEncoderFactory::VideoEncoderSelector.
 */
RTC_OBJC_EXPORT
@protocol RTC_OBJC_TYPE
(RTCVideoEncoderSelector)<NSObject>

    - (void)registerCurrentEncoderInfo : (RTC_OBJC_TYPE(RTCVideoCodecInfo) *)info;
- (nullable RTC_OBJC_TYPE(RTCVideoCodecInfo) *)encoderForBitrate:(NSInteger)bitrate;
- (nullable RTC_OBJC_TYPE(RTCVideoCodecInfo) *)encoderForBrokenEncoder;

@optional
- (nullable RTC_OBJC_TYPE(RTCVideoCodecInfo) *)encoderForResolutionChangeBySize:(CGSize)size;

@end

/** RTCVideoEncoderFactory is an Objective-C version of webrtc::VideoEncoderFactory.
 */
RTC_OBJC_EXPORT
@protocol RTC_OBJC_TYPE
(RTCVideoEncoderFactory)<NSObject>

    - (nullable id<RTC_OBJC_TYPE(RTCVideoEncoder)>)createEncoder
    : (RTC_OBJC_TYPE(RTCVideoCodecInfo) *)info;
- (NSArray<RTC_OBJC_TYPE(RTCVideoCodecInfo) *> *)
    supportedCodecs;  // TODO(andersc): "supportedFormats" instead?

@optional
- (NSArray<RTC_OBJC_TYPE(RTCVideoCodecInfo) *> *)implementations;
- (nullable id<RTC_OBJC_TYPE(RTCVideoEncoderSelector)>)encoderSelector;

@end

NS_ASSUME_NONNULL_END
