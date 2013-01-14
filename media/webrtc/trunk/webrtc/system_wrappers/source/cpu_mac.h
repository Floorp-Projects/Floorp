/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SYSTEM_WRAPPERS_SOURCE_CPU_MAC_H_
#define WEBRTC_SYSTEM_WRAPPERS_SOURCE_CPU_MAC_H_

#include "system_wrappers/interface/cpu_wrapper.h"

namespace webrtc {

class CpuWrapperMac : public CpuWrapper {
 public:
  CpuWrapperMac();
  virtual ~CpuWrapperMac();

  virtual WebRtc_Word32 CpuUsage();
  virtual WebRtc_Word32 CpuUsage(WebRtc_Word8* process_name,
                                 WebRtc_UWord32 length) {
    return -1;
  }
  virtual WebRtc_Word32 CpuUsage(WebRtc_UWord32 process_id) {
    return -1;
  }

  // Note: this class will block the call and sleep if called too fast
  // This function blocks the calling thread if the thread is calling it more
  // often than every 500 ms.
  virtual WebRtc_Word32 CpuUsageMultiCore(WebRtc_UWord32& num_cores,
                                          WebRtc_UWord32*& array);

  virtual void Reset() {}
  virtual void Stop() {}

 private:
  WebRtc_Word32 Update(WebRtc_Word64 time_diffMS);

  WebRtc_UWord32  cpu_count_;
  WebRtc_UWord32* cpu_usage_;
  WebRtc_Word32   total_cpu_usage_;
  WebRtc_Word64*  last_tick_count_;
  WebRtc_Word64   last_time_;
};

} // namespace webrtc

#endif  // WEBRTC_SYSTEM_WRAPPERS_SOURCE_CPU_MAC_H_
