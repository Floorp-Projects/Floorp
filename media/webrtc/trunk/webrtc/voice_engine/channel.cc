/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/channel.h"

#include "webrtc/common.h"
#include "webrtc/modules/audio_device/include/audio_device.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/modules/rtp_rtcp/interface/receive_statistics.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_payload_registry.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_receiver.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_receiver_strategy.h"
#include "webrtc/modules/utility/interface/audio_frame_operations.h"
#include "webrtc/modules/utility/interface/process_thread.h"
#include "webrtc/modules/utility/interface/rtp_dump.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/voice_engine/include/voe_base.h"
#include "webrtc/voice_engine/include/voe_external_media.h"
#include "webrtc/voice_engine/include/voe_rtp_rtcp.h"
#include "webrtc/voice_engine/output_mixer.h"
#include "webrtc/voice_engine/statistics.h"
#include "webrtc/voice_engine/transmit_mixer.h"
#include "webrtc/voice_engine/utility.h"

#if defined(_WIN32)
#include <Qos.h>
#endif

namespace webrtc {
namespace voe {

int32_t
Channel::SendData(FrameType frameType,
                  uint8_t   payloadType,
                  uint32_t  timeStamp,
                  const uint8_t*  payloadData,
                  uint16_t  payloadSize,
                  const RTPFragmentationHeader* fragmentation)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SendData(frameType=%u, payloadType=%u, timeStamp=%u,"
                 " payloadSize=%u, fragmentation=0x%x)",
                 frameType, payloadType, timeStamp, payloadSize, fragmentation);

    if (_includeAudioLevelIndication)
    {
        // Store current audio level in the RTP/RTCP module.
        // The level will be used in combination with voice-activity state
        // (frameType) to add an RTP header extension
        _rtpRtcpModule->SetAudioLevel(rtp_audioproc_->level_estimator()->RMS());
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

int32_t
Channel::InFrameType(int16_t frameType)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::InFrameType(frameType=%d)", frameType);

    CriticalSectionScoped cs(&_callbackCritSect);
    // 1 indicates speech
    _sendFrameType = (frameType == 1) ? 1 : 0;
    return 0;
}

int32_t
Channel::OnRxVadDetected(int vadDecision)
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
        uint8_t* rtpHdr = (uint8_t*)data;
        uint8_t M_PT(0);
        if (_extraMarkerBit)
        {
            M_PT = 0x80;            // set the M-bit
        }
        M_PT += _extraPayloadType;  // set the payload type
        *(++rtpHdr) = M_PT;     // modify the M|PT-byte within the RTP header
        _insertExtraRTPPacket = false;  // insert one packet only
    }

    uint8_t* bufferToSendPtr = (uint8_t*)data;
    int32_t bufferLength = len;

    // Dump the RTP packet to a file (if RTP dump is enabled).
    if (_rtpDumpOut.DumpPacket((const uint8_t*)data, len) == -1)
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
                    new uint8_t[kVoiceEngineMaxIpPacketSizeBytes];
                memset(_encryptionRTPBufferPtr, 0,
                       kVoiceEngineMaxIpPacketSizeBytes);
            }

            // Perform encryption (SRTP or external)
            int32_t encryptedBufferLength = 0;
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

    uint8_t* bufferToSendPtr = (uint8_t*)data;
    int32_t bufferLength = len;

    // Dump the RTCP packet to a file (if RTP dump is enabled).
    if (_rtpDumpOut.DumpPacket((const uint8_t*)data, len) == -1)
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
                    new uint8_t[kVoiceEngineMaxIpPacketSizeBytes];
            }

            // Perform encryption (SRTP or external).
            int32_t encryptedBufferLength = 0;
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
Channel::OnPlayTelephoneEvent(int32_t id,
                              uint8_t event,
                              uint16_t lengthMs,
                              uint8_t volume)
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
Channel::OnIncomingSSRCChanged(int32_t id, uint32_t ssrc)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::OnIncomingSSRCChanged(id=%d, SSRC=%d)",
                 id, ssrc);

    int32_t channel = VoEChannelId(id);
    assert(channel == _channelId);

    // Update ssrc so that NTP for AV sync can be updated.
    _rtpRtcpModule->SetRemoteSSRC(ssrc);

    if (_rtpObserver)
    {
        CriticalSectionScoped cs(&_callbackCritSect);

        if (_rtpObserverPtr)
        {
            // Send new SSRC to registered observer using callback
            _rtpObserverPtr->OnIncomingSSRCChanged(channel, ssrc);
        }
    }
}

void Channel::OnIncomingCSRCChanged(int32_t id,
                                    uint32_t CSRC,
                                    bool added)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::OnIncomingCSRCChanged(id=%d, CSRC=%d, added=%d)",
                 id, CSRC, added);

    int32_t channel = VoEChannelId(id);
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

void Channel::ResetStatistics(uint32_t ssrc) {
  StreamStatistician* statistician =
      rtp_receive_statistics_->GetStatistician(ssrc);
  if (statistician) {
    statistician->ResetStatistics();
  }
}

void
Channel::OnApplicationDataReceived(int32_t id,
                                   uint8_t subType,
                                   uint32_t name,
                                   uint16_t length,
                                   const uint8_t* data)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::OnApplicationDataReceived(id=%d, subType=%u,"
                 " name=%u, length=%u)",
                 id, subType, name, length);

    int32_t channel = VoEChannelId(id);
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

int32_t
Channel::OnInitializeDecoder(
    int32_t id,
    int8_t payloadType,
    const char payloadName[RTP_PAYLOAD_NAME_SIZE],
    int frequency,
    uint8_t channels,
    uint32_t rate)
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

    audio_coding_->Codec(payloadName, &dummyCodec, frequency, channels);
    receiveCodec.pacsize = dummyCodec.pacsize;

    // Register the new codec to the ACM
    if (audio_coding_->RegisterReceiveCodec(receiveCodec) == -1)
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
Channel::OnPacketTimeout(int32_t id)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::OnPacketTimeout(id=%d)", id);

    CriticalSectionScoped cs(_callbackCritSectPtr);
    if (_voiceEngineObserverPtr)
    {
        if (_receiving || _externalTransport)
        {
            int32_t channel = VoEChannelId(id);
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
Channel::OnReceivedPacket(int32_t id,
                          RtpRtcpPacketType packetType)
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
            int32_t channel = VoEChannelId(id);
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
Channel::OnPeriodicDeadOrAlive(int32_t id,
                               RTPAliveType alive)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::OnPeriodicDeadOrAlive(id=%d, alive=%d)", id, alive);

    {
        CriticalSectionScoped cs(&_callbackCritSect);
        if (!_connectionObserver)
            return;
    }

    int32_t channel = VoEChannelId(id);
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

int32_t
Channel::OnReceivedPayloadData(const uint8_t* payloadData,
                               uint16_t payloadSize,
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
    if (audio_coding_->IncomingPacket(payloadData,
                                      payloadSize,
                                      *rtpHeader) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_AUDIO_CODING_MODULE_ERROR, kTraceWarning,
            "Channel::OnReceivedPayloadData() unable to push data to the ACM");
        return -1;
    }

    // Update the packet delay.
    UpdatePacketDelay(rtpHeader->header.timestamp,
                      rtpHeader->header.sequenceNumber);

    uint16_t round_trip_time = 0;
    _rtpRtcpModule->RTT(rtp_receiver_->SSRC(), &round_trip_time,
                        NULL, NULL, NULL);

    std::vector<uint16_t> nack_list = audio_coding_->GetNackList(
        round_trip_time);
    if (!nack_list.empty()) {
      // Can't use nack_list.data() since it's not supported by all
      // compilers.
      ResendPackets(&(nack_list[0]), static_cast<int>(nack_list.size()));
    }
    return 0;
}

bool Channel::OnRecoveredPacket(const uint8_t* rtp_packet,
                                int rtp_packet_length) {
  RTPHeader header;
  if (!rtp_header_parser_->Parse(rtp_packet, rtp_packet_length, &header)) {
    WEBRTC_TRACE(kTraceDebug, webrtc::kTraceVoice, _channelId,
                 "IncomingPacket invalid RTP header");
    return false;
  }
  header.payload_type_frequency =
      rtp_payload_registry_->GetPayloadTypeFrequency(header.payloadType);
  if (header.payload_type_frequency < 0)
    return false;
  return ReceivePacket(rtp_packet, rtp_packet_length, header, false);
}

int32_t Channel::GetAudioFrame(int32_t id, AudioFrame& audioFrame)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::GetAudioFrame(id=%d)", id);

    // Get 10ms raw PCM data from the ACM (mixer limits output frequency)
    if (audio_coding_->PlayoutData10Ms(audioFrame.sample_rate_hz_,
                                       &audioFrame) == -1)
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
                (int16_t*)audioFrame.data_,
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

int32_t
Channel::NeededFrequency(int32_t id)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::NeededFrequency(id=%d)", id);

    int highestNeeded = 0;

    // Determine highest needed receive frequency
    int32_t receiveFrequency = audio_coding_->ReceiveFrequency();

    // Return the bigger of playout and receive frequency in the ACM.
    if (audio_coding_->PlayoutFrequency() > receiveFrequency)
    {
        highestNeeded = audio_coding_->PlayoutFrequency();
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

int32_t
Channel::CreateChannel(Channel*& channel,
                       int32_t channelId,
                       uint32_t instanceId,
                       const Config& config)
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(instanceId,channelId),
                 "Channel::CreateChannel(channelId=%d, instanceId=%d)",
        channelId, instanceId);

    channel = new Channel(channelId, instanceId, config);
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
Channel::PlayNotification(int32_t id, uint32_t durationMs)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::PlayNotification(id=%d, durationMs=%d)",
                 id, durationMs);

    // Not implement yet
}

void
Channel::RecordNotification(int32_t id, uint32_t durationMs)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::RecordNotification(id=%d, durationMs=%d)",
                 id, durationMs);

    // Not implement yet
}

void
Channel::PlayFileEnded(int32_t id)
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
Channel::RecordFileEnded(int32_t id)
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

Channel::Channel(int32_t channelId,
                 uint32_t instanceId,
                 const Config& config) :
    _fileCritSect(*CriticalSectionWrapper::CreateCriticalSection()),
    _callbackCritSect(*CriticalSectionWrapper::CreateCriticalSection()),
    _instanceId(instanceId),
    _channelId(channelId),
    rtp_header_parser_(RtpHeaderParser::Create()),
    rtp_payload_registry_(
        new RTPPayloadRegistry(channelId,
                               RTPPayloadStrategy::CreateStrategy(true))),
    rtp_receive_statistics_(ReceiveStatistics::Create(
        Clock::GetRealTimeClock())),
    rtp_receiver_(RtpReceiver::CreateAudioReceiver(
        VoEModuleId(instanceId, channelId), Clock::GetRealTimeClock(), this,
        this, this, rtp_payload_registry_.get())),
    telephone_event_handler_(rtp_receiver_->GetTelephoneEventHandler()),
    audio_coding_(config.Get<AudioCodingModuleFactory>().Create(
        VoEModuleId(instanceId, channelId))),
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
    playout_timestamp_rtp_(0),
    playout_timestamp_rtcp_(0),
    _numberOfDiscardedPackets(0),
    send_sequence_number_(0),
    _engineStatisticsPtr(NULL),
    _outputMixerPtr(NULL),
    _transmitMixerPtr(NULL),
    _moduleProcessThreadPtr(NULL),
    _audioDeviceModulePtr(NULL),
    _voiceEngineObserverPtr(NULL),
    _callbackCritSectPtr(NULL),
    _transportPtr(NULL),
    _encryptionPtr(NULL),
    rtp_audioproc_(NULL),
    rx_audioproc_(AudioProcessing::Create(VoEModuleId(instanceId, channelId))),
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
    _average_jitter_buffer_delay_us(0),
    least_required_delay_ms_(0),
    _previousTimestamp(0),
    _recPacketDelayMs(20),
    _RxVadDetection(false),
    _rxApmIsEnabled(false),
    _rxAgcIsEnabled(false),
    _rxNsIsEnabled(false),
    restored_packet_in_use_(false)
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::Channel() - ctor");
    _inbandDtmfQueue.ResetDtmf();
    _inbandDtmfGenerator.Init();
    _outputAudioLevel.Clear();

    RtpRtcp::Configuration configuration;
    configuration.id = VoEModuleId(instanceId, channelId);
    configuration.audio = true;
    configuration.outgoing_transport = this;
    configuration.rtcp_feedback = this;
    configuration.audio_messages = this;
    configuration.receive_statistics = rtp_receive_statistics_.get();

    _rtpRtcpModule.reset(RtpRtcp::CreateRtpRtcp(configuration));
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
    if (audio_coding_->RegisterTransportCallback(NULL) == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                     VoEId(_instanceId,_channelId),
                     "~Channel() failed to de-register transport callback"
                     " (Audio coding module)");
    }
    if (audio_coding_->RegisterVADCallback(NULL) == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                     VoEId(_instanceId,_channelId),
                     "~Channel() failed to de-register VAD callback"
                     " (Audio coding module)");
    }
    // De-register modules in process thread
    if (_moduleProcessThreadPtr->DeRegisterModule(_rtpRtcpModule.get()) == -1)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceVoice,
                     VoEId(_instanceId,_channelId),
                     "~Channel() failed to deregister RTP/RTCP module");
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

int32_t
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
        false);
    if (processThreadFail)
    {
        _engineStatisticsPtr->SetLastError(
            VE_CANNOT_INIT_CHANNEL, kTraceError,
            "Channel::Init() modules not registered");
        return -1;
    }
    // --- ACM initialization

    if ((audio_coding_->InitializeReceiver() == -1) ||
#ifdef WEBRTC_CODEC_AVT
        // out-of-band Dtmf tones are played out by default
        (audio_coding_->SetDtmfPlayoutStatus(true) == -1) ||
#endif
        (audio_coding_->InitializeSender() == -1))
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
    telephone_event_handler_->SetTelephoneEventForwardToDecoder(true);
    // RTCP is enabled by default.
    if (_rtpRtcpModule->SetRTCPStatus(kRtcpCompound) == -1)
    {
        _engineStatisticsPtr->SetLastError(
            VE_RTP_RTCP_MODULE_ERROR, kTraceError,
            "Channel::Init() RTP/RTCP module not initialized");
        return -1;
    }

     // --- Register all permanent callbacks
    const bool fail =
        (audio_coding_->RegisterTransportCallback(this) == -1) ||
        (audio_coding_->RegisterVADCallback(this) == -1);

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
    const uint8_t nSupportedCodecs = AudioCodingModule::NumberOfCodecs();

    for (int idx = 0; idx < nSupportedCodecs; idx++)
    {
        // Open up the RTP/RTCP receiver for all supported codecs
        if ((audio_coding_->Codec(idx, &codec) == -1) ||
            (rtp_receiver_->RegisterReceivePayload(
                codec.plname,
                codec.pltype,
                codec.plfreq,
                codec.channels,
                (codec.rate < 0) ? 0 : codec.rate) == -1))
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
                (audio_coding_->RegisterReceiveCodec(codec) == -1))
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
            if ((audio_coding_->RegisterSendCodec(codec) == -1) ||
                (audio_coding_->RegisterReceiveCodec(codec) == -1) ||
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
            if (audio_coding_->RegisterReceiveCodec(codec) == -1)
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


    if (rx_audioproc_->set_sample_rate_hz(8000))
    {
        _engineStatisticsPtr->SetLastError(
            VE_APM_ERROR, kTraceWarning,
            "Channel::Init() failed to set the sample rate to 8K for"
            " far-end AP module");
    }

    if (rx_audioproc_->set_num_channels(1, 1) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_SOUNDCARD_ERROR, kTraceWarning,
            "Init() failed to set channels for the primary audio stream");
    }

    if (rx_audioproc_->high_pass_filter()->Enable(
        WEBRTC_VOICE_ENGINE_RX_HP_DEFAULT_STATE) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_APM_ERROR, kTraceWarning,
            "Channel::Init() failed to set the high-pass filter for"
            " far-end AP module");
    }

    if (rx_audioproc_->noise_suppression()->set_level(
        (NoiseSuppression::Level)WEBRTC_VOICE_ENGINE_RX_NS_DEFAULT_MODE) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_APM_ERROR, kTraceWarning,
            "Init() failed to set noise reduction level for far-end"
            " AP module");
    }
    if (rx_audioproc_->noise_suppression()->Enable(
        WEBRTC_VOICE_ENGINE_RX_NS_DEFAULT_STATE) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_APM_ERROR, kTraceWarning,
            "Init() failed to set noise reduction state for far-end"
            " AP module");
    }

    if (rx_audioproc_->gain_control()->set_mode(
        (GainControl::Mode)WEBRTC_VOICE_ENGINE_RX_AGC_DEFAULT_MODE) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_APM_ERROR, kTraceWarning,
            "Init() failed to set AGC mode for far-end AP module");
    }
    if (rx_audioproc_->gain_control()->Enable(
        WEBRTC_VOICE_ENGINE_RX_AGC_DEFAULT_STATE) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_APM_ERROR, kTraceWarning,
            "Init() failed to set AGC state for far-end AP module");
    }

    return 0;
}

int32_t
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

int32_t
Channel::UpdateLocalTimeStamp()
{

    _timeStamp += _audioFrame.samples_per_channel_;
    return 0;
}

int32_t
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

int32_t
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

int32_t
Channel::StartSend()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::StartSend()");
    // Resume the previous sequence number which was reset by StopSend().
    // This needs to be done before |_sending| is set to true.
    if (send_sequence_number_)
      SetInitSequenceNumber(send_sequence_number_);

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

int32_t
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

    // Store the sequence number to be able to pick up the same sequence for
    // the next StartSend(). This is needed for restarting device, otherwise
    // it might cause libSRTP to complain about packets being replayed.
    // TODO(xians): Remove this workaround after RtpRtcpModule's refactoring
    // CL is landed. See issue
    // https://code.google.com/p/webrtc/issues/detail?id=2111 .
    send_sequence_number_ = _rtpRtcpModule->SequenceNumber();

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

int32_t
Channel::StartReceiving()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::StartReceiving()");
    if (_receiving)
    {
        return 0;
    }
    _receiving = true;
    _numberOfDiscardedPackets = 0;
    return 0;
}

int32_t
Channel::StopReceiving()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::StopReceiving()");
    if (!_receiving)
    {
        return 0;
    }

    // Recover DTMF detection status.
    telephone_event_handler_->SetTelephoneEventForwardToDecoder(true);
    RegisterReceiveCodecsToRTPModule();
    _receiving = false;
    return 0;
}

int32_t
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
    if (audio_coding_->SetPlayoutMode(playoutMode) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
            "SetNetEQPlayoutMode() failed to set playout mode");
        return -1;
    }
    return 0;
}

int32_t
Channel::GetNetEQPlayoutMode(NetEqModes& mode)
{
    const AudioPlayoutMode playoutMode = audio_coding_->PlayoutMode();
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

int32_t
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

int32_t
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

int32_t
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

int32_t
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

int32_t
Channel::GetSendCodec(CodecInst& codec)
{
    return (audio_coding_->SendCodec(&codec));
}

int32_t
Channel::GetRecCodec(CodecInst& codec)
{
    return (audio_coding_->ReceiveCodec(&codec));
}

int32_t
Channel::SetSendCodec(const CodecInst& codec)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetSendCodec()");

    if (audio_coding_->RegisterSendCodec(codec) != 0)
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

int32_t
Channel::SetVADStatus(bool enableVAD, ACMVADMode mode, bool disableDTX)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetVADStatus(mode=%d)", mode);
    // To disable VAD, DTX must be disabled too
    disableDTX = ((enableVAD == false) ? true : disableDTX);
    if (audio_coding_->SetVAD(!disableDTX, enableVAD, mode) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
            "SetVADStatus() failed to set VAD");
        return -1;
    }
    return 0;
}

int32_t
Channel::GetVADStatus(bool& enabledVAD, ACMVADMode& mode, bool& disabledDTX)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::GetVADStatus");
    if (audio_coding_->VAD(&disabledDTX, &enabledVAD, &mode) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
            "GetVADStatus() failed to get VAD status");
        return -1;
    }
    disabledDTX = !disabledDTX;
    return 0;
}

int32_t
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

        int8_t pltype(-1);
        CodecInst rxCodec = codec;

        // Get payload type for the given codec
        rtp_payload_registry_->ReceivePayloadType(
            rxCodec.plname,
            rxCodec.plfreq,
            rxCodec.channels,
            (rxCodec.rate < 0) ? 0 : rxCodec.rate,
            &pltype);
        rxCodec.pltype = pltype;

        if (rtp_receiver_->DeRegisterReceivePayload(pltype) != 0)
        {
            _engineStatisticsPtr->SetLastError(
                    VE_RTP_RTCP_MODULE_ERROR,
                    kTraceError,
                    "SetRecPayloadType() RTP/RTCP-module deregistration "
                    "failed");
            return -1;
        }
        if (audio_coding_->UnregisterReceiveCodec(rxCodec.pltype) != 0)
        {
            _engineStatisticsPtr->SetLastError(
                VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
                "SetRecPayloadType() ACM deregistration failed - 1");
            return -1;
        }
        return 0;
    }

    if (rtp_receiver_->RegisterReceivePayload(
        codec.plname,
        codec.pltype,
        codec.plfreq,
        codec.channels,
        (codec.rate < 0) ? 0 : codec.rate) != 0)
    {
        // First attempt to register failed => de-register and try again
        rtp_receiver_->DeRegisterReceivePayload(codec.pltype);
        if (rtp_receiver_->RegisterReceivePayload(
            codec.plname,
            codec.pltype,
            codec.plfreq,
            codec.channels,
            (codec.rate < 0) ? 0 : codec.rate) != 0)
        {
            _engineStatisticsPtr->SetLastError(
                VE_RTP_RTCP_MODULE_ERROR, kTraceError,
                "SetRecPayloadType() RTP/RTCP-module registration failed");
            return -1;
        }
    }
    if (audio_coding_->RegisterReceiveCodec(codec) != 0)
    {
        audio_coding_->UnregisterReceiveCodec(codec.pltype);
        if (audio_coding_->RegisterReceiveCodec(codec) != 0)
        {
            _engineStatisticsPtr->SetLastError(
                VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
                "SetRecPayloadType() ACM registration failed - 1");
            return -1;
        }
    }
    return 0;
}

int32_t
Channel::GetRecPayloadType(CodecInst& codec)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::GetRecPayloadType()");
    int8_t payloadType(-1);
    if (rtp_payload_registry_->ReceivePayloadType(
        codec.plname,
        codec.plfreq,
        codec.channels,
        (codec.rate < 0) ? 0 : codec.rate,
        &payloadType) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_RTP_RTCP_MODULE_ERROR, kTraceWarning,
            "GetRecPayloadType() failed to retrieve RX payload type");
        return -1;
    }
    codec.pltype = payloadType;
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::GetRecPayloadType() => pltype=%u", codec.pltype);
    return 0;
}

int32_t
Channel::SetAMREncFormat(AmrMode mode)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetAMREncFormat()");

    // ACM doesn't support AMR
    return -1;
}

int32_t
Channel::SetAMRDecFormat(AmrMode mode)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetAMRDecFormat()");

    // ACM doesn't support AMR
    return -1;
}

int32_t
Channel::SetAMRWbEncFormat(AmrMode mode)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetAMRWbEncFormat()");

    // ACM doesn't support AMR
    return -1;

}

int32_t
Channel::SetAMRWbDecFormat(AmrMode mode)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetAMRWbDecFormat()");

    // ACM doesn't support AMR
    return -1;
}

int32_t
Channel::SetSendCNPayloadType(int type, PayloadFrequencies frequency)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetSendCNPayloadType()");

    CodecInst codec;
    int32_t samplingFreqHz(-1);
    const int kMono = 1;
    if (frequency == kFreq32000Hz)
        samplingFreqHz = 32000;
    else if (frequency == kFreq16000Hz)
        samplingFreqHz = 16000;

    if (audio_coding_->Codec("CN", &codec, samplingFreqHz, kMono) == -1)
    {
        _engineStatisticsPtr->SetLastError(
            VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
            "SetSendCNPayloadType() failed to retrieve default CN codec "
            "settings");
        return -1;
    }

    // Modify the payload type (must be set to dynamic range)
    codec.pltype = type;

    if (audio_coding_->RegisterSendCodec(codec) != 0)
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

int32_t
Channel::SetISACInitTargetRate(int rateBps, bool useFixedFrameSize)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetISACInitTargetRate()");

    CodecInst sendCodec;
    if (audio_coding_->SendCodec(&sendCodec) == -1)
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

    uint8_t initFrameSizeMsec(0);
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
        initFrameSizeMsec = (uint8_t)(sendCodec.pacsize / 16);
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
        initFrameSizeMsec = (uint8_t)(sendCodec.pacsize / 32); // 30ms
    }

    if (audio_coding_->ConfigISACBandwidthEstimator(
        initFrameSizeMsec, rateBps, useFixedFrameSize) == -1)
    {
        _engineStatisticsPtr->SetLastError(
            VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
            "SetISACInitTargetRate() iSAC BWE config failed");
        return -1;
    }

    return 0;
}

int32_t
Channel::SetISACMaxRate(int rateBps)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetISACMaxRate()");

    CodecInst sendCodec;
    if (audio_coding_->SendCodec(&sendCodec) == -1)
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
    if (audio_coding_->SetISACMaxRate(rateBps) == -1)
    {
        _engineStatisticsPtr->SetLastError(
            VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
            "SetISACMaxRate() failed to set max rate");
        return -1;
    }

    return 0;
}

int32_t
Channel::SetISACMaxPayloadSize(int sizeBytes)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetISACMaxPayloadSize()");
    CodecInst sendCodec;
    if (audio_coding_->SendCodec(&sendCodec) == -1)
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

    if (audio_coding_->SetISACMaxPayloadSize(sizeBytes) == -1)
    {
        _engineStatisticsPtr->SetLastError(
            VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
            "SetISACMaxPayloadSize() failed to set max payload size");
        return -1;
    }
    return 0;
}

int32_t Channel::RegisterExternalTransport(Transport& transport)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::RegisterExternalTransport()");

    CriticalSectionScoped cs(&_callbackCritSect);

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

int32_t
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
    _transportPtr = NULL;
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "DeRegisterExternalTransport() all transport is disabled");
    return 0;
}

int32_t Channel::ReceivedRTPPacket(const int8_t* data, int32_t length) {
  WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
               "Channel::ReceivedRTPPacket()");

  // Store playout timestamp for the received RTP packet
  UpdatePlayoutTimestamp(false);

  // Dump the RTP packet to a file (if RTP dump is enabled).
  if (_rtpDumpIn.DumpPacket((const uint8_t*)data,
                            (uint16_t)length) == -1) {
    WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                 VoEId(_instanceId,_channelId),
                 "Channel::SendPacket() RTP dump to input file failed");
  }
  const uint8_t* received_packet = reinterpret_cast<const uint8_t*>(data);
  RTPHeader header;
  if (!rtp_header_parser_->Parse(received_packet, length, &header)) {
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVoice, _channelId,
                 "Incoming packet: invalid RTP header");
    return -1;
  }
  header.payload_type_frequency =
      rtp_payload_registry_->GetPayloadTypeFrequency(header.payloadType);
  if (header.payload_type_frequency < 0)
    return -1;
  rtp_receive_statistics_->IncomingPacket(header, length,
                                          IsPacketRetransmitted(header));
  rtp_payload_registry_->SetIncomingPayloadType(header);
  return ReceivePacket(received_packet, length, header,
                       IsPacketInOrder(header)) ? 0 : -1;
}

bool Channel::ReceivePacket(const uint8_t* packet,
                            int packet_length,
                            const RTPHeader& header,
                            bool in_order) {
  if (rtp_payload_registry_->IsEncapsulated(header)) {
    return HandleEncapsulation(packet, packet_length, header);
  }
  const uint8_t* payload = packet + header.headerLength;
  int payload_length = packet_length - header.headerLength;
  assert(payload_length >= 0);
  PayloadUnion payload_specific;
  if (!rtp_payload_registry_->GetPayloadSpecifics(header.payloadType,
                                                  &payload_specific)) {
    return false;
  }
  return rtp_receiver_->IncomingRtpPacket(header, payload, payload_length,
                                          payload_specific, in_order);
}

bool Channel::HandleEncapsulation(const uint8_t* packet,
                                  int packet_length,
                                  const RTPHeader& header) {
  if (!rtp_payload_registry_->IsRtx(header))
    return false;

  // Remove the RTX header and parse the original RTP header.
  if (packet_length < header.headerLength)
    return false;
  if (packet_length > kVoiceEngineMaxIpPacketSizeBytes)
    return false;
  if (restored_packet_in_use_) {
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVoice, _channelId,
                 "Multiple RTX headers detected, dropping packet");
    return false;
  }
  uint8_t* restored_packet_ptr = restored_packet_;
  if (!rtp_payload_registry_->RestoreOriginalPacket(
      &restored_packet_ptr, packet, &packet_length, rtp_receiver_->SSRC(),
      header)) {
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVoice, _channelId,
                 "Incoming RTX packet: invalid RTP header");
    return false;
  }
  restored_packet_in_use_ = true;
  bool ret = OnRecoveredPacket(restored_packet_ptr, packet_length);
  restored_packet_in_use_ = false;
  return ret;
}

bool Channel::IsPacketInOrder(const RTPHeader& header) const {
  StreamStatistician* statistician =
      rtp_receive_statistics_->GetStatistician(header.ssrc);
  if (!statistician)
    return false;
  return statistician->IsPacketInOrder(header.sequenceNumber);
}

bool Channel::IsPacketRetransmitted(const RTPHeader& header) const {
  // Retransmissions are handled separately if RTX is enabled.
  if (rtp_payload_registry_->RtxEnabled())
    return false;
  StreamStatistician* statistician =
      rtp_receive_statistics_->GetStatistician(header.ssrc);
  if (!statistician)
    return false;
  // Check if this is a retransmission.
  uint16_t min_rtt = 0;
  _rtpRtcpModule->RTT(rtp_receiver_->SSRC(), NULL, NULL, &min_rtt, NULL);
  return !IsPacketInOrder(header) &&
      statistician->IsRetransmitOfOldPacket(header, min_rtt);
}

int32_t Channel::ReceivedRTCPPacket(const int8_t* data, int32_t length) {
  WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
               "Channel::ReceivedRTCPPacket()");
  // Store playout timestamp for the received RTCP packet
  UpdatePlayoutTimestamp(true);

  // Dump the RTCP packet to a file (if RTP dump is enabled).
  if (_rtpDumpIn.DumpPacket((const uint8_t*)data,
                            (uint16_t)length) == -1) {
    WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                 VoEId(_instanceId,_channelId),
                 "Channel::SendPacket() RTCP dump to input file failed");
  }

  // Deliver RTCP packet to RTP/RTCP module for parsing
  if (_rtpRtcpModule->IncomingRtcpPacket((const uint8_t*)data,
                                         (uint16_t)length) == -1) {
    _engineStatisticsPtr->SetLastError(
        VE_SOCKET_TRANSPORT_MODULE_ERROR, kTraceWarning,
        "Channel::IncomingRTPPacket() RTCP packet is invalid");
  }
  return 0;
}

int Channel::StartPlayingFileLocally(const char* fileName,
                                     bool loop,
                                     FileFormats format,
                                     int startPosition,
                                     float volumeScaling,
                                     int stopPosition,
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

        const uint32_t notificationTime(0);

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
                                     FileFormats format,
                                     int startPosition,
                                     float volumeScaling,
                                     int stopPosition,
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

      const uint32_t notificationTime(0);

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

    return (int32_t)_outputFilePlaying;
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

int Channel::ScaleLocalFilePlayout(float scale)
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

    uint32_t position;

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
                                          bool loop,
                                          FileFormats format,
                                          int startPosition,
                                          float volumeScaling,
                                          int stopPosition,
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

    const uint32_t notificationTime(0);

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
                                          FileFormats format,
                                          int startPosition,
                                          float volumeScaling,
                                          int stopPosition,
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

    const uint32_t notificationTime(0);

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

int Channel::ScaleFileAsMicrophonePlayout(float scale)
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
    const uint32_t notificationTime(0); // Not supported in VoE
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
    const uint32_t notificationTime(0); // Not supported in VoE
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
Channel::GetSpeechOutputLevel(uint32_t& level) const
{
    int8_t currentLevel = _outputAudioLevel.Level();
    level = static_cast<int32_t> (currentLevel);
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
               VoEId(_instanceId,_channelId),
               "GetSpeechOutputLevel() => level=%u", level);
    return 0;
}

int
Channel::GetSpeechOutputLevelFullRange(uint32_t& level) const
{
    int16_t currentLevel = _outputAudioLevel.LevelFullRange();
    level = static_cast<int32_t> (currentLevel);
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
    if (audio_coding_->SetDtmfPlayoutStatus(enable) != 0)
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
    return audio_coding_->DtmfPlayoutStatus();
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
    CodecInst codec = {};
    codec.plfreq = 8000;
    codec.pltype = type;
    memcpy(codec.plname, "telephone-event", 16);
    if (_rtpRtcpModule->RegisterSendPayload(codec) != 0)
    {
        _rtpRtcpModule->DeRegisterSendPayload(codec.pltype);
        if (_rtpRtcpModule->RegisterSendPayload(codec) != 0) {
            _engineStatisticsPtr->SetLastError(
                VE_RTP_RTCP_MODULE_ERROR, kTraceError,
                "SetSendTelephoneEventPayloadType() failed to register send"
                "payload type");
            return -1;
        }
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
Channel::SetRxAgcStatus(bool enable, AgcModes mode)
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
            agcMode = rx_audioproc_->gain_control()->mode();
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

    if (rx_audioproc_->gain_control()->set_mode(agcMode) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_APM_ERROR, kTraceError,
            "SetRxAgcStatus() failed to set Agc mode");
        return -1;
    }
    if (rx_audioproc_->gain_control()->Enable(enable) != 0)
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

    bool enable = rx_audioproc_->gain_control()->is_enabled();
    GainControl::Mode agcMode =
        rx_audioproc_->gain_control()->mode();

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
Channel::SetRxAgcConfig(AgcConfig config)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetRxAgcConfig()");

    if (rx_audioproc_->gain_control()->set_target_level_dbfs(
        config.targetLeveldBOv) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_APM_ERROR, kTraceError,
            "SetRxAgcConfig() failed to set target peak |level|"
            "(or envelope) of the Agc");
        return -1;
    }
    if (rx_audioproc_->gain_control()->set_compression_gain_db(
        config.digitalCompressionGaindB) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_APM_ERROR, kTraceError,
            "SetRxAgcConfig() failed to set the range in |gain| the"
            " digital compression stage may apply");
        return -1;
    }
    if (rx_audioproc_->gain_control()->enable_limiter(
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
        rx_audioproc_->gain_control()->target_level_dbfs();
    config.digitalCompressionGaindB =
        rx_audioproc_->gain_control()->compression_gain_db();
    config.limiterEnable =
        rx_audioproc_->gain_control()->is_limiter_enabled();

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
Channel::SetRxNsStatus(bool enable, NsModes mode)
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
            nsLevel = rx_audioproc_->noise_suppression()->level();
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

    if (rx_audioproc_->noise_suppression()->set_level(nsLevel)
        != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_APM_ERROR, kTraceError,
            "SetRxAgcStatus() failed to set Ns level");
        return -1;
    }
    if (rx_audioproc_->noise_suppression()->Enable(enable) != 0)
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
        rx_audioproc_->noise_suppression()->is_enabled();
    NoiseSuppression::Level ncLevel =
        rx_audioproc_->noise_suppression()->level();

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
    ssrc = rtp_receiver_->SSRC();
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
    uint32_t arrOfCSRC[kRtpCsrcSize];
    int32_t CSRCs(0);
    CSRCs = _rtpRtcpModule->CSRCs(arrOfCSRC);
    if (CSRCs > 0)
    {
        memcpy(arrCSRC, arrOfCSRC, CSRCs * sizeof(uint32_t));
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
  if (rtp_audioproc_.get() == NULL) {
    rtp_audioproc_.reset(AudioProcessing::Create(VoEModuleId(_instanceId,
                                                             _channelId)));
  }

  if (rtp_audioproc_->level_estimator()->Enable(enable) !=
      AudioProcessing::kNoError) {
    _engineStatisticsPtr->SetLastError(VE_APM_ERROR, kTraceError,
        "Failed to enable AudioProcessing::level_estimator()");
    return -1;
  }

  _includeAudioLevelIndication = enable;
  if (enable) {
    rtp_header_parser_->RegisterRtpHeaderExtension(kRtpExtensionAudioLevel,
                                                   ID);
  } else {
    rtp_header_parser_->DeregisterRtpHeaderExtension(kRtpExtensionAudioLevel);
  }
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
    const uint32_t remoteSSRC = rtp_receiver_->SSRC();
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
    playoutTimestamp = playout_timestamp_rtcp_;

    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
                 VoEId(_instanceId, _channelId),
                 "GetRemoteRTCPData() => playoutTimestamp=%lu",
                 playout_timestamp_rtcp_);

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

        uint32_t remoteSSRC = rtp_receiver_->SSRC();
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
Channel::SendApplicationDefinedRTCPPacket(unsigned char subType,
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
    // The jitter statistics is updated for each received RTP packet and is
    // based on received packets.
    StreamStatistician::Statistics statistics;
    StreamStatistician* statistician =
        rtp_receive_statistics_->GetStatistician(rtp_receiver_->SSRC());
    if (!statistician || !statistician->GetStatistics(
        &statistics, _rtpRtcpModule->RTCP() == kRtcpOff)) {
      _engineStatisticsPtr->SetLastError(
          VE_CANNOT_RETRIEVE_RTP_STAT, kTraceWarning,
          "GetRTPStatistics() failed to read RTP statistics from the "
          "RTP/RTCP module");
    }

    const int32_t playoutFrequency = audio_coding_->PlayoutFrequency();
    if (playoutFrequency > 0)
    {
        // Scale RTP statistics given the current playout frequency
        maxJitterMs = statistics.max_jitter / (playoutFrequency / 1000);
        averageJitterMs = statistics.jitter / (playoutFrequency / 1000);
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
    // --- Part one of the final structure (four values)

    // The jitter statistics is updated for each received RTP packet and is
    // based on received packets.
    StreamStatistician::Statistics statistics;
    StreamStatistician* statistician =
        rtp_receive_statistics_->GetStatistician(rtp_receiver_->SSRC());
    if (!statistician || !statistician->GetStatistics(
        &statistics, _rtpRtcpModule->RTCP() == kRtcpOff)) {
      _engineStatisticsPtr->SetLastError(
          VE_CANNOT_RETRIEVE_RTP_STAT, kTraceWarning,
          "GetRTPStatistics() failed to read RTP statistics from the "
          "RTP/RTCP module");
    }

    stats.fractionLost = statistics.fraction_lost;
    stats.cumulativeLost = statistics.cumulative_lost;
    stats.extendedMax = statistics.extended_max_sequence_number;
    stats.jitterSamples = statistics.jitter;

    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
                 VoEId(_instanceId, _channelId),
                 "GetRTPStatistics() => fractionLost=%lu, cumulativeLost=%lu,"
                 " extendedMax=%lu, jitterSamples=%li)",
                 stats.fractionLost, stats.cumulativeLost, stats.extendedMax,
                 stats.jitterSamples);

    // --- Part two of the final structure (one value)

    uint16_t RTT(0);
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
        uint32_t remoteSSRC = rtp_receiver_->SSRC();
        if (remoteSSRC > 0)
        {
            uint16_t avgRTT(0);
            uint16_t maxRTT(0);
            uint16_t minRTT(0);

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

    uint32_t bytesSent(0);
    uint32_t packetsSent(0);
    uint32_t bytesReceived(0);
    uint32_t packetsReceived(0);

    if (statistician) {
      statistician->GetDataCounters(&bytesReceived, &packetsReceived);
    }

    if (_rtpRtcpModule->DataCountersRTP(&bytesSent,
                                        &packetsSent) != 0)
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

  if (enable) {
    if (redPayloadtype < 0 || redPayloadtype > 127) {
      _engineStatisticsPtr->SetLastError(
          VE_PLTYPE_ERROR, kTraceError,
          "SetFECStatus() invalid RED payload type");
      return -1;
    }

    if (SetRedPayloadType(redPayloadtype) < 0) {
      _engineStatisticsPtr->SetLastError(
          VE_CODEC_ERROR, kTraceError,
          "SetSecondarySendCodec() Failed to register RED ACM");
      return -1;
    }
  }

  if (audio_coding_->SetFECStatus(enable) != 0) {
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
    enabled = audio_coding_->FECStatus();
    if (enabled)
    {
        int8_t payloadType(0);
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

void Channel::SetNACKStatus(bool enable, int maxNumberOfPackets) {
  // None of these functions can fail.
  _rtpRtcpModule->SetStorePacketsStatus(enable, maxNumberOfPackets);
  rtp_receive_statistics_->SetMaxReorderingThreshold(maxNumberOfPackets);
  rtp_receiver_->SetNACKStatus(enable ? kNackRtcp : kNackOff);
  if (enable)
    audio_coding_->EnableNack(maxNumberOfPackets);
  else
    audio_coding_->DisableNack();
}

// Called when we are missing one or more packets.
int Channel::ResendPackets(const uint16_t* sequence_numbers, int length) {
  return _rtpRtcpModule->SendNACK(sequence_numbers, length);
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
                                        (const uint8_t*) payloadData,
                                        payloadSize) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_RTP_RTCP_MODULE_ERROR, kTraceError,
            "InsertExtraRTPPacket() failed to send extra RTP packet");
        return -1;
    }

    return 0;
}

uint32_t
Channel::Demultiplex(const AudioFrame& audioFrame)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::Demultiplex()");
    _audioFrame.CopyFrom(audioFrame);
    _audioFrame.id_ = _channelId;
    return 0;
}

// TODO(xians): This method borrows quite some code from
// TransmitMixer::GenerateAudioFrame(), refactor these two methods and reduce
// code duplication.
void Channel::Demultiplex(const int16_t* audio_data,
                          int sample_rate,
                          int number_of_frames,
                          int number_of_channels) {
  // The highest sample rate that WebRTC supports for mono audio is 96kHz.
  static const int kMaxNumberOfFrames = 960;
  assert(number_of_frames <= kMaxNumberOfFrames);

  // Get the send codec information for doing resampling or downmixing later on.
  CodecInst codec;
  GetSendCodec(codec);
  assert(codec.channels == 1 || codec.channels == 2);
  int support_sample_rate = std::min(32000,
                                     std::min(sample_rate, codec.plfreq));

  // Downmix the data to mono if needed.
  const int16_t* audio_ptr = audio_data;
  if (number_of_channels == 2 && codec.channels == 1) {
    if (!mono_recording_audio_.get())
      mono_recording_audio_.reset(new int16_t[kMaxNumberOfFrames]);

    AudioFrameOperations::StereoToMono(audio_data, number_of_frames,
                                       mono_recording_audio_.get());
    audio_ptr = mono_recording_audio_.get();
  }

  // Resample the data to the sample rate that the codec is using.
  if (input_resampler_.InitializeIfNeeded(sample_rate,
                                          support_sample_rate,
                                          codec.channels)) {
    WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId, -1),
                 "Channel::Demultiplex() unable to resample");
    return;
  }

  int out_length = input_resampler_.Resample(audio_ptr,
                                             number_of_frames * codec.channels,
                                             _audioFrame.data_,
                                             AudioFrame::kMaxDataSizeSamples);
  if (out_length == -1) {
    WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId, -1),
                 "Channel::Demultiplex() resampling failed");
    return;
  }

  _audioFrame.samples_per_channel_ = out_length / codec.channels;
  _audioFrame.timestamp_ = -1;
  _audioFrame.sample_rate_hz_ = support_sample_rate;
  _audioFrame.speech_type_ = AudioFrame::kNormalSpeech;
  _audioFrame.vad_activity_ = AudioFrame::kVadUnknown;
  _audioFrame.num_channels_ = codec.channels;
  _audioFrame.id_ = _channelId;
}

uint32_t
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
               (int16_t*)_audioFrame.data_,
                _audioFrame.samples_per_channel_,
                _audioFrame.sample_rate_hz_,
                isStereo);
        }
    }

    InsertInbandDtmfTone();

    if (_includeAudioLevelIndication)
    {
        if (rtp_audioproc_->set_sample_rate_hz(_audioFrame.sample_rate_hz_) !=
            AudioProcessing::kNoError)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                         VoEId(_instanceId, _channelId),
                         "Error setting AudioProcessing sample rate");
            return -1;
        }

        if (rtp_audioproc_->set_num_channels(_audioFrame.num_channels_,
                                             _audioFrame.num_channels_) !=
            AudioProcessing::kNoError)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                         VoEId(_instanceId, _channelId),
                         "Error setting AudioProcessing channels");
            return -1;
        }

        // Performs level analysis only; does not affect the signal.
        rtp_audioproc_->ProcessStream(&_audioFrame);
    }

    return 0;
}

uint32_t
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
    if (audio_coding_->Add10MsData((AudioFrame&)_audioFrame) != 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,_channelId),
                     "Channel::EncodeAndSend() ACM encoding failed");
        return -1;
    }

    _timeStamp += _audioFrame.samples_per_channel_;

    // --- Encode if complete frame is ready

    // This call will trigger AudioPacketizationCallback::SendData if encoding
    // is done and payload is ready for packetization and transmission.
    return audio_coding_->Process();
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
    uint32_t remoteSSRC(0);
    remoteSSRC = rtp_receiver_->SSRC();
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

    uint32_t remoteSSRC;
    uint16_t RTT;
    uint16_t avgRTT;
    uint16_t maxRTT;
    uint16_t minRTT;
    // The remote SSRC will be zero if no RTP packet has been received.
    remoteSSRC = rtp_receiver_->SSRC();
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
    ACMNetworkStatistics acm_stats;
    int return_value = audio_coding_->NetworkStatistics(&acm_stats);
    if (return_value >= 0) {
      memcpy(&stats, &acm_stats, sizeof(NetworkStatistics));
    }
    return return_value;
}

bool Channel::GetDelayEstimate(int* jitter_buffer_delay_ms,
                               int* playout_buffer_delay_ms) const {
  if (_average_jitter_buffer_delay_us == 0) {
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::GetDelayEstimate() no valid estimate.");
    return false;
  }
  *jitter_buffer_delay_ms = (_average_jitter_buffer_delay_us + 500) / 1000 +
      _recPacketDelayMs;
  *playout_buffer_delay_ms = playout_delay_ms_;
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
               "Channel::GetDelayEstimate()");
  return true;
}

int Channel::SetInitialPlayoutDelay(int delay_ms)
{
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
               "Channel::SetInitialPlayoutDelay()");
  if ((delay_ms < kVoiceEngineMinMinPlayoutDelayMs) ||
      (delay_ms > kVoiceEngineMaxMinPlayoutDelayMs))
  {
    _engineStatisticsPtr->SetLastError(
        VE_INVALID_ARGUMENT, kTraceError,
        "SetInitialPlayoutDelay() invalid min delay");
    return -1;
  }
  if (audio_coding_->SetInitialPlayoutDelay(delay_ms) != 0)
  {
    _engineStatisticsPtr->SetLastError(
        VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
        "SetInitialPlayoutDelay() failed to set min playout delay");
    return -1;
  }
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
    if (audio_coding_->SetMinimumPlayoutDelay(delayMs) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
            "SetMinimumPlayoutDelay() failed to set min playout delay");
        return -1;
    }
    return 0;
}

void Channel::UpdatePlayoutTimestamp(bool rtcp) {
  uint32_t playout_timestamp = 0;

  if (audio_coding_->PlayoutTimestamp(&playout_timestamp) == -1)  {
    WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::UpdatePlayoutTimestamp() failed to read playout"
                 " timestamp from the ACM");
    _engineStatisticsPtr->SetLastError(
        VE_CANNOT_RETRIEVE_VALUE, kTraceError,
        "UpdatePlayoutTimestamp() failed to retrieve timestamp");
    return;
  }

  uint16_t delay_ms = 0;
  if (_audioDeviceModulePtr->PlayoutDelay(&delay_ms) == -1) {
    WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::UpdatePlayoutTimestamp() failed to read playout"
                 " delay from the ADM");
    _engineStatisticsPtr->SetLastError(
        VE_CANNOT_RETRIEVE_VALUE, kTraceError,
        "UpdatePlayoutTimestamp() failed to retrieve playout delay");
    return;
  }

  int32_t playout_frequency = audio_coding_->PlayoutFrequency();
  CodecInst current_recive_codec;
  if (audio_coding_->ReceiveCodec(&current_recive_codec) == 0) {
    if (STR_CASE_CMP("G722", current_recive_codec.plname) == 0) {
      playout_frequency = 8000;
    } else if (STR_CASE_CMP("opus", current_recive_codec.plname) == 0) {
      playout_frequency = 48000;
    }
  }

  // Remove the playout delay.
  playout_timestamp -= (delay_ms * (playout_frequency / 1000));

  WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
               "Channel::UpdatePlayoutTimestamp() => playoutTimestamp = %lu",
               playout_timestamp);

  if (rtcp) {
    playout_timestamp_rtcp_ = playout_timestamp;
  } else {
    playout_timestamp_rtp_ = playout_timestamp;
  }
  playout_delay_ms_ = delay_ms;
}

int Channel::GetPlayoutTimestamp(unsigned int& timestamp) {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
               "Channel::GetPlayoutTimestamp()");
  if (playout_timestamp_rtp_ == 0)  {
    _engineStatisticsPtr->SetLastError(
        VE_CANNOT_RETRIEVE_VALUE, kTraceError,
        "GetPlayoutTimestamp() failed to retrieve timestamp");
    return -1;
  }
  timestamp = playout_timestamp_rtp_;
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
Channel::GetRtpRtcp(RtpRtcp** rtpRtcpModule, RtpReceiver** rtp_receiver) const
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::GetRtpRtcp()");
    *rtpRtcpModule = _rtpRtcpModule.get();
    *rtp_receiver = rtp_receiver_.get();
    return 0;
}

// TODO(andrew): refactor Mix functions here and in transmit_mixer.cc to use
// a shared helper.
int32_t
Channel::MixOrReplaceAudioWithFile(int mixingFrequency)
{
    scoped_array<int16_t> fileBuffer(new int16_t[640]);
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

int32_t
Channel::MixAudioWithFile(AudioFrame& audioFrame,
                          int mixingFrequency)
{
    assert(mixingFrequency <= 32000);

    scoped_array<int16_t> fileBuffer(new int16_t[640]);
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
        int8_t eventCode(0);
        uint16_t lengthMs(0);
        uint8_t attenuationDb(0);

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
        uint16_t frequency(0);
        _inbandDtmfGenerator.GetSampleRate(frequency);

        if (frequency != _audioFrame.sample_rate_hz_)
        {
            // Update sample rate of Dtmf tone since the mixing frequency
            // has changed.
            _inbandDtmfGenerator.SetSampleRate(
                (uint16_t) (_audioFrame.sample_rate_hz_));
            // Reset the tone to be added taking the new sample rate into
            // account.
            _inbandDtmfGenerator.ResetTone();
        }

        int16_t toneBuffer[320];
        uint16_t toneSamples(0);
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
                const int index = sample * _audioFrame.num_channels_ + channel;
                _audioFrame.data_[index] = toneBuffer[sample];
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
    return 0;
}

int32_t
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

// Called for incoming RTP packets after successful RTP header parsing.
void Channel::UpdatePacketDelay(uint32_t rtp_timestamp,
                                uint16_t sequence_number) {
  WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
               "Channel::UpdatePacketDelay(timestamp=%lu, sequenceNumber=%u)",
               rtp_timestamp, sequence_number);

  // Get frequency of last received payload
  int rtp_receive_frequency = audio_coding_->ReceiveFrequency();

  CodecInst current_receive_codec;
  if (audio_coding_->ReceiveCodec(&current_receive_codec) != 0) {
    return;
  }

  // Update the least required delay.
  least_required_delay_ms_ = audio_coding_->LeastRequiredDelayMs();

  if (STR_CASE_CMP("G722", current_receive_codec.plname) == 0) {
    // Even though the actual sampling rate for G.722 audio is
    // 16,000 Hz, the RTP clock rate for the G722 payload format is
    // 8,000 Hz because that value was erroneously assigned in
    // RFC 1890 and must remain unchanged for backward compatibility.
    rtp_receive_frequency = 8000;
  } else if (STR_CASE_CMP("opus", current_receive_codec.plname) == 0) {
    // We are resampling Opus internally to 32,000 Hz until all our
    // DSP routines can operate at 48,000 Hz, but the RTP clock
    // rate for the Opus payload format is standardized to 48,000 Hz,
    // because that is the maximum supported decoding sampling rate.
    rtp_receive_frequency = 48000;
  }

  // playout_timestamp_rtp_ updated in UpdatePlayoutTimestamp for every incoming
  // packet.
  uint32_t timestamp_diff_ms = (rtp_timestamp - playout_timestamp_rtp_) /
      (rtp_receive_frequency / 1000);

  uint16_t packet_delay_ms = (rtp_timestamp - _previousTimestamp) /
      (rtp_receive_frequency / 1000);

  _previousTimestamp = rtp_timestamp;

  if (timestamp_diff_ms > (2 * kVoiceEngineMaxMinPlayoutDelayMs)) {
    timestamp_diff_ms = 0;
  }

  if (timestamp_diff_ms == 0) return;

  if (packet_delay_ms >= 10 && packet_delay_ms <= 60) {
    _recPacketDelayMs = packet_delay_ms;
  }

  if (_average_jitter_buffer_delay_us == 0) {
    _average_jitter_buffer_delay_us = timestamp_diff_ms * 1000;
    return;
  }

  // Filter average delay value using exponential filter (alpha is
  // 7/8). We derive 1000 *_average_jitter_buffer_delay_us here (reduces
  // risk of rounding error) and compensate for it in GetDelayEstimate()
  // later.
  _average_jitter_buffer_delay_us = (_average_jitter_buffer_delay_us * 7 +
      1000 * timestamp_diff_ms + 500) / 8;
}

void
Channel::RegisterReceiveCodecsToRTPModule()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::RegisterReceiveCodecsToRTPModule()");


    CodecInst codec;
    const uint8_t nSupportedCodecs = AudioCodingModule::NumberOfCodecs();

    for (int idx = 0; idx < nSupportedCodecs; idx++)
    {
        // Open up the RTP/RTCP receiver for all supported codecs
        if ((audio_coding_->Codec(idx, &codec) == -1) ||
            (rtp_receiver_->RegisterReceivePayload(
                codec.plname,
                codec.pltype,
                codec.plfreq,
                codec.channels,
                (codec.rate < 0) ? 0 : codec.rate) == -1))
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
  // Register the (possibly new) frame parameters.
  if (rx_audioproc_->set_sample_rate_hz(frame.sample_rate_hz_) != 0) {
    LOG_FERR1(LS_WARNING, set_sample_rate_hz, frame.sample_rate_hz_);
  }
  if (rx_audioproc_->set_num_channels(frame.num_channels_,
                                      frame.num_channels_) != 0) {
    LOG_FERR1(LS_WARNING, set_num_channels, frame.num_channels_);
  }
  if (rx_audioproc_->ProcessStream(&frame) != 0) {
    LOG_FERR0(LS_WARNING, ProcessStream);
  }
  return 0;
}

int Channel::SetSecondarySendCodec(const CodecInst& codec,
                                   int red_payload_type) {
  // Sanity check for payload type.
  if (red_payload_type < 0 || red_payload_type > 127) {
    _engineStatisticsPtr->SetLastError(
        VE_PLTYPE_ERROR, kTraceError,
        "SetRedPayloadType() invalid RED payload type");
    return -1;
  }

  if (SetRedPayloadType(red_payload_type) < 0) {
    _engineStatisticsPtr->SetLastError(
        VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
        "SetSecondarySendCodec() Failed to register RED ACM");
    return -1;
  }
  if (audio_coding_->RegisterSecondarySendCodec(codec) < 0) {
    _engineStatisticsPtr->SetLastError(
        VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
        "SetSecondarySendCodec() Failed to register secondary send codec in "
        "ACM");
    return -1;
  }

  return 0;
}

void Channel::RemoveSecondarySendCodec() {
  audio_coding_->UnregisterSecondarySendCodec();
}

int Channel::GetSecondarySendCodec(CodecInst* codec) {
  if (audio_coding_->SecondarySendCodec(codec) < 0) {
    _engineStatisticsPtr->SetLastError(
        VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
        "GetSecondarySendCodec() Failed to get secondary sent codec from ACM");
    return -1;
  }
  return 0;
}

// Assuming this method is called with valid payload type.
int Channel::SetRedPayloadType(int red_payload_type) {
  CodecInst codec;
  bool found_red = false;

  // Get default RED settings from the ACM database
  const int num_codecs = AudioCodingModule::NumberOfCodecs();
  for (int idx = 0; idx < num_codecs; idx++) {
    audio_coding_->Codec(idx, &codec);
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
  if (audio_coding_->RegisterSendCodec(codec) < 0) {
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

}  // namespace voe
}  // namespace webrtc
