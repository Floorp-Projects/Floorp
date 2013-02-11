/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * decode_bwe.c
 *
 * This C file contains the internal decode bandwidth estimate function.
 *
 */


#include "bandwidth_estimator.h"
#include "codec.h"
#include "entropy_coding.h"
#include "structs.h"




int WebRtcIsacfix_EstimateBandwidth(BwEstimatorstr *bwest_str,
                                    Bitstr_dec  *streamdata,
                                    WebRtc_Word32  packet_size,
                                    WebRtc_UWord16 rtp_seq_number,
                                    WebRtc_UWord32 send_ts,
                                    WebRtc_UWord32 arr_ts)
{
  WebRtc_Word16 index;
  WebRtc_Word16 frame_samples;
  int err;

  /* decode framelength */
  err = WebRtcIsacfix_DecodeFrameLen(streamdata, &frame_samples);
  /* error check */
  if (err<0) {
    return err;
  }

  /* decode BW estimation */
  err = WebRtcIsacfix_DecodeSendBandwidth(streamdata, &index);
  /* error check */
  if (err<0) {
    return err;
  }

  /* Update BWE with received data */
  err = WebRtcIsacfix_UpdateUplinkBwImpl(
      bwest_str,
      rtp_seq_number,
      (WebRtc_UWord16)WEBRTC_SPL_UDIV(WEBRTC_SPL_UMUL(frame_samples,1000), FS),
      send_ts,
      arr_ts,
      (WebRtc_Word16) packet_size,  /* in bytes */
      index);

  /* error check */
  if (err<0) {
    return err;
  }

  /* Succesful */
  return 0;
}
