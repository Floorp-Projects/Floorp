/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCVideoCapturer.h"

@implementation RTC_OBJC_TYPE (RTCVideoCapturer)

@synthesize delegate = _delegate;

- (instancetype)initWithDelegate:(id<RTC_OBJC_TYPE(RTCVideoCapturerDelegate)>)delegate {
  if (self = [super init]) {
    _delegate = delegate;
  }
  return self;
}

@end
