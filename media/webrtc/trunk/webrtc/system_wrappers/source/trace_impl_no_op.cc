/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc {

const int Trace::kBoilerplateLength = 71;
const int Trace::kTimestampPosition = 13;
const int Trace::kTimestampLength = 12;

void Trace::CreateTrace() {
}

void Trace::ReturnTrace() {
}

int32_t Trace::SetLevelFilter(uint32_t filter) {
  return 0;
}

int32_t Trace::LevelFilter(uint32_t& filter) {
  return 0;
}

int32_t Trace::TraceFile(char file_name[1024]) {
  return -1;
}

int32_t Trace::SetTraceFile(const char* file_name,
                            const bool add_file_counter) {
  return -1;
}

int32_t Trace::SetTraceCallback(TraceCallback* callback) {
  return -1;
}

void Trace::Add(const TraceLevel level, const TraceModule module,
                const int32_t id, const char* msg, ...) {
}

}  // namespace webrtc
