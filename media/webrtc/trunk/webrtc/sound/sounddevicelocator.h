/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SOUND_SOUNDDEVICELOCATOR_H_
#define WEBRTC_SOUND_SOUNDDEVICELOCATOR_H_

#include <string>

#include "webrtc/base/constructormagic.h"

namespace rtc {

// A simple container for holding the name of a device and any additional id
// information needed to locate and open it. Implementations of
// SoundSystemInterface must subclass this to add any id information that they
// need.
class SoundDeviceLocator {
 public:
  virtual ~SoundDeviceLocator() {}

  // Human-readable name for the device.
  const std::string &name() const { return name_; }

  // Name sound system uses to locate this device.
  const std::string &device_name() const { return device_name_; }

  // Makes a duplicate of this locator.
  virtual SoundDeviceLocator *Copy() const = 0;

 protected:
  SoundDeviceLocator(const std::string &name,
                     const std::string &device_name)
      : name_(name), device_name_(device_name) {}

  explicit SoundDeviceLocator(const SoundDeviceLocator &that)
      : name_(that.name_), device_name_(that.device_name_) {}

  std::string name_;
  std::string device_name_;

 private:
  DISALLOW_ASSIGN(SoundDeviceLocator);
};

}  // namespace rtc

#endif  // WEBRTC_SOUND_SOUNDDEVICELOCATOR_H_
