/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/rtc_event_log_visualizer/analyzer_bindings.h"

#include <algorithm>
#include <cstddef>

void analyze_rtc_event_log(const char* log,
                           size_t log_size,
                           const char* selection,
                           size_t selection_size,
                           char* output,
                           size_t* output_size) {
  size_t size = std::min(log_size, *output_size);
  for (size_t i = 0; i < size; ++i) {
    output[i] = log[i];
  }
  *output_size = size;
}
