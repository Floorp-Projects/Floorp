/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_LOW_LATENCY_EVENT_H_
#define WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_LOW_LATENCY_EVENT_H_

#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>

namespace webrtc {

// Implementation of event for single waiter, single signal threads. Event
// is sticky.
class LowLatencyEvent {
 public:
  LowLatencyEvent();
  ~LowLatencyEvent();

  // Readies the event. Must be called before signaling or waiting for event.
  // Returns true on success.
  bool Start();
  // Shuts down the event and releases threads calling WaitOnEvent. Once
  // stopped SignalEvent and WaitOnEvent will have no effect. Start can be
  // called to re-enable the event.
  // Returns true on success.
  bool Stop();

  // Releases thread calling WaitOnEvent in a sticky fashion.
  void SignalEvent(int event_id, int event_msg);
  // Waits until SignalEvent or Stop is called.
  void WaitOnEvent(int* event_id, int* event_msg);

 private:
  typedef int Handle;
  static const Handle kInvalidHandle;
  static const int kReadHandle;
  static const int kWriteHandle;

  // Closes the handle. Returns true on success.
  static bool Close(Handle* handle);

  // SignalEvent and WaitOnEvent are actually read/write to file descriptors.
  // Write is signal.
  void WriteFd(int message_id, int message);
  // Read is wait.
  void ReadFd(int* message_id, int* message);

  Handle handles_[2];
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_LOW_LATENCY_EVENT_H_
