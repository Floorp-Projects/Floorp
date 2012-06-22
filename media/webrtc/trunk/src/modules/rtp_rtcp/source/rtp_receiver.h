/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_H_

#include <map>

#include "typedefs.h"
#include "rtp_utility.h"

#include "rtp_header_extension.h"
#include "rtp_rtcp.h"
#include "rtp_rtcp_defines.h"
#include "rtp_receiver_audio.h"
#include "rtp_receiver_video.h"
#include "rtcp_receiver_help.h"
#include "Bitrate.h"

namespace webrtc {
class RtpRtcpFeedback;
class ModuleRtpRtcpImpl;
class Trace;

class RTPReceiver : public RTPReceiverAudio, public RTPReceiverVideo, public Bitrate
{
public:
    RTPReceiver(const WebRtc_Word32 id,
                const bool audio,
                RtpRtcpClock* clock,
                ModuleRtpRtcpImpl* owner);

    virtual ~RTPReceiver();

    virtual void ChangeUniqueId(const WebRtc_Word32 id);

    WebRtc_Word32 Init();

    RtpVideoCodecTypes VideoCodecType() const;
    WebRtc_UWord32 MaxConfiguredBitrate() const;

    WebRtc_Word32 SetPacketTimeout(const WebRtc_UWord32 timeoutMS);
    void PacketTimeout();

    void ProcessDeadOrAlive(const bool RTCPalive, const WebRtc_UWord32 now);

    void ProcessBitrate();

    WebRtc_Word32 RegisterIncomingDataCallback(RtpData* incomingDataCallback);
    WebRtc_Word32 RegisterIncomingRTPCallback(RtpFeedback* incomingMessagesCallback);

    WebRtc_Word32 RegisterReceivePayload(
        const char payloadName[RTP_PAYLOAD_NAME_SIZE],
        const WebRtc_Word8 payloadType,
        const WebRtc_UWord32 frequency,
        const WebRtc_UWord8 channels,
        const WebRtc_UWord32 rate);

    WebRtc_Word32 DeRegisterReceivePayload(const WebRtc_Word8 payloadType);

    WebRtc_Word32 ReceivePayloadType(
        const char payloadName[RTP_PAYLOAD_NAME_SIZE],
        const WebRtc_UWord32 frequency,
        const WebRtc_UWord8 channels,
        const WebRtc_UWord32 rate,
        WebRtc_Word8* payloadType) const;

    WebRtc_Word32 ReceivePayload(const WebRtc_Word8 payloadType,
                                 char payloadName[RTP_PAYLOAD_NAME_SIZE],
                                 WebRtc_UWord32* frequency,
                                 WebRtc_UWord8* channels,
                                 WebRtc_UWord32* rate) const;

    WebRtc_Word32 RemotePayload(char payloadName[RTP_PAYLOAD_NAME_SIZE],
                                WebRtc_Word8* payloadType,
                                WebRtc_UWord32* frequency,
                                WebRtc_UWord8* channels) const;

    WebRtc_Word32 IncomingRTPPacket(WebRtcRTPHeader* rtpheader,
                                    const WebRtc_UWord8* incomingRtpPacket,
                                    const WebRtc_UWord16 incomingRtpPacketLengt);

    NACKMethod NACK() const ;

    // Turn negative acknowledgement requests on/off
    WebRtc_Word32 SetNACKStatus(const NACKMethod method);


    // last received
    virtual WebRtc_UWord32 TimeStamp() const;
    virtual WebRtc_UWord16 SequenceNumber() const;

    WebRtc_Word32 EstimatedRemoteTimeStamp(WebRtc_UWord32& timestamp) const;

    WebRtc_UWord32 SSRC() const;

    WebRtc_Word32 CSRCs( WebRtc_UWord32 arrOfCSRC[kRtpCsrcSize]) const;

    WebRtc_Word32 Energy( WebRtc_UWord8 arrOfEnergy[kRtpCsrcSize]) const;

    // get the currently configured SSRC filter
    WebRtc_Word32 SSRCFilter(WebRtc_UWord32& allowedSSRC) const;

    // set a SSRC to be used as a filter for incoming RTP streams
    WebRtc_Word32 SetSSRCFilter(const bool enable, const WebRtc_UWord32 allowedSSRC);

    WebRtc_Word32 Statistics(WebRtc_UWord8  *fraction_lost,
                             WebRtc_UWord32 *cum_lost,
                             WebRtc_UWord32 *ext_max,
                             WebRtc_UWord32 *jitter,  // will be moved from JB
                             WebRtc_UWord32 *max_jitter,
                             WebRtc_UWord32 *jitter_transmission_time_offset,
                             bool reset) const;

    WebRtc_Word32 Statistics(WebRtc_UWord8  *fraction_lost,
                             WebRtc_UWord32 *cum_lost,
                             WebRtc_UWord32 *ext_max,
                             WebRtc_UWord32 *jitter,  // will be moved from JB
                             WebRtc_UWord32 *max_jitter,
                             WebRtc_UWord32 *jitter_transmission_time_offset,
                             WebRtc_Word32 *missing,
                             bool reset) const;

    WebRtc_Word32 DataCounters(WebRtc_UWord32 *bytesReceived,
                               WebRtc_UWord32 *packetsReceived) const;

    WebRtc_Word32 ResetStatistics();

    WebRtc_Word32 ResetDataCounters();

    WebRtc_UWord16 PacketOHReceived() const;

    WebRtc_UWord32 PacketCountReceived() const;

    WebRtc_UWord32 ByteCountReceived() const;

    WebRtc_Word32 RegisterRtpHeaderExtension(const RTPExtensionType type,
                                             const WebRtc_UWord8 id);

    WebRtc_Word32 DeregisterRtpHeaderExtension(const RTPExtensionType type);

    void GetHeaderExtensionMapCopy(RtpHeaderExtensionMap* map) const;

    virtual WebRtc_UWord32 PayloadTypeToPayload(const WebRtc_UWord8 payloadType,
                                                ModuleRTPUtility::Payload*& payload) const;
    /*
    *  RTX
    */
    void SetRTXStatus(const bool enable, const WebRtc_UWord32 SSRC);

    void RTXStatus(bool* enable, WebRtc_UWord32* SSRC) const;

protected:
    virtual WebRtc_Word32 CallbackOfReceivedPayloadData(const WebRtc_UWord8* payloadData,
                                                        const WebRtc_UWord16 payloadSize,
                                                        const WebRtcRTPHeader* rtpHeader);

    virtual bool RetransmitOfOldPacket(const WebRtc_UWord16 sequenceNumber,
                                       const WebRtc_UWord32 rtpTimeStamp) const;


    void UpdateStatistics(const WebRtcRTPHeader* rtpHeader,
                          const WebRtc_UWord16 bytes,
                          const bool oldPacket);

    virtual WebRtc_Word8 REDPayloadType() const;

private:
    // Is RED configured with payload type payloadType
    bool REDPayloadType(const WebRtc_Word8 payloadType) const;

    bool InOrderPacket(const WebRtc_UWord16 sequenceNumber) const;

    void CheckSSRCChanged(const WebRtcRTPHeader* rtpHeader);
    void CheckCSRC(const WebRtcRTPHeader* rtpHeader);
    WebRtc_Word32 CheckPayloadChanged(const WebRtcRTPHeader* rtpHeader,
                                      const WebRtc_Word8 firstPayloadByte,
                                      bool& isRED,
                                      ModuleRTPUtility::AudioPayload& audioSpecific,
                                      ModuleRTPUtility::VideoPayload& videoSpecific);

    void UpdateNACKBitRate(WebRtc_Word32 bytes, WebRtc_UWord32 now);
    bool ProcessNACKBitRate(WebRtc_UWord32 now);

private:
    WebRtc_Word32           _id;
    const bool              _audio;
    ModuleRtpRtcpImpl&      _rtpRtcp;

    CriticalSectionWrapper*    _criticalSectionCbs;
    RtpFeedback*        _cbRtpFeedback;
    RtpData*            _cbRtpData;

    CriticalSectionWrapper*    _criticalSectionRTPReceiver;
    mutable WebRtc_UWord32    _lastReceiveTime;
    WebRtc_UWord16            _lastReceivedPayloadLength;
    WebRtc_Word8              _lastReceivedPayloadType;
    WebRtc_Word8              _lastReceivedMediaPayloadType;

    ModuleRTPUtility::AudioPayload _lastReceivedAudioSpecific;
    ModuleRTPUtility::VideoPayload _lastReceivedVideoSpecific;

    WebRtc_UWord32            _packetTimeOutMS;

    WebRtc_Word8              _redPayloadType;

    std::map<WebRtc_Word8, ModuleRTPUtility::Payload*> _payloadTypeMap;
    RtpHeaderExtensionMap     _rtpHeaderExtensionMap;

    // SSRCs
    WebRtc_UWord32            _SSRC;
    WebRtc_UWord8             _numCSRCs;
    WebRtc_UWord32            _currentRemoteCSRC[kRtpCsrcSize];
    WebRtc_UWord8             _numEnergy;
    WebRtc_UWord8             _currentRemoteEnergy[kRtpCsrcSize];

    bool                      _useSSRCFilter;
    WebRtc_UWord32            _SSRCFilter;

    // stats on received RTP packets
    WebRtc_UWord32            _jitterQ4;
    mutable WebRtc_UWord32    _jitterMaxQ4;
    mutable WebRtc_UWord32    _cumulativeLoss;
    WebRtc_UWord32            _jitterQ4TransmissionTimeOffset;

    WebRtc_UWord32            _localTimeLastReceivedTimestamp;
    WebRtc_UWord32            _lastReceivedTimestamp;
    WebRtc_UWord16            _lastReceivedSequenceNumber;
    WebRtc_Word32             _lastReceivedTransmissionTimeOffset;
    WebRtc_UWord16            _receivedSeqFirst;
    WebRtc_UWord16            _receivedSeqMax;
    WebRtc_UWord16            _receivedSeqWraps;

    // current counter values
    WebRtc_UWord16            _receivedPacketOH;
    WebRtc_UWord32            _receivedByteCount;
    WebRtc_UWord32            _receivedOldPacketCount;
    WebRtc_UWord32            _receivedInorderPacketCount;

    // counter values when we sent the last report
    mutable WebRtc_UWord32    _lastReportInorderPackets;
    mutable WebRtc_UWord32    _lastReportOldPackets;
    mutable WebRtc_UWord16    _lastReportSeqMax;
    mutable WebRtc_UWord8     _lastReportFractionLost;
    mutable WebRtc_UWord32    _lastReportCumulativeLost;  // 24 bits valid
    mutable WebRtc_UWord32    _lastReportExtendedHighSeqNum;
    mutable WebRtc_UWord32    _lastReportJitter;
    mutable WebRtc_UWord32    _lastReportJitterTransmissionTimeOffset;

    NACKMethod _nackMethod;

    bool _RTX;
    WebRtc_UWord32 _ssrcRTX;
};
} // namespace webrtc

#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_H_
