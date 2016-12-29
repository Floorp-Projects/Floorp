/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCMediaConstraints.h"

#import "webrtc/api/objc/RTCMediaConstraints+Private.h"
#import "webrtc/base/objc/NSString+StdString.h"

namespace webrtc {

MediaConstraints::~MediaConstraints() {}

MediaConstraints::MediaConstraints() {}

MediaConstraints::MediaConstraints(
    const MediaConstraintsInterface::Constraints& mandatory,
    const MediaConstraintsInterface::Constraints& optional)
    : mandatory_(mandatory), optional_(optional) {}

const MediaConstraintsInterface::Constraints&
MediaConstraints::GetMandatory() const {
  return mandatory_;
}

const MediaConstraintsInterface::Constraints&
MediaConstraints::GetOptional() const {
  return optional_;
}

}  // namespace webrtc


@implementation RTCMediaConstraints {
  NSDictionary<NSString *, NSString *> *_mandatory;
  NSDictionary<NSString *, NSString *> *_optional;
}

- (instancetype)initWithMandatoryConstraints:
    (NSDictionary<NSString *, NSString *> *)mandatory
                         optionalConstraints:
    (NSDictionary<NSString *, NSString *> *)optional {
  if (self = [super init]) {
    _mandatory = [[NSDictionary alloc] initWithDictionary:mandatory
                                                copyItems:YES];
    _optional = [[NSDictionary alloc] initWithDictionary:optional
                                               copyItems:YES];
  }
  return self;
}

- (NSString *)description {
  return [NSString stringWithFormat:@"RTCMediaConstraints:\n%@\n%@",
                                    _mandatory,
                                    _optional];
}

#pragma mark - Private

- (rtc::scoped_ptr<webrtc::MediaConstraints>)nativeConstraints {
  webrtc::MediaConstraintsInterface::Constraints mandatory =
      [[self class] nativeConstraintsForConstraints:_mandatory];
  webrtc::MediaConstraintsInterface::Constraints optional =
      [[self class] nativeConstraintsForConstraints:_optional];

  webrtc::MediaConstraints *nativeConstraints =
      new webrtc::MediaConstraints(mandatory, optional);
  return rtc::scoped_ptr<webrtc::MediaConstraints>(nativeConstraints);
}

+ (webrtc::MediaConstraintsInterface::Constraints)
    nativeConstraintsForConstraints:
        (NSDictionary<NSString *, NSString *> *)constraints {
  webrtc::MediaConstraintsInterface::Constraints nativeConstraints;
  for (NSString *key in constraints) {
    NSAssert([key isKindOfClass:[NSString class]],
             @"%@ is not an NSString.", key);
    NSAssert([constraints[key] isKindOfClass:[NSString class]],
             @"%@ is not an NSString.", constraints[key]);
    nativeConstraints.push_back(webrtc::MediaConstraintsInterface::Constraint(
        key.stdString, constraints[key].stdString));
  }
  return nativeConstraints;
}

@end
