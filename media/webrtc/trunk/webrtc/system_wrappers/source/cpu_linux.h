/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SYSTEM_WRAPPERS_SOURCE_CPU_LINUX_H_
#define WEBRTC_SYSTEM_WRAPPERS_SOURCE_CPU_LINUX_H_

#include "system_wrappers/interface/cpu_wrapper.h"

namespace webrtc {

class CpuLinux : public CpuWrapper {
 public:
  CpuLinux();
  virtual ~CpuLinux();

  virtual WebRtc_Word32 CpuUsage();
  virtual WebRtc_Word32 CpuUsage(WebRtc_Word8* process_name,
                                 WebRtc_UWord32 length) {
    return 0;
  }
  virtual WebRtc_Word32 CpuUsage(WebRtc_UWord32 process_id) {
    return 0;
  }

  virtual WebRtc_Word32 CpuUsageMultiCore(WebRtc_UWord32& num_cores,
                                          WebRtc_UWord32*& array);

  virtual void Reset() {
    return;
  }
  virtual void Stop() {
    return;
  }
 private:
  int GetData(long long& busy, long long& idle, long long*& busy_array,
              long long*& idle_array);
  int GetNumCores();

  long long old_busy_time_;
  long long old_idle_time_;

  long long* old_busy_time_multi_;
  long long* old_idle_time_multi_;

  long long* idle_array_;
  long long* busy_array_;
  WebRtc_UWord32* result_array_;
  WebRtc_UWord32  num_cores_;
};

} // namespace webrtc

#endif  // WEBRTC_SYSTEM_WRAPPERS_SOURCE_CPU_LINUX_H_
