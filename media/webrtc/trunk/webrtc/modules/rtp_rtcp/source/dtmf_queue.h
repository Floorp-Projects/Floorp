/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_DTMF_QUEUE_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_DTMF_QUEUE_H_

#include "typedefs.h"
#include "rtp_rtcp_config.h"

#include "critical_section_wrapper.h"

namespace webrtc {
class DTMFqueue
{
public:
    DTMFqueue();
    virtual ~DTMFqueue();

    WebRtc_Word32 AddDTMF(WebRtc_UWord8 DTMFKey, WebRtc_UWord16 len, WebRtc_UWord8 level);
    WebRtc_Word8 NextDTMF(WebRtc_UWord8* DTMFKey, WebRtc_UWord16 * len, WebRtc_UWord8 * level);
    bool PendingDTMF();
    void ResetDTMF();

private:
    CriticalSectionWrapper* _DTMFCritsect;
    WebRtc_UWord8        _nextEmptyIndex;
    WebRtc_UWord8        _DTMFKey[DTMF_OUTBAND_MAX];
    WebRtc_UWord16       _DTMFLen[DTMF_OUTBAND_MAX];
    WebRtc_UWord8        _DTMFLevel[DTMF_OUTBAND_MAX];
};
} // namespace webrtc

#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_DTMF_QUEUE_H_
