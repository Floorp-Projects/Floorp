/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RTCP_IMPL_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RTCP_IMPL_H_

#include <list>

#include "modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "modules/rtp_rtcp/source/rtcp_receiver.h"
#include "modules/rtp_rtcp/source/rtcp_sender.h"
#include "modules/rtp_rtcp/source/rtp_receiver.h"
#include "modules/rtp_rtcp/source/rtp_sender.h"
#include "system_wrappers/interface/scoped_ptr.h"

#ifdef MATLAB
class MatlabPlot;
#endif

namespace webrtc {

class ModuleRtpRtcpImpl : public RtpRtcp {
 public:
    explicit ModuleRtpRtcpImpl(const RtpRtcp::Configuration& configuration);

    virtual ~ModuleRtpRtcpImpl();

    // returns the number of milliseconds until the module want a worker thread to call Process
    virtual WebRtc_Word32 TimeUntilNextProcess();

    // Process any pending tasks such as timeouts
    virtual WebRtc_Word32 Process();

    /**
    *   Receiver
    */
    // configure a timeout value
    virtual WebRtc_Word32 SetPacketTimeout(const WebRtc_UWord32 RTPtimeoutMS,
                                           const WebRtc_UWord32 RTCPtimeoutMS);

    // Set periodic dead or alive notification
    virtual WebRtc_Word32 SetPeriodicDeadOrAliveStatus(
        const bool enable,
        const WebRtc_UWord8 sampleTimeSeconds);

    // Get periodic dead or alive notification status
    virtual WebRtc_Word32 PeriodicDeadOrAliveStatus(
        bool &enable,
        WebRtc_UWord8 &sampleTimeSeconds);

    virtual WebRtc_Word32 RegisterReceivePayload(const CodecInst& voiceCodec);

    virtual WebRtc_Word32 RegisterReceivePayload(const VideoCodec& videoCodec);

    virtual WebRtc_Word32 ReceivePayloadType(const CodecInst& voiceCodec,
                                             WebRtc_Word8* plType);

    virtual WebRtc_Word32 ReceivePayloadType(const VideoCodec& videoCodec,
                                             WebRtc_Word8* plType);

    virtual WebRtc_Word32 DeRegisterReceivePayload(
        const WebRtc_Word8 payloadType);

    // register RTP header extension
    virtual WebRtc_Word32 RegisterReceiveRtpHeaderExtension(
        const RTPExtensionType type,
        const WebRtc_UWord8 id);

    virtual WebRtc_Word32 DeregisterReceiveRtpHeaderExtension(
        const RTPExtensionType type);

    // get the currently configured SSRC filter
    virtual WebRtc_Word32 SSRCFilter(WebRtc_UWord32& allowedSSRC) const;

    // set a SSRC to be used as a filter for incoming RTP streams
    virtual WebRtc_Word32 SetSSRCFilter(const bool enable, const WebRtc_UWord32 allowedSSRC);

    // Get last received remote timestamp
    virtual WebRtc_UWord32 RemoteTimestamp() const;

    // Get the current estimated remote timestamp
    virtual WebRtc_Word32 EstimatedRemoteTimeStamp(WebRtc_UWord32& timestamp) const;

    virtual WebRtc_UWord32 RemoteSSRC() const;

    virtual WebRtc_Word32 RemoteCSRCs( WebRtc_UWord32 arrOfCSRC[kRtpCsrcSize]) const ;

    virtual WebRtc_Word32 SetRTXReceiveStatus(const bool enable,
                                              const WebRtc_UWord32 SSRC);

    virtual WebRtc_Word32 RTXReceiveStatus(bool* enable,
                                           WebRtc_UWord32* SSRC) const;

    // called by the network module when we receive a packet
    virtual WebRtc_Word32 IncomingPacket( const WebRtc_UWord8* incomingPacket,
                                        const WebRtc_UWord16 packetLength);

    /**
    *   Sender
    */
    virtual WebRtc_Word32 RegisterSendPayload(const CodecInst& voiceCodec);

    virtual WebRtc_Word32 RegisterSendPayload(const VideoCodec& videoCodec);

    virtual WebRtc_Word32 DeRegisterSendPayload(const WebRtc_Word8 payloadType);

    virtual WebRtc_Word8 SendPayloadType() const;

    // register RTP header extension
    virtual WebRtc_Word32 RegisterSendRtpHeaderExtension(
        const RTPExtensionType type,
        const WebRtc_UWord8 id);

    virtual WebRtc_Word32 DeregisterSendRtpHeaderExtension(
        const RTPExtensionType type);

    virtual void SetTransmissionSmoothingStatus(const bool enable);

    virtual bool TransmissionSmoothingStatus() const;

    // get start timestamp
    virtual WebRtc_UWord32 StartTimestamp() const;

    // configure start timestamp, default is a random number
    virtual WebRtc_Word32 SetStartTimestamp(const WebRtc_UWord32 timestamp);

    virtual WebRtc_UWord16 SequenceNumber() const;

    // Set SequenceNumber, default is a random number
    virtual WebRtc_Word32 SetSequenceNumber(const WebRtc_UWord16 seq);

    virtual WebRtc_UWord32 SSRC() const;

    // configure SSRC, default is a random number
    virtual WebRtc_Word32 SetSSRC(const WebRtc_UWord32 ssrc);

    virtual WebRtc_Word32 CSRCs( WebRtc_UWord32 arrOfCSRC[kRtpCsrcSize]) const ;

    virtual WebRtc_Word32 SetCSRCs( const WebRtc_UWord32 arrOfCSRC[kRtpCsrcSize],
                                  const WebRtc_UWord8 arrLength);

    virtual WebRtc_Word32 SetCSRCStatus(const bool include);

    virtual WebRtc_UWord32 PacketCountSent() const;

    virtual int CurrentSendFrequencyHz() const;

    virtual WebRtc_UWord32 ByteCountSent() const;

    virtual WebRtc_Word32 SetRTXSendStatus(const bool enable,
                                           const bool setSSRC,
                                           const WebRtc_UWord32 SSRC);

    virtual WebRtc_Word32 RTXSendStatus(bool* enable,
                                        WebRtc_UWord32* SSRC) const;

    // sends kRtcpByeCode when going from true to false
    virtual WebRtc_Word32 SetSendingStatus(const bool sending);

    virtual bool Sending() const;

    // Drops or relays media packets
    virtual WebRtc_Word32 SetSendingMediaStatus(const bool sending);

    virtual bool SendingMedia() const;

    // Used by the codec module to deliver a video or audio frame for packetization
    virtual WebRtc_Word32 SendOutgoingData(
        const FrameType frameType,
        const WebRtc_Word8 payloadType,
        const WebRtc_UWord32 timeStamp,
        int64_t capture_time_ms,
        const WebRtc_UWord8* payloadData,
        const WebRtc_UWord32 payloadSize,
        const RTPFragmentationHeader* fragmentation = NULL,
        const RTPVideoHeader* rtpVideoHdr = NULL);

    /*
    *   RTCP
    */

    // Get RTCP status
    virtual RTCPMethod RTCP() const;

    // configure RTCP status i.e on/off
    virtual WebRtc_Word32 SetRTCPStatus(const RTCPMethod method);

    // Set RTCP CName
    virtual WebRtc_Word32 SetCNAME(const char cName[RTCP_CNAME_SIZE]);

    // Get RTCP CName
    virtual WebRtc_Word32 CNAME(char cName[RTCP_CNAME_SIZE]);

    // Get remote CName
    virtual WebRtc_Word32 RemoteCNAME(const WebRtc_UWord32 remoteSSRC,
                                      char cName[RTCP_CNAME_SIZE]) const;

    // Get remote NTP
    virtual WebRtc_Word32 RemoteNTP(WebRtc_UWord32 *ReceivedNTPsecs,
                                  WebRtc_UWord32 *ReceivedNTPfrac,
                                  WebRtc_UWord32 *RTCPArrivalTimeSecs,
                                  WebRtc_UWord32 *RTCPArrivalTimeFrac) const ;

    virtual WebRtc_Word32 AddMixedCNAME(const WebRtc_UWord32 SSRC,
                                        const char cName[RTCP_CNAME_SIZE]);

    virtual WebRtc_Word32 RemoveMixedCNAME(const WebRtc_UWord32 SSRC);

    // Get RoundTripTime
    virtual WebRtc_Word32 RTT(const WebRtc_UWord32 remoteSSRC,
                            WebRtc_UWord16* RTT,
                            WebRtc_UWord16* avgRTT,
                            WebRtc_UWord16* minRTT,
                            WebRtc_UWord16* maxRTT) const;

    // Reset RoundTripTime statistics
    virtual WebRtc_Word32 ResetRTT(const WebRtc_UWord32 remoteSSRC);

    // Force a send of an RTCP packet
    // normal SR and RR are triggered via the process function
    virtual WebRtc_Word32 SendRTCP(WebRtc_UWord32 rtcpPacketType = kRtcpReport);

    // statistics of our localy created statistics of the received RTP stream
    virtual WebRtc_Word32 StatisticsRTP(WebRtc_UWord8  *fraction_lost,
                                      WebRtc_UWord32 *cum_lost,
                                      WebRtc_UWord32 *ext_max,
                                      WebRtc_UWord32 *jitter,
                                      WebRtc_UWord32 *max_jitter = NULL) const;

    // Reset RTP statistics
    virtual WebRtc_Word32 ResetStatisticsRTP();

    virtual WebRtc_Word32 ResetReceiveDataCountersRTP();

    virtual WebRtc_Word32 ResetSendDataCountersRTP();

    // statistics of the amount of data sent and received
    virtual WebRtc_Word32 DataCountersRTP(WebRtc_UWord32 *bytesSent,
                                          WebRtc_UWord32 *packetsSent,
                                          WebRtc_UWord32 *bytesReceived,
                                          WebRtc_UWord32 *packetsReceived) const;

    virtual WebRtc_Word32 ReportBlockStatistics(
        WebRtc_UWord8 *fraction_lost,
        WebRtc_UWord32 *cum_lost,
        WebRtc_UWord32 *ext_max,
        WebRtc_UWord32 *jitter,
        WebRtc_UWord32 *jitter_transmission_time_offset);

    // Get received RTCP report, sender info
    virtual WebRtc_Word32 RemoteRTCPStat( RTCPSenderInfo* senderInfo);

    // Get received RTCP report, report block
    virtual WebRtc_Word32 RemoteRTCPStat(
        std::vector<RTCPReportBlock>* receiveBlocks) const;

    // Set received RTCP report block
    virtual WebRtc_Word32 AddRTCPReportBlock(const WebRtc_UWord32 SSRC,
                                           const RTCPReportBlock* receiveBlock);

    virtual WebRtc_Word32 RemoveRTCPReportBlock(const WebRtc_UWord32 SSRC);

    /*
    *  (REMB) Receiver Estimated Max Bitrate
    */
    virtual bool REMB() const;

    virtual WebRtc_Word32 SetREMBStatus(const bool enable);

    virtual WebRtc_Word32 SetREMBData(const WebRtc_UWord32 bitrate,
                                      const WebRtc_UWord8 numberOfSSRC,
                                      const WebRtc_UWord32* SSRC);

    /*
    *   (IJ) Extended jitter report.
    */
    virtual bool IJ() const;

    virtual WebRtc_Word32 SetIJStatus(const bool enable);

    /*
    *   (TMMBR) Temporary Max Media Bit Rate
    */
    virtual bool TMMBR() const ;

    virtual WebRtc_Word32 SetTMMBRStatus(const bool enable);

    WebRtc_Word32 SetTMMBN(const TMMBRSet* boundingSet);

    virtual WebRtc_UWord16 MaxPayloadLength() const;

    virtual WebRtc_UWord16 MaxDataPayloadLength() const;

    virtual WebRtc_Word32 SetMaxTransferUnit(const WebRtc_UWord16 size);

    virtual WebRtc_Word32 SetTransportOverhead(const bool TCP,
                                             const bool IPV6,
                                             const WebRtc_UWord8 authenticationOverhead = 0);

    /*
    *   (NACK) Negative acknowledgement
    */

    // Is Negative acknowledgement requests on/off?
    virtual NACKMethod NACK() const ;

    // Turn negative acknowledgement requests on/off
    virtual WebRtc_Word32 SetNACKStatus(const NACKMethod method);

    virtual int SelectiveRetransmissions() const;

    virtual int SetSelectiveRetransmissions(uint8_t settings);

    // Send a Negative acknowledgement packet
    virtual WebRtc_Word32 SendNACK(const WebRtc_UWord16* nackList,
                                   const WebRtc_UWord16 size);

    // Store the sent packets, needed to answer to a Negative acknowledgement requests
    virtual WebRtc_Word32 SetStorePacketsStatus(const bool enable, const WebRtc_UWord16 numberToStore = 200);

    /*
    *   (APP) Application specific data
    */
    virtual WebRtc_Word32 SetRTCPApplicationSpecificData(const WebRtc_UWord8 subType,
                                                       const WebRtc_UWord32 name,
                                                       const WebRtc_UWord8* data,
                                                       const WebRtc_UWord16 length);
    /*
    *   (XR) VOIP metric
    */
    virtual WebRtc_Word32 SetRTCPVoIPMetrics(const RTCPVoIPMetric* VoIPMetric);

    /*
    *   Audio
    */

    // set audio packet size, used to determine when it's time to send a DTMF packet in silence (CNG)
    virtual WebRtc_Word32 SetAudioPacketSize(const WebRtc_UWord16 packetSizeSamples);

    // Outband DTMF detection
    virtual WebRtc_Word32 SetTelephoneEventStatus(const bool enable,
                                                const bool forwardToDecoder,
                                                const bool detectEndOfTone = false);

    // Is outband DTMF turned on/off?
    virtual bool TelephoneEvent() const;

    // Is forwarding of outband telephone events turned on/off?
    virtual bool TelephoneEventForwardToDecoder() const;

    virtual bool SendTelephoneEventActive(WebRtc_Word8& telephoneEvent) const;

    // Send a TelephoneEvent tone using RFC 2833 (4733)
    virtual WebRtc_Word32 SendTelephoneEventOutband(const WebRtc_UWord8 key,
                                                  const WebRtc_UWord16 time_ms,
                                                  const WebRtc_UWord8 level);

    // Set payload type for Redundant Audio Data RFC 2198
    virtual WebRtc_Word32 SetSendREDPayloadType(const WebRtc_Word8 payloadType);

    // Get payload type for Redundant Audio Data RFC 2198
    virtual WebRtc_Word32 SendREDPayloadType(WebRtc_Word8& payloadType) const;

    // Set status and ID for header-extension-for-audio-level-indication.
    virtual WebRtc_Word32 SetRTPAudioLevelIndicationStatus(const bool enable,
                                                         const WebRtc_UWord8 ID);

    // Get status and ID for header-extension-for-audio-level-indication.
    virtual WebRtc_Word32 GetRTPAudioLevelIndicationStatus(bool& enable,
                                                         WebRtc_UWord8& ID) const;

    // Store the audio level in dBov for header-extension-for-audio-level-indication.
    virtual WebRtc_Word32 SetAudioLevel(const WebRtc_UWord8 level_dBov);

    /*
    *   Video
    */
    virtual RtpVideoCodecTypes ReceivedVideoCodec() const;

    virtual RtpVideoCodecTypes SendVideoCodec() const;

    virtual WebRtc_Word32 SendRTCPSliceLossIndication(const WebRtc_UWord8 pictureID);

    // Set method for requestion a new key frame
    virtual WebRtc_Word32 SetKeyFrameRequestMethod(const KeyFrameRequestMethod method);

    // send a request for a keyframe
    virtual WebRtc_Word32 RequestKeyFrame();

    virtual WebRtc_Word32 SetCameraDelay(const WebRtc_Word32 delayMS);

    virtual void SetTargetSendBitrate(const WebRtc_UWord32 bitrate);

    virtual WebRtc_Word32 SetGenericFECStatus(const bool enable,
                                            const WebRtc_UWord8 payloadTypeRED,
                                            const WebRtc_UWord8 payloadTypeFEC);

    virtual WebRtc_Word32 GenericFECStatus(bool& enable,
                                         WebRtc_UWord8& payloadTypeRED,
                                         WebRtc_UWord8& payloadTypeFEC);

    virtual WebRtc_Word32 SetFecParameters(
        const FecProtectionParams* delta_params,
        const FecProtectionParams* key_params);

    virtual WebRtc_Word32 LastReceivedNTP(WebRtc_UWord32& NTPsecs,
                                          WebRtc_UWord32& NTPfrac,
                                          WebRtc_UWord32& remoteSR);

    virtual WebRtc_Word32 BoundingSet(bool &tmmbrOwner,
                                      TMMBRSet*& boundingSetRec);

    virtual void BitrateSent(WebRtc_UWord32* totalRate,
                             WebRtc_UWord32* videoRate,
                             WebRtc_UWord32* fecRate,
                             WebRtc_UWord32* nackRate) const;

    virtual int EstimatedReceiveBandwidth(
        WebRtc_UWord32* available_bandwidth) const;

    virtual void SetRemoteSSRC(const WebRtc_UWord32 SSRC);

    virtual WebRtc_UWord32 SendTimeOfSendReport(const WebRtc_UWord32 sendReport);

    // good state of RTP receiver inform sender
    virtual WebRtc_Word32 SendRTCPReferencePictureSelection(const WebRtc_UWord64 pictureID);

    void OnReceivedTMMBR();

    // bad state of RTP receiver request a keyframe
    void OnRequestIntraFrame();

    // received a request for a new SLI
    void OnReceivedSliceLossIndication(const WebRtc_UWord8 pictureID);

    // received a new refereence frame
    void OnReceivedReferencePictureSelectionIndication(
        const WebRtc_UWord64 pitureID);

    void OnReceivedNACK(const WebRtc_UWord16 nackSequenceNumbersLength,
                        const WebRtc_UWord16* nackSequenceNumbers);

    void OnRequestSendReport();

    // Following function is only called when constructing the object so no
    // need to worry about data race.
    void OwnsClock() { _owns_clock = true; }

protected:
    void RegisterChildModule(RtpRtcp* module);

    void DeRegisterChildModule(RtpRtcp* module);

    bool UpdateRTCPReceiveInformationTimers();

    void ProcessDeadOrAliveTimer();

    WebRtc_UWord32 BitrateReceivedNow() const;

    // Get remote SequenceNumber
    WebRtc_UWord16 RemoteSequenceNumber() const;

    // only for internal testing
    WebRtc_UWord32 LastSendReport(WebRtc_UWord32& lastRTCPTime);

    RTPSender                 _rtpSender;
    RTPReceiver               _rtpReceiver;

    RTCPSender                _rtcpSender;
    RTCPReceiver              _rtcpReceiver;

    bool                      _owns_clock;
    RtpRtcpClock&             _clock;
private:
    WebRtc_Word32             _id;
    const bool                _audio;
    bool                      _collisionDetected;
    WebRtc_Word64             _lastProcessTime;
    WebRtc_Word64             _lastBitrateProcessTime;
    WebRtc_Word64             _lastPacketTimeoutProcessTime;
    WebRtc_UWord16            _packetOverHead;

    scoped_ptr<CriticalSectionWrapper> _criticalSectionModulePtrs;
    scoped_ptr<CriticalSectionWrapper> _criticalSectionModulePtrsFeedback;
    ModuleRtpRtcpImpl*            _defaultModule;
    std::list<ModuleRtpRtcpImpl*> _childModules;

    // Dead or alive
    bool                  _deadOrAliveActive;
    WebRtc_UWord32        _deadOrAliveTimeoutMS;
    WebRtc_Word64        _deadOrAliveLastTimer;
    // send side
    NACKMethod            _nackMethod;
    WebRtc_UWord32        _nackLastTimeSent;
    WebRtc_UWord16        _nackLastSeqNumberSent;

    bool                  _simulcast;
    VideoCodec            _sendVideoCodec;
    KeyFrameRequestMethod _keyFrameReqMethod;

    RemoteBitrateEstimator* remote_bitrate_;

#ifdef MATLAB
    MatlabPlot*           _plot1;
#endif
};
} // namespace webrtc
#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RTCP_IMPL_H_
