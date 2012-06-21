/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_RECEIVER_H_
#define WEBRTC_MODULES_VIDEO_CODING_RECEIVER_H_

#include "critical_section_wrapper.h"
#include "jitter_buffer.h"
#include "modules/video_coding/main/source/tick_time_base.h"
#include "timing.h"
#include "packet.h"

namespace webrtc
{

class VCMEncodedFrame;

enum VCMNackStatus
{
    kNackOk,
    kNackNeedMoreMemory,
    kNackKeyFrameRequest
};


enum VCMReceiverState
{
    kReceiving,
    kPassive,
    kWaitForPrimaryDecode
};

class VCMReceiver
{
public:
    VCMReceiver(VCMTiming& timing,
                TickTimeBase* clock,
                WebRtc_Word32 vcmId = -1,
                WebRtc_Word32 receiverId = -1,
                bool master = true);
    ~VCMReceiver();

    void Reset();
    WebRtc_Word32 Initialize();
    void UpdateRtt(WebRtc_UWord32 rtt);
    WebRtc_Word32 InsertPacket(const VCMPacket& packet,
                               WebRtc_UWord16 frameWidth,
                               WebRtc_UWord16 frameHeight);
    VCMEncodedFrame* FrameForDecoding(WebRtc_UWord16 maxWaitTimeMs,
                                      WebRtc_Word64& nextRenderTimeMs,
                                      bool renderTiming = true,
                                      VCMReceiver* dualReceiver = NULL);
    void ReleaseFrame(VCMEncodedFrame* frame);
    WebRtc_Word32 ReceiveStatistics(WebRtc_UWord32& bitRate, WebRtc_UWord32& frameRate);
    WebRtc_Word32 ReceivedFrameCount(VCMFrameCount& frameCount) const;
    WebRtc_UWord32 DiscardedPackets() const;

    // NACK
    void SetNackMode(VCMNackMode nackMode);
    VCMNackMode NackMode() const;
    VCMNackStatus NackList(WebRtc_UWord16* nackList, WebRtc_UWord16& size);

    // Dual decoder
    bool DualDecoderCaughtUp(VCMEncodedFrame* dualFrame, VCMReceiver& dualReceiver) const;
    VCMReceiverState State() const;

private:
    VCMEncodedFrame* FrameForDecoding(WebRtc_UWord16 maxWaitTimeMs,
                                      WebRtc_Word64 nextrenderTimeMs,
                                      VCMReceiver* dualReceiver);
    VCMEncodedFrame* FrameForRendering(WebRtc_UWord16 maxWaitTimeMs,
                                       WebRtc_Word64 nextrenderTimeMs,
                                       VCMReceiver* dualReceiver);
    void CopyJitterBufferStateFromReceiver(const VCMReceiver& receiver);
    void UpdateState(VCMReceiverState newState);
    void UpdateState(VCMEncodedFrame& frame);
    static WebRtc_Word32 GenerateReceiverId();

    CriticalSectionWrapper* _critSect;
    WebRtc_Word32           _vcmId;
    TickTimeBase*           _clock;
    WebRtc_Word32           _receiverId;
    bool                    _master;
    VCMJitterBuffer         _jitterBuffer;
    VCMTiming&              _timing;
    VCMEvent&               _renderWaitEvent;
    VCMReceiverState        _state;

    static WebRtc_Word32    _receiverIdCounter;
};

} // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_CODING_RECEIVER_H_
