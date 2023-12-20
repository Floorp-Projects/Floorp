/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>
#import <OCMock/OCMock.h>
#import <XCTest/XCTest.h>

#include <vector>

#include "rtc_base/event.h"
#include "rtc_base/gunit.h"

#import "components/audio/RTCAudioSession+Private.h"

#import "components/audio/RTCAudioSession.h"
#import "components/audio/RTCAudioSessionConfiguration.h"

@interface RTC_OBJC_TYPE (RTCAudioSession)
(UnitTesting)

    @property(nonatomic,
              readonly) std::vector<__weak id<RTC_OBJC_TYPE(RTCAudioSessionDelegate)> > delegates;

- (instancetype)initWithAudioSession:(id)audioSession;

@end

@interface MockAVAudioSession : NSObject

@property (nonatomic, readwrite, assign) float outputVolume;

@end

@implementation MockAVAudioSession
@synthesize outputVolume = _outputVolume;
@end

@interface RTCAudioSessionTestDelegate : NSObject <RTC_OBJC_TYPE (RTCAudioSessionDelegate)>

@property (nonatomic, readonly) float outputVolume;

@end

@implementation RTCAudioSessionTestDelegate

@synthesize outputVolume = _outputVolume;

- (instancetype)init {
  if (self = [super init]) {
    _outputVolume = -1;
  }
  return self;
}

- (void)audioSessionDidBeginInterruption:(RTC_OBJC_TYPE(RTCAudioSession) *)session {
}

- (void)audioSessionDidEndInterruption:(RTC_OBJC_TYPE(RTCAudioSession) *)session
                   shouldResumeSession:(BOOL)shouldResumeSession {
}

- (void)audioSessionDidChangeRoute:(RTC_OBJC_TYPE(RTCAudioSession) *)session
                            reason:(AVAudioSessionRouteChangeReason)reason
                     previousRoute:(AVAudioSessionRouteDescription *)previousRoute {
}

- (void)audioSessionMediaServerTerminated:(RTC_OBJC_TYPE(RTCAudioSession) *)session {
}

- (void)audioSessionMediaServerReset:(RTC_OBJC_TYPE(RTCAudioSession) *)session {
}

- (void)audioSessionShouldConfigure:(RTC_OBJC_TYPE(RTCAudioSession) *)session {
}

- (void)audioSessionShouldUnconfigure:(RTC_OBJC_TYPE(RTCAudioSession) *)session {
}

- (void)audioSession:(RTC_OBJC_TYPE(RTCAudioSession) *)audioSession
    didChangeOutputVolume:(float)outputVolume {
  _outputVolume = outputVolume;
}

@end

// A delegate that adds itself to the audio session on init and removes itself
// in its dealloc.
@interface RTCTestRemoveOnDeallocDelegate : RTCAudioSessionTestDelegate
@end

@implementation RTCTestRemoveOnDeallocDelegate

- (instancetype)init {
  if (self = [super init]) {
    RTC_OBJC_TYPE(RTCAudioSession) *session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
    [session addDelegate:self];
  }
  return self;
}

- (void)dealloc {
  RTC_OBJC_TYPE(RTCAudioSession) *session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
  [session removeDelegate:self];
}

@end

@interface RTCAudioSessionTest : XCTestCase

@end

@implementation RTCAudioSessionTest

- (void)testAddAndRemoveDelegates {
  RTC_OBJC_TYPE(RTCAudioSession) *session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
  NSMutableArray *delegates = [NSMutableArray array];
  const size_t count = 5;
  for (size_t i = 0; i < count; ++i) {
    RTCAudioSessionTestDelegate *delegate =
        [[RTCAudioSessionTestDelegate alloc] init];
    [session addDelegate:delegate];
    [delegates addObject:delegate];
    EXPECT_EQ(i + 1, session.delegates.size());
  }
  [delegates enumerateObjectsUsingBlock:^(RTCAudioSessionTestDelegate *obj,
                                          NSUInteger idx,
                                          BOOL *stop) {
    [session removeDelegate:obj];
  }];
  EXPECT_EQ(0u, session.delegates.size());
}

- (void)testPushDelegate {
  RTC_OBJC_TYPE(RTCAudioSession) *session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
  NSMutableArray *delegates = [NSMutableArray array];
  const size_t count = 2;
  for (size_t i = 0; i < count; ++i) {
    RTCAudioSessionTestDelegate *delegate =
        [[RTCAudioSessionTestDelegate alloc] init];
    [session addDelegate:delegate];
    [delegates addObject:delegate];
  }
  // Test that it gets added to the front of the list.
  RTCAudioSessionTestDelegate *pushedDelegate =
      [[RTCAudioSessionTestDelegate alloc] init];
  [session pushDelegate:pushedDelegate];
  EXPECT_TRUE(pushedDelegate == session.delegates[0]);

  // Test that it stays at the front of the list.
  for (size_t i = 0; i < count; ++i) {
    RTCAudioSessionTestDelegate *delegate =
        [[RTCAudioSessionTestDelegate alloc] init];
    [session addDelegate:delegate];
    [delegates addObject:delegate];
  }
  EXPECT_TRUE(pushedDelegate == session.delegates[0]);

  // Test that the next one goes to the front too.
  pushedDelegate = [[RTCAudioSessionTestDelegate alloc] init];
  [session pushDelegate:pushedDelegate];
  EXPECT_TRUE(pushedDelegate == session.delegates[0]);
}

// Tests that delegates added to the audio session properly zero out. This is
// checking an implementation detail (that vectors of __weak work as expected).
- (void)testZeroingWeakDelegate {
  RTC_OBJC_TYPE(RTCAudioSession) *session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
  @autoreleasepool {
    // Add a delegate to the session. There should be one delegate at this
    // point.
    RTCAudioSessionTestDelegate *delegate =
        [[RTCAudioSessionTestDelegate alloc] init];
    [session addDelegate:delegate];
    EXPECT_EQ(1u, session.delegates.size());
    EXPECT_TRUE(session.delegates[0]);
  }
  // The previously created delegate should've de-alloced, leaving a nil ptr.
  EXPECT_FALSE(session.delegates[0]);
  RTCAudioSessionTestDelegate *delegate =
      [[RTCAudioSessionTestDelegate alloc] init];
  [session addDelegate:delegate];
  // On adding a new delegate, nil ptrs should've been cleared.
  EXPECT_EQ(1u, session.delegates.size());
  EXPECT_TRUE(session.delegates[0]);
}

// Tests that we don't crash when removing delegates in dealloc.
// Added as a regression test.
- (void)testRemoveDelegateOnDealloc {
  @autoreleasepool {
    RTCTestRemoveOnDeallocDelegate *delegate =
        [[RTCTestRemoveOnDeallocDelegate alloc] init];
    EXPECT_TRUE(delegate);
  }
  RTC_OBJC_TYPE(RTCAudioSession) *session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
  EXPECT_EQ(0u, session.delegates.size());
}

- (void)testAudioSessionActivation {
  RTC_OBJC_TYPE(RTCAudioSession) *audioSession = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
  EXPECT_EQ(0, audioSession.activationCount);
  [audioSession audioSessionDidActivate:[AVAudioSession sharedInstance]];
  EXPECT_EQ(1, audioSession.activationCount);
  [audioSession audioSessionDidDeactivate:[AVAudioSession sharedInstance]];
  EXPECT_EQ(0, audioSession.activationCount);
}

// TODO(b/298960678): Fix crash when running the test on simulators.
- (void)DISABLED_testConfigureWebRTCSession {
  NSError *error = nil;

  void (^setActiveBlock)(NSInvocation *invocation) = ^(NSInvocation *invocation) {
    __autoreleasing NSError **retError;
    [invocation getArgument:&retError atIndex:4];
    *retError = [NSError errorWithDomain:@"AVAudioSession"
                                    code:AVAudioSessionErrorCodeCannotInterruptOthers
                                userInfo:nil];
    BOOL failure = NO;
    [invocation setReturnValue:&failure];
  };

  id mockAVAudioSession = OCMPartialMock([AVAudioSession sharedInstance]);
  OCMStub([[mockAVAudioSession ignoringNonObjectArgs] setActive:YES
                                                    withOptions:0
                                                          error:([OCMArg anyObjectRef])])
      .andDo(setActiveBlock);

  id mockAudioSession = OCMPartialMock([RTC_OBJC_TYPE(RTCAudioSession) sharedInstance]);
  OCMStub([mockAudioSession session]).andReturn(mockAVAudioSession);

  RTC_OBJC_TYPE(RTCAudioSession) *audioSession = mockAudioSession;
  EXPECT_EQ(0, audioSession.activationCount);
  [audioSession lockForConfiguration];
  // configureWebRTCSession is forced to fail in the above mock interface,
  // so activationCount should remain 0
  OCMExpect([[mockAVAudioSession ignoringNonObjectArgs] setActive:YES
                                                      withOptions:0
                                                            error:([OCMArg anyObjectRef])])
      .andDo(setActiveBlock);
  OCMExpect([mockAudioSession session]).andReturn(mockAVAudioSession);
  EXPECT_FALSE([audioSession configureWebRTCSession:&error]);
  EXPECT_EQ(0, audioSession.activationCount);

  id session = audioSession.session;
  EXPECT_EQ(session, mockAVAudioSession);
  EXPECT_EQ(NO, [mockAVAudioSession setActive:YES withOptions:0 error:&error]);
  [audioSession unlockForConfiguration];

  // The -Wunused-value is a workaround for https://bugs.llvm.org/show_bug.cgi?id=45245
  _Pragma("clang diagnostic push") _Pragma("clang diagnostic ignored \"-Wunused-value\"");
  OCMVerify([mockAudioSession session]);
  OCMVerify([[mockAVAudioSession ignoringNonObjectArgs] setActive:YES withOptions:0 error:&error]);
  OCMVerify([[mockAVAudioSession ignoringNonObjectArgs] setActive:NO withOptions:0 error:&error]);
  _Pragma("clang diagnostic pop");

  [mockAVAudioSession stopMocking];
  [mockAudioSession stopMocking];
}

// TODO(b/298960678): Fix crash when running the test on simulators.
- (void)DISABLED_testConfigureWebRTCSessionWithoutLocking {
  NSError *error = nil;

  id mockAVAudioSession = OCMPartialMock([AVAudioSession sharedInstance]);
  id mockAudioSession = OCMPartialMock([RTC_OBJC_TYPE(RTCAudioSession) sharedInstance]);
  OCMStub([mockAudioSession session]).andReturn(mockAVAudioSession);

  RTC_OBJC_TYPE(RTCAudioSession) *audioSession = mockAudioSession;

  std::unique_ptr<rtc::Thread> thread = rtc::Thread::Create();
  EXPECT_TRUE(thread);
  EXPECT_TRUE(thread->Start());

  rtc::Event waitLock;
  rtc::Event waitCleanup;
  constexpr webrtc::TimeDelta timeout = webrtc::TimeDelta::Seconds(5);
  thread->PostTask([audioSession, &waitLock, &waitCleanup, timeout] {
    [audioSession lockForConfiguration];
    waitLock.Set();
    waitCleanup.Wait(timeout);
    [audioSession unlockForConfiguration];
  });

  waitLock.Wait(timeout);
  [audioSession setCategory:AVAudioSessionCategoryPlayAndRecord withOptions:0 error:&error];
  EXPECT_TRUE(error != nil);
  EXPECT_EQ(error.domain, kRTCAudioSessionErrorDomain);
  EXPECT_EQ(error.code, kRTCAudioSessionErrorLockRequired);
  waitCleanup.Set();
  thread->Stop();

  [mockAVAudioSession stopMocking];
  [mockAudioSession stopMocking];
}

- (void)testAudioVolumeDidNotify {
  MockAVAudioSession *mockAVAudioSession = [[MockAVAudioSession alloc] init];
  RTC_OBJC_TYPE(RTCAudioSession) *session =
      [[RTC_OBJC_TYPE(RTCAudioSession) alloc] initWithAudioSession:mockAVAudioSession];
  RTCAudioSessionTestDelegate *delegate =
      [[RTCAudioSessionTestDelegate alloc] init];
  [session addDelegate:delegate];

  float expectedVolume = 0.75;
  mockAVAudioSession.outputVolume = expectedVolume;

  EXPECT_EQ(expectedVolume, delegate.outputVolume);
}

@end
