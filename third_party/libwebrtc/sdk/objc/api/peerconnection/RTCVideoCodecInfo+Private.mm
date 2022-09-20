/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCVideoCodecInfo+Private.h"

#import "helpers/NSString+RTCStdString.h"

@implementation RTC_OBJC_TYPE (RTCVideoCodecInfo)
(Private)

    - (instancetype)initWithNativeSdpVideoFormat : (webrtc::SdpVideoFormat)format {
  NSMutableDictionary *params = [NSMutableDictionary dictionary];
  for (auto it = format.parameters.begin(); it != format.parameters.end(); ++it) {
    [params setObject:[NSString rtc_stringForStdString:it->second]
               forKey:[NSString rtc_stringForStdString:it->first]];
  }
  return [self initWithName:[NSString rtc_stringForStdString:format.name] parameters:params];
}

- (webrtc::SdpVideoFormat)nativeSdpVideoFormat {
  std::map<std::string, std::string> parameters;
  for (NSString *paramKey in self.parameters.allKeys) {
    std::string key = [NSString rtc_stdStringForString:paramKey];
    std::string value = [NSString rtc_stdStringForString:self.parameters[paramKey]];
    parameters[key] = value;
  }

  return webrtc::SdpVideoFormat([NSString rtc_stdStringForString:self.name], parameters);
}

@end
