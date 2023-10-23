/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "api/peerconnection/RTCAudioSource.h"
#import "api/peerconnection/RTCConfiguration.h"
#import "api/peerconnection/RTCDataChannel.h"
#import "api/peerconnection/RTCDataChannelConfiguration.h"
#import "api/peerconnection/RTCMediaConstraints.h"
#import "api/peerconnection/RTCMediaStreamTrack.h"
#import "api/peerconnection/RTCPeerConnection.h"
#import "api/peerconnection/RTCPeerConnectionFactory.h"
#import "api/peerconnection/RTCRtpReceiver.h"
#import "api/peerconnection/RTCRtpSender.h"
#import "api/peerconnection/RTCRtpTransceiver.h"
#import "api/peerconnection/RTCSessionDescription.h"
#import "api/peerconnection/RTCVideoSource.h"
#import "rtc_base/system/unused.h"

#import <XCTest/XCTest.h>

@interface RTCPeerConnectionFactoryTests : XCTestCase
@end

@implementation RTCPeerConnectionFactoryTests

- (void)testPeerConnectionLifetime {
  @autoreleasepool {
    RTC_OBJC_TYPE(RTCConfiguration) *config = [[RTC_OBJC_TYPE(RTCConfiguration) alloc] init];

    RTC_OBJC_TYPE(RTCMediaConstraints) *constraints =
        [[RTC_OBJC_TYPE(RTCMediaConstraints) alloc] initWithMandatoryConstraints:@{}
                                                             optionalConstraints:nil];

    RTC_OBJC_TYPE(RTCPeerConnectionFactory) * factory;
    RTC_OBJC_TYPE(RTCPeerConnection) * peerConnection;

    @autoreleasepool {
      factory = [[RTC_OBJC_TYPE(RTCPeerConnectionFactory) alloc] init];
      peerConnection =
          [factory peerConnectionWithConfiguration:config constraints:constraints delegate:nil];
      [peerConnection close];
      factory = nil;
    }
    peerConnection = nil;
  }

  XCTAssertTrue(true, @"Expect test does not crash");
}

- (void)testMediaStreamLifetime {
  @autoreleasepool {
    RTC_OBJC_TYPE(RTCPeerConnectionFactory) * factory;
    RTC_OBJC_TYPE(RTCMediaStream) * mediaStream;

    @autoreleasepool {
      factory = [[RTC_OBJC_TYPE(RTCPeerConnectionFactory) alloc] init];
      mediaStream = [factory mediaStreamWithStreamId:@"mediaStream"];
      factory = nil;
    }
    mediaStream = nil;
    RTC_UNUSED(mediaStream);
  }

  XCTAssertTrue(true, "Expect test does not crash");
}

- (void)testDataChannelLifetime {
  @autoreleasepool {
    RTC_OBJC_TYPE(RTCConfiguration) *config = [[RTC_OBJC_TYPE(RTCConfiguration) alloc] init];
    RTC_OBJC_TYPE(RTCMediaConstraints) *constraints =
        [[RTC_OBJC_TYPE(RTCMediaConstraints) alloc] initWithMandatoryConstraints:@{}
                                                             optionalConstraints:nil];
    RTC_OBJC_TYPE(RTCDataChannelConfiguration) *dataChannelConfig =
        [[RTC_OBJC_TYPE(RTCDataChannelConfiguration) alloc] init];

    RTC_OBJC_TYPE(RTCPeerConnectionFactory) * factory;
    RTC_OBJC_TYPE(RTCPeerConnection) * peerConnection;
    RTC_OBJC_TYPE(RTCDataChannel) * dataChannel;

    @autoreleasepool {
      factory = [[RTC_OBJC_TYPE(RTCPeerConnectionFactory) alloc] init];
      peerConnection =
          [factory peerConnectionWithConfiguration:config constraints:constraints delegate:nil];
      dataChannel =
          [peerConnection dataChannelForLabel:@"test_channel" configuration:dataChannelConfig];
      XCTAssertNotNil(dataChannel);
      [peerConnection close];
      peerConnection = nil;
      factory = nil;
    }
    dataChannel = nil;
  }

  XCTAssertTrue(true, "Expect test does not crash");
}

- (void)testRTCRtpTransceiverLifetime {
  @autoreleasepool {
    RTC_OBJC_TYPE(RTCConfiguration) *config = [[RTC_OBJC_TYPE(RTCConfiguration) alloc] init];
    config.sdpSemantics = RTCSdpSemanticsUnifiedPlan;
    RTC_OBJC_TYPE(RTCMediaConstraints) *contraints =
        [[RTC_OBJC_TYPE(RTCMediaConstraints) alloc] initWithMandatoryConstraints:@{}
                                                             optionalConstraints:nil];
    RTC_OBJC_TYPE(RTCRtpTransceiverInit) *init =
        [[RTC_OBJC_TYPE(RTCRtpTransceiverInit) alloc] init];

    RTC_OBJC_TYPE(RTCPeerConnectionFactory) * factory;
    RTC_OBJC_TYPE(RTCPeerConnection) * peerConnection;
    RTC_OBJC_TYPE(RTCRtpTransceiver) * tranceiver;

    @autoreleasepool {
      factory = [[RTC_OBJC_TYPE(RTCPeerConnectionFactory) alloc] init];
      peerConnection =
          [factory peerConnectionWithConfiguration:config constraints:contraints delegate:nil];
      tranceiver = [peerConnection addTransceiverOfType:RTCRtpMediaTypeAudio init:init];
      XCTAssertNotNil(tranceiver);
      [peerConnection close];
      peerConnection = nil;
      factory = nil;
    }
    tranceiver = nil;
  }

  XCTAssertTrue(true, "Expect test does not crash");
}

- (void)testRTCRtpSenderLifetime {
  @autoreleasepool {
    RTC_OBJC_TYPE(RTCConfiguration) *config = [[RTC_OBJC_TYPE(RTCConfiguration) alloc] init];
    config.sdpSemantics = RTCSdpSemanticsPlanB;
    RTC_OBJC_TYPE(RTCMediaConstraints) *constraints =
        [[RTC_OBJC_TYPE(RTCMediaConstraints) alloc] initWithMandatoryConstraints:@{}
                                                             optionalConstraints:nil];

    RTC_OBJC_TYPE(RTCPeerConnectionFactory) * factory;
    RTC_OBJC_TYPE(RTCPeerConnection) * peerConnection;
    RTC_OBJC_TYPE(RTCRtpSender) * sender;

    @autoreleasepool {
      factory = [[RTC_OBJC_TYPE(RTCPeerConnectionFactory) alloc] init];
      peerConnection =
          [factory peerConnectionWithConfiguration:config constraints:constraints delegate:nil];
      sender = [peerConnection senderWithKind:kRTCMediaStreamTrackKindVideo streamId:@"stream"];
      XCTAssertNotNil(sender);
      [peerConnection close];
      peerConnection = nil;
      factory = nil;
    }
    sender = nil;
  }

  XCTAssertTrue(true, "Expect test does not crash");
}

- (void)testRTCRtpReceiverLifetime {
  @autoreleasepool {
    RTC_OBJC_TYPE(RTCConfiguration) *config = [[RTC_OBJC_TYPE(RTCConfiguration) alloc] init];
    config.sdpSemantics = RTCSdpSemanticsPlanB;
    RTC_OBJC_TYPE(RTCMediaConstraints) *constraints =
        [[RTC_OBJC_TYPE(RTCMediaConstraints) alloc] initWithMandatoryConstraints:@{}
                                                             optionalConstraints:nil];

    RTC_OBJC_TYPE(RTCPeerConnectionFactory) * factory;
    RTC_OBJC_TYPE(RTCPeerConnection) * pc1;
    RTC_OBJC_TYPE(RTCPeerConnection) * pc2;

    NSArray<RTC_OBJC_TYPE(RTCRtpReceiver) *> *receivers1;
    NSArray<RTC_OBJC_TYPE(RTCRtpReceiver) *> *receivers2;

    @autoreleasepool {
      factory = [[RTC_OBJC_TYPE(RTCPeerConnectionFactory) alloc] init];
      pc1 = [factory peerConnectionWithConfiguration:config constraints:constraints delegate:nil];
      [pc1 senderWithKind:kRTCMediaStreamTrackKindAudio streamId:@"stream"];

      pc2 = [factory peerConnectionWithConfiguration:config constraints:constraints delegate:nil];
      [pc2 senderWithKind:kRTCMediaStreamTrackKindAudio streamId:@"stream"];

      NSTimeInterval negotiationTimeout = 15;
      XCTAssertTrue([self negotiatePeerConnection:pc1
                               withPeerConnection:pc2
                               negotiationTimeout:negotiationTimeout]);

      XCTAssertEqual(pc1.signalingState, RTCSignalingStateStable);
      XCTAssertEqual(pc2.signalingState, RTCSignalingStateStable);

      receivers1 = pc1.receivers;
      receivers2 = pc2.receivers;
      XCTAssertTrue(receivers1.count > 0);
      XCTAssertTrue(receivers2.count > 0);
      [pc1 close];
      [pc2 close];
      pc1 = nil;
      pc2 = nil;
      factory = nil;
    }
    receivers1 = nil;
    receivers2 = nil;
  }

  XCTAssertTrue(true, "Expect test does not crash");
}

- (void)testAudioSourceLifetime {
  @autoreleasepool {
    RTC_OBJC_TYPE(RTCPeerConnectionFactory) * factory;
    RTC_OBJC_TYPE(RTCAudioSource) * audioSource;

    @autoreleasepool {
      factory = [[RTC_OBJC_TYPE(RTCPeerConnectionFactory) alloc] init];
      audioSource = [factory audioSourceWithConstraints:nil];
      XCTAssertNotNil(audioSource);
      factory = nil;
    }
    audioSource = nil;
  }

  XCTAssertTrue(true, "Expect test does not crash");
}

- (void)testVideoSourceLifetime {
  @autoreleasepool {
    RTC_OBJC_TYPE(RTCPeerConnectionFactory) * factory;
    RTC_OBJC_TYPE(RTCVideoSource) * videoSource;

    @autoreleasepool {
      factory = [[RTC_OBJC_TYPE(RTCPeerConnectionFactory) alloc] init];
      videoSource = [factory videoSource];
      XCTAssertNotNil(videoSource);
      factory = nil;
    }
    videoSource = nil;
  }

  XCTAssertTrue(true, "Expect test does not crash");
}

- (void)testAudioTrackLifetime {
  @autoreleasepool {
    RTC_OBJC_TYPE(RTCPeerConnectionFactory) * factory;
    RTC_OBJC_TYPE(RTCAudioTrack) * audioTrack;

    @autoreleasepool {
      factory = [[RTC_OBJC_TYPE(RTCPeerConnectionFactory) alloc] init];
      audioTrack = [factory audioTrackWithTrackId:@"audioTrack"];
      XCTAssertNotNil(audioTrack);
      factory = nil;
    }
    audioTrack = nil;
  }

  XCTAssertTrue(true, "Expect test does not crash");
}

- (void)testVideoTrackLifetime {
  @autoreleasepool {
    RTC_OBJC_TYPE(RTCPeerConnectionFactory) * factory;
    RTC_OBJC_TYPE(RTCVideoTrack) * videoTrack;

    @autoreleasepool {
      factory = [[RTC_OBJC_TYPE(RTCPeerConnectionFactory) alloc] init];
      videoTrack = [factory videoTrackWithSource:[factory videoSource] trackId:@"videoTrack"];
      XCTAssertNotNil(videoTrack);
      factory = nil;
    }
    videoTrack = nil;
  }

  XCTAssertTrue(true, "Expect test does not crash");
}

- (void)testRollback {
  @autoreleasepool {
    RTC_OBJC_TYPE(RTCConfiguration) *config = [[RTC_OBJC_TYPE(RTCConfiguration) alloc] init];
    config.sdpSemantics = RTCSdpSemanticsUnifiedPlan;
    RTC_OBJC_TYPE(RTCMediaConstraints) *constraints =
        [[RTC_OBJC_TYPE(RTCMediaConstraints) alloc] initWithMandatoryConstraints:@{
          kRTCMediaConstraintsOfferToReceiveAudio : kRTCMediaConstraintsValueTrue
        }
                                                             optionalConstraints:nil];

    __block RTC_OBJC_TYPE(RTCPeerConnectionFactory) * factory;
    __block RTC_OBJC_TYPE(RTCPeerConnection) * pc1;
    RTC_OBJC_TYPE(RTCSessionDescription) *rollback =
        [[RTC_OBJC_TYPE(RTCSessionDescription) alloc] initWithType:RTCSdpTypeRollback sdp:@""];

    @autoreleasepool {
      factory = [[RTC_OBJC_TYPE(RTCPeerConnectionFactory) alloc] init];
      pc1 = [factory peerConnectionWithConfiguration:config constraints:constraints delegate:nil];
      dispatch_semaphore_t negotiatedSem = dispatch_semaphore_create(0);
      [pc1 offerForConstraints:constraints
             completionHandler:^(RTC_OBJC_TYPE(RTCSessionDescription) * offer, NSError * error) {
               XCTAssertNil(error);
               XCTAssertNotNil(offer);

               __weak RTC_OBJC_TYPE(RTCPeerConnection) *weakPC1 = pc1;
               [pc1 setLocalDescription:offer
                      completionHandler:^(NSError *error) {
                        XCTAssertNil(error);
                        [weakPC1 setLocalDescription:rollback
                                   completionHandler:^(NSError *error) {
                                     XCTAssertNil(error);
                                   }];
                      }];
               NSTimeInterval negotiationTimeout = 15;
               dispatch_semaphore_wait(
                   negotiatedSem,
                   dispatch_time(DISPATCH_TIME_NOW, (int64_t)(negotiationTimeout * NSEC_PER_SEC)));

               XCTAssertEqual(pc1.signalingState, RTCSignalingStateStable);

               [pc1 close];
               pc1 = nil;
               factory = nil;
             }];
    }

    XCTAssertTrue(true, "Expect test does not crash");
  }
}

- (bool)negotiatePeerConnection:(RTC_OBJC_TYPE(RTCPeerConnection) *)pc1
             withPeerConnection:(RTC_OBJC_TYPE(RTCPeerConnection) *)pc2
             negotiationTimeout:(NSTimeInterval)timeout {
  __weak RTC_OBJC_TYPE(RTCPeerConnection) *weakPC1 = pc1;
  __weak RTC_OBJC_TYPE(RTCPeerConnection) *weakPC2 = pc2;
  RTC_OBJC_TYPE(RTCMediaConstraints) *sdpConstraints =
      [[RTC_OBJC_TYPE(RTCMediaConstraints) alloc] initWithMandatoryConstraints:@{
        kRTCMediaConstraintsOfferToReceiveAudio : kRTCMediaConstraintsValueTrue
      }
                                                           optionalConstraints:nil];

  dispatch_semaphore_t negotiatedSem = dispatch_semaphore_create(0);
  [weakPC1 offerForConstraints:sdpConstraints
             completionHandler:^(RTC_OBJC_TYPE(RTCSessionDescription) * offer, NSError * error) {
               XCTAssertNil(error);
               XCTAssertNotNil(offer);
               [weakPC1
                   setLocalDescription:offer
                     completionHandler:^(NSError *error) {
                       XCTAssertNil(error);
                       [weakPC2
                           setRemoteDescription:offer
                              completionHandler:^(NSError *error) {
                                XCTAssertNil(error);
                                [weakPC2
                                    answerForConstraints:sdpConstraints
                                       completionHandler:^(
                                           RTC_OBJC_TYPE(RTCSessionDescription) * answer,
                                           NSError * error) {
                                         XCTAssertNil(error);
                                         XCTAssertNotNil(answer);
                                         [weakPC2
                                             setLocalDescription:answer
                                               completionHandler:^(NSError *error) {
                                                 XCTAssertNil(error);
                                                 [weakPC1
                                                     setRemoteDescription:answer
                                                        completionHandler:^(NSError *error) {
                                                          XCTAssertNil(error);
                                                          dispatch_semaphore_signal(negotiatedSem);
                                                        }];
                                               }];
                                       }];
                              }];
                     }];
             }];

  return 0 ==
      dispatch_semaphore_wait(negotiatedSem,
                              dispatch_time(DISPATCH_TIME_NOW, (int64_t)(timeout * NSEC_PER_SEC)));
}

@end
