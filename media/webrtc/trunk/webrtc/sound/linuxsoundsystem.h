/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SOUND_LINUXSOUNDSYSTEM_H_
#define WEBRTC_SOUND_LINUXSOUNDSYSTEM_H_

#include "webrtc/sound/automaticallychosensoundsystem.h"

namespace rtc {

extern const SoundSystemCreator kLinuxSoundSystemCreators[
#ifdef HAVE_LIBPULSE
    2
#else
    1
#endif
    ];

// The vast majority of Linux systems use ALSA for the device-level sound API,
// but an increasing number are using PulseAudio for the application API and
// only using ALSA internally in PulseAudio itself. But like everything on
// Linux this is user-configurable, so we need to support both and choose the
// right one at run-time.
// PulseAudioSoundSystem is designed to only successfully initialize if
// PulseAudio is installed and running, and if it is running then direct device
// access using ALSA typically won't work, so if PulseAudioSoundSystem
// initializes then we choose that. Otherwise we choose ALSA.
typedef AutomaticallyChosenSoundSystem<
    kLinuxSoundSystemCreators,
    ARRAY_SIZE(kLinuxSoundSystemCreators)> LinuxSoundSystem;

}  // namespace rtc

#endif  // WEBRTC_SOUND_LINUXSOUNDSYSTEM_H_
