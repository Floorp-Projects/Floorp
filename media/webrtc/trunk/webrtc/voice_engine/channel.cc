/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "channel.h"

#include "audio_device.h"
#include "audio_frame_operations.h"
#include "audio_processing.h"
#include "critical_section_wrapper.h"
#include "logging.h"
#include "output_mixer.h"
#include "process_thread.h"
#include "rtp_dump.h"
#include "statistics.h"
#include "trace.h"
#include "transmit_mixer.h"
#include "utility.h"
#include "voe_base.h"
#include "voe_codec_impl.h"
#include "voe_external_media.h"
#include "voe_rtp_rtcp.h"

#if defined(_WIN32)
#include <Qos.h>
#endif

namespace webrtc {
namespace voe {

WebRtc_Word32
Channel::SendData(FrameType frameType,
                  WebRtc_UWord8   payloadType,
                  WebRtc_UWord32  timeStamp,
                  const WebRtc_UWord8*  payloadData,
                  WebRtc_UWord16  payloadSize,
                  const RTPFragmentationHeader* fragmentation)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SendData(frameType=%u, payloadType=%u, timeStamp=%u,"
                 " payloadSize=%u, fragmentation=0x%x)",
                 frameType, payloadType, timeStamp, payloadSize, fragmentation);

    if (_includeAudioLevelIndication)
    {
        assert(_rtpAudioProc.get() != NULL);
        // Store current audio level in the RTP/RTCP module.
        // The level will be used in combination with voice-activity state
        // (frameType) to add an RTP header extension
        _rtpRtcpModule->SetAudioLevel(_rtpAudioProc->level_estimator()->RMS());
    }

    // Push data from ACM to RTP/RTCP-module to deliver audio frame for
    // packetization.
    // This call will trigger Transport::SendPacket() from the RTP/RTCP module.
    if (_rtpRtcpModule->SendOutgoingData((FrameType&)frameType,
                                        payloadType,
                                        timeStamp,
                                        // Leaving the time when this frame was
                                        // received from the capture device as
                                        // undefined for voice for now.
                                        -1,
                                        payloadData,
                                        payloadSize,
                                        fragmentation) == -1)
    {
        _engineStatisticsPtr->SetLastError(
            VE_RTP_RTCP_MODULE_ERROR, kTraceWarning,
            "Channel::SendData() failed to send data to RTP/RTCP module");
        return -1;
    }

    _lastLocalTimeStamp = timeStamp;
    _lastPayloadType = payloadType;

    return 0;
}

WebRtc_Word32
Channel::InFrameType(WebRtc_Word16 frameType)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::InFrameType(frameType=%d)", frameType);

    CriticalSectionScoped cs(&_callbackCritSect);
    // 1 indicates speech
    _sendFrameType = (frameType == 1) ? 1 : 0;
    return 0;
}

#ifdef WEBRTC_DTMF_DETECTION
int
Channel::IncomingDtmf(const WebRtc_UWord8 digitDtmf, const bool end)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
               "Channel::IncomingDtmf(digitDtmf=%u, end=%d)",
               digitDtmf, end);

    if (digitDtmf != 999)
    {
        CriticalSectionScoped cs(&_callbackCritSect);
        if (_telephoneEventDetectionPtr)
        {
            _telephoneEventDetectionPtr->OnReceivedTelephoneEventInband(
                _channelId, digitDtmf, end);
        }
    }

    return 0;
}
#endif

WebRtc_Word32
Channel::OnRxVadDetected(const int vadDecision)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
                 "Channel::OnRxVadDetected(vadDecision=%d)", vadDecision);

    CriticalSectionScoped cs(&_callbackCritSect);
    if (_rxVadObserverPtr)
    {
        _rxVadObserverPtr->OnRxVad(_channelId, vadDecision);
    }

    return 0;
}

int
Channel::SendPacket(int channel, const void *data, int len)
{
    channel = VoEChannelId(channel);
    assert(channel == _channelId);

    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SendPacket(channel=%d, len=%d)", channel, len);

    if (_transportPtr == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,_channelId),
                     "Channel::SendPacket() failed to send RTP packet due to"
                     " invalid transport object");
        return -1;
    }

    // Insert extra RTP packet using if user has called the InsertExtraRTPPacket
    // API
    if (_insertExtraRTPPacket)
    {
        WebRtc_UWord8* rtpHdr = (WebRtc_UWord8*)data;
        WebRtc_UWord8 M_PT(0);
        if (_extraMarkerBit)
        {
            M_PT = 0x80;            // set the M-bit
        }
        M_PT += _extraPayloadType;  // set the payload type
        *(++rtpHdr) = M_PT;     // modify the M|PT-byte within the RTP header
        _insertExtraRTPPacket = false;  // insert one packet only
    }

    WebRtc_UWord8* bufferToSendPtr = (WebRtc_UWord8*)data;
    WebRtc_Word32 bufferLength = len;

    // Dump the RTP packet to a file (if RTP dump is enabled).
    if (_rtpDumpOut.DumpPacket((const WebRtc_UWord8*)data, len) == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                     VoEId(_instanceId,_channelId),
                     "Channel::SendPacket() RTP dump to output file failed");
    }

    // SRTP or External encryption
    if (_encrypting)
    {
        CriticalSectionScoped cs(&_callbackCritSect);

        if (_encryptionPtr)
        {
            if (!_encryptionRTPBufferPtr)
            {
                // Allocate memory for encryption buffer one time only
                _encryptionRTPBufferPtr =
                    new WebRtc_UWord8[kVoiceEngineMaxIpPacketSizeBytes];
                memset(_encryptionRTPBufferPtr, 0,
                       kVoiceEngineMaxIpPacketSizeBytes);
            }

            // Perform encryption (SRTP or external)
            WebRtc_Word32 encryptedBufferLength = 0;
            _encryptionPtr->encrypt(_channelId,
                                    bufferToSendPtr,
                                    _encryptionRTPBufferPtr,
                                    bufferLength,
                                    (int*)&encryptedBufferLength);
            if (encryptedBufferLength <= 0)
            {
                _engineStatisticsPtr->SetLastError(
                    VE_ENCRYPTION_FAILED,
                    kTraceError, "Channel::SendPacket() encryption failed");
                return -1;
            }

            // Replace default data buffer with encrypted buffer
            bufferToSendPtr = _encryptionRTPBufferPtr;
            bufferLength = encryptedBufferLength;
        }
    }

    // Packet transmission using WebRtc socket transport
    if (!_externalTransport)
    {
        int n = _transportPtr->SendPacket(channel, bufferToSendPtr,
                                          bufferLength);
        if (n < 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice,
                         VoEId(_instanceId,_channelId),
                         "Channel::SendPacket() RTP transmission using WebRtc"
                         " sockets failed");
            return -1;
        }
        return n;
    }

    // Packet transmission using external transport transport
    {
        CriticalSectionScoped cs(&_callbackCritSect);

        int n = _transportPtr->SendPacket(channel,
                                          bufferToSendPtr,
                                          bufferLength);
        if (n < 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice,
                         VoEId(_instanceId,_channelId),
                         "Channel::SendPacket() RTP transmission using external"
                         " transport failed");
            return -1;
        }
        return n;
    }
}

int
Channel::SendRTCPPacket(int channel, const void *data, int len)
{
    channel = VoEChannelId(channel);
    assert(channel == _channelId);

    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SendRTCPPacket(channel=%d, len=%d)", channel, len);

    {
        CriticalSectionScoped cs(&_callbackCritSect);
        if (_transportPtr == NULL)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice,
                         VoEId(_instanceId,_channelId),
                         "Channel::SendRTCPPacket() failed to send RTCP packet"
                         " due to invalid transport object");
            return -1;
        }
    }

    WebRtc_UWord8* bufferToSendPtr = (WebRtc_UWord8*)data;
    WebRtc_Word32 bufferLength = len;

    // Dump the RTCP packet to a file (if RTP dump is enabled).
    if (_rtpDumpOut.DumpPacket((const WebRtc_UWord8*)data, len) == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                     VoEId(_instanceId,_channelId),
                     "Channel::SendPacket() RTCP dump to output file failed");
    }

    // SRTP or External encryption
    if (_encrypting)
    {
        CriticalSectionScoped cs(&_callbackCritSect);

        if (_encryptionPtr)
        {
            if (!_encryptionRTCPBufferPtr)
            {
                // Allocate memory for encryption buffer one time only
                _encryptionRTCPBufferPtr =
                    new WebRtc_UWord8[kVoiceEngineMaxIpPacketSizeBytes];
            }

            // Perform encryption (SRTP or external).
            WebRtc_Word32 encryptedBufferLength = 0;
            _encryptionPtr->encrypt_rtcp(_channelId,
                                         bufferToSendPtr,
                                         _encryptionRTCPBufferPtr,
                                         bufferLength,
                                         (int*)&encryptedBufferLength);
            if (encryptedBufferLength <= 0)
            {
                _engineStatisticsPtr->SetLastError(
                    VE_ENCRYPTION_FAILED, kTraceError,
                    "Channel::SendRTCPPacket() encryption failed");
                return -1;
            }

            // Replace default data buffer with encrypted buffer
            bufferToSendPtr = _encryptionRTCPBufferPtr;
            bufferLength = encryptedBufferLength;
        }
    }

    // Packet transmission using WebRtc socket transport
    if (!_externalTransport)
    {
        int n = _transportPtr->SendRTCPPacket(channel,
                                              bufferToSendPtr,
                                              bufferLength);
        if (n < 0)
        {
            WEBRTC_TRACE(kTraceInfo, kTraceVoice,
                         VoEId(_instanceId,_channelId),
                         "Channel::SendRTCPPacket() transmission using WebRtc"
                         " sockets failed");
            return -1;
        }
        return n;
    }

    // Packet transmission using external transport transport
    {
        CriticalSectionScoped cs(&_callbackCritSect);
        if (_transportPtr == NULL)
        {
            return -1;
        }
        int n = _transportPtr->SendRTCPPacket(channel,
                                              bufferToSendPtr,
                                              bufferLength);
        if (n < 0)
        {
            WEBRTC_TRACE(kTraceInfo, kTraceVoice,
                         VoEId(_instanceId,_channelId),
                         "Channel::SendRTCPPacket() transmission using external"
                         " transport failed");
            return -1;
        }
        return n;
    }

    return len;
}

void
Channel::IncomingRTPPacket(const WebRtc_Word8* incomingRtpPacket,
                           const WebRtc_Word32 rtpPacketLength,
                           const char* fromIP,
                           const WebRtc_UWord16 fromPort)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::IncomingRTPPacket(rtpPacketLength=%d,"
                 " fromIP=%s, fromPort=%u)",
                 rtpPacketLength, fromIP, fromPort);

    // Store playout timestamp for the received RTP packet
    // to be used for upcoming delay estimations
    WebRtc_UWord32 playoutTimestamp(0);
    if (GetPlayoutTimeStamp(playoutTimestamp) == 0)
    {
        _playoutTimeStampRTP = playoutTimestamp;
    }

    WebRtc_UWord8* rtpBufferPtr = (WebRtc_UWord8*)incomingRtpPacket;
    WebRtc_Word32 rtpBufferLength = rtpPacketLength;

    // SRTP or External decryption
    if (_decrypting)
    {
        CriticalSectionScoped cs(&_callbackCritSect);

        if (_encryptionPtr)
        {
            if (!_decryptionRTPBufferPtr)
            {
                // Allocate memory for decryption buffer one time only
                _decryptionRTPBufferPtr =
                    new WebRtc_UWord8[kVoiceEngineMaxIpPacketSizeBytes];
            }

            // Perform decryption (SRTP or external)
            WebRtc_Word32 decryptedBufferLength = 0;
            _encryptionPtr->decrypt(_channelId,
                                    rtpBufferPtr,
                                    _decryptionRTPBufferPtr,
                                    rtpBufferLength,
                                    (int*)&decryptedBufferLength);
            if (decryptedBufferLength <= 0)
            {
                _engineStatisticsPtr->SetLastError(
                    VE_DECRYPTION_FAILED, kTraceError,
                    "Channel::IncomingRTPPacket() decryption failed");
                return;
            }

            // Replace default data buffer with decrypted buffer
            rtpBufferPtr = _decryptionRTPBufferPtr;
            rtpBufferLength = decryptedBufferLength;
        }
    }

    // Dump the RTP packet to a file (if RTP dump is enabled).
    if (_rtpDumpIn.DumpPacket(rtpBufferPtr,
                              (WebRtc_UWord16)rtpBufferLength) == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                     VoEId(_instanceId,_channelId),
                     "Channel::SendPacket() RTP dump to input file failed");
    }

    // Deliver RTP packet to RTP/RTCP module for parsing
    // The packet will be pushed back to the channel thru the
    // OnReceivedPayloadData callback so we don't push it to the ACM here
    if (_rtpRtcpModule->IncomingPacket((const WebRtc_UWord8*)rtpBufferPtr,
                                      (WebRtc_UWord16)rtpBufferLength) == -1)
    {
        _engineStatisticsPtr->SetLastError(
            VE_SOCKET_TRANSPORT_MODULE_ERROR, kTraceWarning,
            "Channel::IncomingRTPPacket() RTP packet is invalid");
        return;
    }
}

void
Channel::IncomingRTCPPacket(const WebRtc_Word8* incomingRtcpPacket,
                            const WebRtc_Word32 rtcpPacketLength,
                            const char* fromIP,
                            const WebRtc_UWord16 fromPort)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::IncomingRTCPPacket(rtcpPacketLength=%d, fromIP=%s,"
                 " fromPort=%u)",
                 rtcpPacketLength, fromIP, fromPort);

    // Temporary buffer pointer and size for decryption
    WebRtc_UWord8* rtcpBufferPtr = (WebRtc_UWord8*)incomingRtcpPacket;
    WebRtc_Word32 rtcpBufferLength = rtcpPacketLength;

    // Store playout timestamp for the received RTCP packet
    // which will be read by the GetRemoteRTCPData API
    WebRtc_UWord32 playoutTimestamp(0);
    if (GetPlayoutTimeStamp(playoutTimestamp) == 0)
    {
        _playoutTimeStampRTCP = playoutTimestamp;
    }

    // SRTP or External decryption
    if (_decrypting)
    {
        CriticalSectionScoped cs(&_callbackCritSect);

        if (_encryptionPtr)
        {
            if (!_decryptionRTCPBufferPtr)
            {
                // Allocate memory for decryption buffer one time only
                _decryptionRTCPBufferPtr =
                    new WebRtc_UWord8[kVoiceEngineMaxIpPacketSizeBytes];
            }

            // Perform decryption (SRTP or external).
            WebRtc_Word32 decryptedBufferLength = 0;
            _encryptionPtr->decrypt_rtcp(_channelId,
                                         rtcpBufferPtr,
                                         _decryptionRTCPBufferPtr,
                                         rtcpBufferLength,
                                         (int*)&decryptedBufferLength);
            if (decryptedBufferLength <= 0)
            {
                _engineStatisticsPtr->SetLastError(
                    VE_DECRYPTION_FAILED, kTraceError,
                    "Channel::IncomingRTCPPacket() decryption failed");
                return;
            }

            // Replace default data buffer with decrypted buffer
            rtcpBufferPtr = _decryptionRTCPBufferPtr;
            rtcpBufferLength = decryptedBufferLength;
        }
    }

    // Dump the RTCP packet to a file (if RTP dump is enabled).
    if (_rtpDumpIn.DumpPacket(rtcpBufferPtr,
                              (WebRtc_UWord16)rtcpBufferLength) == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                     VoEId(_instanceId,_channelId),
                     "Channel::SendPacket() RTCP dump to input file failed");
    }

    // Deliver RTCP packet to RTP/RTCP module for parsing
    if (_rtpRtcpModule->IncomingPacket((const WebRtc_UWord8*)rtcpBufferPtr,
                                      (WebRtc_UWord16)rtcpBufferLength) == -1)
    {
        _engineStatisticsPtr->SetLastError(
            VE_SOCKET_TRANSPORT_MODULE_ERROR, kTraceWarning,
            "Channel::IncomingRTPPacket() RTCP packet is invalid");
        return;
    }
}

void
Channel::OnReceivedTelephoneEvent(const WebRtc_Word32 id,
                                  const WebRtc_UWord8 event,
                                  const bool endOfEvent)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::OnReceivedTelephoneEvent(id=%d, event=%u,"
                 " endOfEvent=%d)", id, event, endOfEvent);

#ifdef WEBRTC_DTMF_DETECTION
    if (_outOfBandTelephoneEventDetecion)
    {
        CriticalSectionScoped cs(&_callbackCritSect);

        if (_telephoneEventDetectionPtr)
        {
            _telephoneEventDetectionPtr->OnReceivedTelephoneEventOutOfBand(
                _channelId, event, endOfEvent);
        }
    }
#endif
}

void
Channel::OnPlayTelephoneEvent(const WebRtc_Word32 id,
                              const WebRtc_UWord8 event,
                              const WebRtc_UWord16 lengthMs,
                              const WebRtc_UWord8 volume)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::OnPlayTelephoneEvent(id=%d, event=%u, lengthMs=%u,"
                 " volume=%u)", id, event, lengthMs, volume);

    if (!_playOutbandDtmfEvent || (event > 15))
    {
        // Ignore callback since feedback is disabled or event is not a
        // Dtmf tone event.
        return;
    }

    assert(_outputMixerPtr != NULL);

    // Start playing out the Dtmf tone (if playout is enabled).
    // Reduce length of tone with 80ms to the reduce risk of echo.
    _outputMixerPtr->PlayDtmfTone(event, lengthMs - 80, volume);
}

void
Channel::OnIncomingSSRCChanged(const WebRtc_Word32 id,
                               const WebRtc_UWord32 SSRC)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::OnIncomingSSRCChanged(id=%d, SSRC=%d)",
                 id, SSRC);

    WebRtc_Word32 channel = VoEChannelId(id);
    assert(channel == _channelId);

    // Reset RTP-module counters since a new incoming RTP stream is detected
    _rtpRtcpModule->ResetReceiveDataCountersRTP();
    _rtpRtcpModule->ResetStatisticsRTP();

    if (_rtpObserver)
    {
        CriticalSectionScoped cs(&_callbackCritSect);

        if (_rtpObserverPtr)
        {
            // Send new SSRC to registered observer using callback
            _rtpObserverPtr->OnIncomingSSRCChanged(channel, SSRC);
        }
    }
}

void Channel::OnIncomingCSRCChanged(const WebRtc_Word32 id,
                                    const WebRtc_UWord32 CSRC,
                                    const bool added)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::OnIncomingCSRCChanged(id=%d, CSRC=%d, added=%d)",
                 id, CSRC, added);

    WebRtc_Word32 channel = VoEChannelId(id);
    assert(channel == _channelId);

    if (_rtpObserver)
    {
        CriticalSectionScoped cs(&_callbackCritSect);

        if (_rtpObserverPtr)
        {
            _rtpObserverPtr->OnIncomingCSRCChanged(channel, CSRC, added);
        }
    }
}

void
Channel::OnApplicationDataReceived(const WebRtc_Word32 id,
                                   const WebRtc_UWord8 subType,
                                   const WebRtc_UWord32 name,
                                   const WebRtc_UWord16 length,
                                   const WebRtc_UWord8* data)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::OnApplicationDataReceived(id=%d, subType=%u,"
                 " name=%u, length=%u)",
                 id, subType, name, length);

    WebRtc_Word32 channel = VoEChannelId(id);
    assert(channel == _channelId);

    if (_rtcpObserver)
    {
        CriticalSectionScoped cs(&_callbackCritSect);

        if (_rtcpObserverPtr)
        {
            _rtcpObserverPtr->OnApplicationDataReceived(channel,
                                                        subType,
                                                        name,
                                                        data,
                                                        length);
        }
    }
}

WebRtc_Word32
Channel::OnInitializeDecoder(
    const WebRtc_Word32 id,
    const WebRtc_Word8 payloadType,
    const char payloadName[RTP_PAYLOAD_NAME_SIZE],
    const int frequency,
    const WebRtc_UWord8 channels,
    const WebRtc_UWord32 rate)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::OnInitializeDecoder(id=%d, payloadType=%d, "
                 "payloadName=%s, frequency=%u, channels=%u, rate=%u)",
                 id, payloadType, payloadName, frequency, channels, rate);

    assert(VoEChannelId(id) == _channelId);

    CodecInst receiveCodec = {0};
    CodecInst dummyCodec = {0};

    receiveCodec.pltype = payloadType;
    receiveCodec.plfreq = frequency;
    receiveCodec.channels = channels;
    receiveCodec.rate = rate;
    strncpy(receiveCodec.plname, payloadName, RTP_PAYLOAD_NAME_SIZE - 1);
    
    _audioCodingModule.Codec(payloadName, dummyCodec, frequency, channels);
    receiveCodec.pacsize = dummyCodec.pacsize;

    // Register the new codec to the ACM
    if (_audioCodingModule.RegisterReceiveCodec(receiveCodec) == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                     VoEId(_instanceId, _channelId),
                     "Channel::OnInitializeDecoder() invalid codec ("
                     "pt=%d, name=%s) received - 1", payloadType, payloadName);
        _engineStatisticsPtr->SetLastError(VE_AUDIO_CODING_MODULE_ERROR);
        return -1;
    }

    return 0;
}

void
Channel::OnPacketTimeout(const WebRtc_Word32 id)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::OnPacketTimeout(id=%d)", id);

    CriticalSectionScoped cs(_callbackCritSectPtr);
    if (_voiceEngineObserverPtr)
    {
        if (_receiving || _externalTransport)
        {
            WebRtc_Word32 channel = VoEChannelId(id);
            assert(channel == _channelId);
            // Ensure that next OnReceivedPacket() callback will trigger
            // a VE_PACKET_RECEIPT_RESTARTED callback.
            _rtpPacketTimedOut = true;
            // Deliver callback to the observer
            WEBRTC_TRACE(kTraceInfo, kTraceVoice,
                         VoEId(_instanceId,_channelId),
                         "Channel::OnPacketTimeout() => "
                         "CallbackOnError(VE_RECEIVE_PACKET_TIMEOUT)");
            _voiceEngineObserverPtr->CallbackOnError(channel,
                                                     VE_RECEIVE_PACKET_TIMEOUT);
        }
    }
}

void
Channel::OnReceivedPacket(const WebRtc_Word32 id,
                          const RtpRtcpPacketType packetType)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::OnReceivedPacket(id=%d, packetType=%d)",
                 id, packetType);

    assert(VoEChannelId(id) == _channelId);

    // Notify only for the case when we have restarted an RTP session.
    if (_rtpPacketTimedOut && (kPacketRtp == packetType))
    {
        CriticalSectionScoped cs(_callbackCritSectPtr);
        if (_voiceEngineObserverPtr)
        {
            WebRtc_Word32 channel = VoEChannelId(id);
            assert(channel == _channelId);
            // Reset timeout mechanism
            _rtpPacketTimedOut = false;
            // Deliver callback to the observer
            WEBRTC_TRACE(kTraceInfo, kTraceVoice,
                         VoEId(_instanceId,_channelId),
                         "Channel::OnPacketTimeout() =>"
                         " CallbackOnError(VE_PACKET_RECEIPT_RESTARTED)");
            _voiceEngineObserverPtr->CallbackOnError(
                channel,
                VE_PACKET_RECEIPT_RESTARTED);
        }
    }
}

void
Channel::OnPeriodicDeadOrAlive(const WebRtc_Word32 id,
                               const RTPAliveType alive)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::OnPeriodicDeadOrAlive(id=%d, alive=%d)", id, alive);

    if (!_connectionObserver)
        return;

    WebRtc_Word32 channel = VoEChannelId(id);
    assert(channel == _channelId);

    // Use Alive as default to limit risk of false Dead detections
    bool isAlive(true);

    // Always mark the connection as Dead when the module reports kRtpDead
    if (kRtpDead == alive)
    {
        isAlive = false;
    }

    // It is possible that the connection is alive even if no RTP packet has
    // been received for a long time since the other side might use VAD/DTX
    // and a low SID-packet update rate.
    if ((kRtpNoRtp == alive) && _playing)
    {
        // Detect Alive for all NetEQ states except for the case when we are
        // in PLC_CNG state.
        // PLC_CNG <=> background noise only due to long expand or error.
        // Note that, the case where the other side stops sending during CNG
        // state will be detected as Alive. Dead is is not set until after
        // missing RTCP packets for at least twelve seconds (handled
        // internally by the RTP/RTCP module).
        isAlive = (_outputSpeechType != AudioFrame::kPLCCNG);
    }

    UpdateDeadOrAliveCounters(isAlive);

    // Send callback to the registered observer
    if (_connectionObserver)
    {
        CriticalSectionScoped cs(&_callbackCritSect);
        if (_connectionObserverPtr)
        {
            _connectionObserverPtr->OnPeriodicDeadOrAlive(channel, isAlive);
        }
    }
}

WebRtc_Word32
Channel::OnReceivedPayloadData(const WebRtc_UWord8* payloadData,
                               const WebRtc_UWord16 payloadSize,
                               const WebRtcRTPHeader* rtpHeader)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::OnReceivedPayloadData(payloadSize=%d,"
                 " payloadType=%u, audioChannel=%u)",
                 payloadSize,
                 rtpHeader->header.payloadType,
                 rtpHeader->type.Audio.channel);

    _lastRemoteTimeStamp = rtpHeader->header.timestamp;

    if (!_playing)
    {
        // Avoid inserting into NetEQ when we are not playing. Count the
        // packet as discarded.
        WEBRTC_TRACE(kTraceStream, kTraceVoice,
                     VoEId(_instanceId, _channelId),
                     "received packet is discarded since playing is not"
                     " activated");
        _numberOfDiscardedPackets++;
        return 0;
    }

    // Push the incoming payload (parsed and ready for decoding) into the ACM
    if (_audioCodingModule.IncomingPacket(payloadData,
                                          payloadSize,
                                          *rtpHeader) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_AUDIO_CODING_MODULE_ERROR, kTraceWarning,
            "Channel::OnReceivedPayloadData() unable to push data to the ACM");
        return -1;
    }

    // Update the packet delay
    UpdatePacketDelay(rtpHeader->header.timestamp,
                      rtpHeader->header.sequenceNumber);

    return 0;
}

WebRtc_Word32 Channel::GetAudioFrame(const WebRtc_Word32 id,
                                     AudioFrame& audioFrame)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::GetAudioFrame(id=%d)", id);

    // Get 10ms raw PCM data from the ACM (mixer limits output frequency)
    if (_audioCodingModule.PlayoutData10Ms(audioFrame.sample_rate_hz_,
                                           audioFrame) == -1)
    {
        WEBRTC_TRACE(kTraceError, kTraceVoice,
                     VoEId(_instanceId,_channelId),
                     "Channel::GetAudioFrame() PlayoutData10Ms() failed!");
        // In all likelihood, the audio in this frame is garbage. We return an
        // error so that the audio mixer module doesn't add it to the mix. As
        // a result, it won't be played out and the actions skipped here are
        // irrelevant.
        return -1;
    }

    if (_RxVadDetection)
    {
        UpdateRxVadDetection(audioFrame);
    }

    // Convert module ID to internal VoE channel ID
    audioFrame.id_ = VoEChannelId(audioFrame.id_);
    // Store speech type for dead-or-alive detection
    _outputSpeechType = audioFrame.speech_type_;

    // Perform far-end AudioProcessing module processing on the received signal
    if (_rxApmIsEnabled)
    {
        ApmProcessRx(audioFrame);
    }

    // Output volume scaling
    if (_outputGain < 0.99f || _outputGain > 1.01f)
    {
        AudioFrameOperations::ScaleWithSat(_outputGain, audioFrame);
    }

    // Scale left and/or right channel(s) if stereo and master balance is
    // active

    if (_panLeft != 1.0f || _panRight != 1.0f)
    {
        if (audioFrame.num_channels_ == 1)
        {
            // Emulate stereo mode since panning is active.
            // The mono signal is copied to both left and right channels here.
            AudioFrameOperations::MonoToStereo(&audioFrame);
        }
        // For true stereo mode (when we are receiving a stereo signal), no
        // action is needed.

        // Do the panning operation (the audio frame contains stereo at this
        // stage)
        AudioFrameOperations::Scale(_panLeft, _panRight, audioFrame);
    }

    // Mix decoded PCM output with file if file mixing is enabled
    if (_outputFilePlaying)
    {
        MixAudioWithFile(audioFrame, audioFrame.sample_rate_hz_);
    }

    // Place channel in on-hold state (~muted) if on-hold is activated
    if (_outputIsOnHold)
    {
        AudioFrameOperations::Mute(audioFrame);
    }

    // External media
    if (_outputExternalMedia)
    {
        CriticalSectionScoped cs(&_callbackCritSect);
        const bool isStereo = (audioFrame.num_channels_ == 2);
        if (_outputExternalMediaCallbackPtr)
        {
            _outputExternalMediaCallbackPtr->Process(
                _channelId,
                kPlaybackPerChannel,
                (WebRtc_Word16*)audioFrame.data_,
                audioFrame.samples_per_channel_,
                audioFrame.sample_rate_hz_,
                isStereo);
        }
    }

    // Record playout if enabled
    {
        CriticalSectionScoped cs(&_fileCritSect);

        if (_outputFileRecording && _outputFileRecorderPtr)
        {
            _outputFileRecorderPtr->RecordAudioToFile(audioFrame);
        }
    }

    // Measure audio level (0-9)
    _outputAudioLevel.ComputeLevel(audioFrame);

    return 0;
}

WebRtc_Word32
Channel::NeededFrequency(const WebRtc_Word32 id)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::NeededFrequency(id=%d)", id);

    int highestNeeded = 0;

    // Determine highest needed receive frequency
    WebRtc_Word32 receiveFrequency = _audioCodingModule.ReceiveFrequency();

    // Return the bigger of playout and receive frequency in the ACM.
    if (_audioCodingModule.PlayoutFrequency() > receiveFrequency)
    {
        highestNeeded = _audioCodingModule.PlayoutFrequency();
    }
    else
    {
        highestNeeded = receiveFrequency;
    }

    // Special case, if we're playing a file on the playout side
    // we take that frequency into consideration as well
    // This is not needed on sending side, since the codec will
    // limit the spectrum anyway.
    if (_outputFilePlaying)
    {
        CriticalSectionScoped cs(&_fileCritSect);
        if (_outputFilePlayerPtr && _outputFilePlaying)
        {
            if(_outputFilePlayerPtr->Frequency()>highestNeeded)
            {
                highestNeeded=_outputFilePlayerPtr->Frequency();
            }
        }
    }

    return(highestNeeded);
}

WebRtc_Word32
Channel::CreateChannel(Channel*& channel,
                       const WebRtc_Word32 channelId,
                       const WebRtc_UWord32 instanceId)
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(instanceId,channelId),
                 "Channel::CreateChannel(channelId=%d, instanceId=%d)",
        channelId, instanceId);

    channel = new Channel(channelId, instanceId);
    if (channel == NULL)
    {
        WEBRTC_TRACE(kTraceMemory, kTraceVoice,
                     VoEId(instanceId,channelId),
                     "Channel::CreateChannel() unable to allocate memory for"
                     " channel");
        return -1;
    }
    return 0;
}

void
Channel::PlayNotification(const WebRtc_Word32 id,
                          const WebRtc_UWord32 durationMs)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::PlayNotification(id=%d, durationMs=%d)",
                 id, durationMs);

    // Not implement yet
}

void
Channel::RecordNotification(const WebRtc_Word32 id,
                            const WebRtc_UWord32 durationMs)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::RecordNotification(id=%d, durationMs=%d)",
                 id, durationMs);

    // Not implement yet
}

void
Channel::PlayFileEnded(const WebRtc_Word32 id)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::PlayFileEnded(id=%d)", id);

    if (id == _inputFilePlayerId)
    {
        CriticalSectionScoped cs(&_fileCritSect);

        _inputFilePlaying = false;
        WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
                     VoEId(_instanceId,_channelId),
                     "Channel::PlayFileEnded() => input file player module is"
                     " shutdown");
    }
    else if (id == _outputFilePlayerId)
    {
        CriticalSectionScoped cs(&_fileCritSect);

        _outputFilePlaying = false;
        WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
                     VoEId(_instanceId,_channelId),
                     "Channel::PlayFileEnded() => output file player module is"
                     " shutdown");
    }
}

void
Channel::RecordFileEnded(const WebRtc_Word32 id)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::RecordFileEnded(id=%d)", id);

    assert(id == _outputFileRecorderId);

    CriticalSectionScoped cs(&_fileCritSect);

    _outputFileRecording = false;
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
                 VoEId(_instanceId,_channelId),
                 "Channel::RecordFileEnded() => output file recorder module is"
                 " shutdown");
}

Channel::Channel(const WebRtc_Word32 channelId,
                 const WebRtc_UWord32 instanceId) :
    _fileCritSect(*CriticalSectionWrapper::CreateCriticalSection()),
    _callbackCritSect(*CriticalSectionWrapper::CreateCriticalSection()),
    _instanceId(instanceId),
    _channelId(channelId),
    _audioCodingModule(*AudioCodingModule::Create(
        VoEModuleId(instanceId, channelId))),
#ifndef WEBRTC_EXTERNAL_TRANSPORT
     _numSocketThreads(KNumSocketThreads),
    _socketTransportModule(*UdpTransport::Create(
        VoEModuleId(instanceId, channelId), _numSocketThreads)),
#endif
#ifdef WEBRTC_SRTP
    _srtpModule(*SrtpModule::CreateSrtpModule(VoEModuleId(instanceId,
                                                          channelId))),
#endif
    _rtpDumpIn(*RtpDump::CreateRtpDump()),
    _rtpDumpOut(*RtpDump::CreateRtpDump()),
    _outputAudioLevel(),
    _externalTransport(false),
    _inputFilePlayerPtr(NULL),
    _outputFilePlayerPtr(NULL),
    _outputFileRecorderPtr(NULL),
    // Avoid conflict with other channels by adding 1024 - 1026,
    // won't use as much as 1024 channels.
    _inputFilePlayerId(VoEModuleId(instanceId, channelId) + 1024),
    _outputFilePlayerId(VoEModuleId(instanceId, channelId) + 1025),
    _outputFileRecorderId(VoEModuleId(instanceId, channelId) + 1026),
    _inputFilePlaying(false),
    _outputFilePlaying(false),
    _outputFileRecording(false),
    _inbandDtmfQueue(VoEModuleId(instanceId, channelId)),
    _inbandDtmfGenerator(VoEModuleId(instanceId, channelId)),
    _inputExternalMedia(false),
    _outputExternalMedia(false),
    _inputExternalMediaCallbackPtr(NULL),
    _outputExternalMediaCallbackPtr(NULL),
    _encryptionRTPBufferPtr(NULL),
    _decryptionRTPBufferPtr(NULL),
    _encryptionRTCPBufferPtr(NULL),
    _decryptionRTCPBufferPtr(NULL),
    _timeStamp(0), // This is just an offset, RTP module will add it's own random offset
    _sendTelephoneEventPayloadType(106),
    _playoutTimeStampRTP(0),
    _playoutTimeStampRTCP(0),
    _numberOfDiscardedPackets(0),
    _engineStatisticsPtr(NULL),
    _outputMixerPtr(NULL),
    _transmitMixerPtr(NULL),
    _moduleProcessThreadPtr(NULL),
    _audioDeviceModulePtr(NULL),
    _voiceEngineObserverPtr(NULL),
    _callbackCritSectPtr(NULL),
    _transportPtr(NULL),
    _encryptionPtr(NULL),
    _rtpAudioProc(NULL),
    _rxAudioProcessingModulePtr(NULL),
#ifdef WEBRTC_DTMF_DETECTION
    _telephoneEventDetectionPtr(NULL),
#endif
    _rxVadObserverPtr(NULL),
    _oldVadDecision(-1),
    _sendFrameType(0),
    _rtpObserverPtr(NULL),
    _rtcpObserverPtr(NULL),
    _outputIsOnHold(false),
    _externalPlayout(false),
    _externalMixing(false),
    _inputIsOnHold(false),
    _playing(false),
    _sending(false),
    _receiving(false),
    _mixFileWithMicrophone(false),
    _rtpObserver(false),
    _rtcpObserver(false),
    _mute(false),
    _panLeft(1.0f),
    _panRight(1.0f),
    _outputGain(1.0f),
    _encrypting(false),
    _decrypting(false),
    _playOutbandDtmfEvent(false),
    _playInbandDtmfEvent(false),
    _inbandTelephoneEventDetection(false),
    _outOfBandTelephoneEventDetecion(false),
    _extraPayloadType(0),
    _insertExtraRTPPacket(false),
    _extraMarkerBit(false),
    _lastLocalTimeStamp(0),
    _lastRemoteTimeStamp(0),
    _lastPayloadType(0),
    _includeAudioLevelIndication(false),
    _rtpPacketTimedOut(false),
    _rtpPacketTimeOutIsEnabled(false),
    _rtpTimeOutSeconds(0),
    _connectionObserver(false),
    _connectionObserverPtr(NULL),
    _countAliveDetections(0),
    _countDeadDetections(0),
    _outputSpeechType(AudioFrame::kNormalSpeech),
    _averageDelayMs(0),
    _previousSequenceNumber(0),
    _previousTimestamp(0),
    _recPacketDelayMs(20),
    _RxVadDetection(false),
    _rxApmIsEnabled(false),
    _rxAgcIsEnabled(false),
    _rxNsIsEnabled(false)
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::Channel() - ctor");
    _inbandDtmfQueue.ResetDtmf();
    _inbandDtmfGenerator.Init();
    _outputAudioLevel.Clear();

    RtpRtcp::Configuration configuration;
    configuration.id = VoEModuleId(instanceId, channelId);
    configuration.audio = true;
    configuration.incoming_data = this;
    configuration.incoming_messages = this;
    configuration.outgoing_transport = this;
    configuration.rtcp_feedback = this;
    configuration.audio_messages = this;

    _rtpRtcpModule.reset(RtpRtcp::CreateRtpRtcp(configuration));

    // Create far end AudioProcessing Module
    _rxAudioProcessingModulePtr = AudioProcessing::Create(
        VoEModuleId(instanceId, channelId));
}

Channel::~Channel()
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::~Channel() - dtor");

    if (_outputExternalMedia)
    {
        DeRegisterExternalMediaProcessing(kPlaybackPerChannel);
    }
    if (_inputExternalMedia)
    {
        DeRegisterExternalMediaProcessing(kRecordingPerChannel);
    }
    StopSend();
#ifndef WEBRTC_EXTERNAL_TRANSPORT
    StopReceiving();
    // De-register packet callback to ensure we're not in a callback when
    // deleting channel state, avoids race condition and deadlock.
    if (_socketTransportModule.InitializeReceiveSockets(NULL, 0, NULL, NULL, 0)
            != 0)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                     VoEId(_instanceId, _channelId),
                     "~Channel() failed to de-register receive callback");
    }
#endif
    StopPlayout();

    {
        CriticalSectionScoped cs(&_fileCritSect);
        if (_inputFilePlayerPtr)
        {
            _inputFilePlayerPtr->RegisterModuleFileCallback(NULL);
            _inputFilePlayerPtr->StopPlayingFile();
            FilePlayer::DestroyFilePlayer(_inputFilePlayerPtr);
            _inputFilePlayerPtr = NULL;
        }
        if (_outputFilePlayerPtr)
        {
            _outputFilePlayerPtr->RegisterModuleFileCallback(NULL);
            _outputFilePlayerPtr->StopPlayingFile();
            FilePlayer::DestroyFilePlayer(_outputFilePlayerPtr);
            _outputFilePlayerPtr = NULL;
        }
        if (_outputFileRecorderPtr)
        {
            _outputFileRecorderPtr->RegisterModuleFileCallback(NULL);
            _outputFileRecorderPtr->StopRecording();
            FileRecorder::DestroyFileRecorder(_outputFileRecorderPtr);
            _outputFileRecorderPtr = NULL;
        }
    }

    // The order to safely shutdown modules in a channel is:
    // 1. De-register callbacks in modules
    // 2. De-register modules in process thread
    // 3. Destroy modules
    if (_audioCodingModule.RegisterTransportCallback(NULL) == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                     VoEId(_instanceId,_channelId),
                     "~Channel() failed to de-register transport callback"
                     " (Audio coding module)");
    }
    if (_audioCodingModule.RegisterVADCallback(NULL) == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                     VoEId(_instanceId,_channelId),
                     "~Channel() failed to de-register VAD callback"
                     " (Audio coding module)");
    }
#ifdef WEBRTC_DTMF_DETECTION
    if (_audioCodingModule.RegisterIncomingMessagesCallback(NULL) == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                     VoEId(_instanceId,_channelId),
                     "~Channel() failed to de-register incoming messages "
                     "callback (Audio coding module)");
    }
#endif
    // De-register modules in process thread
#ifndef WEBRTC_EXTERNAL_TRANSPORT
    if (_moduleProcessThreadPtr->DeRegisterModule(&_socketTransportModule)
            == -1)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceVoice,
                     VoEId(_instanceId,_channelId),
                     "~Channel() failed to deregister socket module");
    }
#endif
    if (_moduleProcessThreadPtr->DeRegisterModule(_rtpRtcpModule.get()) == -1)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceVoice,
                     VoEId(_instanceId,_channelId),
                     "~Channel() failed to deregister RTP/RTCP module");
    }

    // Destroy modules
#ifndef WEBRTC_EXTERNAL_TRANSPORT
    UdpTransport::Destroy(
        &_socketTransportModule);
#endif
    AudioCodingModule::Destroy(&_audioCodingModule);
#ifdef WEBRTC_SRTP
    SrtpModule::DestroySrtpModule(&_srtpModule);
#endif
    if (_rxAudioProcessingModulePtr != NULL)
    {
        AudioProcessing::Destroy(_rxAudioProcessingModulePtr); // far end APM
        _rxAudioProcessingModulePtr = NULL;
    }

    // End of modules shutdown

    // Delete other objects
    RtpDump::DestroyRtpDump(&_rtpDumpIn);
    RtpDump::DestroyRtpDump(&_rtpDumpOut);
    delete [] _encryptionRTPBufferPtr;
    delete [] _decryptionRTPBufferPtr;
    delete [] _encryptionRTCPBufferPtr;
    delete [] _decryptionRTCPBufferPtr;
    delete &_callbackCritSect;
    delete &_fileCritSect;
}

int Channel::GetCodec(int idx, CodecInst &codec)
{
    CodecInst codecCopy;
    int ret;
    ret = _audioCodingModule.Codec(idx, codecCopy);
    if (ret != -1)
    {
        VoECodecImpl::ACMToExternalCodecRepresentation(codec, codecCopy);
    }
    return ret;
}

WebRtc_Word32
Channel::Init()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::Init()");

    // --- Initial sanity

    if ((_engineStatisticsPtr == NULL) ||
        (_moduleProcessThreadPtr == NULL))
    {
        WEBRTC_TRACE(kTraceError, kTraceVoice,
                     VoEId(_instanceId,_channelId),
                     "Channel::Init() must call SetEngineInformation() first");
        return -1;
    }

    // --- Add modules to process thread (for periodic schedulation)

    const bool processThreadFail =
        ((_moduleProcessThreadPtr->RegisterModule(_rtpRtcpModule.get()) != 0) ||
#ifndef WEBRTC_EXTERNAL_TRANSPORT
        (_moduleProcessThreadPtr->RegisterModule(
                &_socketTransportModule) != 0));
#else
        false);
#endif
    if (processThreadFail)
    {
        _engineStatisticsPtr->SetLastError(
            VE_CANNOT_INIT_CHANNEL, kTraceError,
            "Channel::Init() modules not registered");
        return -1;
    }
    // --- ACM initialization

    if ((_audioCodingModule.InitializeReceiver() == -1) ||
#ifdef WEBRTC_CODEC_AVT
        // out-of-band Dtmf tones are played out by default
        (_audioCodingModule.SetDtmfPlayoutStatus(true) == -1) ||
#endif
        (_audioCodingModule.InitializeSender() == -1))
    {
        _engineStatisticsPtr->SetLastError(
            VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
            "Channel::Init() unable to initialize the ACM - 1");
        return -1;
    }

    // --- RTP/RTCP module initialization

    // Ensure that RTCP is enabled by default for the created channel.
    // Note that, the module will keep generating RTCP until it is explicitly
    // disabled by the user.
    // After StopListen (when no sockets exists), RTCP packets will no longer
    // be transmitted since the Transport object will then be invalid.

    const bool rtpRtcpFail =
        ((_rtpRtcpModule->SetTelephoneEventStatus(false, true, true) == -1) ||
        // RTCP is enabled by default
        (_rtpRtcpModule->SetRTCPStatus(kRtcpCompound) == -1));
    if (rtpRtcpFail)
    {
        _engineStatisticsPtr->SetLastError(
            VE_RTP_RTCP_MODULE_ERROR, kTraceError,
            "Channel::Init() RTP/RTCP module not initialized");
        return -1;
    }

     // --- Register all permanent callbacks
    const bool fail =
        (_audioCodingModule.RegisterTransportCallback(this) == -1) ||
        (_audioCodingModule.RegisterVADCallback(this) == -1);

    if (fail)
    {
        _engineStatisticsPtr->SetLastError(
            VE_CANNOT_INIT_CHANNEL, kTraceError,
            "Channel::Init() callbacks not registered");
        return -1;
    }

    // --- Register all supported codecs to the receiving side of the
    // RTP/RTCP module

    CodecInst codec;
    const WebRtc_UWord8 nSupportedCodecs = AudioCodingModule::NumberOfCodecs();

    for (int idx = 0; idx < nSupportedCodecs; idx++)
    {
        // Open up the RTP/RTCP receiver for all supported codecs
        if ((_audioCodingModule.Codec(idx, codec) == -1) ||
            (_rtpRtcpModule->RegisterReceivePayload(codec) == -1))
        {
            WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                         VoEId(_instanceId,_channelId),
                         "Channel::Init() unable to register %s (%d/%d/%d/%d) "
                         "to RTP/RTCP receiver",
                         codec.plname, codec.pltype, codec.plfreq,
                         codec.channels, codec.rate);
        }
        else
        {
            WEBRTC_TRACE(kTraceInfo, kTraceVoice,
                         VoEId(_instanceId,_channelId),
                         "Channel::Init() %s (%d/%d/%d/%d) has been added to "
                         "the RTP/RTCP receiver",
                         codec.plname, codec.pltype, codec.plfreq,
                         codec.channels, codec.rate);
        }

        // Ensure that PCMU is used as default codec on the sending side
        if (!STR_CASE_CMP(codec.plname, "PCMU") && (codec.channels == 1))
        {
            SetSendCodec(codec);
        }

        // Register default PT for outband 'telephone-event'
        if (!STR_CASE_CMP(codec.plname, "telephone-event"))
        {
            if ((_rtpRtcpModule->RegisterSendPayload(codec) == -1) ||
                (_audioCodingModule.RegisterReceiveCodec(codec) == -1))
            {
                WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                             VoEId(_instanceId,_channelId),
                             "Channel::Init() failed to register outband "
                             "'telephone-event' (%d/%d) correctly",
                             codec.pltype, codec.plfreq);
            }
        }

        if (!STR_CASE_CMP(codec.plname, "CN"))
        {
            if ((_audioCodingModule.RegisterSendCodec(codec) == -1) ||
                (_audioCodingModule.RegisterReceiveCodec(codec) == -1) ||
                (_rtpRtcpModule->RegisterSendPayload(codec) == -1))
            {
                WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                             VoEId(_instanceId,_channelId),
                             "Channel::Init() failed to register CN (%d/%d) "
                             "correctly - 1",
                             codec.pltype, codec.plfreq);
            }
        }
#ifdef WEBRTC_CODEC_RED
        // Register RED to the receiving side of the ACM.
        // We will not receive an OnInitializeDecoder() callback for RED.
        if (!STR_CASE_CMP(codec.plname, "RED"))
        {
            if (_audioCodingModule.RegisterReceiveCodec(codec) == -1)
            {
                WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                             VoEId(_instanceId,_channelId),
                             "Channel::Init() failed to register RED (%d/%d) "
                             "correctly",
                             codec.pltype, codec.plfreq);
            }
        }
#endif
    }
#ifndef WEBRTC_EXTERNAL_TRANSPORT
    // Ensure that the WebRtcSocketTransport implementation is used as
    // Transport on the sending side
    {
        // A lock is needed here since users can call
        // RegisterExternalTransport() at the same time.
        CriticalSectionScoped cs(&_callbackCritSect);
        _transportPtr = &_socketTransportModule;
    }
#endif

    // Initialize the far end AP module
    // Using 8 kHz as initial Fs, the same as in transmission. Might be
    // changed at the first receiving audio.
    if (_rxAudioProcessingModulePtr == NULL)
    {
        _engineStatisticsPtr->SetLastError(
            VE_NO_MEMORY, kTraceCritical,
            "Channel::Init() failed to create the far-end AudioProcessing"
            " module");
        return -1;
    }

    if (_rxAudioProcessingModulePtr->set_sample_rate_hz(8000))
    {
        _engineStatisticsPtr->SetLastError(
            VE_APM_ERROR, kTraceWarning,
            "Channel::Init() failed to set the sample rate to 8K for"
            " far-end AP module");
    }

    if (_rxAudioProcessingModulePtr->set_num_channels(1, 1) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_SOUNDCARD_ERROR, kTraceWarning,
            "Init() failed to set channels for the primary audio stream");
    }

    if (_rxAudioProcessingModulePtr->high_pass_filter()->Enable(
        WEBRTC_VOICE_ENGINE_RX_HP_DEFAULT_STATE) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_APM_ERROR, kTraceWarning,
            "Channel::Init() failed to set the high-pass filter for"
            " far-end AP module");
    }

    if (_rxAudioProcessingModulePtr->noise_suppression()->set_level(
        (NoiseSuppression::Level)WEBRTC_VOICE_ENGINE_RX_NS_DEFAULT_MODE) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_APM_ERROR, kTraceWarning,
            "Init() failed to set noise reduction level for far-end"
            " AP module");
    }
    if (_rxAudioProcessingModulePtr->noise_suppression()->Enable(
        WEBRTC_VOICE_ENGINE_RX_NS_DEFAULT_STATE) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_APM_ERROR, kTraceWarning,
            "Init() failed to set noise reduction state for far-end"
            " AP module");
    }

    if (_rxAudioProcessingModulePtr->gain_control()->set_mode(
        (GainControl::Mode)WEBRTC_VOICE_ENGINE_RX_AGC_DEFAULT_MODE) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_APM_ERROR, kTraceWarning,
            "Init() failed to set AGC mode for far-end AP module");
    }
    if (_rxAudioProcessingModulePtr->gain_control()->Enable(
        WEBRTC_VOICE_ENGINE_RX_AGC_DEFAULT_STATE) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_APM_ERROR, kTraceWarning,
            "Init() failed to set AGC state for far-end AP module");
    }

    return 0;
}

WebRtc_Word32
Channel::SetEngineInformation(Statistics& engineStatistics,
                              OutputMixer& outputMixer,
                              voe::TransmitMixer& transmitMixer,
                              ProcessThread& moduleProcessThread,
                              AudioDeviceModule& audioDeviceModule,
                              VoiceEngineObserver* voiceEngineObserver,
                              CriticalSectionWrapper* callbackCritSect)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetEngineInformation()");
    _engineStatisticsPtr = &engineStatistics;
    _outputMixerPtr = &outputMixer;
    _transmitMixerPtr = &transmitMixer,
    _moduleProcessThreadPtr = &moduleProcessThread;
    _audioDeviceModulePtr = &audioDeviceModule;
    _voiceEngineObserverPtr = voiceEngineObserver;
    _callbackCritSectPtr = callbackCritSect;
    return 0;
}

WebRtc_Word32
Channel::UpdateLocalTimeStamp()
{

    _timeStamp += _audioFrame.samples_per_channel_;
    return 0;
}

WebRtc_Word32
Channel::StartPlayout()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::StartPlayout()");
    if (_playing)
    {
        return 0;
    }

    if (!_externalMixing) {
        // Add participant as candidates for mixing.
        if (_outputMixerPtr->SetMixabilityStatus(*this, true) != 0)
        {
            _engineStatisticsPtr->SetLastError(
                VE_AUDIO_CONF_MIX_MODULE_ERROR, kTraceError,
                "StartPlayout() failed to add participant to mixer");
            return -1;
        }
    }

    _playing = true;

    if (RegisterFilePlayingToMixer() != 0)
        return -1;

    return 0;
}

WebRtc_Word32
Channel::StopPlayout()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::StopPlayout()");
    if (!_playing)
    {
        return 0;
    }

    if (!_externalMixing) {
        // Remove participant as candidates for mixing
        if (_outputMixerPtr->SetMixabilityStatus(*this, false) != 0)
        {
            _engineStatisticsPtr->SetLastError(
                VE_AUDIO_CONF_MIX_MODULE_ERROR, kTraceError,
                "StopPlayout() failed to remove participant from mixer");
            return -1;
        }
    }

    _playing = false;
    _outputAudioLevel.Clear();

    return 0;
}

WebRtc_Word32
Channel::StartSend()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::StartSend()");
    {
        // A lock is needed because |_sending| can be accessed or modified by
        // another thread at the same time.
        CriticalSectionScoped cs(&_callbackCritSect);

        if (_sending)
        {
            return 0;
        }
        _sending = true;
    }

    if (_rtpRtcpModule->SetSendingStatus(true) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_RTP_RTCP_MODULE_ERROR, kTraceError,
            "StartSend() RTP/RTCP failed to start sending");
        CriticalSectionScoped cs(&_callbackCritSect);
        _sending = false;
        return -1;
    }

    return 0;
}

WebRtc_Word32
Channel::StopSend()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::StopSend()");
    {
        // A lock is needed because |_sending| can be accessed or modified by
        // another thread at the same time.
        CriticalSectionScoped cs(&_callbackCritSect);

        if (!_sending)
        {
            return 0;
        }
        _sending = false;
    }

    // Reset sending SSRC and sequence number and triggers direct transmission
    // of RTCP BYE
    if (_rtpRtcpModule->SetSendingStatus(false) == -1 ||
        _rtpRtcpModule->ResetSendDataCountersRTP() == -1)
    {
        _engineStatisticsPtr->SetLastError(
            VE_RTP_RTCP_MODULE_ERROR, kTraceWarning,
            "StartSend() RTP/RTCP failed to stop sending");
    }

    return 0;
}

WebRtc_Word32
Channel::StartReceiving()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::StartReceiving()");
    if (_receiving)
    {
        return 0;
    }
    // If external transport is used, we will only initialize/set the variables
    // after this section, since we are not using the WebRtc transport but
    // still need to keep track of e.g. if we are receiving.
#ifndef WEBRTC_EXTERNAL_TRANSPORT
    if (!_externalTransport)
    {
        if (!_socketTransportModule.ReceiveSocketsInitialized())
        {
            _engineStatisticsPtr->SetLastError(
                VE_SOCKETS_NOT_INITED, kTraceError,
                "StartReceive() must set local receiver first");
            return -1;
        }
        if (_socketTransportModule.StartReceiving(KNumberOfSocketBuffers) != 0)
        {
            _engineStatisticsPtr->SetLastError(
                VE_SOCKET_TRANSPORT_MODULE_ERROR, kTraceError,
                "StartReceiving() failed to start receiving");
            return -1;
        }
    }
#endif
    _receiving = true;
    _numberOfDiscardedPackets = 0;
    return 0;
}

WebRtc_Word32
Channel::StopReceiving()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::StopReceiving()");
    if (!_receiving)
    {
        return 0;
    }

#ifndef WEBRTC_EXTERNAL_TRANSPORT
    if (!_externalTransport &&
        _socketTransportModule.ReceiveSocketsInitialized())
    {
        if (_socketTransportModule.StopReceiving() != 0)
        {
            _engineStatisticsPtr->SetLastError(
                VE_SOCKET_TRANSPORT_MODULE_ERROR, kTraceError,
                "StopReceiving() failed to stop receiving.");
            return -1;
        }
    }
#endif
    bool dtmfDetection = _rtpRtcpModule->TelephoneEvent();
    // Recover DTMF detection status.
    WebRtc_Word32 ret = _rtpRtcpModule->SetTelephoneEventStatus(dtmfDetection,
                                                               true, true);
    if (ret != 0) {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_OPERATION, kTraceWarning,
            "StopReceiving() failed to restore telephone-event status.");
    }
    RegisterReceiveCodecsToRTPModule();
    _receiving = false;
    return 0;
}

#ifndef WEBRTC_EXTERNAL_TRANSPORT
WebRtc_Word32
Channel::SetLocalReceiver(const WebRtc_UWord16 rtpPort,
                          const WebRtc_UWord16 rtcpPort,
                          const char ipAddr[64],
                          const char multicastIpAddr[64])
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetLocalReceiver()");

    if (_externalTransport)
    {
        _engineStatisticsPtr->SetLastError(
            VE_EXTERNAL_TRANSPORT_ENABLED, kTraceError,
            "SetLocalReceiver() conflict with external transport");
        return -1;
    }

    if (_sending)
    {
        _engineStatisticsPtr->SetLastError(
            VE_ALREADY_SENDING, kTraceError,
            "SetLocalReceiver() already sending");
        return -1;
    }
    if (_receiving)
    {
        _engineStatisticsPtr->SetLastError(
            VE_ALREADY_LISTENING, kTraceError,
            "SetLocalReceiver() already receiving");
        return -1;
    }

    if (_socketTransportModule.InitializeReceiveSockets(this,
                                                        rtpPort,
                                                        ipAddr,
                                                        multicastIpAddr,
                                                        rtcpPort) != 0)
    {
        UdpTransport::ErrorCode lastSockError(
            _socketTransportModule.LastError());
        switch (lastSockError)
        {
        case UdpTransport::kIpAddressInvalid:
            _engineStatisticsPtr->SetLastError(
                VE_INVALID_IP_ADDRESS, kTraceError,
                "SetLocalReceiver() invalid IP address");
            break;
        case UdpTransport::kSocketInvalid:
            _engineStatisticsPtr->SetLastError(
                VE_SOCKET_ERROR, kTraceError,
                "SetLocalReceiver() invalid socket");
            break;
        case UdpTransport::kPortInvalid:
            _engineStatisticsPtr->SetLastError(
                VE_INVALID_PORT_NMBR, kTraceError,
                "SetLocalReceiver() invalid port");
            break;
        case UdpTransport::kFailedToBindPort:
            _engineStatisticsPtr->SetLastError(
                VE_BINDING_SOCKET_TO_LOCAL_ADDRESS_FAILED, kTraceError,
                "SetLocalReceiver() binding failed");
            break;
        default:
            _engineStatisticsPtr->SetLastError(
                VE_SOCKET_ERROR, kTraceError,
                "SetLocalReceiver() undefined socket error");
            break;
        }
        return -1;
    }
    return 0;
}
#endif

#ifndef WEBRTC_EXTERNAL_TRANSPORT
WebRtc_Word32
Channel::GetLocalReceiver(int& port, int& RTCPport, char ipAddr[64])
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::GetLocalReceiver()");

    if (_externalTransport)
    {
        _engineStatisticsPtr->SetLastError(
            VE_EXTERNAL_TRANSPORT_ENABLED, kTraceError,
            "SetLocalReceiver() conflict with external transport");
        return -1;
    }

    char ipAddrTmp[UdpTransport::kIpAddressVersion6Length] = {0};
    WebRtc_UWord16 rtpPort(0);
    WebRtc_UWord16 rtcpPort(0);
    char multicastIpAddr[UdpTransport::kIpAddressVersion6Length] = {0};

    // Acquire socket information from the socket module
    if (_socketTransportModule.ReceiveSocketInformation(ipAddrTmp,
                                                        rtpPort,
                                                        rtcpPort,
                                                        multicastIpAddr) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_CANNOT_GET_SOCKET_INFO, kTraceError,
            "GetLocalReceiver() unable to retrieve socket information");
        return -1;
    }

    // Deliver valid results to the user
    port = static_cast<int> (rtpPort);
    RTCPport = static_cast<int> (rtcpPort);
    if (ipAddr != NULL)
    {
        strcpy(ipAddr, ipAddrTmp);
    }
    return 0;
}
#endif

#ifndef WEBRTC_EXTERNAL_TRANSPORT
WebRtc_Word32
Channel::SetSendDestination(const WebRtc_UWord16 rtpPort,
                            const char ipAddr[64],
                            const int sourcePort,
                            const WebRtc_UWord16 rtcpPort)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetSendDestination()");

    if (_externalTransport)
    {
        _engineStatisticsPtr->SetLastError(
            VE_EXTERNAL_TRANSPORT_ENABLED, kTraceError,
            "SetSendDestination() conflict with external transport");
        return -1;
    }

    // Initialize ports and IP address for the remote (destination) side.
    // By default, the sockets used for receiving are used for transmission as
    // well, hence the source ports for outgoing packets are the same as the
    // receiving ports specified in SetLocalReceiver.
    // If an extra send socket has been created, it will be utilized until a
    // new source port is specified or until the channel has been deleted and
    // recreated. If no socket exists, sockets will be created when the first
    // RTP and RTCP packets shall be transmitted (see e.g.
    // UdpTransportImpl::SendPacket()).
    //
    // NOTE: this function does not require that sockets exists; all it does is
    // to build send structures to be used with the sockets when they exist.
    // It is therefore possible to call this method before SetLocalReceiver.
    // However, sockets must exist if a multi-cast address is given as input.

    // Build send structures and enable QoS (if enabled and supported)
    if (_socketTransportModule.InitializeSendSockets(
        ipAddr, rtpPort, rtcpPort) != UdpTransport::kNoSocketError)
    {
        UdpTransport::ErrorCode lastSockError(
            _socketTransportModule.LastError());
        switch (lastSockError)
        {
        case UdpTransport::kIpAddressInvalid:
            _engineStatisticsPtr->SetLastError(
                VE_INVALID_IP_ADDRESS, kTraceError,
                "SetSendDestination() invalid IP address 1");
            break;
        case UdpTransport::kSocketInvalid:
            _engineStatisticsPtr->SetLastError(
                VE_SOCKET_ERROR, kTraceError,
                "SetSendDestination() invalid socket 1");
            break;
        case UdpTransport::kQosError:
            _engineStatisticsPtr->SetLastError(
                VE_GQOS_ERROR, kTraceError,
                "SetSendDestination() failed to set QoS");
            break;
        case UdpTransport::kMulticastAddressInvalid:
            _engineStatisticsPtr->SetLastError(
                VE_INVALID_MULTICAST_ADDRESS, kTraceError,
                "SetSendDestination() invalid multicast address");
            break;
        default:
            _engineStatisticsPtr->SetLastError(
                VE_SOCKET_ERROR, kTraceError,
                "SetSendDestination() undefined socket error 1");
            break;
        }
        return -1;
    }

    // Check if the user has specified a non-default source port different from
    // the local receive port.
    // If so, an extra local socket will be created unless the source port is
    // not unique.
    if (sourcePort != kVoEDefault)
    {
        WebRtc_UWord16 receiverRtpPort(0);
        WebRtc_UWord16 rtcpNA(0);
        if (_socketTransportModule.ReceiveSocketInformation(NULL,
                                                            receiverRtpPort,
                                                            rtcpNA,
                                                            NULL) != 0)
        {
            _engineStatisticsPtr->SetLastError(
                VE_CANNOT_GET_SOCKET_INFO, kTraceError,
                "SetSendDestination() failed to retrieve socket information");
            return -1;
        }

        WebRtc_UWord16 sourcePortUW16 =
                static_cast<WebRtc_UWord16> (sourcePort);

        // An extra socket will only be created if the specified source port
        // differs from the local receive port.
        if (sourcePortUW16 != receiverRtpPort)
        {
            // Initialize extra local socket to get a different source port
            // than the local
            // receiver port. Always use default source for RTCP.
            // Note that, this calls UdpTransport::CloseSendSockets().
            if (_socketTransportModule.InitializeSourcePorts(
                sourcePortUW16,
                sourcePortUW16+1) != 0)
            {
                UdpTransport::ErrorCode lastSockError(
                    _socketTransportModule.LastError());
                switch (lastSockError)
                {
                case UdpTransport::kIpAddressInvalid:
                    _engineStatisticsPtr->SetLastError(
                        VE_INVALID_IP_ADDRESS, kTraceError,
                        "SetSendDestination() invalid IP address 2");
                    break;
                case UdpTransport::kSocketInvalid:
                    _engineStatisticsPtr->SetLastError(
                        VE_SOCKET_ERROR, kTraceError,
                        "SetSendDestination() invalid socket 2");
                    break;
                default:
                    _engineStatisticsPtr->SetLastError(
                        VE_SOCKET_ERROR, kTraceError,
                        "SetSendDestination() undefined socket error 2");
                    break;
                }
                return -1;
            }
            WEBRTC_TRACE(kTraceInfo, kTraceVoice,
                         VoEId(_instanceId,_channelId),
                         "SetSendDestination() extra local socket is created"
                         " to facilitate unique source port");
        }
        else
        {
            WEBRTC_TRACE(kTraceInfo, kTraceVoice,
                         VoEId(_instanceId,_channelId),
                         "SetSendDestination() sourcePort equals the local"
                         " receive port => no extra socket is created");
        }
    }

    return 0;
}
#endif

#ifndef WEBRTC_EXTERNAL_TRANSPORT
WebRtc_Word32
Channel::GetSendDestination(int& port,
                            char ipAddr[64],
                            int& sourcePort,
                            int& RTCPport)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::GetSendDestination()");

    if (_externalTransport)
    {
        _engineStatisticsPtr->SetLastError(
            VE_EXTERNAL_TRANSPORT_ENABLED, kTraceError,
            "GetSendDestination() conflict with external transport");
        return -1;
    }

    char ipAddrTmp[UdpTransport::kIpAddressVersion6Length] = {0};
    WebRtc_UWord16 rtpPort(0);
    WebRtc_UWord16 rtcpPort(0);
    WebRtc_UWord16 rtpSourcePort(0);
    WebRtc_UWord16 rtcpSourcePort(0);

    // Acquire sending socket information from the socket module
    _socketTransportModule.SendSocketInformation(ipAddrTmp, rtpPort, rtcpPort);
    _socketTransportModule.SourcePorts(rtpSourcePort, rtcpSourcePort);

    // Deliver valid results to the user
    port = static_cast<int> (rtpPort);
    RTCPport = static_cast<int> (rtcpPort);
    sourcePort = static_cast<int> (rtpSourcePort);
    if (ipAddr != NULL)
    {
        strcpy(ipAddr, ipAddrTmp);
    }

    return 0;
}
#endif


WebRtc_Word32
Channel::SetNetEQPlayoutMode(NetEqModes mode)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetNetEQPlayoutMode()");
    AudioPlayoutMode playoutMode(voice);
    switch (mode)
    {
        case kNetEqDefault:
            playoutMode = voice;
            break;
        case kNetEqStreaming:
            playoutMode = streaming;
            break;
        case kNetEqFax:
            playoutMode = fax;
            break;
        case kNetEqOff:
            playoutMode = off;
            break;
    }
    if (_audioCodingModule.SetPlayoutMode(playoutMode) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
            "SetNetEQPlayoutMode() failed to set playout mode");
        return -1;
    }
    return 0;
}

WebRtc_Word32
Channel::GetNetEQPlayoutMode(NetEqModes& mode)
{
    const AudioPlayoutMode playoutMode = _audioCodingModule.PlayoutMode();
    switch (playoutMode)
    {
        case voice:
            mode = kNetEqDefault;
            break;
        case streaming:
            mode = kNetEqStreaming;
            break;
        case fax:
            mode = kNetEqFax;
            break;
        case off:
            mode = kNetEqOff;
    }
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
                 VoEId(_instanceId,_channelId),
                 "Channel::GetNetEQPlayoutMode() => mode=%u", mode);
    return 0;
}

WebRtc_Word32
Channel::SetNetEQBGNMode(NetEqBgnModes mode)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetNetEQPlayoutMode()");
    ACMBackgroundNoiseMode noiseMode(On);
    switch (mode)
    {
        case kBgnOn:
            noiseMode = On;
            break;
        case kBgnFade:
            noiseMode = Fade;
            break;
        case kBgnOff:
            noiseMode = Off;
            break;
    }
    if (_audioCodingModule.SetBackgroundNoiseMode(noiseMode) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
            "SetBackgroundNoiseMode() failed to set noise mode");
        return -1;
    }
    return 0;
}

WebRtc_Word32
Channel::SetOnHoldStatus(bool enable, OnHoldModes mode)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetOnHoldStatus()");
    if (mode == kHoldSendAndPlay)
    {
        _outputIsOnHold = enable;
        _inputIsOnHold = enable;
    }
    else if (mode == kHoldPlayOnly)
    {
        _outputIsOnHold = enable;
    }
    if (mode == kHoldSendOnly)
    {
        _inputIsOnHold = enable;
    }
    return 0;
}

WebRtc_Word32
Channel::GetOnHoldStatus(bool& enabled, OnHoldModes& mode)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::GetOnHoldStatus()");
    enabled = (_outputIsOnHold || _inputIsOnHold);
    if (_outputIsOnHold && _inputIsOnHold)
    {
        mode = kHoldSendAndPlay;
    }
    else if (_outputIsOnHold && !_inputIsOnHold)
    {
        mode = kHoldPlayOnly;
    }
    else if (!_outputIsOnHold && _inputIsOnHold)
    {
        mode = kHoldSendOnly;
    }
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::GetOnHoldStatus() => enabled=%d, mode=%d",
                 enabled, mode);
    return 0;
}

WebRtc_Word32
Channel::RegisterVoiceEngineObserver(VoiceEngineObserver& observer)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::RegisterVoiceEngineObserver()");
    CriticalSectionScoped cs(&_callbackCritSect);

    if (_voiceEngineObserverPtr)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_OPERATION, kTraceError,
            "RegisterVoiceEngineObserver() observer already enabled");
        return -1;
    }
    _voiceEngineObserverPtr = &observer;
    return 0;
}

WebRtc_Word32
Channel::DeRegisterVoiceEngineObserver()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::DeRegisterVoiceEngineObserver()");
    CriticalSectionScoped cs(&_callbackCritSect);

    if (!_voiceEngineObserverPtr)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_OPERATION, kTraceWarning,
            "DeRegisterVoiceEngineObserver() observer already disabled");
        return 0;
    }
    _voiceEngineObserverPtr = NULL;
    return 0;
}

WebRtc_Word32
Channel::GetNetEQBGNMode(NetEqBgnModes& mode)
{
  ACMBackgroundNoiseMode noiseMode(On);
    _audioCodingModule.BackgroundNoiseMode(noiseMode);
    switch (noiseMode)
    {
        case On:
            mode = kBgnOn;
            break;
        case Fade:
            mode = kBgnFade;
            break;
        case Off:
            mode = kBgnOff;
            break;
    }
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::GetNetEQBGNMode() => mode=%u", mode);
    return 0;
}

WebRtc_Word32
Channel::GetSendCodec(CodecInst& codec)
{
    return (_audioCodingModule.SendCodec(codec));
}

WebRtc_Word32
Channel::GetRecCodec(CodecInst& codec)
{
    return (_audioCodingModule.ReceiveCodec(codec));
}

WebRtc_Word32
Channel::SetSendCodec(const CodecInst& codec)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetSendCodec()");

    if (_audioCodingModule.RegisterSendCodec(codec) != 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,_channelId),
                     "SetSendCodec() failed to register codec to ACM");
        return -1;
    }

    if (_rtpRtcpModule->RegisterSendPayload(codec) != 0)
    {
        _rtpRtcpModule->DeRegisterSendPayload(codec.pltype);
        if (_rtpRtcpModule->RegisterSendPayload(codec) != 0)
        {
            WEBRTC_TRACE(
                    kTraceError, kTraceVoice, VoEId(_instanceId,_channelId),
                    "SetSendCodec() failed to register codec to"
                    " RTP/RTCP module");
            return -1;
        }
    }

    if (_rtpRtcpModule->SetAudioPacketSize(codec.pacsize) != 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,_channelId),
                     "SetSendCodec() failed to set audio packet size");
        return -1;
    }

    return 0;
}

WebRtc_Word32
Channel::SetVADStatus(bool enableVAD, ACMVADMode mode, bool disableDTX)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetVADStatus(mode=%d)", mode);
    // To disable VAD, DTX must be disabled too
    disableDTX = ((enableVAD == false) ? true : disableDTX);
    if (_audioCodingModule.SetVAD(!disableDTX, enableVAD, mode) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
            "SetVADStatus() failed to set VAD");
        return -1;
    }
    return 0;
}

WebRtc_Word32
Channel::GetVADStatus(bool& enabledVAD, ACMVADMode& mode, bool& disabledDTX)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::GetVADStatus");
    if (_audioCodingModule.VAD(disabledDTX, enabledVAD, mode) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
            "GetVADStatus() failed to get VAD status");
        return -1;
    }
    disabledDTX = !disabledDTX;
    return 0;
}

WebRtc_Word32
Channel::SetRecPayloadType(const CodecInst& codec)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetRecPayloadType()");

    if (_playing)
    {
        _engineStatisticsPtr->SetLastError(
            VE_ALREADY_PLAYING, kTraceError,
            "SetRecPayloadType() unable to set PT while playing");
        return -1;
    }
    if (_receiving)
    {
        _engineStatisticsPtr->SetLastError(
            VE_ALREADY_LISTENING, kTraceError,
            "SetRecPayloadType() unable to set PT while listening");
        return -1;
    }

    if (codec.pltype == -1)
    {
        // De-register the selected codec (RTP/RTCP module and ACM)

        WebRtc_Word8 pltype(-1);
        CodecInst rxCodec = codec;

        // Get payload type for the given codec
        _rtpRtcpModule->ReceivePayloadType(rxCodec, &pltype);
        rxCodec.pltype = pltype;

        if (_rtpRtcpModule->DeRegisterReceivePayload(pltype) != 0)
        {
            _engineStatisticsPtr->SetLastError(
                    VE_RTP_RTCP_MODULE_ERROR,
                    kTraceError,
                    "SetRecPayloadType() RTP/RTCP-module deregistration "
                    "failed");
            return -1;
        }
        if (_audioCodingModule.UnregisterReceiveCodec(rxCodec.pltype) != 0)
        {
            _engineStatisticsPtr->SetLastError(
                VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
                "SetRecPayloadType() ACM deregistration failed - 1");
            return -1;
        }
        return 0;
    }

    CodecInst acmCodec = codec;
    VoECodecImpl::ExternalToACMCodecRepresentation(acmCodec, codec);
    if (_rtpRtcpModule->RegisterReceivePayload(codec) != 0)
    {
        // First attempt to register failed => de-register and try again
        _rtpRtcpModule->DeRegisterReceivePayload(acmCodec.pltype);
        if (_rtpRtcpModule->RegisterReceivePayload(acmCodec) != 0)
        {
            _engineStatisticsPtr->SetLastError(
                VE_RTP_RTCP_MODULE_ERROR, kTraceError,
                "SetRecPayloadType() RTP/RTCP-module registration failed");
            return -1;
        }
    }
    if (_audioCodingModule.RegisterReceiveCodec(acmCodec) != 0)
    {
        _audioCodingModule.UnregisterReceiveCodec(acmCodec.pltype);
        if (_audioCodingModule.RegisterReceiveCodec(acmCodec) != 0)
        {
            _engineStatisticsPtr->SetLastError(
                VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
                "SetRecPayloadType() ACM registration failed - 1");
            return -1;
        }
    }
    return 0;
}

WebRtc_Word32
Channel::GetRecPayloadType(CodecInst& codec)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::GetRecPayloadType()");
    WebRtc_Word8 payloadType(-1);
    CodecInst acmCodec;
    if (_rtpRtcpModule->ReceivePayloadType(acmCodec, &payloadType) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_RTP_RTCP_MODULE_ERROR, kTraceWarning,
            "GetRecPayloadType() failed to retrieve RX payload type");
        return -1;
    }
    VoECodecImpl::ACMToExternalCodecRepresentation(codec, acmCodec);
    codec.pltype = payloadType;
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::GetRecPayloadType() => pltype=%u", codec.pltype);
    return 0;
}

WebRtc_Word32
Channel::SetAMREncFormat(AmrMode mode)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetAMREncFormat()");

    // ACM doesn't support AMR
    return -1;
}

WebRtc_Word32
Channel::SetAMRDecFormat(AmrMode mode)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetAMRDecFormat()");

    // ACM doesn't support AMR
    return -1;
}

WebRtc_Word32
Channel::SetAMRWbEncFormat(AmrMode mode)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetAMRWbEncFormat()");

    // ACM doesn't support AMR
    return -1;

}

WebRtc_Word32
Channel::SetAMRWbDecFormat(AmrMode mode)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetAMRWbDecFormat()");

    // ACM doesn't support AMR
    return -1;
}

WebRtc_Word32
Channel::SetSendCNPayloadType(int type, PayloadFrequencies frequency)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetSendCNPayloadType()");

    CodecInst codec;
    WebRtc_Word32 samplingFreqHz(-1);
    const int kMono = 1;
    if (frequency == kFreq32000Hz)
        samplingFreqHz = 32000;
    else if (frequency == kFreq16000Hz)
        samplingFreqHz = 16000;

    if (_audioCodingModule.Codec("CN", codec, samplingFreqHz, kMono) == -1)
    {
        _engineStatisticsPtr->SetLastError(
            VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
            "SetSendCNPayloadType() failed to retrieve default CN codec "
            "settings");
        return -1;
    }

    // Modify the payload type (must be set to dynamic range)
    codec.pltype = type;

    if (_audioCodingModule.RegisterSendCodec(codec) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
            "SetSendCNPayloadType() failed to register CN to ACM");
        return -1;
    }

    if (_rtpRtcpModule->RegisterSendPayload(codec) != 0)
    {
        _rtpRtcpModule->DeRegisterSendPayload(codec.pltype);
        if (_rtpRtcpModule->RegisterSendPayload(codec) != 0)
        {
            _engineStatisticsPtr->SetLastError(
                VE_RTP_RTCP_MODULE_ERROR, kTraceError,
                "SetSendCNPayloadType() failed to register CN to RTP/RTCP "
                "module");
            return -1;
        }
    }
    return 0;
}

WebRtc_Word32
Channel::SetISACInitTargetRate(int rateBps, bool useFixedFrameSize)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetISACInitTargetRate()");

    CodecInst sendCodec;
    if (_audioCodingModule.SendCodec(sendCodec) == -1)
    {
        _engineStatisticsPtr->SetLastError(
            VE_CODEC_ERROR, kTraceError,
            "SetISACInitTargetRate() failed to retrieve send codec");
        return -1;
    }
    if (STR_CASE_CMP(sendCodec.plname, "ISAC") != 0)
    {
        // This API is only valid if iSAC is setup to run in channel-adaptive
        // mode.
        // We do not validate the adaptive mode here. It is done later in the
        // ConfigISACBandwidthEstimator() API.
        _engineStatisticsPtr->SetLastError(
            VE_CODEC_ERROR, kTraceError,
            "SetISACInitTargetRate() send codec is not iSAC");
        return -1;
    }

    WebRtc_UWord8 initFrameSizeMsec(0);
    if (16000 == sendCodec.plfreq)
    {
        // Note that 0 is a valid and corresponds to "use default
        if ((rateBps != 0 &&
            rateBps < kVoiceEngineMinIsacInitTargetRateBpsWb) ||
            (rateBps > kVoiceEngineMaxIsacInitTargetRateBpsWb))
        {
             _engineStatisticsPtr->SetLastError(
                VE_INVALID_ARGUMENT, kTraceError,
                "SetISACInitTargetRate() invalid target rate - 1");
            return -1;
        }
        // 30 or 60ms
        initFrameSizeMsec = (WebRtc_UWord8)(sendCodec.pacsize / 16);
    }
    else if (32000 == sendCodec.plfreq)
    {
        if ((rateBps != 0 &&
            rateBps < kVoiceEngineMinIsacInitTargetRateBpsSwb) ||
            (rateBps > kVoiceEngineMaxIsacInitTargetRateBpsSwb))
        {
            _engineStatisticsPtr->SetLastError(
                VE_INVALID_ARGUMENT, kTraceError,
                "SetISACInitTargetRate() invalid target rate - 2");
            return -1;
        }
        initFrameSizeMsec = (WebRtc_UWord8)(sendCodec.pacsize / 32); // 30ms
    }

    if (_audioCodingModule.ConfigISACBandwidthEstimator(
        initFrameSizeMsec, rateBps, useFixedFrameSize) == -1)
    {
        _engineStatisticsPtr->SetLastError(
            VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
            "SetISACInitTargetRate() iSAC BWE config failed");
        return -1;
    }

    return 0;
}

WebRtc_Word32
Channel::SetISACMaxRate(int rateBps)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetISACMaxRate()");

    CodecInst sendCodec;
    if (_audioCodingModule.SendCodec(sendCodec) == -1)
    {
        _engineStatisticsPtr->SetLastError(
            VE_CODEC_ERROR, kTraceError,
            "SetISACMaxRate() failed to retrieve send codec");
        return -1;
    }
    if (STR_CASE_CMP(sendCodec.plname, "ISAC") != 0)
    {
        // This API is only valid if iSAC is selected as sending codec.
        _engineStatisticsPtr->SetLastError(
            VE_CODEC_ERROR, kTraceError,
            "SetISACMaxRate() send codec is not iSAC");
        return -1;
    }
    if (16000 == sendCodec.plfreq)
    {
        if ((rateBps < kVoiceEngineMinIsacMaxRateBpsWb) ||
            (rateBps > kVoiceEngineMaxIsacMaxRateBpsWb))
        {
            _engineStatisticsPtr->SetLastError(
                VE_INVALID_ARGUMENT, kTraceError,
                "SetISACMaxRate() invalid max rate - 1");
            return -1;
        }
    }
    else if (32000 == sendCodec.plfreq)
    {
        if ((rateBps < kVoiceEngineMinIsacMaxRateBpsSwb) ||
            (rateBps > kVoiceEngineMaxIsacMaxRateBpsSwb))
        {
            _engineStatisticsPtr->SetLastError(
                VE_INVALID_ARGUMENT, kTraceError,
                "SetISACMaxRate() invalid max rate - 2");
            return -1;
        }
    }
    if (_sending)
    {
        _engineStatisticsPtr->SetLastError(
            VE_SENDING, kTraceError,
            "SetISACMaxRate() unable to set max rate while sending");
        return -1;
    }

    // Set the maximum instantaneous rate of iSAC (works for both adaptive
    // and non-adaptive mode)
    if (_audioCodingModule.SetISACMaxRate(rateBps) == -1)
    {
        _engineStatisticsPtr->SetLastError(
            VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
            "SetISACMaxRate() failed to set max rate");
        return -1;
    }

    return 0;
}

WebRtc_Word32
Channel::SetISACMaxPayloadSize(int sizeBytes)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetISACMaxPayloadSize()");
    CodecInst sendCodec;
    if (_audioCodingModule.SendCodec(sendCodec) == -1)
    {
        _engineStatisticsPtr->SetLastError(
            VE_CODEC_ERROR, kTraceError,
            "SetISACMaxPayloadSize() failed to retrieve send codec");
        return -1;
    }
    if (STR_CASE_CMP(sendCodec.plname, "ISAC") != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_CODEC_ERROR, kTraceError,
            "SetISACMaxPayloadSize() send codec is not iSAC");
        return -1;
    }
    if (16000 == sendCodec.plfreq)
    {
        if ((sizeBytes < kVoiceEngineMinIsacMaxPayloadSizeBytesWb) ||
            (sizeBytes > kVoiceEngineMaxIsacMaxPayloadSizeBytesWb))
        {
            _engineStatisticsPtr->SetLastError(
                VE_INVALID_ARGUMENT, kTraceError,
                "SetISACMaxPayloadSize() invalid max payload - 1");
            return -1;
        }
    }
    else if (32000 == sendCodec.plfreq)
    {
        if ((sizeBytes < kVoiceEngineMinIsacMaxPayloadSizeBytesSwb) ||
            (sizeBytes > kVoiceEngineMaxIsacMaxPayloadSizeBytesSwb))
        {
            _engineStatisticsPtr->SetLastError(
                VE_INVALID_ARGUMENT, kTraceError,
                "SetISACMaxPayloadSize() invalid max payload - 2");
            return -1;
        }
    }
    if (_sending)
    {
        _engineStatisticsPtr->SetLastError(
            VE_SENDING, kTraceError,
            "SetISACMaxPayloadSize() unable to set max rate while sending");
        return -1;
    }

    if (_audioCodingModule.SetISACMaxPayloadSize(sizeBytes) == -1)
    {
        _engineStatisticsPtr->SetLastError(
            VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
            "SetISACMaxPayloadSize() failed to set max payload size");
        return -1;
    }
    return 0;
}

WebRtc_Word32 Channel::RegisterExternalTransport(Transport& transport)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::RegisterExternalTransport()");

    CriticalSectionScoped cs(&_callbackCritSect);

#ifndef WEBRTC_EXTERNAL_TRANSPORT
    // Sanity checks for default (non external transport) to avoid conflict with
    // WebRtc sockets.
    if (_socketTransportModule.SendSocketsInitialized())
    {
        _engineStatisticsPtr->SetLastError(VE_SEND_SOCKETS_CONFLICT,
                                           kTraceError,
                "RegisterExternalTransport() send sockets already initialized");
        return -1;
    }
    if (_socketTransportModule.ReceiveSocketsInitialized())
    {
        _engineStatisticsPtr->SetLastError(VE_RECEIVE_SOCKETS_CONFLICT,
                                           kTraceError,
             "RegisterExternalTransport() receive sockets already initialized");
        return -1;
    }
#endif
    if (_externalTransport)
    {
        _engineStatisticsPtr->SetLastError(VE_INVALID_OPERATION,
                                           kTraceError,
              "RegisterExternalTransport() external transport already enabled");
       return -1;
    }
    _externalTransport = true;
    _transportPtr = &transport;
    return 0;
}

WebRtc_Word32
Channel::DeRegisterExternalTransport()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::DeRegisterExternalTransport()");

    CriticalSectionScoped cs(&_callbackCritSect);

    if (!_transportPtr)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_OPERATION, kTraceWarning,
            "DeRegisterExternalTransport() external transport already "
            "disabled");
        return 0;
    }
    _externalTransport = false;
#ifdef WEBRTC_EXTERNAL_TRANSPORT
    _transportPtr = NULL;
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "DeRegisterExternalTransport() all transport is disabled");
#else
    _transportPtr = &_socketTransportModule;
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "DeRegisterExternalTransport() internal Transport is enabled");
#endif
    return 0;
}

WebRtc_Word32
Channel::ReceivedRTPPacket(const WebRtc_Word8* data, WebRtc_Word32 length)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::ReceivedRTPPacket()");
    const char dummyIP[] = "127.0.0.1";
    IncomingRTPPacket(data, length, dummyIP, 0);
    return 0;
}

WebRtc_Word32
Channel::ReceivedRTCPPacket(const WebRtc_Word8* data, WebRtc_Word32 length)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::ReceivedRTCPPacket()");
    const char dummyIP[] = "127.0.0.1";
    IncomingRTCPPacket(data, length, dummyIP, 0);
    return 0;
}

#ifndef WEBRTC_EXTERNAL_TRANSPORT
WebRtc_Word32
Channel::GetSourceInfo(int& rtpPort, int& rtcpPort, char ipAddr[64])
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::GetSourceInfo()");

    WebRtc_UWord16 rtpPortModule;
    WebRtc_UWord16 rtcpPortModule;
    char ipaddr[UdpTransport::kIpAddressVersion6Length] = {0};

    if (_socketTransportModule.RemoteSocketInformation(ipaddr,
                                                       rtpPortModule,
                                                       rtcpPortModule) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_SOCKET_TRANSPORT_MODULE_ERROR, kTraceError,
            "GetSourceInfo() failed to retrieve remote socket information");
        return -1;
    }
    strcpy(ipAddr, ipaddr);
    rtpPort = rtpPortModule;
    rtcpPort = rtcpPortModule;

    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId,_channelId),
        "GetSourceInfo() => rtpPort=%d, rtcpPort=%d, ipAddr=%s",
        rtpPort, rtcpPort, ipAddr);
    return 0;
}

WebRtc_Word32
Channel::EnableIPv6()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::EnableIPv6()");
    if (_socketTransportModule.ReceiveSocketsInitialized() ||
        _socketTransportModule.SendSocketsInitialized())
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_OPERATION, kTraceError,
            "EnableIPv6() socket layer is already initialized");
        return -1;
    }
    if (_socketTransportModule.EnableIpV6() != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_SOCKET_ERROR, kTraceError,
            "EnableIPv6() failed to enable IPv6");
        const UdpTransport::ErrorCode lastError =
            _socketTransportModule.LastError();
        WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                     "UdpTransport::LastError() => %d", lastError);
        return -1;
    }
    return 0;
}

bool
Channel::IPv6IsEnabled() const
{
    bool isEnabled = _socketTransportModule.IpV6Enabled();
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "IPv6IsEnabled() => %d", isEnabled);
    return isEnabled;
}

WebRtc_Word32
Channel::SetSourceFilter(int rtpPort, int rtcpPort, const char ipAddr[64])
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetSourceFilter()");
    if (_socketTransportModule.SetFilterPorts(
        static_cast<WebRtc_UWord16>(rtpPort),
        static_cast<WebRtc_UWord16>(rtcpPort)) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_SOCKET_TRANSPORT_MODULE_ERROR, kTraceError,
            "SetSourceFilter() failed to set filter ports");
        const UdpTransport::ErrorCode lastError =
            _socketTransportModule.LastError();
        WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                     "UdpTransport::LastError() => %d",
                     lastError);
        return -1;
    }
    const char* filterIpAddress = ipAddr;
    if (_socketTransportModule.SetFilterIP(filterIpAddress) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_IP_ADDRESS, kTraceError,
            "SetSourceFilter() failed to set filter IP address");
        const UdpTransport::ErrorCode lastError =
           _socketTransportModule.LastError();
        WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                     "UdpTransport::LastError() => %d", lastError);
        return -1;
    }
    return 0;
}

WebRtc_Word32
Channel::GetSourceFilter(int& rtpPort, int& rtcpPort, char ipAddr[64])
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::GetSourceFilter()");
    WebRtc_UWord16 rtpFilterPort(0);
    WebRtc_UWord16 rtcpFilterPort(0);
    if (_socketTransportModule.FilterPorts(rtpFilterPort, rtcpFilterPort) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_SOCKET_TRANSPORT_MODULE_ERROR, kTraceWarning,
            "GetSourceFilter() failed to retrieve filter ports");
    }
    char ipAddrTmp[UdpTransport::kIpAddressVersion6Length] = {0};
    if (_socketTransportModule.FilterIP(ipAddrTmp) != 0)
    {
        // no filter has been configured (not seen as an error)
        memset(ipAddrTmp,
               0, UdpTransport::kIpAddressVersion6Length);
    }
    rtpPort = static_cast<int> (rtpFilterPort);
    rtcpPort = static_cast<int> (rtcpFilterPort);
    strcpy(ipAddr, ipAddrTmp);
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId,_channelId),
        "GetSourceFilter() => rtpPort=%d, rtcpPort=%d, ipAddr=%s",
        rtpPort, rtcpPort, ipAddr);
    return 0;
}

WebRtc_Word32
Channel::SetSendTOS(int DSCP, int priority, bool useSetSockopt)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetSendTOS(DSCP=%d, useSetSockopt=%d)",
                 DSCP, (int)useSetSockopt);

    // Set TOS value and possibly try to force usage of setsockopt()
    if (_socketTransportModule.SetToS(DSCP, useSetSockopt) != 0)
    {
        UdpTransport::ErrorCode lastSockError(
            _socketTransportModule.LastError());
        switch (lastSockError)
        {
        case UdpTransport::kTosError:
            _engineStatisticsPtr->SetLastError(VE_TOS_ERROR, kTraceError,
                                               "SetSendTOS() TOS error");
            break;
        case UdpTransport::kQosError:
            _engineStatisticsPtr->SetLastError(
                    VE_TOS_GQOS_CONFLICT, kTraceError,
                    "SetSendTOS() GQOS error");
            break;
        case UdpTransport::kTosInvalid:
            // can't switch SetSockOpt method without disabling TOS first, or
            // SetSockopt() call failed
            _engineStatisticsPtr->SetLastError(VE_TOS_INVALID, kTraceError,
                                               "SetSendTOS() invalid TOS");
            break;
        case UdpTransport::kSocketInvalid:
            _engineStatisticsPtr->SetLastError(VE_SOCKET_ERROR, kTraceError,
                                               "SetSendTOS() invalid Socket");
            break;
        default:
            _engineStatisticsPtr->SetLastError(VE_TOS_ERROR, kTraceError,
                                               "SetSendTOS() TOS error");
            break;
        }
        WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,_channelId),
                     "UdpTransport =>  lastError = %d",
                     lastSockError);
        return -1;
    }

    // Set priority (PCP) value, -1 means don't change
    if (-1 != priority)
    {
        if (_socketTransportModule.SetPCP(priority) != 0)
        {
            UdpTransport::ErrorCode lastSockError(
                _socketTransportModule.LastError());
            switch (lastSockError)
            {
            case UdpTransport::kPcpError:
                _engineStatisticsPtr->SetLastError(VE_TOS_ERROR, kTraceError,
                                                   "SetSendTOS() PCP error");
                break;
            case UdpTransport::kQosError:
                _engineStatisticsPtr->SetLastError(
                        VE_TOS_GQOS_CONFLICT, kTraceError,
                        "SetSendTOS() GQOS conflict");
                break;
            case UdpTransport::kSocketInvalid:
                _engineStatisticsPtr->SetLastError(
                        VE_SOCKET_ERROR, kTraceError,
                        "SetSendTOS() invalid Socket");
                break;
            default:
                _engineStatisticsPtr->SetLastError(VE_TOS_ERROR, kTraceError,
                                                   "SetSendTOS() PCP error");
                break;
            }
            WEBRTC_TRACE(kTraceError, kTraceVoice,
                         VoEId(_instanceId,_channelId),
                         "UdpTransport =>  lastError = %d",
                         lastSockError);
            return -1;
        }
    }

    return 0;
}

WebRtc_Word32
Channel::GetSendTOS(int &DSCP, int& priority, bool &useSetSockopt)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::GetSendTOS(DSCP=?, useSetSockopt=?)");
    WebRtc_Word32 dscp(0), prio(0);
    bool setSockopt(false);
    if (_socketTransportModule.ToS(dscp, setSockopt) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_SOCKET_TRANSPORT_MODULE_ERROR, kTraceError,
            "GetSendTOS() failed to get TOS info");
        return -1;
    }
    if (_socketTransportModule.PCP(prio) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_SOCKET_TRANSPORT_MODULE_ERROR, kTraceError,
            "GetSendTOS() failed to get PCP info");
        return -1;
    }
    DSCP = static_cast<int> (dscp);
    priority = static_cast<int> (prio);
    useSetSockopt = setSockopt;
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId,-1),
                 "GetSendTOS() => DSCP=%d, priority=%d, useSetSockopt=%d",
        DSCP, priority, (int)useSetSockopt);
    return 0;
}

#if defined(_WIN32)
WebRtc_Word32
Channel::SetSendGQoS(bool enable, int serviceType, int overrideDSCP)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetSendGQoS(enable=%d, serviceType=%d, "
                 "overrideDSCP=%d)",
                 (int)enable, serviceType, overrideDSCP);
    if(!_socketTransportModule.ReceiveSocketsInitialized())
    {
        _engineStatisticsPtr->SetLastError(
            VE_SOCKETS_NOT_INITED, kTraceError,
            "SetSendGQoS() GQoS state must be set after sockets are created");
        return -1;
    }
    if(!_socketTransportModule.SendSocketsInitialized())
    {
        _engineStatisticsPtr->SetLastError(
            VE_DESTINATION_NOT_INITED, kTraceError,
            "SetSendGQoS() GQoS state must be set after sending side is "
            "initialized");
        return -1;
    }
    if (enable &&
       (serviceType != SERVICETYPE_BESTEFFORT) &&
       (serviceType != SERVICETYPE_CONTROLLEDLOAD) &&
       (serviceType != SERVICETYPE_GUARANTEED) &&
       (serviceType != SERVICETYPE_QUALITATIVE))
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "SetSendGQoS() Invalid service type");
        return -1;
    }
    if (enable && ((overrideDSCP <  0) || (overrideDSCP > 63)))
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "SetSendGQoS() Invalid overrideDSCP value");
        return -1;
    }

    // Avoid GQoS/ToS conflict when user wants to override the default DSCP
    // mapping
    bool QoS(false);
    WebRtc_Word32 sType(0);
    WebRtc_Word32 ovrDSCP(0);
    if (_socketTransportModule.QoS(QoS, sType, ovrDSCP))
    {
        _engineStatisticsPtr->SetLastError(
            VE_SOCKET_TRANSPORT_MODULE_ERROR, kTraceError,
            "SetSendGQoS() failed to get QOS info");
        return -1;
    }
    if (QoS && ovrDSCP == 0 && overrideDSCP != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_TOS_GQOS_CONFLICT, kTraceError,
            "SetSendGQoS() QOS is already enabled and overrideDSCP differs,"
            " not allowed");
        return -1;
    }
    const WebRtc_Word32 maxBitrate(0);
    if (_socketTransportModule.SetQoS(enable,
                                      static_cast<WebRtc_Word32>(serviceType),
                                      maxBitrate,
                                      static_cast<WebRtc_Word32>(overrideDSCP),
                                      true))
    {
        UdpTransport::ErrorCode lastSockError(
            _socketTransportModule.LastError());
        switch (lastSockError)
        {
        case UdpTransport::kQosError:
            _engineStatisticsPtr->SetLastError(VE_GQOS_ERROR, kTraceError,
                                               "SetSendGQoS() QOS error");
            break;
        default:
            _engineStatisticsPtr->SetLastError(VE_SOCKET_ERROR, kTraceError,
                                               "SetSendGQoS() Socket error");
            break;
        }
        WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,_channelId),
                     "UdpTransport() => lastError = %d",
                     lastSockError);
        return -1;
    }
    return 0;
}
#endif

#if defined(_WIN32)
WebRtc_Word32
Channel::GetSendGQoS(bool &enabled, int &serviceType, int &overrideDSCP)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::GetSendGQoS(enable=?, serviceType=?, "
                 "overrideDSCP=?)");

    bool QoS(false);
    WebRtc_Word32 serviceTypeModule(0);
    WebRtc_Word32 overrideDSCPModule(0);
    _socketTransportModule.QoS(QoS, serviceTypeModule, overrideDSCPModule);

    enabled = QoS;
    serviceType = static_cast<int> (serviceTypeModule);
    overrideDSCP = static_cast<int> (overrideDSCPModule);

    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "GetSendGQoS() => enabled=%d, serviceType=%d, overrideDSCP=%d",
                 (int)enabled, serviceType, overrideDSCP);
    return 0;
}
#endif
#endif

WebRtc_Word32
Channel::SetPacketTimeoutNotification(bool enable, int timeoutSeconds)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetPacketTimeoutNotification()");
    if (enable)
    {
        const WebRtc_UWord32 RTPtimeoutMS = 1000*timeoutSeconds;
        const WebRtc_UWord32 RTCPtimeoutMS = 0;
        _rtpRtcpModule->SetPacketTimeout(RTPtimeoutMS, RTCPtimeoutMS);
        _rtpPacketTimeOutIsEnabled = true;
        _rtpTimeOutSeconds = timeoutSeconds;
    }
    else
    {
        _rtpRtcpModule->SetPacketTimeout(0, 0);
        _rtpPacketTimeOutIsEnabled = false;
        _rtpTimeOutSeconds = 0;
    }
    return 0;
}

WebRtc_Word32
Channel::GetPacketTimeoutNotification(bool& enabled, int& timeoutSeconds)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::GetPacketTimeoutNotification()");
    enabled = _rtpPacketTimeOutIsEnabled;
    if (enabled)
    {
        timeoutSeconds = _rtpTimeOutSeconds;
    }
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId,-1),
                 "GetPacketTimeoutNotification() => enabled=%d,"
                 " timeoutSeconds=%d",
                 enabled, timeoutSeconds);
    return 0;
}

WebRtc_Word32
Channel::RegisterDeadOrAliveObserver(VoEConnectionObserver& observer)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::RegisterDeadOrAliveObserver()");
    CriticalSectionScoped cs(&_callbackCritSect);

    if (_connectionObserverPtr)
    {
        _engineStatisticsPtr->SetLastError(VE_INVALID_OPERATION, kTraceError,
            "RegisterDeadOrAliveObserver() observer already enabled");
        return -1;
    }

    _connectionObserverPtr = &observer;
    _connectionObserver = true;

    return 0;
}

WebRtc_Word32
Channel::DeRegisterDeadOrAliveObserver()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::DeRegisterDeadOrAliveObserver()");
    CriticalSectionScoped cs(&_callbackCritSect);

    if (!_connectionObserverPtr)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_OPERATION, kTraceWarning,
            "DeRegisterDeadOrAliveObserver() observer already disabled");
        return 0;
    }

    _connectionObserver = false;
    _connectionObserverPtr = NULL;

    return 0;
}

WebRtc_Word32
Channel::SetPeriodicDeadOrAliveStatus(bool enable, int sampleTimeSeconds)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetPeriodicDeadOrAliveStatus()");
    if (!_connectionObserverPtr)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId,_channelId),
                     "SetPeriodicDeadOrAliveStatus() connection observer has"
                     " not been registered");
    }
    if (enable)
    {
        ResetDeadOrAliveCounters();
    }
    bool enabled(false);
    WebRtc_UWord8 currentSampleTimeSec(0);
    // Store last state (will be used later if dead-or-alive is disabled).
    _rtpRtcpModule->PeriodicDeadOrAliveStatus(enabled, currentSampleTimeSec);
    // Update the dead-or-alive state.
    if (_rtpRtcpModule->SetPeriodicDeadOrAliveStatus(
        enable, (WebRtc_UWord8)sampleTimeSeconds) != 0)
    {
        _engineStatisticsPtr->SetLastError(
                VE_RTP_RTCP_MODULE_ERROR,
                kTraceError,
                "SetPeriodicDeadOrAliveStatus() failed to set dead-or-alive "
                "status");
        return -1;
    }
    if (!enable)
    {
        // Restore last utilized sample time.
        // Without this, the sample time would always be reset to default
        // (2 sec), each time dead-or-alived was disabled without sample-time
        // parameter.
        _rtpRtcpModule->SetPeriodicDeadOrAliveStatus(enable,
                                                    currentSampleTimeSec);
    }
    return 0;
}

WebRtc_Word32
Channel::GetPeriodicDeadOrAliveStatus(bool& enabled, int& sampleTimeSeconds)
{
    _rtpRtcpModule->PeriodicDeadOrAliveStatus(
        enabled,
        (WebRtc_UWord8&)sampleTimeSeconds);
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId,-1),
                 "GetPeriodicDeadOrAliveStatus() => enabled=%d,"
                 " sampleTimeSeconds=%d",
                 enabled, sampleTimeSeconds);
    return 0;
}

WebRtc_Word32
Channel::SendUDPPacket(const void* data,
                       unsigned int length,
                       int& transmittedBytes,
                       bool useRtcpSocket)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SendUDPPacket()");
    if (_externalTransport)
    {
        _engineStatisticsPtr->SetLastError(
            VE_EXTERNAL_TRANSPORT_ENABLED, kTraceError,
            "SendUDPPacket() external transport is enabled");
        return -1;
    }
    if (useRtcpSocket && !_rtpRtcpModule->RTCP())
    {
        _engineStatisticsPtr->SetLastError(
            VE_RTCP_ERROR, kTraceError,
            "SendUDPPacket() RTCP is disabled");
        return -1;
    }
    if (!_sending)
    {
        _engineStatisticsPtr->SetLastError(
            VE_NOT_SENDING, kTraceError,
            "SendUDPPacket() not sending");
        return -1;
    }

    char* dataC = new char[length];
    if (NULL == dataC)
    {
        _engineStatisticsPtr->SetLastError(
            VE_NO_MEMORY, kTraceError,
            "SendUDPPacket() memory allocation failed");
        return -1;
    }
    memcpy(dataC, data, length);

    transmittedBytes = SendPacketRaw(dataC, length, useRtcpSocket);

    delete [] dataC;
    dataC = NULL;

    if (transmittedBytes <= 0)
    {
        _engineStatisticsPtr->SetLastError(
                VE_SEND_ERROR, kTraceError,
                "SendUDPPacket() transmission failed");
        transmittedBytes = 0;
        return -1;
    }
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "SendUDPPacket() => transmittedBytes=%d", transmittedBytes);
    return 0;
}


int Channel::StartPlayingFileLocally(const char* fileName,
                                     const bool loop,
                                     const FileFormats format,
                                     const int startPosition,
                                     const float volumeScaling,
                                     const int stopPosition,
                                     const CodecInst* codecInst)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::StartPlayingFileLocally(fileNameUTF8[]=%s, loop=%d,"
                 " format=%d, volumeScaling=%5.3f, startPosition=%d, "
                 "stopPosition=%d)", fileName, loop, format, volumeScaling,
                 startPosition, stopPosition);

    if (_outputFilePlaying)
    {
        _engineStatisticsPtr->SetLastError(
            VE_ALREADY_PLAYING, kTraceError,
            "StartPlayingFileLocally() is already playing");
        return -1;
    }

    {
        CriticalSectionScoped cs(&_fileCritSect);

        if (_outputFilePlayerPtr)
        {
            _outputFilePlayerPtr->RegisterModuleFileCallback(NULL);
            FilePlayer::DestroyFilePlayer(_outputFilePlayerPtr);
            _outputFilePlayerPtr = NULL;
        }

        _outputFilePlayerPtr = FilePlayer::CreateFilePlayer(
            _outputFilePlayerId, (const FileFormats)format);

        if (_outputFilePlayerPtr == NULL)
        {
            _engineStatisticsPtr->SetLastError(
                VE_INVALID_ARGUMENT, kTraceError,
                "StartPlayingFileLocally() filePlayer format is not correct");
            return -1;
        }

        const WebRtc_UWord32 notificationTime(0);

        if (_outputFilePlayerPtr->StartPlayingFile(
                fileName,
                loop,
                startPosition,
                volumeScaling,
                notificationTime,
                stopPosition,
                (const CodecInst*)codecInst) != 0)
        {
            _engineStatisticsPtr->SetLastError(
                VE_BAD_FILE, kTraceError,
                "StartPlayingFile() failed to start file playout");
            _outputFilePlayerPtr->StopPlayingFile();
            FilePlayer::DestroyFilePlayer(_outputFilePlayerPtr);
            _outputFilePlayerPtr = NULL;
            return -1;
        }
        _outputFilePlayerPtr->RegisterModuleFileCallback(this);
        _outputFilePlaying = true;
    }

    if (RegisterFilePlayingToMixer() != 0)
        return -1;

    return 0;
}

int Channel::StartPlayingFileLocally(InStream* stream,
                                     const FileFormats format,
                                     const int startPosition,
                                     const float volumeScaling,
                                     const int stopPosition,
                                     const CodecInst* codecInst)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::StartPlayingFileLocally(format=%d,"
                 " volumeScaling=%5.3f, startPosition=%d, stopPosition=%d)",
                 format, volumeScaling, startPosition, stopPosition);

    if(stream == NULL)
    {
        _engineStatisticsPtr->SetLastError(
            VE_BAD_FILE, kTraceError,
            "StartPlayingFileLocally() NULL as input stream");
        return -1;
    }


    if (_outputFilePlaying)
    {
        _engineStatisticsPtr->SetLastError(
            VE_ALREADY_PLAYING, kTraceError,
            "StartPlayingFileLocally() is already playing");
        return -1;
    }

    {
      CriticalSectionScoped cs(&_fileCritSect);

      // Destroy the old instance
      if (_outputFilePlayerPtr)
      {
          _outputFilePlayerPtr->RegisterModuleFileCallback(NULL);
          FilePlayer::DestroyFilePlayer(_outputFilePlayerPtr);
          _outputFilePlayerPtr = NULL;
      }

      // Create the instance
      _outputFilePlayerPtr = FilePlayer::CreateFilePlayer(
          _outputFilePlayerId,
          (const FileFormats)format);

      if (_outputFilePlayerPtr == NULL)
      {
          _engineStatisticsPtr->SetLastError(
              VE_INVALID_ARGUMENT, kTraceError,
              "StartPlayingFileLocally() filePlayer format isnot correct");
          return -1;
      }

      const WebRtc_UWord32 notificationTime(0);

      if (_outputFilePlayerPtr->StartPlayingFile(*stream, startPosition,
                                                 volumeScaling,
                                                 notificationTime,
                                                 stopPosition, codecInst) != 0)
      {
          _engineStatisticsPtr->SetLastError(VE_BAD_FILE, kTraceError,
                                             "StartPlayingFile() failed to "
                                             "start file playout");
          _outputFilePlayerPtr->StopPlayingFile();
          FilePlayer::DestroyFilePlayer(_outputFilePlayerPtr);
          _outputFilePlayerPtr = NULL;
          return -1;
      }
      _outputFilePlayerPtr->RegisterModuleFileCallback(this);
      _outputFilePlaying = true;
    }

    if (RegisterFilePlayingToMixer() != 0)
        return -1;

    return 0;
}

int Channel::StopPlayingFileLocally()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::StopPlayingFileLocally()");

    if (!_outputFilePlaying)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_OPERATION, kTraceWarning,
            "StopPlayingFileLocally() isnot playing");
        return 0;
    }

    {
        CriticalSectionScoped cs(&_fileCritSect);

        if (_outputFilePlayerPtr->StopPlayingFile() != 0)
        {
            _engineStatisticsPtr->SetLastError(
                VE_STOP_RECORDING_FAILED, kTraceError,
                "StopPlayingFile() could not stop playing");
            return -1;
        }
        _outputFilePlayerPtr->RegisterModuleFileCallback(NULL);
        FilePlayer::DestroyFilePlayer(_outputFilePlayerPtr);
        _outputFilePlayerPtr = NULL;
        _outputFilePlaying = false;
    }
    // _fileCritSect cannot be taken while calling
    // SetAnonymousMixibilityStatus. Refer to comments in
    // StartPlayingFileLocally(const char* ...) for more details.
    if (_outputMixerPtr->SetAnonymousMixabilityStatus(*this, false) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_AUDIO_CONF_MIX_MODULE_ERROR, kTraceError,
            "StopPlayingFile() failed to stop participant from playing as"
            "file in the mixer");
        return -1;
    }

    return 0;
}

int Channel::IsPlayingFileLocally() const
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::IsPlayingFileLocally()");

    return (WebRtc_Word32)_outputFilePlaying;
}

int Channel::RegisterFilePlayingToMixer()
{
    // Return success for not registering for file playing to mixer if:
    // 1. playing file before playout is started on that channel.
    // 2. starting playout without file playing on that channel.
    if (!_playing || !_outputFilePlaying)
    {
        return 0;
    }

    // |_fileCritSect| cannot be taken while calling
    // SetAnonymousMixabilityStatus() since as soon as the participant is added
    // frames can be pulled by the mixer. Since the frames are generated from
    // the file, _fileCritSect will be taken. This would result in a deadlock.
    if (_outputMixerPtr->SetAnonymousMixabilityStatus(*this, true) != 0)
    {
        CriticalSectionScoped cs(&_fileCritSect);
        _outputFilePlaying = false;
        _engineStatisticsPtr->SetLastError(
            VE_AUDIO_CONF_MIX_MODULE_ERROR, kTraceError,
            "StartPlayingFile() failed to add participant as file to mixer");
        _outputFilePlayerPtr->StopPlayingFile();
        FilePlayer::DestroyFilePlayer(_outputFilePlayerPtr);
        _outputFilePlayerPtr = NULL;
        return -1;
    }

    return 0;
}

int Channel::ScaleLocalFilePlayout(const float scale)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::ScaleLocalFilePlayout(scale=%5.3f)", scale);

    CriticalSectionScoped cs(&_fileCritSect);

    if (!_outputFilePlaying)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_OPERATION, kTraceError,
            "ScaleLocalFilePlayout() isnot playing");
        return -1;
    }
    if ((_outputFilePlayerPtr == NULL) ||
        (_outputFilePlayerPtr->SetAudioScaling(scale) != 0))
    {
        _engineStatisticsPtr->SetLastError(
            VE_BAD_ARGUMENT, kTraceError,
            "SetAudioScaling() failed to scale the playout");
        return -1;
    }

    return 0;
}

int Channel::GetLocalPlayoutPosition(int& positionMs)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::GetLocalPlayoutPosition(position=?)");

    WebRtc_UWord32 position;

    CriticalSectionScoped cs(&_fileCritSect);

    if (_outputFilePlayerPtr == NULL)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_OPERATION, kTraceError,
            "GetLocalPlayoutPosition() filePlayer instance doesnot exist");
        return -1;
    }

    if (_outputFilePlayerPtr->GetPlayoutPosition(position) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_BAD_FILE, kTraceError,
            "GetLocalPlayoutPosition() failed");
        return -1;
    }
    positionMs = position;

    return 0;
}

int Channel::StartPlayingFileAsMicrophone(const char* fileName,
                                          const bool loop,
                                          const FileFormats format,
                                          const int startPosition,
                                          const float volumeScaling,
                                          const int stopPosition,
                                          const CodecInst* codecInst)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::StartPlayingFileAsMicrophone(fileNameUTF8[]=%s, "
                 "loop=%d, format=%d, volumeScaling=%5.3f, startPosition=%d, "
                 "stopPosition=%d)", fileName, loop, format, volumeScaling,
                 startPosition, stopPosition);

    if (_inputFilePlaying)
    {
        _engineStatisticsPtr->SetLastError(
            VE_ALREADY_PLAYING, kTraceWarning,
            "StartPlayingFileAsMicrophone() filePlayer is playing");
        return 0;
    }

    CriticalSectionScoped cs(&_fileCritSect);

    // Destroy the old instance
    if (_inputFilePlayerPtr)
    {
        _inputFilePlayerPtr->RegisterModuleFileCallback(NULL);
        FilePlayer::DestroyFilePlayer(_inputFilePlayerPtr);
        _inputFilePlayerPtr = NULL;
    }

    // Create the instance
    _inputFilePlayerPtr = FilePlayer::CreateFilePlayer(
        _inputFilePlayerId, (const FileFormats)format);

    if (_inputFilePlayerPtr == NULL)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "StartPlayingFileAsMicrophone() filePlayer format isnot correct");
        return -1;
    }

    const WebRtc_UWord32 notificationTime(0);

    if (_inputFilePlayerPtr->StartPlayingFile(
        fileName,
        loop,
        startPosition,
        volumeScaling,
        notificationTime,
        stopPosition,
        (const CodecInst*)codecInst) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_BAD_FILE, kTraceError,
            "StartPlayingFile() failed to start file playout");
        _inputFilePlayerPtr->StopPlayingFile();
        FilePlayer::DestroyFilePlayer(_inputFilePlayerPtr);
        _inputFilePlayerPtr = NULL;
        return -1;
    }
    _inputFilePlayerPtr->RegisterModuleFileCallback(this);
    _inputFilePlaying = true;

    return 0;
}

int Channel::StartPlayingFileAsMicrophone(InStream* stream,
                                          const FileFormats format,
                                          const int startPosition,
                                          const float volumeScaling,
                                          const int stopPosition,
                                          const CodecInst* codecInst)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::StartPlayingFileAsMicrophone(format=%d, "
                 "volumeScaling=%5.3f, startPosition=%d, stopPosition=%d)",
                 format, volumeScaling, startPosition, stopPosition);

    if(stream == NULL)
    {
        _engineStatisticsPtr->SetLastError(
            VE_BAD_FILE, kTraceError,
            "StartPlayingFileAsMicrophone NULL as input stream");
        return -1;
    }

    if (_inputFilePlaying)
    {
        _engineStatisticsPtr->SetLastError(
            VE_ALREADY_PLAYING, kTraceWarning,
            "StartPlayingFileAsMicrophone() is playing");
        return 0;
    }

    CriticalSectionScoped cs(&_fileCritSect);

    // Destroy the old instance
    if (_inputFilePlayerPtr)
    {
        _inputFilePlayerPtr->RegisterModuleFileCallback(NULL);
        FilePlayer::DestroyFilePlayer(_inputFilePlayerPtr);
        _inputFilePlayerPtr = NULL;
    }

    // Create the instance
    _inputFilePlayerPtr = FilePlayer::CreateFilePlayer(
        _inputFilePlayerId, (const FileFormats)format);

    if (_inputFilePlayerPtr == NULL)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "StartPlayingInputFile() filePlayer format isnot correct");
        return -1;
    }

    const WebRtc_UWord32 notificationTime(0);

    if (_inputFilePlayerPtr->StartPlayingFile(*stream, startPosition,
                                              volumeScaling, notificationTime,
                                              stopPosition, codecInst) != 0)
    {
        _engineStatisticsPtr->SetLastError(VE_BAD_FILE, kTraceError,
                                           "StartPlayingFile() failed to start "
                                           "file playout");
        _inputFilePlayerPtr->StopPlayingFile();
        FilePlayer::DestroyFilePlayer(_inputFilePlayerPtr);
        _inputFilePlayerPtr = NULL;
        return -1;
    }
    
    _inputFilePlayerPtr->RegisterModuleFileCallback(this);
    _inputFilePlaying = true;

    return 0;
}

int Channel::StopPlayingFileAsMicrophone()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::StopPlayingFileAsMicrophone()");

    if (!_inputFilePlaying)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_OPERATION, kTraceWarning,
            "StopPlayingFileAsMicrophone() isnot playing");
        return 0;
    }

    CriticalSectionScoped cs(&_fileCritSect);
    if (_inputFilePlayerPtr->StopPlayingFile() != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_STOP_RECORDING_FAILED, kTraceError,
            "StopPlayingFile() could not stop playing");
        return -1;
    }
    _inputFilePlayerPtr->RegisterModuleFileCallback(NULL);
    FilePlayer::DestroyFilePlayer(_inputFilePlayerPtr);
    _inputFilePlayerPtr = NULL;
    _inputFilePlaying = false;

    return 0;
}

int Channel::IsPlayingFileAsMicrophone() const
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::IsPlayingFileAsMicrophone()");

    return _inputFilePlaying;
}

int Channel::ScaleFileAsMicrophonePlayout(const float scale)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::ScaleFileAsMicrophonePlayout(scale=%5.3f)", scale);

    CriticalSectionScoped cs(&_fileCritSect);

    if (!_inputFilePlaying)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_OPERATION, kTraceError,
            "ScaleFileAsMicrophonePlayout() isnot playing");
        return -1;
    }

    if ((_inputFilePlayerPtr == NULL) ||
        (_inputFilePlayerPtr->SetAudioScaling(scale) != 0))
    {
        _engineStatisticsPtr->SetLastError(
            VE_BAD_ARGUMENT, kTraceError,
            "SetAudioScaling() failed to scale playout");
        return -1;
    }

    return 0;
}

int Channel::StartRecordingPlayout(const char* fileName,
                                   const CodecInst* codecInst)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::StartRecordingPlayout(fileName=%s)", fileName);

    if (_outputFileRecording)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId,-1),
                     "StartRecordingPlayout() is already recording");
        return 0;
    }

    FileFormats format;
    const WebRtc_UWord32 notificationTime(0); // Not supported in VoE
    CodecInst dummyCodec={100,"L16",16000,320,1,320000};

    if ((codecInst != NULL) &&
      ((codecInst->channels < 1) || (codecInst->channels > 2)))
    {
        _engineStatisticsPtr->SetLastError(
            VE_BAD_ARGUMENT, kTraceError,
            "StartRecordingPlayout() invalid compression");
        return(-1);
    }
    if(codecInst == NULL)
    {
        format = kFileFormatPcm16kHzFile;
        codecInst=&dummyCodec;
    }
    else if((STR_CASE_CMP(codecInst->plname,"L16") == 0) ||
        (STR_CASE_CMP(codecInst->plname,"PCMU") == 0) ||
        (STR_CASE_CMP(codecInst->plname,"PCMA") == 0))
    {
        format = kFileFormatWavFile;
    }
    else
    {
        format = kFileFormatCompressedFile;
    }

    CriticalSectionScoped cs(&_fileCritSect);

    // Destroy the old instance
    if (_outputFileRecorderPtr)
    {
        _outputFileRecorderPtr->RegisterModuleFileCallback(NULL);
        FileRecorder::DestroyFileRecorder(_outputFileRecorderPtr);
        _outputFileRecorderPtr = NULL;
    }

    _outputFileRecorderPtr = FileRecorder::CreateFileRecorder(
        _outputFileRecorderId, (const FileFormats)format);
    if (_outputFileRecorderPtr == NULL)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "StartRecordingPlayout() fileRecorder format isnot correct");
        return -1;
    }

    if (_outputFileRecorderPtr->StartRecordingAudioFile(
        fileName, (const CodecInst&)*codecInst, notificationTime) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_BAD_FILE, kTraceError,
            "StartRecordingAudioFile() failed to start file recording");
        _outputFileRecorderPtr->StopRecording();
        FileRecorder::DestroyFileRecorder(_outputFileRecorderPtr);
        _outputFileRecorderPtr = NULL;
        return -1;
    }
    _outputFileRecorderPtr->RegisterModuleFileCallback(this);
    _outputFileRecording = true;

    return 0;
}

int Channel::StartRecordingPlayout(OutStream* stream,
                                   const CodecInst* codecInst)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::StartRecordingPlayout()");

    if (_outputFileRecording)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId,-1),
                     "StartRecordingPlayout() is already recording");
        return 0;
    }

    FileFormats format;
    const WebRtc_UWord32 notificationTime(0); // Not supported in VoE
    CodecInst dummyCodec={100,"L16",16000,320,1,320000};

    if (codecInst != NULL && codecInst->channels != 1)
    {
        _engineStatisticsPtr->SetLastError(
            VE_BAD_ARGUMENT, kTraceError,
            "StartRecordingPlayout() invalid compression");
        return(-1);
    }
    if(codecInst == NULL)
    {
        format = kFileFormatPcm16kHzFile;
        codecInst=&dummyCodec;
    }
    else if((STR_CASE_CMP(codecInst->plname,"L16") == 0) ||
        (STR_CASE_CMP(codecInst->plname,"PCMU") == 0) ||
        (STR_CASE_CMP(codecInst->plname,"PCMA") == 0))
    {
        format = kFileFormatWavFile;
    }
    else
    {
        format = kFileFormatCompressedFile;
    }

    CriticalSectionScoped cs(&_fileCritSect);

    // Destroy the old instance
    if (_outputFileRecorderPtr)
    {
        _outputFileRecorderPtr->RegisterModuleFileCallback(NULL);
        FileRecorder::DestroyFileRecorder(_outputFileRecorderPtr);
        _outputFileRecorderPtr = NULL;
    }

    _outputFileRecorderPtr = FileRecorder::CreateFileRecorder(
        _outputFileRecorderId, (const FileFormats)format);
    if (_outputFileRecorderPtr == NULL)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "StartRecordingPlayout() fileRecorder format isnot correct");
        return -1;
    }

    if (_outputFileRecorderPtr->StartRecordingAudioFile(*stream, *codecInst,
                                                        notificationTime) != 0)
    {
        _engineStatisticsPtr->SetLastError(VE_BAD_FILE, kTraceError,
                                           "StartRecordingPlayout() failed to "
                                           "start file recording");
        _outputFileRecorderPtr->StopRecording();
        FileRecorder::DestroyFileRecorder(_outputFileRecorderPtr);
        _outputFileRecorderPtr = NULL;
        return -1;
    }
    
    _outputFileRecorderPtr->RegisterModuleFileCallback(this);
    _outputFileRecording = true;

    return 0;
}

int Channel::StopRecordingPlayout()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,-1),
                 "Channel::StopRecordingPlayout()");

    if (!_outputFileRecording)
    {
        WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,-1),
                     "StopRecordingPlayout() isnot recording");
        return -1;
    }


    CriticalSectionScoped cs(&_fileCritSect);

    if (_outputFileRecorderPtr->StopRecording() != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_STOP_RECORDING_FAILED, kTraceError,
            "StopRecording() could not stop recording");
        return(-1);
    }
    _outputFileRecorderPtr->RegisterModuleFileCallback(NULL);
    FileRecorder::DestroyFileRecorder(_outputFileRecorderPtr);
    _outputFileRecorderPtr = NULL;
    _outputFileRecording = false;

    return 0;
}

void
Channel::SetMixWithMicStatus(bool mix)
{
    _mixFileWithMicrophone=mix;
}

int
Channel::GetSpeechOutputLevel(WebRtc_UWord32& level) const
{
    WebRtc_Word8 currentLevel = _outputAudioLevel.Level();
    level = static_cast<WebRtc_Word32> (currentLevel);
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
               VoEId(_instanceId,_channelId),
               "GetSpeechOutputLevel() => level=%u", level);
    return 0;
}

int
Channel::GetSpeechOutputLevelFullRange(WebRtc_UWord32& level) const
{
    WebRtc_Word16 currentLevel = _outputAudioLevel.LevelFullRange();
    level = static_cast<WebRtc_Word32> (currentLevel);
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
               VoEId(_instanceId,_channelId),
               "GetSpeechOutputLevelFullRange() => level=%u", level);
    return 0;
}

int
Channel::SetMute(bool enable)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
               "Channel::SetMute(enable=%d)", enable);
    _mute = enable;
    return 0;
}

bool
Channel::Mute() const
{
    return _mute;
}

int
Channel::SetOutputVolumePan(float left, float right)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
               "Channel::SetOutputVolumePan()");
    _panLeft = left;
    _panRight = right;
    return 0;
}

int
Channel::GetOutputVolumePan(float& left, float& right) const
{
    left = _panLeft;
    right = _panRight;
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
               VoEId(_instanceId,_channelId),
               "GetOutputVolumePan() => left=%3.2f, right=%3.2f", left, right);
    return 0;
}

int
Channel::SetChannelOutputVolumeScaling(float scaling)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
               "Channel::SetChannelOutputVolumeScaling()");
    _outputGain = scaling;
    return 0;
}

int
Channel::GetChannelOutputVolumeScaling(float& scaling) const
{
    scaling = _outputGain;
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
               VoEId(_instanceId,_channelId),
               "GetChannelOutputVolumeScaling() => scaling=%3.2f", scaling);
    return 0;
}

#ifdef WEBRTC_SRTP

int
Channel::EnableSRTPSend(
    CipherTypes cipherType,
    int cipherKeyLength,
    AuthenticationTypes authType,
    int authKeyLength,
    int authTagLength,
    SecurityLevels level,
    const unsigned char key[kVoiceEngineMaxSrtpKeyLength],
    bool useForRTCP)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
               "Channel::EnableSRTPSend()");

    CriticalSectionScoped cs(&_callbackCritSect);

    if (_encrypting)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_OPERATION, kTraceWarning,
            "EnableSRTPSend() encryption already enabled");
        return -1;
    }

    if (key == NULL)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceWarning,
            "EnableSRTPSend() invalid key string");
        return -1;
    }

    if (((kEncryption == level ||
            kEncryptionAndAuthentication == level) &&
            (cipherKeyLength < kVoiceEngineMinSrtpEncryptLength ||
            cipherKeyLength > kVoiceEngineMaxSrtpEncryptLength)) ||
        ((kAuthentication == level ||
            kEncryptionAndAuthentication == level) &&
            kAuthHmacSha1 == authType &&
            (authKeyLength > kVoiceEngineMaxSrtpAuthSha1Length ||
            authTagLength > kVoiceEngineMaxSrtpAuthSha1Length)) ||
        ((kAuthentication == level ||
            kEncryptionAndAuthentication == level) &&
            kAuthNull == authType &&
            (authKeyLength > kVoiceEngineMaxSrtpKeyAuthNullLength ||
            authTagLength > kVoiceEngineMaxSrtpTagAuthNullLength)))
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "EnableSRTPSend() invalid key length(s)");
        return -1;
    }


    if (_srtpModule.EnableSRTPEncrypt(
        !useForRTCP,
        (SrtpModule::CipherTypes)cipherType,
        cipherKeyLength,
        (SrtpModule::AuthenticationTypes)authType,
        authKeyLength, authTagLength,
        (SrtpModule::SecurityLevels)level,
        key) == -1)
    {
        _engineStatisticsPtr->SetLastError(
            VE_SRTP_ERROR, kTraceError,
            "EnableSRTPSend() failed to enable SRTP encryption");
        return -1;
    }

    if (_encryptionPtr == NULL)
    {
        _encryptionPtr = &_srtpModule;
    }
    _encrypting = true;

    return 0;
}

int
Channel::DisableSRTPSend()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
               "Channel::DisableSRTPSend()");

    CriticalSectionScoped cs(&_callbackCritSect);

    if (!_encrypting)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_OPERATION, kTraceWarning,
            "DisableSRTPSend() SRTP encryption already disabled");
        return 0;
    }

    _encrypting = false;

    if (_srtpModule.DisableSRTPEncrypt() == -1)
    {
        _engineStatisticsPtr->SetLastError(
            VE_SRTP_ERROR, kTraceError,
            "DisableSRTPSend() failed to disable SRTP encryption");
        return -1;
    }

    if (!_srtpModule.SRTPDecrypt() && !_srtpModule.SRTPEncrypt())
    {
        // Both directions are disabled
        _encryptionPtr = NULL;
    }

    return 0;
}

int
Channel::EnableSRTPReceive(
    CipherTypes  cipherType,
    int cipherKeyLength,
    AuthenticationTypes authType,
    int authKeyLength,
    int authTagLength,
    SecurityLevels level,
    const unsigned char key[kVoiceEngineMaxSrtpKeyLength],
    bool useForRTCP)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
               "Channel::EnableSRTPReceive()");

    CriticalSectionScoped cs(&_callbackCritSect);

    if (_decrypting)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_OPERATION, kTraceWarning,
            "EnableSRTPReceive() SRTP decryption already enabled");
        return -1;
    }

    if (key == NULL)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceWarning,
            "EnableSRTPReceive() invalid key string");
        return -1;
    }

    if ((((kEncryption == level) ||
            (kEncryptionAndAuthentication == level)) &&
            ((cipherKeyLength < kVoiceEngineMinSrtpEncryptLength) ||
            (cipherKeyLength > kVoiceEngineMaxSrtpEncryptLength))) ||
        (((kAuthentication == level) ||
            (kEncryptionAndAuthentication == level)) &&
            (kAuthHmacSha1 == authType) &&
            ((authKeyLength > kVoiceEngineMaxSrtpAuthSha1Length) ||
            (authTagLength > kVoiceEngineMaxSrtpAuthSha1Length))) ||
        (((kAuthentication == level) ||
            (kEncryptionAndAuthentication == level)) &&
            (kAuthNull == authType) &&
            ((authKeyLength > kVoiceEngineMaxSrtpKeyAuthNullLength) ||
            (authTagLength > kVoiceEngineMaxSrtpTagAuthNullLength))))
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "EnableSRTPReceive() invalid key length(s)");
        return -1;
    }

    if (_srtpModule.EnableSRTPDecrypt(
        !useForRTCP,
        (SrtpModule::CipherTypes)cipherType,
        cipherKeyLength,
        (SrtpModule::AuthenticationTypes)authType,
        authKeyLength,
        authTagLength,
        (SrtpModule::SecurityLevels)level,
        key) == -1)
    {
        _engineStatisticsPtr->SetLastError(
            VE_SRTP_ERROR, kTraceError,
            "EnableSRTPReceive() failed to enable SRTP decryption");
        return -1;
    }

    if (_encryptionPtr == NULL)
    {
        _encryptionPtr = &_srtpModule;
    }

    _decrypting = true;

    return 0;
}

int
Channel::DisableSRTPReceive()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
               "Channel::DisableSRTPReceive()");

    CriticalSectionScoped cs(&_callbackCritSect);

    if (!_decrypting)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_OPERATION, kTraceWarning,
            "DisableSRTPReceive() SRTP decryption already disabled");
        return 0;
    }

    _decrypting = false;

    if (_srtpModule.DisableSRTPDecrypt() == -1)
    {
        _engineStatisticsPtr->SetLastError(
            VE_SRTP_ERROR, kTraceError,
            "DisableSRTPReceive() failed to disable SRTP decryption");
        return -1;
    }

    if (!_srtpModule.SRTPDecrypt() && !_srtpModule.SRTPEncrypt())
    {
        _encryptionPtr = NULL;
    }

    return 0;
}

#endif

int
Channel::RegisterExternalEncryption(Encryption& encryption)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
               "Channel::RegisterExternalEncryption()");

    CriticalSectionScoped cs(&_callbackCritSect);

    if (_encryptionPtr)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_OPERATION, kTraceError,
            "RegisterExternalEncryption() encryption already enabled");
        return -1;
    }

    _encryptionPtr = &encryption;

    _decrypting = true;
    _encrypting = true;

    return 0;
}

int
Channel::DeRegisterExternalEncryption()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
               "Channel::DeRegisterExternalEncryption()");

    CriticalSectionScoped cs(&_callbackCritSect);

    if (!_encryptionPtr)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_OPERATION, kTraceWarning,
            "DeRegisterExternalEncryption() encryption already disabled");
        return 0;
    }

    _decrypting = false;
    _encrypting = false;

    _encryptionPtr = NULL;

    return 0;
}

int Channel::SendTelephoneEventOutband(unsigned char eventCode,
                                          int lengthMs, int attenuationDb,
                                          bool playDtmfEvent)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::SendTelephoneEventOutband(..., playDtmfEvent=%d)",
               playDtmfEvent);

    _playOutbandDtmfEvent = playDtmfEvent;

    if (_rtpRtcpModule->SendTelephoneEventOutband(eventCode, lengthMs,
                                                 attenuationDb) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_SEND_DTMF_FAILED,
            kTraceWarning,
            "SendTelephoneEventOutband() failed to send event");
        return -1;
    }
    return 0;
}

int Channel::SendTelephoneEventInband(unsigned char eventCode,
                                         int lengthMs,
                                         int attenuationDb,
                                         bool playDtmfEvent)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::SendTelephoneEventInband(..., playDtmfEvent=%d)",
               playDtmfEvent);

    _playInbandDtmfEvent = playDtmfEvent;
    _inbandDtmfQueue.AddDtmf(eventCode, lengthMs, attenuationDb);

    return 0;
}

int
Channel::SetDtmfPlayoutStatus(bool enable)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
               "Channel::SetDtmfPlayoutStatus()");
    if (_audioCodingModule.SetDtmfPlayoutStatus(enable) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_AUDIO_CODING_MODULE_ERROR, kTraceWarning,
            "SetDtmfPlayoutStatus() failed to set Dtmf playout");
        return -1;
    }
    return 0;
}

bool
Channel::DtmfPlayoutStatus() const
{
    return _audioCodingModule.DtmfPlayoutStatus();
}

int
Channel::SetSendTelephoneEventPayloadType(unsigned char type)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
               "Channel::SetSendTelephoneEventPayloadType()");
    if (type > 127)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "SetSendTelephoneEventPayloadType() invalid type");
        return -1;
    }
    CodecInst codec;
    codec.plfreq = 8000;
    codec.pltype = type;
    memcpy(codec.plname, "telephone-event", 16);
    if (_rtpRtcpModule->RegisterSendPayload(codec) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_RTP_RTCP_MODULE_ERROR, kTraceError,
            "SetSendTelephoneEventPayloadType() failed to register send"
            "payload type");
        return -1;
    }
    _sendTelephoneEventPayloadType = type;
    return 0;
}

int
Channel::GetSendTelephoneEventPayloadType(unsigned char& type)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::GetSendTelephoneEventPayloadType()");
    type = _sendTelephoneEventPayloadType;
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
               VoEId(_instanceId,_channelId),
               "GetSendTelephoneEventPayloadType() => type=%u", type);
    return 0;
}

#ifdef WEBRTC_DTMF_DETECTION

WebRtc_Word32
Channel::RegisterTelephoneEventDetection(
    TelephoneEventDetectionMethods detectionMethod,
    VoETelephoneEventObserver& observer)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::RegisterTelephoneEventDetection()");
    CriticalSectionScoped cs(&_callbackCritSect);

    if (_telephoneEventDetectionPtr)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_OPERATION, kTraceError,
            "RegisterTelephoneEventDetection() detection already enabled");
        return -1;
    }

    _telephoneEventDetectionPtr = &observer;

    switch (detectionMethod)
    {
        case kInBand:
            _inbandTelephoneEventDetection = true;
            _outOfBandTelephoneEventDetecion = false;
            break;
        case kOutOfBand:
            _inbandTelephoneEventDetection = false;
            _outOfBandTelephoneEventDetecion = true;
            break;
        case kInAndOutOfBand:
            _inbandTelephoneEventDetection = true;
            _outOfBandTelephoneEventDetecion = true;
            break;
        default:
            _engineStatisticsPtr->SetLastError(
                VE_INVALID_ARGUMENT, kTraceError,
                "RegisterTelephoneEventDetection() invalid detection method");
            return -1;
    }

    if (_inbandTelephoneEventDetection)
    {
        // Enable in-band Dtmf detectin in the ACM.
        if (_audioCodingModule.RegisterIncomingMessagesCallback(this) != 0)
        {
            _engineStatisticsPtr->SetLastError(
                VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
                "RegisterTelephoneEventDetection() failed to enable Dtmf "
                "detection");
        }
    }

    // Enable/disable out-of-band detection of received telephone-events.
    // When enabled, RtpAudioFeedback::OnReceivedTelephoneEvent() will be
    // called two times by the RTP/RTCP module (start & end).
    const bool forwardToDecoder =
        _rtpRtcpModule->TelephoneEventForwardToDecoder();
    const bool detectEndOfTone = true;
    _rtpRtcpModule->SetTelephoneEventStatus(_outOfBandTelephoneEventDetecion,
                                           forwardToDecoder,
                                           detectEndOfTone);

    return 0;
}

int
Channel::DeRegisterTelephoneEventDetection()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
                 "Channel::DeRegisterTelephoneEventDetection()");

    CriticalSectionScoped cs(&_callbackCritSect);

    if (!_telephoneEventDetectionPtr)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_OPERATION,
            kTraceWarning,
            "DeRegisterTelephoneEventDetection() detection already disabled");
        return 0;
    }

    // Disable out-of-band event detection
    const bool forwardToDecoder =
        _rtpRtcpModule->TelephoneEventForwardToDecoder();
    _rtpRtcpModule->SetTelephoneEventStatus(false, forwardToDecoder);

    // Disable in-band Dtmf detection
    _audioCodingModule.RegisterIncomingMessagesCallback(NULL);

    _inbandTelephoneEventDetection = false;
    _outOfBandTelephoneEventDetecion = false;
    _telephoneEventDetectionPtr = NULL;

    return 0;
}

int
Channel::GetTelephoneEventDetectionStatus(
    bool& enabled,
    TelephoneEventDetectionMethods& detectionMethod)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
                 "Channel::GetTelephoneEventDetectionStatus()");

    {
        CriticalSectionScoped cs(&_callbackCritSect);
        enabled = (_telephoneEventDetectionPtr != NULL);
    }

    if (enabled)
    {
        if (_inbandTelephoneEventDetection && !_outOfBandTelephoneEventDetecion)
            detectionMethod = kInBand;
        else if (!_inbandTelephoneEventDetection
            && _outOfBandTelephoneEventDetecion)
            detectionMethod = kOutOfBand;
        else if (_inbandTelephoneEventDetection
            && _outOfBandTelephoneEventDetecion)
            detectionMethod = kInAndOutOfBand;
        else
        {
            assert(false);
            return -1;
        }
    }

    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
               VoEId(_instanceId, _channelId),
               "GetTelephoneEventDetectionStatus() => enabled=%d,"
               "detectionMethod=%d", enabled, detectionMethod);
    return 0;
}

#endif  // #ifdef WEBRTC_DTMF_DETECTION

int
Channel::UpdateRxVadDetection(AudioFrame& audioFrame)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::UpdateRxVadDetection()");

    int vadDecision = 1;

    vadDecision = (audioFrame.vad_activity_ == AudioFrame::kVadActive)? 1 : 0;

    if ((vadDecision != _oldVadDecision) && _rxVadObserverPtr)
    {
        OnRxVadDetected(vadDecision);
        _oldVadDecision = vadDecision;
    }

    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::UpdateRxVadDetection() => vadDecision=%d",
                 vadDecision);
    return 0;
}

int
Channel::RegisterRxVadObserver(VoERxVadCallback &observer)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::RegisterRxVadObserver()");
    CriticalSectionScoped cs(&_callbackCritSect);

    if (_rxVadObserverPtr)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_OPERATION, kTraceError,
            "RegisterRxVadObserver() observer already enabled");
        return -1;
    }
    _rxVadObserverPtr = &observer;
    _RxVadDetection = true;
    return 0;
}

int
Channel::DeRegisterRxVadObserver()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::DeRegisterRxVadObserver()");
    CriticalSectionScoped cs(&_callbackCritSect);

    if (!_rxVadObserverPtr)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_OPERATION, kTraceWarning,
            "DeRegisterRxVadObserver() observer already disabled");
        return 0;
    }
    _rxVadObserverPtr = NULL;
    _RxVadDetection = false;
    return 0;
}

int
Channel::VoiceActivityIndicator(int &activity)
{
    activity = _sendFrameType;

    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
               "Channel::VoiceActivityIndicator(indicator=%d)", activity);
    return 0;
}

#ifdef WEBRTC_VOICE_ENGINE_AGC

int
Channel::SetRxAgcStatus(const bool enable, const AgcModes mode)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetRxAgcStatus(enable=%d, mode=%d)",
                 (int)enable, (int)mode);

    GainControl::Mode agcMode(GainControl::kFixedDigital);
    switch (mode)
    {
        case kAgcDefault:
            agcMode = GainControl::kAdaptiveDigital;
            break;
        case kAgcUnchanged:
            agcMode = _rxAudioProcessingModulePtr->gain_control()->mode();
            break;
        case kAgcFixedDigital:
            agcMode = GainControl::kFixedDigital;
            break;
        case kAgcAdaptiveDigital:
            agcMode =GainControl::kAdaptiveDigital;
            break;
        default:
            _engineStatisticsPtr->SetLastError(
                VE_INVALID_ARGUMENT, kTraceError,
                "SetRxAgcStatus() invalid Agc mode");
            return -1;
    }

    if (_rxAudioProcessingModulePtr->gain_control()->set_mode(agcMode) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_APM_ERROR, kTraceError,
            "SetRxAgcStatus() failed to set Agc mode");
        return -1;
    }
    if (_rxAudioProcessingModulePtr->gain_control()->Enable(enable) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_APM_ERROR, kTraceError,
            "SetRxAgcStatus() failed to set Agc state");
        return -1;
    }

    _rxAgcIsEnabled = enable;
    _rxApmIsEnabled = ((_rxAgcIsEnabled == true) || (_rxNsIsEnabled == true));

    return 0;
}

int
Channel::GetRxAgcStatus(bool& enabled, AgcModes& mode)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                     "Channel::GetRxAgcStatus(enable=?, mode=?)");

    bool enable = _rxAudioProcessingModulePtr->gain_control()->is_enabled();
    GainControl::Mode agcMode =
        _rxAudioProcessingModulePtr->gain_control()->mode();

    enabled = enable;

    switch (agcMode)
    {
        case GainControl::kFixedDigital:
            mode = kAgcFixedDigital;
            break;
        case GainControl::kAdaptiveDigital:
            mode = kAgcAdaptiveDigital;
            break;
        default:
            _engineStatisticsPtr->SetLastError(
                VE_APM_ERROR, kTraceError,
                "GetRxAgcStatus() invalid Agc mode");
            return -1;
    }

    return 0;
}

int
Channel::SetRxAgcConfig(const AgcConfig config)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetRxAgcConfig()");

    if (_rxAudioProcessingModulePtr->gain_control()->set_target_level_dbfs(
        config.targetLeveldBOv) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_APM_ERROR, kTraceError,
            "SetRxAgcConfig() failed to set target peak |level|"
            "(or envelope) of the Agc");
        return -1;
    }
    if (_rxAudioProcessingModulePtr->gain_control()->set_compression_gain_db(
        config.digitalCompressionGaindB) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_APM_ERROR, kTraceError,
            "SetRxAgcConfig() failed to set the range in |gain| the"
            " digital compression stage may apply");
        return -1;
    }
    if (_rxAudioProcessingModulePtr->gain_control()->enable_limiter(
        config.limiterEnable) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_APM_ERROR, kTraceError,
            "SetRxAgcConfig() failed to set hard limiter to the signal");
        return -1;
    }

    return 0;
}

int
Channel::GetRxAgcConfig(AgcConfig& config)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::GetRxAgcConfig(config=%?)");

    config.targetLeveldBOv =
        _rxAudioProcessingModulePtr->gain_control()->target_level_dbfs();
    config.digitalCompressionGaindB =
        _rxAudioProcessingModulePtr->gain_control()->compression_gain_db();
    config.limiterEnable =
        _rxAudioProcessingModulePtr->gain_control()->is_limiter_enabled();

    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
               VoEId(_instanceId,_channelId), "GetRxAgcConfig() => "
                   "targetLeveldBOv=%u, digitalCompressionGaindB=%u,"
                   " limiterEnable=%d",
                   config.targetLeveldBOv,
                   config.digitalCompressionGaindB,
                   config.limiterEnable);

    return 0;
}

#endif // #ifdef WEBRTC_VOICE_ENGINE_AGC

#ifdef WEBRTC_VOICE_ENGINE_NR

int
Channel::SetRxNsStatus(const bool enable, const NsModes mode)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetRxNsStatus(enable=%d, mode=%d)",
                 (int)enable, (int)mode);

    NoiseSuppression::Level nsLevel(
        (NoiseSuppression::Level)WEBRTC_VOICE_ENGINE_RX_NS_DEFAULT_MODE);
    switch (mode)
    {

        case kNsDefault:
            nsLevel = (NoiseSuppression::Level)
                WEBRTC_VOICE_ENGINE_RX_NS_DEFAULT_MODE;
            break;
        case kNsUnchanged:
            nsLevel = _rxAudioProcessingModulePtr->noise_suppression()->level();
            break;
        case kNsConference:
            nsLevel = NoiseSuppression::kHigh;
            break;
        case kNsLowSuppression:
            nsLevel = NoiseSuppression::kLow;
            break;
        case kNsModerateSuppression:
            nsLevel = NoiseSuppression::kModerate;
            break;
        case kNsHighSuppression:
            nsLevel = NoiseSuppression::kHigh;
            break;
        case kNsVeryHighSuppression:
            nsLevel = NoiseSuppression::kVeryHigh;
            break;
    }

    if (_rxAudioProcessingModulePtr->noise_suppression()->set_level(nsLevel)
        != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_APM_ERROR, kTraceError,
            "SetRxAgcStatus() failed to set Ns level");
        return -1;
    }
    if (_rxAudioProcessingModulePtr->noise_suppression()->Enable(enable) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_APM_ERROR, kTraceError,
            "SetRxAgcStatus() failed to set Agc state");
        return -1;
    }

    _rxNsIsEnabled = enable;
    _rxApmIsEnabled = ((_rxAgcIsEnabled == true) || (_rxNsIsEnabled == true));

    return 0;
}

int
Channel::GetRxNsStatus(bool& enabled, NsModes& mode)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::GetRxNsStatus(enable=?, mode=?)");

    bool enable =
        _rxAudioProcessingModulePtr->noise_suppression()->is_enabled();
    NoiseSuppression::Level ncLevel =
        _rxAudioProcessingModulePtr->noise_suppression()->level();

    enabled = enable;

    switch (ncLevel)
    {
        case NoiseSuppression::kLow:
            mode = kNsLowSuppression;
            break;
        case NoiseSuppression::kModerate:
            mode = kNsModerateSuppression;
            break;
        case NoiseSuppression::kHigh:
            mode = kNsHighSuppression;
            break;
        case NoiseSuppression::kVeryHigh:
            mode = kNsVeryHighSuppression;
            break;
    }

    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
               VoEId(_instanceId,_channelId),
               "GetRxNsStatus() => enabled=%d, mode=%d", enabled, mode);
    return 0;
}

#endif // #ifdef WEBRTC_VOICE_ENGINE_NR

int
Channel::RegisterRTPObserver(VoERTPObserver& observer)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
                 "Channel::RegisterRTPObserver()");
    CriticalSectionScoped cs(&_callbackCritSect);

    if (_rtpObserverPtr)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_OPERATION, kTraceError,
            "RegisterRTPObserver() observer already enabled");
        return -1;
    }

    _rtpObserverPtr = &observer;
    _rtpObserver = true;

    return 0;
}

int
Channel::DeRegisterRTPObserver()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::DeRegisterRTPObserver()");
    CriticalSectionScoped cs(&_callbackCritSect);

    if (!_rtpObserverPtr)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_OPERATION, kTraceWarning,
            "DeRegisterRTPObserver() observer already disabled");
        return 0;
    }

    _rtpObserver = false;
    _rtpObserverPtr = NULL;

    return 0;
}

int
Channel::RegisterRTCPObserver(VoERTCPObserver& observer)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::RegisterRTCPObserver()");
    CriticalSectionScoped cs(&_callbackCritSect);

    if (_rtcpObserverPtr)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_OPERATION, kTraceError,
            "RegisterRTCPObserver() observer already enabled");
        return -1;
    }

    _rtcpObserverPtr = &observer;
    _rtcpObserver = true;

    return 0;
}

int
Channel::DeRegisterRTCPObserver()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
                 "Channel::DeRegisterRTCPObserver()");
    CriticalSectionScoped cs(&_callbackCritSect);

    if (!_rtcpObserverPtr)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_OPERATION, kTraceWarning,
            "DeRegisterRTCPObserver() observer already disabled");
        return 0;
    }

    _rtcpObserver = false;
    _rtcpObserverPtr = NULL;

    return 0;
}

int
Channel::SetLocalSSRC(unsigned int ssrc)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
                 "Channel::SetLocalSSRC()");
    if (_sending)
    {
        _engineStatisticsPtr->SetLastError(
            VE_ALREADY_SENDING, kTraceError,
            "SetLocalSSRC() already sending");
        return -1;
    }
    if (_rtpRtcpModule->SetSSRC(ssrc) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_RTP_RTCP_MODULE_ERROR, kTraceError,
            "SetLocalSSRC() failed to set SSRC");
        return -1;
    }
    return 0;
}

int
Channel::GetLocalSSRC(unsigned int& ssrc)
{
    ssrc = _rtpRtcpModule->SSRC();
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
                 VoEId(_instanceId,_channelId),
                 "GetLocalSSRC() => ssrc=%lu", ssrc);
    return 0;
}

int
Channel::GetRemoteSSRC(unsigned int& ssrc)
{
    ssrc = _rtpRtcpModule->RemoteSSRC();
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
                 VoEId(_instanceId,_channelId),
                 "GetRemoteSSRC() => ssrc=%lu", ssrc);
    return 0;
}

int
Channel::GetRemoteCSRCs(unsigned int arrCSRC[15])
{
    if (arrCSRC == NULL)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "GetRemoteCSRCs() invalid array argument");
        return -1;
    }
    WebRtc_UWord32 arrOfCSRC[kRtpCsrcSize];
    WebRtc_Word32 CSRCs(0);
    CSRCs = _rtpRtcpModule->CSRCs(arrOfCSRC);
    if (CSRCs > 0)
    {
        memcpy(arrCSRC, arrOfCSRC, CSRCs * sizeof(WebRtc_UWord32));
        for (int i = 0; i < (int) CSRCs; i++)
        {
            WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
                       VoEId(_instanceId, _channelId),
                       "GetRemoteCSRCs() => arrCSRC[%d]=%lu", i, arrCSRC[i]);
        }
    } else
    {
        WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
                   VoEId(_instanceId, _channelId),
                   "GetRemoteCSRCs() => list is empty!");
    }
    return CSRCs;
}

int
Channel::SetRTPAudioLevelIndicationStatus(bool enable, unsigned char ID)
{
    if (_rtpAudioProc.get() == NULL)
    {
        _rtpAudioProc.reset(AudioProcessing::Create(VoEModuleId(_instanceId,
                                                                _channelId)));
        if (_rtpAudioProc.get() == NULL)
        {
            _engineStatisticsPtr->SetLastError(VE_NO_MEMORY, kTraceCritical,
                "Failed to create AudioProcessing");
            return -1;
        }
    }

    if (_rtpAudioProc->level_estimator()->Enable(enable) !=
        AudioProcessing::kNoError)
    {
        _engineStatisticsPtr->SetLastError(VE_APM_ERROR, kTraceWarning,
            "Failed to enable AudioProcessing::level_estimator()");
    }

    _includeAudioLevelIndication = enable;
    return _rtpRtcpModule->SetRTPAudioLevelIndicationStatus(enable, ID);
}
int
Channel::GetRTPAudioLevelIndicationStatus(bool& enabled, unsigned char& ID)
{
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
                 VoEId(_instanceId,_channelId),
                 "GetRTPAudioLevelIndicationStatus() => enabled=%d, ID=%u",
                 enabled, ID);
    return _rtpRtcpModule->GetRTPAudioLevelIndicationStatus(enabled, ID);
}

int
Channel::SetRTCPStatus(bool enable)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetRTCPStatus()");
    if (_rtpRtcpModule->SetRTCPStatus(enable ?
        kRtcpCompound : kRtcpOff) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_RTP_RTCP_MODULE_ERROR, kTraceError,
            "SetRTCPStatus() failed to set RTCP status");
        return -1;
    }
    return 0;
}

int
Channel::GetRTCPStatus(bool& enabled)
{
    RTCPMethod method = _rtpRtcpModule->RTCP();
    enabled = (method != kRtcpOff);
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
                 VoEId(_instanceId,_channelId),
                 "GetRTCPStatus() => enabled=%d", enabled);
    return 0;
}

int
Channel::SetRTCP_CNAME(const char cName[256])
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
                 "Channel::SetRTCP_CNAME()");
    if (_rtpRtcpModule->SetCNAME(cName) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_RTP_RTCP_MODULE_ERROR, kTraceError,
            "SetRTCP_CNAME() failed to set RTCP CNAME");
        return -1;
    }
    return 0;
}

int
Channel::GetRTCP_CNAME(char cName[256])
{
    if (_rtpRtcpModule->CNAME(cName) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_RTP_RTCP_MODULE_ERROR, kTraceError,
            "GetRTCP_CNAME() failed to retrieve RTCP CNAME");
        return -1;
    }
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
                 VoEId(_instanceId, _channelId),
                 "GetRTCP_CNAME() => cName=%s", cName);
    return 0;
}

int
Channel::GetRemoteRTCP_CNAME(char cName[256])
{
    if (cName == NULL)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "GetRemoteRTCP_CNAME() invalid CNAME input buffer");
        return -1;
    }
    char cname[RTCP_CNAME_SIZE];
    const WebRtc_UWord32 remoteSSRC = _rtpRtcpModule->RemoteSSRC();
    if (_rtpRtcpModule->RemoteCNAME(remoteSSRC, cname) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_CANNOT_RETRIEVE_CNAME, kTraceError,
            "GetRemoteRTCP_CNAME() failed to retrieve remote RTCP CNAME");
        return -1;
    }
    strcpy(cName, cname);
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
                 VoEId(_instanceId, _channelId),
                 "GetRemoteRTCP_CNAME() => cName=%s", cName);
    return 0;
}

int
Channel::GetRemoteRTCPData(
    unsigned int& NTPHigh,
    unsigned int& NTPLow,
    unsigned int& timestamp,
    unsigned int& playoutTimestamp,
    unsigned int* jitter,
    unsigned short* fractionLost)
{
    // --- Information from sender info in received Sender Reports

    RTCPSenderInfo senderInfo;
    if (_rtpRtcpModule->RemoteRTCPStat(&senderInfo) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_RTP_RTCP_MODULE_ERROR, kTraceError,
            "GetRemoteRTCPData() failed to retrieve sender info for remote "
            "side");
        return -1;
    }

    // We only utilize 12 out of 20 bytes in the sender info (ignores packet
    // and octet count)
    NTPHigh = senderInfo.NTPseconds;
    NTPLow = senderInfo.NTPfraction;
    timestamp = senderInfo.RTPtimeStamp;

    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
                 VoEId(_instanceId, _channelId),
                 "GetRemoteRTCPData() => NTPHigh=%lu, NTPLow=%lu, "
                 "timestamp=%lu",
                 NTPHigh, NTPLow, timestamp);

    // --- Locally derived information

    // This value is updated on each incoming RTCP packet (0 when no packet
    // has been received)
    playoutTimestamp = _playoutTimeStampRTCP;

    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
                 VoEId(_instanceId, _channelId),
                 "GetRemoteRTCPData() => playoutTimestamp=%lu",
                 _playoutTimeStampRTCP);

    if (NULL != jitter || NULL != fractionLost)
    {
        // Get all RTCP receiver report blocks that have been received on this
        // channel. If we receive RTP packets from a remote source we know the
        // remote SSRC and use the report block from him.
        // Otherwise use the first report block.
        std::vector<RTCPReportBlock> remote_stats;
        if (_rtpRtcpModule->RemoteRTCPStat(&remote_stats) != 0 ||
            remote_stats.empty()) {
          WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                       VoEId(_instanceId, _channelId),
                       "GetRemoteRTCPData() failed to measure statistics due"
                       " to lack of received RTP and/or RTCP packets");
          return -1;
        }

        WebRtc_UWord32 remoteSSRC = _rtpRtcpModule->RemoteSSRC();
        std::vector<RTCPReportBlock>::const_iterator it = remote_stats.begin();
        for (; it != remote_stats.end(); ++it) {
          if (it->remoteSSRC == remoteSSRC)
            break;
        }

        if (it == remote_stats.end()) {
          // If we have not received any RTCP packets from this SSRC it probably
          // means that we have not received any RTP packets.
          // Use the first received report block instead.
          it = remote_stats.begin();
          remoteSSRC = it->remoteSSRC;
        }

        if (jitter) {
          *jitter = it->jitter;
          WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
                       VoEId(_instanceId, _channelId),
                       "GetRemoteRTCPData() => jitter = %lu", *jitter);
        }

        if (fractionLost) {
          *fractionLost = it->fractionLost;
          WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
                       VoEId(_instanceId, _channelId),
                       "GetRemoteRTCPData() => fractionLost = %lu",
                       *fractionLost);
        }
    }
    return 0;
}

int
Channel::SendApplicationDefinedRTCPPacket(const unsigned char subType,
                                             unsigned int name,
                                             const char* data,
                                             unsigned short dataLengthInBytes)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
                 "Channel::SendApplicationDefinedRTCPPacket()");
    if (!_sending)
    {
        _engineStatisticsPtr->SetLastError(
            VE_NOT_SENDING, kTraceError,
            "SendApplicationDefinedRTCPPacket() not sending");
        return -1;
    }
    if (NULL == data)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "SendApplicationDefinedRTCPPacket() invalid data value");
        return -1;
    }
    if (dataLengthInBytes % 4 != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "SendApplicationDefinedRTCPPacket() invalid length value");
        return -1;
    }
    RTCPMethod status = _rtpRtcpModule->RTCP();
    if (status == kRtcpOff)
    {
        _engineStatisticsPtr->SetLastError(
            VE_RTCP_ERROR, kTraceError,
            "SendApplicationDefinedRTCPPacket() RTCP is disabled");
        return -1;
    }

    // Create and schedule the RTCP APP packet for transmission
    if (_rtpRtcpModule->SetRTCPApplicationSpecificData(
        subType,
        name,
        (const unsigned char*) data,
        dataLengthInBytes) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_SEND_ERROR, kTraceError,
            "SendApplicationDefinedRTCPPacket() failed to send RTCP packet");
        return -1;
    }
    return 0;
}

int
Channel::GetRTPStatistics(
        unsigned int& averageJitterMs,
        unsigned int& maxJitterMs,
        unsigned int& discardedPackets)
{
    WebRtc_UWord8 fraction_lost(0);
    WebRtc_UWord32 cum_lost(0);
    WebRtc_UWord32 ext_max(0);
    WebRtc_UWord32 jitter(0);
    WebRtc_UWord32 max_jitter(0);

    // The jitter statistics is updated for each received RTP packet and is
    // based on received packets.
    if (_rtpRtcpModule->StatisticsRTP(&fraction_lost,
                                     &cum_lost,
                                     &ext_max,
                                     &jitter,
                                     &max_jitter) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_CANNOT_RETRIEVE_RTP_STAT, kTraceWarning,
            "GetRTPStatistics() failed to read RTP statistics from the "
            "RTP/RTCP module");
    }

    const WebRtc_Word32 playoutFrequency =
        _audioCodingModule.PlayoutFrequency();
    if (playoutFrequency > 0)
    {
        // Scale RTP statistics given the current playout frequency
        maxJitterMs = max_jitter / (playoutFrequency / 1000);
        averageJitterMs = jitter / (playoutFrequency / 1000);
    }

    discardedPackets = _numberOfDiscardedPackets;

    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
               VoEId(_instanceId, _channelId),
               "GetRTPStatistics() => averageJitterMs = %lu, maxJitterMs = %lu,"
               " discardedPackets = %lu)",
               averageJitterMs, maxJitterMs, discardedPackets);
    return 0;
}

int Channel::GetRemoteRTCPSenderInfo(SenderInfo* sender_info) {
  if (sender_info == NULL) {
    _engineStatisticsPtr->SetLastError(VE_INVALID_ARGUMENT, kTraceError,
        "GetRemoteRTCPSenderInfo() invalid sender_info.");
    return -1;
  }

  // Get the sender info from the latest received RTCP Sender Report.
  RTCPSenderInfo rtcp_sender_info;
  if (_rtpRtcpModule->RemoteRTCPStat(&rtcp_sender_info) != 0) {
    _engineStatisticsPtr->SetLastError(VE_RTP_RTCP_MODULE_ERROR, kTraceError,
        "GetRemoteRTCPSenderInfo() failed to read RTCP SR sender info.");
    return -1;
  }

  sender_info->NTP_timestamp_high = rtcp_sender_info.NTPseconds;
  sender_info->NTP_timestamp_low = rtcp_sender_info.NTPfraction;
  sender_info->RTP_timestamp = rtcp_sender_info.RTPtimeStamp;
  sender_info->sender_packet_count = rtcp_sender_info.sendPacketCount;
  sender_info->sender_octet_count = rtcp_sender_info.sendOctetCount;
  return 0;
}

int Channel::GetRemoteRTCPReportBlocks(
    std::vector<ReportBlock>* report_blocks) {
  if (report_blocks == NULL) {
    _engineStatisticsPtr->SetLastError(VE_INVALID_ARGUMENT, kTraceError,
      "GetRemoteRTCPReportBlock()s invalid report_blocks.");
    return -1;
  }

  // Get the report blocks from the latest received RTCP Sender or Receiver
  // Report. Each element in the vector contains the sender's SSRC and a
  // report block according to RFC 3550.
  std::vector<RTCPReportBlock> rtcp_report_blocks;
  if (_rtpRtcpModule->RemoteRTCPStat(&rtcp_report_blocks) != 0) {
    _engineStatisticsPtr->SetLastError(VE_RTP_RTCP_MODULE_ERROR, kTraceError,
        "GetRemoteRTCPReportBlocks() failed to read RTCP SR/RR report block.");
    return -1;
  }

  if (rtcp_report_blocks.empty())
    return 0;

  std::vector<RTCPReportBlock>::const_iterator it = rtcp_report_blocks.begin();
  for (; it != rtcp_report_blocks.end(); ++it) {
    ReportBlock report_block;
    report_block.sender_SSRC = it->remoteSSRC;
    report_block.source_SSRC = it->sourceSSRC;
    report_block.fraction_lost = it->fractionLost;
    report_block.cumulative_num_packets_lost = it->cumulativeLost;
    report_block.extended_highest_sequence_number = it->extendedHighSeqNum;
    report_block.interarrival_jitter = it->jitter;
    report_block.last_SR_timestamp = it->lastSR;
    report_block.delay_since_last_SR = it->delaySinceLastSR;
    report_blocks->push_back(report_block);
  }
  return 0;
}

int
Channel::GetRTPStatistics(CallStatistics& stats)
{
    WebRtc_UWord8 fraction_lost(0);
    WebRtc_UWord32 cum_lost(0);
    WebRtc_UWord32 ext_max(0);
    WebRtc_UWord32 jitter(0);
    WebRtc_UWord32 max_jitter(0);

    // --- Part one of the final structure (four values)

    // The jitter statistics is updated for each received RTP packet and is
    // based on received packets.
    if (_rtpRtcpModule->StatisticsRTP(&fraction_lost,
                                     &cum_lost,
                                     &ext_max,
                                     &jitter,
                                     &max_jitter) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_CANNOT_RETRIEVE_RTP_STAT, kTraceWarning,
            "GetRTPStatistics() failed to read RTP statistics from the "
            "RTP/RTCP module");
    }

    stats.fractionLost = fraction_lost;
    stats.cumulativeLost = cum_lost;
    stats.extendedMax = ext_max;
    stats.jitterSamples = jitter;

    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
                 VoEId(_instanceId, _channelId),
                 "GetRTPStatistics() => fractionLost=%lu, cumulativeLost=%lu,"
                 " extendedMax=%lu, jitterSamples=%li)",
                 stats.fractionLost, stats.cumulativeLost, stats.extendedMax,
                 stats.jitterSamples);

    // --- Part two of the final structure (one value)

    WebRtc_UWord16 RTT(0);
    RTCPMethod method = _rtpRtcpModule->RTCP();
    if (method == kRtcpOff)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                     VoEId(_instanceId, _channelId),
                     "GetRTPStatistics() RTCP is disabled => valid RTT "
                     "measurements cannot be retrieved");
    } else
    {
        // The remote SSRC will be zero if no RTP packet has been received.
        WebRtc_UWord32 remoteSSRC = _rtpRtcpModule->RemoteSSRC();
        if (remoteSSRC > 0)
        {
            WebRtc_UWord16 avgRTT(0);
            WebRtc_UWord16 maxRTT(0);
            WebRtc_UWord16 minRTT(0);

            if (_rtpRtcpModule->RTT(remoteSSRC, &RTT, &avgRTT, &minRTT, &maxRTT)
                != 0)
            {
                WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                             VoEId(_instanceId, _channelId),
                             "GetRTPStatistics() failed to retrieve RTT from "
                             "the RTP/RTCP module");
            }
        } else
        {
            WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                         VoEId(_instanceId, _channelId),
                         "GetRTPStatistics() failed to measure RTT since no "
                         "RTP packets have been received yet");
        }
    }

    stats.rttMs = static_cast<int> (RTT);

    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
                 VoEId(_instanceId, _channelId),
                 "GetRTPStatistics() => rttMs=%d", stats.rttMs);

    // --- Part three of the final structure (four values)

    WebRtc_UWord32 bytesSent(0);
    WebRtc_UWord32 packetsSent(0);
    WebRtc_UWord32 bytesReceived(0);
    WebRtc_UWord32 packetsReceived(0);

    if (_rtpRtcpModule->DataCountersRTP(&bytesSent,
                                       &packetsSent,
                                       &bytesReceived,
                                       &packetsReceived) != 0)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                     VoEId(_instanceId, _channelId),
                     "GetRTPStatistics() failed to retrieve RTP datacounters =>"
                     " output will not be complete");
    }

    stats.bytesSent = bytesSent;
    stats.packetsSent = packetsSent;
    stats.bytesReceived = bytesReceived;
    stats.packetsReceived = packetsReceived;

    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
                 VoEId(_instanceId, _channelId),
                 "GetRTPStatistics() => bytesSent=%d, packetsSent=%d,"
                 " bytesReceived=%d, packetsReceived=%d)",
                 stats.bytesSent, stats.packetsSent, stats.bytesReceived,
                 stats.packetsReceived);

    return 0;
}

int Channel::SetFECStatus(bool enable, int redPayloadtype) {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::SetFECStatus()");

  if (SetRedPayloadType(redPayloadtype) < 0) {
    _engineStatisticsPtr->SetLastError(
        VE_CODEC_ERROR, kTraceError,
        "SetSecondarySendCodec() Failed to register RED ACM");
    return -1;
  }

  if (_audioCodingModule.SetFECStatus(enable) != 0) {
    _engineStatisticsPtr->SetLastError(
        VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
        "SetFECStatus() failed to set FEC state in the ACM");
    return -1;
  }
  return 0;
}

int
Channel::GetFECStatus(bool& enabled, int& redPayloadtype)
{
    enabled = _audioCodingModule.FECStatus();
    if (enabled)
    {
        WebRtc_Word8 payloadType(0);
        if (_rtpRtcpModule->SendREDPayloadType(payloadType) != 0)
        {
            _engineStatisticsPtr->SetLastError(
                VE_RTP_RTCP_MODULE_ERROR, kTraceError,
                "GetFECStatus() failed to retrieve RED PT from RTP/RTCP "
                "module");
            return -1;
        }
        WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
                   VoEId(_instanceId, _channelId),
                   "GetFECStatus() => enabled=%d, redPayloadtype=%d",
                   enabled, redPayloadtype);
        return 0;
    }
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
                 VoEId(_instanceId, _channelId),
                 "GetFECStatus() => enabled=%d", enabled);
    return 0;
}

int
Channel::StartRTPDump(const char fileNameUTF8[1024],
                      RTPDirections direction)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
                 "Channel::StartRTPDump()");
    if ((direction != kRtpIncoming) && (direction != kRtpOutgoing))
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "StartRTPDump() invalid RTP direction");
        return -1;
    }
    RtpDump* rtpDumpPtr = (direction == kRtpIncoming) ?
        &_rtpDumpIn : &_rtpDumpOut;
    if (rtpDumpPtr == NULL)
    {
        assert(false);
        return -1;
    }
    if (rtpDumpPtr->IsActive())
    {
        rtpDumpPtr->Stop();
    }
    if (rtpDumpPtr->Start(fileNameUTF8) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_BAD_FILE, kTraceError,
            "StartRTPDump() failed to create file");
        return -1;
    }
    return 0;
}

int
Channel::StopRTPDump(RTPDirections direction)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
                 "Channel::StopRTPDump()");
    if ((direction != kRtpIncoming) && (direction != kRtpOutgoing))
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "StopRTPDump() invalid RTP direction");
        return -1;
    }
    RtpDump* rtpDumpPtr = (direction == kRtpIncoming) ?
        &_rtpDumpIn : &_rtpDumpOut;
    if (rtpDumpPtr == NULL)
    {
        assert(false);
        return -1;
    }
    if (!rtpDumpPtr->IsActive())
    {
        return 0;
    }
    return rtpDumpPtr->Stop();
}

bool
Channel::RTPDumpIsActive(RTPDirections direction)
{
    if ((direction != kRtpIncoming) &&
        (direction != kRtpOutgoing))
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "RTPDumpIsActive() invalid RTP direction");
        return false;
    }
    RtpDump* rtpDumpPtr = (direction == kRtpIncoming) ?
        &_rtpDumpIn : &_rtpDumpOut;
    return rtpDumpPtr->IsActive();
}

int
Channel::InsertExtraRTPPacket(unsigned char payloadType,
                              bool markerBit,
                              const char* payloadData,
                              unsigned short payloadSize)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::InsertExtraRTPPacket()");
    if (payloadType > 127)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_PLTYPE, kTraceError,
            "InsertExtraRTPPacket() invalid payload type");
        return -1;
    }
    if (payloadData == NULL)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "InsertExtraRTPPacket() invalid payload data");
        return -1;
    }
    if (payloadSize > _rtpRtcpModule->MaxDataPayloadLength())
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "InsertExtraRTPPacket() invalid payload size");
        return -1;
    }
    if (!_sending)
    {
        _engineStatisticsPtr->SetLastError(
            VE_NOT_SENDING, kTraceError,
            "InsertExtraRTPPacket() not sending");
        return -1;
    }

    // Create extra RTP packet by calling RtpRtcp::SendOutgoingData().
    // Transport::SendPacket() will be called by the module when the RTP packet
    // is created.
    // The call to SendOutgoingData() does *not* modify the timestamp and
    // payloadtype to ensure that the RTP module generates a valid RTP packet
    // (user might utilize a non-registered payload type).
    // The marker bit and payload type will be replaced just before the actual
    // transmission, i.e., the actual modification is done *after* the RTP
    // module has delivered its RTP packet back to the VoE.
    // We will use the stored values above when the packet is modified
    // (see Channel::SendPacket()).

    _extraPayloadType = payloadType;
    _extraMarkerBit = markerBit;
    _insertExtraRTPPacket = true;

    if (_rtpRtcpModule->SendOutgoingData(kAudioFrameSpeech,
                                        _lastPayloadType,
                                        _lastLocalTimeStamp,
                                        // Leaving the time when this frame was
                                        // received from the capture device as
                                        // undefined for voice for now.
                                        -1,
                                        (const WebRtc_UWord8*) payloadData,
                                        payloadSize) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_RTP_RTCP_MODULE_ERROR, kTraceError,
            "InsertExtraRTPPacket() failed to send extra RTP packet");
        return -1;
    }

    return 0;
}

WebRtc_UWord32
Channel::Demultiplex(const AudioFrame& audioFrame)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::Demultiplex()");
    _audioFrame = audioFrame;
    _audioFrame.id_ = _channelId;
    return 0;
}

WebRtc_UWord32
Channel::PrepareEncodeAndSend(int mixingFrequency)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::PrepareEncodeAndSend()");

    if (_audioFrame.samples_per_channel_ == 0)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId,_channelId),
                     "Channel::PrepareEncodeAndSend() invalid audio frame");
        return -1;
    }

    if (_inputFilePlaying)
    {
        MixOrReplaceAudioWithFile(mixingFrequency);
    }

    if (_mute)
    {
        AudioFrameOperations::Mute(_audioFrame);
    }

    if (_inputExternalMedia)
    {
        CriticalSectionScoped cs(&_callbackCritSect);
        const bool isStereo = (_audioFrame.num_channels_ == 2);
        if (_inputExternalMediaCallbackPtr)
        {
            _inputExternalMediaCallbackPtr->Process(
                _channelId,
                kRecordingPerChannel,
               (WebRtc_Word16*)_audioFrame.data_,
                _audioFrame.samples_per_channel_,
                _audioFrame.sample_rate_hz_,
                isStereo);
        }
    }

    InsertInbandDtmfTone();

    if (_includeAudioLevelIndication)
    {
        assert(_rtpAudioProc.get() != NULL);

        // Check if settings need to be updated.
        if (_rtpAudioProc->sample_rate_hz() != _audioFrame.sample_rate_hz_)
        {
            if (_rtpAudioProc->set_sample_rate_hz(_audioFrame.sample_rate_hz_) !=
                AudioProcessing::kNoError)
            {
                WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                             VoEId(_instanceId, _channelId),
                             "Error setting AudioProcessing sample rate");
                return -1;
            }
        }

        if (_rtpAudioProc->num_input_channels() != _audioFrame.num_channels_)
        {
            if (_rtpAudioProc->set_num_channels(_audioFrame.num_channels_,
                                                _audioFrame.num_channels_)
                != AudioProcessing::kNoError)
            {
                WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                             VoEId(_instanceId, _channelId),
                             "Error setting AudioProcessing channels");
                return -1;
            }
        }

        // Performs level analysis only; does not affect the signal.
        _rtpAudioProc->ProcessStream(&_audioFrame);
    }

    return 0;
}

WebRtc_UWord32
Channel::EncodeAndSend()
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::EncodeAndSend()");

    assert(_audioFrame.num_channels_ <= 2);
    if (_audioFrame.samples_per_channel_ == 0)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId,_channelId),
                     "Channel::EncodeAndSend() invalid audio frame");
        return -1;
    }

    _audioFrame.id_ = _channelId;

    // --- Add 10ms of raw (PCM) audio data to the encoder @ 32kHz.

    // The ACM resamples internally.
    _audioFrame.timestamp_ = _timeStamp;
    if (_audioCodingModule.Add10MsData((AudioFrame&)_audioFrame) != 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,_channelId),
                     "Channel::EncodeAndSend() ACM encoding failed");
        return -1;
    }

    _timeStamp += _audioFrame.samples_per_channel_;

    // --- Encode if complete frame is ready

    // This call will trigger AudioPacketizationCallback::SendData if encoding
    // is done and payload is ready for packetization and transmission.
    return _audioCodingModule.Process();
}

int Channel::RegisterExternalMediaProcessing(
    ProcessingTypes type,
    VoEMediaProcess& processObject)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::RegisterExternalMediaProcessing()");

    CriticalSectionScoped cs(&_callbackCritSect);

    if (kPlaybackPerChannel == type)
    {
        if (_outputExternalMediaCallbackPtr)
        {
            _engineStatisticsPtr->SetLastError(
                VE_INVALID_OPERATION, kTraceError,
                "Channel::RegisterExternalMediaProcessing() "
                "output external media already enabled");
            return -1;
        }
        _outputExternalMediaCallbackPtr = &processObject;
        _outputExternalMedia = true;
    }
    else if (kRecordingPerChannel == type)
    {
        if (_inputExternalMediaCallbackPtr)
        {
            _engineStatisticsPtr->SetLastError(
                VE_INVALID_OPERATION, kTraceError,
                "Channel::RegisterExternalMediaProcessing() "
                "output external media already enabled");
            return -1;
        }
        _inputExternalMediaCallbackPtr = &processObject;
        _inputExternalMedia = true;
    }
    return 0;
}

int Channel::DeRegisterExternalMediaProcessing(ProcessingTypes type)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::DeRegisterExternalMediaProcessing()");

    CriticalSectionScoped cs(&_callbackCritSect);

    if (kPlaybackPerChannel == type)
    {
        if (!_outputExternalMediaCallbackPtr)
        {
            _engineStatisticsPtr->SetLastError(
                VE_INVALID_OPERATION, kTraceWarning,
                "Channel::DeRegisterExternalMediaProcessing() "
                "output external media already disabled");
            return 0;
        }
        _outputExternalMedia = false;
        _outputExternalMediaCallbackPtr = NULL;
    }
    else if (kRecordingPerChannel == type)
    {
        if (!_inputExternalMediaCallbackPtr)
        {
            _engineStatisticsPtr->SetLastError(
                VE_INVALID_OPERATION, kTraceWarning,
                "Channel::DeRegisterExternalMediaProcessing() "
                "input external media already disabled");
            return 0;
        }
        _inputExternalMedia = false;
        _inputExternalMediaCallbackPtr = NULL;
    }

    return 0;
}

int Channel::SetExternalMixing(bool enabled) {
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetExternalMixing(enabled=%d)", enabled);

    if (_playing)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_OPERATION, kTraceError,
            "Channel::SetExternalMixing() "
            "external mixing cannot be changed while playing.");
        return -1;
    }

    _externalMixing = enabled;

    return 0;
}

int
Channel::ResetRTCPStatistics()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::ResetRTCPStatistics()");
    WebRtc_UWord32 remoteSSRC(0);
    remoteSSRC = _rtpRtcpModule->RemoteSSRC();
    return _rtpRtcpModule->ResetRTT(remoteSSRC);
}

int
Channel::GetRoundTripTimeSummary(StatVal& delaysMs) const
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::GetRoundTripTimeSummary()");
    // Override default module outputs for the case when RTCP is disabled.
    // This is done to ensure that we are backward compatible with the
    // VoiceEngine where we did not use RTP/RTCP module.
    if (!_rtpRtcpModule->RTCP())
    {
        delaysMs.min = -1;
        delaysMs.max = -1;
        delaysMs.average = -1;
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId,_channelId),
                     "Channel::GetRoundTripTimeSummary() RTCP is disabled =>"
                     " valid RTT measurements cannot be retrieved");
        return 0;
    }

    WebRtc_UWord32 remoteSSRC;
    WebRtc_UWord16 RTT;
    WebRtc_UWord16 avgRTT;
    WebRtc_UWord16 maxRTT;
    WebRtc_UWord16 minRTT;
    // The remote SSRC will be zero if no RTP packet has been received.
    remoteSSRC = _rtpRtcpModule->RemoteSSRC();
    if (remoteSSRC == 0)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId,_channelId),
                     "Channel::GetRoundTripTimeSummary() unable to measure RTT"
                     " since no RTP packet has been received yet");
    }

    // Retrieve RTT statistics from the RTP/RTCP module for the specified
    // channel and SSRC. The SSRC is required to parse out the correct source
    // in conference scenarios.
    if (_rtpRtcpModule->RTT(remoteSSRC, &RTT, &avgRTT, &minRTT,&maxRTT) != 0)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId,_channelId),
                     "GetRoundTripTimeSummary unable to retrieve RTT values"
                     " from the RTCP layer");
        delaysMs.min = -1; delaysMs.max = -1; delaysMs.average = -1;
    }
    else
    {
        delaysMs.min = minRTT;
        delaysMs.max = maxRTT;
        delaysMs.average = avgRTT;
    }
    return 0;
}

int
Channel::GetNetworkStatistics(NetworkStatistics& stats)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::GetNetworkStatistics()");
    return _audioCodingModule.NetworkStatistics(
        (ACMNetworkStatistics &)stats);
}

int
Channel::GetDelayEstimate(int& delayMs) const
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::GetDelayEstimate()");
    delayMs = (_averageDelayMs + 5) / 10 + _recPacketDelayMs;
    return 0;
}

int
Channel::SetMinimumPlayoutDelay(int delayMs)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetMinimumPlayoutDelay()");
    if ((delayMs < kVoiceEngineMinMinPlayoutDelayMs) ||
        (delayMs > kVoiceEngineMaxMinPlayoutDelayMs))
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "SetMinimumPlayoutDelay() invalid min delay");
        return -1;
    }
    if (_audioCodingModule.SetMinimumPlayoutDelay(delayMs) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
            "SetMinimumPlayoutDelay() failed to set min playout delay");
        return -1;
    }
    return 0;
}

int
Channel::GetPlayoutTimestamp(unsigned int& timestamp)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::GetPlayoutTimestamp()");
    WebRtc_UWord32 playoutTimestamp(0);
    if (GetPlayoutTimeStamp(playoutTimestamp) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_CANNOT_RETRIEVE_VALUE, kTraceError,
            "GetPlayoutTimestamp() failed to retrieve timestamp");
        return -1;
    }
    timestamp = playoutTimestamp;
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
                 VoEId(_instanceId,_channelId),
                 "GetPlayoutTimestamp() => timestamp=%u", timestamp);
    return 0;
}

int
Channel::SetInitTimestamp(unsigned int timestamp)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
               "Channel::SetInitTimestamp()");
    if (_sending)
    {
        _engineStatisticsPtr->SetLastError(
            VE_SENDING, kTraceError, "SetInitTimestamp() already sending");
        return -1;
    }
    if (_rtpRtcpModule->SetStartTimestamp(timestamp) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_RTP_RTCP_MODULE_ERROR, kTraceError,
            "SetInitTimestamp() failed to set timestamp");
        return -1;
    }
    return 0;
}

int
Channel::SetInitSequenceNumber(short sequenceNumber)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetInitSequenceNumber()");
    if (_sending)
    {
        _engineStatisticsPtr->SetLastError(
            VE_SENDING, kTraceError,
            "SetInitSequenceNumber() already sending");
        return -1;
    }
    if (_rtpRtcpModule->SetSequenceNumber(sequenceNumber) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_RTP_RTCP_MODULE_ERROR, kTraceError,
            "SetInitSequenceNumber() failed to set sequence number");
        return -1;
    }
    return 0;
}

int
Channel::GetRtpRtcp(RtpRtcp* &rtpRtcpModule) const
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::GetRtpRtcp()");
    rtpRtcpModule = _rtpRtcpModule.get();
    return 0;
}

// TODO(andrew): refactor Mix functions here and in transmit_mixer.cc to use
// a shared helper.
WebRtc_Word32
Channel::MixOrReplaceAudioWithFile(const int mixingFrequency)
{
    scoped_array<WebRtc_Word16> fileBuffer(new WebRtc_Word16[640]);
    int fileSamples(0);

    {
        CriticalSectionScoped cs(&_fileCritSect);

        if (_inputFilePlayerPtr == NULL)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                         VoEId(_instanceId, _channelId),
                         "Channel::MixOrReplaceAudioWithFile() fileplayer"
                             " doesnt exist");
            return -1;
        }

        if (_inputFilePlayerPtr->Get10msAudioFromFile(fileBuffer.get(),
                                                      fileSamples,
                                                      mixingFrequency) == -1)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                         VoEId(_instanceId, _channelId),
                         "Channel::MixOrReplaceAudioWithFile() file mixing "
                         "failed");
            return -1;
        }
        if (fileSamples == 0)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                         VoEId(_instanceId, _channelId),
                         "Channel::MixOrReplaceAudioWithFile() file is ended");
            return 0;
        }
    }

    assert(_audioFrame.samples_per_channel_ == fileSamples);

    if (_mixFileWithMicrophone)
    {
        // Currently file stream is always mono.
        // TODO(xians): Change the code when FilePlayer supports real stereo.
        Utility::MixWithSat(_audioFrame.data_,
                            _audioFrame.num_channels_,
                            fileBuffer.get(),
                            1,
                            fileSamples);
    }
    else
    {
        // Replace ACM audio with file.
        // Currently file stream is always mono.
        // TODO(xians): Change the code when FilePlayer supports real stereo.
        _audioFrame.UpdateFrame(_channelId,
                                -1,
                                fileBuffer.get(),
                                fileSamples,
                                mixingFrequency,
                                AudioFrame::kNormalSpeech,
                                AudioFrame::kVadUnknown,
                                1);

    }
    return 0;
}

WebRtc_Word32
Channel::MixAudioWithFile(AudioFrame& audioFrame,
                          const int mixingFrequency)
{
    assert(mixingFrequency <= 32000);

    scoped_array<WebRtc_Word16> fileBuffer(new WebRtc_Word16[640]);
    int fileSamples(0);

    {
        CriticalSectionScoped cs(&_fileCritSect);

        if (_outputFilePlayerPtr == NULL)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                         VoEId(_instanceId, _channelId),
                         "Channel::MixAudioWithFile() file mixing failed");
            return -1;
        }

        // We should get the frequency we ask for.
        if (_outputFilePlayerPtr->Get10msAudioFromFile(fileBuffer.get(),
                                                       fileSamples,
                                                       mixingFrequency) == -1)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                         VoEId(_instanceId, _channelId),
                         "Channel::MixAudioWithFile() file mixing failed");
            return -1;
        }
    }

    if (audioFrame.samples_per_channel_ == fileSamples)
    {
        // Currently file stream is always mono.
        // TODO(xians): Change the code when FilePlayer supports real stereo.
        Utility::MixWithSat(audioFrame.data_,
                            audioFrame.num_channels_,
                            fileBuffer.get(),
                            1,
                            fileSamples);
    }
    else
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId,_channelId),
            "Channel::MixAudioWithFile() samples_per_channel_(%d) != "
            "fileSamples(%d)",
            audioFrame.samples_per_channel_, fileSamples);
        return -1;
    }

    return 0;
}

int
Channel::InsertInbandDtmfTone()
{
    // Check if we should start a new tone.
    if (_inbandDtmfQueue.PendingDtmf() &&
        !_inbandDtmfGenerator.IsAddingTone() &&
        _inbandDtmfGenerator.DelaySinceLastTone() >
        kMinTelephoneEventSeparationMs)
    {
        WebRtc_Word8 eventCode(0);
        WebRtc_UWord16 lengthMs(0);
        WebRtc_UWord8 attenuationDb(0);

        eventCode = _inbandDtmfQueue.NextDtmf(&lengthMs, &attenuationDb);
        _inbandDtmfGenerator.AddTone(eventCode, lengthMs, attenuationDb);
        if (_playInbandDtmfEvent)
        {
            // Add tone to output mixer using a reduced length to minimize
            // risk of echo.
            _outputMixerPtr->PlayDtmfTone(eventCode, lengthMs - 80,
                                          attenuationDb);
        }
    }

    if (_inbandDtmfGenerator.IsAddingTone())
    {
        WebRtc_UWord16 frequency(0);
        _inbandDtmfGenerator.GetSampleRate(frequency);

        if (frequency != _audioFrame.sample_rate_hz_)
        {
            // Update sample rate of Dtmf tone since the mixing frequency
            // has changed.
            _inbandDtmfGenerator.SetSampleRate(
                (WebRtc_UWord16) (_audioFrame.sample_rate_hz_));
            // Reset the tone to be added taking the new sample rate into
            // account.
            _inbandDtmfGenerator.ResetTone();
        }
        
        WebRtc_Word16 toneBuffer[320];
        WebRtc_UWord16 toneSamples(0);
        // Get 10ms tone segment and set time since last tone to zero
        if (_inbandDtmfGenerator.Get10msTone(toneBuffer, toneSamples) == -1)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                       VoEId(_instanceId, _channelId),
                       "Channel::EncodeAndSend() inserting Dtmf failed");
            return -1;
        }

        // Replace mixed audio with DTMF tone.
        for (int sample = 0; 
            sample < _audioFrame.samples_per_channel_;
            sample++)
        {
            for (int channel = 0; 
                channel < _audioFrame.num_channels_; 
                channel++)
            {
                _audioFrame.data_[sample * _audioFrame.num_channels_ + channel] = 
                        toneBuffer[sample];
            }
        }
        
        assert(_audioFrame.samples_per_channel_ == toneSamples);
    } else
    {
        // Add 10ms to "delay-since-last-tone" counter
        _inbandDtmfGenerator.UpdateDelaySinceLastTone();
    }
    return 0;
}

WebRtc_Word32
Channel::GetPlayoutTimeStamp(WebRtc_UWord32& playoutTimestamp)
{
    WebRtc_UWord32 timestamp(0);
    CodecInst currRecCodec;

    if (_audioCodingModule.PlayoutTimestamp(timestamp) == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId,_channelId),
                     "Channel::GetPlayoutTimeStamp() failed to read playout"
                     " timestamp from the ACM");
        return -1;
    }

    WebRtc_UWord16 delayMS(0);
    if (_audioDeviceModulePtr->PlayoutDelay(&delayMS) == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId,_channelId),
                     "Channel::GetPlayoutTimeStamp() failed to read playout"
                     " delay from the ADM");
        return -1;
    }

    WebRtc_Word32 playoutFrequency = _audioCodingModule.PlayoutFrequency();
    if (_audioCodingModule.ReceiveCodec(currRecCodec) == 0) {
      if (STR_CASE_CMP("G722", currRecCodec.plname) == 0) {
        playoutFrequency = 8000;
      } else if (STR_CASE_CMP("opus", currRecCodec.plname) == 0) {
        playoutFrequency = 48000;
      }
    }
    timestamp -= (delayMS * (playoutFrequency/1000));

    playoutTimestamp = timestamp;

    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::GetPlayoutTimeStamp() => playoutTimestamp = %lu",
                 playoutTimestamp);
    return 0;
}

void
Channel::ResetDeadOrAliveCounters()
{
    _countDeadDetections = 0;
    _countAliveDetections = 0;
}

void
Channel::UpdateDeadOrAliveCounters(bool alive)
{
    if (alive)
        _countAliveDetections++;
    else
        _countDeadDetections++;
}

int
Channel::GetDeadOrAliveCounters(int& countDead, int& countAlive) const
{
    bool enabled;
    WebRtc_UWord8 timeSec;

    _rtpRtcpModule->PeriodicDeadOrAliveStatus(enabled, timeSec);
    if (!enabled)
        return (-1);

    countDead = static_cast<int> (_countDeadDetections);
    countAlive = static_cast<int> (_countAliveDetections);
    return 0;
}

WebRtc_Word32
Channel::SendPacketRaw(const void *data, int len, bool RTCP)
{
    if (_transportPtr == NULL)
    {
        return -1;
    }
    if (!RTCP)
    {
        return _transportPtr->SendPacket(_channelId, data, len);
    }
    else
    {
        return _transportPtr->SendRTCPPacket(_channelId, data, len);
    }
}

WebRtc_Word32
Channel::UpdatePacketDelay(const WebRtc_UWord32 timestamp,
                           const WebRtc_UWord16 sequenceNumber)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::UpdatePacketDelay(timestamp=%lu, sequenceNumber=%u)",
                 timestamp, sequenceNumber);

    WebRtc_Word32 rtpReceiveFrequency(0);

    // Get frequency of last received payload
    rtpReceiveFrequency = _audioCodingModule.ReceiveFrequency();

    CodecInst currRecCodec;
    if (_audioCodingModule.ReceiveCodec(currRecCodec) == 0) {
      if (STR_CASE_CMP("G722", currRecCodec.plname) == 0) {
        // Even though the actual sampling rate for G.722 audio is
        // 16,000 Hz, the RTP clock rate for the G722 payload format is
        // 8,000 Hz because that value was erroneously assigned in
        // RFC 1890 and must remain unchanged for backward compatibility.
        rtpReceiveFrequency = 8000;
      } else if (STR_CASE_CMP("opus", currRecCodec.plname) == 0) {
        // We are resampling Opus internally to 32,000 Hz until all our
        // DSP routines can operate at 48,000 Hz, but the RTP clock
        // rate for the Opus payload format is standardized to 48,000 Hz,
        // because that is the maximum supported decoding sampling rate.
        rtpReceiveFrequency = 48000;
      }
    }

    const WebRtc_UWord32 timeStampDiff = timestamp - _playoutTimeStampRTP;
    WebRtc_UWord32 timeStampDiffMs(0);

    if (timeStampDiff > 0)
    {
        switch (rtpReceiveFrequency) {
          case 8000:
            timeStampDiffMs = static_cast<WebRtc_UWord32>(timeStampDiff >> 3);
            break;
          case 16000:
            timeStampDiffMs = static_cast<WebRtc_UWord32>(timeStampDiff >> 4);
            break;
          case 32000:
            timeStampDiffMs = static_cast<WebRtc_UWord32>(timeStampDiff >> 5);
            break;
          case 48000:
            timeStampDiffMs = static_cast<WebRtc_UWord32>(timeStampDiff / 48);
            break;
          default:
            WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                         VoEId(_instanceId, _channelId),
                         "Channel::UpdatePacketDelay() invalid sample rate");
            timeStampDiffMs = 0;
            return -1;
        }
        if (timeStampDiffMs > 5000)
        {
            timeStampDiffMs = 0;
        }

        if (_averageDelayMs == 0)
        {
            _averageDelayMs = timeStampDiffMs;
        }
        else
        {
            // Filter average delay value using exponential filter (alpha is
            // 7/8). We derive 10*_averageDelayMs here (reduces risk of
            // rounding error) and compensate for it in GetDelayEstimate()
            // later. Adding 4/8 results in correct rounding.
            _averageDelayMs = ((_averageDelayMs*7 + 10*timeStampDiffMs + 4)>>3);
        }

        if (sequenceNumber - _previousSequenceNumber == 1)
        {
            WebRtc_UWord16 packetDelayMs = 0;
            switch (rtpReceiveFrequency) {
              case 8000:
                packetDelayMs = static_cast<WebRtc_UWord16>(
                    (timestamp - _previousTimestamp) >> 3);
                break;
              case 16000:
                packetDelayMs = static_cast<WebRtc_UWord16>(
                    (timestamp - _previousTimestamp) >> 4);
                break;
              case 32000:
                packetDelayMs = static_cast<WebRtc_UWord16>(
                    (timestamp - _previousTimestamp) >> 5);
                break;
              case 48000:
                packetDelayMs = static_cast<WebRtc_UWord16>(
                    (timestamp - _previousTimestamp) / 48);
                break;
            }

            if (packetDelayMs >= 10 && packetDelayMs <= 60)
                _recPacketDelayMs = packetDelayMs;
        }
    }

    _previousSequenceNumber = sequenceNumber;
    _previousTimestamp = timestamp;

    return 0;
}

void
Channel::RegisterReceiveCodecsToRTPModule()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::RegisterReceiveCodecsToRTPModule()");


    CodecInst codec;
    const WebRtc_UWord8 nSupportedCodecs = AudioCodingModule::NumberOfCodecs();

    for (int idx = 0; idx < nSupportedCodecs; idx++)
    {
        // Open up the RTP/RTCP receiver for all supported codecs
        if ((_audioCodingModule.Codec(idx, codec) == -1) ||
            (_rtpRtcpModule->RegisterReceivePayload(codec) == -1))
        {
            WEBRTC_TRACE(
                         kTraceWarning,
                         kTraceVoice,
                         VoEId(_instanceId, _channelId),
                         "Channel::RegisterReceiveCodecsToRTPModule() unable"
                         " to register %s (%d/%d/%d/%d) to RTP/RTCP receiver",
                         codec.plname, codec.pltype, codec.plfreq,
                         codec.channels, codec.rate);
        }
        else
        {
            WEBRTC_TRACE(
                         kTraceInfo,
                         kTraceVoice,
                         VoEId(_instanceId, _channelId),
                         "Channel::RegisterReceiveCodecsToRTPModule() %s "
                         "(%d/%d/%d/%d) has been added to the RTP/RTCP "
                         "receiver",
                         codec.plname, codec.pltype, codec.plfreq,
                         codec.channels, codec.rate);
        }
    }
}

int Channel::ApmProcessRx(AudioFrame& frame) {
  AudioProcessing* audioproc = _rxAudioProcessingModulePtr;
  // Register the (possibly new) frame parameters.
  if (audioproc->set_sample_rate_hz(frame.sample_rate_hz_) != 0) {
    LOG_FERR1(LS_WARNING, set_sample_rate_hz, frame.sample_rate_hz_);
  }
  if (audioproc->set_num_channels(frame.num_channels_,
                                  frame.num_channels_) != 0) {
    LOG_FERR1(LS_WARNING, set_num_channels, frame.num_channels_);
  }
  if (audioproc->ProcessStream(&frame) != 0) {
    LOG_FERR0(LS_WARNING, ProcessStream);
  }
  return 0;
}

int Channel::SetSecondarySendCodec(const CodecInst& codec,
                                   int red_payload_type) {
  if (SetRedPayloadType(red_payload_type) < 0) {
    _engineStatisticsPtr->SetLastError(
        VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
        "SetSecondarySendCodec() Failed to register RED ACM");
    return -1;
  }
  if (_audioCodingModule.RegisterSecondarySendCodec(codec) < 0) {
    _engineStatisticsPtr->SetLastError(
        VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
        "SetSecondarySendCodec() Failed to register secondary send codec in "
        "ACM");
    return -1;
  }

  return 0;
}

void Channel::RemoveSecondarySendCodec() {
  _audioCodingModule.UnregisterSecondarySendCodec();
}

int Channel::GetSecondarySendCodec(CodecInst* codec) {
  if (_audioCodingModule.SecondarySendCodec(codec) < 0) {
    _engineStatisticsPtr->SetLastError(
        VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
        "GetSecondarySendCodec() Failed to get secondary sent codec from ACM");
    return -1;
  }
  return 0;
}

int Channel::SetRedPayloadType(int red_payload_type) {
  if (red_payload_type < 0) {
    _engineStatisticsPtr->SetLastError(
        VE_PLTYPE_ERROR, kTraceError,
        "SetRedPayloadType() invalid RED paylaod type");
    return -1;
  }

  CodecInst codec;
  bool found_red = false;

  // Get default RED settings from the ACM database
  const int num_codecs = AudioCodingModule::NumberOfCodecs();
  for (int idx = 0; idx < num_codecs; idx++) {
    _audioCodingModule.Codec(idx, codec);
    if (!STR_CASE_CMP(codec.plname, "RED")) {
      found_red = true;
      break;
    }
  }

  if (!found_red) {
    _engineStatisticsPtr->SetLastError(
        VE_CODEC_ERROR, kTraceError,
        "SetRedPayloadType() RED is not supported");
    return -1;
  }

  codec.pltype = red_payload_type;
  if (_audioCodingModule.RegisterSendCodec(codec) < 0) {
    _engineStatisticsPtr->SetLastError(
        VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
        "SetRedPayloadType() RED registration in ACM module failed");
    return -1;
  }

  if (_rtpRtcpModule->SetSendREDPayloadType(red_payload_type) != 0) {
    _engineStatisticsPtr->SetLastError(
        VE_RTP_RTCP_MODULE_ERROR, kTraceError,
        "SetRedPayloadType() RED registration in RTP/RTCP module failed");
    return -1;
  }
  return 0;
}

} // namespace voe
} // namespace webrtc
