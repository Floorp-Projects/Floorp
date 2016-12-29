/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCMediaSource.h"

#include "talk/app/webrtc/mediastreaminterface.h"

NS_ASSUME_NONNULL_BEGIN

@interface RTCMediaSource ()

/**
 * The MediaSourceInterface object passed to this RTCMediaSource during
 * construction.
 */
@property(nonatomic, readonly)
    rtc::scoped_refptr<webrtc::MediaSourceInterface> nativeMediaSource;

/** Initialize an RTCMediaSource from a native MediaSourceInterface. */
- (instancetype)initWithNativeMediaSource:
    (rtc::scoped_refptr<webrtc::MediaSourceInterface>)nativeMediaSource
    NS_DESIGNATED_INITIALIZER;

+ (webrtc::MediaSourceInterface::SourceState)nativeSourceStateForState:
    (RTCSourceState)state;

+ (RTCSourceState)sourceStateForNativeState:
    (webrtc::MediaSourceInterface::SourceState)nativeState;

+ (NSString *)stringForState:(RTCSourceState)state;

@end

NS_ASSUME_NONNULL_END
