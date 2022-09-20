/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCPeerConnection+Private.h"

#import "RTCDataChannel+Private.h"
#import "RTCDataChannelConfiguration+Private.h"
#import "helpers/NSString+RTCStdString.h"

@implementation RTC_OBJC_TYPE (RTCPeerConnection)
(DataChannel)

    - (nullable RTC_OBJC_TYPE(RTCDataChannel) *)dataChannelForLabel
    : (NSString *)label configuration
    : (RTC_OBJC_TYPE(RTCDataChannelConfiguration) *)configuration {
  std::string labelString = [NSString rtc_stdStringForString:label];
  const webrtc::DataChannelInit nativeInit =
      configuration.nativeDataChannelInit;
  auto result = self.nativePeerConnection->CreateDataChannelOrError(labelString, &nativeInit);
  if (!result.ok()) {
    return nil;
  }
  return [[RTC_OBJC_TYPE(RTCDataChannel) alloc] initWithFactory:self.factory
                                              nativeDataChannel:result.MoveValue()];
}

@end
