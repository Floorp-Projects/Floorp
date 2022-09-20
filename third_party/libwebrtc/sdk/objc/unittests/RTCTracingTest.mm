/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#include <vector>

#include "rtc_base/gunit.h"

#import "api/peerconnection/RTCTracing.h"
#import "helpers/NSString+RTCStdString.h"

@interface RTCTracingTests : XCTestCase
@end

@implementation RTCTracingTests

- (NSString *)documentsFilePathForFileName:(NSString *)fileName {
  NSParameterAssert(fileName.length);
  NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
  NSString *documentsDirPath = paths.firstObject;
  NSString *filePath =
  [documentsDirPath stringByAppendingPathComponent:fileName];
  return filePath;
}

- (void)testTracingTestNoInitialization {
  NSString *filePath = [self documentsFilePathForFileName:@"webrtc-trace.txt"];
  EXPECT_EQ(NO, RTCStartInternalCapture(filePath));
  RTCStopInternalCapture();
}

@end
