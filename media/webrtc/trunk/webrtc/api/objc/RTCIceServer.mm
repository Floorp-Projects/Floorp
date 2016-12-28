/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCIceServer.h"

#import "webrtc/api/objc/RTCIceServer+Private.h"
#import "webrtc/base/objc/NSString+StdString.h"

@implementation RTCIceServer

@synthesize urlStrings = _urlStrings;
@synthesize username = _username;
@synthesize credential = _credential;

- (instancetype)initWithURLStrings:(NSArray<NSString *> *)urlStrings {
  NSParameterAssert(urlStrings.count);
  return [self initWithURLStrings:urlStrings
                         username:nil
                       credential:nil];
}

- (instancetype)initWithURLStrings:(NSArray<NSString *> *)urlStrings
                          username:(NSString *)username
                        credential:(NSString *)credential {
  NSParameterAssert(urlStrings.count);
  if (self = [super init]) {
    _urlStrings = [[NSArray alloc] initWithArray:urlStrings copyItems:YES];
    _username = [username copy];
    _credential = [credential copy];
  }
  return self;
}

- (NSString *)description {
  return [NSString stringWithFormat:@"RTCIceServer:\n%@\n%@\n%@",
                                    _urlStrings,
                                    _username,
                                    _credential];
}

#pragma mark - Private

- (webrtc::PeerConnectionInterface::IceServer)iceServer {
  __block webrtc::PeerConnectionInterface::IceServer iceServer;

  iceServer.username = [NSString stdStringForString:_username];
  iceServer.password = [NSString stdStringForString:_credential];

  [_urlStrings enumerateObjectsUsingBlock:^(NSString *url,
                                            NSUInteger idx,
                                            BOOL *stop) {
    iceServer.urls.push_back(url.stdString);
  }];
  return iceServer;
}

@end
