/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "frame_buffer.h"
#include "packet.h"

#include <cassert>
#include <string.h>

#if defined(_WIN32)
    // VS 2005: Don't warn for default initialized arrays. See help for more info.
    #pragma warning(disable:4351)
#endif

namespace webrtc {

VCMFrameBuffer::VCMFrameBuffer()
  :
    _state(kStateFree),
    _frameCounted(false),
    _nackCount(0),
    _latestPacketTimeMs(-1) {
}

VCMFrameBuffer::~VCMFrameBuffer() {
}

VCMFrameBuffer::VCMFrameBuffer(VCMFrameBuffer& rhs)
:
VCMEncodedFrame(rhs),
_state(rhs._state),
_frameCounted(rhs._frameCounted),
_sessionInfo(),
_nackCount(rhs._nackCount),
_latestPacketTimeMs(rhs._latestPacketTimeMs)
{
    _sessionInfo = rhs._sessionInfo;
    _sessionInfo.UpdateDataPointers(rhs._buffer, _buffer);
}

webrtc::FrameType
VCMFrameBuffer::FrameType() const
{
    return _sessionInfo.FrameType();
}

void
VCMFrameBuffer::SetPreviousFrameLoss()
{
    _sessionInfo.SetPreviousFrameLoss();
}

WebRtc_Word32
VCMFrameBuffer::GetLowSeqNum() const
{
    return _sessionInfo.LowSequenceNumber();
}

WebRtc_Word32
VCMFrameBuffer::GetHighSeqNum() const
{
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
VCMFrameBuffer::IsSessionComplete() const
{
    return _sessionInfo.complete();
}

// Insert packet
VCMFrameBufferEnum
VCMFrameBuffer::InsertPacket(const VCMPacket& packet, WebRtc_Word64 timeInMs,
                             bool enableDecodableState, WebRtc_UWord32 rttMS)
{
    if (_state == kStateDecoding)
    {
        // Do not insert packet
        return kNoError;
    }

    // Sanity to check if the frame has been freed. (Too old for example)
    if (_state == kStateFree)
    {
        return kStateError;
    }

    // is this packet part of this frame
    if (TimeStamp() && (TimeStamp() != packet.timestamp))
    {
        return kTimeStampError;
    }

    // sanity checks
    if (_size + packet.sizeBytes +
        (packet.insertStartCode ?  kH264StartCodeLengthBytes : 0 )
        > kMaxJBFrameSizeBytes)
    {
        return kSizeError;
    }
    if (NULL == packet.dataPtr && packet.sizeBytes > 0)
    {
        return kSizeError;
    }
    if (packet.dataPtr != NULL)
    {
        _payloadType = packet.payloadType;
    }

    if (kStateEmpty == _state)
    {
        // First packet (empty and/or media) inserted into this frame.
        // store some info and set some initial values.
        _timeStamp = packet.timestamp;
        _codec = packet.codec;
        if (packet.frameType != kFrameEmpty)
        {
            // first media packet
            SetState(kStateIncomplete);
        }
    }

    WebRtc_UWord32 requiredSizeBytes = Length() + packet.sizeBytes +
                   (packet.insertStartCode ? kH264StartCodeLengthBytes : 0);
    if (requiredSizeBytes >= _size)
    {
        const WebRtc_UWord8* prevBuffer = _buffer;
        const WebRtc_UWord32 increments = requiredSizeBytes /
                                          kBufferIncStepSizeBytes +
                                        (requiredSizeBytes %
                                         kBufferIncStepSizeBytes > 0);
        const WebRtc_UWord32 newSize = _size +
                                       increments * kBufferIncStepSizeBytes;
        if (newSize > kMaxJBFrameSizeBytes)
        {
            return kSizeError;
        }
        if (VerifyAndAllocate(newSize) == -1)
        {
            return kSizeError;
        }
        _sessionInfo.UpdateDataPointers(prevBuffer, _buffer);
    }

    CopyCodecSpecific(&packet.codecSpecificHeader);

    int retVal = _sessionInfo.InsertPacket(packet, _buffer,
                                           enableDecodableState,
                                           rttMS);
    if (retVal == -1)
    {
        return kSizeError;
    }
    else if (retVal == -2)
    {
        return kDuplicatePacket;
    }
    // update length
    _length = Length() + static_cast<WebRtc_UWord32>(retVal);

    _latestPacketTimeMs = timeInMs;

    if (_sessionInfo.complete()) {
      return kCompleteSession;
    } else if (_sessionInfo.decodable()) {
      SetState(kStateDecodable);
      return kDecodableSession;
    } else {
      // this layer is not complete
      if (_state == kStateComplete) {
        // we already have a complete layer
        // wait for all independent layers belonging to the same frame
        _state = kStateIncomplete;
      }
    }
    return kIncomplete;
}

WebRtc_Word64
VCMFrameBuffer::LatestPacketTimeMs()
{
    return _latestPacketTimeMs;
}

// Build hard NACK list:Zero out all entries in list up to and including the
// (first) entry equal to _lowSeqNum.
int VCMFrameBuffer::BuildHardNackList(int* list, int num) {
  if (_sessionInfo.BuildHardNackList(list, num) != 0) {
   return -1;
  }
  return 0;
}

// Build selective NACK list: Create a soft (selective) list of entries to zero
// out up to and including the (first) entry equal to _lowSeqNum.
int VCMFrameBuffer::BuildSoftNackList(int* list, int num, int rttMs) {
  return _sessionInfo.BuildSoftNackList(list, num, rttMs);
}

void
VCMFrameBuffer::IncrementNackCount()
{
    _nackCount++;
}

WebRtc_Word16
VCMFrameBuffer::GetNackCount() const
{
    return _nackCount;
}

bool
VCMFrameBuffer::HaveLastPacket() const
{
    return _sessionInfo.HaveLastPacket();
}

void
VCMFrameBuffer::Reset()
{
    _length = 0;
    _timeStamp = 0;
    _sessionInfo.Reset();
    _frameCounted = false;
    _payloadType = 0;
    _nackCount = 0;
    _latestPacketTimeMs = -1;
    _state = kStateFree;
    VCMEncodedFrame::Reset();
}

// Makes sure the session contains a decodable stream.
void
VCMFrameBuffer::MakeSessionDecodable()
{
    WebRtc_UWord32 retVal;
#ifdef INDEPENDENT_PARTITIONS
    if (_codec != kVideoCodecVP8) {
        retVal = _sessionInfo.MakeDecodable();
        _length -= retVal;
    }
#else
    retVal = _sessionInfo.MakeDecodable();
    _length -= retVal;
#endif
}

// Set state of frame
void
VCMFrameBuffer::SetState(VCMFrameBufferStateEnum state)
{
    if (_state == state)
    {
        return;
    }
    switch (state)
    {
    case kStateFree:
        // Reset everything
        // We can go to this state from all other states.
        // The one setting the state to free must ensure
        // that the frame is removed from the timestamp
        // ordered frame list in the jb.
        Reset();
        break;

    case kStateIncomplete:
        // we can go to this state from state kStateEmpty
        assert(_state == kStateEmpty ||
            _state == kStateDecoding);

        // Do nothing, we received a packet
        break;

    case kStateComplete:
        assert(_state == kStateEmpty ||
               _state == kStateIncomplete ||
               _state == kStateDecodable);

        break;

    case kStateEmpty:
        assert(_state == kStateFree);
        // Do nothing
        break;

    case kStateDecoding:
        // A frame might have received empty packets, or media packets might
        // have been removed when making the frame decodable. The frame can
        // still be set to decodable since it can be used to inform the
        // decoder of a frame loss.
        assert(_state == kStateComplete || _state == kStateIncomplete ||
               _state == kStateDecodable || _state == kStateEmpty);
        // Transfer frame information to EncodedFrame and create any codec
        // specific information
        RestructureFrameInformation();
        break;

    case kStateDecodable:
        assert(_state == kStateEmpty ||
               _state == kStateIncomplete);
        break;
    }
    _state = state;
}

void
VCMFrameBuffer::RestructureFrameInformation()
{
    PrepareForDecode();
    _frameType = ConvertFrameType(_sessionInfo.FrameType());
    _completeFrame = _sessionInfo.complete();
    _missingFrame = _sessionInfo.PreviousFrameLoss();
}

WebRtc_Word32
VCMFrameBuffer::ExtractFromStorage(const EncodedVideoData& frameFromStorage)
{
    _frameType = ConvertFrameType(frameFromStorage.frameType);
    _timeStamp = frameFromStorage.timeStamp;
    _payloadType = frameFromStorage.payloadType;
    _encodedWidth = frameFromStorage.encodedWidth;
    _encodedHeight = frameFromStorage.encodedHeight;
    _missingFrame = frameFromStorage.missingFrame;
    _completeFrame = frameFromStorage.completeFrame;
    _renderTimeMs = frameFromStorage.renderTimeMs;
    _codec = frameFromStorage.codec;
    const WebRtc_UWord8 *prevBuffer = _buffer;
    if (VerifyAndAllocate(frameFromStorage.payloadSize) < 0)
    {
        return VCM_MEMORY;
    }
    _sessionInfo.UpdateDataPointers(prevBuffer, _buffer);
    memcpy(_buffer, frameFromStorage.payloadData, frameFromStorage.payloadSize);
    _length = frameFromStorage.payloadSize;
    return VCM_OK;
}

int VCMFrameBuffer::NotDecodablePackets() const {
  return _sessionInfo.packets_not_decodable();
}

// Set counted status (as counted by JB or not)
void VCMFrameBuffer::SetCountedFrame(bool frameCounted)
{
    _frameCounted = frameCounted;
}

bool VCMFrameBuffer::GetCountedFrame() const
{
    return _frameCounted;
}

// Get current state of frame
VCMFrameBufferStateEnum
VCMFrameBuffer::GetState() const
{
    return _state;
}

// Get current state of frame
VCMFrameBufferStateEnum
VCMFrameBuffer::GetState(WebRtc_UWord32& timeStamp) const
{
    timeStamp = TimeStamp();
    return GetState();
}

bool
VCMFrameBuffer::IsRetransmitted() const
{
    return _sessionInfo.session_nack();
}

void
VCMFrameBuffer::PrepareForDecode()
{
#ifdef INDEPENDENT_PARTITIONS
    if (_codec == kVideoCodecVP8)
    {
        _length =
            _sessionInfo.BuildVP8FragmentationHeader(_buffer, _length,
                                                     &_fragmentation);
    }
#endif
}

}
