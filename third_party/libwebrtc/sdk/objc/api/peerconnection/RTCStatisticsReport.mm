/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCStatisticsReport+Private.h"

#include "helpers/NSString+StdString.h"
#include "rtc_base/checks.h"

namespace webrtc {

/** Converts a single value to a suitable NSNumber, NSString or NSArray containing NSNumbers
    or NSStrings, or NSDictionary of NSString keys to NSNumber values.*/
NSObject *ValueFromStatsAttribute(const Attribute &attribute) {
  if (!attribute.has_value()) {
    return nil;
  }
  if (attribute.holds_alternative<bool>()) {
    return [NSNumber numberWithBool:attribute.get<bool>()];
  } else if (attribute.holds_alternative<int32_t>()) {
    return [NSNumber numberWithInt:attribute.get<int32_t>()];
  } else if (attribute.holds_alternative<uint32_t>()) {
    return [NSNumber numberWithUnsignedInt:attribute.get<uint32_t>()];
  } else if (attribute.holds_alternative<int64_t>()) {
    return [NSNumber numberWithLong:attribute.get<int64_t>()];
  } else if (attribute.holds_alternative<uint64_t>()) {
    return [NSNumber numberWithUnsignedLong:attribute.get<uint64_t>()];
  } else if (attribute.holds_alternative<double>()) {
    return [NSNumber numberWithDouble:attribute.get<double>()];
  } else if (attribute.holds_alternative<std::string>()) {
    return [NSString stringForStdString:attribute.get<std::string>()];
  } else if (attribute.holds_alternative<std::vector<bool>>()) {
    std::vector<bool> sequence = attribute.get<std::vector<bool>>();
    NSMutableArray *array = [NSMutableArray arrayWithCapacity:sequence.size()];
    for (auto item : sequence) {
      [array addObject:[NSNumber numberWithBool:item]];
    }
    return [array copy];
  } else if (attribute.holds_alternative<std::vector<int32_t>>()) {
    std::vector<int32_t> sequence = attribute.get<std::vector<int32_t>>();
    NSMutableArray<NSNumber *> *array = [NSMutableArray arrayWithCapacity:sequence.size()];
    for (const auto &item : sequence) {
      [array addObject:[NSNumber numberWithInt:item]];
    }
    return [array copy];
  } else if (attribute.holds_alternative<std::vector<uint32_t>>()) {
    std::vector<uint32_t> sequence = attribute.get<std::vector<uint32_t>>();
    NSMutableArray<NSNumber *> *array = [NSMutableArray arrayWithCapacity:sequence.size()];
    for (const auto &item : sequence) {
      [array addObject:[NSNumber numberWithUnsignedInt:item]];
    }
    return [array copy];
  } else if (attribute.holds_alternative<std::vector<int64_t>>()) {
    std::vector<int64_t> sequence = attribute.get<std::vector<int64_t>>();
    NSMutableArray<NSNumber *> *array = [NSMutableArray arrayWithCapacity:sequence.size()];
    for (const auto &item : sequence) {
      [array addObject:[NSNumber numberWithLong:item]];
    }
    return [array copy];
  } else if (attribute.holds_alternative<std::vector<uint64_t>>()) {
    std::vector<uint64_t> sequence = attribute.get<std::vector<uint64_t>>();
    NSMutableArray<NSNumber *> *array = [NSMutableArray arrayWithCapacity:sequence.size()];
    for (const auto &item : sequence) {
      [array addObject:[NSNumber numberWithUnsignedLong:item]];
    }
    return [array copy];
  } else if (attribute.holds_alternative<std::vector<double>>()) {
    std::vector<double> sequence = attribute.get<std::vector<double>>();
    NSMutableArray<NSNumber *> *array = [NSMutableArray arrayWithCapacity:sequence.size()];
    for (const auto &item : sequence) {
      [array addObject:[NSNumber numberWithDouble:item]];
    }
    return [array copy];
  } else if (attribute.holds_alternative<std::vector<std::string>>()) {
    std::vector<std::string> sequence = attribute.get<std::vector<std::string>>();
    NSMutableArray<NSString *> *array = [NSMutableArray arrayWithCapacity:sequence.size()];
    for (const auto &item : sequence) {
      [array addObject:[NSString stringForStdString:item]];
    }
    return [array copy];
  } else if (attribute.holds_alternative<std::map<std::string, uint64_t>>()) {
    std::map<std::string, uint64_t> map = attribute.get<std::map<std::string, uint64_t>>();
    NSMutableDictionary<NSString *, NSNumber *> *dictionary =
        [NSMutableDictionary dictionaryWithCapacity:map.size()];
    for (const auto &item : map) {
      dictionary[[NSString stringForStdString:item.first]] = @(item.second);
    }
    return [dictionary copy];
  } else if (attribute.holds_alternative<std::map<std::string, double>>()) {
    std::map<std::string, double> map = attribute.get<std::map<std::string, double>>();
    NSMutableDictionary<NSString *, NSNumber *> *dictionary =
        [NSMutableDictionary dictionaryWithCapacity:map.size()];
    for (const auto &item : map) {
      dictionary[[NSString stringForStdString:item.first]] = @(item.second);
    }
    return [dictionary copy];
  }
  RTC_DCHECK_NOTREACHED();
  return nil;
}
}  // namespace webrtc

@implementation RTC_OBJC_TYPE (RTCStatistics)

@synthesize id = _id;
@synthesize timestamp_us = _timestamp_us;
@synthesize type = _type;
@synthesize values = _values;

- (instancetype)initWithStatistics:(const webrtc::RTCStats &)statistics {
  if (self = [super init]) {
    _id = [NSString stringForStdString:statistics.id()];
    _timestamp_us = statistics.timestamp().us();
    _type = [NSString stringWithCString:statistics.type() encoding:NSUTF8StringEncoding];

    NSMutableDictionary<NSString *, NSObject *> *values = [NSMutableDictionary dictionary];
    for (const auto &attribute : statistics.Attributes()) {
      NSObject *value = ValueFromStatsAttribute(attribute);
      if (value) {
        NSString *name = [NSString stringWithCString:attribute.name()
                                            encoding:NSUTF8StringEncoding];
        RTC_DCHECK(name.length > 0);
        RTC_DCHECK(!values[name]);
        values[name] = value;
      }
    }
    _values = [values copy];
  }

  return self;
}

- (NSString *)description {
  return [NSString stringWithFormat:@"id = %@, type = %@, timestamp = %.0f, values = %@",
                                    self.id,
                                    self.type,
                                    self.timestamp_us,
                                    self.values];
}

@end

@implementation RTC_OBJC_TYPE (RTCStatisticsReport)

@synthesize timestamp_us = _timestamp_us;
@synthesize statistics = _statistics;

- (NSString *)description {
  return [NSString
      stringWithFormat:@"timestamp = %.0f, statistics = %@", self.timestamp_us, self.statistics];
}

@end

@implementation RTC_OBJC_TYPE (RTCStatisticsReport) (Private)

- (instancetype)initWithReport : (const webrtc::RTCStatsReport &)report {
  if (self = [super init]) {
    _timestamp_us = report.timestamp().us();

    NSMutableDictionary *statisticsById =
        [NSMutableDictionary dictionaryWithCapacity:report.size()];
    for (const auto &stat : report) {
      RTC_OBJC_TYPE(RTCStatistics) *statistics =
          [[RTC_OBJC_TYPE(RTCStatistics) alloc] initWithStatistics:stat];
      statisticsById[statistics.id] = statistics;
    }
    _statistics = [statisticsById copy];
  }

  return self;
}

@end
