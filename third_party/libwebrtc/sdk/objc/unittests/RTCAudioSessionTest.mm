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

#include <vector>

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


@interface RTCAudioSessionTest : NSObject

- (void)testLockForConfiguration;

@end

@implementation RTCAudioSessionTest

- (void)testLockForConfiguration {
  RTC_OBJC_TYPE(RTCAudioSession) *session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];

  for (size_t i = 0; i < 2; i++) {
    [session lockForConfiguration];
    EXPECT_TRUE(session.isLocked);
  }
  for (size_t i = 0; i < 2; i++) {
    EXPECT_TRUE(session.isLocked);
    [session unlockForConfiguration];
  }
  EXPECT_FALSE(session.isLocked);
}

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

// Hack - fixes OCMVerify link error
// Link error is: Undefined symbols for architecture i386:
// "OCMMakeLocation(objc_object*, char const*, int)", referenced from:
// -[RTCAudioSessionTest testConfigureWebRTCSession] in RTCAudioSessionTest.o
// ld: symbol(s) not found for architecture i386
// REASON: https://github.com/erikdoe/ocmock/issues/238
OCMLocation *OCMMakeLocation(id testCase, const char *fileCString, int line){
  return [OCMLocation locationWithTestCase:testCase
                                      file:[NSString stringWithUTF8String:fileCString]
                                      line:line];
}

- (void)testConfigureWebRTCSession {
  NSError *error = nil;

  void (^setActiveBlock)(NSInvocation *invocation) = ^(NSInvocation *invocation) {
    __autoreleasing NSError **retError;
    [invocation getArgument:&retError atIndex:4];
    *retError = [NSError errorWithDomain:@"AVAudioSession"
                                    code:AVAudioSessionErrorInsufficientPriority
                                userInfo:nil];
    BOOL failure = NO;
    [invocation setReturnValue:&failure];
  };

  id mockAVAudioSession = OCMPartialMock([AVAudioSession sharedInstance]);
  OCMStub([[mockAVAudioSession ignoringNonObjectArgs]
      setActive:YES withOptions:0 error:((NSError __autoreleasing **)[OCMArg anyPointer])]).
      andDo(setActiveBlock);

  id mockAudioSession = OCMPartialMock([RTC_OBJC_TYPE(RTCAudioSession) sharedInstance]);
  OCMStub([mockAudioSession session]).andReturn(mockAVAudioSession);

  RTC_OBJC_TYPE(RTCAudioSession) *audioSession = mockAudioSession;
  EXPECT_EQ(0, audioSession.activationCount);
  [audioSession lockForConfiguration];
  EXPECT_TRUE([audioSession checkLock:nil]);
  // configureWebRTCSession is forced to fail in the above mock interface,
  // so activationCount should remain 0
  OCMExpect([[mockAVAudioSession ignoringNonObjectArgs]
      setActive:YES withOptions:0 error:((NSError __autoreleasing **)[OCMArg anyPointer])]).
      andDo(setActiveBlock);
  OCMExpect([mockAudioSession session]).andReturn(mockAVAudioSession);
  EXPECT_FALSE([audioSession configureWebRTCSession:&error]);
  EXPECT_EQ(0, audioSession.activationCount);

  id session = audioSession.session;
  EXPECT_EQ(session, mockAVAudioSession);
  EXPECT_EQ(NO, [mockAVAudioSession setActive:YES withOptions:0 error:&error]);
  [audioSession unlockForConfiguration];

  OCMVerify([mockAudioSession session]);
  OCMVerify([[mockAVAudioSession ignoringNonObjectArgs] setActive:YES withOptions:0 error:&error]);
  OCMVerify([[mockAVAudioSession ignoringNonObjectArgs] setActive:NO withOptions:0 error:&error]);

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

namespace webrtc {

class AudioSessionTest : public ::testing::Test {
 protected:
  void TearDown() override {
    RTC_OBJC_TYPE(RTCAudioSession) *session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
    for (id<RTC_OBJC_TYPE(RTCAudioSessionDelegate)> delegate : session.delegates) {
      [session removeDelegate:delegate];
    }
  }
};

TEST_F(AudioSessionTest, LockForConfiguration) {
  RTCAudioSessionTest *test = [[RTCAudioSessionTest alloc] init];
  [test testLockForConfiguration];
}

TEST_F(AudioSessionTest, AddAndRemoveDelegates) {
  RTCAudioSessionTest *test = [[RTCAudioSessionTest alloc] init];
  [test testAddAndRemoveDelegates];
}

TEST_F(AudioSessionTest, PushDelegate) {
  RTCAudioSessionTest *test = [[RTCAudioSessionTest alloc] init];
  [test testPushDelegate];
}

TEST_F(AudioSessionTest, ZeroingWeakDelegate) {
  RTCAudioSessionTest *test = [[RTCAudioSessionTest alloc] init];
  [test testZeroingWeakDelegate];
}

TEST_F(AudioSessionTest, RemoveDelegateOnDealloc) {
  RTCAudioSessionTest *test = [[RTCAudioSessionTest alloc] init];
  [test testRemoveDelegateOnDealloc];
}

TEST_F(AudioSessionTest, AudioSessionActivation) {
  RTCAudioSessionTest *test = [[RTCAudioSessionTest alloc] init];
  [test testAudioSessionActivation];
}

TEST_F(AudioSessionTest, ConfigureWebRTCSession) {
  RTCAudioSessionTest *test = [[RTCAudioSessionTest alloc] init];
  [test testConfigureWebRTCSession];
}

TEST_F(AudioSessionTest, AudioVolumeDidNotify) {
  RTCAudioSessionTest *test = [[RTCAudioSessionTest alloc] init];
  [test testAudioVolumeDidNotify];
}

}  // namespace webrtc
