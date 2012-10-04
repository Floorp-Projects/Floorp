/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/video_coding/main/source/jitter_buffer.h"

#include <algorithm>
#include <cassert>

#include "modules/video_coding/main/source/event.h"
#include "modules/video_coding/main/source/frame_buffer.h"
#include "modules/video_coding/main/source/inter_frame_delay.h"
#include "modules/video_coding/main/source/internal_defines.h"
#include "modules/video_coding/main/source/jitter_buffer_common.h"
#include "modules/video_coding/main/source/jitter_estimator.h"
#include "modules/video_coding/main/source/packet.h"
#include "modules/video_coding/main/source/tick_time_base.h"
#include "system_wrappers/interface/critical_section_wrapper.h"
#include "system_wrappers/interface/trace.h"

namespace webrtc {

// Predicates used when searching for frames in the frame buffer list
class FrameSmallerTimestamp {
 public:
  FrameSmallerTimestamp(uint32_t timestamp) : timestamp_(timestamp) {}
  bool operator()(VCMFrameBuffer* frame) {
    return (LatestTimestamp(timestamp_, frame->TimeStamp(), NULL) ==
        timestamp_);
  }

 private:
  uint32_t timestamp_;
};

class FrameEqualTimestamp {
 public:
  FrameEqualTimestamp(uint32_t timestamp) : timestamp_(timestamp) {}
  bool operator()(VCMFrameBuffer* frame) {
    return (timestamp_ == frame->TimeStamp());
  }

 private:
  uint32_t timestamp_;
};

class CompleteDecodableKeyFrameCriteria {
 public:
  bool operator()(VCMFrameBuffer* frame) {
    return (frame->FrameType() == kVideoFrameKey) &&
           (frame->GetState() == kStateComplete ||
            frame->GetState() == kStateDecodable);
  }
};

// Constructor
VCMJitterBuffer::VCMJitterBuffer(TickTimeBase* clock,
                                 WebRtc_Word32 vcmId,
                                 WebRtc_Word32 receiverId,
                                 bool master) :
    _vcmId(vcmId),
    _receiverId(receiverId),
    _clock(clock),
    _running(false),
    _critSect(CriticalSectionWrapper::CreateCriticalSection()),
    _master(master),
    _frameEvent(),
    _packetEvent(),
    _maxNumberOfFrames(kStartNumberOfFrames),
    _frameBuffers(),
    _frameList(),
    _lastDecodedState(),
    _packetsNotDecodable(0),
    _receiveStatistics(),
    _incomingFrameRate(0),
    _incomingFrameCount(0),
    _timeLastIncomingFrameCount(0),
    _incomingBitCount(0),
    _incomingBitRate(0),
    _dropCount(0),
    _numConsecutiveOldFrames(0),
    _numConsecutiveOldPackets(0),
    _discardedPackets(0),
    _jitterEstimate(vcmId, receiverId),
    _delayEstimate(_clock->MillisecondTimestamp()),
    _rttMs(0),
    _nackMode(kNoNack),
    _lowRttNackThresholdMs(-1),
    _highRttNackThresholdMs(-1),
    _NACKSeqNum(),
    _NACKSeqNumLength(0),
    _waitingForKeyFrame(false),
    _firstPacket(true)
{
    memset(_frameBuffers, 0, sizeof(_frameBuffers));
    memset(_receiveStatistics, 0, sizeof(_receiveStatistics));
    memset(_NACKSeqNumInternal, -1, sizeof(_NACKSeqNumInternal));

    for (int i = 0; i< kStartNumberOfFrames; i++)
    {
        _frameBuffers[i] = new VCMFrameBuffer();
    }
}

// Destructor
VCMJitterBuffer::~VCMJitterBuffer()
{
    Stop();
    for (int i = 0; i< kMaxNumberOfFrames; i++)
    {
        if (_frameBuffers[i])
        {
            delete _frameBuffers[i];
        }
    }
    delete _critSect;
}

void
VCMJitterBuffer::CopyFrom(const VCMJitterBuffer& rhs)
{
    if (this != &rhs)
    {
        _critSect->Enter();
        rhs._critSect->Enter();
        _vcmId = rhs._vcmId;
        _receiverId = rhs._receiverId;
        _running = rhs._running;
        _master = !rhs._master;
        _maxNumberOfFrames = rhs._maxNumberOfFrames;
        _incomingFrameRate = rhs._incomingFrameRate;
        _incomingFrameCount = rhs._incomingFrameCount;
        _timeLastIncomingFrameCount = rhs._timeLastIncomingFrameCount;
        _incomingBitCount = rhs._incomingBitCount;
        _incomingBitRate = rhs._incomingBitRate;
        _dropCount = rhs._dropCount;
        _numConsecutiveOldFrames = rhs._numConsecutiveOldFrames;
        _numConsecutiveOldPackets = rhs._numConsecutiveOldPackets;
        _discardedPackets = rhs._discardedPackets;
        _jitterEstimate = rhs._jitterEstimate;
        _delayEstimate = rhs._delayEstimate;
        _waitingForCompletion = rhs._waitingForCompletion;
        _rttMs = rhs._rttMs;
        _NACKSeqNumLength = rhs._NACKSeqNumLength;
        _waitingForKeyFrame = rhs._waitingForKeyFrame;
        _firstPacket = rhs._firstPacket;
        _lastDecodedState =  rhs._lastDecodedState;
        _packetsNotDecodable = rhs._packetsNotDecodable;
        memcpy(_receiveStatistics, rhs._receiveStatistics,
               sizeof(_receiveStatistics));
        memcpy(_NACKSeqNumInternal, rhs._NACKSeqNumInternal,
               sizeof(_NACKSeqNumInternal));
        memcpy(_NACKSeqNum, rhs._NACKSeqNum, sizeof(_NACKSeqNum));
        for (int i = 0; i < kMaxNumberOfFrames; i++)
        {
            if (_frameBuffers[i] != NULL)
            {
                delete _frameBuffers[i];
                _frameBuffers[i] = NULL;
            }
        }
        _frameList.clear();
        for (int i = 0; i < _maxNumberOfFrames; i++)
        {
            _frameBuffers[i] = new VCMFrameBuffer(*(rhs._frameBuffers[i]));
            if (_frameBuffers[i]->Length() > 0)
            {
                FrameList::reverse_iterator rit = std::find_if(
                    _frameList.rbegin(), _frameList.rend(),
                    FrameSmallerTimestamp(_frameBuffers[i]->TimeStamp()));
                _frameList.insert(rit.base(), _frameBuffers[i]);
            }
        }
        rhs._critSect->Leave();
        _critSect->Leave();
    }
}

// Start jitter buffer
void
VCMJitterBuffer::Start()
{
    CriticalSectionScoped cs(_critSect);
    _running = true;
    _incomingFrameCount = 0;
    _incomingFrameRate = 0;
    _incomingBitCount = 0;
    _incomingBitRate = 0;
    _timeLastIncomingFrameCount = _clock->MillisecondTimestamp();
    memset(_receiveStatistics, 0, sizeof(_receiveStatistics));

    _numConsecutiveOldFrames = 0;
    _numConsecutiveOldPackets = 0;
    _discardedPackets = 0;

    _frameEvent.Reset(); // start in a non-signaled state
    _packetEvent.Reset(); // start in a non-signaled state
    _waitingForCompletion.frameSize = 0;
    _waitingForCompletion.timestamp = 0;
    _waitingForCompletion.latestPacketTime = -1;
    _firstPacket = true;
    _NACKSeqNumLength = 0;
    _waitingForKeyFrame = false;
    _rttMs = 0;
    _packetsNotDecodable = 0;

    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding, VCMId(_vcmId,
                 _receiverId), "JB(0x%x): Jitter buffer: start", this);
}


// Stop jitter buffer
void
VCMJitterBuffer::Stop()
{
    _critSect->Enter();
    _running = false;
    _lastDecodedState.Reset();
    _frameList.clear();
    for (int i = 0; i < kMaxNumberOfFrames; i++)
    {
        if (_frameBuffers[i] != NULL)
        {
            static_cast<VCMFrameBuffer*>(_frameBuffers[i])->SetState(kStateFree);
        }
    }

    _critSect->Leave();
    _frameEvent.Set(); // Make sure we exit from trying to get a frame to decoder
    _packetEvent.Set(); // Make sure we exit from trying to get a sequence number
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding, VCMId(_vcmId,
                 _receiverId), "JB(0x%x): Jitter buffer: stop", this);
}

bool
VCMJitterBuffer::Running() const
{
    CriticalSectionScoped cs(_critSect);
    return _running;
}

// Flush jitter buffer
void
VCMJitterBuffer::Flush()
{
    CriticalSectionScoped cs(_critSect);
    FlushInternal();
}

// Must be called under the critical section _critSect
void
VCMJitterBuffer::FlushInternal()
{
    // Erase all frames from the sorted list and set their state to free.
    _frameList.clear();
    for (WebRtc_Word32 i = 0; i < _maxNumberOfFrames; i++)
    {
        ReleaseFrameInternal(_frameBuffers[i]);
    }
    _lastDecodedState.Reset(); // TODO (mikhal): sync reset
    _packetsNotDecodable = 0;

    _frameEvent.Reset();
    _packetEvent.Reset();

    _numConsecutiveOldFrames = 0;
    _numConsecutiveOldPackets = 0;

    // Also reset the jitter and delay estimates
    _jitterEstimate.Reset();
    _delayEstimate.Reset(_clock->MillisecondTimestamp());

    _waitingForCompletion.frameSize = 0;
    _waitingForCompletion.timestamp = 0;
    _waitingForCompletion.latestPacketTime = -1;

    _firstPacket = true;

    _NACKSeqNumLength = 0;

    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding, VCMId(_vcmId,
                 _receiverId), "JB(0x%x): Jitter buffer: flush", this);
}

// Set the frame state to free and remove it from the sorted
// frame list. Must be called from inside the critical section _critSect.
void
VCMJitterBuffer::ReleaseFrameInternal(VCMFrameBuffer* frame)
{
    if (frame != NULL && frame->GetState() != kStateDecoding)
    {
        frame->SetState(kStateFree);
    }
}

// Update frame state (set as complete if conditions are met)
// Doing it here increases the degree of freedom for e.g. future
// reconstructability of separate layers. Must be called under the
// critical section _critSect.
VCMFrameBufferEnum
VCMJitterBuffer::UpdateFrameState(VCMFrameBuffer* frame)
{
    if (frame == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideoCoding,
                     VCMId(_vcmId, _receiverId), "JB(0x%x) FB(0x%x): "
                         "UpdateFrameState NULL frame pointer", this, frame);
        return kNoError;
    }

    int length = frame->Length();
    if (_master)
    {
        // Only trace the primary jitter buffer to make it possible to parse
        // and plot the trace file.
        WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding,
                     VCMId(_vcmId, _receiverId),
                     "JB(0x%x) FB(0x%x): Complete frame added to jitter buffer,"
                     " size:%d type %d",
                     this, frame,length,frame->FrameType());
    }

    if (length != 0 && !frame->GetCountedFrame())
    {
        // ignore Ack frames
        _incomingFrameCount++;
        frame->SetCountedFrame(true);
    }

    // Check if we should drop frame
    // an old complete frame can arrive too late
    if (_lastDecodedState.IsOldFrame(frame))
    {
        // Frame is older than the latest decoded frame, drop it. Will be
        // released by CleanUpOldFrames later.
        frame->Reset();
        frame->SetState(kStateEmpty);

        WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding,
                     VCMId(_vcmId, _receiverId),
                     "JB(0x%x) FB(0x%x): Dropping old frame in Jitter buffer",
                     this, frame);
        _dropCount++;
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideoCoding,
                     VCMId(_vcmId, _receiverId),
                     "Jitter buffer drop count: %d, consecutive drops: %u",
                     _dropCount, _numConsecutiveOldFrames);
        // Flush() if this happens consistently.
        _numConsecutiveOldFrames++;
        if (_numConsecutiveOldFrames > kMaxConsecutiveOldFrames) {
          FlushInternal();
          return kFlushIndicator;
        }
        return kNoError;
    }
    _numConsecutiveOldFrames = 0;
    frame->SetState(kStateComplete);


    // Update receive statistics. We count all layers, thus when you use layers
    // adding all key and delta frames might differ from frame count
    if (frame->IsSessionComplete())
    {
        switch (frame->FrameType())
        {
        case kVideoFrameKey:
            {
                _receiveStatistics[0]++;
                break;
            }
        case kVideoFrameDelta:
            {
                _receiveStatistics[1]++;
                break;
            }
        case kVideoFrameGolden:
            {
                _receiveStatistics[2]++;
                break;
            }
        case kVideoFrameAltRef:
            {
                _receiveStatistics[3]++;
                break;
            }
        default:
            assert(false);

        }
    }
    const FrameList::iterator it = FindOldestCompleteContinuousFrame(false);
    VCMFrameBuffer* oldFrame = NULL;
    if (it != _frameList.end())
    {
        oldFrame = *it;
    }

    // Only signal if this is the oldest frame.
    // Not necessary the case due to packet reordering or NACK.
    if (!WaitForNack() || (oldFrame != NULL && oldFrame == frame))
    {
        _frameEvent.Set();
    }
    return kNoError;
}

// Get received key and delta frames
WebRtc_Word32
VCMJitterBuffer::GetFrameStatistics(WebRtc_UWord32& receivedDeltaFrames,
                                    WebRtc_UWord32& receivedKeyFrames) const
{
    {
        CriticalSectionScoped cs(_critSect);
        receivedDeltaFrames = _receiveStatistics[1] + _receiveStatistics[3];
        receivedKeyFrames = _receiveStatistics[0] + _receiveStatistics[2];
    }
    return 0;
}

WebRtc_UWord32 VCMJitterBuffer::NumNotDecodablePackets() const {
  CriticalSectionScoped cs(_critSect);
  return _packetsNotDecodable;
}

WebRtc_UWord32 VCMJitterBuffer::DiscardedPackets() const {
  CriticalSectionScoped cs(_critSect);
  return _discardedPackets;
}

// Gets frame to use for this timestamp. If no match, get empty frame.
WebRtc_Word32
VCMJitterBuffer::GetFrame(const VCMPacket& packet, VCMEncodedFrame*& frame)
{
    if (!_running) // don't accept incoming packets until we are started
    {
        return VCM_UNINITIALIZED;
    }

    _critSect->Enter();
    // Does this packet belong to an old frame?
    if (_lastDecodedState.IsOldPacket(&packet))
    {
        // Account only for media packets
        if (packet.sizeBytes > 0)
        {
            _discardedPackets++;
            _numConsecutiveOldPackets++;
        }
        // Update last decoded sequence number if the packet arrived late and
        // belongs to a frame with a timestamp equal to the last decoded
        // timestamp.
        _lastDecodedState.UpdateOldPacket(&packet);

        if (_numConsecutiveOldPackets > kMaxConsecutiveOldPackets)
        {
            FlushInternal();
            _critSect->Leave();
            return VCM_FLUSH_INDICATOR;
        }
        _critSect->Leave();
        return VCM_OLD_PACKET_ERROR;
    }
    _numConsecutiveOldPackets = 0;

    FrameList::iterator it = std::find_if(
        _frameList.begin(),
        _frameList.end(),
        FrameEqualTimestamp(packet.timestamp));

    if (it != _frameList.end()) {
      frame = *it;
      _critSect->Leave();
      return VCM_OK;
    }

    _critSect->Leave();

    // No match, return empty frame
    frame = GetEmptyFrame();
    if (frame != NULL)
    {
        return VCM_OK;
    }
    // No free frame! Try to reclaim some...
    _critSect->Enter();
    RecycleFramesUntilKeyFrame();
    _critSect->Leave();

    frame = GetEmptyFrame();
    if (frame != NULL)
    {
        return VCM_OK;
    }
    return VCM_JITTER_BUFFER_ERROR;
}

// Deprecated! Kept for testing purposes.
VCMEncodedFrame*
VCMJitterBuffer::GetFrame(const VCMPacket& packet)
{
    VCMEncodedFrame* frame = NULL;
    if (GetFrame(packet, frame) < 0)
    {
        return NULL;
    }
    return frame;
}

// Get empty frame, creates new (i.e. increases JB size) if necessary
VCMFrameBuffer*
VCMJitterBuffer::GetEmptyFrame()
{
    if (!_running) // don't accept incoming packets until we are started
    {
        return NULL;
    }

    _critSect->Enter();

    for (int i = 0; i <_maxNumberOfFrames; ++i)
    {
        if (kStateFree == _frameBuffers[i]->GetState())
        {
            // found a free buffer
            _frameBuffers[i]->SetState(kStateEmpty);
            _critSect->Leave();
            return _frameBuffers[i];
        }
    }

    // Check if we can increase JB size
    if (_maxNumberOfFrames < kMaxNumberOfFrames)
    {
        VCMFrameBuffer* ptrNewBuffer = new VCMFrameBuffer();
        ptrNewBuffer->SetState(kStateEmpty);
        _frameBuffers[_maxNumberOfFrames] = ptrNewBuffer;
        _maxNumberOfFrames++;

        _critSect->Leave();
        WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding,
        VCMId(_vcmId, _receiverId), "JB(0x%x) FB(0x%x): Jitter buffer "
        "increased to:%d frames", this, ptrNewBuffer, _maxNumberOfFrames);
        return ptrNewBuffer;
    }
    _critSect->Leave();

    // We have reached max size, cannot increase JB size
    return NULL;
}


// Find oldest complete frame used for getting next frame to decode
// Must be called under critical section
FrameList::iterator
VCMJitterBuffer::FindOldestCompleteContinuousFrame(bool enable_decodable) {
  // If we have more than one frame done since last time, pick oldest.
  VCMFrameBuffer* oldest_frame = NULL;
  FrameList::iterator it = _frameList.begin();

  // When temporal layers are available, we search for a complete or decodable
  // frame until we hit one of the following:
  // 1. Continuous base or sync layer.
  // 2. The end of the list was reached.
  for (; it != _frameList.end(); ++it)  {
    oldest_frame = *it;
    VCMFrameBufferStateEnum state = oldest_frame->GetState();
    // Is this frame complete or decodable and continuous?
    if ((state == kStateComplete ||
        (enable_decodable && state == kStateDecodable)) &&
        _lastDecodedState.ContinuousFrame(oldest_frame)) {
      break;
    } else {
      int temporal_id = oldest_frame->TemporalId();
      oldest_frame = NULL;
      if (temporal_id <= 0) {
        // When temporal layers are disabled or we have hit a base layer
        // we break (regardless of continuity and completeness).
        break;
      }
    }
  }

  if (oldest_frame == NULL) {
    // No complete frame no point to continue.
    return _frameList.end();
  } else  if (_waitingForKeyFrame &&
              oldest_frame->FrameType() != kVideoFrameKey) {
    // We are waiting for a key frame.
    return _frameList.end();
  }

  // We have a complete continuous frame.
  return it;
}

// Call from inside the critical section _critSect
void
VCMJitterBuffer::RecycleFrame(VCMFrameBuffer* frame)
{
    if (frame == NULL)
    {
        return;
    }

    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding,
                 VCMId(_vcmId, _receiverId),
                 "JB(0x%x) FB(0x%x): RecycleFrame, size:%d",
                 this, frame, frame->Length());

    ReleaseFrameInternal(frame);
}

// Calculate frame and bit rates
WebRtc_Word32
VCMJitterBuffer::GetUpdate(WebRtc_UWord32& frameRate, WebRtc_UWord32& bitRate)
{
    CriticalSectionScoped cs(_critSect);
    const WebRtc_Word64 now = _clock->MillisecondTimestamp();
    WebRtc_Word64 diff = now - _timeLastIncomingFrameCount;
    if (diff < 1000 && _incomingFrameRate > 0 && _incomingBitRate > 0)
    {
        // Make sure we report something even though less than
        // 1 second has passed since last update.
        frameRate = _incomingFrameRate;
        bitRate = _incomingBitRate;
    }
    else if (_incomingFrameCount != 0)
    {
        // We have received frame(s) since last call to this function

        // Prepare calculations
        if (diff <= 0)
        {
            diff = 1;
        }
        // we add 0.5f for rounding
        float rate = 0.5f + ((_incomingFrameCount * 1000.0f) / diff);
        if (rate < 1.0f) // don't go below 1, can crash
        {
            rate = 1.0f;
        }

        // Calculate frame rate
        // Let r be rate.
        // r(0) = 1000*framecount/delta_time.
        // (I.e. frames per second since last calculation.)
        // frameRate = r(0)/2 + r(-1)/2
        // (I.e. fr/s average this and the previous calculation.)
        frameRate = (_incomingFrameRate + (WebRtc_Word32)rate) >> 1;
        _incomingFrameRate = (WebRtc_UWord8)rate;

        // Calculate bit rate
        if (_incomingBitCount == 0)
        {
            bitRate = 0;
        }
        else
        {
            bitRate = 10 * ((100 * _incomingBitCount) /
                      static_cast<WebRtc_UWord32>(diff));
        }
        _incomingBitRate = bitRate;

        // Reset count
        _incomingFrameCount = 0;
        _incomingBitCount = 0;
        _timeLastIncomingFrameCount = now;

    }
    else
    {
        // No frames since last call
        _timeLastIncomingFrameCount = _clock->MillisecondTimestamp();
        frameRate = 0;
        bitRate = 0;
        _incomingBitRate = 0;
    }

    return 0;
}

// Returns immediately or a X ms event hang waiting for a complete frame,
// X decided by caller
VCMEncodedFrame*
VCMJitterBuffer::GetCompleteFrameForDecoding(WebRtc_UWord32 maxWaitTimeMS)
{
    if (!_running)
    {
        return NULL;
    }

    _critSect->Enter();

    CleanUpOldFrames();

    if (_lastDecodedState.init() && WaitForNack()) {
      _waitingForKeyFrame = true;
    }

    FrameList::iterator it = FindOldestCompleteContinuousFrame(false);
    if (it == _frameList.end())
    {
        if (maxWaitTimeMS == 0)
        {
            _critSect->Leave();
            return NULL;
        }
        const WebRtc_Word64 endWaitTimeMs = _clock->MillisecondTimestamp()
                                            + maxWaitTimeMS;
        WebRtc_Word64 waitTimeMs = maxWaitTimeMS;
        while (waitTimeMs > 0)
        {
            _critSect->Leave();
            const EventTypeWrapper ret =
                  _frameEvent.Wait(static_cast<WebRtc_UWord32>(waitTimeMs));
            _critSect->Enter();
            if (ret == kEventSignaled)
            {
                // are we closing down the Jitter buffer
                if (!_running)
                {
                    _critSect->Leave();
                    return NULL;
                }

                // Finding oldest frame ready for decoder, but check
                // sequence number and size
                CleanUpOldFrames();
                it = FindOldestCompleteContinuousFrame(false);
                if (it == _frameList.end())
                {
                    waitTimeMs = endWaitTimeMs -
                                 _clock->MillisecondTimestamp();
                }
                else
                {
                    break;
                }
            }
            else
            {
                _critSect->Leave();
                return NULL;
            }
        }
        // Inside critSect
    }
    else
    {
        // we already have a frame reset the event
        _frameEvent.Reset();
    }

    if (it == _frameList.end())
    {
        // Even after signaling we're still missing a complete continuous frame
        _critSect->Leave();
        return NULL;
    }

    VCMFrameBuffer* oldestFrame = *it;
    it = _frameList.erase(it);

    // Update jitter estimate
    const bool retransmitted = (oldestFrame->GetNackCount() > 0);
    if (retransmitted)
    {
        _jitterEstimate.FrameNacked();
    }
    else if (oldestFrame->Length() > 0)
    {
        // Ignore retransmitted and empty frames.
        UpdateJitterAndDelayEstimates(*oldestFrame, false);
    }

    oldestFrame->SetState(kStateDecoding);

    CleanUpOldFrames();

    if (oldestFrame->FrameType() == kVideoFrameKey)
    {
        _waitingForKeyFrame = false;
    }

    _critSect->Leave();

    // We have a frame - update decoded state with frame info.
    _lastDecodedState.SetState(oldestFrame);

    return oldestFrame;
}

WebRtc_UWord32
VCMJitterBuffer::GetEstimatedJitterMS()
{
    CriticalSectionScoped cs(_critSect);
    return GetEstimatedJitterMsInternal();
}

WebRtc_UWord32
VCMJitterBuffer::GetEstimatedJitterMsInternal()
{
    WebRtc_UWord32 estimate = VCMJitterEstimator::OPERATING_SYSTEM_JITTER;

    // Compute RTT multiplier for estimation
    // _lowRttNackThresholdMs == -1 means no FEC.
    double rttMult = 1.0f;
    if (_nackMode == kNackHybrid && (_lowRttNackThresholdMs >= 0 &&
        static_cast<int>(_rttMs) > _lowRttNackThresholdMs))
    {
        // from here we count on FEC
        rttMult = 0.0f;
    }
    estimate += static_cast<WebRtc_UWord32>
                (_jitterEstimate.GetJitterEstimate(rttMult) + 0.5);
    return estimate;
}

void
VCMJitterBuffer::UpdateRtt(WebRtc_UWord32 rttMs)
{
    CriticalSectionScoped cs(_critSect);
    _rttMs = rttMs;
    _jitterEstimate.UpdateRtt(rttMs);
}

// wait for the first packet in the next frame to arrive
WebRtc_Word64
VCMJitterBuffer::GetNextTimeStamp(WebRtc_UWord32 maxWaitTimeMS,
                                  FrameType& incomingFrameType,
                                  WebRtc_Word64& renderTimeMs)
{
    if (!_running)
    {
        return -1;
    }

    _critSect->Enter();

    // Finding oldest frame ready for decoder, check sequence number and size
    CleanUpOldFrames();

    FrameList::iterator it = _frameList.begin();

    if (it == _frameList.end())
    {
        _packetEvent.Reset();
        _critSect->Leave();

        if (_packetEvent.Wait(maxWaitTimeMS) == kEventSignaled)
        {
            // are we closing down the Jitter buffer
            if (!_running)
            {
                return -1;
            }
            _critSect->Enter();

            CleanUpOldFrames();
            it = _frameList.begin();
        }
        else
        {
            _critSect->Enter();
        }
    }

    if (it == _frameList.end())
    {
        _critSect->Leave();
        return -1;
    }
    // we have a frame

    // return frame type
    // All layers are assumed to have the same type
    incomingFrameType = (*it)->FrameType();

    renderTimeMs = (*it)->RenderTimeMs();

    const WebRtc_UWord32 timestamp = (*it)->TimeStamp();

    _critSect->Leave();

    // return current time
    return timestamp;
}

// Answers the question:
// Will the packet sequence be complete if the next frame is grabbed for
// decoding right now? That is, have we lost a frame between the last decoded
// frame and the next, or is the next
// frame missing one or more packets?
bool
VCMJitterBuffer::CompleteSequenceWithNextFrame()
{
    CriticalSectionScoped cs(_critSect);
    // Finding oldest frame ready for decoder, check sequence number and size
    CleanUpOldFrames();

    if (_frameList.empty())
      return true;

    VCMFrameBuffer* oldestFrame = _frameList.front();
    if (_frameList.size() <= 1 &&
        oldestFrame->GetState() != kStateComplete)
    {
        // Frame not ready to be decoded.
        return true;
    }
    if (!oldestFrame->Complete())
    {
        return false;
    }

    // See if we have lost a frame before this one.
    if (_lastDecodedState.init())
    {
        // Following start, reset or flush -> check for key frame.
        if (oldestFrame->FrameType() != kVideoFrameKey)
        {
            return false;
        }
    }
    else if (oldestFrame->GetLowSeqNum() == -1)
    {
        return false;
    }
    else if (!_lastDecodedState.ContinuousFrame(oldestFrame))
    {
        return false;
    }
    return true;
}

// Returns immediately
VCMEncodedFrame*
VCMJitterBuffer::GetFrameForDecoding()
{
    CriticalSectionScoped cs(_critSect);
    if (!_running)
    {
        return NULL;
    }

    if (WaitForNack())
    {
        return GetFrameForDecodingNACK();
    }

    CleanUpOldFrames();

    if (_frameList.empty()) {
      return NULL;
    }

    VCMFrameBuffer* oldestFrame = _frameList.front();
    if (_frameList.size() <= 1 &&
        oldestFrame->GetState() != kStateComplete) {
      return NULL;
    }

    // Incomplete frame pulled out from jitter buffer,
    // update the jitter estimate with what we currently know.
    // This frame shouldn't have been retransmitted, but if we recently
    // turned off NACK this might still happen.
    const bool retransmitted = (oldestFrame->GetNackCount() > 0);
    if (retransmitted)
    {
        _jitterEstimate.FrameNacked();
    }
    else if (oldestFrame->Length() > 0)
    {
        // Ignore retransmitted and empty frames.
        // Update with the previous incomplete frame first
        if (_waitingForCompletion.latestPacketTime >= 0)
        {
            UpdateJitterAndDelayEstimates(_waitingForCompletion, true);
        }
        // Then wait for this one to get complete
        _waitingForCompletion.frameSize = oldestFrame->Length();
        _waitingForCompletion.latestPacketTime =
                              oldestFrame->LatestPacketTimeMs();
        _waitingForCompletion.timestamp = oldestFrame->TimeStamp();
    }
    _frameList.erase(_frameList.begin());

    // Look for previous frame loss
    VerifyAndSetPreviousFrameLost(*oldestFrame);

    // The state must be changed to decoding before cleaning up zero sized
    // frames to avoid empty frames being cleaned up and then given to the
    // decoder.
    // Set as decoding. Propagates the missingFrame bit.
    oldestFrame->SetState(kStateDecoding);

    CleanUpOldFrames();

    if (oldestFrame->FrameType() == kVideoFrameKey)
    {
        _waitingForKeyFrame = false;
    }

    _packetsNotDecodable += oldestFrame->NotDecodablePackets();

    // We have a frame - update decoded state with frame info.
    _lastDecodedState.SetState(oldestFrame);

    return oldestFrame;
}

VCMEncodedFrame*
VCMJitterBuffer::GetFrameForDecodingNACK()
{
    // when we use NACK we don't release non complete frames
    // unless we have a complete key frame.
    // In hybrid mode, we may release decodable frames (non-complete)

    // Clean up old frames and empty frames
    CleanUpOldFrames();

    // First look for a complete _continuous_ frame.
    // When waiting for nack, wait for a key frame, if a continuous frame cannot
    // be determined (i.e. initial decoding state).
    if (_lastDecodedState.init()) {
      _waitingForKeyFrame = true;
    }

    // Allow for a decodable frame when in Hybrid mode.
    bool enableDecodable = _nackMode == kNackHybrid ? true : false;
    FrameList::iterator it = FindOldestCompleteContinuousFrame(enableDecodable);
    if (it == _frameList.end())
    {
        // If we didn't find one we're good with a complete key/decodable frame.
        it = find_if(_frameList.begin(), _frameList.end(),
                     CompleteDecodableKeyFrameCriteria());
        if (it == _frameList.end())
        {
            return NULL;
        }
    }
    VCMFrameBuffer* oldestFrame = *it;
    // Update jitter estimate
    const bool retransmitted = (oldestFrame->GetNackCount() > 0);
    if (retransmitted)
    {
        _jitterEstimate.FrameNacked();
    }
    else if (oldestFrame->Length() > 0)
    {
        // Ignore retransmitted and empty frames.
        UpdateJitterAndDelayEstimates(*oldestFrame, false);
    }
    it = _frameList.erase(it);

    // Look for previous frame loss
    VerifyAndSetPreviousFrameLost(*oldestFrame);

    // The state must be changed to decoding before cleaning up zero sized
    // frames to avoid empty frames being cleaned up and then given to the
    // decoder.
    oldestFrame->SetState(kStateDecoding);

    // Clean up old frames and empty frames
    CleanUpOldFrames();

    if (oldestFrame->FrameType() == kVideoFrameKey)
    {
        _waitingForKeyFrame = false;
    }

    // We have a frame - update decoded state with frame info.
    _lastDecodedState.SetState(oldestFrame);

    return oldestFrame;
}

// Must be called under the critical section _critSect. Should never be called
// with retransmitted frames, they must be filtered out before this function is
// called.
void
VCMJitterBuffer::UpdateJitterAndDelayEstimates(VCMJitterSample& sample,
                                               bool incompleteFrame)
{
    if (sample.latestPacketTime == -1)
    {
        return;
    }
    if (incompleteFrame)
    {
        WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding,
                     VCMId(_vcmId, _receiverId), "Received incomplete frame "
                     "timestamp %u frame size %u at time %u",
                     sample.timestamp, sample.frameSize,
                     MaskWord64ToUWord32(sample.latestPacketTime));
    }
    else
    {
        WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding,
                     VCMId(_vcmId, _receiverId), "Received complete frame "
                     "timestamp %u frame size %u at time %u",
                     sample.timestamp, sample.frameSize,
                     MaskWord64ToUWord32(sample.latestPacketTime));
    }
    UpdateJitterAndDelayEstimates(sample.latestPacketTime,
                                  sample.timestamp,
                                  sample.frameSize,
                                  incompleteFrame);
}

// Must be called under the critical section _critSect. Should never be
// called with retransmitted frames, they must be filtered out before this
// function is called.
void
VCMJitterBuffer::UpdateJitterAndDelayEstimates(VCMFrameBuffer& frame,
                                               bool incompleteFrame)
{
    if (frame.LatestPacketTimeMs() == -1)
    {
        return;
    }
    // No retransmitted frames should be a part of the jitter
    // estimate.
    if (incompleteFrame)
    {
        WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding,
                     VCMId(_vcmId, _receiverId),
                   "Received incomplete frame timestamp %u frame type %d "
                   "frame size %u at time %u, jitter estimate was %u",
                   frame.TimeStamp(), frame.FrameType(), frame.Length(),
                   MaskWord64ToUWord32(frame.LatestPacketTimeMs()),
                   GetEstimatedJitterMsInternal());
    }
    else
    {
        WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding,
                     VCMId(_vcmId, _receiverId),"Received complete frame "
                     "timestamp %u frame type %d frame size %u at time %u, "
                     "jitter estimate was %u",
                     frame.TimeStamp(), frame.FrameType(), frame.Length(),
                     MaskWord64ToUWord32(frame.LatestPacketTimeMs()),
                     GetEstimatedJitterMsInternal());
    }
    UpdateJitterAndDelayEstimates(frame.LatestPacketTimeMs(), frame.TimeStamp(),
                                  frame.Length(), incompleteFrame);
}

// Must be called under the critical section _critSect. Should never be called
// with retransmitted frames, they must be filtered out before this function
// is called.
void
VCMJitterBuffer::UpdateJitterAndDelayEstimates(WebRtc_Word64 latestPacketTimeMs,
                                               WebRtc_UWord32 timestamp,
                                               WebRtc_UWord32 frameSize,
                                               bool incompleteFrame)
{
    if (latestPacketTimeMs == -1)
    {
        return;
    }
    WebRtc_Word64 frameDelay;
    // Calculate the delay estimate
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding,
                 VCMId(_vcmId, _receiverId),
                 "Packet received and sent to jitter estimate with: "
                 "timestamp=%u wallClock=%u", timestamp,
                 MaskWord64ToUWord32(latestPacketTimeMs));
    bool notReordered = _delayEstimate.CalculateDelay(timestamp,
                                                      &frameDelay,
                                                      latestPacketTimeMs);
    // Filter out frames which have been reordered in time by the network
    if (notReordered)
    {
        // Update the jitter estimate with the new samples
        _jitterEstimate.UpdateEstimate(frameDelay, frameSize, incompleteFrame);
    }
}

WebRtc_UWord16*
VCMJitterBuffer::GetNackList(WebRtc_UWord16& nackSize,bool& listExtended)
{
    return CreateNackList(nackSize,listExtended);
}

// Assume called internally with critsect
WebRtc_Word32
VCMJitterBuffer::GetLowHighSequenceNumbers(WebRtc_Word32& lowSeqNum,
                                           WebRtc_Word32& highSeqNum) const
{
    // TODO (mikhal/stefan): refactor to use lastDecodedState
    WebRtc_Word32 i = 0;
    WebRtc_Word32 seqNum = -1;

    highSeqNum = -1;
    lowSeqNum = -1;
    if (!_lastDecodedState.init())
      lowSeqNum = _lastDecodedState.sequence_num();

    // find highest seq numbers
    for (i = 0; i < _maxNumberOfFrames; ++i)
    {
        seqNum = _frameBuffers[i]->GetHighSeqNum();

        // Ignore free / empty frames
        VCMFrameBufferStateEnum state = _frameBuffers[i]->GetState();

        if ((kStateFree != state) &&
            (kStateEmpty != state) &&
            (kStateDecoding != state) &&
             seqNum != -1)
        {
            bool wrap;
            highSeqNum = LatestSequenceNumber(seqNum, highSeqNum, &wrap);
        }
    } // for
    return 0;
}


WebRtc_UWord16*
VCMJitterBuffer::CreateNackList(WebRtc_UWord16& nackSize, bool& listExtended)
{
    // TODO (mikhal/stefan): Refactor to use lastDecodedState.
    CriticalSectionScoped cs(_critSect);
    int i = 0;
    WebRtc_Word32 lowSeqNum = -1;
    WebRtc_Word32 highSeqNum = -1;
    listExtended = false;

    // Don't create list, if we won't wait for it
    if (!WaitForNack())
    {
        nackSize = 0;
        return NULL;
    }

    // Find the lowest (last decoded) sequence number and
    // the highest (highest sequence number of the newest frame)
    // sequence number. The nack list is a subset of the range
    // between those two numbers.
    GetLowHighSequenceNumbers(lowSeqNum, highSeqNum);

    // write a list of all seq num we have
    if (lowSeqNum == -1 || highSeqNum == -1)
    {
        // This happens if we lose the first packet, nothing is popped
        if (highSeqNum == -1)
        {
            // we have not received any packets yet
            nackSize = 0;
        }
        else
        {
            // signal that we want a key frame request to be sent
            nackSize = 0xffff;
        }
        return NULL;
    }

    int numberOfSeqNum = 0;
    if (lowSeqNum > highSeqNum)
    {
        if (lowSeqNum - highSeqNum > 0x00ff)
        {
            // wrap
            numberOfSeqNum = (0xffff-lowSeqNum) + highSeqNum + 1;
        }
    }
    else
    {
        numberOfSeqNum = highSeqNum - lowSeqNum;
    }

    if (numberOfSeqNum > kNackHistoryLength)
    {
        // Nack list is too big, flush and try to restart.
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideoCoding,
                     VCMId(_vcmId, _receiverId),
                     "Nack list too large, try to find a key frame and restart "
                     "from seq: %d. Lowest seq in jb %d", highSeqNum,lowSeqNum);

        // This nack size will trigger a key request...
        bool foundKeyFrame = false;

        while (numberOfSeqNum > kNackHistoryLength)
        {
            foundKeyFrame = RecycleFramesUntilKeyFrame();

            if (!foundKeyFrame)
            {
                break;
            }

            // Check if we still have too many packets in JB
            lowSeqNum = -1;
            highSeqNum = -1;
            GetLowHighSequenceNumbers(lowSeqNum, highSeqNum);

            if (highSeqNum == -1)
            {
                assert(lowSeqNum != -1); // This should never happen
                // We can't calculate the nack list length...
                return NULL;
            }

            numberOfSeqNum = 0;
            if (lowSeqNum > highSeqNum)
            {
                if (lowSeqNum - highSeqNum > 0x00ff)
                {
                    // wrap
                    numberOfSeqNum = (0xffff-lowSeqNum) + highSeqNum + 1;
                    highSeqNum=lowSeqNum;
                }
            }
            else
            {
                numberOfSeqNum = highSeqNum - lowSeqNum;
            }

        } // end while

        if (!foundKeyFrame)
        {
            // No key frame in JB.

            // Set the last decoded sequence number to current high.
            // This is to not get a large nack list again right away
            _lastDecodedState.SetSeqNum(static_cast<uint16_t>(highSeqNum));
            // Set to trigger key frame signal
            nackSize = 0xffff;
            listExtended = true;
            WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding, -1,
                    "\tNo key frame found, request one. _lastDecodedSeqNum[0] "
                    "%d", _lastDecodedState.sequence_num());
        }
        else
        {
            // We have cleaned up the jb and found a key frame
            // The function itself has set last decoded seq.
            WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding, -1,
                    "\tKey frame found. _lastDecodedSeqNum[0] %d",
                    _lastDecodedState.sequence_num());
            nackSize = 0;
        }

        return NULL;
    }

    WebRtc_UWord16 seqNumberIterator = (WebRtc_UWord16)(lowSeqNum + 1);
    for (i = 0; i < numberOfSeqNum; i++)
    {
        _NACKSeqNumInternal[i] = seqNumberIterator;
        seqNumberIterator++;
    }

    // now we have a list of all sequence numbers that could have been sent

    // zero out the ones we have received
    for (i = 0; i < _maxNumberOfFrames; i++)
    {
        // loop all created frames
        // We don't need to check if frame is decoding since lowSeqNum is based
        // on _lastDecodedSeqNum
        // Ignore free frames
        VCMFrameBufferStateEnum state = _frameBuffers[i]->GetState();

        if ((kStateFree != state) &&
            (kStateEmpty != state) &&
            (kStateDecoding != state))
        {
            // Reaching thus far means we are going to update the nack list
            // When in hybrid mode, we use the soft NACKing feature.
            if (_nackMode == kNackHybrid)
            {
                _frameBuffers[i]->BuildSoftNackList(_NACKSeqNumInternal,
                                                    numberOfSeqNum,
                                                    _rttMs);
            }
            else
            {
                // Used when the frame is being processed by the decoding thread
                // don't need to use that info in this loop.
                _frameBuffers[i]->BuildHardNackList(_NACKSeqNumInternal,
                                                    numberOfSeqNum);
            }
        }
    }

    // compress list
    int emptyIndex = -1;
    for (i = 0; i < numberOfSeqNum; i++)
    {
        if (_NACKSeqNumInternal[i] == -1 || _NACKSeqNumInternal[i] == -2 )
        {
            // this is empty
            if (emptyIndex == -1)
            {
                // no empty index before, remember this position
                emptyIndex = i;
            }
        }
        else
        {
            // this is not empty
            if (emptyIndex == -1)
            {
                // no empty index, continue
            }
            else
            {
                _NACKSeqNumInternal[emptyIndex] = _NACKSeqNumInternal[i];
                _NACKSeqNumInternal[i] = -1;
                emptyIndex++;
            }
        }
    } // for

    if (emptyIndex == -1)
    {
        // no empty
        nackSize = numberOfSeqNum;
    }
    else
    {
        nackSize = emptyIndex;
    }

    if (nackSize > _NACKSeqNumLength)
    {
        // Larger list: nack list was extended since the last call.
        listExtended = true;
    }

    for (WebRtc_UWord32 j = 0; j < nackSize; j++)
    {
        // Check if the list has been extended since it was last created. I.e,
        // new items have been added
        if (_NACKSeqNumLength > j && !listExtended)
        {
            WebRtc_UWord32 k = 0;
            for (k = j; k < _NACKSeqNumLength; k++)
            {
                // Found the item in the last list, i.e, no new items found yet.
                if (_NACKSeqNum[k] == (WebRtc_UWord16)_NACKSeqNumInternal[j])
                {
                   break;
                }
            }
            if (k == _NACKSeqNumLength) // New item not found in last list.
            {
                listExtended = true;
            }
        }
        else
        {
            listExtended = true;
        }
        _NACKSeqNum[j] = (WebRtc_UWord16)_NACKSeqNumInternal[j];
    }

    _NACKSeqNumLength = nackSize;

    return _NACKSeqNum;
}

// Release frame when done with decoding. Should never be used to release
// frames from within the jitter buffer.
void
VCMJitterBuffer::ReleaseFrame(VCMEncodedFrame* frame)
{
    CriticalSectionScoped cs(_critSect);
    VCMFrameBuffer* frameBuffer = static_cast<VCMFrameBuffer*>(frame);
    if (frameBuffer != NULL)
        frameBuffer->SetState(kStateFree);
}

WebRtc_Word64
VCMJitterBuffer::LastPacketTime(VCMEncodedFrame* frame,
                                bool& retransmitted) const
{
    CriticalSectionScoped cs(_critSect);
    retransmitted = (static_cast<VCMFrameBuffer*>(frame)->GetNackCount() > 0);
    return static_cast<VCMFrameBuffer*>(frame)->LatestPacketTimeMs();
}

WebRtc_Word64
VCMJitterBuffer::LastDecodedTimestamp() const
{
    CriticalSectionScoped cs(_critSect);
    return _lastDecodedState.time_stamp();
}

// Insert packet
// Takes crit sect, and inserts packet in frame buffer, possibly does logging
VCMFrameBufferEnum
VCMJitterBuffer::InsertPacket(VCMEncodedFrame* buffer, const VCMPacket& packet)
{
    CriticalSectionScoped cs(_critSect);
    WebRtc_Word64 nowMs = _clock->MillisecondTimestamp();
    VCMFrameBufferEnum bufferReturn = kSizeError;
    VCMFrameBufferEnum ret = kSizeError;
    VCMFrameBuffer* frame = static_cast<VCMFrameBuffer*>(buffer);

    // We are keeping track of the first seq num, the latest seq num and
    // the number of wraps to be able to calculate how many packets we expect.
    if (_firstPacket)
    {
        // Now it's time to start estimating jitter
        // reset the delay estimate.
        _delayEstimate.Reset(_clock->MillisecondTimestamp());
        _firstPacket = false;
    }

    // Empty packets may bias the jitter estimate (lacking size component),
    // therefore don't let empty packet trigger the following updates:
    if (packet.frameType != kFrameEmpty)
    {
        if (_waitingForCompletion.timestamp == packet.timestamp)
        {
            // This can get bad if we have a lot of duplicate packets,
            // we will then count some packet multiple times.
            _waitingForCompletion.frameSize += packet.sizeBytes;
            _waitingForCompletion.latestPacketTime = nowMs;
        }
        else if (_waitingForCompletion.latestPacketTime >= 0 &&
                 _waitingForCompletion.latestPacketTime + 2000 <= nowMs)
        {
            // A packet should never be more than two seconds late
            UpdateJitterAndDelayEstimates(_waitingForCompletion, true);
            _waitingForCompletion.latestPacketTime = -1;
            _waitingForCompletion.frameSize = 0;
            _waitingForCompletion.timestamp = 0;
        }
    }

    if (frame != NULL)
    {
        VCMFrameBufferStateEnum state = frame->GetState();
        _lastDecodedState.UpdateOldPacket(&packet);
        // Insert packet
        // Check for first packet
        // High sequence number will be -1 if neither an empty packet nor
        // a media packet has been inserted.
        bool first = (frame->GetHighSeqNum() == -1);
        // When in Hybrid mode, we allow for a decodable state
        // Note: Under current version, a decodable frame will never be
        // triggered, as the body of the function is empty.
        // TODO (mikhal): Update when decodable is enabled.
        bufferReturn = frame->InsertPacket(packet, nowMs,
                                           _nackMode == kNackHybrid,
                                           _rttMs);
        ret = bufferReturn;

        if (bufferReturn > 0)
        {
            _incomingBitCount += packet.sizeBytes << 3;

            // Has this packet been nacked or is it about to be nacked?
            if (IsPacketRetransmitted(packet))
            {
                frame->IncrementNackCount();
            }

            // Insert each frame once on the arrival of the first packet
            // belonging to that frame (media or empty)
            if (state == kStateEmpty && first)
            {
                ret = kFirstPacket;
                FrameList::reverse_iterator rit = std::find_if(
                    _frameList.rbegin(), _frameList.rend(),
                    FrameSmallerTimestamp(frame->TimeStamp()));
                _frameList.insert(rit.base(), frame);
            }
        }
    }
    switch(bufferReturn)
    {
    case kStateError:
    case kTimeStampError:
    case kSizeError:
        {
            if (frame != NULL)
            {
                // Will be released when it gets old.
                frame->Reset();
                frame->SetState(kStateEmpty);
            }
            break;
        }
    case kCompleteSession:
        {
            // Only update return value for a JB flush indicator.
            if (UpdateFrameState(frame) == kFlushIndicator)
              ret = kFlushIndicator;
            // Signal that we have a received packet
            _packetEvent.Set();
            break;
        }
    case kDecodableSession:
    case kIncomplete:
        {
          // Signal that we have a received packet
          _packetEvent.Set();
          break;
        }
    case kNoError:
    case kDuplicatePacket:
        {
            break;
        }
    default:
        {
            assert(false && "JitterBuffer::InsertPacket: Undefined value");
        }
    }
   return ret;
}

// Must be called from within _critSect
void
VCMJitterBuffer::UpdateOldJitterSample(const VCMPacket& packet)
{
    if (_waitingForCompletion.timestamp != packet.timestamp &&
        LatestTimestamp(_waitingForCompletion.timestamp, packet.timestamp,
                        NULL) == packet.timestamp)
    {
        // This is a newer frame than the one waiting for completion.
        _waitingForCompletion.frameSize = packet.sizeBytes;
        _waitingForCompletion.timestamp = packet.timestamp;
    }
    else
    {
        // This can get bad if we have a lot of duplicate packets,
        // we will then count some packet multiple times.
        _waitingForCompletion.frameSize += packet.sizeBytes;
        _jitterEstimate.UpdateMaxFrameSize(_waitingForCompletion.frameSize);
    }
}

// Must be called from within _critSect
bool
VCMJitterBuffer::IsPacketRetransmitted(const VCMPacket& packet) const
{
    if (_NACKSeqNumLength > 0)
    {
        for (WebRtc_UWord16 i = 0; i < _NACKSeqNumLength; i++)
        {
            if (packet.seqNum == _NACKSeqNum[i])
            {
                return true;
            }
        }
    }
    return false;
}

// Get nack status (enabled/disabled)
VCMNackMode
VCMJitterBuffer::GetNackMode() const
{
    CriticalSectionScoped cs(_critSect);
    return _nackMode;
}

// Set NACK mode
void
VCMJitterBuffer::SetNackMode(VCMNackMode mode,
                             int lowRttNackThresholdMs,
                             int highRttNackThresholdMs)
{
    CriticalSectionScoped cs(_critSect);
    _nackMode = mode;
    assert(lowRttNackThresholdMs >= -1 && highRttNackThresholdMs >= -1);
    assert(highRttNackThresholdMs == -1 ||
           lowRttNackThresholdMs <= highRttNackThresholdMs);
    assert(lowRttNackThresholdMs > -1 || highRttNackThresholdMs == -1);
    _lowRttNackThresholdMs = lowRttNackThresholdMs;
    _highRttNackThresholdMs = highRttNackThresholdMs;
    if (_nackMode == kNoNack)
    {
        _jitterEstimate.ResetNackCount();
    }
}


// Recycle oldest frames up to a key frame, used if JB is completely full
bool
VCMJitterBuffer::RecycleFramesUntilKeyFrame()
{
    // Remove up to oldest key frame
    while (_frameList.size() > 0)
    {
        // Throw at least one frame.
        _dropCount++;
        FrameList::iterator it = _frameList.begin();
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideoCoding,
                     VCMId(_vcmId, _receiverId),
                     "Jitter buffer drop count:%d, lowSeq %d", _dropCount,
                     (*it)->GetLowSeqNum());
        RecycleFrame(*it);
        it = _frameList.erase(it);
        if (it != _frameList.end() && (*it)->FrameType() == kVideoFrameKey)
        {
            // Fake the lastDecodedState to match this key frame.
            _lastDecodedState.SetStateOneBack(*it);
            return true;
        }
    }
    _waitingForKeyFrame = true;
    _lastDecodedState.Reset(); // TODO (mikhal): no sync
    return false;
}

// Must be called under the critical section _critSect.
void VCMJitterBuffer::CleanUpOldFrames() {
  while (_frameList.size() > 0) {
    VCMFrameBuffer* oldestFrame = _frameList.front();
    bool nextFrameEmpty = (_lastDecodedState.ContinuousFrame(oldestFrame) &&
        oldestFrame->GetState() == kStateEmpty);
    if (_lastDecodedState.IsOldFrame(oldestFrame) ||
        (nextFrameEmpty && _frameList.size() > 1)) {
      ReleaseFrameInternal(_frameList.front());
      _frameList.erase(_frameList.begin());
    } else {
      break;
    }
  }
}

// Used in GetFrameForDecoding
void VCMJitterBuffer::VerifyAndSetPreviousFrameLost(VCMFrameBuffer& frame) {
  frame.MakeSessionDecodable();  // Make sure the session can be decoded.
  if (frame.FrameType() == kVideoFrameKey)
    return;

  if (!_lastDecodedState.ContinuousFrame(&frame))
    frame.SetPreviousFrameLoss();
}

bool
VCMJitterBuffer::WaitForNack()
{
     // NACK disabled -> can't wait
     if (_nackMode == kNoNack)
     {
         return false;
     }
     // NACK only -> always wait
     else if (_nackMode == kNackInfinite)
     {
         return true;
     }
     // else: hybrid mode, evaluate
     // RTT high, don't wait
     if (_highRttNackThresholdMs >= 0 &&
         _rttMs >= static_cast<unsigned int>(_highRttNackThresholdMs))
     {
         return false;
     }
     // Either NACK only or hybrid
     return true;
}

}  // namespace webrtc
