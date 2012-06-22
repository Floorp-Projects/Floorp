/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_engine/test/libvietest/include/tb_external_transport.h"

#include <stdio.h> // printf
#include <stdlib.h> // rand
#include <cassert>

#if defined(WEBRTC_LINUX) || defined(__linux__)
#include <string.h>
#endif
#if defined(WEBRTC_MAC)
#include <cstring>
#endif

#include "system_wrappers/interface/critical_section_wrapper.h"
#include "system_wrappers/interface/event_wrapper.h"
#include "system_wrappers/interface/thread_wrapper.h"
#include "system_wrappers/interface/tick_util.h"
#include "video_engine/include/vie_network.h"

#if defined(_WIN32)
#pragma warning(disable: 4355) // 'this' : used in base member initializer list
#endif

TbExternalTransport::TbExternalTransport(webrtc::ViENetwork& vieNetwork) :
        _vieNetwork(vieNetwork),
        _thread(*webrtc::ThreadWrapper::CreateThread(
            ViEExternalTransportRun, this, webrtc::kHighPriority,
            "AutotestTransport")),
        _event(*webrtc::EventWrapper::Create()),
        _crit(*webrtc::CriticalSectionWrapper::CreateCriticalSection()),
        _statCrit(*webrtc::CriticalSectionWrapper::CreateCriticalSection()),
        _lossRate(0),
        _networkDelayMs(0),
        _rtpCount(0),
        _rtcpCount(0),
        _dropCount(0),
        _rtpPackets(),
        _rtcpPackets(),
        _send_frame_callback(NULL),
        _receive_frame_callback(NULL),
        _temporalLayers(0),
        _seqNum(0),
        _sendPID(0),
        _receivedPID(0),
        _switchLayer(false),
        _currentRelayLayer(0),
        _lastTimeMs(webrtc::TickTime::MillisecondTimestamp()),
        _checkSSRC(false),
        _lastSSRC(0),
        _filterSSRC(false),
        _SSRC(0),
        _checkSequenceNumber(0),
        _firstSequenceNumber(0),
        _firstRTPTimestamp(0),
        _lastSendRTPTimestamp(0),
        _lastReceiveRTPTimestamp(0)
{
    srand((int) webrtc::TickTime::MicrosecondTimestamp());
    unsigned int tId = 0;
    _thread.Start(tId);
}

TbExternalTransport::~TbExternalTransport()
{
    _thread.SetNotAlive();
    _event.Set();
    if (_thread.Stop())
    {
        delete &_thread;
        delete &_event;
    }
    delete &_crit;
    delete &_statCrit;
}

int TbExternalTransport::SendPacket(int channel, const void *data, int len)
{
  // Parse timestamp from RTP header according to RFC 3550, section 5.1.
    WebRtc_UWord8* ptr = (WebRtc_UWord8*)data;
    WebRtc_UWord32 rtp_timestamp = ptr[4] << 24;
    rtp_timestamp += ptr[5] << 16;
    rtp_timestamp += ptr[6] << 8;
    rtp_timestamp += ptr[7];
    _crit.Enter();
    if (_firstRTPTimestamp == 0) {
      _firstRTPTimestamp = rtp_timestamp;
    }
    _crit.Leave();
    if (_send_frame_callback != NULL &&
        _lastSendRTPTimestamp != rtp_timestamp) {
      _send_frame_callback->FrameSent(rtp_timestamp);
    }
    _lastSendRTPTimestamp = rtp_timestamp;

    if (_filterSSRC)
    {
        WebRtc_UWord8* ptr = (WebRtc_UWord8*)data;
        WebRtc_UWord32 ssrc = ptr[8] << 24;
        ssrc += ptr[9] << 16;
        ssrc += ptr[10] << 8;
        ssrc += ptr[11];
        if (ssrc != _SSRC)
        {
            return len; // return len to avoid error in trace file
        }
    }
    if (_temporalLayers) {
        // parse out vp8 temporal layers
        // 12 bytes RTP
        WebRtc_UWord8* ptr = (WebRtc_UWord8*)data;

        if (ptr[12] & 0x80 &&  // X-bit
            ptr[13] & 0x20)  // T-bit
        {
            int offset = 1;
            if (ptr[13] & 0x80) // PID-bit
            {
                offset++;
                if (ptr[14] & 0x80) // 2 byte PID
                {
                    offset++;
                }
            }
            if (ptr[13] & 0x40)
            {
                offset++;
            }
            unsigned char TID = (ptr[13 + offset] >> 5);
            unsigned int timeMs = NowMs();

            // Every 5 second switch layer
            if (_lastTimeMs + 5000 < timeMs)
            {
                _lastTimeMs = timeMs;
                _switchLayer = true;
            }
            // Switch at the non ref frame
            if (_switchLayer && (ptr[12] & 0x20))
            {   // N-bit
              _currentRelayLayer++;
                if (_currentRelayLayer >= _temporalLayers)
                  _currentRelayLayer = 0;

                _switchLayer = false;
                printf("\t Switching to layer:%d\n", _currentRelayLayer);
            }
            if (_currentRelayLayer < TID)
            {
                return len; // return len to avoid error in trace file
            }
            if (ptr[14] & 0x80) // 2 byte PID
            {
                if(_receivedPID != ptr[15])
                {
                    _sendPID++;
                    _receivedPID = ptr[15];
                }
            } else
            {
              if(_receivedPID != ptr[14])
              {
                _sendPID++;
                _receivedPID = ptr[14];
              }
            }
        }
    }
    _statCrit.Enter();
    _rtpCount++;
    _statCrit.Leave();

    // Packet loss. Never drop packets from the first RTP timestamp, i.e. the
    // first frame being transmitted.
    int dropThis = rand() % 100;
    if (dropThis < _lossRate && _firstRTPTimestamp != rtp_timestamp)
    {
        _statCrit.Enter();
        _dropCount++;
        _statCrit.Leave();
        return 0;
    }

    VideoPacket* newPacket = new VideoPacket();
    memcpy(newPacket->packetBuffer, data, len);

    if (_temporalLayers)
    {
        // rewrite seqNum
        newPacket->packetBuffer[2] = _seqNum >> 8;
        newPacket->packetBuffer[3] = _seqNum;
        _seqNum++;

        // rewrite PID
        if (newPacket->packetBuffer[14] & 0x80) // 2 byte PID
        {
            newPacket->packetBuffer[14] = (_sendPID >> 8) | 0x80;
            newPacket->packetBuffer[15] = _sendPID;
        } else
        {
            newPacket->packetBuffer[14] = (_sendPID & 0x7f);
        }
    }
    newPacket->length = len;
    newPacket->channel = channel;

    _crit.Enter();
    newPacket->receiveTime = NowMs() + _networkDelayMs;
    _rtpPackets.push_back(newPacket);
    _event.Set();
    _crit.Leave();
    return len;
}

void TbExternalTransport::RegisterSendFrameCallback(
    SendFrameCallback* callback) {
  _send_frame_callback = callback;
}

void TbExternalTransport::RegisterReceiveFrameCallback(
    ReceiveFrameCallback* callback) {
  _receive_frame_callback = callback;
}

// Set to 0 to disable.
void TbExternalTransport::SetTemporalToggle(unsigned char layers)
{
    _temporalLayers = layers;
}

int TbExternalTransport::SendRTCPPacket(int channel, const void *data, int len)
{
    _statCrit.Enter();
    _rtcpCount++;
    _statCrit.Leave();

    VideoPacket* newPacket = new VideoPacket();
    memcpy(newPacket->packetBuffer, data, len);
    newPacket->length = len;
    newPacket->channel = channel;

    _crit.Enter();
    newPacket->receiveTime = NowMs() + _networkDelayMs;
    _rtcpPackets.push_back(newPacket);
    _event.Set();
    _crit.Leave();
    return len;
}

WebRtc_Word32 TbExternalTransport::SetPacketLoss(WebRtc_Word32 lossRate)
{
    webrtc::CriticalSectionScoped cs(_statCrit);
    _lossRate = lossRate;
    return 0;
}

void TbExternalTransport::SetNetworkDelay(WebRtc_Word64 delayMs)
{
    webrtc::CriticalSectionScoped cs(_crit);
    _networkDelayMs = delayMs;
}

void TbExternalTransport::SetSSRCFilter(WebRtc_UWord32 ssrc)
{
    webrtc::CriticalSectionScoped cs(_crit);
    _filterSSRC = true;
    _SSRC = ssrc;
}

void TbExternalTransport::ClearStats()
{
    webrtc::CriticalSectionScoped cs(_statCrit);
    _rtpCount = 0;
    _dropCount = 0;
    _rtcpCount = 0;
}

void TbExternalTransport::GetStats(WebRtc_Word32& numRtpPackets,
                                   WebRtc_Word32& numDroppedPackets,
                                   WebRtc_Word32& numRtcpPackets)
{
    webrtc::CriticalSectionScoped cs(_statCrit);
    numRtpPackets = _rtpCount;
    numDroppedPackets = _dropCount;
    numRtcpPackets = _rtcpCount;
}

void TbExternalTransport::EnableSSRCCheck()
{
    webrtc::CriticalSectionScoped cs(_statCrit);
    _checkSSRC = true;
}

unsigned int TbExternalTransport::ReceivedSSRC()
{
    webrtc::CriticalSectionScoped cs(_statCrit);
    return _lastSSRC;
}

void TbExternalTransport::EnableSequenceNumberCheck()
{
    webrtc::CriticalSectionScoped cs(_statCrit);
    _checkSequenceNumber = true;
}

unsigned short TbExternalTransport::GetFirstSequenceNumber()
{
    webrtc::CriticalSectionScoped cs(_statCrit);
    return _firstSequenceNumber;
}

bool TbExternalTransport::ViEExternalTransportRun(void* object)
{
    return static_cast<TbExternalTransport*>
        (object)->ViEExternalTransportProcess();
}
bool TbExternalTransport::ViEExternalTransportProcess()
{
    unsigned int waitTime = KMaxWaitTimeMs;

    VideoPacket* packet = NULL;

    while (!_rtpPackets.empty())
    {
        // Take first packet in queue
        _crit.Enter();
        packet = _rtpPackets.front();
        WebRtc_Word64 timeToReceive = 0;
        if (packet)
        {
          timeToReceive = packet->receiveTime - NowMs();
        }
        else
        {
          // There should never be any empty packets in the list.
          assert(false);
        }
        if (timeToReceive > 0)
        {
            // No packets to receive yet
            if (timeToReceive < waitTime && timeToReceive > 0)
            {
                waitTime = (unsigned int) timeToReceive;
            }
            _crit.Leave();
            break;
        }
        _rtpPackets.pop_front();
        _crit.Leave();

        // Send to ViE
        if (packet)
        {
            {
                webrtc::CriticalSectionScoped cs(_statCrit);
                if (_checkSSRC)
                {
                    _lastSSRC = ((packet->packetBuffer[8]) << 24);
                    _lastSSRC += (packet->packetBuffer[9] << 16);
                    _lastSSRC += (packet->packetBuffer[10] << 8);
                    _lastSSRC += packet->packetBuffer[11];
                    _checkSSRC = false;
                }
                if (_checkSequenceNumber)
                {
                    _firstSequenceNumber
                        = (unsigned char) packet->packetBuffer[2] << 8;
                    _firstSequenceNumber
                        += (unsigned char) packet->packetBuffer[3];
                    _checkSequenceNumber = false;
                }
            }
            // Signal received packet of frame
            WebRtc_UWord8* ptr = (WebRtc_UWord8*)packet->packetBuffer;
            WebRtc_UWord32 rtp_timestamp = ptr[4] << 24;
            rtp_timestamp += ptr[5] << 16;
            rtp_timestamp += ptr[6] << 8;
            rtp_timestamp += ptr[7];
            if (_receive_frame_callback != NULL &&
                _lastReceiveRTPTimestamp != rtp_timestamp) {
              _receive_frame_callback->FrameReceived(rtp_timestamp);
            }
            _lastReceiveRTPTimestamp = rtp_timestamp;

            _vieNetwork.ReceivedRTPPacket(packet->channel,
                                          packet->packetBuffer, packet->length);
            delete packet;
            packet = NULL;
        }
    }
    while (!_rtcpPackets.empty())
    {
        // Take first packet in queue
        _crit.Enter();
        packet = _rtcpPackets.front();
        WebRtc_Word64 timeToReceive = 0;
        if (packet)
        {
          timeToReceive = packet->receiveTime - NowMs();
        }
        else
        {
            // There should never be any empty packets in the list.
            assert(false);
        }
        if (timeToReceive > 0)
        {
            // No packets to receive yet
            if (timeToReceive < waitTime && timeToReceive > 0)
            {
                waitTime = (unsigned int) timeToReceive;
            }
            _crit.Leave();
            break;
        }
        _rtcpPackets.pop_front();
        _crit.Leave();

        // Send to ViE
        if (packet)
        {
            _vieNetwork.ReceivedRTCPPacket(
                 packet->channel,
                 packet->packetBuffer, packet->length);
            delete packet;
            packet = NULL;
        }
    }
    _event.Wait(waitTime + 1); // Add 1 ms to not call to early...
    return true;
}

WebRtc_Word64 TbExternalTransport::NowMs()
{
    return webrtc::TickTime::MillisecondTimestamp();
}
