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

#include "module.h"
#include "rtp_rtcp_defines.h"

namespace webrtc {
// forward declaration
class Transport;

class RtpRtcp : public Module
{
public:
    /*
    *   create a RTP/RTCP module object using the system clock
    *
    *   id      - unique identifier of this RTP/RTCP module object
    *   audio   - true for a audio version of the RTP/RTCP module object false will create a video version
    */
    static RtpRtcp* CreateRtpRtcp(const WebRtc_Word32 id,
                                  const bool audio);

    /*
    *   create a RTP/RTCP module object
    *
    *   id      - unique identifier of this RTP/RTCP module object
    *   audio   - true for a audio version of the RTP/RTCP module object
    *             false will create a video version
    *   clock   - the clock to use to read time; must not be NULL
    */
    static RtpRtcp* CreateRtpRtcp(const WebRtc_Word32 id,
                                  const bool audio,
                                  RtpRtcpClock* clock);

    /*
    *   destroy a RTP/RTCP module object
    *
    *   module  - object to destroy
    */
    static void DestroyRtpRtcp(RtpRtcp* module);

    /*
    *   Change the unique identifier of this object
    *
    *   id      - new unique identifier of this RTP/RTCP module object
    */
    virtual WebRtc_Word32 ChangeUniqueId(const WebRtc_Word32 id) = 0;

    /*
    *   De-muxing functionality for conferencing
    *
    *   register a module that will act as a default module for this module
    *   used for feedback messages back to the encoder when one encoded stream
    *   is sent to multiple destinations
    *
    *   module  - default module
    */
    virtual WebRtc_Word32 RegisterDefaultModule(RtpRtcp* module) = 0;

    /*
    *   unregister the default module
    *   will stop the demuxing feedback
    */
    virtual WebRtc_Word32 DeRegisterDefaultModule() = 0;

    /*
    *   returns true if a default module is registered, false otherwise
    */
    virtual bool DefaultModuleRegistered() = 0;

    /*
    *   returns number of registered child modules
    */
    virtual WebRtc_UWord32 NumberChildModules() = 0;

    /*
    *   Lip-sync between voice-video
    *
    *   module  - audio module
    *
    *   Note: only allowed on a video module
    */
    virtual WebRtc_Word32 RegisterSyncModule(RtpRtcp* module) = 0;

    /*
    *   Turn off lip-sync between voice-video
    */
    virtual WebRtc_Word32 DeRegisterSyncModule() = 0;

    /**************************************************************************
    *
    *   Receiver functions
    *
    ***************************************************************************/

    /*
    *   Initialize receive side
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 InitReceiver() = 0;

    /*
    *   Used by the module to deliver the incoming data to the codec module
    *
    *   incomingDataCallback    - callback object that will receive the incoming data
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 RegisterIncomingDataCallback(RtpData* incomingDataCallback) = 0;

    /*
    *   Used by the module to deliver messages to the codec module/appliation
    *
    *   incomingMessagesCallback    - callback object that will receive the incoming messages
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 RegisterIncomingRTPCallback(RtpFeedback* incomingMessagesCallback) = 0;

    /*
    *   configure a RTP packet timeout value
    *
    *   RTPtimeoutMS   - time in milliseconds after last received RTP packet
    *   RTCPtimeoutMS  - time in milliseconds after last received RTCP packet
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 SetPacketTimeout(const WebRtc_UWord32 RTPtimeoutMS,
                                         const WebRtc_UWord32 RTCPtimeoutMS) = 0;

    /*
    *   Set periodic dead or alive notification
    *
    *   enable              - turn periodic dead or alive notification on/off
    *   sampleTimeSeconds   - sample interval in seconds for dead or alive notifications
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 SetPeriodicDeadOrAliveStatus(const bool enable,
                                                     const WebRtc_UWord8 sampleTimeSeconds) = 0;

    /*
    *   Get periodic dead or alive notification status
    *
    *   enable              - periodic dead or alive notification on/off
    *   sampleTimeSeconds   - sample interval in seconds for dead or alive notifications
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 PeriodicDeadOrAliveStatus(bool &enable,
                                                  WebRtc_UWord8 &sampleTimeSeconds) = 0;

    /*
    *   set voice codec name and payload type
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 RegisterReceivePayload(
        const CodecInst& voiceCodec) = 0;

    /*
    *   set video codec name and payload type
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 RegisterReceivePayload(
        const VideoCodec& videoCodec) = 0;

    /*
    *   get payload type for a voice codec
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 ReceivePayloadType(
        const CodecInst& voiceCodec,
        WebRtc_Word8* plType) = 0;

    /*
    *   get payload type for a video codec
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 ReceivePayloadType(
        const VideoCodec& videoCodec,
        WebRtc_Word8* plType) = 0;

    /*
    *   Remove a registered payload type from list of accepted payloads
    *
    *   payloadType - payload type of codec
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 DeRegisterReceivePayload(
        const WebRtc_Word8 payloadType) = 0;

   /*
    *   (De)register RTP header extension type and id.
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 RegisterReceiveRtpHeaderExtension(
        const RTPExtensionType type,
        const WebRtc_UWord8 id) = 0;

    virtual WebRtc_Word32 DeregisterReceiveRtpHeaderExtension(
        const RTPExtensionType type) = 0;

    /*
    *   Get last received remote timestamp
    */
    virtual WebRtc_UWord32 RemoteTimestamp() const = 0;

    /*
    *   Get the current estimated remote timestamp
    *
    *   timestamp   - estimated timestamp
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 EstimatedRemoteTimeStamp(WebRtc_UWord32& timestamp) const = 0;

    /*
    *   Get incoming SSRC
    */
    virtual WebRtc_UWord32 RemoteSSRC() const = 0;

    /*
    *   Get remote CSRC
    *
    *   arrOfCSRC   - array that will receive the CSRCs
    *
    *   return -1 on failure else the number of valid entries in the list
    */
    virtual WebRtc_Word32 RemoteCSRCs( WebRtc_UWord32 arrOfCSRC[kRtpCsrcSize]) const  = 0;

    /*
    *   get the currently configured SSRC filter
    *
    *   allowedSSRC - SSRC that will be allowed through
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 SSRCFilter(WebRtc_UWord32& allowedSSRC) const = 0;

    /*
    *   set a SSRC to be used as a filter for incoming RTP streams
    *
    *   allowedSSRC - SSRC that will be allowed through
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 SetSSRCFilter(const bool enable,
                                        const WebRtc_UWord32 allowedSSRC) = 0;

    /*
    * Turn on/off receiving RTX (RFC 4588) on a specific SSRC.
    */
    virtual WebRtc_Word32 SetRTXReceiveStatus(const bool enable,
                                              const WebRtc_UWord32 SSRC) = 0;

    /*
    * Get status of receiving RTX (RFC 4588) on a specific SSRC.
    */
    virtual WebRtc_Word32 RTXReceiveStatus(bool* enable,
                                           WebRtc_UWord32* SSRC) const = 0;

    /*
    *   called by the network module when we receive a packet
    *
    *   incomingPacket - incoming packet buffer
    *   packetLength   - length of incoming buffer
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 IncomingPacket(const WebRtc_UWord8* incomingPacket,
                                         const WebRtc_UWord16 packetLength) = 0;


    /*
    *    Option when not using the RegisterSyncModule function
    *
    *    Inform the module about the received audion NTP
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 IncomingAudioNTP(
        const WebRtc_UWord32 audioReceivedNTPsecs,
        const WebRtc_UWord32 audioReceivedNTPfrac,
        const WebRtc_UWord32 audioRTCPArrivalTimeSecs,
        const WebRtc_UWord32 audioRTCPArrivalTimeFrac) = 0;

    /**************************************************************************
    *
    *   Sender
    *
    ***************************************************************************/

    /*
    *   Initialize send side
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 InitSender() = 0;

    /*
    *   Used by the module to send RTP and RTCP packet to the network module
    *
    *   outgoingTransport   - transport object that will be called when packets are ready to be sent out on the network
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 RegisterSendTransport(Transport* outgoingTransport) = 0;

    /*
    *   set MTU
    *
    *   size    -  Max transfer unit in bytes, default is 1500
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 SetMaxTransferUnit(const WebRtc_UWord16 size) = 0;

    /*
    *   set transtport overhead
    *   default is IPv4 and UDP with no encryption
    *
    *   TCP                     - true for TCP false UDP
    *   IPv6                    - true for IP version 6 false for version 4
    *   authenticationOverhead  - number of bytes to leave for an authentication header
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 SetTransportOverhead(const bool TCP,
                                             const bool IPV6,
                                             const WebRtc_UWord8 authenticationOverhead = 0) = 0;

    /*
    *   Get max payload length
    *
    *   A combination of the configuration MaxTransferUnit and TransportOverhead.
    *   Does not account FEC/ULP/RED overhead if FEC is enabled.
    *   Does not account for RTP headers
    */
    virtual WebRtc_UWord16 MaxPayloadLength() const = 0;

    /*
    *   Get max data payload length
    *
    *   A combination of the configuration MaxTransferUnit, headers and TransportOverhead.
    *   Takes into account FEC/ULP/RED overhead if FEC is enabled.
    *   Takes into account RTP headers
    */
    virtual WebRtc_UWord16 MaxDataPayloadLength() const = 0;

    /*
    *   set codec name and payload type
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 RegisterSendPayload(
        const CodecInst& voiceCodec) = 0;

    /*
    *   set codec name and payload type
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 RegisterSendPayload(
        const VideoCodec& videoCodec) = 0;

    /*
    *   Unregister a send payload
    *
    *   payloadType - payload type of codec
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 DeRegisterSendPayload(
        const WebRtc_Word8 payloadType) = 0;

   /*
    *   (De)register RTP header extension type and id.
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 RegisterSendRtpHeaderExtension(
        const RTPExtensionType type,
        const WebRtc_UWord8 id) = 0;

    virtual WebRtc_Word32 DeregisterSendRtpHeaderExtension(
        const RTPExtensionType type) = 0;

   /*
    *   Enable/disable traffic smoothing of sending stream.
    */
    virtual void SetTransmissionSmoothingStatus(const bool enable) = 0;

    virtual bool TransmissionSmoothingStatus() const = 0;

    /*
    *   get start timestamp
    */
    virtual WebRtc_UWord32 StartTimestamp() const = 0;

    /*
    *   configure start timestamp, default is a random number
    *
    *   timestamp   - start timestamp
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 SetStartTimestamp(
        const WebRtc_UWord32 timestamp) = 0;

    /*
    *   Get SequenceNumber
    */
    virtual WebRtc_UWord16 SequenceNumber() const = 0;

    /*
    *   Set SequenceNumber, default is a random number
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 SetSequenceNumber(const WebRtc_UWord16 seq) = 0;

    /*
    *   Get SSRC
    */
    virtual WebRtc_UWord32 SSRC() const = 0;

    /*
    *   configure SSRC, default is a random number
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 SetSSRC(const WebRtc_UWord32 ssrc) = 0;

    /*
    *   Get CSRC
    *
    *   arrOfCSRC   - array of CSRCs
    *
    *   return -1 on failure else number of valid entries in the array
    */
    virtual WebRtc_Word32 CSRCs(
        WebRtc_UWord32 arrOfCSRC[kRtpCsrcSize]) const = 0;

    /*
    *   Set CSRC
    *
    *   arrOfCSRC   - array of CSRCs
    *   arrLength   - number of valid entries in the array
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 SetCSRCs(
        const WebRtc_UWord32 arrOfCSRC[kRtpCsrcSize],
        const WebRtc_UWord8 arrLength) = 0;

    /*
    *   includes CSRCs in RTP header if enabled
    *
    *   include CSRC - on/off
    *
    *    default:on
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 SetCSRCStatus(const bool include) = 0;

    /*
    * Turn on/off sending RTX (RFC 4588) on a specific SSRC.
    */
    virtual WebRtc_Word32 SetRTXSendStatus(const bool enable,
                                           const bool setSSRC,
                                           const WebRtc_UWord32 SSRC) = 0;


    /*
    * Get status of sending RTX (RFC 4588) on a specific SSRC.
    */
    virtual WebRtc_Word32 RTXSendStatus(bool* enable,
                                        WebRtc_UWord32* SSRC) const = 0;

    /*
    *   sends kRtcpByeCode when going from true to false
    *
    *   sending - on/off
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 SetSendingStatus(const bool sending) = 0;

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
    virtual WebRtc_Word32 SetSendingMediaStatus(const bool sending) = 0;

    /*
    *   get send status
    */
    virtual bool SendingMedia() const = 0;

    /*
    *   get sent bitrate in Kbit/s
    */
    virtual void BitrateSent(WebRtc_UWord32* totalRate,
                             WebRtc_UWord32* videoRate,
                             WebRtc_UWord32* fecRate,
                             WebRtc_UWord32* nackRate) const = 0;

    /*
     *  Get the send-side estimate of the available bandwidth.
     */
    virtual int EstimatedSendBandwidth(
        WebRtc_UWord32* available_bandwidth) const = 0;

    /*
     *  Get the receive-side estimate of the available bandwidth.
     */
    virtual int EstimatedReceiveBandwidth(
        WebRtc_UWord32* available_bandwidth) const = 0;

    /*
    *   Used by the codec module to deliver a video or audio frame for packetization
    *
    *   frameType       - type of frame to send
    *   payloadType     - payload type of frame to send
    *   timestamp       - timestamp of frame to send
    *   payloadData     - payload buffer of frame to send
    *   payloadSize     - size of payload buffer to send
    *   fragmentation   - fragmentation offset data for fragmented frames such as layers or RED
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 SendOutgoingData(
        const FrameType frameType,
        const WebRtc_Word8 payloadType,
        const WebRtc_UWord32 timeStamp,
        const WebRtc_UWord8* payloadData,
        const WebRtc_UWord32 payloadSize,
        const RTPFragmentationHeader* fragmentation = NULL,
        const RTPVideoHeader* rtpVideoHdr = NULL) = 0;

    /**************************************************************************
    *
    *   RTCP
    *
    ***************************************************************************/

    /*
    *   RegisterIncomingRTCPCallback
    *
    *   incomingMessagesCallback    - callback object that will receive messages from RTCP
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 RegisterIncomingRTCPCallback(RtcpFeedback* incomingMessagesCallback) = 0;

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
    virtual WebRtc_Word32 SetRTCPStatus(const RTCPMethod method) = 0;

    /*
    *   Set RTCP CName (i.e unique identifier)
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 SetCNAME(const char cName[RTCP_CNAME_SIZE]) = 0;

    /*
    *   Get RTCP CName (i.e unique identifier)
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 CNAME(char cName[RTCP_CNAME_SIZE]) = 0;

    /*
    *   Get remote CName
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 RemoteCNAME(
        const WebRtc_UWord32 remoteSSRC,
        char cName[RTCP_CNAME_SIZE]) const = 0;

    /*
    *   Get remote NTP
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 RemoteNTP(
        WebRtc_UWord32 *ReceivedNTPsecs,
        WebRtc_UWord32 *ReceivedNTPfrac,
        WebRtc_UWord32 *RTCPArrivalTimeSecs,
        WebRtc_UWord32 *RTCPArrivalTimeFrac) const  = 0;

    /*
    *   AddMixedCNAME
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 AddMixedCNAME(
        const WebRtc_UWord32 SSRC,
        const char cName[RTCP_CNAME_SIZE]) = 0;

    /*
    *   RemoveMixedCNAME
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 RemoveMixedCNAME(const WebRtc_UWord32 SSRC) = 0;

    /*
    *   Get RoundTripTime
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 RTT(const WebRtc_UWord32 remoteSSRC,
                              WebRtc_UWord16* RTT,
                              WebRtc_UWord16* avgRTT,
                              WebRtc_UWord16* minRTT,
                              WebRtc_UWord16* maxRTT) const = 0 ;

    /*
    *   Reset RoundTripTime statistics
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 ResetRTT(const WebRtc_UWord32 remoteSSRC)= 0 ;

    /*
    *   Force a send of a RTCP packet
    *   normal SR and RR are triggered via the process function
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 SendRTCP(WebRtc_UWord32 rtcpPacketType = kRtcpReport) = 0;

    /*
    *    Good state of RTP receiver inform sender
    */
    virtual WebRtc_Word32 SendRTCPReferencePictureSelection(const WebRtc_UWord64 pictureID) = 0;

    /*
    *    Send a RTCP Slice Loss Indication (SLI)
    *    6 least significant bits of pictureID
    */
    virtual WebRtc_Word32 SendRTCPSliceLossIndication(const WebRtc_UWord8 pictureID) = 0;

    /*
    *   Reset RTP statistics
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 ResetStatisticsRTP() = 0;

    /*
    *   statistics of our localy created statistics of the received RTP stream
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 StatisticsRTP(WebRtc_UWord8  *fraction_lost,  // scale 0 to 255
                                      WebRtc_UWord32 *cum_lost,       // number of lost packets
                                      WebRtc_UWord32 *ext_max,        // highest sequence number received
                                      WebRtc_UWord32 *jitter,
                                      WebRtc_UWord32 *max_jitter = NULL) const = 0;

    /*
    *   Reset RTP data counters for the receiving side
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 ResetReceiveDataCountersRTP() = 0;

    /*
    *   Reset RTP data counters for the sending side
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 ResetSendDataCountersRTP() = 0;

    /*
    *   statistics of the amount of data sent and received
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 DataCountersRTP(WebRtc_UWord32 *bytesSent,
                                        WebRtc_UWord32 *packetsSent,
                                        WebRtc_UWord32 *bytesReceived,
                                        WebRtc_UWord32 *packetsReceived) const = 0;
    /*
    *   Get received RTCP sender info
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 RemoteRTCPStat(RTCPSenderInfo* senderInfo) = 0;

    /*
    *   Get received RTCP report block
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 RemoteRTCPStat(
        std::vector<RTCPReportBlock>* receiveBlocks) const = 0;
    /*
    *   Set received RTCP report block
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 AddRTCPReportBlock(
        const WebRtc_UWord32 SSRC,
        const RTCPReportBlock* receiveBlock) = 0;

    /*
    *   RemoveRTCPReportBlock
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 RemoveRTCPReportBlock(const WebRtc_UWord32 SSRC) = 0;

    /*
    *   (APP) Application specific data
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 SetRTCPApplicationSpecificData(const WebRtc_UWord8 subType,
                                                       const WebRtc_UWord32 name,
                                                       const WebRtc_UWord8* data,
                                                       const WebRtc_UWord16 length) = 0;
    /*
    *   (XR) VOIP metric
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 SetRTCPVoIPMetrics(const RTCPVoIPMetric* VoIPMetric) = 0;

    /*
    *  (REMB) Receiver Estimated Max Bitrate
    */
    virtual bool REMB() const = 0;

    virtual WebRtc_Word32 SetREMBStatus(const bool enable) = 0;

    virtual WebRtc_Word32 SetREMBData(const WebRtc_UWord32 bitrate,
                                      const WebRtc_UWord8 numberOfSSRC,
                                      const WebRtc_UWord32* SSRC) = 0;

    // Used to set maximum bitrate estimate received in a REMB packet.
    virtual WebRtc_Word32 SetMaximumBitrateEstimate(
        const WebRtc_UWord32 bitrate) = 0;

    // Registers an observer to call when the estimate of the incoming channel
    // changes.
    virtual bool SetRemoteBitrateObserver(
        RtpRemoteBitrateObserver* observer) = 0;

    /*
    *   (IJ) Extended jitter report.
    */
    virtual bool IJ() const = 0;

    virtual WebRtc_Word32 SetIJStatus(const bool enable) = 0;

    /*
    *   (TMMBR) Temporary Max Media Bit Rate
    */
    virtual bool TMMBR() const = 0;

    /*
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 SetTMMBRStatus(const bool enable) = 0;

    /*
    *   (NACK)
    */
    virtual NACKMethod NACK() const  = 0;

    /*
    *   Turn negative acknowledgement requests on/off
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 SetNACKStatus(const NACKMethod method) = 0;

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
    virtual WebRtc_Word32 SendNACK(const WebRtc_UWord16* nackList,
                                   const WebRtc_UWord16 size) = 0;

    /*
    *   Store the sent packets, needed to answer to a Negative acknowledgement requests
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 SetStorePacketsStatus(const bool enable, const WebRtc_UWord16 numberToStore = 200) = 0;

    /**************************************************************************
    *
    *   Audio
    *
    ***************************************************************************/

    /*
    *   RegisterAudioCallback
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 RegisterAudioCallback(RtpAudioFeedback* messagesCallback) = 0;

    /*
    *   set audio packet size, used to determine when it's time to send a DTMF packet in silence (CNG)
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 SetAudioPacketSize(const WebRtc_UWord16 packetSizeSamples) = 0;

    /*
    *   Outband TelephoneEvent(DTMF) detection
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 SetTelephoneEventStatus(const bool enable,
                                                const bool forwardToDecoder,
                                                const bool detectEndOfTone = false) = 0;

    /*
    *   Is outband TelephoneEvent(DTMF) turned on/off?
    */
    virtual bool TelephoneEvent() const = 0;

    /*
    *   Returns true if received DTMF events are forwarded to the decoder using
    *    the OnPlayTelephoneEvent callback.
    */
    virtual bool TelephoneEventForwardToDecoder() const = 0;

    /*
    *   SendTelephoneEventActive
    *
    *   return true if we currently send a telephone event and 100 ms after an event is sent
    *   used to prevent teh telephone event tone to be recorded by the microphone and send inband
    *   just after the tone has ended
    */
    virtual bool SendTelephoneEventActive(WebRtc_Word8& telephoneEvent) const = 0;

    /*
    *   Send a TelephoneEvent tone using RFC 2833 (4733)
    *
    *   return -1 on failure else 0
    */
      virtual WebRtc_Word32 SendTelephoneEventOutband(const WebRtc_UWord8 key,
                                                  const WebRtc_UWord16 time_ms,
                                                  const WebRtc_UWord8 level) = 0;

    /*
    *   Set payload type for Redundant Audio Data RFC 2198
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 SetSendREDPayloadType(const WebRtc_Word8 payloadType) = 0;

    /*
    *   Get payload type for Redundant Audio Data RFC 2198
    *
    *   return -1 on failure else 0
    */
     virtual WebRtc_Word32 SendREDPayloadType(WebRtc_Word8& payloadType) const = 0;

     /*
     * Set status and ID for header-extension-for-audio-level-indication.
     * See https://datatracker.ietf.org/doc/draft-lennox-avt-rtp-audio-level-exthdr/
     * for more details.
     *
     * return -1 on failure else 0
     */
     virtual WebRtc_Word32 SetRTPAudioLevelIndicationStatus(const bool enable,
                                                          const WebRtc_UWord8 ID) = 0;

     /*
     * Get status and ID for header-extension-for-audio-level-indication.
     *
     * return -1 on failure else 0
     */
     virtual WebRtc_Word32 GetRTPAudioLevelIndicationStatus(bool& enable,
                                                          WebRtc_UWord8& ID) const = 0;

     /*
     * Store the audio level in dBov for header-extension-for-audio-level-indication.
     * This API shall be called before transmision of an RTP packet to ensure
     * that the |level| part of the extended RTP header is updated.
     *
     * return -1 on failure else 0.
     */
     virtual WebRtc_Word32 SetAudioLevel(const WebRtc_UWord8 level_dBov) = 0;

    /**************************************************************************
    *
    *   Video
    *
    ***************************************************************************/

    /*
    *   Register a callback object that will receive callbacks for video related events
    *   such as an incoming key frame request.
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 RegisterIncomingVideoCallback(RtpVideoFeedback* incomingMessagesCallback) = 0;

    /*
    *   Set the estimated camera delay in MS
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 SetCameraDelay(const WebRtc_Word32 delayMS) = 0;

    /*
    *   Set the start and max send bitrate
    *   used by the bandwidth management
    *
    *   Not calling this or setting startBitrateKbit to 0 disables the bandwidth management
    *
    *   minBitrateKbit = 0 equals no min bitrate
    *   maxBitrateKbit = 0 equals no max bitrate
    *
    *   return -1 on failure else 0
    */
    virtual void SetSendBitrate(const WebRtc_UWord32 startBitrate,
                                const WebRtc_UWord16 minBitrateKbit,
                                const WebRtc_UWord16 maxBitrateKbit) = 0;

    /*
    *   Turn on/off generic FEC
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 SetGenericFECStatus(const bool enable,
                                            const WebRtc_UWord8 payloadTypeRED,
                                            const WebRtc_UWord8 payloadTypeFEC) = 0;

    /*
    *   Get generic FEC setting
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 GenericFECStatus(bool& enable,
                                         WebRtc_UWord8& payloadTypeRED,
                                         WebRtc_UWord8& payloadTypeFEC) = 0;


    virtual WebRtc_Word32 SetFecParameters(
        const FecProtectionParams* delta_params,
        const FecProtectionParams* key_params) = 0;

    /*
    *   Set method for requestion a new key frame
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 SetKeyFrameRequestMethod(const KeyFrameRequestMethod method) = 0;

    /*
    *   send a request for a keyframe
    *
    *   return -1 on failure else 0
    */
    virtual WebRtc_Word32 RequestKeyFrame() = 0;
};
} // namespace webrtc
#endif // WEBRTC_MODULES_RTP_RTCP_INTERFACE_RTP_RTCP_H_
