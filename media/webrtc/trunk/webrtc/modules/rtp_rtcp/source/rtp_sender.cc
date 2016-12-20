/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtp_sender.h"

#include <stdlib.h>  // srand

#include "webrtc/modules/rtp_rtcp/interface/rtp_cvo.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_sender_audio.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_sender_video.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/system_wrappers/interface/tick_util.h"
#include "webrtc/system_wrappers/interface/trace_event.h"

namespace webrtc {

// Max in the RFC 3550 is 255 bytes, we limit it to be modulus 32 for SRTP.
const size_t kMaxPaddingLength = 224;
const int kSendSideDelayWindowMs = 1000;

namespace {

const size_t kRtpHeaderLength = 12;

const char* FrameTypeToString(FrameType frame_type) {
  switch (frame_type) {
    case kFrameEmpty: return "empty";
    case kAudioFrameSpeech: return "audio_speech";
    case kAudioFrameCN: return "audio_cn";
    case kVideoFrameKey: return "video_key";
    case kVideoFrameDelta: return "video_delta";
  }
  return "";
}

}  // namespace

class BitrateAggregator {
 public:
  explicit BitrateAggregator(BitrateStatisticsObserver* bitrate_callback)
      : callback_(bitrate_callback),
        total_bitrate_observer_(*this),
        retransmit_bitrate_observer_(*this),
        ssrc_(0) {}

  void OnStatsUpdated() const {
    if (callback_)
      callback_->Notify(total_bitrate_observer_.statistics(),
                        retransmit_bitrate_observer_.statistics(),
                        ssrc_);
  }

  Bitrate::Observer* total_bitrate_observer() {
    return &total_bitrate_observer_;
  }
  Bitrate::Observer* retransmit_bitrate_observer() {
    return &retransmit_bitrate_observer_;
  }

  void set_ssrc(uint32_t ssrc) { ssrc_ = ssrc; }

 private:
  // We assume that these observers are called on the same thread, which is
  // true for RtpSender as they are called on the Process thread.
  class BitrateObserver : public Bitrate::Observer {
   public:
    explicit BitrateObserver(const BitrateAggregator& aggregator)
        : aggregator_(aggregator) {}

    // Implements Bitrate::Observer.
    void BitrateUpdated(const BitrateStatistics& stats) override {
      statistics_ = stats;
      aggregator_.OnStatsUpdated();
    }

    BitrateStatistics statistics() const { return statistics_; }

   private:
    BitrateStatistics statistics_;
    const BitrateAggregator& aggregator_;
  };

  BitrateStatisticsObserver* const callback_;
  BitrateObserver total_bitrate_observer_;
  BitrateObserver retransmit_bitrate_observer_;
  uint32_t ssrc_;
};

RTPSender::RTPSender(int32_t id,
                     bool audio,
                     Clock* clock,
                     Transport* transport,
                     RtpAudioFeedback* audio_feedback,
                     PacedSender* paced_sender,
                     BitrateStatisticsObserver* bitrate_callback,
                     FrameCountObserver* frame_count_observer,
                     SendSideDelayObserver* send_side_delay_observer)
    : clock_(clock),
      // TODO(holmer): Remove this conversion when we remove the use of
      // TickTime.
      clock_delta_ms_(clock_->TimeInMilliseconds() -
                      TickTime::MillisecondTimestamp()),
      bitrates_(new BitrateAggregator(bitrate_callback)),
      total_bitrate_sent_(clock, bitrates_->total_bitrate_observer()),
      id_(id),
      audio_configured_(audio),
      audio_(audio ? new RTPSenderAudio(id, clock, this, audio_feedback)
                   : nullptr),
      video_(audio ? nullptr : new RTPSenderVideo(clock, this)),
      paced_sender_(paced_sender),
      last_capture_time_ms_sent_(0),
      send_critsect_(CriticalSectionWrapper::CreateCriticalSection()),
      transport_(transport),
      sending_media_(true),                      // Default to sending media.
      max_payload_length_(IP_PACKET_SIZE - 28),  // Default is IP-v4/UDP.
      packet_over_head_(28),
      payload_type_(-1),
      payload_type_map_(),
      rtp_header_extension_map_(),
      transmission_time_offset_(0),
      absolute_send_time_(0),
      rotation_(kVideoRotation_0),
      cvo_mode_(kCVONone),
      transport_sequence_number_(0),
      rid_(NULL),
      // NACK.
      nack_byte_count_times_(),
      nack_byte_count_(),
      nack_bitrate_(clock, bitrates_->retransmit_bitrate_observer()),
      packet_history_(clock),
      // Statistics
      statistics_crit_(CriticalSectionWrapper::CreateCriticalSection()),
      rtp_stats_callback_(NULL),
      frame_count_observer_(frame_count_observer),
      send_side_delay_observer_(send_side_delay_observer),
      // RTP variables
      start_timestamp_forced_(false),
      start_timestamp_(0),
      ssrc_db_(*SSRCDatabase::GetSSRCDatabase()),
      remote_ssrc_(0),
      sequence_number_forced_(false),
      ssrc_forced_(false),
      timestamp_(0),
      capture_time_ms_(0),
      last_timestamp_time_ms_(0),
      media_has_been_sent_(false),
      last_packet_marker_bit_(false),
      csrcs_(),
      rtx_(kRtxOff),
      payload_type_rtx_(-1),
      target_bitrate_critsect_(CriticalSectionWrapper::CreateCriticalSection()),
      target_bitrate_(0) {
  memset(nack_byte_count_times_, 0, sizeof(nack_byte_count_times_));
  memset(nack_byte_count_, 0, sizeof(nack_byte_count_));
  // We need to seed the random generator.
  srand(static_cast<uint32_t>(clock_->TimeInMilliseconds()));
  ssrc_ = ssrc_db_.CreateSSRC();  // Can't be 0.
  ssrc_rtx_ = ssrc_db_.CreateSSRC();  // Can't be 0.
  bitrates_->set_ssrc(ssrc_);
  // Random start, 16 bits. Can't be 0.
  sequence_number_rtx_ = static_cast<uint16_t>(rand() + 1) & 0x7FFF;
  sequence_number_ = static_cast<uint16_t>(rand() + 1) & 0x7FFF;
}

RTPSender::~RTPSender() {
  if (remote_ssrc_ != 0) {
    ssrc_db_.ReturnSSRC(remote_ssrc_);
  }
  ssrc_db_.ReturnSSRC(ssrc_);

  SSRCDatabase::ReturnSSRCDatabase();
  while (!payload_type_map_.empty()) {
    std::map<int8_t, RtpUtility::Payload*>::iterator it =
        payload_type_map_.begin();
    delete it->second;
    payload_type_map_.erase(it);
  }
}

void RTPSender::SetTargetBitrate(uint32_t bitrate) {
  CriticalSectionScoped cs(target_bitrate_critsect_.get());
  target_bitrate_ = bitrate;
}

uint32_t RTPSender::GetTargetBitrate() {
  CriticalSectionScoped cs(target_bitrate_critsect_.get());
  return target_bitrate_;
}

uint16_t RTPSender::ActualSendBitrateKbit() const {
  return (uint16_t)(total_bitrate_sent_.BitrateNow() / 1000);
}

uint32_t RTPSender::VideoBitrateSent() const {
  if (video_) {
    return video_->VideoBitrateSent();
  }
  return 0;
}

uint32_t RTPSender::FecOverheadRate() const {
  if (video_) {
    return video_->FecOverheadRate();
  }
  return 0;
}

uint32_t RTPSender::NackOverheadRate() const {
  return nack_bitrate_.BitrateLast();
}

bool RTPSender::GetSendSideDelay(int* avg_send_delay_ms,
                                 int* max_send_delay_ms) const {
  CriticalSectionScoped lock(statistics_crit_.get());
  SendDelayMap::const_iterator it = send_delays_.upper_bound(
      clock_->TimeInMilliseconds() - kSendSideDelayWindowMs);
  if (it == send_delays_.end())
    return false;
  int num_delays = 0;
  for (; it != send_delays_.end(); ++it) {
    *max_send_delay_ms = std::max(*max_send_delay_ms, it->second);
    *avg_send_delay_ms += it->second;
    ++num_delays;
  }
  *avg_send_delay_ms = (*avg_send_delay_ms + num_delays / 2) / num_delays;
  return true;
}

int32_t RTPSender::SetTransmissionTimeOffset(int32_t transmission_time_offset) {
  if (transmission_time_offset > (0x800000 - 1) ||
      transmission_time_offset < -(0x800000 - 1)) {  // Word24.
    return -1;
  }
  CriticalSectionScoped cs(send_critsect_.get());
  transmission_time_offset_ = transmission_time_offset;
  return 0;
}

int32_t RTPSender::SetAbsoluteSendTime(uint32_t absolute_send_time) {
  if (absolute_send_time > 0xffffff) {  // UWord24.
    return -1;
  }
  CriticalSectionScoped cs(send_critsect_.get());
  absolute_send_time_ = absolute_send_time;
  return 0;
}

void RTPSender::SetVideoRotation(VideoRotation rotation) {
  CriticalSectionScoped cs(send_critsect_.get());
  rotation_ = rotation;
}

int32_t RTPSender::SetTransportSequenceNumber(uint16_t sequence_number) {
  CriticalSectionScoped cs(send_critsect_.get());
  transport_sequence_number_ = sequence_number;
  return 0;
}

int32_t RTPSender::SetRID(const char* rid) {
  CriticalSectionScoped cs(send_critsect_.get());
  // TODO(jesup) avoid allocations
  if (!rid_ || strlen(rid_) < strlen(rid)) {
    // rid rarely changes length....
    delete [] rid_;
    rid_ = new char[strlen(rid)+1];
  }
  strcpy(rid_, rid);
  return 0;
}

int32_t RTPSender::RegisterRtpHeaderExtension(RTPExtensionType type,
                                              uint8_t id) {
  CriticalSectionScoped cs(send_critsect_.get());
  if (type == kRtpExtensionVideoRotation) {
    cvo_mode_ = kCVOInactive;
    return rtp_header_extension_map_.RegisterInactive(type, id);
  }
  return rtp_header_extension_map_.Register(type, id);
}

bool RTPSender::IsRtpHeaderExtensionRegistered(RTPExtensionType type) {
  CriticalSectionScoped cs(send_critsect_.get());
  return rtp_header_extension_map_.IsRegistered(type);
}

int32_t RTPSender::DeregisterRtpHeaderExtension(RTPExtensionType type) {
  CriticalSectionScoped cs(send_critsect_.get());
  return rtp_header_extension_map_.Deregister(type);
}

size_t RTPSender::RtpHeaderExtensionTotalLength() const {
  CriticalSectionScoped cs(send_critsect_.get());
  return rtp_header_extension_map_.GetTotalLengthInBytes();
}

int32_t RTPSender::RegisterPayload(
    const char payload_name[RTP_PAYLOAD_NAME_SIZE],
    int8_t payload_number,
    uint32_t frequency,
    uint8_t channels,
    uint32_t rate) {
  assert(payload_name);
  CriticalSectionScoped cs(send_critsect_.get());

  std::map<int8_t, RtpUtility::Payload*>::iterator it =
      payload_type_map_.find(payload_number);

  if (payload_type_map_.end() != it) {
    // We already use this payload type.
    RtpUtility::Payload* payload = it->second;
    assert(payload);

    // Check if it's the same as we already have.
    if (RtpUtility::StringCompare(
            payload->name, payload_name, RTP_PAYLOAD_NAME_SIZE - 1)) {
      if (audio_configured_ && payload->audio &&
          payload->typeSpecific.Audio.frequency == frequency &&
          (payload->typeSpecific.Audio.rate == rate ||
           payload->typeSpecific.Audio.rate == 0 || rate == 0)) {
        payload->typeSpecific.Audio.rate = rate;
        // Ensure that we update the rate if new or old is zero.
        return 0;
      }
      if (!audio_configured_ && !payload->audio) {
        return 0;
      }
    }
    return -1;
  }
  int32_t ret_val = -1;
  RtpUtility::Payload* payload = NULL;
  if (audio_configured_) {
    ret_val = audio_->RegisterAudioPayload(payload_name, payload_number,
                                           frequency, channels, rate, payload);
  } else {
    ret_val = video_->RegisterVideoPayload(payload_name, payload_number, rate,
                                           payload);
  }
  if (payload) {
    payload_type_map_[payload_number] = payload;
  }
  return ret_val;
}

int32_t RTPSender::DeRegisterSendPayload(int8_t payload_type) {
  CriticalSectionScoped lock(send_critsect_.get());

  std::map<int8_t, RtpUtility::Payload*>::iterator it =
      payload_type_map_.find(payload_type);

  if (payload_type_map_.end() == it) {
    return -1;
  }
  RtpUtility::Payload* payload = it->second;
  delete payload;
  payload_type_map_.erase(it);
  return 0;
}

void RTPSender::SetSendPayloadType(int8_t payload_type) {
  CriticalSectionScoped cs(send_critsect_.get());
  payload_type_ = payload_type;
}

int8_t RTPSender::SendPayloadType() const {
  CriticalSectionScoped cs(send_critsect_.get());
  return payload_type_;
}

int RTPSender::SendPayloadFrequency() const {
  return audio_ != NULL ? audio_->AudioFrequency() : kVideoPayloadTypeFrequency;
}

int32_t RTPSender::SetMaxPayloadLength(size_t max_payload_length,
                                       uint16_t packet_over_head) {
  // Sanity check.
  if (max_payload_length < 100 || max_payload_length > IP_PACKET_SIZE) {
    LOG(LS_ERROR) << "Invalid max payload length: " << max_payload_length;
    return -1;
  }
  CriticalSectionScoped cs(send_critsect_.get());
  max_payload_length_ = max_payload_length;
  packet_over_head_ = packet_over_head;
  return 0;
}

size_t RTPSender::MaxDataPayloadLength() const {
  int rtx;
  {
    CriticalSectionScoped rtx_lock(send_critsect_.get());
    rtx = rtx_;
  }
  if (audio_configured_) {
    return max_payload_length_ - RTPHeaderLength();
  } else {
    return max_payload_length_ - RTPHeaderLength()  // RTP overhead.
           - video_->FECPacketOverhead()            // FEC/ULP/RED overhead.
           - ((rtx) ? 2 : 0);                       // RTX overhead.
  }
}

size_t RTPSender::MaxPayloadLength() const {
  return max_payload_length_;
}

uint16_t RTPSender::PacketOverHead() const { return packet_over_head_; }

void RTPSender::SetRtxStatus(int mode) {
  CriticalSectionScoped cs(send_critsect_.get());
  rtx_ = mode;
}

int RTPSender::RtxStatus() const {
  CriticalSectionScoped cs(send_critsect_.get());
  return rtx_;
}

void RTPSender::SetRtxSsrc(uint32_t ssrc) {
  CriticalSectionScoped cs(send_critsect_.get());
  ssrc_rtx_ = ssrc;
}

uint32_t RTPSender::RtxSsrc() const {
  CriticalSectionScoped cs(send_critsect_.get());
  return ssrc_rtx_;
}

void RTPSender::SetRtxPayloadType(int payload_type) {
  CriticalSectionScoped cs(send_critsect_.get());
  payload_type_rtx_ = payload_type;
}

int32_t RTPSender::CheckPayloadType(int8_t payload_type,
                                    RtpVideoCodecTypes* video_type) {
  CriticalSectionScoped cs(send_critsect_.get());

  if (payload_type < 0) {
    LOG(LS_ERROR) << "Invalid payload_type " << payload_type;
    return -1;
  }
  if (audio_configured_) {
    int8_t red_pl_type = -1;
    if (audio_->RED(red_pl_type) == 0) {
      // We have configured RED.
      if (red_pl_type == payload_type) {
        // And it's a match...
        return 0;
      }
    }
  }
  if (payload_type_ == payload_type) {
    if (!audio_configured_) {
      *video_type = video_->VideoCodecType();
    }
    return 0;
  }
  std::map<int8_t, RtpUtility::Payload*>::iterator it =
      payload_type_map_.find(payload_type);
  if (it == payload_type_map_.end()) {
    LOG(LS_WARNING) << "Payload type " << payload_type << " not registered.";
    return -1;
  }
  SetSendPayloadType(payload_type);
  RtpUtility::Payload* payload = it->second;
  assert(payload);
  if (!payload->audio && !audio_configured_) {
    video_->SetVideoCodecType(payload->typeSpecific.Video.videoCodecType);
    *video_type = payload->typeSpecific.Video.videoCodecType;
    video_->SetMaxConfiguredBitrateVideo(payload->typeSpecific.Video.maxRate);
  }
  return 0;
}

RTPSenderInterface::CVOMode RTPSender::ActivateCVORtpHeaderExtension() {
  if (cvo_mode_ == kCVOInactive) {
    CriticalSectionScoped cs(send_critsect_.get());
    if (rtp_header_extension_map_.SetActive(kRtpExtensionVideoRotation, true)) {
      cvo_mode_ = kCVOActivated;
    }
  }
  return cvo_mode_;
}

int32_t RTPSender::SendOutgoingData(FrameType frame_type,
                                    int8_t payload_type,
                                    uint32_t capture_timestamp,
                                    int64_t capture_time_ms,
                                    const uint8_t* payload_data,
                                    size_t payload_size,
                                    const RTPFragmentationHeader* fragmentation,
                                    VideoCodecInformation* codec_info,
                                    const RTPVideoHeader* rtp_hdr) {
  uint32_t ssrc;
  {
    // Drop this packet if we're not sending media packets.
    CriticalSectionScoped cs(send_critsect_.get());
    ssrc = ssrc_;
    if (!sending_media_) {
      return 0;
    }
  }
  RtpVideoCodecTypes video_type = kRtpVideoGeneric;
  if (CheckPayloadType(payload_type, &video_type) != 0) {
    LOG(LS_ERROR) << "Don't send data with unknown payload type.";
    return -1;
  }

  uint32_t ret_val;
  if (audio_configured_) {
    TRACE_EVENT_ASYNC_STEP1("webrtc", "Audio", capture_timestamp,
                            "Send", "type", FrameTypeToString(frame_type));
    assert(frame_type == kAudioFrameSpeech || frame_type == kAudioFrameCN ||
           frame_type == kFrameEmpty);

    ret_val = audio_->SendAudio(frame_type, payload_type, capture_timestamp,
                                payload_data, payload_size, fragmentation);
  } else {
    TRACE_EVENT_ASYNC_STEP1("webrtc", "Video", capture_time_ms,
                            "Send", "type", FrameTypeToString(frame_type));
    assert(frame_type != kAudioFrameSpeech && frame_type != kAudioFrameCN);

    if (frame_type == kFrameEmpty)
      return 0;

    ret_val =
        video_->SendVideo(video_type, frame_type, payload_type,
                          capture_timestamp, capture_time_ms, payload_data,
                          payload_size, fragmentation, codec_info, rtp_hdr);
  }

  CriticalSectionScoped cs(statistics_crit_.get());
  // Note: This is currently only counting for video.
  if (frame_type == kVideoFrameKey) {
    ++frame_counts_.key_frames;
  } else if (frame_type == kVideoFrameDelta) {
    ++frame_counts_.delta_frames;
  }
  if (frame_count_observer_) {
    frame_count_observer_->FrameCountUpdated(frame_counts_, ssrc);
  }

  return ret_val;
}

size_t RTPSender::TrySendRedundantPayloads(size_t bytes_to_send) {
  {
    CriticalSectionScoped cs(send_critsect_.get());
    if ((rtx_ & kRtxRedundantPayloads) == 0)
      return 0;
  }

  uint8_t buffer[IP_PACKET_SIZE];
  int bytes_left = static_cast<int>(bytes_to_send);
  while (bytes_left > 0) {
    size_t length = bytes_left;
    int64_t capture_time_ms;
    if (!packet_history_.GetBestFittingPacket(buffer, &length,
                                              &capture_time_ms)) {
      break;
    }
    if (!PrepareAndSendPacket(buffer, length, capture_time_ms, true, false))
      break;
    RtpUtility::RtpHeaderParser rtp_parser(buffer, length);
    RTPHeader rtp_header;
    rtp_parser.Parse(rtp_header);
    bytes_left -= static_cast<int>(length - rtp_header.headerLength);
  }
  return bytes_to_send - bytes_left;
}

size_t RTPSender::BuildPaddingPacket(uint8_t* packet, size_t header_length) {
  size_t padding_bytes_in_packet = kMaxPaddingLength;
  packet[0] |= 0x20;  // Set padding bit.
  int32_t *data =
      reinterpret_cast<int32_t *>(&(packet[header_length]));

  // Fill data buffer with random data.
  for (size_t j = 0; j < (padding_bytes_in_packet >> 2); ++j) {
    data[j] = rand();  // NOLINT
  }
  // Set number of padding bytes in the last byte of the packet.
  packet[header_length + padding_bytes_in_packet - 1] =
      static_cast<uint8_t>(padding_bytes_in_packet);
  return padding_bytes_in_packet;
}

size_t RTPSender::TrySendPadData(size_t bytes) {
  int64_t capture_time_ms;
  uint32_t timestamp;
  {
    CriticalSectionScoped cs(send_critsect_.get());
    timestamp = timestamp_;
    capture_time_ms = capture_time_ms_;
    if (last_timestamp_time_ms_ > 0) {
      timestamp +=
          (clock_->TimeInMilliseconds() - last_timestamp_time_ms_) * 90;
      capture_time_ms +=
          (clock_->TimeInMilliseconds() - last_timestamp_time_ms_);
    }
  }
  return SendPadData(timestamp, capture_time_ms, bytes);
}

size_t RTPSender::SendPadData(uint32_t timestamp,
                              int64_t capture_time_ms,
                              size_t bytes) {
  size_t padding_bytes_in_packet = 0;
  size_t bytes_sent = 0;
  for (; bytes > 0; bytes -= padding_bytes_in_packet) {
    // Always send full padding packets.
    if (bytes < kMaxPaddingLength)
      bytes = kMaxPaddingLength;

    uint32_t ssrc;
    uint16_t sequence_number;
    int payload_type;
    bool over_rtx;
    {
      CriticalSectionScoped cs(send_critsect_.get());
      // Only send padding packets following the last packet of a frame,
      // indicated by the marker bit.
      if (rtx_ == kRtxOff) {
        // Without RTX we can't send padding in the middle of frames.
        if (!last_packet_marker_bit_)
          return 0;
        ssrc = ssrc_;
        sequence_number = sequence_number_;
        ++sequence_number_;
        payload_type = payload_type_;
        over_rtx = false;
      } else {
        // Without abs-send-time a media packet must be sent before padding so
        // that the timestamps used for estimation are correct.
        if (!media_has_been_sent_ && !rtp_header_extension_map_.IsRegistered(
            kRtpExtensionAbsoluteSendTime))
          return 0;
        ssrc = ssrc_rtx_;
        sequence_number = sequence_number_rtx_;
        ++sequence_number_rtx_;
        payload_type = ((rtx_ & kRtxRedundantPayloads) > 0) ? payload_type_rtx_
                                                            : payload_type_;
        over_rtx = true;
      }
    }

    uint8_t padding_packet[IP_PACKET_SIZE];
    size_t header_length =
        CreateRtpHeader(padding_packet, payload_type, ssrc, false, timestamp,
                        sequence_number, std::vector<uint32_t>());
    assert(header_length != static_cast<size_t>(-1));
    padding_bytes_in_packet = BuildPaddingPacket(padding_packet, header_length);
    assert(padding_bytes_in_packet <= bytes);
    size_t length = padding_bytes_in_packet + header_length;
    int64_t now_ms = clock_->TimeInMilliseconds();

    RtpUtility::RtpHeaderParser rtp_parser(padding_packet, length);
    RTPHeader rtp_header;
    rtp_parser.Parse(rtp_header);

    if (capture_time_ms > 0) {
      UpdateTransmissionTimeOffset(
          padding_packet, length, rtp_header, now_ms - capture_time_ms);
    }

    UpdateAbsoluteSendTime(padding_packet, length, rtp_header, now_ms);
    if (!SendPacketToNetwork(padding_packet, length))
      break;
    bytes_sent += padding_bytes_in_packet;
    UpdateRtpStats(padding_packet, length, rtp_header, over_rtx, false);
  }

  return bytes_sent;
}

void RTPSender::SetStorePacketsStatus(bool enable, uint16_t number_to_store) {
  packet_history_.SetStorePacketsStatus(enable, number_to_store);
}

bool RTPSender::StorePackets() const {
  return packet_history_.StorePackets();
}

int32_t RTPSender::ReSendPacket(uint16_t packet_id, int64_t min_resend_time) {
  size_t length = IP_PACKET_SIZE;
  uint8_t data_buffer[IP_PACKET_SIZE];
  int64_t capture_time_ms;
  if (!packet_history_.GetPacketAndSetSendTime(packet_id, min_resend_time, true,
                                               data_buffer, &length,
                                               &capture_time_ms)) {
    // Packet not found.
    return 0;
  }

  if (paced_sender_) {
    RtpUtility::RtpHeaderParser rtp_parser(data_buffer, length);
    RTPHeader header;
    if (!rtp_parser.Parse(header)) {
      assert(false);
      return -1;
    }
    // Convert from TickTime to Clock since capture_time_ms is based on
    // TickTime.
    int64_t corrected_capture_tims_ms = capture_time_ms + clock_delta_ms_;
    if (!paced_sender_->SendPacket(
            PacedSender::kHighPriority, header.ssrc, header.sequenceNumber,
            corrected_capture_tims_ms, length - header.headerLength, true)) {
      // We can't send the packet right now.
      // We will be called when it is time.
      return length;
    }
  }
  int rtx = kRtxOff;
  {
    CriticalSectionScoped lock(send_critsect_.get());
    rtx = rtx_;
  }
  return PrepareAndSendPacket(data_buffer, length, capture_time_ms,
                              (rtx & kRtxRetransmitted) > 0, true) ?
      static_cast<int32_t>(length) : -1;
}

bool RTPSender::SendPacketToNetwork(const uint8_t *packet, size_t size) {
  int bytes_sent = -1;
  if (transport_) {
    bytes_sent = transport_->SendPacket(id_, packet, size);
  }
  TRACE_EVENT_INSTANT2(TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"),
                       "RTPSender::SendPacketToNetwork", "size", size, "sent",
                       bytes_sent);
  // TODO(pwestin): Add a separate bitrate for sent bitrate after pacer.
  if (bytes_sent <= 0) {
    LOG(LS_WARNING) << "Transport failed to send packet";
    return false;
  }
  return true;
}

int RTPSender::SelectiveRetransmissions() const {
  if (!video_)
    return -1;
  return video_->SelectiveRetransmissions();
}

int RTPSender::SetSelectiveRetransmissions(uint8_t settings) {
  if (!video_)
    return -1;
  return video_->SetSelectiveRetransmissions(settings);
}

void RTPSender::OnReceivedNACK(const std::list<uint16_t>& nack_sequence_numbers,
                               int64_t avg_rtt) {
  TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"),
               "RTPSender::OnReceivedNACK", "num_seqnum",
               nack_sequence_numbers.size(), "avg_rtt", avg_rtt);
  const int64_t now = clock_->TimeInMilliseconds();
  uint32_t bytes_re_sent = 0;
  uint32_t target_bitrate = GetTargetBitrate();

  // Enough bandwidth to send NACK?
  if (!ProcessNACKBitRate(now)) {
    LOG(LS_INFO) << "NACK bitrate reached. Skip sending NACK response. Target "
                 << target_bitrate;
    return;
  }

  for (std::list<uint16_t>::const_iterator it = nack_sequence_numbers.begin();
      it != nack_sequence_numbers.end(); ++it) {
    const int32_t bytes_sent = ReSendPacket(*it, 5 + avg_rtt);
    if (bytes_sent > 0) {
      bytes_re_sent += bytes_sent;
    } else if (bytes_sent == 0) {
      // The packet has previously been resent.
      // Try resending next packet in the list.
      continue;
    } else {
      // Failed to send one Sequence number. Give up the rest in this nack.
      LOG(LS_WARNING) << "Failed resending RTP packet " << *it
                      << ", Discard rest of packets";
      break;
    }
    // Delay bandwidth estimate (RTT * BW).
    if (target_bitrate != 0 && avg_rtt) {
      // kbits/s * ms = bits => bits/8 = bytes
      size_t target_bytes =
          (static_cast<size_t>(target_bitrate / 1000) * avg_rtt) >> 3;
      if (bytes_re_sent > target_bytes) {
        break;  // Ignore the rest of the packets in the list.
      }
    }
  }
  if (bytes_re_sent > 0) {
    UpdateNACKBitRate(bytes_re_sent, now);
  }
}

bool RTPSender::ProcessNACKBitRate(uint32_t now) {
  uint32_t num = 0;
  size_t byte_count = 0;
  const uint32_t kAvgIntervalMs = 1000;
  uint32_t target_bitrate = GetTargetBitrate();

  CriticalSectionScoped cs(send_critsect_.get());

  if (target_bitrate == 0) {
    return true;
  }
  for (num = 0; num < NACK_BYTECOUNT_SIZE; ++num) {
    if ((now - nack_byte_count_times_[num]) > kAvgIntervalMs) {
      // Don't use data older than 1sec.
      break;
    } else {
      byte_count += nack_byte_count_[num];
    }
  }
  uint32_t time_interval = kAvgIntervalMs;
  if (num == NACK_BYTECOUNT_SIZE) {
    // More than NACK_BYTECOUNT_SIZE nack messages has been received
    // during the last msg_interval.
    if (nack_byte_count_times_[num - 1] <= now) {
      time_interval = now - nack_byte_count_times_[num - 1];
    }
  }
  return (byte_count * 8) < (target_bitrate / 1000 * time_interval);
}

void RTPSender::UpdateNACKBitRate(uint32_t bytes, int64_t now) {
  CriticalSectionScoped cs(send_critsect_.get());
  if (bytes == 0)
    return;
  nack_bitrate_.Update(bytes);
  // Save bitrate statistics.
  // Shift all but first time.
  for (int i = NACK_BYTECOUNT_SIZE - 2; i >= 0; i--) {
    nack_byte_count_[i + 1] = nack_byte_count_[i];
    nack_byte_count_times_[i + 1] = nack_byte_count_times_[i];
  }
  nack_byte_count_[0] = bytes;
  nack_byte_count_times_[0] = now;
}

// Called from pacer when we can send the packet.
bool RTPSender::TimeToSendPacket(uint16_t sequence_number,
                                 int64_t capture_time_ms,
                                 bool retransmission) {
  size_t length = IP_PACKET_SIZE;
  uint8_t data_buffer[IP_PACKET_SIZE];
  int64_t stored_time_ms;

  if (!packet_history_.GetPacketAndSetSendTime(sequence_number,
                                               0,
                                               retransmission,
                                               data_buffer,
                                               &length,
                                               &stored_time_ms)) {
    // Packet cannot be found. Allow sending to continue.
    return true;
  }
  if (!retransmission && capture_time_ms > 0) {
    UpdateDelayStatistics(capture_time_ms, clock_->TimeInMilliseconds());
  }
  int rtx;
  {
    CriticalSectionScoped lock(send_critsect_.get());
    rtx = rtx_;
  }
  return PrepareAndSendPacket(data_buffer,
                              length,
                              capture_time_ms,
                              retransmission && (rtx & kRtxRetransmitted) > 0,
                              retransmission);
}

bool RTPSender::PrepareAndSendPacket(uint8_t* buffer,
                                     size_t length,
                                     int64_t capture_time_ms,
                                     bool send_over_rtx,
                                     bool is_retransmit) {
  uint8_t *buffer_to_send_ptr = buffer;

  RtpUtility::RtpHeaderParser rtp_parser(buffer, length);
  RTPHeader rtp_header;
  rtp_parser.Parse(rtp_header);
  if (!is_retransmit && rtp_header.markerBit) {
    TRACE_EVENT_ASYNC_END0(TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"), "PacedSend",
                           capture_time_ms);
  }

  TRACE_EVENT_INSTANT2(
      TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"), "PrepareAndSendPacket",
      "timestamp", rtp_header.timestamp, "seqnum", rtp_header.sequenceNumber);

  uint8_t data_buffer_rtx[IP_PACKET_SIZE];
  if (send_over_rtx) {
    BuildRtxPacket(buffer, &length, data_buffer_rtx);
    buffer_to_send_ptr = data_buffer_rtx;
  }

  int64_t now_ms = clock_->TimeInMilliseconds();
  int64_t diff_ms = now_ms - capture_time_ms;
  UpdateTransmissionTimeOffset(buffer_to_send_ptr, length, rtp_header,
                               diff_ms);
  UpdateAbsoluteSendTime(buffer_to_send_ptr, length, rtp_header, now_ms);
  bool ret = SendPacketToNetwork(buffer_to_send_ptr, length);
  if (ret) {
    CriticalSectionScoped lock(send_critsect_.get());
    media_has_been_sent_ = true;
  }
  UpdateRtpStats(buffer_to_send_ptr, length, rtp_header, send_over_rtx,
                 is_retransmit);
  return ret;
}

void RTPSender::UpdateRtpStats(const uint8_t* buffer,
                               size_t packet_length,
                               const RTPHeader& header,
                               bool is_rtx,
                               bool is_retransmit) {
  StreamDataCounters* counters;
  // Get ssrc before taking statistics_crit_ to avoid possible deadlock.
  uint32_t ssrc = is_rtx ? RtxSsrc() : SSRC();

  CriticalSectionScoped lock(statistics_crit_.get());
  if (is_rtx) {
    counters = &rtx_rtp_stats_;
  } else {
    counters = &rtp_stats_;
  }

  total_bitrate_sent_.Update(packet_length);

  if (counters->first_packet_time_ms == -1) {
    counters->first_packet_time_ms = clock_->TimeInMilliseconds();
  }
  if (IsFecPacket(buffer, header)) {
    counters->fec.AddPacket(packet_length, header);
  }
  if (is_retransmit) {
    counters->retransmitted.AddPacket(packet_length, header);
  }
  counters->transmitted.AddPacket(packet_length, header);

  if (rtp_stats_callback_) {
    rtp_stats_callback_->DataCountersUpdated(*counters, ssrc);
  }
}

bool RTPSender::IsFecPacket(const uint8_t* buffer,
                            const RTPHeader& header) const {
  if (!video_) {
    return false;
  }
  bool fec_enabled;
  uint8_t pt_red;
  uint8_t pt_fec;
  video_->GenericFECStatus(fec_enabled, pt_red, pt_fec);
  return fec_enabled &&
      header.payloadType == pt_red &&
      buffer[header.headerLength] == pt_fec;
}

size_t RTPSender::TimeToSendPadding(size_t bytes) {
  {
    CriticalSectionScoped cs(send_critsect_.get());
    if (!sending_media_) return 0;
  }
  if (bytes == 0)
    return 0;
  size_t bytes_sent = TrySendRedundantPayloads(bytes);
  if (bytes_sent < bytes)
    bytes_sent += TrySendPadData(bytes - bytes_sent);
  return bytes_sent;
}

// TODO(pwestin): send in the RtpHeaderParser to avoid parsing it again.
int32_t RTPSender::SendToNetwork(
    uint8_t *buffer, size_t payload_length, size_t rtp_header_length,
    int64_t capture_time_ms, StorageType storage,
    PacedSender::Priority priority) {
  RtpUtility::RtpHeaderParser rtp_parser(buffer,
                                         payload_length + rtp_header_length);
  RTPHeader rtp_header;
  rtp_parser.Parse(rtp_header);

  int64_t now_ms = clock_->TimeInMilliseconds();

  // |capture_time_ms| <= 0 is considered invalid.
  // TODO(holmer): This should be changed all over Video Engine so that negative
  // time is consider invalid, while 0 is considered a valid time.
  if (capture_time_ms > 0) {
    UpdateTransmissionTimeOffset(buffer, payload_length + rtp_header_length,
                                 rtp_header, now_ms - capture_time_ms);
  }

  UpdateAbsoluteSendTime(buffer, payload_length + rtp_header_length,
                         rtp_header, now_ms);

  // Used for NACK and to spread out the transmission of packets.
  if (packet_history_.PutRTPPacket(buffer, rtp_header_length + payload_length,
                                   max_payload_length_, capture_time_ms,
                                   storage) != 0) {
    return -1;
  }

  if (paced_sender_ && storage != kDontStore) {
    // Correct offset between implementations of millisecond time stamps in
    // TickTime and Clock.
    int64_t corrected_time_ms = capture_time_ms + clock_delta_ms_;
    if (!paced_sender_->SendPacket(priority, rtp_header.ssrc,
                                   rtp_header.sequenceNumber, corrected_time_ms,
                                   payload_length, false)) {
      if (last_capture_time_ms_sent_ == 0 ||
          corrected_time_ms > last_capture_time_ms_sent_) {
        last_capture_time_ms_sent_ = corrected_time_ms;
        TRACE_EVENT_ASYNC_BEGIN1(TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"),
                                 "PacedSend", corrected_time_ms,
                                 "capture_time_ms", corrected_time_ms);
      }
      // We can't send the packet right now.
      // We will be called when it is time.
      return 0;
    }
  }
  if (capture_time_ms > 0) {
    UpdateDelayStatistics(capture_time_ms, now_ms);
  }

  size_t length = payload_length + rtp_header_length;
  bool sent = SendPacketToNetwork(buffer, length);

  if (storage != kDontStore) {
    // Mark the packet as sent in the history even if send failed. Dropping a
    // packet here should be treated as any other packet drop so we should be
    // ready for a retransmission.
    packet_history_.SetSent(rtp_header.sequenceNumber);
  }
  if (!sent)
    return -1;

  {
    CriticalSectionScoped lock(send_critsect_.get());
    media_has_been_sent_ = true;
  }
  UpdateRtpStats(buffer, length, rtp_header, false, false);
  return 0;
}

void RTPSender::UpdateDelayStatistics(int64_t capture_time_ms, int64_t now_ms) {
  uint32_t ssrc;
  int avg_delay_ms = 0;
  int max_delay_ms = 0;
  {
    CriticalSectionScoped lock(send_critsect_.get());
    ssrc = ssrc_;
  }
  {
    CriticalSectionScoped cs(statistics_crit_.get());
    // TODO(holmer): Compute this iteratively instead.
    send_delays_[now_ms] = now_ms - capture_time_ms;
    send_delays_.erase(send_delays_.begin(),
                       send_delays_.lower_bound(now_ms -
                       kSendSideDelayWindowMs));
  }
  if (send_side_delay_observer_ &&
      GetSendSideDelay(&avg_delay_ms, &max_delay_ms)) {
    send_side_delay_observer_->SendSideDelayUpdated(avg_delay_ms,
        max_delay_ms, ssrc);
  }
}

void RTPSender::ProcessBitrate() {
  CriticalSectionScoped cs(send_critsect_.get());
  total_bitrate_sent_.Process();
  nack_bitrate_.Process();
  if (audio_configured_) {
    return;
  }
  video_->ProcessBitrate();
}

size_t RTPSender::RTPHeaderLength() const {
  CriticalSectionScoped lock(send_critsect_.get());
  size_t rtp_header_length = kRtpHeaderLength;
  rtp_header_length += sizeof(uint32_t) * csrcs_.size();
  rtp_header_length += RtpHeaderExtensionTotalLength();
  return rtp_header_length;
}

uint16_t RTPSender::IncrementSequenceNumber() {
  CriticalSectionScoped cs(send_critsect_.get());
  return sequence_number_++;
}

void RTPSender::ResetDataCounters() {
  uint32_t ssrc;
  uint32_t ssrc_rtx;
  bool report_rtx;
  {
    CriticalSectionScoped ssrc_lock(send_critsect_.get());
    ssrc = ssrc_;
    ssrc_rtx = ssrc_rtx_;
    report_rtx = rtx_ != kRtxOff;
  }
  CriticalSectionScoped lock(statistics_crit_.get());
  rtp_stats_ = StreamDataCounters();
  rtx_rtp_stats_ = StreamDataCounters();
  if (rtp_stats_callback_) {
    rtp_stats_callback_->DataCountersUpdated(rtp_stats_, ssrc);
    if (report_rtx)
      rtp_stats_callback_->DataCountersUpdated(rtx_rtp_stats_, ssrc_rtx);
  }
}

void RTPSender::GetDataCounters(StreamDataCounters* rtp_stats,
                                StreamDataCounters* rtx_stats) const {
  CriticalSectionScoped lock(statistics_crit_.get());
  *rtp_stats = rtp_stats_;
  *rtx_stats = rtx_rtp_stats_;
}

size_t RTPSender::CreateRtpHeader(uint8_t* header,
                                  int8_t payload_type,
                                  uint32_t ssrc,
                                  bool marker_bit,
                                  uint32_t timestamp,
                                  uint16_t sequence_number,
                                  const std::vector<uint32_t>& csrcs) const {
  header[0] = 0x80;  // version 2.
  header[1] = static_cast<uint8_t>(payload_type);
  if (marker_bit) {
    header[1] |= kRtpMarkerBitMask;  // Marker bit is set.
  }
  ByteWriter<uint16_t>::WriteBigEndian(header + 2, sequence_number);
  ByteWriter<uint32_t>::WriteBigEndian(header + 4, timestamp);
  ByteWriter<uint32_t>::WriteBigEndian(header + 8, ssrc);
  int32_t rtp_header_length = kRtpHeaderLength;

  if (csrcs.size() > 0) {
    uint8_t *ptr = &header[rtp_header_length];
    for (size_t i = 0; i < csrcs.size(); ++i) {
      ByteWriter<uint32_t>::WriteBigEndian(ptr, csrcs[i]);
      ptr += 4;
    }
    header[0] = (header[0] & 0xf0) | csrcs.size();

    // Update length of header.
    rtp_header_length += sizeof(uint32_t) * csrcs.size();
  }

  uint16_t len =
      BuildRTPHeaderExtension(header + rtp_header_length, marker_bit);
  if (len > 0) {
    header[0] |= 0x10;  // Set extension bit.
    rtp_header_length += len;
  }
  return rtp_header_length;
}

int32_t RTPSender::BuildRTPheader(uint8_t* data_buffer,
                                  int8_t payload_type,
                                  bool marker_bit,
                                  uint32_t capture_timestamp,
                                  int64_t capture_time_ms,
                                  bool timestamp_provided,
                                  bool inc_sequence_number) {
  assert(payload_type >= 0);
  CriticalSectionScoped cs(send_critsect_.get());

  if (timestamp_provided) {
    timestamp_ = start_timestamp_ + capture_timestamp;
  } else {
    // Make a unique time stamp.
    // We can't inc by the actual time, since then we increase the risk of back
    // timing.
    timestamp_++;
  }
  last_timestamp_time_ms_ = clock_->TimeInMilliseconds();
  uint32_t sequence_number = sequence_number_++;
  capture_time_ms_ = capture_time_ms;
  last_packet_marker_bit_ = marker_bit;
  return CreateRtpHeader(data_buffer, payload_type, ssrc_, marker_bit,
                         timestamp_, sequence_number, csrcs_);
}

uint16_t RTPSender::BuildRTPHeaderExtension(uint8_t* data_buffer,
                                            bool marker_bit) const {
  if (rtp_header_extension_map_.Size() <= 0) {
    return 0;
  }
  // RTP header extension, RFC 3550.
  //   0                   1                   2                   3
  //   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //  |      defined by profile       |           length              |
  //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //  |                        header extension                       |
  //  |                             ....                              |
  //
  const uint32_t kPosLength = 2;
  const uint32_t kHeaderLength = kRtpOneByteHeaderLength;

  // Add extension ID (0xBEDE).
  ByteWriter<uint16_t>::WriteBigEndian(data_buffer,
                                       kRtpOneByteHeaderExtensionId);

  // Add extensions.
  uint16_t total_block_length = 0;

  RTPExtensionType type = rtp_header_extension_map_.First();
  while (type != kRtpExtensionNone) {
    uint8_t block_length = 0;
    uint8_t* extension_data = &data_buffer[kHeaderLength + total_block_length];
    switch (type) {
      case kRtpExtensionTransmissionTimeOffset:
        block_length = BuildTransmissionTimeOffsetExtension(extension_data);
        break;
      case kRtpExtensionAudioLevel:
        block_length = BuildAudioLevelExtension(extension_data);
        break;
      case kRtpExtensionAbsoluteSendTime:
        block_length = BuildAbsoluteSendTimeExtension(extension_data);
        break;
      case kRtpExtensionVideoRotation:
        block_length = BuildVideoRotationExtension(extension_data);
        break;
      case kRtpExtensionTransportSequenceNumber:
        block_length = BuildTransportSequenceNumberExtension(extension_data);
        break;
      case kRtpExtensionRtpStreamId:
        block_length = BuildRIDExtension(extension_data);
        break;
      default:
        assert(false);
    }
    total_block_length += block_length;
    type = rtp_header_extension_map_.Next(type);
  }
  if (total_block_length == 0) {
    // No extension added.
    return 0;
  }
  // Add padding elements until we've filled a 32 bit block.
  size_t padding_bytes =
      RtpUtility::Word32Align(total_block_length) - total_block_length;
  if (padding_bytes > 0) {
    memset(&data_buffer[kHeaderLength + total_block_length], 0, padding_bytes);
    total_block_length += padding_bytes;
  }
  // Set header length (in number of Word32, header excluded).
  ByteWriter<uint16_t>::WriteBigEndian(data_buffer + kPosLength,
                                       total_block_length / 4);
  // Total added length.
  return kHeaderLength + total_block_length;
}

uint8_t RTPSender::BuildTransmissionTimeOffsetExtension(
    uint8_t* data_buffer) const {
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
  uint8_t id;
  if (rtp_header_extension_map_.GetId(kRtpExtensionTransmissionTimeOffset,
                                      &id) != 0) {
    // Not registered.
    return 0;
  }
  size_t pos = 0;
  const uint8_t len = 2;
  data_buffer[pos++] = (id << 4) + len;
  ByteWriter<int32_t, 3>::WriteBigEndian(data_buffer + pos,
                                         transmission_time_offset_);
  pos += 3;
  assert(pos == kTransmissionTimeOffsetLength);
  return kTransmissionTimeOffsetLength;
}

uint8_t RTPSender::BuildAudioLevelExtension(uint8_t* data_buffer) const {
  // An RTP Header Extension for Client-to-Mixer Audio Level Indication
  //
  // https://datatracker.ietf.org/doc/draft-lennox-avt-rtp-audio-level-exthdr/
  //
  // The form of the audio level extension block:
  //
  //    0                   1
  //    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |  ID   | len=0 |V|   level     |
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //

  // Get id defined by user.
  uint8_t id;
  if (rtp_header_extension_map_.GetId(kRtpExtensionAudioLevel, &id) != 0) {
    // Not registered.
    return 0;
  }
  size_t pos = 0;
  const uint8_t len = 0;
  data_buffer[pos++] = (id << 4) + len;
  data_buffer[pos++] = (1 << 7) + 0;     // Voice, 0 dBov.
  assert(pos == kAudioLevelLength);
  return kAudioLevelLength;
}

uint8_t RTPSender::BuildAbsoluteSendTimeExtension(uint8_t* data_buffer) const {
  // Absolute send time in RTP streams.
  //
  // The absolute send time is signaled to the receiver in-band using the
  // general mechanism for RTP header extensions [RFC5285]. The payload
  // of this extension (the transmitted value) is a 24-bit unsigned integer
  // containing the sender's current time in seconds as a fixed point number
  // with 18 bits fractional part.
  //
  // The form of the absolute send time extension block:
  //
  //    0                   1                   2                   3
  //    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |  ID   | len=2 |              absolute send time               |
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

  // Get id defined by user.
  uint8_t id;
  if (rtp_header_extension_map_.GetId(kRtpExtensionAbsoluteSendTime,
                                      &id) != 0) {
    // Not registered.
    return 0;
  }
  size_t pos = 0;
  const uint8_t len = 2;
  data_buffer[pos++] = (id << 4) + len;
  ByteWriter<uint32_t, 3>::WriteBigEndian(data_buffer + pos,
                                          absolute_send_time_);
  pos += 3;
  assert(pos == kAbsoluteSendTimeLength);
  return kAbsoluteSendTimeLength;
}

uint8_t RTPSender::BuildVideoRotationExtension(uint8_t* data_buffer) const {
  // Coordination of Video Orientation in RTP streams.
  //
  // Coordination of Video Orientation consists in signaling of the current
  // orientation of the image captured on the sender side to the receiver for
  // appropriate rendering and displaying.
  //
  //    0                   1
  //    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |  ID   | len=0 |0 0 0 0 C F R R|
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //

  // Get id defined by user.
  uint8_t id;
  if (rtp_header_extension_map_.GetId(kRtpExtensionVideoRotation, &id) != 0) {
    // Not registered.
    return 0;
  }
  size_t pos = 0;
  const uint8_t len = 0;
  data_buffer[pos++] = (id << 4) + len;
  data_buffer[pos++] = ConvertVideoRotationToCVOByte(rotation_);
  assert(pos == kVideoRotationLength);
  return kVideoRotationLength;
}

uint8_t RTPSender::BuildTransportSequenceNumberExtension(
    uint8_t* data_buffer) const {
  //   0                   1                   2
  //   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
  //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //  |  ID   | L=1   |transport wide sequence number |
  //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

  // Get id defined by user.
  uint8_t id;
  if (rtp_header_extension_map_.GetId(kRtpExtensionTransportSequenceNumber,
                                      &id) != 0) {
    // Not registered.
    return 0;
  }
  size_t pos = 0;
  const uint8_t len = 1;
  data_buffer[pos++] = (id << 4) + len;
  ByteWriter<uint16_t>::WriteBigEndian(data_buffer + pos,
                                       transport_sequence_number_);
  pos += 2;
  assert(pos == kTransportSequenceNumberLength);
  return kTransportSequenceNumberLength;
}

uint8_t RTPSender::BuildRIDExtension(
    uint8_t* data_buffer) const {
  //   0                   1                   2
  //   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
  //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //  |  ID   | L=?   |UTF-8 RID value......          |...
  //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

  // Get id defined by user.
  uint8_t id;
  if (!rid_ ||
    rtp_header_extension_map_.GetId(kRtpExtensionRtpStreamId,
                                      &id) != 0) {
    // No RtpStreamId or not registered
    return 0;
  }
  size_t pos = 0;
  // RID value is not null-terminated in header, so no +1
  const uint8_t len = strlen(rid_);
  if (len > 16 || len == 0) {
    LOG(LS_ERROR) << "Failed to add RID header because of unsupported RID"
                    " length: " << len;
    return 0;
  }
  data_buffer[pos++] = (id << 4) + (len - 1);
  memcpy(data_buffer + pos, rid_, len);
  pos += len;
  return pos;
}

bool RTPSender::FindHeaderExtensionPosition(RTPExtensionType type,
                                            const uint8_t* rtp_packet,
                                            size_t rtp_packet_length,
                                            const RTPHeader& rtp_header,
                                            size_t* position) const {
  // Get length until start of header extension block.
  int extension_block_pos =
      rtp_header_extension_map_.GetLengthUntilBlockStartInBytes(type);
  if (extension_block_pos < 0) {
    LOG(LS_WARNING) << "Failed to find extension position for " << type
                    << " as it is not registered.";
    return false;
  }

  HeaderExtension header_extension(type);

  size_t block_pos =
      kRtpHeaderLength + rtp_header.numCSRCs + extension_block_pos;
  if (rtp_packet_length < block_pos + header_extension.length ||
      rtp_header.headerLength < block_pos + header_extension.length) {
    LOG(LS_WARNING) << "Failed to find extension position for " << type
                    << " as the length is invalid.";
    return false;
  }

  // Verify that header contains extension.
  if (!((rtp_packet[kRtpHeaderLength + rtp_header.numCSRCs] == 0xBE) &&
        (rtp_packet[kRtpHeaderLength + rtp_header.numCSRCs + 1] == 0xDE))) {
    LOG(LS_WARNING) << "Failed to find extension position for " << type
                    << "as hdr extension not found.";
    return false;
  }

  *position = block_pos;
  return true;
}

void RTPSender::UpdateTransmissionTimeOffset(uint8_t* rtp_packet,
                                             size_t rtp_packet_length,
                                             const RTPHeader& rtp_header,
                                             int64_t time_diff_ms) const {
  CriticalSectionScoped cs(send_critsect_.get());
  // Get id.
  uint8_t id = 0;
  if (rtp_header_extension_map_.GetId(kRtpExtensionTransmissionTimeOffset,
                                      &id) != 0) {
    // Not registered.
    return;
  }

  size_t block_pos = 0;
  if (!FindHeaderExtensionPosition(kRtpExtensionTransmissionTimeOffset,
                                   rtp_packet, rtp_packet_length, rtp_header,
                                   &block_pos)) {
    LOG(LS_WARNING) << "Failed to update transmission time offset.";
    return;
  }

  // Verify first byte in block.
  const uint8_t first_block_byte = (id << 4) + 2;
  if (rtp_packet[block_pos] != first_block_byte) {
    LOG(LS_WARNING) << "Failed to update transmission time offset.";
    return;
  }
  // Update transmission offset field (converting to a 90 kHz timestamp).
  ByteWriter<int32_t, 3>::WriteBigEndian(rtp_packet + block_pos + 1,
                                         time_diff_ms * 90);  // RTP timestamp.
}

bool RTPSender::UpdateAudioLevel(uint8_t* rtp_packet,
                                 size_t rtp_packet_length,
                                 const RTPHeader& rtp_header,
                                 bool is_voiced,
                                 uint8_t dBov) const {
  CriticalSectionScoped cs(send_critsect_.get());

  // Get id.
  uint8_t id = 0;
  if (rtp_header_extension_map_.GetId(kRtpExtensionAudioLevel, &id) != 0) {
    // Not registered.
    return false;
  }

  size_t block_pos = 0;
  if (!FindHeaderExtensionPosition(kRtpExtensionAudioLevel, rtp_packet,
                                   rtp_packet_length, rtp_header, &block_pos)) {
    LOG(LS_WARNING) << "Failed to update audio level.";
    return false;
  }

  // Verify first byte in block.
  const uint8_t first_block_byte = (id << 4) + 0;
  if (rtp_packet[block_pos] != first_block_byte) {
    LOG(LS_WARNING) << "Failed to update audio level.";
    return false;
  }
  rtp_packet[block_pos + 1] = (is_voiced ? 0x80 : 0x00) + (dBov & 0x7f);
  return true;
}

bool RTPSender::UpdateVideoRotation(uint8_t* rtp_packet,
                                    size_t rtp_packet_length,
                                    const RTPHeader& rtp_header,
                                    VideoRotation rotation) const {
  CriticalSectionScoped cs(send_critsect_.get());

  // Get id.
  uint8_t id = 0;
  if (rtp_header_extension_map_.GetId(kRtpExtensionVideoRotation, &id) != 0) {
    // Not registered.
    return false;
  }

  size_t block_pos = 0;
  if (!FindHeaderExtensionPosition(kRtpExtensionVideoRotation, rtp_packet,
                                   rtp_packet_length, rtp_header, &block_pos)) {
    LOG(LS_WARNING) << "Failed to update video rotation (CVO).";
    return false;
  }
  // Get length until start of header extension block.
  int extension_block_pos =
      rtp_header_extension_map_.GetLengthUntilBlockStartInBytes(
          kRtpExtensionVideoRotation);
  if (extension_block_pos < 0) {
    // The feature is not enabled.
    return false;
  }

  // Verify first byte in block.
  const uint8_t first_block_byte = (id << 4) + 0;
  if (rtp_packet[block_pos] != first_block_byte) {
    LOG(LS_WARNING) << "Failed to update CVO.";
    return false;
  }
  rtp_packet[block_pos + 1] = ConvertVideoRotationToCVOByte(rotation);
  return true;
}

void RTPSender::UpdateAbsoluteSendTime(uint8_t* rtp_packet,
                                       size_t rtp_packet_length,
                                       const RTPHeader& rtp_header,
                                       int64_t now_ms) const {
  CriticalSectionScoped cs(send_critsect_.get());

  // Get id.
  uint8_t id = 0;
  if (rtp_header_extension_map_.GetId(kRtpExtensionAbsoluteSendTime,
                                      &id) != 0) {
    // Not registered.
    return;
  }
  // Get length until start of header extension block.
  int extension_block_pos =
      rtp_header_extension_map_.GetLengthUntilBlockStartInBytes(
          kRtpExtensionAbsoluteSendTime);
  if (extension_block_pos < 0) {
    // The feature is not enabled.
    return;
  }
  size_t block_pos =
      kRtpHeaderLength + rtp_header.numCSRCs + extension_block_pos;
  if (rtp_packet_length < block_pos + kAbsoluteSendTimeLength ||
      rtp_header.headerLength < block_pos + kAbsoluteSendTimeLength) {
    LOG(LS_WARNING) << "Failed to update absolute send time, invalid length.";
    return;
  }
  // Verify that header contains extension.
  if (!((rtp_packet[kRtpHeaderLength + rtp_header.numCSRCs] == 0xBE) &&
        (rtp_packet[kRtpHeaderLength + rtp_header.numCSRCs + 1] == 0xDE))) {
    LOG(LS_WARNING)
        << "Failed to update absolute send time, hdr extension not found.";
    return;
  }
  // Verify first byte in block.
  const uint8_t first_block_byte = (id << 4) + 2;
  if (rtp_packet[block_pos] != first_block_byte) {
    LOG(LS_WARNING) << "Failed to update absolute send time.";
    return;
  }
  // Update absolute send time field (convert ms to 24-bit unsigned with 18 bit
  // fractional part).
  ByteWriter<uint32_t, 3>::WriteBigEndian(rtp_packet + block_pos + 1,
                                          ((now_ms << 18) / 1000) & 0x00ffffff);
}

void RTPSender::SetSendingStatus(bool enabled) {
  if (enabled) {
    uint32_t frequency_hz = SendPayloadFrequency();
    uint32_t RTPtime = RtpUtility::GetCurrentRTP(clock_, frequency_hz);

    // Will be ignored if it's already configured via API.
    SetStartTimestamp(RTPtime, false);
  } else {
    CriticalSectionScoped lock(send_critsect_.get());
    if (!ssrc_forced_) {
      // Generate a new SSRC.
      ssrc_db_.ReturnSSRC(ssrc_);
      ssrc_ = ssrc_db_.CreateSSRC();  // Can't be 0.
      bitrates_->set_ssrc(ssrc_);
    }
    // Don't initialize seq number if SSRC passed externally.
    if (!sequence_number_forced_ && !ssrc_forced_) {
      // Generate a new sequence number.
      sequence_number_ =
          rand() / (RAND_MAX / MAX_INIT_RTP_SEQ_NUMBER);  // NOLINT
    }
  }
}

void RTPSender::SetSendingMediaStatus(bool enabled) {
  CriticalSectionScoped cs(send_critsect_.get());
  sending_media_ = enabled;
}

bool RTPSender::SendingMedia() const {
  CriticalSectionScoped cs(send_critsect_.get());
  return sending_media_;
}

uint32_t RTPSender::Timestamp() const {
  CriticalSectionScoped cs(send_critsect_.get());
  return timestamp_;
}

void RTPSender::SetStartTimestamp(uint32_t timestamp, bool force) {
  CriticalSectionScoped cs(send_critsect_.get());
  if (force) {
    start_timestamp_forced_ = true;
    start_timestamp_ = timestamp;
  } else {
    if (!start_timestamp_forced_) {
      start_timestamp_ = timestamp;
    }
  }
}

uint32_t RTPSender::StartTimestamp() const {
  CriticalSectionScoped cs(send_critsect_.get());
  return start_timestamp_;
}

uint32_t RTPSender::GenerateNewSSRC() {
  // If configured via API, return 0.
  CriticalSectionScoped cs(send_critsect_.get());

  if (ssrc_forced_) {
    return 0;
  }
  ssrc_ = ssrc_db_.CreateSSRC();  // Can't be 0.
  bitrates_->set_ssrc(ssrc_);
  return ssrc_;
}

void RTPSender::SetSSRC(uint32_t ssrc) {
  // This is configured via the API.
  CriticalSectionScoped cs(send_critsect_.get());

  if (ssrc_ == ssrc && ssrc_forced_) {
    return;  // Since it's same ssrc, don't reset anything.
  }
  ssrc_forced_ = true;
  ssrc_db_.ReturnSSRC(ssrc_);
  ssrc_db_.RegisterSSRC(ssrc);
  ssrc_ = ssrc;
  bitrates_->set_ssrc(ssrc_);
  if (!sequence_number_forced_) {
    sequence_number_ =
        rand() / (RAND_MAX / MAX_INIT_RTP_SEQ_NUMBER);  // NOLINT
  }
}

uint32_t RTPSender::SSRC() const {
  CriticalSectionScoped cs(send_critsect_.get());
  return ssrc_;
}

void RTPSender::SetCsrcs(const std::vector<uint32_t>& csrcs) {
  assert(csrcs.size() <= kRtpCsrcSize);
  CriticalSectionScoped cs(send_critsect_.get());
  csrcs_ = csrcs;
}

void RTPSender::SetSequenceNumber(uint16_t seq) {
  CriticalSectionScoped cs(send_critsect_.get());
  sequence_number_forced_ = true;
  sequence_number_ = seq;
}

uint16_t RTPSender::SequenceNumber() const {
  CriticalSectionScoped cs(send_critsect_.get());
  return sequence_number_;
}

// Audio.
int32_t RTPSender::SendTelephoneEvent(uint8_t key,
                                      uint16_t time_ms,
                                      uint8_t level) {
  if (!audio_configured_) {
    return -1;
  }
  return audio_->SendTelephoneEvent(key, time_ms, level);
}

int32_t RTPSender::SetAudioPacketSize(uint16_t packet_size_samples) {
  if (!audio_configured_) {
    return -1;
  }
  return audio_->SetAudioPacketSize(packet_size_samples);
}

int32_t RTPSender::SetAudioLevel(uint8_t level_d_bov) {
  return audio_->SetAudioLevel(level_d_bov);
}

int32_t RTPSender::SetRED(int8_t payload_type) {
  if (!audio_configured_) {
    return -1;
  }
  return audio_->SetRED(payload_type);
}

int32_t RTPSender::RED(int8_t *payload_type) const {
  if (!audio_configured_) {
    return -1;
  }
  return audio_->RED(*payload_type);
}

// Video
VideoCodecInformation *RTPSender::CodecInformationVideo() {
  if (audio_configured_) {
    return NULL;
  }
  return video_->CodecInformationVideo();
}

RtpVideoCodecTypes RTPSender::VideoCodecType() const {
  assert(!audio_configured_ && "Sender is an audio stream!");
  return video_->VideoCodecType();
}

uint32_t RTPSender::MaxConfiguredBitrateVideo() const {
  if (audio_configured_) {
    return 0;
  }
  return video_->MaxConfiguredBitrateVideo();
}

int32_t RTPSender::SendRTPIntraRequest() {
  if (audio_configured_) {
    return -1;
  }
  return video_->SendRTPIntraRequest();
}

int32_t RTPSender::SetGenericFECStatus(bool enable,
                                       uint8_t payload_type_red,
                                       uint8_t payload_type_fec) {
  if (audio_configured_) {
    return -1;
  }
  return video_->SetGenericFECStatus(enable, payload_type_red,
                                     payload_type_fec);
}

int32_t RTPSender::GenericFECStatus(bool* enable,
                                    uint8_t* payload_type_red,
                                    uint8_t* payload_type_fec) const {
  if (audio_configured_) {
    return -1;
  }
  return video_->GenericFECStatus(
      *enable, *payload_type_red, *payload_type_fec);
}

int32_t RTPSender::SetFecParameters(
    const FecProtectionParams *delta_params,
    const FecProtectionParams *key_params) {
  if (audio_configured_) {
    return -1;
  }
  return video_->SetFecParameters(delta_params, key_params);
}

void RTPSender::BuildRtxPacket(uint8_t* buffer, size_t* length,
                               uint8_t* buffer_rtx) {
  CriticalSectionScoped cs(send_critsect_.get());
  uint8_t* data_buffer_rtx = buffer_rtx;
  // Add RTX header.
  RtpUtility::RtpHeaderParser rtp_parser(
      reinterpret_cast<const uint8_t*>(buffer), *length);

  RTPHeader rtp_header;
  rtp_parser.Parse(rtp_header);

  // Add original RTP header.
  memcpy(data_buffer_rtx, buffer, rtp_header.headerLength);

  // Replace payload type, if a specific type is set for RTX.
  if (payload_type_rtx_ != -1) {
    data_buffer_rtx[1] = static_cast<uint8_t>(payload_type_rtx_);
    if (rtp_header.markerBit)
      data_buffer_rtx[1] |= kRtpMarkerBitMask;
  }

  // Replace sequence number.
  uint8_t *ptr = data_buffer_rtx + 2;
  ByteWriter<uint16_t>::WriteBigEndian(ptr, sequence_number_rtx_++);

  // Replace SSRC.
  ptr += 6;
  ByteWriter<uint32_t>::WriteBigEndian(ptr, ssrc_rtx_);

  // Add OSN (original sequence number).
  ptr = data_buffer_rtx + rtp_header.headerLength;
  ByteWriter<uint16_t>::WriteBigEndian(ptr, rtp_header.sequenceNumber);
  ptr += 2;

  // Add original payload data.
  memcpy(ptr, buffer + rtp_header.headerLength,
         *length - rtp_header.headerLength);
  *length += 2;
}

void RTPSender::RegisterRtpStatisticsCallback(
    StreamDataCountersCallback* callback) {
  CriticalSectionScoped cs(statistics_crit_.get());
  rtp_stats_callback_ = callback;
}

StreamDataCountersCallback* RTPSender::GetRtpStatisticsCallback() const {
  CriticalSectionScoped cs(statistics_crit_.get());
  return rtp_stats_callback_;
}

uint32_t RTPSender::BitrateSent() const {
  return total_bitrate_sent_.BitrateLast();
}

void RTPSender::SetRtpState(const RtpState& rtp_state) {
  SetStartTimestamp(rtp_state.start_timestamp, true);
  CriticalSectionScoped lock(send_critsect_.get());
  sequence_number_ = rtp_state.sequence_number;
  sequence_number_forced_ = true;
  timestamp_ = rtp_state.timestamp;
  capture_time_ms_ = rtp_state.capture_time_ms;
  last_timestamp_time_ms_ = rtp_state.last_timestamp_time_ms;
  media_has_been_sent_ = rtp_state.media_has_been_sent;
}

RtpState RTPSender::GetRtpState() const {
  CriticalSectionScoped lock(send_critsect_.get());

  RtpState state;
  state.sequence_number = sequence_number_;
  state.start_timestamp = start_timestamp_;
  state.timestamp = timestamp_;
  state.capture_time_ms = capture_time_ms_;
  state.last_timestamp_time_ms = last_timestamp_time_ms_;
  state.media_has_been_sent = media_has_been_sent_;

  return state;
}

void RTPSender::SetRtxRtpState(const RtpState& rtp_state) {
  CriticalSectionScoped lock(send_critsect_.get());
  sequence_number_rtx_ = rtp_state.sequence_number;
}

RtpState RTPSender::GetRtxRtpState() const {
  CriticalSectionScoped lock(send_critsect_.get());

  RtpState state;
  state.sequence_number = sequence_number_rtx_;
  state.start_timestamp = start_timestamp_;

  return state;
}

}  // namespace webrtc
