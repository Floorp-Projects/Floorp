/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCStatsReport.h"

#include "webrtc/base/checks.h"

#import "webrtc/api/objc/RTCStatsReport+Private.h"
#import "webrtc/base/objc/NSString+StdString.h"
#import "webrtc/base/objc/RTCLogging.h"

@implementation RTCStatsReport

@synthesize timestamp = _timestamp;
@synthesize type = _type;
@synthesize statsId = _statsId;
@synthesize values = _values;

- (NSString *)description {
  return [NSString stringWithFormat:@"RTCStatsReport:\n%@\n%@\n%f\n%@",
                                    _statsId,
                                    _type,
                                    _timestamp,
                                    _values];
}

#pragma mark - Private

- (instancetype)initWithNativeReport:(const webrtc::StatsReport &)nativeReport {
  if (self = [super init]) {
    _timestamp = nativeReport.timestamp();
    _type = [NSString stringForStdString:nativeReport.TypeToString()];
    _statsId = [NSString stringForStdString:
        nativeReport.id()->ToString()];

    NSUInteger capacity = nativeReport.values().size();
    NSMutableDictionary *values =
        [NSMutableDictionary dictionaryWithCapacity:capacity];
    for (auto const &valuePair : nativeReport.values()) {
      NSString *key = [NSString stringForStdString:
          valuePair.second->display_name()];
      NSString *value = [NSString stringForStdString:
          valuePair.second->ToString()];

      // Not expecting duplicate keys.
      RTC_DCHECK(values[key]);

      values[key] = value;
    }
    _values = values;
  }
  return self;
}

@end
