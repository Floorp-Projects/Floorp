/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "jitter_buffer_common.h"

#include <cstdlib>

namespace webrtc {

WebRtc_UWord32 LatestTimestamp(WebRtc_UWord32 timestamp1,
                               WebRtc_UWord32 timestamp2,
                               bool* has_wrapped) {
  bool wrap = (timestamp2 < 0x0000ffff && timestamp1 > 0xffff0000) ||
      (timestamp2 > 0xffff0000 && timestamp1 < 0x0000ffff);
  if (has_wrapped != NULL)
    *has_wrapped = wrap;
  if (timestamp1 > timestamp2 && !wrap)
      return timestamp1;
  else if (timestamp1 <= timestamp2 && !wrap)
      return timestamp2;
  else if (timestamp1 < timestamp2 && wrap)
      return timestamp1;
  else
      return timestamp2;
}

WebRtc_Word32 LatestSequenceNumber(WebRtc_Word32 seq_num1,
                                   WebRtc_Word32 seq_num2,
                                   bool* has_wrapped) {
  if (seq_num1 < 0 && seq_num2 < 0)
    return -1;
  else if (seq_num1 < 0)
    return seq_num2;
  else if (seq_num2 < 0)
    return seq_num1;

  bool wrap = (seq_num1 < 0x00ff && seq_num2 > 0xff00) ||
          (seq_num1 > 0xff00 && seq_num2 < 0x00ff);

  if (has_wrapped != NULL)
    *has_wrapped = wrap;

  if (seq_num2 > seq_num1 && !wrap)
    return seq_num2;
  else if (seq_num2 <= seq_num1 && !wrap)
    return seq_num1;
  else if (seq_num2 < seq_num1 && wrap)
    return seq_num2;
  else
    return seq_num1;
}

}  // namespace webrtc
