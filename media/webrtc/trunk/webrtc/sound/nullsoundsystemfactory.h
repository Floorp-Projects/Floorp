/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SOUND_NULLSOUNDSYSTEMFACTORY_H_
#define WEBRTC_SOUND_NULLSOUNDSYSTEMFACTORY_H_

#include "webrtc/sound/soundsystemfactory.h"

namespace rtc {

// A SoundSystemFactory that always returns a NullSoundSystem. Intended for
// testing.
class NullSoundSystemFactory : public SoundSystemFactory {
 public:
  NullSoundSystemFactory();
  virtual ~NullSoundSystemFactory();

 protected:
  // Inherited from SoundSystemFactory.
  virtual bool SetupInstance();
  virtual void CleanupInstance();
};

}  // namespace rtc

#endif  // WEBRTC_SOUND_NULLSOUNDSYSTEMFACTORY_H_
