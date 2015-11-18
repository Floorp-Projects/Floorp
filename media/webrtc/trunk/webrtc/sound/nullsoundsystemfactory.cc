/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/sound/nullsoundsystemfactory.h"

#include "webrtc/sound/nullsoundsystem.h"

namespace rtc {

NullSoundSystemFactory::NullSoundSystemFactory() {
}

NullSoundSystemFactory::~NullSoundSystemFactory() {
}

bool NullSoundSystemFactory::SetupInstance() {
  instance_.reset(new NullSoundSystem());
  return true;
}

void NullSoundSystemFactory::CleanupInstance() {
  instance_.reset();
}

}  // namespace rtc
