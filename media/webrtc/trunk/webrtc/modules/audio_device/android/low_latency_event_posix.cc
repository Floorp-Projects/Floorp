/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_device/android/low_latency_event.h"

#include <assert.h>

#define HANDLE_EINTR(x) ({                \
      typeof(x) eintr_wrapper_result;     \
      do {                                                        \
        eintr_wrapper_result = (x);                               \
      } while (eintr_wrapper_result == -1 && errno == EINTR);     \
      eintr_wrapper_result;                                       \
    })

#define IGNORE_EINTR(x) ({                \
      typeof(x) eintr_wrapper_result;     \
      do {                                                        \
        eintr_wrapper_result = (x);                               \
        if (eintr_wrapper_result == -1 && errno == EINTR) {       \
          eintr_wrapper_result = 0;                               \
        }                                                         \
      } while (0);                                                \
      eintr_wrapper_result;                                       \
    })

namespace webrtc {

const LowLatencyEvent::Handle LowLatencyEvent::kInvalidHandle = -1;
const int LowLatencyEvent::kReadHandle = 0;
const int LowLatencyEvent::kWriteHandle = 1;

LowLatencyEvent::LowLatencyEvent() {
  handles_[kReadHandle] = kInvalidHandle;
  handles_[kWriteHandle] = kInvalidHandle;
}

LowLatencyEvent::~LowLatencyEvent() {
  Stop();
}

bool LowLatencyEvent::Start() {
  assert(handles_[kReadHandle] == kInvalidHandle);
  assert(handles_[kWriteHandle] == kInvalidHandle);

  return socketpair(AF_UNIX, SOCK_STREAM, 0, handles_) == 0;
}

bool LowLatencyEvent::Stop() {
  bool ret = Close(&handles_[kReadHandle]) && Close(&handles_[kWriteHandle]);
  handles_[kReadHandle] = kInvalidHandle;
  handles_[kWriteHandle] = kInvalidHandle;
  return ret;
}

void LowLatencyEvent::SignalEvent(int event_id, int event_msg) {
  WriteFd(event_id, event_msg);
}

void LowLatencyEvent::WaitOnEvent(int* event_id, int* event_msg) {
  ReadFd(event_id, event_msg);
}

bool LowLatencyEvent::Close(Handle* handle) {
  if (*handle == kInvalidHandle) {
    return false;
  }
  int retval = IGNORE_EINTR(close(*handle));
  *handle = kInvalidHandle;
  return retval == 0;
}

void LowLatencyEvent::WriteFd(int message_id, int message) {
  char buffer[sizeof(message_id) + sizeof(message)];
  size_t bytes = sizeof(buffer);
  memcpy(buffer, &message_id, sizeof(message_id));
  memcpy(&buffer[sizeof(message_id)], &message, sizeof(message));
  ssize_t bytes_written = HANDLE_EINTR(write(handles_[kWriteHandle], buffer,
                                             bytes));
  if (bytes_written != static_cast<ssize_t>(bytes)) {
    assert(false);
  }
}

void LowLatencyEvent::ReadFd(int* message_id, int* message) {
  char buffer[sizeof(message_id) + sizeof(message)];
  size_t bytes = sizeof(buffer);
  ssize_t bytes_read = HANDLE_EINTR(read(handles_[kReadHandle], buffer, bytes));
  if (bytes_read == 0) {
    *message_id = 0;
    *message = 0;
    return;
  } else if (bytes_read == static_cast<ssize_t>(bytes)) {
    memcpy(message_id, buffer, sizeof(*message_id));
    memcpy(message, &buffer[sizeof(*message_id)], sizeof(*message));
  } else {
    assert(false);
  }
}

}  // namespace webrtc
