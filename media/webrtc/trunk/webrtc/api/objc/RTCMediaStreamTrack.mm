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

#import "webrtc/api/objc/RTCMediaStreamTrack+Private.h"
#import "webrtc/base/objc/NSString+StdString.h"

@implementation RTCMediaStreamTrack {
  rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> _nativeTrack;
}

- (NSString *)kind {
  return [NSString stringForStdString:_nativeTrack->kind()];
}

- (NSString *)trackId {
  return [NSString stringForStdString:_nativeTrack->id()];
}

- (BOOL)isEnabled {
  return _nativeTrack->enabled();
}

- (void)setIsEnabled:(BOOL)isEnabled {
  _nativeTrack->set_enabled(isEnabled);
}

- (RTCMediaStreamTrackState)readyState {
  return [[self class] trackStateForNativeState:_nativeTrack->state()];
}

- (NSString *)description {
  NSString *readyState = [[self class] stringForState:self.readyState];
  return [NSString stringWithFormat:@"RTCMediaStreamTrack:\n%@\n%@\n%@\n%@",
                                    self.kind,
                                    self.trackId,
                                    self.isEnabled ? @"enabled" : @"disabled",
                                    readyState];
}

#pragma mark - Private

- (rtc::scoped_refptr<webrtc::MediaStreamTrackInterface>)nativeTrack {
  return _nativeTrack;
}

- (instancetype)initWithNativeTrack:
    (rtc::scoped_refptr<webrtc::MediaStreamTrackInterface>)nativeTrack {
  NSParameterAssert(nativeTrack);
  if (self = [super init]) {
    _nativeTrack = nativeTrack;
  }
  return self;
}

+ (webrtc::MediaStreamTrackInterface::TrackState)nativeTrackStateForState:
    (RTCMediaStreamTrackState)state {
  switch (state) {
    case RTCMediaStreamTrackStateInitializing:
      return webrtc::MediaStreamTrackInterface::kInitializing;
    case RTCMediaStreamTrackStateLive:
      return webrtc::MediaStreamTrackInterface::kLive;
    case RTCMediaStreamTrackStateEnded:
      return webrtc::MediaStreamTrackInterface::kEnded;
    case RTCMediaStreamTrackStateFailed:
      return webrtc::MediaStreamTrackInterface::kFailed;
  }
}

+ (RTCMediaStreamTrackState)trackStateForNativeState:
    (webrtc::MediaStreamTrackInterface::TrackState)nativeState {
  switch (nativeState) {
    case webrtc::MediaStreamTrackInterface::kInitializing:
      return RTCMediaStreamTrackStateInitializing;
    case webrtc::MediaStreamTrackInterface::kLive:
      return RTCMediaStreamTrackStateLive;
    case webrtc::MediaStreamTrackInterface::kEnded:
      return RTCMediaStreamTrackStateEnded;
    case webrtc::MediaStreamTrackInterface::kFailed:
      return RTCMediaStreamTrackStateFailed;
  }
}

+ (NSString *)stringForState:(RTCMediaStreamTrackState)state {
  switch (state) {
    case RTCMediaStreamTrackStateInitializing:
      return @"Initializing";
    case RTCMediaStreamTrackStateLive:
      return @"Live";
    case RTCMediaStreamTrackStateEnded:
      return @"Ended";
    case RTCMediaStreamTrackStateFailed:
      return @"Failed";
  }
}

@end
