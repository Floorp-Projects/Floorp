/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SOUND_SOUNDOUTPUTSTREAMINTERFACE_H_
#define WEBRTC_SOUND_SOUNDOUTPUTSTREAMINTERFACE_H_

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/sigslot.h"

namespace rtc {

// Interface for outputting a stream to a playback device.
// Semantics and thread-safety of EnableBufferMonitoring()/
// DisableBufferMonitoring() are the same as for rtc::Worker.
class SoundOutputStreamInterface {
 public:
  virtual ~SoundOutputStreamInterface();

  // Enables monitoring the available buffer space on the current thread.
  virtual bool EnableBufferMonitoring() = 0;
  // Disables the monitoring.
  virtual bool DisableBufferMonitoring() = 0;

  // Write the given samples to the devices. If currently monitoring then this
  // may only be called from the monitoring thread.
  virtual bool WriteSamples(const void *sample_data,
                            size_t size) = 0;

  // Retrieves the current output volume for this stream. Nominal range is
  // defined by SoundSystemInterface::k(Max|Min)Volume, but values exceeding the
  // max may be possible in some implementations. This call retrieves the actual
  // volume currently in use by the OS, not a cached value from a previous
  // (Get|Set)Volume() call.
  virtual bool GetVolume(int *volume) = 0;

  // Changes the output volume for this stream. Nominal range is defined by
  // SoundSystemInterface::k(Max|Min)Volume. The effect of exceeding kMaxVolume
  // is implementation-defined.
  virtual bool SetVolume(int volume) = 0;

  // Closes this stream object. If currently monitoring then this may only be
  // called from the monitoring thread.
  virtual bool Close() = 0;

  // Get the latency of the stream.
  virtual int LatencyUsecs() = 0;

  // Notifies the producer of the available buffer space for writes.
  // It fires continuously as long as the space is greater than zero.
  // The first parameter is the amount of buffer space available for data to
  // be written (i.e., the maximum amount of data that can be written right now
  // with WriteSamples() without blocking).
  // The 2nd parameter is the stream that is issuing the callback.
  sigslot::signal2<size_t, SoundOutputStreamInterface *> SignalBufferSpace;

 protected:
  SoundOutputStreamInterface();

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(SoundOutputStreamInterface);
};

}  // namespace rtc

#endif  // WEBRTC_SOUND_SOUNDOUTPUTSTREAMINTERFACE_H_
