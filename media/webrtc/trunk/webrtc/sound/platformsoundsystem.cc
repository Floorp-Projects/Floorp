/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/sound/platformsoundsystem.h"

#include "webrtc/base/common.h"
#if defined(WEBRTC_LINUX) && !defined(WEBRTC_ANDROID)
#include "webrtc/sound/linuxsoundsystem.h"
#else
#include "webrtc/sound/nullsoundsystem.h"
#endif

namespace rtc {

SoundSystemInterface *CreatePlatformSoundSystem() {
#if defined(WEBRTC_LINUX) && !defined(WEBRTC_ANDROID)
  return new LinuxSoundSystem();
#else
  ASSERT(false && "Not implemented");
  return new NullSoundSystem();
#endif
}

}  // namespace rtc
