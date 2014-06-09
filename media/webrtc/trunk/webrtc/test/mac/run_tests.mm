/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Cocoa/Cocoa.h>

#include "testing/gtest/include/gtest/gtest.h"

@interface TestRunner : NSObject {
  BOOL running_;
  int testResult_;
}
- (void)runAllTests:(NSObject *)ignored;
- (BOOL)running;
- (int)result;
@end

@implementation TestRunner
- (id)init {
  self = [super init];
  if (self) {
    running_ = YES;
  }
  return self;
}

- (void)runAllTests:(NSObject *)ignored {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  testResult_  = RUN_ALL_TESTS();
  running_ = NO;
  [pool release];
}

- (BOOL)running {
  return running_;
}

- (int)result {
  return testResult_;
}
@end

namespace webrtc {
namespace test {

int RunAllTests() {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  [NSApplication sharedApplication];

  TestRunner *testRunner = [[TestRunner alloc] init];
  [NSThread detachNewThreadSelector:@selector(runAllTests:)
                           toTarget:testRunner
                         withObject:nil];

  NSRunLoop *runLoop = [NSRunLoop currentRunLoop];
  while ([testRunner running] &&
         [runLoop runMode:NSDefaultRunLoopMode
               beforeDate:[NSDate distantFuture]]);

  int result = [testRunner result];
  [testRunner release];
  [pool release];
  return result;
}

}  // namespace test
}  // namespace webrtc
