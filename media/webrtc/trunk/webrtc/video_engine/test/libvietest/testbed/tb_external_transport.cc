/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/test/libvietest/include/tb_external_transport.h"

#include <cassert>
#include <math.h>
#include <stdio.h> // printf
#include <stdlib.h> // rand

#if defined(WEBRTC_LINUX) || defined(__linux__)
#include <string.h>
#endif
#if defined(WEBRTC_MAC)
#include <cstring>
#endif

#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/thread_wrapper.h"
#include "webrtc/system_wrappers/interface/tick_util.h"
#include "webrtc/video_engine/include/vie_network.h"

#if defined(_WIN32)
#pragma warning(disable: 4355) // 'this' : used in base member initializer list
#endif

const uint8_t kSenderReportPayloadType = 200;
const uint8_t kReceiverReportPayloadType = 201;

TbExternalTransport::TbExternalTransport(
    webrtc::ViENetwork& vieNetwork,
    int sender_channel,
    TbExternalTransport::SsrcChannelMap* receive_channels)
    :
      sender_channel_(sender_channel),
      receive_channels_(receive_channels),
      _vieNetwork(vieNetwork),
      _thread(*webrtc::ThreadWrapper::CreateThread(
          ViEExternalTransportRun, this, webrtc::kHighPriority,
          "AutotestTransport")),
      _event(*webrtc::EventWrapper::Create()),
      _crit(*webrtc::CriticalSectionWrapper::CreateCriticalSection()),
      _statCrit(*webrtc::CriticalSectionWrapper::CreateCriticalSection()),
      network_parameters_(),
      _rtpCount(0),
      _rtcpCount(0),
      _dropCount(0),
      packet_counters_(),
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
      _lastReceiveRTPTimestamp(0),
      last_receive_time_(-1),
      previous_drop_(false)
{
    srand((int) webrtc::TickTime::MicrosecondTimestamp());
    unsigned int tId = 0;
    memset(&network_parameters_, 0, sizeof(NetworkParameters));
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
    for (std::list<VideoPacket*>::iterator it = _rtpPackets.begin();
         it != _rtpPackets.end(); ++it) {
        delete *it;
    }
    _rtpPackets.clear();
    for (std::list<VideoPacket*>::iterator it = _rtcpPackets.begin();
         it != _rtcpPackets.end(); ++it) {
        delete *it;
    }
    _rtcpPackets.clear();
    delete &_crit;
    delete &_statCrit;
}

int TbExternalTransport::SendPacket(int channel, const void *data, int len)
{
  // Parse timestamp from RTP header according to RFC 3550, section 5.1.
    uint8_t* ptr = (uint8_t*)data;
    uint8_t payload_type = ptr[1] & 0x7F;
    uint32_t rtp_timestamp = ptr[4] << 24;
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
    ++packet_counters_[payload_type];
    _lastSendRTPTimestamp = rtp_timestamp;

    if (_filterSSRC)
    {
        uint8_t* ptr = (uint8_t*)data;
        uint32_t ssrc = ptr[8] << 24;
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
        uint8_t* ptr = (uint8_t*)data;

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

    // Packet loss.
    switch (network_parameters_.loss_model)
    {
        case (kNoLoss):
            previous_drop_ = false;
            break;
        case (kUniformLoss):
            previous_drop_ = UniformLoss(network_parameters_.packet_loss_rate);
            break;
        case (kGilbertElliotLoss):
            previous_drop_ = GilbertElliotLoss(
                network_parameters_.packet_loss_rate,
                network_parameters_.burst_length);
            break;
    }
    // Never drop packets from the first RTP timestamp (first frame)
    // transmitted.
    if (previous_drop_ && _firstRTPTimestamp != rtp_timestamp)
    {
        _statCrit.Enter();
        _dropCount++;
        _statCrit.Leave();
        return len;
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
    // Add jitter and make sure receiveTime isn't lower than receive time of
    // last frame.
    int network_delay_ms = GaussianRandom(
        network_parameters_.mean_one_way_delay,
        network_parameters_.std_dev_one_way_delay);
    newPacket->receiveTime = NowMs() + network_delay_ms;
    if (newPacket->receiveTime < last_receive_time_) {
      newPacket->receiveTime = last_receive_time_;
    }
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
    int network_delay_ms = GaussianRandom(
            network_parameters_.mean_one_way_delay,
            network_parameters_.std_dev_one_way_delay);
    newPacket->receiveTime = NowMs() + network_delay_ms;
    _rtcpPackets.push_back(newPacket);
    _event.Set();
    _crit.Leave();
    return len;
}

void TbExternalTransport::SetNetworkParameters(
    const NetworkParameters& network_parameters)
{
    webrtc::CriticalSectionScoped cs(&_crit);
    network_parameters_ = network_parameters;
}

void TbExternalTransport::SetSSRCFilter(uint32_t ssrc)
{
    webrtc::CriticalSectionScoped cs(&_crit);
    _filterSSRC = true;
    _SSRC = ssrc;
}

void TbExternalTransport::ClearStats()
{
    webrtc::CriticalSectionScoped cs(&_statCrit);
    _rtpCount = 0;
    _dropCount = 0;
    _rtcpCount = 0;
    packet_counters_.clear();
}

void TbExternalTransport::GetStats(int32_t& numRtpPackets,
                                   int32_t& numDroppedPackets,
                                   int32_t& numRtcpPackets,
                                   std::map<uint8_t, int>* packet_counters)
{
    webrtc::CriticalSectionScoped cs(&_statCrit);
    numRtpPackets = _rtpCount;
    numDroppedPackets = _dropCount;
    numRtcpPackets = _rtcpCount;
    *packet_counters = packet_counters_;
}

void TbExternalTransport::EnableSSRCCheck()
{
    webrtc::CriticalSectionScoped cs(&_statCrit);
    _checkSSRC = true;
}

unsigned int TbExternalTransport::ReceivedSSRC()
{
    webrtc::CriticalSectionScoped cs(&_statCrit);
    return _lastSSRC;
}

void TbExternalTransport::EnableSequenceNumberCheck()
{
    webrtc::CriticalSectionScoped cs(&_statCrit);
    _checkSequenceNumber = true;
}

unsigned short TbExternalTransport::GetFirstSequenceNumber()
{
    webrtc::CriticalSectionScoped cs(&_statCrit);
    return _firstSequenceNumber;
}

bool TbExternalTransport::EmptyQueue() const {
  webrtc::CriticalSectionScoped cs(&_crit);
  return _rtpPackets.empty() && _rtcpPackets.empty();
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

    _crit.Enter();
    while (!_rtpPackets.empty())
    {
        // Take first packet in queue
        packet = _rtpPackets.front();
        int64_t timeToReceive = 0;
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
            unsigned int ssrc = 0;
            {
                webrtc::CriticalSectionScoped cs(&_statCrit);
                ssrc = ((packet->packetBuffer[8]) << 24);
                ssrc += (packet->packetBuffer[9] << 16);
                ssrc += (packet->packetBuffer[10] << 8);
                ssrc += packet->packetBuffer[11];
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
            uint8_t* ptr = (uint8_t*)packet->packetBuffer;
            uint32_t rtp_timestamp = ptr[4] << 24;
            rtp_timestamp += ptr[5] << 16;
            rtp_timestamp += ptr[6] << 8;
            rtp_timestamp += ptr[7];
            if (_receive_frame_callback != NULL &&
                _lastReceiveRTPTimestamp != rtp_timestamp) {
              _receive_frame_callback->FrameReceived(rtp_timestamp);
            }
            _lastReceiveRTPTimestamp = rtp_timestamp;
            int destination_channel = sender_channel_;
            if (receive_channels_) {
              SsrcChannelMap::iterator it = receive_channels_->find(ssrc);
              if (it == receive_channels_->end()) {
                return false;
              }
              destination_channel = it->second;
            }
            _vieNetwork.ReceivedRTPPacket(destination_channel,
                                          packet->packetBuffer,
                                          packet->length);
            delete packet;
            packet = NULL;
        }
        _crit.Enter();
    }
    _crit.Leave();
    _crit.Enter();
    while (!_rtcpPackets.empty())
    {
        // Take first packet in queue
        packet = _rtcpPackets.front();
        int64_t timeToReceive = 0;
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
            uint8_t pltype = static_cast<uint8_t>(packet->packetBuffer[1]);
            if (pltype == kSenderReportPayloadType) {
              // Sender report.
              if (receive_channels_) {
                for (SsrcChannelMap::iterator it = receive_channels_->begin();
                    it != receive_channels_->end(); ++it) {
                  _vieNetwork.ReceivedRTCPPacket(it->second,
                                                 packet->packetBuffer,
                                                 packet->length);
                }
              } else {
                _vieNetwork.ReceivedRTCPPacket(sender_channel_,
                                               packet->packetBuffer,
                                               packet->length);
              }
            } else if (pltype == kReceiverReportPayloadType) {
              // Receiver report.
              _vieNetwork.ReceivedRTCPPacket(sender_channel_,
                                             packet->packetBuffer,
                                             packet->length);
            }
            delete packet;
            packet = NULL;
        }
        _crit.Enter();
    }
    _crit.Leave();
    _event.Wait(waitTime + 1); // Add 1 ms to not call to early...
    return true;
}

int64_t TbExternalTransport::NowMs()
{
    return webrtc::TickTime::MillisecondTimestamp();
}

bool TbExternalTransport::UniformLoss(int loss_rate) {
  int dropThis = rand() % 100;
  return (dropThis < loss_rate);
}

bool TbExternalTransport::GilbertElliotLoss(int loss_rate, int burst_length) {
  // Simulate bursty channel (Gilbert model)
  // (1st order) Markov chain model with memory of the previous/last
  // packet state (loss or received)

  // 0 = received state
  // 1 = loss state

  // probTrans10: if previous packet is lost, prob. to -> received state
  // probTrans11: if previous packet is lost, prob. to -> loss state

  // probTrans01: if previous packet is received, prob. to -> loss state
  // probTrans00: if previous packet is received, prob. to -> received

  // Map the two channel parameters (average loss rate and burst length)
  // to the transition probabilities:
  double probTrans10 = 100 * (1.0 / burst_length);
  double probTrans11 = (100.0 - probTrans10);
  double probTrans01 = (probTrans10 * ( loss_rate / (100.0 - loss_rate)));

  // Note: Random loss (Bernoulli) model is a special case where:
  // burstLength = 100.0 / (100.0 - _lossPct) (i.e., p10 + p01 = 100)

  if (previous_drop_) {
    // Previous packet was not received.
    return UniformLoss(probTrans11);
  } else {
    return UniformLoss(probTrans01);
  }
}

#define PI  3.14159265
int TbExternalTransport::GaussianRandom(int mean_ms,
                                        int standard_deviation_ms) {
  // Creating a Normal distribution variable from two independent uniform
  // variables based on the Box-Muller transform.
  double uniform1 = (rand() + 1.0) / (RAND_MAX + 1.0);
  double uniform2 = (rand() + 1.0) / (RAND_MAX + 1.0);
  return static_cast<int>(mean_ms + standard_deviation_ms *
      sqrt(-2 * log(uniform1)) * cos(2 * PI * uniform2));
}
