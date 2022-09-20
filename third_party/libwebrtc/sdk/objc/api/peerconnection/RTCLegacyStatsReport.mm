/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCLegacyStatsReport+Private.h"

#import "base/RTCLogging.h"
#import "helpers/NSString+RTCStdString.h"

#include "rtc_base/checks.h"

@implementation RTC_OBJC_TYPE (RTCLegacyStatsReport)

@synthesize timestamp = _timestamp;
@synthesize type = _type;
@synthesize reportId = _reportId;
@synthesize values = _values;

- (NSString *)description {
  return [NSString stringWithFormat:@"RTC_OBJC_TYPE(RTCLegacyStatsReport):\n%@\n%@\n%f\n%@",
                                    _reportId,
                                    _type,
                                    _timestamp,
                                    _values];
}

#pragma mark - Private

- (instancetype)initWithNativeReport:(const webrtc::StatsReport &)nativeReport {
  if (self = [super init]) {
    _timestamp = nativeReport.timestamp();
    _type = [NSString rtc_stringForStdString:nativeReport.TypeToString()];
    _reportId = [NSString rtc_stringForStdString:nativeReport.id()->ToString()];

    NSUInteger capacity = nativeReport.values().size();
    NSMutableDictionary *values =
        [NSMutableDictionary dictionaryWithCapacity:capacity];
    for (auto const &valuePair : nativeReport.values()) {
      NSString *key = [NSString rtc_stringForStdString:valuePair.second->display_name()];
      NSString *value = [NSString rtc_stringForStdString:valuePair.second->ToString()];

      // Not expecting duplicate keys.
      RTC_DCHECK(![values objectForKey:key]);
      [values setObject:value forKey:key];
    }
    _values = values;
  }
  return self;
}

@end
