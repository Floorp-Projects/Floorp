/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/include/flexfec_sender.h"

#include <utility>

#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/forward_error_correction.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "rtc_base/logging.h"

namespace webrtc {

namespace {

// Let first sequence number be in the first half of the interval.
constexpr uint16_t kMaxInitRtpSeqNumber = 0x7fff;

// See breakdown in flexfec_header_reader_writer.cc.
constexpr size_t kFlexfecMaxHeaderSize = 32;

// Since we will mainly use FlexFEC to protect video streams, we use a 90 kHz
// clock for the RTP timestamps. (This is according to the RFC, which states
// that it is RECOMMENDED to use the same clock frequency for FlexFEC as for
// the protected media stream.)
// The constant converts from clock millisecond timestamps to the 90 kHz
// RTP timestamp.
const int kMsToRtpTimestamp = kVideoPayloadTypeFrequency / 1000;

// How often to log the generated FEC packets to the text log.
constexpr int64_t kPacketLogIntervalMs = 10000;

RtpHeaderExtensionMap RegisterBweExtensions(
    const std::vector<RtpExtension>& rtp_header_extensions) {
  RtpHeaderExtensionMap map;
  for (const auto& extension : rtp_header_extensions) {
    if (extension.uri == TransportSequenceNumber::kUri) {
      map.Register<TransportSequenceNumber>(extension.id);
    } else if (extension.uri == AbsoluteSendTime::kUri) {
      map.Register<AbsoluteSendTime>(extension.id);
    } else if (extension.uri == TransmissionOffset::kUri) {
      map.Register<TransmissionOffset>(extension.id);
    } else {
      RTC_LOG(LS_INFO)
          << "FlexfecSender only supports RTP header extensions for "
          << "BWE, so the extension " << extension.ToString()
          << " will not be used.";
    }
  }
  return map;
}

}  // namespace

FlexfecSender::FlexfecSender(
    int payload_type,
    uint32_t ssrc,
    uint32_t protected_media_ssrc,
    const std::vector<RtpExtension>& rtp_header_extensions,
    rtc::ArrayView<const RtpExtensionSize> extension_sizes,
    const RtpState* rtp_state,
    Clock* clock)
    : clock_(clock),
      random_(clock_->TimeInMicroseconds()),
      last_generated_packet_ms_(-1),
      payload_type_(payload_type),
      // Reset RTP state if this is not the first time we are operating.
      // Otherwise, randomize the initial timestamp offset and RTP sequence
      // numbers. (This is not intended to be cryptographically strong.)
      timestamp_offset_(rtp_state ? rtp_state->start_timestamp
                                  : random_.Rand<uint32_t>()),
      ssrc_(ssrc),
      protected_media_ssrc_(protected_media_ssrc),
      seq_num_(rtp_state ? rtp_state->sequence_number
                         : random_.Rand(1, kMaxInitRtpSeqNumber)),
      ulpfec_generator_(
          ForwardErrorCorrection::CreateFlexfec(ssrc, protected_media_ssrc)),
      rtp_header_extension_map_(RegisterBweExtensions(rtp_header_extensions)),
      header_extensions_size_(
          rtp_header_extension_map_.GetTotalLengthInBytes(extension_sizes)) {
  // This object should not have been instantiated if FlexFEC is disabled.
  RTC_DCHECK_GE(payload_type, 0);
  RTC_DCHECK_LE(payload_type, 127);
}

FlexfecSender::~FlexfecSender() = default;

// We are reusing the implementation from UlpfecGenerator for SetFecParameters,
// AddRtpPacketAndGenerateFec, and FecAvailable.
void FlexfecSender::SetFecParameters(const FecProtectionParams& params) {
  ulpfec_generator_.SetFecParameters(params);
}

bool FlexfecSender::AddRtpPacketAndGenerateFec(const RtpPacketToSend& packet) {
  // TODO(brandtr): Generalize this SSRC check when we support multistream
  // protection.
  RTC_DCHECK_EQ(packet.Ssrc(), protected_media_ssrc_);
  return ulpfec_generator_.AddRtpPacketAndGenerateFec(
             packet.data(), packet.payload_size(), packet.headers_size()) == 0;
}

bool FlexfecSender::FecAvailable() const {
  return ulpfec_generator_.FecAvailable();
}

std::vector<std::unique_ptr<RtpPacketToSend>> FlexfecSender::GetFecPackets() {
  std::vector<std::unique_ptr<RtpPacketToSend>> fec_packets_to_send;
  fec_packets_to_send.reserve(ulpfec_generator_.generated_fec_packets_.size());
  for (const auto& fec_packet : ulpfec_generator_.generated_fec_packets_) {
    std::unique_ptr<RtpPacketToSend> fec_packet_to_send(
        new RtpPacketToSend(&rtp_header_extension_map_));

    // RTP header.
    fec_packet_to_send->SetMarker(false);
    fec_packet_to_send->SetPayloadType(payload_type_);
    fec_packet_to_send->SetSequenceNumber(seq_num_++);
    fec_packet_to_send->SetTimestamp(
        timestamp_offset_ +
        static_cast<uint32_t>(kMsToRtpTimestamp *
                              clock_->TimeInMilliseconds()));
    // Set "capture time" so that the TransmissionOffset header extension
    // can be set by the RTPSender.
    fec_packet_to_send->set_capture_time_ms(clock_->TimeInMilliseconds());
    fec_packet_to_send->SetSsrc(ssrc_);
    // Reserve extensions, if registered. These will be set by the RTPSender.
    fec_packet_to_send->ReserveExtension<AbsoluteSendTime>();
    fec_packet_to_send->ReserveExtension<TransmissionOffset>();
    fec_packet_to_send->ReserveExtension<TransportSequenceNumber>();

    // RTP payload.
    uint8_t* payload = fec_packet_to_send->AllocatePayload(fec_packet->length);
    memcpy(payload, fec_packet->data, fec_packet->length);

    fec_packets_to_send.push_back(std::move(fec_packet_to_send));
  }
  ulpfec_generator_.ResetState();

  int64_t now_ms = clock_->TimeInMilliseconds();
  if (!fec_packets_to_send.empty() &&
      now_ms - last_generated_packet_ms_ > kPacketLogIntervalMs) {
    RTC_LOG(LS_VERBOSE) << "Generated " << fec_packets_to_send.size()
                        << " FlexFEC packets with payload type: "
                        << payload_type_ << " and SSRC: " << ssrc_ << ".";
    last_generated_packet_ms_ = now_ms;
  }

  return fec_packets_to_send;
}

// The overhead is BWE RTP header extensions and FlexFEC header.
size_t FlexfecSender::MaxPacketOverhead() const {
  return header_extensions_size_ + kFlexfecMaxHeaderSize;
}

RtpState FlexfecSender::GetRtpState() {
  RtpState rtp_state;
  rtp_state.sequence_number = seq_num_;
  rtp_state.start_timestamp = timestamp_offset_;
  return rtp_state;
}

}  // namespace webrtc
