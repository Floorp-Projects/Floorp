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

#import "webrtc/api/objc/RTCMediaSource+Private.h"

@implementation RTCMediaSource {
  rtc::scoped_refptr<webrtc::MediaSourceInterface> _nativeMediaSource;
}

- (RTCSourceState)state {
  return [[self class] sourceStateForNativeState:_nativeMediaSource->state()];
}

- (NSString *)description {
  return [NSString stringWithFormat:@"RTCMediaSource:\n%@",
                                    [[self class] stringForState:self.state]];
}

#pragma mark - Private

- (rtc::scoped_refptr<webrtc::MediaSourceInterface>)nativeMediaSource {
  return _nativeMediaSource;
}

- (instancetype)initWithNativeMediaSource:
    (rtc::scoped_refptr<webrtc::MediaSourceInterface>)nativeMediaSource {
  NSParameterAssert(nativeMediaSource);
  if (self = [super init]) {
    _nativeMediaSource = nativeMediaSource;
  }
  return self;
}

+ (webrtc::MediaSourceInterface::SourceState)nativeSourceStateForState:
    (RTCSourceState)state {
  switch (state) {
    case RTCSourceStateInitializing:
      return webrtc::MediaSourceInterface::kInitializing;
    case RTCSourceStateLive:
      return webrtc::MediaSourceInterface::kLive;
    case RTCSourceStateEnded:
      return webrtc::MediaSourceInterface::kEnded;
    case RTCSourceStateMuted:
      return webrtc::MediaSourceInterface::kMuted;
  }
}

+ (RTCSourceState)sourceStateForNativeState:
    (webrtc::MediaSourceInterface::SourceState)nativeState {
  switch (nativeState) {
    case webrtc::MediaSourceInterface::kInitializing:
      return RTCSourceStateInitializing;
    case webrtc::MediaSourceInterface::kLive:
      return RTCSourceStateLive;
    case webrtc::MediaSourceInterface::kEnded:
      return RTCSourceStateEnded;
    case webrtc::MediaSourceInterface::kMuted:
      return RTCSourceStateMuted;
  }
}

+ (NSString *)stringForState:(RTCSourceState)state {
  switch (state) {
    case RTCSourceStateInitializing:
      return @"Initializing";
    case RTCSourceStateLive:
      return @"Live";
    case RTCSourceStateEnded:
      return @"Ended";
    case RTCSourceStateMuted:
      return @"Muted";
  }
}

@end
