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
class RemoteBitrateEstimator;
class RemoteBitrateObserver;
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
    *  rtcp_feedback        - Callback object that will receive the incoming
    *                         RTCP messages.
    *  intra_frame_callback - Called when the receiver request a intra frame.
    *  bandwidth_callback   - Called when we receive a changed estimate from
    *                         the receiver of out stream.
    *  audio_messages       - Telehone events. May not be NULL; default callback
    *                         will do nothing.
    *  remote_bitrate_estimator - Estimates the bandwidth available for a set of
    *                             streams from the same client.
    *  paced_sender             - Spread any bursts of packets into smaller
    *                             bursts to minimize packet loss.
    */
    int32_t id;
    bool audio;
    Clock* clock;
    RtpRtcp* default_module;
    RtpData* incoming_data;
    RtpFeedback* incoming_messages;
    Transport* outgoing_transport;
    RtcpFeedback* rtcp_feedback;
    RtcpIntraFrameObserver* intra_frame_callback;
    RtcpBandwidthObserver* bandwidth_callback;
    RtcpRttObserver* rtt_observer;
    RtpAudioFeedback* audio_messages;
    RemoteBitrateEstimator* remote_bitrate_estimator;
    PacedSender* paced_sender;
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

    /*
    *   configure a RTP packet timeout value
    *
    *   RTPtimeoutMS   - time in milliseconds after last received RTP packet
    *   RTCPtimeoutMS  - time in milliseconds after last received RTCP packet
    *
    *   return -1 on failure else 0
    */
    virtual int32_t SetPacketTimeout(
        const uint32_t RTPtimeoutMS,
        const uint32_t RTCPtimeoutMS) = 0;

    /*
    *   Set periodic dead or alive notification
    *
    *   enable              - turn periodic dead or alive notification on/off
    *   sampleTimeSeconds   - sample interval in seconds for dead or alive
    *                         notifications
    *
    *   return -1 on failure else 0
    */
    virtual int32_t SetPeriodicDeadOrAliveStatus(
        const bool enable,
        const uint8_t sampleTimeSeconds) = 0;

    /*
    *   Get periodic dead or alive notification status
    *
    *   enable              - periodic dead or alive notification on/off
    *   sampleTimeSeconds   - sample interval in seconds for dead or alive
    *                         notifications
    *
    *   return -1 on failure else 0
    */
    virtual int32_t PeriodicDeadOrAliveStatus(
        bool& enable,
        uint8_t& sampleTimeSeconds) = 0;

    /*
    *   set voice codec name and payload type
    *
    *   return -1 on failure else 0
    */
    virtual int32_t RegisterReceivePayload(
        const CodecInst& voiceCodec) = 0;

    /*
    *   set video codec name and payload type
    *
    *   return -1 on failure else 0
    */
    virtual int32_t RegisterReceivePayload(
        const VideoCodec& videoCodec) = 0;

    /*
    *   get payload type for a voice codec
    *
    *   return -1 on failure else 0
    */
    virtual int32_t ReceivePayloadType(
        const CodecInst& voiceCodec,
        int8_t* plType) = 0;

    /*
    *   get payload type for a video codec
    *
    *   return -1 on failure else 0
    */
    virtual int32_t ReceivePayloadType(
        const VideoCodec& videoCodec,
        int8_t* plType) = 0;

    /*
    *   Remove a registered payload type from list of accepted payloads
    *
    *   payloadType - payload type of codec
    *
    *   return -1 on failure else 0
    */
    virtual int32_t DeRegisterReceivePayload(
        const int8_t payloadType) = 0;

   /*
    *   (De)register RTP header extension type and id.
    *
    *   return -1 on failure else 0
    */
    virtual int32_t RegisterReceiveRtpHeaderExtension(
        const RTPExtensionType type,
        const uint8_t id) = 0;

    virtual int32_t DeregisterReceiveRtpHeaderExtension(
        const RTPExtensionType type) = 0;

    /*
    *   Get last received remote timestamp
    */
    virtual uint32_t RemoteTimestamp() const = 0;

    /*
    *   Get the local time of the last received remote timestamp
    */
    virtual int64_t LocalTimeOfRemoteTimeStamp() const = 0;

    /*
    *   Get the current estimated remote timestamp
    *
    *   timestamp   - estimated timestamp
    *
    *   return -1 on failure else 0
    */
    virtual int32_t EstimatedRemoteTimeStamp(
        uint32_t& timestamp) const = 0;

    /*
    *   Get incoming SSRC
    */
    virtual uint32_t RemoteSSRC() const = 0;

    /*
    *   Get remote CSRC
    *
    *   arrOfCSRC   - array that will receive the CSRCs
    *
    *   return -1 on failure else the number of valid entries in the list
    */
    virtual int32_t RemoteCSRCs(
        uint32_t arrOfCSRC[kRtpCsrcSize]) const  = 0;

    /*
    *   get the currently configured SSRC filter
    *
    *   allowedSSRC - SSRC that will be allowed through
    *
    *   return -1 on failure else 0
    */
    virtual int32_t SSRCFilter(uint32_t& allowedSSRC) const = 0;

    /*
    *   set a SSRC to be used as a filter for incoming RTP streams
    *
    *   allowedSSRC - SSRC that will be allowed through
    *
    *   return -1 on failure else 0
    */
    virtual int32_t SetSSRCFilter(const bool enable,
                                  const uint32_t allowedSSRC) = 0;

    /*
    * Turn on/off receiving RTX (RFC 4588) on a specific SSRC.
    */
    virtual int32_t SetRTXReceiveStatus(bool enable, uint32_t SSRC) = 0;

    // Sets the payload type to expected for received RTX packets. Note
    // that this doesn't enable RTX, only the payload type is set.
    virtual void SetRtxReceivePayloadType(int payload_type) = 0;

    /*
    * Get status of receiving RTX (RFC 4588) on a specific SSRC.
    */
    virtual int32_t RTXReceiveStatus(bool* enable,
                                     uint32_t* SSRC,
                                     int* payloadType) const = 0;

    /*
    *   called by the network module when we receive a packet
    *
    *   incomingPacket - incoming packet buffer
    *   packetLength   - length of incoming buffer
    *
    *   return -1 on failure else 0
    */
    virtual int32_t IncomingPacket(const uint8_t* incomingPacket,
                                   const uint16_t packetLength) = 0;

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
    virtual int32_t SetMaxTransferUnit(const uint16_t size) = 0;

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
        const bool TCP,
        const bool IPV6,
        const uint8_t authenticationOverhead = 0) = 0;

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
    virtual int32_t DeRegisterSendPayload(
        const int8_t payloadType) = 0;

   /*
    *   (De)register RTP header extension type and id.
    *
    *   return -1 on failure else 0
    */
    virtual int32_t RegisterSendRtpHeaderExtension(
        const RTPExtensionType type,
        const uint8_t id) = 0;

    virtual int32_t DeregisterSendRtpHeaderExtension(
        const RTPExtensionType type) = 0;

    /*
    *   get start timestamp
    */
    virtual uint32_t StartTimestamp() const = 0;

    /*
    *   configure start timestamp, default is a random number
    *
    *   timestamp   - start timestamp
    *
    *   return -1 on failure else 0
    */
    virtual int32_t SetStartTimestamp(
        const uint32_t timestamp) = 0;

    /*
    *   Get SequenceNumber
    */
    virtual uint16_t SequenceNumber() const = 0;

    /*
    *   Set SequenceNumber, default is a random number
    *
    *   return -1 on failure else 0
    */
    virtual int32_t SetSequenceNumber(const uint16_t seq) = 0;

    /*
    *   Get SSRC
    */
    virtual uint32_t SSRC() const = 0;

    /*
    *   configure SSRC, default is a random number
    *
    *   return -1 on failure else 0
    */
    virtual int32_t SetSSRC(const uint32_t ssrc) = 0;

    /*
    *   Get CSRC
    *
    *   arrOfCSRC   - array of CSRCs
    *
    *   return -1 on failure else number of valid entries in the array
    */
    virtual int32_t CSRCs(
        uint32_t arrOfCSRC[kRtpCsrcSize]) const = 0;

    /*
    *   Set CSRC
    *
    *   arrOfCSRC   - array of CSRCs
    *   arrLength   - number of valid entries in the array
    *
    *   return -1 on failure else 0
    */
    virtual int32_t SetCSRCs(
        const uint32_t arrOfCSRC[kRtpCsrcSize],
        const uint8_t arrLength) = 0;

    /*
    *   includes CSRCs in RTP header if enabled
    *
    *   include CSRC - on/off
    *
    *    default:on
    *
    *   return -1 on failure else 0
    */
    virtual int32_t SetCSRCStatus(const bool include) = 0;

    /*
    * Turn on/off sending RTX (RFC 4588) on a specific SSRC.
    */
    virtual int32_t SetRTXSendStatus(RtxMode mode, bool set_ssrc,
                                     uint32_t ssrc) = 0;

    // Sets the payload type to use when sending RTX packets. Note that this
    // doesn't enable RTX, only the payload type is set.
    virtual void SetRtxSendPayloadType(int payload_type) = 0;

    /*
    * Get status of sending RTX (RFC 4588) on a specific SSRC.
    */
    virtual int32_t RTXSendStatus(RtxMode* mode, uint32_t* ssrc,
                                  int* payloadType) const = 0;

    /*
    *   sends kRtcpByeCode when going from true to false
    *
    *   sending - on/off
    *
    *   return -1 on failure else 0
    */
    virtual int32_t SetSendingStatus(const bool sending) = 0;

    /*
    *   get send status
    */
    virtual bool Sending() const = 0;

    /*
    *   Starts/Stops media packets, on by default
    *
    *   sending - on/off
    *
    *   return -1 on failure else 0
    */
    virtual int32_t SetSendingMediaStatus(const bool sending) = 0;

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
        const FrameType frameType,
        const int8_t payloadType,
        const uint32_t timeStamp,
        int64_t capture_time_ms,
        const uint8_t* payloadData,
        const uint32_t payloadSize,
        const RTPFragmentationHeader* fragmentation = NULL,
        const RTPVideoHeader* rtpVideoHdr = NULL) = 0;

    virtual void TimeToSendPacket(uint32_t ssrc, uint16_t sequence_number,
                                  int64_t capture_time_ms) = 0;

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
    *
    *   return -1 on failure else 0
    */
    virtual int32_t SetRTCPStatus(const RTCPMethod method) = 0;

    /*
    *   Set RTCP CName (i.e unique identifier)
    *
    *   return -1 on failure else 0
    */
    virtual int32_t SetCNAME(const char cName[RTCP_CNAME_SIZE]) = 0;

    /*
    *   Get RTCP CName (i.e unique identifier)
    *
    *   return -1 on failure else 0
    */
    virtual int32_t CNAME(char cName[RTCP_CNAME_SIZE]) = 0;

    /*
    *   Get remote CName
    *
    *   return -1 on failure else 0
    */
    virtual int32_t RemoteCNAME(
        const uint32_t remoteSSRC,
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
    virtual int32_t AddMixedCNAME(
        const uint32_t SSRC,
        const char cName[RTCP_CNAME_SIZE]) = 0;

    /*
    *   RemoveMixedCNAME
    *
    *   return -1 on failure else 0
    */
    virtual int32_t RemoveMixedCNAME(const uint32_t SSRC) = 0;

    /*
    *   Get RoundTripTime
    *
    *   return -1 on failure else 0
    */
    virtual int32_t RTT(const uint32_t remoteSSRC,
                        uint16_t* RTT,
                        uint16_t* avgRTT,
                        uint16_t* minRTT,
                        uint16_t* maxRTT) const = 0 ;

    /*
    *   Reset RoundTripTime statistics
    *
    *   return -1 on failure else 0
    */
    virtual int32_t ResetRTT(const uint32_t remoteSSRC)= 0 ;

    /*
     * Sets the estimated RTT, to be used for receive only modules without
     * possibility of calculating its own RTT.
     */
    virtual void SetRtt(uint32_t rtt) = 0;

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
    virtual int32_t SendRTCPSliceLossIndication(
        const uint8_t pictureID) = 0;

    /*
    *   Reset RTP statistics
    *
    *   return -1 on failure else 0
    */
    virtual int32_t ResetStatisticsRTP() = 0;

    /*
    *   statistics of our localy created statistics of the received RTP stream
    *
    *   return -1 on failure else 0
    */
    virtual int32_t StatisticsRTP(
        uint8_t* fraction_lost,  // scale 0 to 255
        uint32_t* cum_lost,      // number of lost packets
        uint32_t* ext_max,       // highest sequence number received
        uint32_t* jitter,
        uint32_t* max_jitter = NULL) const = 0;

    /*
    *   Reset RTP data counters for the receiving side
    *
    *   return -1 on failure else 0
    */
    virtual int32_t ResetReceiveDataCountersRTP() = 0;

    /*
    *   Reset RTP data counters for the sending side
    *
    *   return -1 on failure else 0
    */
    virtual int32_t ResetSendDataCountersRTP() = 0;

    /*
    *   statistics of the amount of data sent and received
    *
    *   return -1 on failure else 0
    */
    virtual int32_t DataCountersRTP(
        uint32_t* bytesSent,
        uint32_t* packetsSent,
        uint32_t* bytesReceived,
        uint32_t* packetsReceived) const = 0;
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
    virtual int32_t AddRTCPReportBlock(
        const uint32_t SSRC,
        const RTCPReportBlock* receiveBlock) = 0;

    /*
    *   RemoveRTCPReportBlock
    *
    *   return -1 on failure else 0
    */
    virtual int32_t RemoveRTCPReportBlock(const uint32_t SSRC) = 0;

    /*
    *   (APP) Application specific data
    *
    *   return -1 on failure else 0
    */
    virtual int32_t SetRTCPApplicationSpecificData(
        const uint8_t subType,
        const uint32_t name,
        const uint8_t* data,
        const uint16_t length) = 0;
    /*
    *   (XR) VOIP metric
    *
    *   return -1 on failure else 0
    */
    virtual int32_t SetRTCPVoIPMetrics(
        const RTCPVoIPMetric* VoIPMetric) = 0;

    /*
    *  (REMB) Receiver Estimated Max Bitrate
    */
    virtual bool REMB() const = 0;

    virtual int32_t SetREMBStatus(const bool enable) = 0;

    virtual int32_t SetREMBData(const uint32_t bitrate,
                                const uint8_t numberOfSSRC,
                                const uint32_t* SSRC) = 0;

    /*
    *   (IJ) Extended jitter report.
    */
    virtual bool IJ() const = 0;

    virtual int32_t SetIJStatus(const bool enable) = 0;

    /*
    *   (TMMBR) Temporary Max Media Bit Rate
    */
    virtual bool TMMBR() const = 0;

    /*
    *
    *   return -1 on failure else 0
    */
    virtual int32_t SetTMMBRStatus(const bool enable) = 0;

    /*
    *   (NACK)
    */
    virtual NACKMethod NACK() const  = 0;

    /*
    *   Turn negative acknowledgement requests on/off
    *   |max_reordering_threshold| should be set to how much a retransmitted
    *   packet can be expected to be reordered (in sequence numbers) compared to
    *   a packet which has not been retransmitted.
    *
    *   return -1 on failure else 0
    */
    virtual int32_t SetNACKStatus(const NACKMethod method,
                                  int max_reordering_threshold) = 0;

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
    virtual int32_t SendNACK(const uint16_t* nackList,
                             const uint16_t size) = 0;

    /*
    *   Store the sent packets, needed to answer to a Negative acknowledgement
    *   requests
    *
    *   return -1 on failure else 0
    */
    virtual int32_t SetStorePacketsStatus(
        const bool enable,
        const uint16_t numberToStore) = 0;

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
    virtual int32_t SetAudioPacketSize(
        const uint16_t packetSizeSamples) = 0;

    /*
    *   Forward DTMF to decoder for playout.
    *
    *   return -1 on failure else 0
    */
    virtual int SetTelephoneEventForwardToDecoder(bool forwardToDecoder) = 0;

    /*
    *   Returns true if received DTMF events are forwarded to the decoder using
    *    the OnPlayTelephoneEvent callback.
    */
    virtual bool TelephoneEventForwardToDecoder() const = 0;

    /*
    *   SendTelephoneEventActive
    *
    *   return true if we currently send a telephone event and 100 ms after an
    *   event is sent used to prevent the telephone event tone to be recorded
    *   by the microphone and send inband just after the tone has ended.
    */
    virtual bool SendTelephoneEventActive(
        int8_t& telephoneEvent) const = 0;

    /*
    *   Send a TelephoneEvent tone using RFC 2833 (4733)
    *
    *   return -1 on failure else 0
    */
    virtual int32_t SendTelephoneEventOutband(
        const uint8_t key,
        const uint16_t time_ms,
        const uint8_t level) = 0;

    /*
    *   Set payload type for Redundant Audio Data RFC 2198
    *
    *   return -1 on failure else 0
    */
    virtual int32_t SetSendREDPayloadType(
        const int8_t payloadType) = 0;

    /*
    *   Get payload type for Redundant Audio Data RFC 2198
    *
    *   return -1 on failure else 0
    */
     virtual int32_t SendREDPayloadType(
         int8_t& payloadType) const = 0;

     /*
     * Set status and ID for header-extension-for-audio-level-indication.
     * See http://tools.ietf.org/html/rfc6464 for more details.
     *
     * return -1 on failure else 0
     */
     virtual int32_t SetRTPAudioLevelIndicationStatus(
         const bool enable,
         const uint8_t ID) = 0;

     /*
     * Get status and ID for header-extension-for-audio-level-indication.
     *
     * return -1 on failure else 0
     */
     virtual int32_t GetRTPAudioLevelIndicationStatus(
         bool& enable,
         uint8_t& ID) const = 0;

     /*
     * Store the audio level in dBov for header-extension-for-audio-level-
     * indication.
     * This API shall be called before transmision of an RTP packet to ensure
     * that the |level| part of the extended RTP header is updated.
     *
     * return -1 on failure else 0.
     */
     virtual int32_t SetAudioLevel(const uint8_t level_dBov) = 0;

    /**************************************************************************
    *
    *   Video
    *
    ***************************************************************************/

    /*
    *   Set the estimated camera delay in MS
    *
    *   return -1 on failure else 0
    */
    virtual int32_t SetCameraDelay(const int32_t delayMS) = 0;

    /*
    *   Set the target send bitrate
    */
    virtual void SetTargetSendBitrate(const uint32_t bitrate) = 0;

    /*
    *   Turn on/off generic FEC
    *
    *   return -1 on failure else 0
    */
    virtual int32_t SetGenericFECStatus(
        const bool enable,
        const uint8_t payloadTypeRED,
        const uint8_t payloadTypeFEC) = 0;

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
    virtual int32_t SetKeyFrameRequestMethod(
        const KeyFrameRequestMethod method) = 0;

    /*
    *   send a request for a keyframe
    *
    *   return -1 on failure else 0
    */
    virtual int32_t RequestKeyFrame() = 0;
};
} // namespace webrtc
#endif // WEBRTC_MODULES_RTP_RTCP_INTERFACE_RTP_RTCP_H_
