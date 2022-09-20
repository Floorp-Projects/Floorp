/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCRtcpParameters+Private.h"

#import "helpers/NSString+RTCStdString.h"

@implementation RTC_OBJC_TYPE (RTCRtcpParameters)

@synthesize cname = _cname;
@synthesize isReducedSize = _isReducedSize;

- (instancetype)init {
  webrtc::RtcpParameters nativeParameters;
  return [self initWithNativeParameters:nativeParameters];
}

- (instancetype)initWithNativeParameters:(const webrtc::RtcpParameters &)nativeParameters {
  if (self = [super init]) {
    _cname = [NSString rtc_stringForStdString:nativeParameters.cname];
    _isReducedSize = nativeParameters.reduced_size;
  }
  return self;
}

- (webrtc::RtcpParameters)nativeParameters {
  webrtc::RtcpParameters parameters;
  parameters.cname = [NSString rtc_stdStringForString:_cname];
  parameters.reduced_size = _isReducedSize;
  return parameters;
}

@end
