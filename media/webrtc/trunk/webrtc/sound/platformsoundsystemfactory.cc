/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/sound/platformsoundsystemfactory.h"

#include "webrtc/sound/platformsoundsystem.h"
#include "webrtc/sound/soundsysteminterface.h"

namespace rtc {

PlatformSoundSystemFactory::PlatformSoundSystemFactory() {
}

PlatformSoundSystemFactory::~PlatformSoundSystemFactory() {
}

bool PlatformSoundSystemFactory::SetupInstance() {
  if (!instance_.get()) {
    instance_.reset(CreatePlatformSoundSystem());
  }
  if (!instance_->Init()) {
    LOG(LS_ERROR) << "Can't initialize platform's sound system";
    return false;
  }
  return true;
}

void PlatformSoundSystemFactory::CleanupInstance() {
  instance_->Terminate();
  // We do not delete the sound system because we might be re-initialized soon.
}

}  // namespace rtc
