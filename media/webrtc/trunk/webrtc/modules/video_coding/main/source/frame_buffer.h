/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_FRAME_BUFFER_H_
#define WEBRTC_MODULES_VIDEO_CODING_FRAME_BUFFER_H_

#include "modules/interface/module_common_types.h"
#include "modules/video_coding/main/source/encoded_frame.h"
#include "modules/video_coding/main/source/jitter_buffer_common.h"
#include "modules/video_coding/main/source/session_info.h"
#include "typedefs.h"

namespace webrtc
{

class VCMFrameBuffer : public VCMEncodedFrame
{
public:
    VCMFrameBuffer();
    virtual ~VCMFrameBuffer();

    VCMFrameBuffer(VCMFrameBuffer& rhs);

    virtual void Reset();

    VCMFrameBufferEnum InsertPacket(const VCMPacket& packet,
                                    int64_t timeInMs,
                                    bool enableDecodableState,
                                    uint32_t rttMs);

    // State
    // Get current state of frame
    VCMFrameBufferStateEnum GetState() const;
    // Get current state and timestamp of frame
    VCMFrameBufferStateEnum GetState(uint32_t& timeStamp) const;
    void SetState(VCMFrameBufferStateEnum state); // Set state of frame

    bool IsRetransmitted() const;
    bool IsSessionComplete() const;
    bool HaveFirstPacket() const;
    bool HaveLastPacket() const;
    // Makes sure the session contain a decodable stream.
    void MakeSessionDecodable();

    // Sequence numbers
    // Get lowest packet sequence number in frame
    int32_t GetLowSeqNum() const;
    // Get highest packet sequence number in frame
    int32_t GetHighSeqNum() const;

    int PictureId() const;
    int TemporalId() const;
    bool LayerSync() const;
    int Tl0PicId() const;
    bool NonReference() const;

    // Set counted status (as counted by JB or not)
    void SetCountedFrame(bool frameCounted);
    bool GetCountedFrame() const;

    // Increments a counter to keep track of the number of packets of this frame
    // which were NACKed before they arrived.
    void IncrementNackCount();
    // Returns the number of packets of this frame which were NACKed before they
    // arrived.
    int16_t GetNackCount() const;

    int64_t LatestPacketTimeMs() const;

    webrtc::FrameType FrameType() const;
    void SetPreviousFrameLoss();

    int32_t ExtractFromStorage(const EncodedVideoData& frameFromStorage);

    // The number of packets discarded because the decoder can't make use of
    // them.
    int NotDecodablePackets() const;

protected:
    void RestructureFrameInformation();
    void PrepareForDecode();

private:
    VCMFrameBufferStateEnum    _state;         // Current state of the frame
    bool                       _frameCounted;  // Was this frame counted by JB?
    VCMSessionInfo             _sessionInfo;
    uint16_t             _nackCount;
    int64_t              _latestPacketTimeMs;
};

} // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_CODING_FRAME_BUFFER_H_
