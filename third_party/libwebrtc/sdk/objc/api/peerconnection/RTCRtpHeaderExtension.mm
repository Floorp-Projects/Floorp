/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCRtpHeaderExtension+Private.h"

#import "helpers/NSString+RTCStdString.h"

@implementation RTC_OBJC_TYPE (RTCRtpHeaderExtension)

@synthesize uri = _uri;
@synthesize id = _id;
@synthesize encrypted = _encrypted;

- (instancetype)init {
  webrtc::RtpExtension nativeExtension;
  return [self initWithNativeParameters:nativeExtension];
}

- (instancetype)initWithNativeParameters:(const webrtc::RtpExtension &)nativeParameters {
  if (self = [super init]) {
    _uri = [NSString rtc_stringForStdString:nativeParameters.uri];
    _id = nativeParameters.id;
    _encrypted = nativeParameters.encrypt;
  }
  return self;
}

- (webrtc::RtpExtension)nativeParameters {
  webrtc::RtpExtension extension;
  extension.uri = [NSString rtc_stdStringForString:_uri];
  extension.id = _id;
  extension.encrypt = _encrypted;
  return extension;
}

@end
