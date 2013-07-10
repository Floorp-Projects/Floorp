/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

//
// tb_external_transport.h
//

#ifndef WEBRTC_VIDEO_ENGINE_TEST_AUTOTEST_INTERFACE_TB_EXTERNAL_TRANSPORT_H_
#define WEBRTC_VIDEO_ENGINE_TEST_AUTOTEST_INTERFACE_TB_EXTERNAL_TRANSPORT_H_

#include <list>
#include <map>

#include "common_types.h"

namespace webrtc
{
class CriticalSectionWrapper;
class EventWrapper;
class ThreadWrapper;
class ViENetwork;
}

enum RandomLossModel {
  kNoLoss,
  kUniformLoss,
  kGilbertElliotLoss
};
struct NetworkParameters {
  int packet_loss_rate;
  int burst_length;  // Only applicable for kGilbertElliotLoss.
  int mean_one_way_delay;
  int std_dev_one_way_delay;
  RandomLossModel loss_model;
  NetworkParameters():
    packet_loss_rate(0), burst_length(0), mean_one_way_delay(0),
        std_dev_one_way_delay(0), loss_model(kNoLoss) {}
};

// Allows to subscribe for callback when a frame is started being sent.
class SendFrameCallback
{
public:
    // Called once per frame (when a new RTP timestamp is detected) when the
    // first data packet of the frame is being sent using the
    // TbExternalTransport.SendPacket method.
    virtual void FrameSent(unsigned int rtp_timestamp) = 0;
protected:
    SendFrameCallback() {}
    virtual ~SendFrameCallback() {}
};

// Allows to subscribe for callback when the first packet of a frame is
// received.
class ReceiveFrameCallback
{
public:
    // Called once per frame (when a new RTP timestamp is detected)
    // during the processing of the RTP packet queue in
    // TbExternalTransport::ViEExternalTransportProcess.
    virtual void FrameReceived(unsigned int rtp_timestamp) = 0;
protected:
    ReceiveFrameCallback() {}
    virtual ~ReceiveFrameCallback() {}
};

// External transport implementation for testing purposes.
// A packet loss probability must be set in order to drop packets from the data
// being sent to this class.
// Will never drop packets from the first frame of a video sequence.
class TbExternalTransport : public webrtc::Transport
{
public:
    typedef std::map<unsigned int, unsigned int> SsrcChannelMap;

    TbExternalTransport(webrtc::ViENetwork& vieNetwork,
                        int sender_channel,
                        TbExternalTransport::SsrcChannelMap* receive_channels);
    ~TbExternalTransport(void);

    virtual int SendPacket(int channel, const void *data, int len);
    virtual int SendRTCPPacket(int channel, const void *data, int len);

    // Should only be called before/after traffic is being processed.
    // Only one observer can be set (multiple calls will overwrite each other).
    virtual void RegisterSendFrameCallback(SendFrameCallback* callback);

    // Should only be called before/after traffic is being processed.
    // Only one observer can be set (multiple calls will overwrite each other).
    virtual void RegisterReceiveFrameCallback(ReceiveFrameCallback* callback);

    // The network parameters of the link. Regarding packet losses, packets
    // belonging to the first frame (same RTP timestamp) will never be dropped.
    void SetNetworkParameters(const NetworkParameters& network_parameters);
    void SetSSRCFilter(uint32_t SSRC);

    void ClearStats();
    void GetStats(int32_t& numRtpPackets,
                  int32_t& numDroppedPackets,
                  int32_t& numRtcpPackets);

    void SetTemporalToggle(unsigned char layers);
    void EnableSSRCCheck();
    unsigned int ReceivedSSRC();

    void EnableSequenceNumberCheck();
    unsigned short GetFirstSequenceNumber();

    bool EmptyQueue() const;

protected:
    static bool ViEExternalTransportRun(void* object);
    bool ViEExternalTransportProcess();
private:
    // TODO(mikhal): Break these out to classes.
    static int GaussianRandom(int mean_ms, int standard_deviation_ms);
    bool UniformLoss(int loss_rate);
    bool GilbertElliotLoss(int loss_rate, int burst_length);
    int64_t NowMs();

    enum
    {
        KMaxPacketSize = 1650
    };
    enum
    {
        KMaxWaitTimeMs = 100
    };
    typedef struct
    {
        int8_t packetBuffer[KMaxPacketSize];
        int32_t length;
        int32_t channel;
        int64_t receiveTime;
    } VideoPacket;

    int sender_channel_;
    SsrcChannelMap* receive_channels_;
    webrtc::ViENetwork& _vieNetwork;
    webrtc::ThreadWrapper& _thread;
    webrtc::EventWrapper& _event;
    webrtc::CriticalSectionWrapper& _crit;
    webrtc::CriticalSectionWrapper& _statCrit;

    NetworkParameters network_parameters_;
    int32_t _rtpCount;
    int32_t _rtcpCount;
    int32_t _dropCount;

    std::list<VideoPacket*> _rtpPackets;
    std::list<VideoPacket*> _rtcpPackets;

    SendFrameCallback* _send_frame_callback;
    ReceiveFrameCallback* _receive_frame_callback;

    unsigned char _temporalLayers;
    unsigned short _seqNum;
    unsigned short _sendPID;
    unsigned char _receivedPID;
    bool _switchLayer;
    unsigned char _currentRelayLayer;
    unsigned int _lastTimeMs;

    bool _checkSSRC;
    uint32_t _lastSSRC;
    bool _filterSSRC;
    uint32_t _SSRC;
    bool _checkSequenceNumber;
    uint16_t _firstSequenceNumber;

    // Keep track of the first RTP timestamp so we don't do packet loss on
    // the first frame.
    uint32_t _firstRTPTimestamp;
    // Track RTP timestamps so we invoke callbacks properly (if registered).
    uint32_t _lastSendRTPTimestamp;
    uint32_t _lastReceiveRTPTimestamp;
    int64_t last_receive_time_;
    bool previous_drop_;
};

#endif  // WEBRTC_VIDEO_ENGINE_TEST_AUTOTEST_INTERFACE_TB_EXTERNAL_TRANSPORT_H_
