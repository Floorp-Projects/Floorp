/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_INTERFACE_RTP_RTCP_H_
#define WEBRTC_MODULES_RTP_RTCP_INTERFACE_RTP_RTCP_H_

#include <vector>

#include "webrtc/modules/interface/module.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"

namespace webrtc {
// Forward declarations.
class PacedSender;
class ReceiveStatistics;
class RemoteBitrateEstimator;
class RtpReceiver;
class Transport;

class RtpRtcp : public Module {
 public:
  struct Configuration {
    Configuration();

   /*  id                   - Unique identifier of this RTP/RTCP module object
    *  audio                - True for a audio version of the RTP/RTCP module
    *                         object false will create a video version
    *  clock                - The clock to use to read time. If NULL object
    *                         will be using the system clock.
    *  incoming_data        - Callback object that will receive the incoming
    *                         data. May not be NULL; default callback will do
    *                         nothing.
    *  incoming_messages    - Callback object that will receive the incoming
    *                         RTP messages. May not be NULL; default callback
    *                         will do nothing.
    *  outgoing_transport   - Transport object that will be called when packets
    *                         are ready to be sent out on the network
    *  intra_frame_callback - Called when the receiver request a intra frame.
    *  bandwidth_callback   - Called when we receive a changed estimate from
    *                         the receiver of out stream.
    *  audio_messages       - Telephone events. May not be NULL; default
    *                         callback will do nothing.
    *  remote_bitrate_estimator - Estimates the bandwidth available for a set of
    *                             streams from the same client.
    *  paced_sender             - Spread any bursts of packets into smaller
    *                             bursts to minimize packet loss.
    */
    int32_t id;
    bool audio;
    Clock* clock;
    ReceiveStatistics* receive_statistics;
    Transport* outgoing_transport;
    RtcpIntraFrameObserver* intra_frame_callback;
    RtcpBandwidthObserver* bandwidth_callback;
    RtcpRttStats* rtt_stats;
    RtcpPacketTypeCounterObserver* rtcp_packet_type_counter_observer;
    RtpAudioFeedback* audio_messages;
    RemoteBitrateEstimator* remote_bitrate_estimator;
    PacedSender* paced_sender;
    BitrateStatisticsObserver* send_bitrate_observer;
    FrameCountObserver* send_frame_count_observer;
    SendSideDelayObserver* send_side_delay_observer;
  };

  /*
   *   Create a RTP/RTCP module object using the system clock.
   *
   *   configuration  - Configuration of the RTP/RTCP module.
   */
  static RtpRtcp* CreateRtpRtcp(const RtpRtcp::Configuration& configuration);

  /**************************************************************************
   *
   *   Receiver functions
   *
   ***************************************************************************/

    virtual int32_t IncomingRtcpPacket(const uint8_t* incoming_packet,
                                       size_t incoming_packet_length) = 0;

    virtual void SetRemoteSSRC(uint32_t ssrc) = 0;

    /**************************************************************************
    *
    *   Sender
    *
    ***************************************************************************/

    /*
    *   set MTU
    *
    *   size    -  Max transfer unit in bytes, default is 1500
    *
    *   return -1 on failure else 0
    */
    virtual int32_t SetMaxTransferUnit(uint16_t size) = 0;

    /*
    *   set transtport overhead
    *   default is IPv4 and UDP with no encryption
    *
    *   TCP                     - true for TCP false UDP
    *   IPv6                    - true for IP version 6 false for version 4
    *   authenticationOverhead  - number of bytes to leave for an
    *                             authentication header
    *
    *   return -1 on failure else 0
    */
    virtual int32_t SetTransportOverhead(
        bool TCP,
        bool IPV6,
        uint8_t authenticationOverhead = 0) = 0;

    /*
    *   Get max payload length
    *
    *   A combination of the configuration MaxTransferUnit and
    *   TransportOverhead.
    *   Does not account FEC/ULP/RED overhead if FEC is enabled.
    *   Does not account for RTP headers
    */
    virtual uint16_t MaxPayloadLength() const = 0;

    /*
    *   Get max data payload length
    *
    *   A combination of the configuration MaxTransferUnit, headers and
    *   TransportOverhead.
    *   Takes into account FEC/ULP/RED overhead if FEC is enabled.
    *   Takes into account RTP headers
    */
    virtual uint16_t MaxDataPayloadLength() const = 0;

    /*
    *   set codec name and payload type
    *
    *   return -1 on failure else 0
    */
    virtual int32_t RegisterSendPayload(
        const CodecInst& voiceCodec) = 0;

    /*
    *   set codec name and payload type
    *
    *   return -1 on failure else 0
    */
    virtual int32_t RegisterSendPayload(
        const VideoCodec& videoCodec) = 0;

    /*
    *   Unregister a send payload
    *
    *   payloadType - payload type of codec
    *
    *   return -1 on failure else 0
    */
    virtual int32_t DeRegisterSendPayload(int8_t payloadType) = 0;

   /*
    *   (De)register RTP header extension type and id.
    *
    *   return -1 on failure else 0
    */
    virtual int32_t RegisterSendRtpHeaderExtension(RTPExtensionType type,
                                                   uint8_t id) = 0;

    virtual int32_t DeregisterSendRtpHeaderExtension(RTPExtensionType type) = 0;

    /*
    *   get start timestamp
    */
    virtual uint32_t StartTimestamp() const = 0;

    /*
    *   configure start timestamp, default is a random number
    *
    *   timestamp   - start timestamp
    */
    virtual void SetStartTimestamp(uint32_t timestamp) = 0;

    /*
    *   Get SequenceNumber
    */
    virtual uint16_t SequenceNumber() const = 0;

    /*
    *   Set SequenceNumber, default is a random number
    */
    virtual void SetSequenceNumber(uint16_t seq) = 0;

    // Returns true if the ssrc matched this module, false otherwise.
    virtual bool SetRtpStateForSsrc(uint32_t ssrc,
                                    const RtpState& rtp_state) = 0;
    virtual bool GetRtpStateForSsrc(uint32_t ssrc, RtpState* rtp_state) = 0;

    /*
    *   Get SSRC
    */
    virtual uint32_t SSRC() const = 0;

    /*
    *   configure SSRC, default is a random number
    */
    virtual void SetSSRC(uint32_t ssrc) = 0;

    /*
    *   Set CSRC
    *
    *   csrcs   - vector of CSRCs
    */
    virtual void SetCsrcs(const std::vector<uint32_t>& csrcs) = 0;

    /*
    * Turn on/off sending RTX (RFC 4588). The modes can be set as a combination
    * of values of the enumerator RtxMode.
    */
    virtual void SetRtxSendStatus(int modes) = 0;

    /*
    * Get status of sending RTX (RFC 4588). The returned value can be
    * a combination of values of the enumerator RtxMode.
    */
    virtual int RtxSendStatus() const = 0;

    // Sets the SSRC to use when sending RTX packets. This doesn't enable RTX,
    // only the SSRC is set.
    virtual void SetRtxSsrc(uint32_t ssrc) = 0;

    // Sets the payload type to use when sending RTX packets. Note that this
    // doesn't enable RTX, only the payload type is set.
    virtual void SetRtxSendPayloadType(int payload_type) = 0;

    /*
    *   sends kRtcpByeCode when going from true to false
    *
    *   sending - on/off
    *
    *   return -1 on failure else 0
    */
    virtual int32_t SetSendingStatus(bool sending) = 0;

    /*
    *   get send status
    */
    virtual bool Sending() const = 0;

    /*
    *   Starts/Stops media packets, on by default
    *
    *   sending - on/off
    */
    virtual void SetSendingMediaStatus(bool sending) = 0;

    /*
    *   get send status
    */
    virtual bool SendingMedia() const = 0;

    /*
    *   get sent bitrate in Kbit/s
    */
    virtual void BitrateSent(uint32_t* totalRate,
                             uint32_t* videoRate,
                             uint32_t* fecRate,
                             uint32_t* nackRate) const = 0;

    /*
    *   Used by the codec module to deliver a video or audio frame for
    *   packetization.
    *
    *   frameType       - type of frame to send
    *   payloadType     - payload type of frame to send
    *   timestamp       - timestamp of frame to send
    *   payloadData     - payload buffer of frame to send
    *   payloadSize     - size of payload buffer to send
    *   fragmentation   - fragmentation offset data for fragmented frames such
    *                     as layers or RED
    *
    *   return -1 on failure else 0
    */
    virtual int32_t SendOutgoingData(
        FrameType frameType,
        int8_t payloadType,
        uint32_t timeStamp,
        int64_t capture_time_ms,
        const uint8_t* payloadData,
        size_t payloadSize,
        const RTPFragmentationHeader* fragmentation = NULL,
        const RTPVideoHeader* rtpVideoHdr = NULL) = 0;

    virtual bool TimeToSendPacket(uint32_t ssrc,
                                  uint16_t sequence_number,
                                  int64_t capture_time_ms,
                                  bool retransmission) = 0;

    virtual size_t TimeToSendPadding(size_t bytes) = 0;

    virtual bool GetSendSideDelay(int* avg_send_delay_ms,
                                  int* max_send_delay_ms) const = 0;

    // Called on generation of new statistics after an RTP send.
    virtual void RegisterSendChannelRtpStatisticsCallback(
        StreamDataCountersCallback* callback) = 0;
    virtual StreamDataCountersCallback*
        GetSendChannelRtpStatisticsCallback() const = 0;

    /**************************************************************************
    *
    *   RTCP
    *
    ***************************************************************************/

    /*
    *    Get RTCP status
    */
    virtual RTCPMethod RTCP() const = 0;

    /*
    *   configure RTCP status i.e on(compound or non- compound)/off
    *
    *   method  - RTCP method to use
    */
    virtual void SetRTCPStatus(RTCPMethod method) = 0;

    /*
    *   Set RTCP CName (i.e unique identifier)
    *
    *   return -1 on failure else 0
    */
    virtual int32_t SetCNAME(const char cName[RTCP_CNAME_SIZE]) = 0;

    /*
    *   Get remote CName
    *
    *   return -1 on failure else 0
    */
    virtual int32_t RemoteCNAME(uint32_t remoteSSRC,
                                char cName[RTCP_CNAME_SIZE]) const = 0;

    /*
    *   Get remote NTP
    *
    *   return -1 on failure else 0
    */
    virtual int32_t RemoteNTP(
        uint32_t *ReceivedNTPsecs,
        uint32_t *ReceivedNTPfrac,
        uint32_t *RTCPArrivalTimeSecs,
        uint32_t *RTCPArrivalTimeFrac,
        uint32_t *rtcp_timestamp) const  = 0;

    /*
    *   AddMixedCNAME
    *
    *   return -1 on failure else 0
    */
    virtual int32_t AddMixedCNAME(uint32_t SSRC,
                                  const char cName[RTCP_CNAME_SIZE]) = 0;

    /*
    *   RemoveMixedCNAME
    *
    *   return -1 on failure else 0
    */
    virtual int32_t RemoveMixedCNAME(uint32_t SSRC) = 0;

    /*
    *   Get RoundTripTime
    *
    *   return -1 on failure else 0
    */
    virtual int32_t RTT(uint32_t remoteSSRC,
                        int64_t* RTT,
                        int64_t* avgRTT,
                        int64_t* minRTT,
                        int64_t* maxRTT) const = 0;

    /*
    *   Force a send of a RTCP packet
    *   normal SR and RR are triggered via the process function
    *
    *   return -1 on failure else 0
    */
    virtual int32_t SendRTCP(
        uint32_t rtcpPacketType = kRtcpReport) = 0;

    /*
    *    Good state of RTP receiver inform sender
    */
    virtual int32_t SendRTCPReferencePictureSelection(
        const uint64_t pictureID) = 0;

    /*
    *    Send a RTCP Slice Loss Indication (SLI)
    *    6 least significant bits of pictureID
    */
    virtual int32_t SendRTCPSliceLossIndication(uint8_t pictureID) = 0;

    /*
    *   Reset RTP data counters for the sending side
    *
    *   return -1 on failure else 0
    */
    virtual int32_t ResetSendDataCountersRTP() = 0;

    /*
    *   Statistics of the amount of data sent
    *
    *   return -1 on failure else 0
    */
    virtual int32_t DataCountersRTP(
        size_t* bytesSent,
        uint32_t* packetsSent) const = 0;

    /*
    *   Get send statistics for the RTP and RTX stream.
    */
    virtual void GetSendStreamDataCounters(
        StreamDataCounters* rtp_counters,
        StreamDataCounters* rtx_counters) const = 0;

    /*
    *   Get received RTCP sender info
    *
    *   return -1 on failure else 0
    */
    virtual int32_t RemoteRTCPStat(RTCPSenderInfo* senderInfo) = 0;

    /*
    *   Get received RTCP report block
    *
    *   return -1 on failure else 0
    */
    virtual int32_t RemoteRTCPStat(
        std::vector<RTCPReportBlock>* receiveBlocks) const = 0;

    /*
    *   Set received RTCP report block
    *
    *   return -1 on failure else 0
    */
    virtual int32_t AddRTCPReportBlock(uint32_t SSRC,
                                       const RTCPReportBlock* receiveBlock) = 0;

    /*
    *   RemoveRTCPReportBlock
    *
    *   return -1 on failure else 0
    */
    virtual int32_t RemoveRTCPReportBlock(uint32_t SSRC) = 0;

    /*
    *   (APP) Application specific data
    *
    *   return -1 on failure else 0
    */
    virtual int32_t SetRTCPApplicationSpecificData(uint8_t subType,
                                                   uint32_t name,
                                                   const uint8_t* data,
                                                   uint16_t length) = 0;
    /*
    *   (XR) VOIP metric
    *
    *   return -1 on failure else 0
    */
    virtual int32_t SetRTCPVoIPMetrics(
        const RTCPVoIPMetric* VoIPMetric) = 0;

    /*
    *   (XR) Receiver Reference Time Report
    */
    virtual void SetRtcpXrRrtrStatus(bool enable) = 0;

    virtual bool RtcpXrRrtrStatus() const = 0;

    /*
    *  (REMB) Receiver Estimated Max Bitrate
    */
    virtual bool REMB() const = 0;

    virtual void SetREMBStatus(bool enable) = 0;

    virtual void SetREMBData(uint32_t bitrate,
                             const std::vector<uint32_t>& ssrcs) = 0;

    /*
    *   (IJ) Extended jitter report.
    */
    virtual bool IJ() const = 0;

    virtual void SetIJStatus(bool enable) = 0;

    /*
    *   (TMMBR) Temporary Max Media Bit Rate
    */
    virtual bool TMMBR() const = 0;

    virtual void SetTMMBRStatus(bool enable) = 0;

    /*
    *   (NACK)
    */

    /*
     *  TODO(holmer): Propagate this API to VideoEngine.
     *  Returns the currently configured selective retransmission settings.
     */
    virtual int SelectiveRetransmissions() const = 0;

    /*
     *  TODO(holmer): Propagate this API to VideoEngine.
     *  Sets the selective retransmission settings, which will decide which
     *  packets will be retransmitted if NACKed. Settings are constructed by
     *  combining the constants in enum RetransmissionMode with bitwise OR.
     *  All packets are retransmitted if kRetransmitAllPackets is set, while no
     *  packets are retransmitted if kRetransmitOff is set.
     *  By default all packets except FEC packets are retransmitted. For VP8
     *  with temporal scalability only base layer packets are retransmitted.
     *
     *  Returns -1 on failure, otherwise 0.
     */
    virtual int SetSelectiveRetransmissions(uint8_t settings) = 0;

    /*
    *   Send a Negative acknowledgement packet
    *
    *   return -1 on failure else 0
    */
    virtual int32_t SendNACK(const uint16_t* nackList, uint16_t size) = 0;

    /*
    *   Store the sent packets, needed to answer to a Negative acknowledgement
    *   requests
    */
    virtual void SetStorePacketsStatus(bool enable, uint16_t numberToStore) = 0;

    // Returns true if the module is configured to store packets.
    virtual bool StorePackets() const = 0;

    // Called on receipt of RTCP report block from remote side.
    virtual void RegisterRtcpStatisticsCallback(
        RtcpStatisticsCallback* callback) = 0;
    virtual RtcpStatisticsCallback*
        GetRtcpStatisticsCallback() = 0;

    /**************************************************************************
    *
    *   Audio
    *
    ***************************************************************************/

    /*
    *   set audio packet size, used to determine when it's time to send a DTMF
    *   packet in silence (CNG)
    *
    *   return -1 on failure else 0
    */
    virtual int32_t SetAudioPacketSize(uint16_t packetSizeSamples) = 0;

    /*
    *   Send a TelephoneEvent tone using RFC 2833 (4733)
    *
    *   return -1 on failure else 0
    */
    virtual int32_t SendTelephoneEventOutband(uint8_t key,
                                              uint16_t time_ms,
                                              uint8_t level) = 0;

    /*
    *   Set payload type for Redundant Audio Data RFC 2198
    *
    *   return -1 on failure else 0
    */
    virtual int32_t SetSendREDPayloadType(int8_t payloadType) = 0;

    /*
    *   Get payload type for Redundant Audio Data RFC 2198
    *
    *   return -1 on failure else 0
    */
     virtual int32_t SendREDPayloadType(
         int8_t& payloadType) const = 0;

     /*
     * Store the audio level in dBov for header-extension-for-audio-level-
     * indication.
     * This API shall be called before transmision of an RTP packet to ensure
     * that the |level| part of the extended RTP header is updated.
     *
     * return -1 on failure else 0.
     */
     virtual int32_t SetAudioLevel(uint8_t level_dBov) = 0;

    /**************************************************************************
    *
    *   Video
    *
    ***************************************************************************/

    /*
    *   Set the target send bitrate
    */
    virtual void SetTargetSendBitrate(uint32_t bitrate_bps) = 0;

    /*
    *   Turn on/off generic FEC
    *
    *   return -1 on failure else 0
    */
    virtual int32_t SetGenericFECStatus(bool enable,
                                        uint8_t payloadTypeRED,
                                        uint8_t payloadTypeFEC) = 0;

    /*
    *   Get generic FEC setting
    *
    *   return -1 on failure else 0
    */
    virtual int32_t GenericFECStatus(bool& enable,
                                     uint8_t& payloadTypeRED,
                                     uint8_t& payloadTypeFEC) = 0;


    virtual int32_t SetFecParameters(
        const FecProtectionParams* delta_params,
        const FecProtectionParams* key_params) = 0;

    /*
    *   Set method for requestion a new key frame
    *
    *   return -1 on failure else 0
    */
    virtual int32_t SetKeyFrameRequestMethod(KeyFrameRequestMethod method) = 0;

    /*
    *   send a request for a keyframe
    *
    *   return -1 on failure else 0
    */
    virtual int32_t RequestKeyFrame() = 0;
};
}  // namespace webrtc
#endif // WEBRTC_MODULES_RTP_RTCP_INTERFACE_RTP_RTCP_H_
