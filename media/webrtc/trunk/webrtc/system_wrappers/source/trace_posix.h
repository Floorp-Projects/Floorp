/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SYSTEM_WRAPPERS_SOURCE_TRACE_POSIX_H_
#define WEBRTC_SYSTEM_WRAPPERS_SOURCE_TRACE_POSIX_H_

#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/source/trace_impl.h"

namespace webrtc {

class TracePosix : public TraceImpl {
 public:
  TracePosix();
  virtual ~TracePosix();

  virtual WebRtc_Word32 AddTime(char* trace_message,
                                const TraceLevel level) const;

  virtual WebRtc_Word32 AddBuildInfo(char* trace_message) const;
  virtual WebRtc_Word32 AddDateTimeInfo(char* trace_message) const;

 private:
  volatile mutable WebRtc_UWord32  prev_api_tick_count_;
  volatile mutable WebRtc_UWord32  prev_tick_count_;
};

}  // namespace webrtc

#endif  // WEBRTC_SYSTEM_WRAPPERS_SOURCE_TRACE_POSIX_H_
