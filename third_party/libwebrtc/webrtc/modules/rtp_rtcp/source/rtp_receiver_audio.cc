/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_receiver_audio.h"

#include <assert.h>  // assert
#include <math.h>   // pow()
#include <string.h>  // memcpy()

#include "common_types.h"  // NOLINT(build/include)
#include "rtc_base/logging.h"
#include "rtc_base/trace_event.h"

namespace webrtc {
RTPReceiverStrategy* RTPReceiverStrategy::CreateAudioStrategy(
    RtpData* data_callback) {
  return new RTPReceiverAudio(data_callback);
}

RTPReceiverAudio::RTPReceiverAudio(RtpData* data_callback)
    : RTPReceiverStrategy(data_callback),
      TelephoneEventHandler(),
      telephone_event_forward_to_decoder_(false),
      telephone_event_payload_type_(-1),
      cng_nb_payload_type_(-1),
      cng_wb_payload_type_(-1),
      cng_swb_payload_type_(-1),
      cng_fb_payload_type_(-1),
      num_energy_(0),
      current_remote_energy_() {
  memset(current_remote_energy_, 0, sizeof(current_remote_energy_));
}

// Outband TelephoneEvent(DTMF) detection
void RTPReceiverAudio::SetTelephoneEventForwardToDecoder(
    bool forward_to_decoder) {
  rtc::CritScope lock(&crit_sect_);
  telephone_event_forward_to_decoder_ = forward_to_decoder;
}

// Is forwarding of outband telephone events turned on/off?
bool RTPReceiverAudio::TelephoneEventForwardToDecoder() const {
  rtc::CritScope lock(&crit_sect_);
  return telephone_event_forward_to_decoder_;
}

bool RTPReceiverAudio::TelephoneEventPayloadType(
    int8_t payload_type) const {
  rtc::CritScope lock(&crit_sect_);
  return telephone_event_payload_type_ == payload_type;
}

bool RTPReceiverAudio::CNGPayloadType(int8_t payload_type) {
  rtc::CritScope lock(&crit_sect_);
  return payload_type == cng_nb_payload_type_ ||
         payload_type == cng_wb_payload_type_ ||
         payload_type == cng_swb_payload_type_ ||
         payload_type == cng_fb_payload_type_;
}

bool RTPReceiverAudio::ShouldReportCsrcChanges(uint8_t payload_type) const {
  // Don't do this for DTMF packets, otherwise it's fine.
  return !TelephoneEventPayloadType(payload_type);
}

// -   Sample based or frame based codecs based on RFC 3551
// -
// -   NOTE! There is one error in the RFC, stating G.722 uses 8 bits/samples.
// -   The correct rate is 4 bits/sample.
// -
// -   name of                              sampling              default
// -   encoding  sample/frame  bits/sample      rate  ms/frame  ms/packet
// -
// -   Sample based audio codecs
// -   DVI4      sample        4                var.                   20
// -   G722      sample        4              16,000                   20
// -   G726-40   sample        5               8,000                   20
// -   G726-32   sample        4               8,000                   20
// -   G726-24   sample        3               8,000                   20
// -   G726-16   sample        2               8,000                   20
// -   L8        sample        8                var.                   20
// -   L16       sample        16               var.                   20
// -   PCMA      sample        8                var.                   20
// -   PCMU      sample        8                var.                   20
// -
// -   Frame based audio codecs
// -   G723      frame         N/A             8,000        30         30
// -   G728      frame         N/A             8,000       2.5         20
// -   G729      frame         N/A             8,000        10         20
// -   G729D     frame         N/A             8,000        10         20
// -   G729E     frame         N/A             8,000        10         20
// -   GSM       frame         N/A             8,000        20         20
// -   GSM-EFR   frame         N/A             8,000        20         20
// -   LPC       frame         N/A             8,000        20         20
// -   MPA       frame         N/A              var.      var.
// -
// -   G7221     frame         N/A
int32_t RTPReceiverAudio::OnNewPayloadTypeCreated(
    int payload_type,
    const SdpAudioFormat& audio_format) {
  rtc::CritScope lock(&crit_sect_);

  if (RtpUtility::StringCompare(audio_format.name.c_str(), "telephone-event",
                                15)) {
    telephone_event_payload_type_ = payload_type;
  }
  if (RtpUtility::StringCompare(audio_format.name.c_str(), "cn", 2)) {
    // We support comfort noise at four different frequencies.
    if (audio_format.clockrate_hz == 8000) {
      cng_nb_payload_type_ = payload_type;
    } else if (audio_format.clockrate_hz == 16000) {
      cng_wb_payload_type_ = payload_type;
    } else if (audio_format.clockrate_hz == 32000) {
      cng_swb_payload_type_ = payload_type;
    } else if (audio_format.clockrate_hz == 48000) {
      cng_fb_payload_type_ = payload_type;
    } else {
      assert(false);
      return -1;
    }
  }
  return 0;
}

int32_t RTPReceiverAudio::ParseRtpPacket(WebRtcRTPHeader* rtp_header,
                                         const PayloadUnion& specific_payload,
                                         bool is_red,
                                         const uint8_t* payload,
                                         size_t payload_length,
                                         int64_t timestamp_ms) {
  TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"), "Audio::ParseRtp",
               "seqnum", rtp_header->header.sequenceNumber, "timestamp",
               rtp_header->header.timestamp);
  rtp_header->type.Audio.numEnergy = rtp_header->header.numCSRCs;
  num_energy_ = rtp_header->type.Audio.numEnergy;
  if (rtp_header->type.Audio.numEnergy > 0 &&
      rtp_header->type.Audio.numEnergy <= kRtpCsrcSize) {
    memcpy(current_remote_energy_,
           rtp_header->type.Audio.arrOfEnergy,
           rtp_header->type.Audio.numEnergy);
  }

  if (first_packet_received_()) {
    RTC_LOG(LS_INFO) << "Received first audio RTP packet";
  }

  return ParseAudioCodecSpecific(rtp_header, payload, payload_length,
                                 specific_payload.audio_payload(), is_red);
}

RTPAliveType RTPReceiverAudio::ProcessDeadOrAlive(
    uint16_t last_payload_length) const {

  // Our CNG is 9 bytes; if it's a likely CNG the receiver needs to check
  // kRtpNoRtp against NetEq speech_type kOutputPLCtoCNG.
  if (last_payload_length < 10) {  // our CNG is 9 bytes
    return kRtpNoRtp;
  } else {
    return kRtpDead;
  }
}

void RTPReceiverAudio::CheckPayloadChanged(int8_t payload_type,
                                           PayloadUnion* /* specific_payload */,
                                           bool* should_discard_changes) {
  *should_discard_changes =
      TelephoneEventPayloadType(payload_type) || CNGPayloadType(payload_type);
}

int RTPReceiverAudio::Energy(uint8_t array_of_energy[kRtpCsrcSize]) const {
  rtc::CritScope cs(&crit_sect_);

  assert(num_energy_ <= kRtpCsrcSize);

  if (num_energy_ > 0) {
    memcpy(array_of_energy, current_remote_energy_,
           sizeof(uint8_t) * num_energy_);
  }
  return num_energy_;
}

int32_t RTPReceiverAudio::InvokeOnInitializeDecoder(
    RtpFeedback* callback,
    int8_t payload_type,
    const char payload_name[RTP_PAYLOAD_NAME_SIZE],
    const PayloadUnion& specific_payload) const {
  const auto& ap = specific_payload.audio_payload();
  if (callback->OnInitializeDecoder(payload_type, ap.format, ap.rate) == -1) {
    RTC_LOG(LS_ERROR) << "Failed to create decoder for payload type: "
                      << payload_name << "/" << static_cast<int>(payload_type);
    return -1;
  }
  return 0;
}

// We are not allowed to have any critsects when calling data_callback.
int32_t RTPReceiverAudio::ParseAudioCodecSpecific(
    WebRtcRTPHeader* rtp_header,
    const uint8_t* payload_data,
    size_t payload_length,
    const AudioPayload& audio_specific,
    bool is_red) {
  RTC_DCHECK_GE(payload_length, rtp_header->header.paddingLength);
  const size_t payload_data_length =
      payload_length - rtp_header->header.paddingLength;
  if (payload_data_length == 0) {
    rtp_header->type.Audio.isCNG = false;
    rtp_header->frameType = kEmptyFrame;
    return data_callback_->OnReceivedPayloadData(nullptr, 0, rtp_header);
  }

  bool telephone_event_packet =
      TelephoneEventPayloadType(rtp_header->header.payloadType);
  if (telephone_event_packet) {
    rtc::CritScope lock(&crit_sect_);

    // RFC 4733 2.3
    // 0                   1                   2                   3
    // 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    // |     event     |E|R| volume    |          duration             |
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //
    if (payload_data_length % 4 != 0) {
      return -1;
    }
    size_t number_of_events = payload_data_length / 4;

    // sanity
    if (number_of_events >= MAX_NUMBER_OF_PARALLEL_TELEPHONE_EVENTS) {
      number_of_events = MAX_NUMBER_OF_PARALLEL_TELEPHONE_EVENTS;
    }
    for (size_t n = 0; n < number_of_events; ++n) {
      RTC_DCHECK_GE(payload_data_length, (4 * n) + 2);
      bool end = (payload_data[(4 * n) + 1] & 0x80) ? true : false;

      std::set<uint8_t>::iterator event =
          telephone_event_reported_.find(payload_data[4 * n]);

      if (event != telephone_event_reported_.end()) {
        // we have already seen this event
        if (end) {
          telephone_event_reported_.erase(payload_data[4 * n]);
        }
      } else {
        if (end) {
          // don't add if it's a end of a tone
        } else {
          telephone_event_reported_.insert(payload_data[4 * n]);
        }
      }
    }

    // RFC 4733 2.5.1.3 & 2.5.2.3 Long-Duration Events
    // should not be a problem since we don't care about the duration

    // RFC 4733 See 2.5.1.5. & 2.5.2.4.  Multiple Events in a Packet
  }

  {
    rtc::CritScope lock(&crit_sect_);

    // Check if this is a CNG packet, receiver might want to know
    if (CNGPayloadType(rtp_header->header.payloadType)) {
      rtp_header->type.Audio.isCNG = true;
      rtp_header->frameType = kAudioFrameCN;
    } else {
      rtp_header->frameType = kAudioFrameSpeech;
      rtp_header->type.Audio.isCNG = false;
    }

    // check if it's a DTMF event, hence something we can playout
    if (telephone_event_packet) {
      if (!telephone_event_forward_to_decoder_) {
        // don't forward event to decoder
        return 0;
      }
      std::set<uint8_t>::iterator first =
          telephone_event_reported_.begin();
      if (first != telephone_event_reported_.end() && *first > 15) {
        // don't forward non DTMF events
        return 0;
      }
    }
  }
  // TODO(holmer): Break this out to have RED parsing handled generically.
  RTC_DCHECK_GT(payload_data_length, 0);
  if (is_red && !(payload_data[0] & 0x80)) {
    // we recive only one frame packed in a RED packet remove the RED wrapper
    rtp_header->header.payloadType = payload_data[0];

    // only one frame in the RED strip the one byte to help NetEq
    return data_callback_->OnReceivedPayloadData(
        payload_data + 1, payload_data_length - 1, rtp_header);
  }

  rtp_header->type.Audio.channel = audio_specific.format.num_channels;
  return data_callback_->OnReceivedPayloadData(payload_data,
                                               payload_data_length, rtp_header);
}
}  // namespace webrtc
