/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstdlib> // srand

#include "rtp_sender.h"

#include "critical_section_wrapper.h"
#include "trace.h"

#include "rtp_packet_history.h"
#include "rtp_sender_audio.h"
#include "rtp_sender_video.h"

namespace webrtc {
RTPSender::RTPSender(const WebRtc_Word32 id,
                     const bool audio,
                     RtpRtcpClock* clock) :
    Bitrate(clock),
    _id(id),
    _audioConfigured(audio),
    _audio(NULL),
    _video(NULL),
    _sendCritsect(CriticalSectionWrapper::CreateCriticalSection()),
    _transportCritsect(CriticalSectionWrapper::CreateCriticalSection()),

    _transport(NULL),

    _sendingMedia(true), // Default to sending media

    _maxPayloadLength(IP_PACKET_SIZE-28), // default is IP/UDP
    _targetSendBitrate(0),
    _packetOverHead(28),

    _payloadType(-1),
    _payloadTypeMap(),

    _rtpHeaderExtensionMap(),
    _transmissionTimeOffset(0),

    // NACK
    _nackByteCountTimes(),
    _nackByteCount(),
    _nackBitrate(clock),

    _packetHistory(new RTPPacketHistory(clock)),
    _sendBucket(),
    _timeLastSendToNetworkUpdate(clock->GetTimeInMS()),
    _transmissionSmoothing(false),

    // statistics
    _packetsSent(0),
    _payloadBytesSent(0),

    // RTP variables
    _startTimeStampForced(false),
    _startTimeStamp(0),
    _ssrcDB(*SSRCDatabase::GetSSRCDatabase()),
    _remoteSSRC(0),
    _sequenceNumberForced(false),
    _sequenceNumber(0),
    _sequenceNumberRTX(0),
    _ssrcForced(false),
    _ssrc(0),
    _timeStamp(0),
    _CSRCs(0),
    _CSRC(),
    _includeCSRCs(true),
    _RTX(false),
    _ssrcRTX(0)
{
    memset(_nackByteCountTimes, 0, sizeof(_nackByteCountTimes));
    memset(_nackByteCount, 0, sizeof(_nackByteCount));

    memset(_CSRC, 0, sizeof(_CSRC));

    // we need to seed the random generator, otherwise we get 26500 each time, hardly a random value :)
    srand( (WebRtc_UWord32)_clock.GetTimeInMS() );

    _ssrc = _ssrcDB.CreateSSRC(); // can't be 0

    if(audio)
    {
        _audio = new RTPSenderAudio(id, &_clock, this);
    } else
    {
        _video = new RTPSenderVideo(id, &_clock, this);
    }
    WEBRTC_TRACE(kTraceMemory, kTraceRtpRtcp, id, "%s created", __FUNCTION__);
}

RTPSender::~RTPSender() {
  if(_remoteSSRC != 0) {
    _ssrcDB.ReturnSSRC(_remoteSSRC);
  }
  _ssrcDB.ReturnSSRC(_ssrc);

  SSRCDatabase::ReturnSSRCDatabase();
  delete _sendCritsect;
  delete _transportCritsect;
  while (!_payloadTypeMap.empty()) {
    std::map<WebRtc_Word8, ModuleRTPUtility::Payload*>::iterator it =
        _payloadTypeMap.begin();
    delete it->second;
    _payloadTypeMap.erase(it);
  }
  delete _packetHistory;
  delete _audio;
  delete _video;

  WEBRTC_TRACE(kTraceMemory, kTraceRtpRtcp, _id, "%s deleted", __FUNCTION__);
}
/*
WebRtc_Word32
RTPSender::Init(const WebRtc_UWord32 remoteSSRC)
{
    CriticalSectionScoped cs(_sendCritsect);

    // reset to default generation
    _ssrcForced = false;
    _startTimeStampForced = false;

    // register a remote SSRC if we have it to avoid collisions
    if(remoteSSRC != 0)
    {
        if(_ssrc == remoteSSRC)
        {
            // collision detected
            _ssrc = _ssrcDB.CreateSSRC(); // can't be 0
        }
        _remoteSSRC = remoteSSRC;
        _ssrcDB.RegisterSSRC(remoteSSRC);
    }
    _sequenceNumber = rand() / (RAND_MAX / MAX_INIT_RTP_SEQ_NUMBER);
    _sequenceNumberRTX = rand() / (RAND_MAX / MAX_INIT_RTP_SEQ_NUMBER);
    _packetsSent = 0;
    _payloadBytesSent = 0;
    _packetOverHead = 28;

    _rtpHeaderExtensionMap.Erase();

    while (!_payloadTypeMap.empty()) {
      std::map<WebRtc_Word8, ModuleRTPUtility::Payload*>::iterator it =
          _payloadTypeMap.begin();
      delete it->second;
      _payloadTypeMap.erase(it);
    }

    memset(_CSRC, 0, sizeof(_CSRC));

    memset(_nackByteCount, 0, sizeof(_nackByteCount));
    memset(_nackByteCountTimes, 0, sizeof(_nackByteCountTimes));
    _nackBitrate.Init();

    SetStorePacketsStatus(false, 0);
    _sendBucket.Reset();

    Bitrate::Init();

    if(_audioConfigured)
    {
        _audio->Init();
    } else
    {
        _video->Init();
    }
    return(0);
}
*/

void RTPSender::SetTargetSendBitrate(const WebRtc_UWord32 bits) {
  _targetSendBitrate = static_cast<uint16_t>(bits / 1000);
}

WebRtc_UWord16
RTPSender::ActualSendBitrateKbit() const
{
    return (WebRtc_UWord16) (Bitrate::BitrateNow()/1000);
}

WebRtc_UWord32
RTPSender::VideoBitrateSent() const {
  if (_video)
    return _video->VideoBitrateSent();
  else
    return 0;
}

WebRtc_UWord32
RTPSender::FecOverheadRate() const {
  if (_video)
    return _video->FecOverheadRate();
  else
    return 0;
}

WebRtc_UWord32
RTPSender::NackOverheadRate() const {
  return _nackBitrate.BitrateLast();
}

WebRtc_Word32
RTPSender::SetTransmissionTimeOffset(
    const WebRtc_Word32 transmissionTimeOffset)
{
    if (transmissionTimeOffset > (0x800000 - 1) ||
        transmissionTimeOffset < -(0x800000 - 1))  // Word24
    {
        return -1;
    }
    CriticalSectionScoped cs(_sendCritsect);
    _transmissionTimeOffset = transmissionTimeOffset;
    return 0;
}

WebRtc_Word32
RTPSender::RegisterRtpHeaderExtension(const RTPExtensionType type,
                                      const WebRtc_UWord8 id)
{
    CriticalSectionScoped cs(_sendCritsect);
    return _rtpHeaderExtensionMap.Register(type, id);
}

WebRtc_Word32
RTPSender::DeregisterRtpHeaderExtension(const RTPExtensionType type)
{
    CriticalSectionScoped cs(_sendCritsect);
    return _rtpHeaderExtensionMap.Deregister(type);
}

WebRtc_UWord16
RTPSender::RtpHeaderExtensionTotalLength() const
{
    CriticalSectionScoped cs(_sendCritsect);
    return _rtpHeaderExtensionMap.GetTotalLengthInBytes();
}

//can be called multiple times
WebRtc_Word32 RTPSender::RegisterPayload(
    const char payloadName[RTP_PAYLOAD_NAME_SIZE],
    const WebRtc_Word8 payloadNumber,
    const WebRtc_UWord32 frequency,
    const WebRtc_UWord8 channels,
    const WebRtc_UWord32 rate) {
  assert(payloadName);
  CriticalSectionScoped cs(_sendCritsect);

  std::map<WebRtc_Word8, ModuleRTPUtility::Payload*>::iterator it =
      _payloadTypeMap.find(payloadNumber);

  if (_payloadTypeMap.end() != it) {
    // we already use this payload type
    ModuleRTPUtility::Payload* payload = it->second;
    assert(payload);

    // check if it's the same as we already have
    if (ModuleRTPUtility::StringCompare(payload->name, payloadName,
                                        RTP_PAYLOAD_NAME_SIZE - 1)) {
      if (_audioConfigured && payload->audio &&
          payload->typeSpecific.Audio.frequency == frequency &&
          (payload->typeSpecific.Audio.rate == rate ||
              payload->typeSpecific.Audio.rate == 0 || rate == 0)) {
        payload->typeSpecific.Audio.rate = rate;
        // Ensure that we update the rate if new or old is zero
        return 0;
      }
      if(!_audioConfigured && !payload->audio) {
        return 0;
      }
    }
    return -1;
  }
  WebRtc_Word32 retVal = -1;
  ModuleRTPUtility::Payload* payload = NULL;
  if (_audioConfigured) {
    retVal = _audio->RegisterAudioPayload(payloadName, payloadNumber, frequency,
                                          channels, rate, payload);
  } else {
    retVal = _video->RegisterVideoPayload(payloadName, payloadNumber, rate,
                                          payload);
  }
  if(payload) {
    _payloadTypeMap[payloadNumber] = payload;
  }
  return retVal;
}

WebRtc_Word32 RTPSender::DeRegisterSendPayload(const WebRtc_Word8 payloadType) {
  CriticalSectionScoped lock(_sendCritsect);

  std::map<WebRtc_Word8, ModuleRTPUtility::Payload*>::iterator it =
      _payloadTypeMap.find(payloadType);

  if (_payloadTypeMap.end() == it) return -1;

  ModuleRTPUtility::Payload* payload = it->second;
  delete payload;
  _payloadTypeMap.erase(it);
  return 0;
}

WebRtc_Word8 RTPSender::SendPayloadType() const
{
    return _payloadType;
}


int RTPSender::SendPayloadFrequency() const
{
    return _audio->AudioFrequency();
}


WebRtc_Word32
RTPSender::SetMaxPayloadLength(const WebRtc_UWord16 maxPayloadLength, const WebRtc_UWord16 packetOverHead)
{
    // sanity check
    if(maxPayloadLength < 100 || maxPayloadLength > IP_PACKET_SIZE)
    {
        WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id, "%s invalid argument", __FUNCTION__);
        return -1;
    }

    CriticalSectionScoped cs(_sendCritsect);
    _maxPayloadLength = maxPayloadLength;
    _packetOverHead = packetOverHead;

    WEBRTC_TRACE(kTraceInfo, kTraceRtpRtcp, _id, "SetMaxPayloadLength to %d.", maxPayloadLength);
    return 0;
}

WebRtc_UWord16 RTPSender::MaxDataPayloadLength() const {
  if(_audioConfigured) {
    return _maxPayloadLength - RTPHeaderLength();
  } else {
    return _maxPayloadLength - RTPHeaderLength() -
        _video->FECPacketOverhead() - ((_RTX) ? 2 : 0);
        // Include the FEC/ULP/RED overhead.
  }
}

WebRtc_UWord16
RTPSender::MaxPayloadLength() const
{
    return _maxPayloadLength;
}

WebRtc_UWord16
RTPSender::PacketOverHead() const
{
    return _packetOverHead;
}

void RTPSender::SetTransmissionSmoothingStatus(const bool enable) {
  CriticalSectionScoped cs(_sendCritsect);
  _transmissionSmoothing = enable;
}

bool RTPSender::TransmissionSmoothingStatus() const {
  CriticalSectionScoped cs(_sendCritsect);
  return _transmissionSmoothing;
}

void RTPSender::SetRTXStatus(const bool enable,
                             const bool setSSRC,
                             const WebRtc_UWord32 SSRC) {
  CriticalSectionScoped cs(_sendCritsect);
  _RTX = enable;
  if (enable) {
    if (setSSRC) {
     _ssrcRTX = SSRC;
    } else {
     _ssrcRTX = _ssrcDB.CreateSSRC();   // can't be 0
    }
  }
}

void RTPSender::RTXStatus(bool* enable,
                          WebRtc_UWord32* SSRC) const {
  CriticalSectionScoped cs(_sendCritsect);
  *enable = _RTX;
  *SSRC = _ssrcRTX;
}

WebRtc_Word32 RTPSender::CheckPayloadType(const WebRtc_Word8 payloadType,
                                          RtpVideoCodecTypes& videoType) {
  CriticalSectionScoped cs(_sendCritsect);

  if (payloadType < 0) {
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                 "\tinvalid payloadType (%d)", payloadType);
    return -1;
  }
  if (_audioConfigured) {
    WebRtc_Word8 redPlType = -1;
    if (_audio->RED(redPlType) == 0) {
      // We have configured RED.
      if(redPlType == payloadType) {
        // And it's a match...
        return 0;
      }
    }
  }
  if (_payloadType == payloadType) {
    if (!_audioConfigured) {
      videoType = _video->VideoCodecType();
    }
    return 0;
  }
  std::map<WebRtc_Word8, ModuleRTPUtility::Payload*>::iterator it =
      _payloadTypeMap.find(payloadType);
  if (it == _payloadTypeMap.end()) {
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                 "\tpayloadType:%d not registered", payloadType);
    return -1;
  }
  _payloadType = payloadType;
  ModuleRTPUtility::Payload* payload = it->second;
  assert(payload);
  if (payload->audio) {
    if (_audioConfigured) {
      // Extract payload frequency
      int payloadFreqHz;
      if (ModuleRTPUtility::StringCompare(payload->name,"g722",4)&&
          (payload->name[4] == 0)) {
        //Check that strings end there, g722.1...
        // Special case for G.722, bug in spec
        payloadFreqHz=8000;
      } else {
        payloadFreqHz=payload->typeSpecific.Audio.frequency;
      }

      //we don't do anything if it's CN
      if ((_audio->AudioFrequency() != payloadFreqHz)&&
          (!ModuleRTPUtility::StringCompare(payload->name,"cn",2))) {
        _audio->SetAudioFrequency(payloadFreqHz);
        // We need to correct the timestamp again,
        // since this might happen after we've set it
        WebRtc_UWord32 RTPtime =
            ModuleRTPUtility::GetCurrentRTP(&_clock, payloadFreqHz);
        SetStartTimestamp(RTPtime);
        // will be ignored if it's already configured via API
      }
    }
  } else {
    if(!_audioConfigured) {
      _video->SetVideoCodecType(payload->typeSpecific.Video.videoCodecType);
      videoType = payload->typeSpecific.Video.videoCodecType;
      _video->SetMaxConfiguredBitrateVideo(
          payload->typeSpecific.Video.maxRate);
    }
  }
  return 0;
}

WebRtc_Word32
RTPSender::SendOutgoingData(const FrameType frame_type,
                            const WebRtc_Word8 payload_type,
                            const WebRtc_UWord32 capture_timestamp,
                            int64_t capture_time_ms,
                            const WebRtc_UWord8* payload_data,
                            const WebRtc_UWord32 payload_size,
                            const RTPFragmentationHeader* fragmentation,
                            VideoCodecInformation* codec_info,
                            const RTPVideoTypeHeader* rtp_type_hdr)
{
    {
        // Drop this packet if we're not sending media packets.
        CriticalSectionScoped cs(_sendCritsect);
        if (!_sendingMedia)
        {
            return 0;
        }
    }
    RtpVideoCodecTypes video_type = kRtpNoVideo;
    if (CheckPayloadType(payload_type, video_type) != 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
            "%s invalid argument failed to find payloadType:%d",
            __FUNCTION__, payload_type);
        return -1;
    }

    if (_audioConfigured)
    {
        assert(frame_type == kAudioFrameSpeech ||
               frame_type == kAudioFrameCN ||
               frame_type == kFrameEmpty);

        return _audio->SendAudio(frame_type, payload_type, capture_timestamp,
            payload_data, payload_size,fragmentation);
    } else {
        assert(frame_type != kAudioFrameSpeech &&
               frame_type != kAudioFrameCN);

        if (frame_type == kFrameEmpty) {
          return SendPaddingAccordingToBitrate(payload_type, capture_timestamp,
                                               capture_time_ms);
        }
        return _video->SendVideo(video_type,
                                 frame_type,
                                 payload_type,
                                 capture_timestamp,
                                 capture_time_ms,
                                 payload_data,
                                 payload_size,
                                 fragmentation,
                                 codec_info,
                                 rtp_type_hdr);
    }
}

WebRtc_Word32 RTPSender::SendPaddingAccordingToBitrate(
    WebRtc_Word8 payload_type,
    WebRtc_UWord32 capture_timestamp,
    int64_t capture_time_ms) {
  // Current bitrate since last estimate(1 second) averaged with the
  // estimate since then, to get the most up to date bitrate.
  uint32_t current_bitrate = BitrateNow();
  int bitrate_diff = _targetSendBitrate * 1000 - current_bitrate;
  if (bitrate_diff > 0) {
    int bytes = 0;
    if (current_bitrate == 0) {
      // Start up phase. Send one 33.3 ms batch to start with.
      bytes = (bitrate_diff / 8) / 30;
    } else {
      bytes = (bitrate_diff / 8);
      // Cap at 200 ms of target send data.
      int bytes_cap = _targetSendBitrate * 25;  // 1000 / 8 / 5
      if (bytes > bytes_cap) {
        bytes = bytes_cap;
      }
    }
    // Send padding data.
    return SendPadData(payload_type, capture_timestamp, capture_time_ms, bytes);
  }
  return 0;
}

WebRtc_Word32 RTPSender::SendPadData(WebRtc_Word8 payload_type,
                                     WebRtc_UWord32 capture_timestamp,
                                     int64_t capture_time_ms,
                                     WebRtc_Word32 bytes) {
  // Drop this packet if we're not sending media packets
  if (!_sendingMedia) {
    return 0;
  }
  // Max in the RFC 3550 is 255 bytes, we limit it to be modulus 32 for SRTP.
  int max_length = 224;
  WebRtc_UWord8 data_buffer[IP_PACKET_SIZE];

  for (; bytes > 0; bytes -= max_length) {
    int padding_bytes_in_packet = max_length;
    if (bytes < max_length) {
      padding_bytes_in_packet = (bytes + 16) & 0xffe0;  // Keep our modulus 32.
    }
    if (padding_bytes_in_packet < 32) {
       // Sanity don't send empty packets.
       break;
    }

    WebRtc_Word32 header_length;
    {
      // Correct seq num, timestamp and payload type.
      header_length = BuildRTPheader(data_buffer,
                                     payload_type,
                                     false,  // No markerbit.
                                     capture_timestamp,
                                     true,  // Timestamp provided.
                                     true);  // Increment sequence number.
    }
    data_buffer[0] |= 0x20;  // Set padding bit.
    WebRtc_Word32* data =
        reinterpret_cast<WebRtc_Word32*>(&(data_buffer[header_length]));

    // Fill data buffer with random data.
    for(int j = 0; j < (padding_bytes_in_packet >> 2); j++) {
      data[j] = rand();
    }
    // Set number of padding bytes in the last byte of the packet.
    data_buffer[header_length + padding_bytes_in_packet - 1] =
        padding_bytes_in_packet;
    // Send the packet
    if (0 > SendToNetwork(data_buffer,
                          padding_bytes_in_packet,
                          header_length,
                          capture_time_ms,
                          kDontRetransmit)) {
      // Error sending the packet.
      break;
    }
  }
  if (bytes > 31) {  // 31 due to our modulus 32.
    // We did not manage to send all bytes.
    return -1;
  }
  return 0;
}

WebRtc_Word32 RTPSender::SetStorePacketsStatus(
    const bool enable,
    const WebRtc_UWord16 numberToStore) {
  _packetHistory->SetStorePacketsStatus(enable, numberToStore);
  return 0;
}

bool RTPSender::StorePackets() const {
  return _packetHistory->StorePackets();
}

WebRtc_Word32 RTPSender::ReSendPacket(WebRtc_UWord16 packet_id,
                                      WebRtc_UWord32 min_resend_time) {

  WebRtc_UWord16 length = IP_PACKET_SIZE;
  WebRtc_UWord8 data_buffer[IP_PACKET_SIZE];
  WebRtc_UWord8* buffer_to_send_ptr = data_buffer;

  int64_t stored_time_in_ms;
  StorageType type;
  bool found = _packetHistory->GetRTPPacket(packet_id,
      min_resend_time, data_buffer, &length, &stored_time_in_ms, &type);
  if (!found) {
    // Packet not found.
    return 0;
  }

  if (length == 0 || type == kDontRetransmit) {
    // No bytes copied (packet recently resent, skip resending) or
    // packet should not be retransmitted.
    return 0;
  }

  WebRtc_UWord8 data_buffer_rtx[IP_PACKET_SIZE];
  if (_RTX) {
    buffer_to_send_ptr = data_buffer_rtx;

    CriticalSectionScoped cs(_sendCritsect);
    // Add RTX header.
    ModuleRTPUtility::RTPHeaderParser rtpParser(
        reinterpret_cast<const WebRtc_UWord8*>(data_buffer),
        length);

    WebRtcRTPHeader rtp_header;
    rtpParser.Parse(rtp_header);

    // Add original RTP header.
    memcpy(data_buffer_rtx, data_buffer, rtp_header.header.headerLength);

    // Replace sequence number.
    WebRtc_UWord8* ptr = data_buffer_rtx + 2;
    ModuleRTPUtility::AssignUWord16ToBuffer(ptr, _sequenceNumberRTX++);

    // Replace SSRC.
    ptr += 6;
    ModuleRTPUtility::AssignUWord32ToBuffer(ptr, _ssrcRTX);

    // Add OSN (original sequence number).
    ptr = data_buffer_rtx + rtp_header.header.headerLength;
    ModuleRTPUtility::AssignUWord16ToBuffer(
        ptr, rtp_header.header.sequenceNumber);
    ptr += 2;

    // Add original payload data.
    memcpy(ptr,
           data_buffer + rtp_header.header.headerLength,
           length - rtp_header.header.headerLength);
    length += 2;
  }

  WebRtc_Word32 bytes_sent = ReSendToNetwork(buffer_to_send_ptr, length);
  if (bytes_sent <= 0) {
    WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, _id,
                 "Transport failed to resend packet_id %u", packet_id);
    return -1;
  }

  // Store the time when the packet was last resent.
  _packetHistory->UpdateResendTime(packet_id);

  return bytes_sent;
}

WebRtc_Word32 RTPSender::ReSendToNetwork(const WebRtc_UWord8* packet,
                                         const WebRtc_UWord32 size) {
  WebRtc_Word32 bytes_sent = -1;
  {
    CriticalSectionScoped lock(_transportCritsect);
    if (_transport) {
      bytes_sent = _transport->SendPacket(_id, packet, size);
    }
  }

  if (bytes_sent <= 0) {
    return -1;
  }

  // Update send statistics
  CriticalSectionScoped cs(_sendCritsect);
  Bitrate::Update(bytes_sent);
  _packetsSent++;
  // We on purpose don't add to _payloadBytesSent since this is a
  // re-transmit and not new payload data.
  return bytes_sent;
}

int RTPSender::SelectiveRetransmissions() const {
  if (!_video) return -1;
  return _video->SelectiveRetransmissions();
}

int RTPSender::SetSelectiveRetransmissions(uint8_t settings) {
  if (!_video) return -1;
  return _video->SetSelectiveRetransmissions(settings);
}

void
RTPSender::OnReceivedNACK(const WebRtc_UWord16 nackSequenceNumbersLength,
                          const WebRtc_UWord16* nackSequenceNumbers,
                          const WebRtc_UWord16 avgRTT) {
    const WebRtc_Word64 now = _clock.GetTimeInMS();
    WebRtc_UWord32 bytesReSent = 0;

  // Enough bandwidth to send NACK?
  if (!ProcessNACKBitRate(now)) {
    WEBRTC_TRACE(kTraceStream,
                 kTraceRtpRtcp,
                 _id,
                 "NACK bitrate reached. Skip sending NACK response. Target %d",
                 _targetSendBitrate);
    return;
  }

  for (WebRtc_UWord16 i = 0; i < nackSequenceNumbersLength; ++i) {
    const WebRtc_Word32 bytesSent = ReSendPacket(nackSequenceNumbers[i],
                                                 5+avgRTT);
    if (bytesSent > 0) {
      bytesReSent += bytesSent;
    } else if (bytesSent == 0) {
      // The packet has previously been resent.
      // Try resending next packet in the list.
      continue;
    } else if (bytesSent < 0) {
      // Failed to send one Sequence number. Give up the rest in this nack.
      WEBRTC_TRACE(kTraceWarning,
                   kTraceRtpRtcp,
                   _id,
                   "Failed resending RTP packet %d, Discard rest of packets",
                   nackSequenceNumbers[i]);
      break;
    }
    // delay bandwidth estimate (RTT * BW)
    if (_targetSendBitrate != 0 && avgRTT) {
      // kbits/s * ms = bits => bits/8 = bytes
      WebRtc_UWord32 targetBytes =
          (static_cast<WebRtc_UWord32>(_targetSendBitrate) * avgRTT) >> 3;
      if (bytesReSent > targetBytes) {
        break; // ignore the rest of the packets in the list
      }
    }
  }
  if (bytesReSent > 0) {
    // TODO(pwestin) consolidate these two methods.
    UpdateNACKBitRate(bytesReSent, now);
    _nackBitrate.Update(bytesReSent);
  }
}

/**
*    @return true if the nack bitrate is lower than the requested max bitrate
*/
bool RTPSender::ProcessNACKBitRate(const WebRtc_UWord32 now) {
  WebRtc_UWord32 num = 0;
  WebRtc_Word32 byteCount = 0;
  const WebRtc_UWord32 avgInterval=1000;

  CriticalSectionScoped cs(_sendCritsect);

  if (_targetSendBitrate == 0) {
    return true;
  }
  for (num = 0; num < NACK_BYTECOUNT_SIZE; num++) {
    if ((now - _nackByteCountTimes[num]) > avgInterval) {
      // don't use data older than 1sec
      break;
    } else {
      byteCount += _nackByteCount[num];
    }
  }
  WebRtc_Word32 timeInterval = avgInterval;
  if (num == NACK_BYTECOUNT_SIZE) {
    // More than NACK_BYTECOUNT_SIZE nack messages has been received
    // during the last msgInterval
    timeInterval = now - _nackByteCountTimes[num-1];
    if(timeInterval < 0) {
      timeInterval = avgInterval;
    }
  }
  return (byteCount*8) < (_targetSendBitrate * timeInterval);
}

void RTPSender::UpdateNACKBitRate(const WebRtc_UWord32 bytes,
                                  const WebRtc_UWord32 now) {
  CriticalSectionScoped cs(_sendCritsect);

  // save bitrate statistics
  if(bytes > 0) {
    if(now == 0) {
      // add padding length
      _nackByteCount[0] += bytes;
    } else {
      if(_nackByteCountTimes[0] == 0) {
        // first no shift
      } else {
        // shift
        for(int i = (NACK_BYTECOUNT_SIZE-2); i >= 0 ; i--) {
          _nackByteCount[i+1] = _nackByteCount[i];
          _nackByteCountTimes[i+1] = _nackByteCountTimes[i];
        }
      }
      _nackByteCount[0] = bytes;
      _nackByteCountTimes[0] = now;
    }
  }
}

// Function triggered by timer.
void RTPSender::ProcessSendToNetwork() {
  WebRtc_Word64 delta_time_ms;
  {
    CriticalSectionScoped cs(_sendCritsect);

    if (!_transmissionSmoothing) {
      return;
    }
    WebRtc_Word64 now = _clock.GetTimeInMS();
    delta_time_ms = now - _timeLastSendToNetworkUpdate;
    _timeLastSendToNetworkUpdate = now;
  }
  _sendBucket.UpdateBytesPerInterval(delta_time_ms, _targetSendBitrate);

  while (!_sendBucket.Empty()) {

    WebRtc_Word32 seq_num = _sendBucket.GetNextPacket();
    if (seq_num < 0) {
      break;
    }

    WebRtc_UWord8 data_buffer[IP_PACKET_SIZE];
    WebRtc_UWord16 length = IP_PACKET_SIZE;
    int64_t stored_time_ms;
    StorageType type;
    bool found = _packetHistory->GetRTPPacket(seq_num, 0, data_buffer, &length,
        &stored_time_ms, &type);
    if (!found) {
      assert(false);
      return;
    }
    assert(length > 0);

    WebRtc_Word64 diff_ms = _clock.GetTimeInMS() - stored_time_ms;

    ModuleRTPUtility::RTPHeaderParser rtpParser(data_buffer, length);
    WebRtcRTPHeader rtp_header;
    rtpParser.Parse(rtp_header);

    UpdateTransmissionTimeOffset(data_buffer, length, rtp_header, diff_ms);

    // Send packet
    WebRtc_Word32 bytes_sent = -1;
    {
      CriticalSectionScoped cs(_transportCritsect);
      if (_transport) {
        bytes_sent = _transport->SendPacket(_id, data_buffer, length);
      }
    }

    // Update send statistics
    if (bytes_sent > 0) {
      CriticalSectionScoped cs(_sendCritsect);
      Bitrate::Update(bytes_sent);
      _packetsSent++;
      if (bytes_sent > rtp_header.header.headerLength) {
        _payloadBytesSent += bytes_sent - rtp_header.header.headerLength;
      }
    }
  }
}

WebRtc_Word32
RTPSender::SendToNetwork(WebRtc_UWord8* buffer,
                         const WebRtc_UWord16 length,
                         const WebRtc_UWord16 rtpLength,
                         int64_t capture_time_ms,
                         const StorageType storage)
{
  // Used for NACK or to spead out the transmission of packets.
  if (_packetHistory->PutRTPPacket(
      buffer, rtpLength + length, _maxPayloadLength, capture_time_ms, storage)
      != 0) {
    return -1;
  }

  if (_transmissionSmoothing) {
    const WebRtc_UWord16 sequenceNumber = (buffer[2] << 8) + buffer[3];
    _sendBucket.Fill(sequenceNumber, rtpLength + length);
    // Packet will be sent at a later time.
    return 0;
  }

  // |capture_time_ms| <= 0 is considered invalid.
  // TODO(holmer): This should be changed all over Video Engine so that negative
  // time is consider invalid, while 0 is considered a valid time.
  if (capture_time_ms > 0) {
    ModuleRTPUtility::RTPHeaderParser rtpParser(buffer, length);
    WebRtcRTPHeader rtp_header;
    rtpParser.Parse(rtp_header);
    int64_t time_now = _clock.GetTimeInMS();
    UpdateTransmissionTimeOffset(buffer, length, rtp_header,
                                 time_now - capture_time_ms);
  }

  // Send packet
  WebRtc_Word32 bytes_sent = -1;
  {
    CriticalSectionScoped cs(_transportCritsect);
    if (_transport) {
      bytes_sent = _transport->SendPacket(_id, buffer, length + rtpLength);
    }
  }

  if (bytes_sent <= 0) {
    return -1;
  }

  // Update send statistics
  CriticalSectionScoped cs(_sendCritsect);
  Bitrate::Update(bytes_sent);
  _packetsSent++;
  if (bytes_sent > rtpLength) {
    _payloadBytesSent += bytes_sent - rtpLength;
  }
  return 0;
}

void
RTPSender::ProcessBitrate()
{
    CriticalSectionScoped cs(_sendCritsect);

    Bitrate::Process();
    _nackBitrate.Process();

    if (_audioConfigured)
      return;
    _video->ProcessBitrate();
}

WebRtc_UWord16
RTPSender::RTPHeaderLength() const
{
    WebRtc_UWord16 rtpHeaderLength = 12;

    if(_includeCSRCs)
    {
        rtpHeaderLength += sizeof(WebRtc_UWord32)*_CSRCs;
    }
    rtpHeaderLength += RtpHeaderExtensionTotalLength();

    return rtpHeaderLength;
}

WebRtc_UWord16
RTPSender::IncrementSequenceNumber()
{
    CriticalSectionScoped cs(_sendCritsect);
    return _sequenceNumber++;
}

WebRtc_Word32
RTPSender::ResetDataCounters()
{
    _packetsSent = 0;
    _payloadBytesSent = 0;

    return 0;
}

// number of sent RTP packets
// dont use critsect to avoid potental deadlock
WebRtc_UWord32
RTPSender::Packets() const
{
    return _packetsSent;
}

// number of sent RTP bytes
// dont use critsect to avoid potental deadlock
WebRtc_UWord32
RTPSender::Bytes() const
{
    return _payloadBytesSent;
}

WebRtc_Word32
RTPSender::BuildRTPheader(WebRtc_UWord8* dataBuffer,
                          const WebRtc_Word8 payloadType,
                          const bool markerBit,
                          const WebRtc_UWord32 captureTimeStamp,
                          const bool timeStampProvided,
                          const bool incSequenceNumber)
{
    assert(payloadType>=0);

    CriticalSectionScoped cs(_sendCritsect);

    dataBuffer[0] = static_cast<WebRtc_UWord8>(0x80);            // version 2
    dataBuffer[1] = static_cast<WebRtc_UWord8>(payloadType);
    if (markerBit)
    {
        dataBuffer[1] |= kRtpMarkerBitMask;  // MarkerBit is set
    }

    if(timeStampProvided)
    {
        _timeStamp = _startTimeStamp + captureTimeStamp;
    } else
    {
        // make a unique time stamp
        // used for inband signaling
        // we can't inc by the actual time, since then we increase the risk of back timing
        _timeStamp++;
    }

    ModuleRTPUtility::AssignUWord16ToBuffer(dataBuffer+2, _sequenceNumber);
    ModuleRTPUtility::AssignUWord32ToBuffer(dataBuffer+4, _timeStamp);
    ModuleRTPUtility::AssignUWord32ToBuffer(dataBuffer+8, _ssrc);

    WebRtc_Word32 rtpHeaderLength = 12;

    // Add the CSRCs if any
    if (_includeCSRCs && _CSRCs > 0)
    {
        if(_CSRCs > kRtpCsrcSize)
        {
            // error
            assert(false);
            return -1;
        }
        WebRtc_UWord8* ptr = &dataBuffer[rtpHeaderLength];
        for (WebRtc_UWord32 i = 0; i < _CSRCs; ++i)
        {
            ModuleRTPUtility::AssignUWord32ToBuffer(ptr, _CSRC[i]);
            ptr +=4;
        }
        dataBuffer[0] = (dataBuffer[0]&0xf0) | _CSRCs;

        // Update length of header
        rtpHeaderLength += sizeof(WebRtc_UWord32)*_CSRCs;
    }
    {
        _sequenceNumber++; // prepare for next packet
    }

    WebRtc_UWord16 len = BuildRTPHeaderExtension(dataBuffer + rtpHeaderLength);
    if (len)
    {
      dataBuffer[0] |= 0x10;  // set eXtension bit
      rtpHeaderLength += len;
    }

    return rtpHeaderLength;
}

WebRtc_UWord16
RTPSender::BuildRTPHeaderExtension(WebRtc_UWord8* dataBuffer) const
{
    if (_rtpHeaderExtensionMap.Size() <= 0) {
       return 0;
    }

    /* RTP header extension, RFC 3550.
     0                   1                   2                   3
     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |      defined by profile       |           length              |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                        header extension                       |
    |                             ....                              |
    */

    const WebRtc_UWord32 kPosLength = 2;
    const WebRtc_UWord32 kHeaderLength = RTP_ONE_BYTE_HEADER_LENGTH_IN_BYTES;

    // Add extension ID (0xBEDE).
    ModuleRTPUtility::AssignUWord16ToBuffer(dataBuffer,
                                            RTP_ONE_BYTE_HEADER_EXTENSION);

    // Add extensions.
    WebRtc_UWord16 total_block_length = 0;

    RTPExtensionType type = _rtpHeaderExtensionMap.First();
    while (type != kRtpExtensionNone)
    {
        WebRtc_UWord8 block_length = 0;
        if (type == kRtpExtensionTransmissionTimeOffset)
        {
            block_length = BuildTransmissionTimeOffsetExtension(
                dataBuffer + kHeaderLength + total_block_length);
        }
        total_block_length += block_length;
        type = _rtpHeaderExtensionMap.Next(type);
    }

    if (total_block_length == 0)
    {
        // No extension added.
        return 0;
    }

    // Set header length (in number of Word32, header excluded).
    assert(total_block_length % 4 == 0);
    ModuleRTPUtility::AssignUWord16ToBuffer(dataBuffer + kPosLength,
                                            total_block_length / 4);

    // Total added length.
    return kHeaderLength + total_block_length;
}

WebRtc_UWord8
RTPSender::BuildTransmissionTimeOffsetExtension(WebRtc_UWord8* dataBuffer) const
{
   // From RFC 5450: Transmission Time Offsets in RTP Streams.
   //
   // The transmission time is signaled to the receiver in-band using the
   // general mechanism for RTP header extensions [RFC5285]. The payload
   // of this extension (the transmitted value) is a 24-bit signed integer.
   // When added to the RTP timestamp of the packet, it represents the
   // "effective" RTP transmission time of the packet, on the RTP
   // timescale.
   //
   // The form of the transmission offset extension block:
   //
   //    0                   1                   2                   3
   //    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   //   |  ID   | len=2 |              transmission offset              |
   //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

    // Get id defined by user.
    WebRtc_UWord8 id;
    if (_rtpHeaderExtensionMap.GetId(kRtpExtensionTransmissionTimeOffset, &id)
        != 0) {
      // Not registered.
      return 0;
    }

    int pos = 0;
    const WebRtc_UWord8 len = 2;
    dataBuffer[pos++] = (id << 4) + len;
    ModuleRTPUtility::AssignUWord24ToBuffer(dataBuffer + pos,
                                            _transmissionTimeOffset);
    pos += 3;
    assert(pos == TRANSMISSION_TIME_OFFSET_LENGTH_IN_BYTES);
    return TRANSMISSION_TIME_OFFSET_LENGTH_IN_BYTES;
}

void RTPSender::UpdateTransmissionTimeOffset(
    WebRtc_UWord8* rtp_packet,
    const WebRtc_UWord16 rtp_packet_length,
    const WebRtcRTPHeader& rtp_header,
    const WebRtc_Word64 time_diff_ms) const {
  CriticalSectionScoped cs(_sendCritsect);

  // Get length until start of transmission block.
  int transmission_block_pos =
      _rtpHeaderExtensionMap.GetLengthUntilBlockStartInBytes(
      kRtpExtensionTransmissionTimeOffset);
  if (transmission_block_pos < 0) {
    WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, _id,
        "Failed to update transmission time offset, not registered.");
    return;
  }

  int block_pos = 12 + rtp_header.header.numCSRCs + transmission_block_pos;
  if (rtp_packet_length < block_pos + 4 ||
      rtp_header.header.headerLength < block_pos + 4) {
    WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, _id,
        "Failed to update transmission time offset, invalid length.");
    return;
  }

  // Verify that header contains extension.
  if (!((rtp_packet[12 + rtp_header.header.numCSRCs] == 0xBE) &&
        (rtp_packet[12 + rtp_header.header.numCSRCs + 1] == 0xDE))) {
    WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, _id,
        "Failed to update transmission time offset, hdr extension not found.");
    return;
  }

  // Get id.
  WebRtc_UWord8 id = 0;
  if (_rtpHeaderExtensionMap.GetId(kRtpExtensionTransmissionTimeOffset,
                                   &id) != 0) {
    WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, _id,
        "Failed to update transmission time offset, no id.");
    return;
  }

  // Verify first byte in block.
  const WebRtc_UWord8 first_block_byte = (id << 4) + 2;
  if (rtp_packet[block_pos] != first_block_byte) {
    WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, _id,
        "Failed to update transmission time offset.");
    return;
  }

  // Update transmission offset field.
  ModuleRTPUtility::AssignUWord24ToBuffer(rtp_packet + block_pos + 1,
                                          time_diff_ms * 90);  // RTP timestamp.
}

WebRtc_Word32
RTPSender::RegisterSendTransport(Transport* transport)
{
     CriticalSectionScoped cs(_transportCritsect);
    _transport = transport;
    return 0;
}

void
RTPSender::SetSendingStatus(const bool enabled)
{
    if(enabled)
    {
        WebRtc_UWord32 freq;
        if(_audioConfigured)
        {
            WebRtc_UWord32 frequency = _audio->AudioFrequency();

            // sanity
            switch(frequency)
            {
            case 8000:
            case 12000:
            case 16000:
            case 24000:
            case 32000:
                break;
            default:
                assert(false);
                return;
            }
            freq = frequency;
        } else
        {
            freq = 90000; // 90 KHz for all video
        }
        WebRtc_UWord32 RTPtime = ModuleRTPUtility::GetCurrentRTP(&_clock, freq);

        SetStartTimestamp(RTPtime); // will be ignored if it's already configured via API

    } else
    {
        if(!_ssrcForced)
        {
            // generate a new SSRC
            _ssrcDB.ReturnSSRC(_ssrc);
            _ssrc = _ssrcDB.CreateSSRC();   // can't be 0

        }
        if(!_sequenceNumberForced && !_ssrcForced) // don't initialize seq number if SSRC passed externally
        {
            // generate a new sequence number
            _sequenceNumber = rand() / (RAND_MAX / MAX_INIT_RTP_SEQ_NUMBER);
        }
    }
}

void
RTPSender::SetSendingMediaStatus(const bool enabled)
{
    CriticalSectionScoped cs(_sendCritsect);
    _sendingMedia = enabled;
}

bool
RTPSender::SendingMedia() const
{
    CriticalSectionScoped cs(_sendCritsect);
    return _sendingMedia;
}

WebRtc_UWord32
RTPSender::Timestamp() const
{
    CriticalSectionScoped cs(_sendCritsect);
    return _timeStamp;
}


WebRtc_Word32
RTPSender::SetStartTimestamp( const WebRtc_UWord32 timestamp, const bool force)
{
    CriticalSectionScoped cs(_sendCritsect);
    if(force)
    {
        _startTimeStampForced = force;
        _startTimeStamp = timestamp;
    } else
    {
        if(!_startTimeStampForced)
        {
            _startTimeStamp = timestamp;
        }
    }
    return 0;
}

WebRtc_UWord32
RTPSender::StartTimestamp() const
{
    CriticalSectionScoped cs(_sendCritsect);
    return _startTimeStamp;
}

WebRtc_UWord32
RTPSender::GenerateNewSSRC()
{
    // if configured via API, return 0
    CriticalSectionScoped cs(_sendCritsect);

    if(_ssrcForced)
    {
        return 0;
    }
    _ssrc = _ssrcDB.CreateSSRC();   // can't be 0
    return _ssrc;
}

WebRtc_Word32
RTPSender::SetSSRC(WebRtc_UWord32 ssrc)
{
    // this is configured via the API
    CriticalSectionScoped cs(_sendCritsect);

    if (_ssrc == ssrc && _ssrcForced)
    {
        return 0; // since it's same ssrc, don't reset anything
    }

    _ssrcForced = true;

    _ssrcDB.ReturnSSRC(_ssrc);
    _ssrcDB.RegisterSSRC(ssrc);
    _ssrc = ssrc;

    if(!_sequenceNumberForced)
    {
        _sequenceNumber = rand() / (RAND_MAX / MAX_INIT_RTP_SEQ_NUMBER);
    }
    return 0;
}

WebRtc_UWord32
RTPSender::SSRC() const
{
    CriticalSectionScoped cs(_sendCritsect);
    return _ssrc;
}

WebRtc_Word32
RTPSender::SetCSRCStatus(const bool include)
{
    _includeCSRCs = include;
    return 0;
}

WebRtc_Word32
RTPSender::SetCSRCs(const WebRtc_UWord32 arrOfCSRC[kRtpCsrcSize],
                    const WebRtc_UWord8 arrLength)
{
    if(arrLength > kRtpCsrcSize)
    {
        assert(false);
        return -1;
    }

    CriticalSectionScoped cs(_sendCritsect);

    for(int i = 0; i < arrLength;i++)
    {
        _CSRC[i] = arrOfCSRC[i];
    }
    _CSRCs = arrLength;
    return 0;
}

WebRtc_Word32
RTPSender::CSRCs(WebRtc_UWord32 arrOfCSRC[kRtpCsrcSize]) const
{
    CriticalSectionScoped cs(_sendCritsect);

    if(arrOfCSRC == NULL)
    {
        assert(false);
        return -1;
    }
    for(int i = 0; i < _CSRCs && i < kRtpCsrcSize;i++)
    {
        arrOfCSRC[i] = _CSRC[i];
    }
    return _CSRCs;
}

WebRtc_Word32
RTPSender::SetSequenceNumber(WebRtc_UWord16 seq)
{
    CriticalSectionScoped cs(_sendCritsect);
    _sequenceNumberForced = true;
    _sequenceNumber = seq;
    return 0;
}

WebRtc_UWord16
RTPSender::SequenceNumber() const
{
    CriticalSectionScoped cs(_sendCritsect);
    return _sequenceNumber;
}


    /*
    *    Audio
    */
WebRtc_Word32
RTPSender::RegisterAudioCallback(RtpAudioFeedback* messagesCallback)
{
    if(!_audioConfigured)
    {
        return -1;
    }
    return _audio->RegisterAudioCallback(messagesCallback);
}

    // Send a DTMF tone, RFC 2833 (4733)
WebRtc_Word32
RTPSender::SendTelephoneEvent(const WebRtc_UWord8 key,
                              const WebRtc_UWord16 time_ms,
                              const WebRtc_UWord8 level)
{
    if(!_audioConfigured)
    {
        return -1;
    }
    return _audio->SendTelephoneEvent(key, time_ms, level);
}

bool
RTPSender::SendTelephoneEventActive(WebRtc_Word8& telephoneEvent) const
{
    if(!_audioConfigured)
    {
        return false;
    }
    return _audio->SendTelephoneEventActive(telephoneEvent);
}

    // set audio packet size, used to determine when it's time to send a DTMF packet in silence (CNG)
WebRtc_Word32
RTPSender::SetAudioPacketSize(const WebRtc_UWord16 packetSizeSamples)
{
    if(!_audioConfigured)
    {
        return -1;
    }
    return _audio->SetAudioPacketSize(packetSizeSamples);
}

WebRtc_Word32
RTPSender::SetAudioLevelIndicationStatus(const bool enable,
                                         const WebRtc_UWord8 ID)
{
    if(!_audioConfigured)
    {
        return -1;
    }
    return _audio->SetAudioLevelIndicationStatus(enable, ID);
}

WebRtc_Word32
RTPSender::AudioLevelIndicationStatus(bool& enable,
                                      WebRtc_UWord8& ID) const
{
    return _audio->AudioLevelIndicationStatus(enable, ID);
}

WebRtc_Word32
RTPSender::SetAudioLevel(const WebRtc_UWord8 level_dBov)
{
    return _audio->SetAudioLevel(level_dBov);
}

    // Set payload type for Redundant Audio Data RFC 2198
WebRtc_Word32
RTPSender::SetRED(const WebRtc_Word8 payloadType)
{
    if(!_audioConfigured)
    {
        return -1;
    }
    return _audio->SetRED(payloadType);
}

    // Get payload type for Redundant Audio Data RFC 2198
WebRtc_Word32
RTPSender::RED(WebRtc_Word8& payloadType) const
{
    if(!_audioConfigured)
    {
        return -1;
    }
    return _audio->RED(payloadType);
}

    /*
    *    Video
    */
VideoCodecInformation*
RTPSender::CodecInformationVideo()
{
    if(_audioConfigured)
    {
        return NULL;
    }
    return _video->CodecInformationVideo();
}

RtpVideoCodecTypes
RTPSender::VideoCodecType() const
{
    if(_audioConfigured)
    {
        return kRtpNoVideo;
    }
    return _video->VideoCodecType();
}

WebRtc_UWord32
RTPSender::MaxConfiguredBitrateVideo() const
{
    if(_audioConfigured)
    {
        return 0;
    }
    return _video->MaxConfiguredBitrateVideo();
}

WebRtc_Word32
RTPSender::SendRTPIntraRequest()
{
    if(_audioConfigured)
    {
        return -1;
    }
    return _video->SendRTPIntraRequest();
}

// FEC
WebRtc_Word32
RTPSender::SetGenericFECStatus(const bool enable,
                               const WebRtc_UWord8 payloadTypeRED,
                               const WebRtc_UWord8 payloadTypeFEC)
{
    if(_audioConfigured)
    {
        return -1;
    }
    return _video->SetGenericFECStatus(enable, payloadTypeRED, payloadTypeFEC);
}

WebRtc_Word32
RTPSender::GenericFECStatus(bool& enable,
                            WebRtc_UWord8& payloadTypeRED,
                            WebRtc_UWord8& payloadTypeFEC) const
{
    if(_audioConfigured)
    {
        return -1;
    }
    return _video->GenericFECStatus(enable, payloadTypeRED, payloadTypeFEC);
}

WebRtc_Word32 RTPSender::SetFecParameters(
    const FecProtectionParams* delta_params,
    const FecProtectionParams* key_params) {
  if (_audioConfigured) {
    return -1;
  }
  return _video->SetFecParameters(delta_params, key_params);
}
} // namespace webrtc
