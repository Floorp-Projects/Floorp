/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/sound/soundsysteminterface.h"

#include "webrtc/sound/sounddevicelocator.h"

namespace rtc {

void SoundSystemInterface::ClearSoundDeviceLocatorList(
    SoundSystemInterface::SoundDeviceLocatorList *devices) {
  for (SoundDeviceLocatorList::iterator i = devices->begin();
       i != devices->end();
       ++i) {
    if (*i) {
      delete *i;
    }
  }
  devices->clear();
}

}  // namespace rtc
