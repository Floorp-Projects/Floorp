/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SOUND_AUTOMATICALLYCHOSENSOUNDSYSTEM_H_
#define WEBRTC_SOUND_AUTOMATICALLYCHOSENSOUNDSYSTEM_H_

#include "webrtc/sound/soundsysteminterface.h"
#include "webrtc/sound/soundsystemproxy.h"
#include "webrtc/base/common.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/scoped_ptr.h"

namespace rtc {

// A function type that creates an instance of a sound system implementation.
typedef SoundSystemInterface *(*SoundSystemCreator)();

// An AutomaticallyChosenSoundSystem is a sound system proxy that defers to
// an instance of the first sound system implementation in a list that
// successfully initializes.
template <const SoundSystemCreator kSoundSystemCreators[], int kNumSoundSystems>
class AutomaticallyChosenSoundSystem : public SoundSystemProxy {
 public:
  // Chooses and initializes the underlying sound system.
  virtual bool Init();
  // Terminates the underlying sound system implementation, but caches it for
  // future re-use.
  virtual void Terminate();

  virtual const char *GetName() const;

 private:
  rtc::scoped_ptr<SoundSystemInterface> sound_systems_[kNumSoundSystems];
};

template <const SoundSystemCreator kSoundSystemCreators[], int kNumSoundSystems>
bool AutomaticallyChosenSoundSystem<kSoundSystemCreators,
                                    kNumSoundSystems>::Init() {
  if (wrapped_) {
    return true;
  }
  for (int i = 0; i < kNumSoundSystems; ++i) {
    if (!sound_systems_[i].get()) {
      sound_systems_[i].reset((*kSoundSystemCreators[i])());
    }
    if (sound_systems_[i]->Init()) {
      // This is the first sound system in the list to successfully
      // initialize, so we're done.
      wrapped_ = sound_systems_[i].get();
      break;
    }
    // Else it failed to initialize, so try the remaining ones.
  }
  if (!wrapped_) {
    LOG(LS_ERROR) << "Failed to find a usable sound system";
    return false;
  }
  LOG(LS_INFO) << "Selected " << wrapped_->GetName() << " sound system";
  return true;
}

template <const SoundSystemCreator kSoundSystemCreators[], int kNumSoundSystems>
void AutomaticallyChosenSoundSystem<kSoundSystemCreators,
                                    kNumSoundSystems>::Terminate() {
  if (!wrapped_) {
    return;
  }
  wrapped_->Terminate();
  wrapped_ = NULL;
  // We do not free the scoped_ptrs because we may be re-init'ed soon.
}

template <const SoundSystemCreator kSoundSystemCreators[], int kNumSoundSystems>
const char *AutomaticallyChosenSoundSystem<kSoundSystemCreators,
                                           kNumSoundSystems>::GetName() const {
  return wrapped_ ? wrapped_->GetName() : "automatic";
}

}  // namespace rtc

#endif  // WEBRTC_SOUND_AUTOMATICALLYCHOSENSOUNDSYSTEM_H_
