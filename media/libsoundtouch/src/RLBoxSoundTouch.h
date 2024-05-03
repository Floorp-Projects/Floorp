/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RLBOXSOUNDTOUCH_H_
#define RLBOXSOUNDTOUCH_H_

#include "RLBoxSoundTouchTypes.h"

// Load general firefox configuration of RLBox
#include "mozilla/rlbox/rlbox_config.h"
#undef RLBOX_WASM2C_MODULE_NAME
#define RLBOX_WASM2C_MODULE_NAME rlboxsoundtouch

#ifdef MOZ_WASM_SANDBOXING_SOUNDTOUCH
// Include the generated header file so that we are able to resolve the symbols
// in the wasm binary
#  include "rlboxsoundtouch.wasm.h"
#  define RLBOX_USE_STATIC_CALLS() rlbox_wasm2c_sandbox_lookup_symbol
#  include "mozilla/rlbox/rlbox_wasm2c_sandbox.hpp"
#else
// Extra configuration for no-op sandbox
#  define RLBOX_USE_STATIC_CALLS() rlbox_noop_sandbox_lookup_symbol
#  include "mozilla/rlbox/rlbox_noop_sandbox.hpp"
#endif

#include "mozilla/rlbox/rlbox.hpp"
#include "AudioStream.h"
//  Use abort() instead of exception in SoundTouch.
#define ST_NO_EXCEPTION_HANDLING 1
#include "soundtouch/SoundTouchFactory.h"

#if defined(WIN32)
#if defined(BUILDING_SOUNDTOUCH)
#define RLBOX_SOUNDTOUCH_API __declspec(dllexport)
#else
#define RLBOX_SOUNDTOUCH_API __declspec(dllimport)
#endif
#else
#define RLBOX_SOUNDTOUCH_API __attribute__((visibility("default")))
#endif

namespace mozilla {

class RLBoxSoundTouch {
 public:
  RLBOX_SOUNDTOUCH_API
  RLBoxSoundTouch();
  RLBOX_SOUNDTOUCH_API
  ~RLBoxSoundTouch();

  RLBOX_SOUNDTOUCH_API
  void setSampleRate(uint aRate);
  RLBOX_SOUNDTOUCH_API
  void setChannels(uint aChannels);
  RLBOX_SOUNDTOUCH_API
  void setPitch(double aPitch);
  RLBOX_SOUNDTOUCH_API
  void setSetting(int aSettingId, int aValue);
  RLBOX_SOUNDTOUCH_API
  void setTempo(double aTempo);
  RLBOX_SOUNDTOUCH_API
  uint numChannels();
  RLBOX_SOUNDTOUCH_API
  tainted_soundtouch<uint> numSamples();
  RLBOX_SOUNDTOUCH_API
  tainted_soundtouch<uint> numUnprocessedSamples();
  RLBOX_SOUNDTOUCH_API
  void setRate(double aRate);
  RLBOX_SOUNDTOUCH_API
  void putSamples(const mozilla::AudioDataValue* aSamples, uint aNumSamples);
  RLBOX_SOUNDTOUCH_API
  uint receiveSamples(mozilla::AudioDataValue* aOutput, uint aMaxSamples);
  RLBOX_SOUNDTOUCH_API
  void flush();

 private:
  uint mChannels{0};
  rlbox_sandbox_soundtouch mSandbox;
  tainted_soundtouch<mozilla::AudioDataValue*> mSampleBuffer{nullptr};
  uint mSampleBufferSize{1};
  tainted_soundtouch<soundtouch::SoundTouch*> mTimeStretcher{nullptr};

  RLBOX_SOUNDTOUCH_API
  void resizeSampleBuffer(uint aNewSize);
};

}  // namespace mozilla
#endif
