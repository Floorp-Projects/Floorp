/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <XCTest/XCTest.h>

#include <stdlib.h>

#include "api/task_queue/default_task_queue_factory.h"

#import "sdk/objc/components/audio/RTCAudioSession+Private.h"
#import "sdk/objc/native/api/audio_device_module.h"
#import "sdk/objc/native/src/audio/audio_device_ios.h"

@interface RTCAudioDeviceTests : XCTestCase {
  bool _testEnabled;
  rtc::scoped_refptr<webrtc::AudioDeviceModule> _audioDeviceModule;
  std::unique_ptr<webrtc::ios_adm::AudioDeviceIOS> _audio_device;
}

@property(nonatomic) RTC_OBJC_TYPE(RTCAudioSession) * audioSession;

@end

@implementation RTCAudioDeviceTests

@synthesize audioSession = _audioSession;

- (void)setUp {
  [super setUp];
#if defined(WEBRTC_IOS) && TARGET_OS_SIMULATOR
  // TODO(peterhanspers): Reenable these tests on simulator.
  // See bugs.webrtc.org/7812
  _testEnabled = false;
  if (::getenv("WEBRTC_IOS_RUN_AUDIO_TESTS") != nullptr) {
    _testEnabled = true;
  }
#else
  _testEnabled = true;
#endif

  _audioDeviceModule = webrtc::CreateAudioDeviceModule();
  _audio_device.reset(new webrtc::ios_adm::AudioDeviceIOS(/*bypass_voice_processing=*/false));
  self.audioSession = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];

  NSError *error = nil;
  [self.audioSession lockForConfiguration];
  [self.audioSession setCategory:AVAudioSessionCategoryPlayAndRecord withOptions:0 error:&error];
  XCTAssertNil(error);

  [self.audioSession setMode:AVAudioSessionModeVoiceChat error:&error];
  XCTAssertNil(error);

  [self.audioSession setActive:YES error:&error];
  XCTAssertNil(error);

  [self.audioSession unlockForConfiguration];
}

- (void)tearDown {
  _audio_device->Terminate();
  _audio_device.reset(nullptr);
  _audioDeviceModule = nullptr;
  [self.audioSession notifyDidEndInterruptionWithShouldResumeSession:NO];

  [super tearDown];
}

// Verifies that the AudioDeviceIOS is_interrupted_ flag is reset correctly
// after an iOS AVAudioSessionInterruptionTypeEnded notification event.
// AudioDeviceIOS listens to RTC_OBJC_TYPE(RTCAudioSession) interrupted notifications by:
// - In AudioDeviceIOS.InitPlayOrRecord registers its audio_session_observer_
//   callback with RTC_OBJC_TYPE(RTCAudioSession)'s delegate list.
// - When RTC_OBJC_TYPE(RTCAudioSession) receives an iOS audio interrupted notification, it
//   passes the notification to callbacks in its delegate list which sets
//   AudioDeviceIOS's is_interrupted_ flag to true.
// - When AudioDeviceIOS.ShutdownPlayOrRecord is called, its
//   audio_session_observer_ callback is removed from RTCAudioSessions's
//   delegate list.
//   So if RTC_OBJC_TYPE(RTCAudioSession) receives an iOS end audio interruption notification,
//   AudioDeviceIOS is not notified as its callback is not in RTC_OBJC_TYPE(RTCAudioSession)'s
//   delegate list. This causes AudioDeviceIOS's is_interrupted_ flag to be in
//   the wrong (true) state and the audio session will ignore audio changes.
// As RTC_OBJC_TYPE(RTCAudioSession) keeps its own interrupted state, the fix is to initialize
// AudioDeviceIOS's is_interrupted_ flag to RTC_OBJC_TYPE(RTCAudioSession)'s isInterrupted
// flag in AudioDeviceIOS.InitPlayOrRecord.
- (void)testInterruptedAudioSession {
  XCTSkipIf(!_testEnabled);
  XCTAssertTrue(self.audioSession.isActive);
  XCTAssertTrue([self.audioSession.category isEqual:AVAudioSessionCategoryPlayAndRecord] ||
                [self.audioSession.category isEqual:AVAudioSessionCategoryPlayback]);
  XCTAssertEqual(AVAudioSessionModeVoiceChat, self.audioSession.mode);

  std::unique_ptr<webrtc::TaskQueueFactory> task_queue_factory =
      webrtc::CreateDefaultTaskQueueFactory();
  std::unique_ptr<webrtc::AudioDeviceBuffer> audio_buffer;
  audio_buffer.reset(new webrtc::AudioDeviceBuffer(task_queue_factory.get()));
  _audio_device->AttachAudioBuffer(audio_buffer.get());
  XCTAssertEqual(webrtc::AudioDeviceGeneric::InitStatus::OK, _audio_device->Init());
  XCTAssertEqual(0, _audio_device->InitPlayout());
  XCTAssertEqual(0, _audio_device->StartPlayout());

  // Force interruption.
  [self.audioSession notifyDidBeginInterruption];

  // Wait for notification to propagate.
  rtc::ThreadManager::ProcessAllMessageQueuesForTesting();
  XCTAssertTrue(_audio_device->IsInterrupted());

  // Force it for testing.
  _audio_device->StopPlayout();

  [self.audioSession notifyDidEndInterruptionWithShouldResumeSession:YES];
  // Wait for notification to propagate.
  rtc::ThreadManager::ProcessAllMessageQueuesForTesting();
  XCTAssertTrue(_audio_device->IsInterrupted());

  _audio_device->Init();
  _audio_device->InitPlayout();
  XCTAssertFalse(_audio_device->IsInterrupted());
}

@end
