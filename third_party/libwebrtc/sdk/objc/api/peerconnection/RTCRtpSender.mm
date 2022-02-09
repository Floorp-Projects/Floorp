/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCRtpSender+Private.h"

#import "RTCDtmfSender+Private.h"
#import "RTCMediaStreamTrack+Private.h"
#import "RTCRtpParameters+Private.h"
#import "RTCRtpSender+Native.h"
#import "base/RTCLogging.h"
#import "helpers/NSString+StdString.h"

#include "api/media_stream_interface.h"

@implementation RTC_OBJC_TYPE (RTCRtpSender) {
  RTC_OBJC_TYPE(RTCPeerConnectionFactory) * _factory;
  rtc::scoped_refptr<webrtc::RtpSenderInterface> _nativeRtpSender;
}

@synthesize dtmfSender = _dtmfSender;

- (NSString *)senderId {
  return [NSString stringForStdString:_nativeRtpSender->id()];
}

- (RTC_OBJC_TYPE(RTCRtpParameters) *)parameters {
  return [[RTC_OBJC_TYPE(RTCRtpParameters) alloc]
      initWithNativeParameters:_nativeRtpSender->GetParameters()];
}

- (void)setParameters:(RTC_OBJC_TYPE(RTCRtpParameters) *)parameters {
  if (!_nativeRtpSender->SetParameters(parameters.nativeParameters).ok()) {
    RTCLogError(@"RTC_OBJC_TYPE(RTCRtpSender)(%p): Failed to set parameters: %@", self, parameters);
  }
}

- (RTC_OBJC_TYPE(RTCMediaStreamTrack) *)track {
  rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> nativeTrack(
    _nativeRtpSender->track());
  if (nativeTrack) {
    return [RTC_OBJC_TYPE(RTCMediaStreamTrack) mediaTrackForNativeTrack:nativeTrack
                                                                factory:_factory];
  }
  return nil;
}

- (void)setTrack:(RTC_OBJC_TYPE(RTCMediaStreamTrack) *)track {
  if (!_nativeRtpSender->SetTrack(track.nativeTrack)) {
    RTCLogError(@"RTC_OBJC_TYPE(RTCRtpSender)(%p): Failed to set track %@", self, track);
  }
}

- (NSArray<NSString *> *)streamIds {
  std::vector<std::string> nativeStreamIds = _nativeRtpSender->stream_ids();
  NSMutableArray *streamIds = [NSMutableArray arrayWithCapacity:nativeStreamIds.size()];
  for (const auto &s : nativeStreamIds) {
    [streamIds addObject:[NSString stringForStdString:s]];
  }
  return streamIds;
}

- (void)setStreamIds:(NSArray<NSString *> *)streamIds {
  std::vector<std::string> nativeStreamIds;
  for (NSString *streamId in streamIds) {
    nativeStreamIds.push_back([streamId UTF8String]);
  }
  _nativeRtpSender->SetStreams(nativeStreamIds);
}

- (NSString *)description {
  return [NSString
      stringWithFormat:@"RTC_OBJC_TYPE(RTCRtpSender) {\n  senderId: %@\n}", self.senderId];
}

- (BOOL)isEqual:(id)object {
  if (self == object) {
    return YES;
  }
  if (object == nil) {
    return NO;
  }
  if (![object isMemberOfClass:[self class]]) {
    return NO;
  }
  RTC_OBJC_TYPE(RTCRtpSender) *sender = (RTC_OBJC_TYPE(RTCRtpSender) *)object;
  return _nativeRtpSender == sender.nativeRtpSender;
}

- (NSUInteger)hash {
  return (NSUInteger)_nativeRtpSender.get();
}

#pragma mark - Native

- (void)setFrameEncryptor:(rtc::scoped_refptr<webrtc::FrameEncryptorInterface>)frameEncryptor {
  _nativeRtpSender->SetFrameEncryptor(frameEncryptor);
}

#pragma mark - Private

- (rtc::scoped_refptr<webrtc::RtpSenderInterface>)nativeRtpSender {
  return _nativeRtpSender;
}

- (instancetype)initWithFactory:(RTC_OBJC_TYPE(RTCPeerConnectionFactory) *)factory
                nativeRtpSender:(rtc::scoped_refptr<webrtc::RtpSenderInterface>)nativeRtpSender {
  NSParameterAssert(factory);
  NSParameterAssert(nativeRtpSender);
  if (self = [super init]) {
    _factory = factory;
    _nativeRtpSender = nativeRtpSender;
    rtc::scoped_refptr<webrtc::DtmfSenderInterface> nativeDtmfSender(
        _nativeRtpSender->GetDtmfSender());
    if (nativeDtmfSender) {
      _dtmfSender =
          [[RTC_OBJC_TYPE(RTCDtmfSender) alloc] initWithNativeDtmfSender:nativeDtmfSender];
    }
    RTCLogInfo(@"RTC_OBJC_TYPE(RTCRtpSender)(%p): created sender: %@", self, self.description);
  }
  return self;
}

@end
