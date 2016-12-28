/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#include <vector>

#include "webrtc/base/gunit.h"

#import "webrtc/api/objc/RTCIceServer.h"
#import "webrtc/api/objc/RTCIceServer+Private.h"

@interface RTCIceServerTest : NSObject
- (void)testOneURLServer;
- (void)testTwoURLServer;
- (void)testPasswordCredential;
@end

@implementation RTCIceServerTest

- (void)testOneURLServer {
  RTCIceServer *server = [[RTCIceServer alloc] initWithURLStrings:@[
      @"stun:stun1.example.net" ]];

  webrtc::PeerConnectionInterface::IceServer iceStruct = server.iceServer;
  EXPECT_EQ((size_t)1, iceStruct.urls.size());
  EXPECT_EQ("stun:stun1.example.net", iceStruct.urls.front());
  EXPECT_EQ("", iceStruct.username);
  EXPECT_EQ("", iceStruct.password);
}

- (void)testTwoURLServer {
  RTCIceServer *server = [[RTCIceServer alloc] initWithURLStrings:@[
      @"turn1:turn1.example.net", @"turn2:turn2.example.net" ]];

  webrtc::PeerConnectionInterface::IceServer iceStruct = server.iceServer;
  EXPECT_EQ((size_t)2, iceStruct.urls.size());
  EXPECT_EQ("turn1:turn1.example.net", iceStruct.urls.front());
  EXPECT_EQ("turn2:turn2.example.net", iceStruct.urls.back());
  EXPECT_EQ("", iceStruct.username);
  EXPECT_EQ("", iceStruct.password);
}

- (void)testPasswordCredential {
  RTCIceServer *server = [[RTCIceServer alloc]
      initWithURLStrings:@[ @"turn1:turn1.example.net" ]
                username:@"username"
              credential:@"credential"];
  webrtc::PeerConnectionInterface::IceServer iceStruct = server.iceServer;
  EXPECT_EQ((size_t)1, iceStruct.urls.size());
  EXPECT_EQ("turn1:turn1.example.net", iceStruct.urls.front());
  EXPECT_EQ("username", iceStruct.username);
  EXPECT_EQ("credential", iceStruct.password);
}

@end

TEST(RTCIceServerTest, OneURLTest) {
  @autoreleasepool {
    RTCIceServerTest *test = [[RTCIceServerTest alloc] init];
    [test testOneURLServer];
  }
}

TEST(RTCIceServerTest, TwoURLTest) {
  @autoreleasepool {
    RTCIceServerTest *test = [[RTCIceServerTest alloc] init];
    [test testTwoURLServer];
  }
}

TEST(RTCIceServerTest, PasswordCredentialTest) {
  @autoreleasepool {
    RTCIceServerTest *test = [[RTCIceServerTest alloc] init];
    [test testPasswordCredential];
  }
}
