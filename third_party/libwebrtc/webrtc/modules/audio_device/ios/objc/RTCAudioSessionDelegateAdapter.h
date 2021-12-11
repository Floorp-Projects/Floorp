/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "sdk/objc/Framework/Headers/WebRTC/RTCAudioSession.h"

namespace webrtc {
class AudioSessionObserver;
}

/** Adapter that forwards RTCAudioSessionDelegate calls to the appropriate
 *  methods on the AudioSessionObserver.
 */
@interface RTCAudioSessionDelegateAdapter : NSObject <RTCAudioSessionDelegate>

- (instancetype)init NS_UNAVAILABLE;

/** |observer| is a raw pointer and should be kept alive
 *  for this object's lifetime.
 */
- (instancetype)initWithObserver:(webrtc::AudioSessionObserver *)observer
    NS_DESIGNATED_INITIALIZER;

@end
