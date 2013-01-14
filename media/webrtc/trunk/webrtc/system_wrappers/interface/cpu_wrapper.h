/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SYSTEM_WRAPPERS_INTERFACE_CPU_WRAPPER_H_
#define WEBRTC_SYSTEM_WRAPPERS_INTERFACE_CPU_WRAPPER_H_

#include "typedefs.h"

namespace webrtc {

class CpuWrapper {
 public:
  static CpuWrapper* CreateCpu();
  virtual ~CpuWrapper() {}

  // Returns the average CPU usage for all processors. The CPU usage can be
  // between and including 0 to 100 (%)
  virtual WebRtc_Word32 CpuUsage() = 0;
  virtual WebRtc_Word32 CpuUsage(WebRtc_Word8* process_name,
                                 WebRtc_UWord32 length) = 0;
  virtual WebRtc_Word32 CpuUsage(WebRtc_UWord32 process_id) = 0;

  // The CPU usage per core is returned in cpu_usage. The CPU can be between
  // and including 0 to 100 (%)
  // Note that the pointer passed as cpu_usage is redirected to a local member
  // of the CPU wrapper.
  // num_cores is the number of cores in the cpu_usage array.
  // The return value is -1 for failure or 0-100, indicating the average
  // CPU usage across all cores.
  // Note: on some OSs this class is initialized lazy. This means that it
  // might not yet be possible to retrieve any CPU metrics. When this happens
  // the return value will be zero (indicating that there is not a failure),
  // num_cores will be 0 and cpu_usage will be set to NULL (indicating that
  // no metrics are available yet). Once the initialization is completed,
  // which can take in the order of seconds, CPU metrics can be retrieved.
  virtual WebRtc_Word32 CpuUsageMultiCore(WebRtc_UWord32& num_cores,
                                          WebRtc_UWord32*& cpu_usage) = 0;

  virtual void Reset() = 0;
  virtual void Stop() = 0;

 protected:
  CpuWrapper() {}
};

} // namespace webrtc

#endif // WEBRTC_SYSTEM_WRAPPERS_INTERFACE_CPU_WRAPPER_H_
