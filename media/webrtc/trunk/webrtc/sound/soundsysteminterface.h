/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SOUND_SOUNDSYSTEMINTERFACE_H_
#define WEBRTC_SOUND_SOUNDSYSTEMINTERFACE_H_

#include <vector>

#include "webrtc/base/constructormagic.h"

namespace rtc {

class SoundDeviceLocator;
class SoundInputStreamInterface;
class SoundOutputStreamInterface;

// Interface for a platform's sound system.
// Implementations must guarantee thread-safety for at least the following use
// cases:
// 1) Concurrent enumeration and opening of devices from different threads.
// 2) Concurrent use of different Sound(Input|Output)StreamInterface
// instances from different threads (but concurrent use of the _same_ one from
// different threads need not be supported).
class SoundSystemInterface {
 public:
  typedef std::vector<SoundDeviceLocator *> SoundDeviceLocatorList;

  enum SampleFormat {
    // Only one supported sample format at this time.
    // The values here may be used in lookup tables, so they shouldn't change.
    FORMAT_S16LE = 0,
  };

  enum Flags {
    // Enable reporting the current stream latency in
    // Sound(Input|Output)StreamInterface. See those classes for more details.
    FLAG_REPORT_LATENCY = (1 << 0),
  };

  struct OpenParams {
    // Format for the sound stream.
    SampleFormat format;
    // Sampling frequency in hertz.
    unsigned int freq;
    // Number of channels in the PCM stream.
    unsigned int channels;
    // Misc flags. Should be taken from the Flags enum above.
    int flags;
    // Desired latency, measured as number of bytes of sample data
    int latency;
  };

  // Special values for the "latency" field of OpenParams.
  // Use this one to say you don't care what the latency is. The sound system
  // will optimize for other things instead.
  static const int kNoLatencyRequirements = -1;
  // Use this one to say that you want the sound system to pick an appropriate
  // small latency value. The sound system may pick the minimum allowed one, or
  // a slightly higher one in the event that the true minimum requires an
  // undesirable trade-off.
  static const int kLowLatency = 0;

  // Max value for the volume parameters for Sound(Input|Output)StreamInterface.
  static const int kMaxVolume = 255;
  // Min value for the volume parameters for Sound(Input|Output)StreamInterface.
  static const int kMinVolume = 0;

  // Helper for clearing a locator list and deleting the entries.
  static void ClearSoundDeviceLocatorList(SoundDeviceLocatorList *devices);

  virtual ~SoundSystemInterface() {}

  virtual bool Init() = 0;
  virtual void Terminate() = 0;

  // Enumerates the available devices. (Any pre-existing locators in the lists
  // are deleted.)
  virtual bool EnumeratePlaybackDevices(SoundDeviceLocatorList *devices) = 0;
  virtual bool EnumerateCaptureDevices(SoundDeviceLocatorList *devices) = 0;

  // Gets a special locator for the default device.
  virtual bool GetDefaultPlaybackDevice(SoundDeviceLocator **device) = 0;
  virtual bool GetDefaultCaptureDevice(SoundDeviceLocator **device) = 0;

  // Opens the given device, or returns NULL on error.
  virtual SoundOutputStreamInterface *OpenPlaybackDevice(
      const SoundDeviceLocator *device,
      const OpenParams &params) = 0;
  virtual SoundInputStreamInterface *OpenCaptureDevice(
      const SoundDeviceLocator *device,
      const OpenParams &params) = 0;

  // A human-readable name for this sound system.
  virtual const char *GetName() const = 0;

 protected:
  SoundSystemInterface() {}

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(SoundSystemInterface);
};

}  // namespace rtc

#endif  // WEBRTC_SOUND_SOUNDSYSTEMINTERFACE_H_
