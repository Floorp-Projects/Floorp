/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCMediaStreamTrack.h"

#include "talk/app/webrtc/mediastreaminterface.h"
#include "webrtc/base/scoped_ptr.h"

NS_ASSUME_NONNULL_BEGIN

@interface RTCMediaStreamTrack ()

/**
 * The native MediaStreamTrackInterface representation of this
 * RTCMediaStreamTrack object. This is needed to pass to the underlying C++
 * APIs.
 */
@property(nonatomic, readonly)
    rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> nativeTrack;

/**
 * Initialize an RTCMediaStreamTrack from a native MediaStreamTrackInterface.
 */
- (instancetype)initWithNativeTrack:
    (rtc::scoped_refptr<webrtc::MediaStreamTrackInterface>)nativeTrack
    NS_DESIGNATED_INITIALIZER;

+ (webrtc::MediaStreamTrackInterface::TrackState)nativeTrackStateForState:
    (RTCMediaStreamTrackState)state;

+ (RTCMediaStreamTrackState)trackStateForNativeState:
    (webrtc::MediaStreamTrackInterface::TrackState)nativeState;

+ (NSString *)stringForState:(RTCMediaStreamTrackState)state;

@end

NS_ASSUME_NONNULL_END
