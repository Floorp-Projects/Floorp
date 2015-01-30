/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/main/source/frame_buffer.h"

#include <assert.h>
#include <string.h>

#include "webrtc/modules/video_coding/main/source/packet.h"
#include "webrtc/system_wrappers/interface/logging.h"

namespace webrtc {

VCMFrameBuffer::VCMFrameBuffer()
  :
    _state(kStateEmpty),
    _frameCounted(false),
    _nackCount(0),
    _latestPacketTimeMs(-1) {
}

VCMFrameBuffer::~VCMFrameBuffer() {
}

VCMFrameBuffer::VCMFrameBuffer(const VCMFrameBuffer& rhs)
:
VCMEncodedFrame(rhs),
_state(rhs._state),
_frameCounted(rhs._frameCounted),
_sessionInfo(),
_nackCount(rhs._nackCount),
_latestPacketTimeMs(rhs._latestPacketTimeMs) {
    _sessionInfo = rhs._sessionInfo;
    _sessionInfo.UpdateDataPointers(rhs._buffer, _buffer);
}

webrtc::FrameType
VCMFrameBuffer::FrameType() const {
    return _sessionInfo.FrameType();
}

int32_t
VCMFrameBuffer::GetLowSeqNum() const {
    return _sessionInfo.LowSequenceNumber();
}

int32_t
VCMFrameBuffer::GetHighSeqNum() const {
    return _sessionInfo.HighSequenceNumber();
}

int VCMFrameBuffer::PictureId() const {
  return _sessionInfo.PictureId();
}

int VCMFrameBuffer::TemporalId() const {
  return _sessionInfo.TemporalId();
}

bool VCMFrameBuffer::LayerSync() const {
  return _sessionInfo.LayerSync();
}

int VCMFrameBuffer::Tl0PicId() const {
  return _sessionInfo.Tl0PicId();
}

bool VCMFrameBuffer::NonReference() const {
  return _sessionInfo.NonReference();
}

bool
VCMFrameBuffer::IsSessionComplete() const {
    return _sessionInfo.complete();
}

// Insert packet
VCMFrameBufferEnum
VCMFrameBuffer::InsertPacket(const VCMPacket& packet,
                             int64_t timeInMs,
                             VCMDecodeErrorMode decode_error_mode,
                             const FrameData& frame_data) {
    assert(!(NULL == packet.dataPtr && packet.sizeBytes > 0));
    if (packet.dataPtr != NULL) {
        _payloadType = packet.payloadType;
    }

    if (kStateEmpty == _state) {
        // First packet (empty and/or media) inserted into this frame.
        // store some info and set some initial values.
        _timeStamp = packet.timestamp;
        // We only take the ntp timestamp of the first packet of a frame.
        ntp_time_ms_ = packet.ntp_time_ms_;
        _codec = packet.codec;
        if (packet.frameType != kFrameEmpty) {
            // first media packet
            SetState(kStateIncomplete);
        }
    }

    uint32_t requiredSizeBytes = Length() + packet.sizeBytes +
                   (packet.insertStartCode ? kH264StartCodeLengthBytes : 0);
    if (requiredSizeBytes >= _size) {
        const uint8_t* prevBuffer = _buffer;
        const uint32_t increments = requiredSizeBytes /
                                          kBufferIncStepSizeBytes +
                                        (requiredSizeBytes %
                                         kBufferIncStepSizeBytes > 0);
        const uint32_t newSize = _size +
                                       increments * kBufferIncStepSizeBytes;
        if (newSize > kMaxJBFrameSizeBytes) {
            LOG(LS_ERROR) << "Failed to insert packet due to frame being too "
                             "big.";
            return kSizeError;
        }
        VerifyAndAllocate(newSize);
        _sessionInfo.UpdateDataPointers(prevBuffer, _buffer);
    }

    if (packet.width > 0 && packet.height > 0) {
      _encodedWidth = packet.width;
      _encodedHeight = packet.height;
    }

    CopyCodecSpecific(&packet.codecSpecificHeader);

    int retVal = _sessionInfo.InsertPacket(packet, _buffer,
                                           decode_error_mode,
                                           frame_data);
    if (retVal == -1) {
        return kSizeError;
    } else if (retVal == -2) {
        return kDuplicatePacket;
    } else if (retVal == -3) {
        return kOutOfBoundsPacket;
    }
    // update length
    _length = Length() + static_cast<uint32_t>(retVal);

    _latestPacketTimeMs = timeInMs;

    if (_sessionInfo.complete()) {
      SetState(kStateComplete);
      return kCompleteSession;
    } else if (_sessionInfo.decodable()) {
      SetState(kStateDecodable);
      return kDecodableSession;
    }
    return kIncomplete;
}

int64_t
VCMFrameBuffer::LatestPacketTimeMs() const {
    return _latestPacketTimeMs;
}

void
VCMFrameBuffer::IncrementNackCount() {
    _nackCount++;
}

int16_t
VCMFrameBuffer::GetNackCount() const {
    return _nackCount;
}

bool
VCMFrameBuffer::HaveFirstPacket() const {
    return _sessionInfo.HaveFirstPacket();
}

bool
VCMFrameBuffer::HaveLastPacket() const {
    return _sessionInfo.HaveLastPacket();
}

int
VCMFrameBuffer::NumPackets() const {
    return _sessionInfo.NumPackets();
}

void
VCMFrameBuffer::Reset() {
    _length = 0;
    _timeStamp = 0;
    _sessionInfo.Reset();
    _frameCounted = false;
    _payloadType = 0;
    _nackCount = 0;
    _latestPacketTimeMs = -1;
    _state = kStateEmpty;
    VCMEncodedFrame::Reset();
}

// Set state of frame
void
VCMFrameBuffer::SetState(VCMFrameBufferStateEnum state) {
    if (_state == state) {
        return;
    }
    switch (state) {
    case kStateIncomplete:
        // we can go to this state from state kStateEmpty
        assert(_state == kStateEmpty);

        // Do nothing, we received a packet
        break;

    case kStateComplete:
        assert(_state == kStateEmpty ||
               _state == kStateIncomplete ||
               _state == kStateDecodable);

        break;

    case kStateEmpty:
        // Should only be set to empty through Reset().
        assert(false);
        break;

    case kStateDecodable:
        assert(_state == kStateEmpty ||
               _state == kStateIncomplete);
        break;
    }
    _state = state;
}

// Set counted status (as counted by JB or not)
void VCMFrameBuffer::SetCountedFrame(bool frameCounted) {
    _frameCounted = frameCounted;
}

bool VCMFrameBuffer::GetCountedFrame() const {
    return _frameCounted;
}

// Get current state of frame
VCMFrameBufferStateEnum
VCMFrameBuffer::GetState() const {
    return _state;
}

// Get current state of frame
VCMFrameBufferStateEnum
VCMFrameBuffer::GetState(uint32_t& timeStamp) const {
    timeStamp = TimeStamp();
    return GetState();
}

bool
VCMFrameBuffer::IsRetransmitted() const {
    return _sessionInfo.session_nack();
}

void
VCMFrameBuffer::PrepareForDecode(bool continuous) {
#ifdef INDEPENDENT_PARTITIONS
    if (_codec == kVideoCodecVP8) {
        _length =
            _sessionInfo.BuildVP8FragmentationHeader(_buffer, _length,
                                                     &_fragmentation);
    } else {
        int bytes_removed = _sessionInfo.MakeDecodable();
        _length -= bytes_removed;
    }
#else
    int bytes_removed = _sessionInfo.MakeDecodable();
    _length -= bytes_removed;
#endif
    // Transfer frame information to EncodedFrame and create any codec
    // specific information.
    _frameType = ConvertFrameType(_sessionInfo.FrameType());
    _completeFrame = _sessionInfo.complete();
    _missingFrame = !continuous;
}

}  // namespace webrtc
