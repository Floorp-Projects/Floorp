/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SOUND_SOUNDINPUTSTREAMINTERFACE_H_
#define WEBRTC_SOUND_SOUNDINPUTSTREAMINTERFACE_H_

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/sigslot.h"

namespace rtc {

// Interface for consuming an input stream from a recording device.
// Semantics and thread-safety of StartReading()/StopReading() are the same as
// for rtc::Worker.
class SoundInputStreamInterface {
 public:
  virtual ~SoundInputStreamInterface() {}

  // Starts the reading of samples on the current thread.
  virtual bool StartReading() = 0;
  // Stops the reading of samples.
  virtual bool StopReading() = 0;

  // Retrieves the current input volume for this stream. Nominal range is
  // defined by SoundSystemInterface::k(Max|Min)Volume, but values exceeding the
  // max may be possible in some implementations. This call retrieves the actual
  // volume currently in use by the OS, not a cached value from a previous
  // (Get|Set)Volume() call.
  virtual bool GetVolume(int *volume) = 0;

  // Changes the input volume for this stream. Nominal range is defined by
  // SoundSystemInterface::k(Max|Min)Volume. The effect of exceeding kMaxVolume
  // is implementation-defined.
  virtual bool SetVolume(int volume) = 0;

  // Closes this stream object. If currently reading then this may only be
  // called from the reading thread.
  virtual bool Close() = 0;

  // Get the latency of the stream.
  virtual int LatencyUsecs() = 0;

  // Notifies the consumer of new data read from the device.
  // The first parameter is a pointer to the data read, and is only valid for
  // the duration of the call.
  // The second parameter is the amount of data read in bytes (i.e., the valid
  // length of the memory pointed to).
  // The 3rd parameter is the stream that is issuing the callback.
  sigslot::signal3<const void *, size_t,
      SoundInputStreamInterface *> SignalSamplesRead;

 protected:
  SoundInputStreamInterface() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SoundInputStreamInterface);
};

}  // namespace rtc

#endif  // WEBRTC_SOUND_SOUNDOUTPUTSTREAMINTERFACE_H_
