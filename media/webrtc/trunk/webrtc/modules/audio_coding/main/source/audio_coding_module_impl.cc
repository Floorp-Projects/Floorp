/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/main/source/audio_coding_module_impl.h"

#include <assert.h>
#include <stdlib.h>

#include <algorithm>  // For std::max.

#include "webrtc/engine_configurations.h"
#include "webrtc/modules/audio_coding/main/source/acm_codec_database.h"
#include "webrtc/modules/audio_coding/main/source/acm_common_defs.h"
#include "webrtc/modules/audio_coding/main/source/acm_dtmf_detection.h"
#include "webrtc/modules/audio_coding/main/source/acm_generic_codec.h"
#include "webrtc/modules/audio_coding/main/source/acm_resampler.h"
#include "webrtc/modules/audio_coding/main/source/nack.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/system_wrappers/interface/rw_lock_wrapper.h"
#include "webrtc/system_wrappers/interface/tick_util.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/system_wrappers/interface/trace_event.h"

namespace webrtc {

enum {
  kACMToneEnd = 999
};

// Maximum number of bytes in one packet (PCM16B, 20 ms packets, stereo).
enum {
  kMaxPacketSize = 2560
};

// Maximum number of payloads that can be packed in one RED payload. For
// regular FEC, we only pack two payloads. In case of dual-streaming, in worst
// case we might pack 3 payloads in one RED payload.
enum {
  kNumFecFragmentationVectors = 2,
  kMaxNumFragmentationVectors = 3
};

static const uint32_t kMaskTimestamp = 0x03ffffff;
static const int kDefaultTimestampDiff = 960;  // 20 ms @ 48 kHz.

// If packet N is arrived all packets prior to N - |kNackThresholdPackets| which
// are not received are considered as lost, and appear in NACK list.
static const int kNackThresholdPackets = 2;

namespace {

bool IsCodecRED(const CodecInst* codec) {
  return (STR_CASE_CMP(codec->plname, "RED") == 0);
}

bool IsCodecRED(int index) {
  return (IsCodecRED(&ACMCodecDB::database_[index]));
}

bool IsCodecCN(const CodecInst* codec) {
  return (STR_CASE_CMP(codec->plname, "CN") == 0);
}

bool IsCodecCN(int index) {
  return (IsCodecCN(&ACMCodecDB::database_[index]));
}

// Stereo-to-mono can be used as in-place.
int DownMix(const AudioFrame& frame, int length_out_buff, int16_t* out_buff) {
  if (length_out_buff < frame.samples_per_channel_) {
    return -1;
  }
  for (int n = 0; n < frame.samples_per_channel_; ++n)
    out_buff[n] = (frame.data_[2 * n] + frame.data_[2 * n + 1]) >> 1;
  return 0;
}

// Mono-to-stereo can be used as in-place.
int UpMix(const AudioFrame& frame, int length_out_buff, int16_t* out_buff) {
  if (length_out_buff < frame.samples_per_channel_) {
    return -1;
  }
  for (int n = frame.samples_per_channel_ - 1; n >= 0; --n) {
    out_buff[2 * n + 1] = frame.data_[n];
    out_buff[2 * n] = frame.data_[n];
  }
  return 0;
}

// Return 1 if timestamp t1 is less than timestamp t2, while compensating for
// wrap-around.
int TimestampLessThan(uint32_t t1, uint32_t t2) {
  uint32_t kHalfFullRange = static_cast<uint32_t>(0xFFFFFFFF) / 2;
  if (t1 == t2) {
    return 0;
  } else if (t1 < t2) {
    if (t2 - t1 < kHalfFullRange)
      return 1;
    return 0;
  } else {
    if (t1 - t2 < kHalfFullRange)
      return 0;
    return 1;
  }
}

}  // namespace

AudioCodingModuleImpl::AudioCodingModuleImpl(const int32_t id, Clock* clock)
    : packetization_callback_(NULL),
      id_(id),
      last_timestamp_(0xD87F3F9F),
      last_in_timestamp_(0xD87F3F9F),
      send_codec_inst_(),
      cng_nb_pltype_(255),
      cng_wb_pltype_(255),
      cng_swb_pltype_(255),
      cng_fb_pltype_(255),
      red_pltype_(255),
      vad_enabled_(false),
      dtx_enabled_(false),
      vad_mode_(VADNormal),
      stereo_receive_registered_(false),
      stereo_send_(false),
      prev_received_channel_(0),
      expected_channels_(1),
      current_send_codec_idx_(-1),
      current_receive_codec_idx_(-1),
      send_codec_registered_(false),
      acm_crit_sect_(CriticalSectionWrapper::CreateCriticalSection()),
      vad_callback_(NULL),
      last_recv_audio_codec_pltype_(255),
      is_first_red_(true),
      fec_enabled_(false),
      last_fec_timestamp_(0),
      receive_red_pltype_(255),
      previous_pltype_(255),
      dummy_rtp_header_(NULL),
      recv_pl_frame_size_smpls_(0),
      receiver_initialized_(false),
      dtmf_detector_(NULL),
      dtmf_callback_(NULL),
      last_detected_tone_(kACMToneEnd),
      callback_crit_sect_(CriticalSectionWrapper::CreateCriticalSection()),
      secondary_send_codec_inst_(),
      secondary_encoder_(NULL),
      initial_delay_ms_(0),
      num_packets_accumulated_(0),
      num_bytes_accumulated_(0),
      accumulated_audio_ms_(0),
      first_payload_received_(false),
      last_incoming_send_timestamp_(0),
      track_neteq_buffer_(false),
      playout_ts_(0),
      av_sync_(false),
      last_timestamp_diff_(kDefaultTimestampDiff),
      last_sequence_number_(0),
      last_ssrc_(0),
      last_packet_was_sync_(false),
      clock_(clock),
      nack_(),
      nack_enabled_(false) {

  // Nullify send codec memory, set payload type and set codec name to
  // invalid values.
  const char no_name[] = "noCodecRegistered";
  strncpy(send_codec_inst_.plname, no_name, RTP_PAYLOAD_NAME_SIZE - 1);
  send_codec_inst_.pltype = -1;

  strncpy(secondary_send_codec_inst_.plname, no_name,
          RTP_PAYLOAD_NAME_SIZE - 1);
  secondary_send_codec_inst_.pltype = -1;

  for (int i = 0; i < ACMCodecDB::kMaxNumCodecs; i++) {
    codecs_[i] = NULL;
    registered_pltypes_[i] = -1;
    stereo_receive_[i] = false;
    slave_codecs_[i] = NULL;
    mirror_codec_idx_[i] = -1;
  }

  neteq_.set_id(id_);

  // Allocate memory for RED.
  red_buffer_ = new uint8_t[MAX_PAYLOAD_SIZE_BYTE];

  // TODO(turajs): This might not be exactly how this class is supposed to work.
  // The external usage might be that |fragmentationVectorSize| has to match
  // the allocated space for the member-arrays, while here, we allocate
  // according to the maximum number of fragmentations and change
  // |fragmentationVectorSize| on-the-fly based on actual number of
  // fragmentations. However, due to copying to local variable before calling
  // SendData, the RTP module receives a "valid" fragmentation, where allocated
  // space matches |fragmentationVectorSize|, therefore, this should not cause
  // any problem. A better approach is not using RTPFragmentationHeader as
  // member variable, instead, use an ACM-specific structure to hold RED-related
  // data. See module_common_type.h for the definition of
  // RTPFragmentationHeader.
  fragmentation_.VerifyAndAllocateFragmentationHeader(
      kMaxNumFragmentationVectors);

  // Register the default payload type for RED and for CNG at sampling rates of
  // 8, 16, 32 and 48 kHz.
  for (int i = (ACMCodecDB::kNumCodecs - 1); i >= 0; i--) {
    if (IsCodecRED(i)) {
      red_pltype_ = static_cast<uint8_t>(ACMCodecDB::database_[i].pltype);
    } else if (IsCodecCN(i)) {
      if (ACMCodecDB::database_[i].plfreq == 8000) {
        cng_nb_pltype_ = static_cast<uint8_t>(ACMCodecDB::database_[i].pltype);
      } else if (ACMCodecDB::database_[i].plfreq == 16000) {
        cng_wb_pltype_ = static_cast<uint8_t>(ACMCodecDB::database_[i].pltype);
      } else if (ACMCodecDB::database_[i].plfreq == 32000) {
        cng_swb_pltype_ = static_cast<uint8_t>(ACMCodecDB::database_[i].pltype);
      } else if (ACMCodecDB::database_[i].plfreq == 48000) {
        cng_fb_pltype_ = static_cast<uint8_t>(ACMCodecDB::database_[i].pltype);
      }
    }
  }

  if (InitializeReceiverSafe() < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "Cannot initialize receiver");
  }
  WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceAudioCoding, id, "Created");
}

AudioCodingModuleImpl::~AudioCodingModuleImpl() {
  {
    CriticalSectionScoped lock(acm_crit_sect_);
    current_send_codec_idx_ = -1;

    for (int i = 0; i < ACMCodecDB::kMaxNumCodecs; i++) {
      if (codecs_[i] != NULL) {
        // True stereo codecs share the same memory for master and
        // slave, so slave codec need to be nullified here, since the
        // memory will be deleted.
        if (slave_codecs_[i] == codecs_[i]) {
          slave_codecs_[i] = NULL;
        }

        // Mirror index holds the address of the codec memory.
        assert(mirror_codec_idx_[i] > -1);
        if (codecs_[mirror_codec_idx_[i]] != NULL) {
          delete codecs_[mirror_codec_idx_[i]];
          codecs_[mirror_codec_idx_[i]] = NULL;
        }

        codecs_[i] = NULL;
      }

      if (slave_codecs_[i] != NULL) {
        // Delete memory for stereo usage of mono codecs.
        assert(mirror_codec_idx_[i] > -1);
        if (slave_codecs_[mirror_codec_idx_[i]] != NULL) {
          delete slave_codecs_[mirror_codec_idx_[i]];
          slave_codecs_[mirror_codec_idx_[i]] = NULL;
        }
        slave_codecs_[i] = NULL;
      }
    }

    if (dtmf_detector_ != NULL) {
      delete dtmf_detector_;
      dtmf_detector_ = NULL;
    }
    if (dummy_rtp_header_ != NULL) {
      delete dummy_rtp_header_;
      dummy_rtp_header_ = NULL;
    }
    if (red_buffer_ != NULL) {
      delete[] red_buffer_;
      red_buffer_ = NULL;
    }
  }

  delete callback_crit_sect_;
  callback_crit_sect_ = NULL;

  delete acm_crit_sect_;
  acm_crit_sect_ = NULL;
  WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceAudioCoding, id_,
               "Destroyed");
}

int32_t AudioCodingModuleImpl::ChangeUniqueId(const int32_t id) {
  {
    CriticalSectionScoped lock(acm_crit_sect_);
    id_ = id;

    for (int i = 0; i < ACMCodecDB::kMaxNumCodecs; i++) {
      if (codecs_[i] != NULL) {
        codecs_[i]->SetUniqueID(id);
      }
    }
  }

  neteq_.set_id(id_);
  return 0;
}

// Returns the number of milliseconds until the module want a
// worker thread to call Process.
int32_t AudioCodingModuleImpl::TimeUntilNextProcess() {
  CriticalSectionScoped lock(acm_crit_sect_);

  if (!HaveValidEncoder("TimeUntilNextProcess")) {
    return -1;
  }
  return codecs_[current_send_codec_idx_]->SamplesLeftToEncode() /
      (send_codec_inst_.plfreq / 1000);
}

int32_t AudioCodingModuleImpl::Process() {
  bool dual_stream;
  {
    CriticalSectionScoped lock(acm_crit_sect_);
    dual_stream = (secondary_encoder_.get() != NULL);
  }
  if (dual_stream) {
    return ProcessDualStream();
  }
  return ProcessSingleStream();
}

int AudioCodingModuleImpl::EncodeFragmentation(int fragmentation_index,
                                               int payload_type,
                                               uint32_t current_timestamp,
                                               ACMGenericCodec* encoder,
                                               uint8_t* stream) {
  int16_t len_bytes = MAX_PAYLOAD_SIZE_BYTE;
  uint32_t rtp_timestamp;
  WebRtcACMEncodingType encoding_type;
  if (encoder->Encode(stream, &len_bytes, &rtp_timestamp, &encoding_type) < 0) {
    return -1;
  }
  assert(encoding_type == kActiveNormalEncoded);
  assert(len_bytes > 0);

  fragmentation_.fragmentationLength[fragmentation_index] = len_bytes;
  fragmentation_.fragmentationPlType[fragmentation_index] = payload_type;
  fragmentation_.fragmentationTimeDiff[fragmentation_index] =
      static_cast<uint16_t>(current_timestamp - rtp_timestamp);
  fragmentation_.fragmentationVectorSize++;
  return len_bytes;
}

// Primary payloads are sent immediately, whereas a single secondary payload is
// buffered to be combined with "the next payload."
// Normally "the next payload" would be a primary payload. In case two
// consecutive secondary payloads are generated with no primary payload in
// between, then two secondary payloads are packed in one RED.
int AudioCodingModuleImpl::ProcessDualStream() {
  uint8_t stream[kMaxNumFragmentationVectors * MAX_PAYLOAD_SIZE_BYTE];
  uint32_t current_timestamp;
  int16_t length_bytes = 0;
  RTPFragmentationHeader my_fragmentation;

  uint8_t my_red_payload_type;

  {
    CriticalSectionScoped lock(acm_crit_sect_);
    // Check if there is an encoder before.
    if (!HaveValidEncoder("ProcessDualStream") ||
        secondary_encoder_.get() == NULL) {
      return -1;
    }
    ACMGenericCodec* primary_encoder = codecs_[current_send_codec_idx_];
    // If primary encoder has a full frame of audio to generate payload.
    bool primary_ready_to_encode = primary_encoder->HasFrameToEncode();
    // If the secondary encoder has a frame of audio to generate a payload.
    bool secondary_ready_to_encode = secondary_encoder_->HasFrameToEncode();

    if (!primary_ready_to_encode && !secondary_ready_to_encode) {
      // Nothing to send.
      return 0;
    }
    int len_bytes_previous_secondary = static_cast<int>(
        fragmentation_.fragmentationLength[2]);
    assert(len_bytes_previous_secondary <= MAX_PAYLOAD_SIZE_BYTE);
    bool has_previous_payload = len_bytes_previous_secondary > 0;

    uint32_t primary_timestamp = primary_encoder->EarliestTimestamp();
    uint32_t secondary_timestamp = secondary_encoder_->EarliestTimestamp();

    if (!has_previous_payload && !primary_ready_to_encode &&
        secondary_ready_to_encode) {
      // Secondary payload will be the ONLY bit-stream. Encode by secondary
      // encoder, store the payload, and return. No packet is sent.
      int16_t len_bytes = MAX_PAYLOAD_SIZE_BYTE;
      WebRtcACMEncodingType encoding_type;
      if (secondary_encoder_->Encode(red_buffer_, &len_bytes,
                                     &last_fec_timestamp_,
                                     &encoding_type) < 0) {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                     "ProcessDual(): Encoding of secondary encoder Failed");
        return -1;
      }
      assert(len_bytes > 0);
      assert(encoding_type == kActiveNormalEncoded);
      assert(len_bytes <= MAX_PAYLOAD_SIZE_BYTE);
      fragmentation_.fragmentationLength[2] = len_bytes;
      return 0;
    }

    // Initialize with invalid but different values, so later can have sanity
    // check if they are different.
    int index_primary = -1;
    int index_secondary = -2;
    int index_previous_secondary = -3;

    if (primary_ready_to_encode) {
      index_primary = secondary_ready_to_encode ?
          TimestampLessThan(primary_timestamp, secondary_timestamp) : 0;
      index_primary += has_previous_payload ?
          TimestampLessThan(primary_timestamp, last_fec_timestamp_) : 0;
    }

    if (secondary_ready_to_encode) {
      // Timestamp of secondary payload can only be less than primary payload,
      // but is always larger than the timestamp of previous secondary payload.
      index_secondary = primary_ready_to_encode ?
          (1 - TimestampLessThan(primary_timestamp, secondary_timestamp)) : 0;
    }

    if (has_previous_payload) {
      index_previous_secondary = primary_ready_to_encode ?
          (1 - TimestampLessThan(primary_timestamp, last_fec_timestamp_)) : 0;
      // If secondary is ready it always have a timestamp larger than previous
      // secondary. So the index is either 0 or 1.
      index_previous_secondary += secondary_ready_to_encode ? 1 : 0;
    }

    // Indices must not be equal.
    assert(index_primary != index_secondary);
    assert(index_primary != index_previous_secondary);
    assert(index_secondary != index_previous_secondary);

    // One of the payloads has to be at position zero.
    assert(index_primary == 0 || index_secondary == 0 ||
           index_previous_secondary == 0);

    // Timestamp of the RED payload.
    if (index_primary == 0) {
      current_timestamp = primary_timestamp;
    } else if (index_secondary == 0) {
      current_timestamp = secondary_timestamp;
    } else {
      current_timestamp = last_fec_timestamp_;
    }

    fragmentation_.fragmentationVectorSize = 0;
    if (has_previous_payload) {
      assert(index_previous_secondary >= 0 &&
             index_previous_secondary < kMaxNumFragmentationVectors);
      assert(len_bytes_previous_secondary <= MAX_PAYLOAD_SIZE_BYTE);
      memcpy(&stream[index_previous_secondary * MAX_PAYLOAD_SIZE_BYTE],
             red_buffer_, sizeof(stream[0]) * len_bytes_previous_secondary);
      fragmentation_.fragmentationLength[index_previous_secondary] =
          len_bytes_previous_secondary;
      fragmentation_.fragmentationPlType[index_previous_secondary] =
          secondary_send_codec_inst_.pltype;
      fragmentation_.fragmentationTimeDiff[index_previous_secondary] =
          static_cast<uint16_t>(current_timestamp - last_fec_timestamp_);
      fragmentation_.fragmentationVectorSize++;
    }

    if (primary_ready_to_encode) {
      assert(index_primary >= 0 && index_primary < kMaxNumFragmentationVectors);
      int i = index_primary * MAX_PAYLOAD_SIZE_BYTE;
      if (EncodeFragmentation(index_primary, send_codec_inst_.pltype,
                              current_timestamp, primary_encoder,
                              &stream[i]) < 0) {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                     "ProcessDualStream(): Encoding of primary encoder Failed");
        return -1;
      }
    }

    if (secondary_ready_to_encode) {
      assert(index_secondary >= 0 &&
             index_secondary < kMaxNumFragmentationVectors - 1);
      int i = index_secondary * MAX_PAYLOAD_SIZE_BYTE;
      if (EncodeFragmentation(index_secondary,
                              secondary_send_codec_inst_.pltype,
                              current_timestamp, secondary_encoder_.get(),
                              &stream[i]) < 0) {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                     "ProcessDualStream(): Encoding of secondary encoder "
                     "Failed");
        return -1;
      }
    }
    // Copy to local variable, as it will be used outside the ACM lock.
    my_fragmentation.CopyFrom(fragmentation_);
    my_red_payload_type = red_pltype_;
    length_bytes = 0;
    for (int n = 0; n < fragmentation_.fragmentationVectorSize; n++) {
      length_bytes += fragmentation_.fragmentationLength[n];
    }
  }

  {
    CriticalSectionScoped lock(callback_crit_sect_);
    if (packetization_callback_ != NULL) {
      // Callback with payload data, including redundant data (FEC/RED).
      if (packetization_callback_->SendData(kAudioFrameSpeech,
                                            my_red_payload_type,
                                            current_timestamp, stream,
                                            length_bytes,
                                            &my_fragmentation) < 0) {
        return -1;
      }
    }
  }

  {
    CriticalSectionScoped lock(acm_crit_sect_);
    // Now that data is sent, clean up fragmentation.
    ResetFragmentation(0);
  }
  return 0;
}

// Process any pending tasks such as timeouts.
int AudioCodingModuleImpl::ProcessSingleStream() {
  // Make room for 1 RED payload.
  uint8_t stream[2 * MAX_PAYLOAD_SIZE_BYTE];
  int16_t length_bytes = 2 * MAX_PAYLOAD_SIZE_BYTE;
  int16_t red_length_bytes = length_bytes;
  uint32_t rtp_timestamp;
  int16_t status;
  WebRtcACMEncodingType encoding_type;
  FrameType frame_type = kAudioFrameSpeech;
  uint8_t current_payload_type = 0;
  bool has_data_to_send = false;
  bool fec_active = false;
  RTPFragmentationHeader my_fragmentation;

  // Keep the scope of the ACM critical section limited.
  {
    CriticalSectionScoped lock(acm_crit_sect_);
    // Check if there is an encoder before.
    if (!HaveValidEncoder("ProcessSingleStream")) {
      return -1;
    }
    status = codecs_[current_send_codec_idx_]->Encode(stream, &length_bytes,
                                                      &rtp_timestamp,
                                                      &encoding_type);
    if (status < 0) {
      // Encode failed.
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                   "ProcessSingleStream(): Encoding Failed");
      length_bytes = 0;
      return -1;
    } else if (status == 0) {
      // Not enough data.
      return 0;
    } else {
      switch (encoding_type) {
        case kNoEncoding: {
          current_payload_type = previous_pltype_;
          frame_type = kFrameEmpty;
          length_bytes = 0;
          break;
        }
        case kActiveNormalEncoded:
        case kPassiveNormalEncoded: {
          current_payload_type = (uint8_t) send_codec_inst_.pltype;
          frame_type = kAudioFrameSpeech;
          break;
        }
        case kPassiveDTXNB: {
          current_payload_type = cng_nb_pltype_;
          frame_type = kAudioFrameCN;
          is_first_red_ = true;
          break;
        }
        case kPassiveDTXWB: {
          current_payload_type = cng_wb_pltype_;
          frame_type = kAudioFrameCN;
          is_first_red_ = true;
          break;
        }
        case kPassiveDTXSWB: {
          current_payload_type = cng_swb_pltype_;
          frame_type = kAudioFrameCN;
          is_first_red_ = true;
          break;
        }
        case kPassiveDTXFB: {
          current_payload_type = cng_fb_pltype_;
          frame_type = kAudioFrameCN;
          is_first_red_ = true;
          break;
        }
      }
      has_data_to_send = true;
      previous_pltype_ = current_payload_type;

      // Redundancy encode is done here. The two bitstreams packetized into
      // one RTP packet and the fragmentation points are set.
      // Only apply RED on speech data.
      if ((fec_enabled_) &&
          ((encoding_type == kActiveNormalEncoded) ||
              (encoding_type == kPassiveNormalEncoded))) {
        // FEC is enabled within this scope.
        //
        // Note that, a special solution exists for iSAC since it is the only
        // codec for which GetRedPayload has a non-empty implementation.
        //
        // Summary of the FEC scheme below (use iSAC as example):
        //
        //  1st (is_first_red_ is true) encoded iSAC frame (primary #1) =>
        //      - call GetRedPayload() and store redundancy for packet #1 in
        //        second fragment of RED buffer (old data)
        //      - drop the primary iSAC frame
        //      - don't call SendData
        //  2nd (is_first_red_ is false) encoded iSAC frame (primary #2) =>
        //      - store primary #2 in 1st fragment of RED buffer and send the
        //        combined packet
        //      - the transmitted packet contains primary #2 (new) and
        //        reduncancy for packet #1 (old)
        //      - call GetRed_Payload() and store redundancy for packet #2 in
        //        second fragment of RED buffer
        //
        //  ...
        //
        //  Nth encoded iSAC frame (primary #N) =>
        //      - store primary #N in 1st fragment of RED buffer and send the
        //        combined packet
        //      - the transmitted packet contains primary #N (new) and
        //        reduncancy for packet #(N-1) (old)
        //      - call GetRedPayload() and store redundancy for packet #N in
        //        second fragment of RED buffer
        //
        //  For all other codecs, GetRedPayload does nothing and returns -1 =>
        //  redundant data is only a copy.
        //
        //  First combined packet contains : #2 (new) and #1 (old)
        //  Second combined packet contains: #3 (new) and #2 (old)
        //  Third combined packet contains : #4 (new) and #3 (old)
        //
        //  Hence, even if every second packet is dropped, perfect
        //  reconstruction is possible.
        fec_active = true;

        has_data_to_send = false;
        // Skip the following part for the first packet in a RED session.
        if (!is_first_red_) {
          // Rearrange stream such that FEC packets are included.
          // Replace stream now that we have stored current stream.
          memcpy(stream + fragmentation_.fragmentationOffset[1], red_buffer_,
                 fragmentation_.fragmentationLength[1]);
          // Update the fragmentation time difference vector, in number of
          // timestamps.
          uint16_t time_since_last = uint16_t(
              rtp_timestamp - last_fec_timestamp_);

          // Update fragmentation vectors.
          fragmentation_.fragmentationPlType[1] =
              fragmentation_.fragmentationPlType[0];
          fragmentation_.fragmentationTimeDiff[1] = time_since_last;
          has_data_to_send = true;
        }

        // Insert new packet length.
        fragmentation_.fragmentationLength[0] = length_bytes;

        // Insert new packet payload type.
        fragmentation_.fragmentationPlType[0] = current_payload_type;
        last_fec_timestamp_ = rtp_timestamp;

        // Can be modified by the GetRedPayload() call if iSAC is utilized.
        red_length_bytes = length_bytes;

        // A fragmentation header is provided => packetization according to
        // RFC 2198 (RTP Payload for Redundant Audio Data) will be used.
        // First fragment is the current data (new).
        // Second fragment is the previous data (old).
        length_bytes = static_cast<int16_t>(
            fragmentation_.fragmentationLength[0] +
            fragmentation_.fragmentationLength[1]);

        // Get, and store, redundant data from the encoder based on the recently
        // encoded frame.
        // NOTE - only iSAC contains an implementation; all other codecs does
        // nothing and returns -1.
        if (codecs_[current_send_codec_idx_]->GetRedPayload(
            red_buffer_,
            &red_length_bytes) == -1) {
          // The codec was not iSAC => use current encoder output as redundant
          // data instead (trivial FEC scheme).
          memcpy(red_buffer_, stream, red_length_bytes);
        }

        is_first_red_ = false;
        // Update payload type with RED payload type.
        current_payload_type = red_pltype_;
        // We have packed 2 payloads.
        fragmentation_.fragmentationVectorSize = kNumFecFragmentationVectors;

        // Copy to local variable, as it will be used outside ACM lock.
        my_fragmentation.CopyFrom(fragmentation_);
        // Store RED length.
        fragmentation_.fragmentationLength[1] = red_length_bytes;
      }
    }
  }

  if (has_data_to_send) {
    CriticalSectionScoped lock(callback_crit_sect_);

    if (packetization_callback_ != NULL) {
      if (fec_active) {
        // Callback with payload data, including redundant data (FEC/RED).
        packetization_callback_->SendData(frame_type, current_payload_type,
                                          rtp_timestamp, stream,
                                          length_bytes,
                                          &my_fragmentation);
      } else {
        // Callback with payload data.
        packetization_callback_->SendData(frame_type, current_payload_type,
                                          rtp_timestamp, stream,
                                          length_bytes, NULL);
      }
    }

    if (vad_callback_ != NULL) {
      // Callback with VAD decision.
      vad_callback_->InFrameType(((int16_t) encoding_type));
    }
  }
  return length_bytes;
}

/////////////////////////////////////////
//   Sender
//

// Initialize send codec.
int32_t AudioCodingModuleImpl::InitializeSender() {
  CriticalSectionScoped lock(acm_crit_sect_);

  // Start with invalid values.
  send_codec_registered_ = false;
  current_send_codec_idx_ = -1;
  send_codec_inst_.plname[0] = '\0';

  // Delete all encoders to start fresh.
  for (int id = 0; id < ACMCodecDB::kMaxNumCodecs; id++) {
    if (codecs_[id] != NULL) {
      codecs_[id]->DestructEncoder();
    }
  }

  // Initialize FEC/RED.
  is_first_red_ = true;
  if (fec_enabled_ || secondary_encoder_.get() != NULL) {
    if (red_buffer_ != NULL) {
      memset(red_buffer_, 0, MAX_PAYLOAD_SIZE_BYTE);
    }
    if (fec_enabled_) {
      ResetFragmentation(kNumFecFragmentationVectors);
    } else {
      ResetFragmentation(0);
    }
  }

  return 0;
}

int32_t AudioCodingModuleImpl::ResetEncoder() {
  CriticalSectionScoped lock(acm_crit_sect_);
  if (!HaveValidEncoder("ResetEncoder")) {
    return -1;
  }
  return codecs_[current_send_codec_idx_]->ResetEncoder();
}

void AudioCodingModuleImpl::UnregisterSendCodec() {
  CriticalSectionScoped lock(acm_crit_sect_);
  send_codec_registered_ = false;
  current_send_codec_idx_ = -1;
  // If send Codec is unregistered then remove the secondary codec as well.
  if (secondary_encoder_.get() != NULL)
    secondary_encoder_.reset();
  return;
}

ACMGenericCodec* AudioCodingModuleImpl::CreateCodec(const CodecInst& codec) {
  ACMGenericCodec* my_codec = NULL;

  my_codec = ACMCodecDB::CreateCodecInstance(&codec);
  if (my_codec == NULL) {
    // Error, could not create the codec.
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "ACMCodecDB::CreateCodecInstance() failed in CreateCodec()");
    return my_codec;
  }
  my_codec->SetUniqueID(id_);
  my_codec->SetNetEqDecodeLock(neteq_.DecodeLock());

  return my_codec;
}

// Check if the given codec is a valid to be registered as send codec.
static int IsValidSendCodec(const CodecInst& send_codec,
                            bool is_primary_encoder,
                            int acm_id,
                            int* mirror_id) {
  if ((send_codec.channels != 1) && (send_codec.channels != 2)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, acm_id,
                 "Wrong number of channels (%d, only mono and stereo are "
                 "supported) for %s encoder", send_codec.channels,
                 is_primary_encoder ? "primary" : "secondary");
    return -1;
  }

  int codec_id = ACMCodecDB::CodecNumber(&send_codec, mirror_id);
  if (codec_id < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, acm_id,
                 "Invalid settings for the send codec.");
    return -1;
  }

  // TODO(tlegrand): Remove this check. Already taken care of in
  // ACMCodecDB::CodecNumber().
  // Check if the payload-type is valid
  if (!ACMCodecDB::ValidPayloadType(send_codec.pltype)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, acm_id,
                 "Invalid payload-type %d for %s.", send_codec.pltype,
                 send_codec.plname);
    return -1;
  }

  // Telephone-event cannot be a send codec.
  if (!STR_CASE_CMP(send_codec.plname, "telephone-event")) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, acm_id,
                 "telephone-event cannot be a send codec");
    *mirror_id = -1;
    return -1;
  }

  if (ACMCodecDB::codec_settings_[codec_id].channel_support
      < send_codec.channels) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, acm_id,
                 "%d number of channels not supportedn for %s.",
                 send_codec.channels, send_codec.plname);
    *mirror_id = -1;
    return -1;
  }

  if (!is_primary_encoder) {
    // If registering the secondary encoder, then RED and CN are not valid
    // choices as encoder.
    if (IsCodecRED(&send_codec)) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, acm_id,
                   "RED cannot be secondary codec");
      *mirror_id = -1;
      return -1;
    }

    if (IsCodecCN(&send_codec)) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, acm_id,
                   "DTX cannot be secondary codec");
      *mirror_id = -1;
      return -1;
    }
  }
  return codec_id;
}

int AudioCodingModuleImpl::RegisterSecondarySendCodec(
    const CodecInst& send_codec) {
  CriticalSectionScoped lock(acm_crit_sect_);
  if (!send_codec_registered_) {
    return -1;
  }
  // Primary and Secondary codecs should have the same sampling rates.
  if (send_codec.plfreq != send_codec_inst_.plfreq) {
    return -1;
  }
  int mirror_id;
  int codec_id = IsValidSendCodec(send_codec, false, id_, &mirror_id);
  if (codec_id < 0) {
    return -1;
  }
  ACMGenericCodec* encoder = CreateCodec(send_codec);
  WebRtcACMCodecParams codec_params;
  // Initialize the codec before registering. For secondary codec VAD & DTX are
  // disabled.
  memcpy(&(codec_params.codec_inst), &send_codec, sizeof(CodecInst));
  codec_params.enable_vad = false;
  codec_params.enable_dtx = false;
  codec_params.vad_mode = VADNormal;
  // Force initialization.
  if (encoder->InitEncoder(&codec_params, true) < 0) {
    // Could not initialize, therefore cannot be registered.
    delete encoder;
    return -1;
  }
  secondary_encoder_.reset(encoder);
  memcpy(&secondary_send_codec_inst_, &send_codec, sizeof(send_codec));

  // Disable VAD & DTX.
  SetVADSafe(false, false, VADNormal);

  // Cleaning.
  if (red_buffer_) {
    memset(red_buffer_, 0, MAX_PAYLOAD_SIZE_BYTE);
  }
  ResetFragmentation(0);
  return 0;
}

void AudioCodingModuleImpl::UnregisterSecondarySendCodec() {
  CriticalSectionScoped lock(acm_crit_sect_);
  if (secondary_encoder_.get() == NULL) {
    return;
  }
  secondary_encoder_.reset();
  ResetFragmentation(0);
}

int AudioCodingModuleImpl::SecondarySendCodec(
    CodecInst* secondary_codec) const {
  CriticalSectionScoped lock(acm_crit_sect_);
  if (secondary_encoder_.get() == NULL) {
    return -1;
  }
  memcpy(secondary_codec, &secondary_send_codec_inst_,
         sizeof(secondary_send_codec_inst_));
  return 0;
}

// Can be called multiple times for Codec, CNG, RED.
int32_t AudioCodingModuleImpl::RegisterSendCodec(
    const CodecInst& send_codec) {
  int mirror_id;
  int codec_id = IsValidSendCodec(send_codec, true, id_, &mirror_id);

  CriticalSectionScoped lock(acm_crit_sect_);

  // Check for reported errors from function IsValidSendCodec().
  if (codec_id < 0) {
    if (!send_codec_registered_) {
      // This values has to be NULL if there is no codec registered.
      current_send_codec_idx_ = -1;
    }
    return -1;
  }

  // RED can be registered with other payload type. If not registered a default
  // payload type is used.
  if (IsCodecRED(&send_codec)) {
    // TODO(tlegrand): Remove this check. Already taken care of in
    // ACMCodecDB::CodecNumber().
    // Check if the payload-type is valid
    if (!ACMCodecDB::ValidPayloadType(send_codec.pltype)) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                   "Invalid payload-type %d for %s.", send_codec.pltype,
                   send_codec.plname);
      return -1;
    }
    // Set RED payload type.
    red_pltype_ = static_cast<uint8_t>(send_codec.pltype);
    return 0;
  }

  // CNG can be registered with other payload type. If not registered the
  // default payload types from codec database will be used.
  if (IsCodecCN(&send_codec)) {
    // CNG is registered.
    switch (send_codec.plfreq) {
      case 8000: {
        cng_nb_pltype_ = static_cast<uint8_t>(send_codec.pltype);
        break;
      }
      case 16000: {
        cng_wb_pltype_ = static_cast<uint8_t>(send_codec.pltype);
        break;
      }
      case 32000: {
        cng_swb_pltype_ = static_cast<uint8_t>(send_codec.pltype);
        break;
      }
      case 48000: {
        cng_fb_pltype_ = static_cast<uint8_t>(send_codec.pltype);
        break;
      }
      default: {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                     "RegisterSendCodec() failed, invalid frequency for CNG "
                     "registration");
        return -1;
      }
    }
    return 0;
  }

  // Set Stereo, and make sure VAD and DTX is turned off.
  if (send_codec.channels == 2) {
    stereo_send_ = true;
    if (vad_enabled_ || dtx_enabled_) {
      WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceAudioCoding, id_,
                   "VAD/DTX is turned off, not supported when sending stereo.");
    }
    vad_enabled_ = false;
    dtx_enabled_ = false;
  } else {
    stereo_send_ = false;
  }

  // Check if the codec is already registered as send codec.
  bool is_send_codec;
  if (send_codec_registered_) {
    int send_codec_mirror_id;
    int send_codec_id = ACMCodecDB::CodecNumber(&send_codec_inst_,
                                                &send_codec_mirror_id);
    assert(send_codec_id >= 0);
    is_send_codec = (send_codec_id == codec_id) ||
        (mirror_id == send_codec_mirror_id);
  } else {
    is_send_codec = false;
  }

  // If there is secondary codec registered and the new send codec has a
  // sampling rate different than that of secondary codec, then unregister the
  // secondary codec.
  if (secondary_encoder_.get() != NULL &&
      secondary_send_codec_inst_.plfreq != send_codec.plfreq) {
    secondary_encoder_.reset();
    ResetFragmentation(0);
  }

  // If new codec, or new settings, register.
  if (!is_send_codec) {
    if (codecs_[mirror_id] == NULL) {
      codecs_[mirror_id] = CreateCodec(send_codec);
      if (codecs_[mirror_id] == NULL) {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                     "Cannot Create the codec");
        return -1;
      }
      mirror_codec_idx_[mirror_id] = mirror_id;
    }

    if (mirror_id != codec_id) {
      codecs_[codec_id] = codecs_[mirror_id];
      mirror_codec_idx_[codec_id] = mirror_id;
    }

    ACMGenericCodec* codec_ptr = codecs_[codec_id];
    int16_t status;
    WebRtcACMCodecParams codec_params;

    memcpy(&(codec_params.codec_inst), &send_codec, sizeof(CodecInst));
    codec_params.enable_vad = vad_enabled_;
    codec_params.enable_dtx = dtx_enabled_;
    codec_params.vad_mode = vad_mode_;
    // Force initialization.
    status = codec_ptr->InitEncoder(&codec_params, true);

    // Check if VAD was turned on, or if error is reported.
    if (status == 1) {
      vad_enabled_ = true;
    } else if (status < 0) {
      // Could not initialize the encoder.

      // Check if already have a registered codec.
      // Depending on that different messages are logged.
      if (!send_codec_registered_) {
        current_send_codec_idx_ = -1;
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                     "Cannot Initialize the encoder No Encoder is registered");
      } else {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                     "Cannot Initialize the encoder, continue encoding with "
                     "the previously registered codec");
      }
      return -1;
    }

    // Everything is fine so we can replace the previous codec with this one.
    if (send_codec_registered_) {
      // If we change codec we start fresh with FEC.
      // This is not strictly required by the standard.
      is_first_red_ = true;

      if (codec_ptr->SetVAD(dtx_enabled_, vad_enabled_, vad_mode_) < 0) {
        // SetVAD failed.
        vad_enabled_ = false;
        dtx_enabled_ = false;
      }
    }

    current_send_codec_idx_ = codec_id;
    send_codec_registered_ = true;
    memcpy(&send_codec_inst_, &send_codec, sizeof(CodecInst));
    previous_pltype_ = send_codec_inst_.pltype;
    return 0;
  } else {
    // If codec is the same as already registered check if any parameters
    // has changed compared to the current values.
    // If any parameter is valid then apply it and record.
    bool force_init = false;

    if (mirror_id != codec_id) {
      codecs_[codec_id] = codecs_[mirror_id];
      mirror_codec_idx_[codec_id] = mirror_id;
    }

    // Check the payload type.
    if (send_codec.pltype != send_codec_inst_.pltype) {
      // At this point check if the given payload type is valid.
      // Record it later when the sampling frequency is changed
      // successfully.
      if (!ACMCodecDB::ValidPayloadType(send_codec.pltype)) {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                     "Out of range payload type");
        return -1;
      }
    }

    // If there is a codec that ONE instance of codec supports multiple
    // sampling frequencies, then we need to take care of it here.
    // one such a codec is iSAC. Both WB and SWB are encoded and decoded
    // with one iSAC instance. Therefore, we need to update the encoder
    // frequency if required.
    if (send_codec_inst_.plfreq != send_codec.plfreq) {
      force_init = true;

      // If sampling frequency is changed we have to start fresh with RED.
      is_first_red_ = true;
    }

    // If packet size or number of channels has changed, we need to
    // re-initialize the encoder.
    if (send_codec_inst_.pacsize != send_codec.pacsize) {
      force_init = true;
    }
    if (send_codec_inst_.channels != send_codec.channels) {
      force_init = true;
    }

    if (force_init) {
      WebRtcACMCodecParams codec_params;

      memcpy(&(codec_params.codec_inst), &send_codec, sizeof(CodecInst));
      codec_params.enable_vad = vad_enabled_;
      codec_params.enable_dtx = dtx_enabled_;
      codec_params.vad_mode = vad_mode_;

      // Force initialization.
      if (codecs_[current_send_codec_idx_]->InitEncoder(&codec_params,
                                                        true) < 0) {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                     "Could not change the codec packet-size.");
        return -1;
      }

      send_codec_inst_.plfreq = send_codec.plfreq;
      send_codec_inst_.pacsize = send_codec.pacsize;
      send_codec_inst_.channels = send_codec.channels;
    }

    // If the change of sampling frequency has been successful then
    // we store the payload-type.
    send_codec_inst_.pltype = send_codec.pltype;

    // Check if a change in Rate is required.
    if (send_codec.rate != send_codec_inst_.rate) {
      if (codecs_[codec_id]->SetBitRate(send_codec.rate) < 0) {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                     "Could not change the codec rate.");
        return -1;
      }
      send_codec_inst_.rate = send_codec.rate;
    }
    previous_pltype_ = send_codec_inst_.pltype;

    return 0;
  }
}

// Get current send codec.
int32_t AudioCodingModuleImpl::SendCodec(
    CodecInst* current_codec) const {
  WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceAudioCoding, id_,
               "SendCodec()");
  CriticalSectionScoped lock(acm_crit_sect_);

  assert(current_codec);
  if (!send_codec_registered_) {
    WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceAudioCoding, id_,
                 "SendCodec Failed, no codec is registered");

    return -1;
  }
  WebRtcACMCodecParams encoder_param;
  codecs_[current_send_codec_idx_]->EncoderParams(&encoder_param);
  encoder_param.codec_inst.pltype = send_codec_inst_.pltype;
  memcpy(current_codec, &(encoder_param.codec_inst), sizeof(CodecInst));

  return 0;
}

// Get current send frequency.
int32_t AudioCodingModuleImpl::SendFrequency() const {
  WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceAudioCoding, id_,
               "SendFrequency()");
  CriticalSectionScoped lock(acm_crit_sect_);

  if (!send_codec_registered_) {
    WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceAudioCoding, id_,
                 "SendFrequency Failed, no codec is registered");

    return -1;
  }

  return send_codec_inst_.plfreq;
}

// Get encode bitrate.
// Adaptive rate codecs return their current encode target rate, while other
// codecs return there longterm avarage or their fixed rate.
int32_t AudioCodingModuleImpl::SendBitrate() const {
  CriticalSectionScoped lock(acm_crit_sect_);

  if (!send_codec_registered_) {
    WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceAudioCoding, id_,
                 "SendBitrate Failed, no codec is registered");

    return -1;
  }

  WebRtcACMCodecParams encoder_param;
  codecs_[current_send_codec_idx_]->EncoderParams(&encoder_param);

  return encoder_param.codec_inst.rate;
}

// Set available bandwidth, inform the encoder about the estimated bandwidth
// received from the remote party.
int32_t AudioCodingModuleImpl::SetReceivedEstimatedBandwidth(
    const int32_t bw) {
  return codecs_[current_send_codec_idx_]->SetEstimatedBandwidth(bw);
}

// Register a transport callback which will be called to deliver
// the encoded buffers.
int32_t AudioCodingModuleImpl::RegisterTransportCallback(
    AudioPacketizationCallback* transport) {
  CriticalSectionScoped lock(callback_crit_sect_);
  packetization_callback_ = transport;
  return 0;
}

// Used by the module to deliver messages to the codec module/application
// AVT(DTMF).
int32_t AudioCodingModuleImpl::RegisterIncomingMessagesCallback(
#ifndef WEBRTC_DTMF_DETECTION
    AudioCodingFeedback* /* incoming_message */,
    const ACMCountries /* cpt */) {
  return -1;
#else
    AudioCodingFeedback* incoming_message,
    const ACMCountries cpt) {
  int16_t status = 0;

  // Enter the critical section for callback.
  {
    CriticalSectionScoped lock(callback_crit_sect_);
    dtmf_callback_ = incoming_message;
  }
  // Enter the ACM critical section to set up the DTMF class.
  {
    CriticalSectionScoped lock(acm_crit_sect_);
    // Check if the call is to disable or enable the callback.
    if (incoming_message == NULL) {
      // Callback is disabled, delete DTMF-detector class.
      if (dtmf_detector_ != NULL) {
        delete dtmf_detector_;
        dtmf_detector_ = NULL;
      }
      status = 0;
    } else {
      status = 0;
      if (dtmf_detector_ == NULL) {
        dtmf_detector_ = new ACMDTMFDetection;
        if (dtmf_detector_ == NULL) {
          status = -1;
        }
      }
      if (status >= 0) {
        status = dtmf_detector_->Enable(cpt);
        if (status < 0) {
          // Failed to initialize if DTMF-detection was not enabled before,
          // delete the class, and set the callback to NULL and return -1.
          delete dtmf_detector_;
          dtmf_detector_ = NULL;
        }
      }
    }
  }
  // Check if we failed in setting up the DTMF-detector class.
  if ((status < 0)) {
    // We failed, we cannot have the callback.
    CriticalSectionScoped lock(callback_crit_sect_);
    dtmf_callback_ = NULL;
  }

  return status;
#endif
}

// Add 10MS of raw (PCM) audio data to the encoder.
int32_t AudioCodingModuleImpl::Add10MsData(
    const AudioFrame& audio_frame) {
  TRACE_EVENT2("webrtc", "ACM::Add10MsData",
               "timestamp", audio_frame.timestamp_,
               "samples_per_channel", audio_frame.samples_per_channel_);

  if (audio_frame.samples_per_channel_ <= 0) {
    assert(false);
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "Cannot Add 10 ms audio, payload length is negative or "
                 "zero");
    return -1;
  }

  // Allow for 8, 16, 32 and 48kHz input audio.
  if ((audio_frame.sample_rate_hz_ != 8000)
      && (audio_frame.sample_rate_hz_ != 16000)
      && (audio_frame.sample_rate_hz_ != 32000)
      && (audio_frame.sample_rate_hz_ != 48000)) {
    assert(false);
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "Cannot Add 10 ms audio, input frequency not valid");
    return -1;
  }

  // If the length and frequency matches. We currently just support raw PCM.
  if ((audio_frame.sample_rate_hz_ / 100)
      != audio_frame.samples_per_channel_) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "Cannot Add 10 ms audio, input frequency and length doesn't"
                 " match");
    return -1;
  }

  if (audio_frame.num_channels_ != 1 && audio_frame.num_channels_ != 2) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "Cannot Add 10 ms audio, invalid number of channels.");
    return -1;
  }

  CriticalSectionScoped lock(acm_crit_sect_);
  // Do we have a codec registered?
  if (!HaveValidEncoder("Add10MsData")) {
    return -1;
  }

  const AudioFrame* ptr_frame;
  // Perform a resampling, also down-mix if it is required and can be
  // performed before resampling (a down mix prior to resampling will take
  // place if both primary and secondary encoders are mono and input is in
  // stereo).
  if (PreprocessToAddData(audio_frame, &ptr_frame) < 0) {
    return -1;
  }

  // Check whether we need an up-mix or down-mix?
  bool remix = ptr_frame->num_channels_ != send_codec_inst_.channels;
  if (secondary_encoder_.get() != NULL) {
    remix = remix ||
        (ptr_frame->num_channels_ != secondary_send_codec_inst_.channels);
  }

  // If a re-mix is required (up or down), this buffer will store re-mixed
  // version of the input.
  int16_t buffer[WEBRTC_10MS_PCM_AUDIO];
  if (remix) {
    if (ptr_frame->num_channels_ == 1) {
      if (UpMix(*ptr_frame, WEBRTC_10MS_PCM_AUDIO, buffer) < 0)
        return -1;
    } else {
      if (DownMix(*ptr_frame, WEBRTC_10MS_PCM_AUDIO, buffer) < 0)
        return -1;
    }
  }

  // When adding data to encoders this pointer is pointing to an audio buffer
  // with correct number of channels.
  const int16_t* ptr_audio = ptr_frame->data_;

  // For pushing data to primary, point the |ptr_audio| to correct buffer.
  if (send_codec_inst_.channels != ptr_frame->num_channels_)
    ptr_audio = buffer;

  if (codecs_[current_send_codec_idx_]->Add10MsData(
      ptr_frame->timestamp_, ptr_audio, ptr_frame->samples_per_channel_,
      send_codec_inst_.channels) < 0)
    return -1;

  if (secondary_encoder_.get() != NULL) {
    // For pushing data to secondary, point the |ptr_audio| to correct buffer.
    ptr_audio = ptr_frame->data_;
    if (secondary_send_codec_inst_.channels != ptr_frame->num_channels_)
      ptr_audio = buffer;

    if (secondary_encoder_->Add10MsData(
        ptr_frame->timestamp_, ptr_audio, ptr_frame->samples_per_channel_,
        secondary_send_codec_inst_.channels) < 0)
      return -1;
  }

  return 0;
}

// Perform a resampling and down-mix if required. We down-mix only if
// encoder is mono and input is stereo. In case of dual-streaming, both
// encoders has to be mono for down-mix to take place.
// |*ptr_out| will point to the pre-processed audio-frame. If no pre-processing
// is required, |*ptr_out| points to |in_frame|.
int AudioCodingModuleImpl::PreprocessToAddData(const AudioFrame& in_frame,
                                               const AudioFrame** ptr_out) {
  // Primary and secondary (if exists) should have the same sampling rate.
  assert((secondary_encoder_.get() != NULL) ?
      secondary_send_codec_inst_.plfreq == send_codec_inst_.plfreq : true);

  bool resample = ((int32_t) in_frame.sample_rate_hz_
      != send_codec_inst_.plfreq);

  // This variable is true if primary codec and secondary codec (if exists)
  // are both mono and input is stereo.
  bool down_mix;
  if (secondary_encoder_.get() != NULL) {
    down_mix = (in_frame.num_channels_ == 2) &&
        (send_codec_inst_.channels == 1) &&
        (secondary_send_codec_inst_.channels == 1);
  } else {
    down_mix = (in_frame.num_channels_ == 2) &&
        (send_codec_inst_.channels == 1);
  }

  if (!down_mix && !resample) {
    // No pre-processing is required.
    last_in_timestamp_ = in_frame.timestamp_;
    last_timestamp_ = in_frame.timestamp_;
    *ptr_out = &in_frame;
    return 0;
  }

  *ptr_out = &preprocess_frame_;
  preprocess_frame_.num_channels_ = in_frame.num_channels_;
  int16_t audio[WEBRTC_10MS_PCM_AUDIO];
  const int16_t* src_ptr_audio = in_frame.data_;
  int16_t* dest_ptr_audio = preprocess_frame_.data_;
  if (down_mix) {
    // If a resampling is required the output of a down-mix is written into a
    // local buffer, otherwise, it will be written to the output frame.
    if (resample)
      dest_ptr_audio = audio;
    if (DownMix(in_frame, WEBRTC_10MS_PCM_AUDIO, dest_ptr_audio) < 0)
      return -1;
    preprocess_frame_.num_channels_ = 1;
    // Set the input of the resampler is the down-mixed signal.
    src_ptr_audio = audio;
  }

  preprocess_frame_.timestamp_ = in_frame.timestamp_;
  preprocess_frame_.samples_per_channel_ = in_frame.samples_per_channel_;
  preprocess_frame_.sample_rate_hz_ = in_frame.sample_rate_hz_;
  // If it is required, we have to do a resampling.
  if (resample) {
    // The result of the resampler is written to output frame.
    dest_ptr_audio = preprocess_frame_.data_;

    uint32_t timestamp_diff;

    // Calculate the timestamp of this frame.
    if (last_in_timestamp_ > in_frame.timestamp_) {
      // A wrap around has happened.
      timestamp_diff = ((uint32_t) 0xFFFFFFFF - last_in_timestamp_)
                                                 + in_frame.timestamp_;
    } else {
      timestamp_diff = in_frame.timestamp_ - last_in_timestamp_;
    }
    preprocess_frame_.timestamp_ = last_timestamp_ +
        static_cast<uint32_t>(timestamp_diff *
            (static_cast<double>(send_codec_inst_.plfreq) /
            static_cast<double>(in_frame.sample_rate_hz_)));

    preprocess_frame_.samples_per_channel_ = input_resampler_.Resample10Msec(
        src_ptr_audio, in_frame.sample_rate_hz_, dest_ptr_audio,
        send_codec_inst_.plfreq, preprocess_frame_.num_channels_);

    if (preprocess_frame_.samples_per_channel_ < 0) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                   "Cannot add 10 ms audio, resmapling failed");
      return -1;
    }
    preprocess_frame_.sample_rate_hz_ = send_codec_inst_.plfreq;
  }
  last_in_timestamp_ = in_frame.timestamp_;
  last_timestamp_ = preprocess_frame_.timestamp_;

  return 0;
}

/////////////////////////////////////////
//   (FEC) Forward Error Correction
//

bool AudioCodingModuleImpl::FECStatus() const {
  CriticalSectionScoped lock(acm_crit_sect_);
  return fec_enabled_;
}

// Configure FEC status i.e on/off.
int32_t
AudioCodingModuleImpl::SetFECStatus(
#ifdef WEBRTC_CODEC_RED
    const bool enable_fec) {
  CriticalSectionScoped lock(acm_crit_sect_);

  if (fec_enabled_ != enable_fec) {
    // Reset the RED buffer.
    memset(red_buffer_, 0, MAX_PAYLOAD_SIZE_BYTE);

    // Reset fragmentation buffers.
    ResetFragmentation(kNumFecFragmentationVectors);
    // Set fec_enabled_.
    fec_enabled_ = enable_fec;
  }
  is_first_red_ = true;  // Make sure we restart FEC.
  return 0;
#else
    const bool /* enable_fec */) {
  fec_enabled_ = false;
  WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceAudioCoding, id_,
               "  WEBRTC_CODEC_RED is undefined => fec_enabled_ = %d",
               fec_enabled_);
  return -1;
#endif
}

/////////////////////////////////////////
//   (VAD) Voice Activity Detection
//
int32_t AudioCodingModuleImpl::SetVAD(const bool enable_dtx,
                                      const bool enable_vad,
                                      const ACMVADMode mode) {
  CriticalSectionScoped lock(acm_crit_sect_);
  return SetVADSafe(enable_dtx, enable_vad, mode);
}

int AudioCodingModuleImpl::SetVADSafe(bool enable_dtx,
                                      bool enable_vad,
                                      ACMVADMode mode) {
  // Sanity check of the mode.
  if ((mode != VADNormal) && (mode != VADLowBitrate)
      && (mode != VADAggr) && (mode != VADVeryAggr)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "Invalid VAD Mode %d, no change is made to VAD/DTX status",
                 static_cast<int>(mode));
    return -1;
  }

  // Check that the send codec is mono. We don't support VAD/DTX for stereo
  // sending.
  if ((enable_dtx || enable_vad) && stereo_send_) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "VAD/DTX not supported for stereo sending");
    return -1;
  }

  // We don't support VAD/DTX when dual-streaming is enabled, i.e.
  // secondary-encoder is registered.
  if ((enable_dtx || enable_vad) && secondary_encoder_.get() != NULL) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "VAD/DTX not supported when dual-streaming is enabled.");
    return -1;
  }

  // If a send codec is registered, set VAD/DTX for the codec.
  if (HaveValidEncoder("SetVAD")) {
    int16_t status = codecs_[current_send_codec_idx_]->SetVAD(enable_dtx,
                                                              enable_vad,
                                                              mode);
    if (status == 1) {
      // Vad was enabled.
      vad_enabled_ = true;
      dtx_enabled_ = enable_dtx;
      vad_mode_ = mode;

      return 0;
    } else if (status < 0) {
      // SetVAD failed.
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                   "SetVAD failed");

      vad_enabled_ = false;
      dtx_enabled_ = false;

      return -1;
    }
  }

  vad_enabled_ = enable_vad;
  dtx_enabled_ = enable_dtx;
  vad_mode_ = mode;

  return 0;
}

// Get VAD/DTX settings.
// TODO(tlegrand): Change this method to void.
int32_t AudioCodingModuleImpl::VAD(bool* dtx_enabled, bool* vad_enabled,
                                   ACMVADMode* mode) const {
  CriticalSectionScoped lock(acm_crit_sect_);

  *dtx_enabled = dtx_enabled_;
  *vad_enabled = vad_enabled_;
  *mode = vad_mode_;

  return 0;
}

/////////////////////////////////////////
//   Receiver
//

int32_t AudioCodingModuleImpl::InitializeReceiver() {
  CriticalSectionScoped lock(acm_crit_sect_);
  return InitializeReceiverSafe();
}

// Initialize receiver, resets codec database etc.
int32_t AudioCodingModuleImpl::InitializeReceiverSafe() {
  initial_delay_ms_ = 0;
  num_packets_accumulated_ = 0;
  num_bytes_accumulated_ = 0;
  accumulated_audio_ms_ = 0;
  first_payload_received_ = 0;
  last_incoming_send_timestamp_ = 0;
  track_neteq_buffer_ = false;
  playout_ts_ = 0;
  // If the receiver is already initialized then we want to destroy any
  // existing decoders. After a call to this function, we should have a clean
  // start-up.
  if (receiver_initialized_) {
    for (int i = 0; i < ACMCodecDB::kNumCodecs; i++) {
      if (UnregisterReceiveCodecSafe(i) < 0) {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                     "InitializeReceiver() failed, Could not unregister codec");
        return -1;
      }
    }
  }
  if (neteq_.Init() != 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "InitializeReceiver() failed, Could not initialize NetEQ");
    return -1;
  }
  neteq_.set_id(id_);
  if (neteq_.AllocatePacketBuffer(ACMCodecDB::NetEQDecoders(),
                                   ACMCodecDB::kNumCodecs) != 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "NetEQ cannot allocate_packet Buffer");
    return -1;
  }

  // Register RED and CN.
  for (int i = 0; i < ACMCodecDB::kNumCodecs; i++) {
    if (IsCodecRED(i) || IsCodecCN(i)) {
      if (RegisterRecCodecMSSafe(ACMCodecDB::database_[i], i, i,
                                 ACMNetEQ::kMasterJb) < 0) {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                     "Cannot register master codec.");
        return -1;
      }
      registered_pltypes_[i] = ACMCodecDB::database_[i].pltype;
    }
  }

  receiver_initialized_ = true;
  return 0;
}

// Reset the decoder state.
int32_t AudioCodingModuleImpl::ResetDecoder() {
  CriticalSectionScoped lock(acm_crit_sect_);

  for (int id = 0; id < ACMCodecDB::kMaxNumCodecs; id++) {
    if ((codecs_[id] != NULL) && (registered_pltypes_[id] != -1)) {
      if (codecs_[id]->ResetDecoder(registered_pltypes_[id]) < 0) {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                     "ResetDecoder failed:");
        return -1;
      }
    }
  }
  return neteq_.FlushBuffers();
}

// Get current receive frequency.
int32_t AudioCodingModuleImpl::ReceiveFrequency() const {
  WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceAudioCoding, id_,
               "ReceiveFrequency()");
  WebRtcACMCodecParams codec_params;

  CriticalSectionScoped lock(acm_crit_sect_);
  if (DecoderParamByPlType(last_recv_audio_codec_pltype_, codec_params) < 0) {
    return neteq_.CurrentSampFreqHz();
  } else if (codec_params.codec_inst.plfreq == 48000) {
    // TODO(tlegrand): Remove this option when we have full 48 kHz support.
    return 32000;
  } else {
    return codec_params.codec_inst.plfreq;
  }
}

// Get current playout frequency.
int32_t AudioCodingModuleImpl::PlayoutFrequency() const {
  WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceAudioCoding, id_,
               "PlayoutFrequency()");

  CriticalSectionScoped lock(acm_crit_sect_);

  return neteq_.CurrentSampFreqHz();
}

// Register possible receive codecs, can be called multiple times,
// for codecs, CNG (NB, WB and SWB), DTMF, RED.
int32_t AudioCodingModuleImpl::RegisterReceiveCodec(
    const CodecInst& receive_codec) {
  CriticalSectionScoped lock(acm_crit_sect_);

  if (receive_codec.channels > 2) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "More than 2 audio channel is not supported.");
    return -1;
  }

  int mirror_id;
  int codec_id = ACMCodecDB::ReceiverCodecNumber(&receive_codec, &mirror_id);

  if (codec_id < 0 || codec_id >= ACMCodecDB::kNumCodecs) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "Wrong codec params to be registered as receive codec");
    return -1;
  }
  // Check if the payload-type is valid.
  if (!ACMCodecDB::ValidPayloadType(receive_codec.pltype)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "Invalid payload-type %d for %s.", receive_codec.pltype,
                 receive_codec.plname);
    return -1;
  }

  if (!receiver_initialized_) {
    if (InitializeReceiverSafe() < 0) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                   "Cannot initialize reciver, so failed registering a codec.");
      return -1;
    }
  }

  // If codec already registered, unregister. Except for CN where we only
  // unregister if payload type is changing.
  if ((registered_pltypes_[codec_id] == receive_codec.pltype)
      && IsCodecCN(&receive_codec)) {
    // Codec already registered as receiver with this payload type. Nothing
    // to be done.
    return 0;
  } else if (registered_pltypes_[codec_id] != -1) {
    if (UnregisterReceiveCodecSafe(codec_id) < 0) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                   "Cannot register master codec.");
      return -1;
    }
  }

  if (RegisterRecCodecMSSafe(receive_codec, codec_id, mirror_id,
                             ACMNetEQ::kMasterJb) < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "Cannot register master codec.");
    return -1;
  }

  // TODO(andrew): Refactor how the slave is initialized. Can we instead
  // always start up a slave and pre-register CN and RED? We should be able
  // to get rid of stereo_receive_registered_.
  // http://code.google.com/p/webrtc/issues/detail?id=453

  // Register stereo codecs with the slave, or, if we've had already seen a
  // stereo codec, register CN or RED as a special case.
  if (receive_codec.channels == 2 ||
      (stereo_receive_registered_ && (IsCodecCN(&receive_codec) ||
          IsCodecRED(&receive_codec)))) {
    // TODO(andrew): refactor this block to combine with InitStereoSlave().

    if (!stereo_receive_registered_) {
      // This is the first time a stereo codec has been registered. Make
      // some stereo preparations.

      // Add a stereo slave.
      assert(neteq_.num_slaves() == 0);
      if (neteq_.AddSlave(ACMCodecDB::NetEQDecoders(),
                           ACMCodecDB::kNumCodecs) < 0) {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                     "Cannot add slave jitter buffer to NetEQ.");
        return -1;
      }

      // Register any existing CN or RED codecs with the slave and as stereo.
      for (int i = 0; i < ACMCodecDB::kNumCodecs; i++) {
        if (registered_pltypes_[i] != -1 && (IsCodecRED(i) || IsCodecCN(i))) {
          stereo_receive_[i] = true;

          CodecInst codec;
          memcpy(&codec, &ACMCodecDB::database_[i], sizeof(CodecInst));
          codec.pltype = registered_pltypes_[i];
          if (RegisterRecCodecMSSafe(codec, i, i, ACMNetEQ::kSlaveJb) < 0) {
            WEBRTC_TRACE(kTraceError, kTraceAudioCoding, id_,
                         "Cannot register slave codec.");
            return -1;
          }
        }
      }
    }

    if (RegisterRecCodecMSSafe(receive_codec, codec_id, mirror_id,
                               ACMNetEQ::kSlaveJb) < 0) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                   "Cannot register slave codec.");
      return -1;
    }

    if (!stereo_receive_[codec_id] &&
        (last_recv_audio_codec_pltype_ == receive_codec.pltype)) {
      // The last received payload type is the same as the one we are
      // registering. Expected number of channels to receive is one (mono),
      // but we are now registering the receiving codec as stereo (number of
      // channels is 2).
      // Set |last_recv_audio_coded_pltype_| to invalid value to trigger a
      // flush in NetEq, and a reset of expected number of channels next time a
      // packet is received in AudioCodingModuleImpl::IncomingPacket().
      last_recv_audio_codec_pltype_ = -1;
    }

    stereo_receive_[codec_id] = true;
    stereo_receive_registered_ = true;
  } else {
    if (last_recv_audio_codec_pltype_ == receive_codec.pltype &&
        expected_channels_ == 2) {
      // The last received payload type is the same as the one we are
      // registering. Expected number of channels to receive is two (stereo),
      // but we are now registering the receiving codec as mono (number of
      // channels is 1).
      // Set |last_recv_audio_coded_pl_type_| to invalid value to trigger a
      // flush in NetEq, and a reset of expected number of channels next time a
      // packet is received in AudioCodingModuleImpl::IncomingPacket().
      last_recv_audio_codec_pltype_ = -1;
    }
    stereo_receive_[codec_id] = false;
  }

  registered_pltypes_[codec_id] = receive_codec.pltype;

  if (IsCodecRED(&receive_codec)) {
    receive_red_pltype_ = receive_codec.pltype;
  }
  return 0;
}

int32_t AudioCodingModuleImpl::RegisterRecCodecMSSafe(
    const CodecInst& receive_codec, int16_t codec_id,
    int16_t mirror_id, ACMNetEQ::JitterBuffer jitter_buffer) {
  ACMGenericCodec** codecs;
  if (jitter_buffer == ACMNetEQ::kMasterJb) {
    codecs = &codecs_[0];
  } else if (jitter_buffer == ACMNetEQ::kSlaveJb) {
    codecs = &slave_codecs_[0];
    if (codecs_[codec_id]->IsTrueStereoCodec()) {
      // True stereo codecs need to use the same codec memory
      // for both master and slave.
      slave_codecs_[mirror_id] = codecs_[mirror_id];
      mirror_codec_idx_[mirror_id] = mirror_id;
    }
  } else {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "RegisterReceiveCodecMSSafe failed, jitter_buffer is neither "
                 "master or slave ");
    return -1;
  }

  if (codecs[mirror_id] == NULL) {
    codecs[mirror_id] = CreateCodec(receive_codec);
    if (codecs[mirror_id] == NULL) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                   "Cannot create codec to register as receive codec");
      return -1;
    }
    mirror_codec_idx_[mirror_id] = mirror_id;
  }
  if (mirror_id != codec_id) {
    codecs[codec_id] = codecs[mirror_id];
    mirror_codec_idx_[codec_id] = mirror_id;
  }

  codecs[codec_id]->SetIsMaster(jitter_buffer == ACMNetEQ::kMasterJb);

  int16_t status = 0;
  WebRtcACMCodecParams codec_params;
  memcpy(&(codec_params.codec_inst), &receive_codec, sizeof(CodecInst));
  codec_params.enable_vad = false;
  codec_params.enable_dtx = false;
  codec_params.vad_mode = VADNormal;
  if (!codecs[codec_id]->DecoderInitialized()) {
    // Force initialization.
    status = codecs[codec_id]->InitDecoder(&codec_params, true);
    if (status < 0) {
      // Could not initialize the decoder, we don't want to
      // continue if we could not initialize properly.
      WEBRTC_TRACE(
          webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
          "could not initialize the receive codec, codec not registered");

      return -1;
    }
  } else if (mirror_id != codec_id) {
    // Currently this only happens for iSAC.
    // We have to store the decoder parameters.
    codecs[codec_id]->SaveDecoderParam(&codec_params);
  }

  if (codecs[codec_id]->RegisterInNetEq(&neteq_, receive_codec) != 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "Receive codec could not be registered in NetEQ");
    return -1;
  }
  // Guarantee that the same payload-type that is
  // registered in NetEQ is stored in the codec.
  codecs[codec_id]->SaveDecoderParam(&codec_params);

  return status;
}

// Get current received codec.
int32_t AudioCodingModuleImpl::ReceiveCodec(
    CodecInst* current_codec) const {
  WebRtcACMCodecParams decoder_param;
  CriticalSectionScoped lock(acm_crit_sect_);

  for (int id = 0; id < ACMCodecDB::kMaxNumCodecs; id++) {
    if (codecs_[id] != NULL) {
      if (codecs_[id]->DecoderInitialized()) {
        if (codecs_[id]->DecoderParams(&decoder_param,
                                       last_recv_audio_codec_pltype_)) {
          memcpy(current_codec, &decoder_param.codec_inst,
                 sizeof(CodecInst));
          return 0;
        }
      }
    }
  }

  // If we are here then we haven't found any codec. Set codec pltype to -1 to
  // indicate that the structure is invalid and return -1.
  current_codec->pltype = -1;
  return -1;
}

// Incoming packet from network parsed and ready for decode.
int32_t AudioCodingModuleImpl::IncomingPacket(
    const uint8_t* incoming_payload,
    const int32_t payload_length,
    const WebRtcRTPHeader& rtp_info) {
  WebRtcRTPHeader rtp_header;

  memcpy(&rtp_header, &rtp_info, sizeof(WebRtcRTPHeader));

  if (payload_length < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "IncomingPacket() Error, payload-length cannot be negative");
    return -1;
  }

  {
    // Store the payload Type. This will be used to retrieve "received codec"
    // and "received frequency."
    CriticalSectionScoped lock(acm_crit_sect_);

    // Check there are packets missed between the last injected packet, and the
    // latest received packet. If so and we are in AV-sync mode then we would
    // like to fill the gap. Shouldn't be the first payload.
    if (av_sync_ && first_payload_received_ &&
        rtp_info.header.sequenceNumber > last_sequence_number_ + 1) {
      // If the last packet pushed was sync-packet account for all missing
      // packets. Otherwise leave some room for PLC.
      if (last_packet_was_sync_) {
        while (rtp_info.header.sequenceNumber > last_sequence_number_ + 2) {
          PushSyncPacketSafe();
        }
      } else {
        // Leave two packet room for NetEq perform PLC.
        if (rtp_info.header.sequenceNumber > last_sequence_number_ + 3) {
          last_sequence_number_ += 2;
          last_incoming_send_timestamp_ += last_timestamp_diff_ * 2;
          last_receive_timestamp_ += 2 * last_timestamp_diff_;
          while (rtp_info.header.sequenceNumber > last_sequence_number_ + 1)
            PushSyncPacketSafe();
        }
      }
    }

    uint8_t my_payload_type;

    // Check if this is an RED payload.
    if (rtp_info.header.payloadType == receive_red_pltype_) {
      // Get the primary payload-type.
      my_payload_type = incoming_payload[0] & 0x7F;
    } else {
      my_payload_type = rtp_info.header.payloadType;
    }

    // If payload is audio, check if received payload is different from
    // previous.
    if (!rtp_info.type.Audio.isCNG) {
      // This is Audio not CNG.

      if (my_payload_type != last_recv_audio_codec_pltype_) {
        // We detect a change in payload type. It is necessary for iSAC
        // we are going to use ONE iSAC instance for decoding both WB and
        // SWB payloads. If payload is changed there might be a need to reset
        // sampling rate of decoder. depending what we have received "now".
        for (int i = 0; i < ACMCodecDB::kMaxNumCodecs; i++) {
          if (registered_pltypes_[i] == my_payload_type) {
            if (UpdateUponReceivingCodec(i) != 0)
              return -1;
            break;
          }
        }
        // Codec is changed, there might be a jump in timestamp, therefore,
        // we have to reset some variables that track NetEq buffer.
        if (track_neteq_buffer_ || av_sync_) {
          last_incoming_send_timestamp_ = rtp_info.header.timestamp;
        }

        if (nack_enabled_) {
          assert(nack_.get());
          // Codec is changed, reset NACK and update sampling rate.
          nack_->Reset();
          nack_->UpdateSampleRate(
              ACMCodecDB::database_[current_receive_codec_idx_].plfreq);
        }
      }
      last_recv_audio_codec_pltype_ = my_payload_type;
    }

    // Current timestamp based on the receiver sampling frequency.
    last_receive_timestamp_ = NowTimestamp(current_receive_codec_idx_);

    if (nack_enabled_) {
      assert(nack_.get());
      nack_->UpdateLastReceivedPacket(rtp_header.header.sequenceNumber,
                                      rtp_header.header.timestamp);
    }
  }

  int per_neteq_payload_length = payload_length;
  // Split the payload for stereo packets, so that first half of payload
  // vector holds left channel, and second half holds right channel.
  if (expected_channels_ == 2) {
    if (!rtp_info.type.Audio.isCNG) {
      // Create a new vector for the payload, maximum payload size.
      int32_t length = payload_length;
      uint8_t payload[kMaxPacketSize];
      assert(payload_length <= kMaxPacketSize);
      memcpy(payload, incoming_payload, payload_length);
      codecs_[current_receive_codec_idx_]->SplitStereoPacket(payload, &length);
      rtp_header.type.Audio.channel = 2;
      per_neteq_payload_length = length / 2;
      // Insert packet into NetEQ.
      if (neteq_.RecIn(payload, length, rtp_header,
                       last_receive_timestamp_) < 0)
        return -1;
    } else {
      // If we receive a CNG packet while expecting stereo, we ignore the
      // packet and continue. CNG is not supported for stereo.
      return 0;
    }
  } else {
    if (neteq_.RecIn(incoming_payload, payload_length, rtp_header,
                     last_receive_timestamp_) < 0)
      return -1;
  }

  {
    CriticalSectionScoped lock(acm_crit_sect_);

    // Update buffering uses |last_incoming_send_timestamp_| so it should be
    // before the next block.
    if (track_neteq_buffer_)
      UpdateBufferingSafe(rtp_header, per_neteq_payload_length);

    if (av_sync_) {
      if (rtp_info.header.sequenceNumber == last_sequence_number_ + 1) {
        last_timestamp_diff_ = rtp_info.header.timestamp -
            last_incoming_send_timestamp_;
      }
      last_sequence_number_ = rtp_info.header.sequenceNumber;
      last_ssrc_ = rtp_info.header.ssrc;
      last_packet_was_sync_ = false;
    }

    if (av_sync_ || track_neteq_buffer_) {
      last_incoming_send_timestamp_ = rtp_info.header.timestamp;
    }

    // Set the following regardless of tracking NetEq buffer or being in
    // AV-sync mode. Only if the received packet is not CNG.
    if (!rtp_info.type.Audio.isCNG)
      first_payload_received_ = true;
  }
  return 0;
}

int AudioCodingModuleImpl::UpdateUponReceivingCodec(int index) {
  if (codecs_[index] == NULL) {
    WEBRTC_TRACE(kTraceError, kTraceAudioCoding, id_,
                 "IncomingPacket() error: payload type found but "
                 "corresponding codec is NULL");
    return -1;
  }
  codecs_[index]->UpdateDecoderSampFreq(index);
  neteq_.set_received_stereo(stereo_receive_[index]);
  current_receive_codec_idx_ = index;

  // If we have a change in the expected number of channels, flush packet
  // buffers in NetEQ.
  if ((stereo_receive_[index] && (expected_channels_ == 1)) ||
      (!stereo_receive_[index] && (expected_channels_ == 2))) {
    neteq_.FlushBuffers();
    codecs_[index]->ResetDecoder(registered_pltypes_[index]);
  }

  if (stereo_receive_[index] && (expected_channels_ == 1)) {
    // When switching from a mono to stereo codec reset the slave.
    if (InitStereoSlave() != 0)
      return -1;
  }

  // Store number of channels we expect to receive for the current payload type.
  if (stereo_receive_[index]) {
    expected_channels_ = 2;
  } else {
    expected_channels_ = 1;
  }

  // Reset previous received channel.
  prev_received_channel_ = 0;
  return 0;
}

bool AudioCodingModuleImpl::IsCodecForSlave(int index) const {
  return (registered_pltypes_[index] != -1 && stereo_receive_[index]);
}

int AudioCodingModuleImpl::InitStereoSlave() {
  neteq_.RemoveSlaves();

  if (neteq_.AddSlave(ACMCodecDB::NetEQDecoders(),
                       ACMCodecDB::kNumCodecs) < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "Cannot add slave jitter buffer to NetEQ.");
    return -1;
  }

  // Register all needed codecs with slave.
  for (int i = 0; i < ACMCodecDB::kNumCodecs; i++) {
    if (codecs_[i] != NULL && IsCodecForSlave(i)) {
      WebRtcACMCodecParams decoder_params;
      if (codecs_[i]->DecoderParams(&decoder_params, registered_pltypes_[i])) {
        if (RegisterRecCodecMSSafe(decoder_params.codec_inst,
                                   i, ACMCodecDB::MirrorID(i),
                                   ACMNetEQ::kSlaveJb) < 0) {
          WEBRTC_TRACE(kTraceError, kTraceAudioCoding, id_,
                       "Cannot register slave codec.");
          return -1;
        }
      }
    }
  }
  return 0;
}

// Minimum playout delay (Used for lip-sync).
int AudioCodingModuleImpl::SetMinimumPlayoutDelay(int time_ms) {
  if ((time_ms < 0) || (time_ms > 10000)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "Delay must be in the range of 0-10000 milliseconds.");
    return -1;
  }
  {
    CriticalSectionScoped lock(acm_crit_sect_);
    // Don't let the extra delay modified while accumulating buffers in NetEq.
    if (track_neteq_buffer_ && first_payload_received_)
      return 0;
  }
  return neteq_.SetMinimumDelay(time_ms);
}

// Get Dtmf playout status.
bool AudioCodingModuleImpl::DtmfPlayoutStatus() const {
#ifndef WEBRTC_CODEC_AVT
  return false;
#else
  return neteq_.avt_playout();
#endif
}

// Configure Dtmf playout status i.e on/off playout the incoming outband
// Dtmf tone.
int32_t AudioCodingModuleImpl::SetDtmfPlayoutStatus(
#ifndef WEBRTC_CODEC_AVT
    const bool /* enable */) {
  WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceAudioCoding, id_,
              "SetDtmfPlayoutStatus() failed: AVT is not supported.");
  return -1;
#else
    const bool enable) {
  return neteq_.SetAVTPlayout(enable);
#endif
}

// Estimate the Bandwidth based on the incoming stream, needed for one way
// audio where the RTCP send the BW estimate.
// This is also done in the RTP module.
int32_t AudioCodingModuleImpl::DecoderEstimatedBandwidth() const {
  CodecInst codec;
  int16_t codec_id = -1;
  int pltype_wb;
  int pltype_swb;

  // Get iSAC settings.
  for (int id = 0; id < ACMCodecDB::kNumCodecs; id++) {
    // Store codec settings for codec number "codeCntr" in the output struct.
    ACMCodecDB::Codec(id, &codec);

    if (!STR_CASE_CMP(codec.plname, "isac")) {
      codec_id = 1;
      pltype_wb = codec.pltype;

      ACMCodecDB::Codec(id + 1, &codec);
      pltype_swb = codec.pltype;

      break;
    }
  }

  if (codec_id < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "DecoderEstimatedBandwidth failed");
    return -1;
  }

  if ((last_recv_audio_codec_pltype_ == pltype_wb) ||
      (last_recv_audio_codec_pltype_ == pltype_swb)) {
    return codecs_[codec_id]->GetEstimatedBandwidth();
  } else {
    return -1;
  }
}

// Set playout mode for: voice, fax, or streaming.
int32_t AudioCodingModuleImpl::SetPlayoutMode(
    const AudioPlayoutMode mode) {
  if ((mode != voice) && (mode != fax) && (mode != streaming) &&
      (mode != off)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "Invalid playout mode.");
    return -1;
  }
  return neteq_.SetPlayoutMode(mode);
}

// Get playout mode voice, fax.
AudioPlayoutMode AudioCodingModuleImpl::PlayoutMode() const {
  return neteq_.playout_mode();
}

// Get 10 milliseconds of raw audio data to play out.
// Automatic resample to the requested frequency.
int32_t AudioCodingModuleImpl::PlayoutData10Ms(
    int32_t desired_freq_hz, AudioFrame* audio_frame) {
  TRACE_EVENT_ASYNC_BEGIN0("webrtc", "ACM::PlayoutData10Ms", 0);
  bool stereo_mode;

  if (GetSilence(desired_freq_hz, audio_frame)) {
     TRACE_EVENT_ASYNC_END1("webrtc", "ACM::PlayoutData10Ms", 0,
                            "silence", true);
     return 0;  // Silence is generated, return.
  }

  // RecOut always returns 10 ms.
  if (neteq_.RecOut(audio_frame_) != 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "PlayoutData failed, RecOut Failed");
    return -1;
  }
  int seq_num;
  uint32_t timestamp;
  bool update_nack = nack_enabled_ &&  // Update NACK only if it is enabled.
      neteq_.DecodedRtpInfo(&seq_num, &timestamp);

  audio_frame->num_channels_ = audio_frame_.num_channels_;
  audio_frame->vad_activity_ = audio_frame_.vad_activity_;
  audio_frame->speech_type_ = audio_frame_.speech_type_;

  stereo_mode = (audio_frame_.num_channels_ > 1);

  // For stereo playout:
  // Master and Slave samples are interleaved starting with Master.
  const uint16_t receive_freq =
      static_cast<uint16_t>(audio_frame_.sample_rate_hz_);
  bool tone_detected = false;
  int16_t last_detected_tone;
  int16_t tone;

  // Limit the scope of ACM Critical section.
  {
    CriticalSectionScoped lock(acm_crit_sect_);

    if (update_nack) {
      assert(nack_.get());
      nack_->UpdateLastDecodedPacket(seq_num, timestamp);
    }

    // If we are in AV-sync and have already received an audio packet, but the
    // latest packet is too late, then insert sync packet.
    if (av_sync_ && first_payload_received_ &&
        NowTimestamp(current_receive_codec_idx_) > 5 * last_timestamp_diff_ +
        last_receive_timestamp_) {
      if (!last_packet_was_sync_) {
        // If the last packet inserted has been a regular packet Skip two
        // packets to give room for PLC.
        last_incoming_send_timestamp_ += 2 * last_timestamp_diff_;
        last_sequence_number_ += 2;
        last_receive_timestamp_ += 2 * last_timestamp_diff_;
      }

      // One sync packet.
      if (PushSyncPacketSafe() < 0)
        return -1;
    }

    if ((receive_freq != desired_freq_hz) && (desired_freq_hz != -1)) {
      TRACE_EVENT_ASYNC_END2("webrtc", "ACM::PlayoutData10Ms", 0,
                             "stereo", stereo_mode, "resample", true);
      // Resample payload_data.
      int16_t temp_len = output_resampler_.Resample10Msec(
          audio_frame_.data_, receive_freq, audio_frame->data_,
          desired_freq_hz, audio_frame_.num_channels_);

      if (temp_len < 0) {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                     "PlayoutData failed, resampler failed");
        return -1;
      }

      // Set the payload data length from the resampler.
      audio_frame->samples_per_channel_ = (uint16_t) temp_len;
      // Set the sampling frequency.
      audio_frame->sample_rate_hz_ = desired_freq_hz;
    } else {
      TRACE_EVENT_ASYNC_END2("webrtc", "ACM::PlayoutData10Ms", 0,
                             "stereo", stereo_mode, "resample", false);
      memcpy(audio_frame->data_, audio_frame_.data_,
             audio_frame_.samples_per_channel_ * audio_frame->num_channels_
             * sizeof(int16_t));
      // Set the payload length.
      audio_frame->samples_per_channel_ =
          audio_frame_.samples_per_channel_;
      // Set the sampling frequency.
      audio_frame->sample_rate_hz_ = receive_freq;
    }

    // Tone detection done for master channel.
    if (dtmf_detector_ != NULL) {
      // Dtmf Detection.
      if (audio_frame->sample_rate_hz_ == 8000) {
        // Use audio_frame->data_ then Dtmf detector doesn't
        // need resampling.
        if (!stereo_mode) {
          dtmf_detector_->Detect(audio_frame->data_,
                                 audio_frame->samples_per_channel_,
                                 audio_frame->sample_rate_hz_, tone_detected,
                                 tone);
        } else {
          // We are in 8 kHz so the master channel needs only 80 samples.
          int16_t master_channel[80];
          for (int n = 0; n < 80; n++) {
            master_channel[n] = audio_frame->data_[n << 1];
          }
          dtmf_detector_->Detect(master_channel,
                                 audio_frame->samples_per_channel_,
                                 audio_frame->sample_rate_hz_, tone_detected,
                                 tone);
        }
      } else {
        // Do the detection on the audio that we got from NetEQ (audio_frame_).
        if (!stereo_mode) {
          dtmf_detector_->Detect(audio_frame_.data_,
                                 audio_frame_.samples_per_channel_,
                                 receive_freq, tone_detected, tone);
        } else {
          int16_t master_channel[WEBRTC_10MS_PCM_AUDIO];
          for (int n = 0; n < audio_frame_.samples_per_channel_; n++) {
            master_channel[n] = audio_frame_.data_[n << 1];
          }
          dtmf_detector_->Detect(master_channel,
                                 audio_frame_.samples_per_channel_,
                                 receive_freq, tone_detected, tone);
        }
      }
    }

    // We want to do this while we are in acm_crit_sect_.
    // (Doesn't really need to initialize the following
    // variable but Linux complains if we don't.)
    last_detected_tone = kACMToneEnd;
    if (tone_detected) {
      last_detected_tone = last_detected_tone_;
      last_detected_tone_ = tone;
    }
  }

  if (tone_detected) {
    // We will deal with callback here, so enter callback critical section.
    CriticalSectionScoped lock(callback_crit_sect_);

    if (dtmf_callback_ != NULL) {
      if (tone != kACMToneEnd) {
        // just a tone
        dtmf_callback_->IncomingDtmf((uint8_t) tone, false);
      } else if ((tone == kACMToneEnd) && (last_detected_tone != kACMToneEnd)) {
        // The tone is "END" and the previously detected tone is
        // not "END," so call fir an end.
        dtmf_callback_->IncomingDtmf((uint8_t) last_detected_tone, true);
      }
    }
  }

  audio_frame->id_ = id_;
  audio_frame->energy_ = -1;
  audio_frame->timestamp_ = 0;

  return 0;
}

/////////////////////////////////////////
//   (CNG) Comfort Noise Generation
//   Generate comfort noise when receiving DTX packets
//

// Get VAD aggressiveness on the incoming stream
ACMVADMode AudioCodingModuleImpl::ReceiveVADMode() const {
  return neteq_.vad_mode();
}

// Configure VAD aggressiveness on the incoming stream.
int16_t AudioCodingModuleImpl::SetReceiveVADMode(const ACMVADMode mode) {
  return neteq_.SetVADMode(mode);
}

/////////////////////////////////////////
//   Statistics
//

int32_t AudioCodingModuleImpl::NetworkStatistics(
    ACMNetworkStatistics* statistics) const {
  int32_t status;
  status = neteq_.NetworkStatistics(statistics);
  return status;
}

void AudioCodingModuleImpl::DestructEncoderInst(void* inst) {
  WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceAudioCoding, id_,
               "DestructEncoderInst()");
  if (!HaveValidEncoder("DestructEncoderInst")) {
    return;
  }

  codecs_[current_send_codec_idx_]->DestructEncoderInst(inst);
}

int16_t AudioCodingModuleImpl::AudioBuffer(
    WebRtcACMAudioBuff& buffer) {
  WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceAudioCoding, id_,
               "AudioBuffer()");
  if (!HaveValidEncoder("AudioBuffer")) {
    return -1;
  }
  buffer.last_in_timestamp = last_in_timestamp_;
  return codecs_[current_send_codec_idx_]->AudioBuffer(buffer);
}

int16_t AudioCodingModuleImpl::SetAudioBuffer(
    WebRtcACMAudioBuff& buffer) {
  WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceAudioCoding, id_,
               "SetAudioBuffer()");
  if (!HaveValidEncoder("SetAudioBuffer")) {
    return -1;
  }
  return codecs_[current_send_codec_idx_]->SetAudioBuffer(buffer);
}

uint32_t AudioCodingModuleImpl::EarliestTimestamp() const {
  WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceAudioCoding, id_,
               "EarliestTimestamp()");
  if (!HaveValidEncoder("EarliestTimestamp")) {
    return -1;
  }
  return codecs_[current_send_codec_idx_]->EarliestTimestamp();
}

int32_t AudioCodingModuleImpl::RegisterVADCallback(
    ACMVADCallback* vad_callback) {
  WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceAudioCoding, id_,
               "RegisterVADCallback()");
  CriticalSectionScoped lock(callback_crit_sect_);
  vad_callback_ = vad_callback;
  return 0;
}

// TODO(turajs): Remove this API if it is not used.
// TODO(tlegrand): Modify this function to work for stereo, and add tests.
// TODO(turajs): Receive timestamp in this method is incremented by frame-size
// and does not reflect the true receive frame-size. Therefore, subsequent
// jitter computations are not accurate.
int32_t AudioCodingModuleImpl::IncomingPayload(
    const uint8_t* incoming_payload, const int32_t payload_length,
    const uint8_t payload_type, const uint32_t timestamp) {
  if (payload_length < 0) {
    // Log error in trace file.
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "IncomingPacket() Error, payload-length cannot be negative");
    return -1;
  }

  if (dummy_rtp_header_ == NULL) {
    // This is the first time that we are using |dummy_rtp_header_|
    // so we have to create it.
    WebRtcACMCodecParams codec_params;
    dummy_rtp_header_ = new WebRtcRTPHeader;
    if (dummy_rtp_header_ == NULL) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                   "IncomingPayload() Error, out of memory");
      return -1;
    }
    dummy_rtp_header_->header.payloadType = payload_type;
    // Don't matter in this case.
    dummy_rtp_header_->header.ssrc = 0;
    dummy_rtp_header_->header.markerBit = false;
    // Start with random numbers.
    dummy_rtp_header_->header.sequenceNumber = rand();
    dummy_rtp_header_->header.timestamp =
        (static_cast<uint32_t>(rand()) << 16) +
        static_cast<uint32_t>(rand());
    dummy_rtp_header_->type.Audio.channel = 1;

    if (DecoderParamByPlType(payload_type, codec_params) < 0) {
      // We didn't find a codec with the given payload.
      // Something is wrong we exit, but we delete |dummy_rtp_header_|
      // and set it to NULL to start clean next time.
      delete dummy_rtp_header_;
      dummy_rtp_header_ = NULL;
      return -1;
    }
    recv_pl_frame_size_smpls_ = codec_params.codec_inst.pacsize;
  }

  if (payload_type != dummy_rtp_header_->header.payloadType) {
    // Payload type has changed since the last time we might need to
    // update the frame-size.
    WebRtcACMCodecParams codec_params;
    if (DecoderParamByPlType(payload_type, codec_params) < 0) {
      // We didn't find a codec with the given payload.
      return -1;
    }
    recv_pl_frame_size_smpls_ = codec_params.codec_inst.pacsize;
    dummy_rtp_header_->header.payloadType = payload_type;
  }

  if (timestamp > 0) {
    dummy_rtp_header_->header.timestamp = timestamp;
  }

  // Store the payload Type. this will be used to retrieve "received codec"
  // and "received frequency."
  last_recv_audio_codec_pltype_ = payload_type;

  last_receive_timestamp_ += recv_pl_frame_size_smpls_;
  // Insert in NetEQ.
  if (neteq_.RecIn(incoming_payload, payload_length, *dummy_rtp_header_,
                   last_receive_timestamp_) < 0) {
    return -1;
  }

  // Get ready for the next payload.
  dummy_rtp_header_->header.sequenceNumber++;
  dummy_rtp_header_->header.timestamp += recv_pl_frame_size_smpls_;
  return 0;
}

int16_t AudioCodingModuleImpl::DecoderParamByPlType(
    const uint8_t payload_type,
    WebRtcACMCodecParams& codec_params) const {
  CriticalSectionScoped lock(acm_crit_sect_);
  for (int16_t id = 0; id < ACMCodecDB::kMaxNumCodecs;
      id++) {
    if (codecs_[id] != NULL) {
      if (codecs_[id]->DecoderInitialized()) {
        if (codecs_[id]->DecoderParams(&codec_params, payload_type)) {
          return 0;
        }
      }
    }
  }
  // If we are here it means that we could not find a
  // codec with that payload type. reset the values to
  // not acceptable values and return -1.
  codec_params.codec_inst.plname[0] = '\0';
  codec_params.codec_inst.pacsize = 0;
  codec_params.codec_inst.rate = 0;
  codec_params.codec_inst.pltype = -1;
  return -1;
}

int16_t AudioCodingModuleImpl::DecoderListIDByPlName(
    const char* name, const uint16_t frequency) const {
  WebRtcACMCodecParams codec_params;
  CriticalSectionScoped lock(acm_crit_sect_);
  for (int16_t id = 0; id < ACMCodecDB::kMaxNumCodecs; id++) {
    if ((codecs_[id] != NULL)) {
      if (codecs_[id]->DecoderInitialized()) {
        assert(registered_pltypes_[id] >= 0);
        assert(registered_pltypes_[id] <= 255);
        codecs_[id]->DecoderParams(
            &codec_params, (uint8_t) registered_pltypes_[id]);
        if (!STR_CASE_CMP(codec_params.codec_inst.plname, name)) {
          // Check if the given sampling frequency matches.
          // A zero sampling frequency means we matching the names
          // is sufficient and we don't need to check for the
          // frequencies.
          // Currently it is only iSAC which has one name but two
          // sampling frequencies.
          if ((frequency == 0)||
              (codec_params.codec_inst.plfreq == frequency)) {
            return id;
          }
        }
      }
    }
  }
  // If we are here it means that we could not find a
  // codec with that payload type. return -1.
  return -1;
}

int32_t AudioCodingModuleImpl::LastEncodedTimestamp(
    uint32_t& timestamp) const {
  CriticalSectionScoped lock(acm_crit_sect_);
  if (!HaveValidEncoder("LastEncodedTimestamp")) {
    return -1;
  }
  timestamp = codecs_[current_send_codec_idx_]->LastEncodedTimestamp();
  return 0;
}

int32_t AudioCodingModuleImpl::ReplaceInternalDTXWithWebRtc(
    bool use_webrtc_dtx) {
  CriticalSectionScoped lock(acm_crit_sect_);

  if (!HaveValidEncoder("ReplaceInternalDTXWithWebRtc")) {
    WEBRTC_TRACE(
        webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
        "Cannot replace codec internal DTX when no send codec is registered.");
    return -1;
  }

  int32_t res = codecs_[current_send_codec_idx_]->ReplaceInternalDTX(
      use_webrtc_dtx);
  // Check if VAD is turned on, or if there is any error.
  if (res == 1) {
    vad_enabled_ = true;
  } else if (res < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "Failed to set ReplaceInternalDTXWithWebRtc(%d)",
                 use_webrtc_dtx);
    return res;
  }

  return 0;
}

int32_t AudioCodingModuleImpl::IsInternalDTXReplacedWithWebRtc(
    bool* uses_webrtc_dtx) {
  CriticalSectionScoped lock(acm_crit_sect_);

  if (!HaveValidEncoder("IsInternalDTXReplacedWithWebRtc")) {
    return -1;
  }
  if (codecs_[current_send_codec_idx_]->IsInternalDTXReplaced(uses_webrtc_dtx)
      < 0) {
    return -1;
  }
  return 0;
}

int32_t AudioCodingModuleImpl::SetISACMaxRate(
    const uint32_t max_bit_per_sec) {
  CriticalSectionScoped lock(acm_crit_sect_);

  if (!HaveValidEncoder("SetISACMaxRate")) {
    return -1;
  }

  return codecs_[current_send_codec_idx_]->SetISACMaxRate(max_bit_per_sec);
}

int32_t AudioCodingModuleImpl::SetISACMaxPayloadSize(
    const uint16_t max_size_bytes) {
  CriticalSectionScoped lock(acm_crit_sect_);

  if (!HaveValidEncoder("SetISACMaxPayloadSize")) {
    return -1;
  }

  return codecs_[current_send_codec_idx_]->SetISACMaxPayloadSize(
      max_size_bytes);
}

int32_t AudioCodingModuleImpl::ConfigISACBandwidthEstimator(
    const uint8_t frame_size_ms,
    const uint16_t rate_bit_per_sec,
    const bool enforce_frame_size) {
  CriticalSectionScoped lock(acm_crit_sect_);

  if (!HaveValidEncoder("ConfigISACBandwidthEstimator")) {
    return -1;
  }

  return codecs_[current_send_codec_idx_]->ConfigISACBandwidthEstimator(
      frame_size_ms, rate_bit_per_sec, enforce_frame_size);
}

int32_t AudioCodingModuleImpl::SetBackgroundNoiseMode(
    const ACMBackgroundNoiseMode mode) {
  if ((mode < On) || (mode > Off)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "The specified background noise is out of range.\n");
    return -1;
  }
  return neteq_.SetBackgroundNoiseMode(mode);
}

int32_t AudioCodingModuleImpl::BackgroundNoiseMode(
    ACMBackgroundNoiseMode* mode) {
  return neteq_.BackgroundNoiseMode(*mode);
}

int32_t AudioCodingModuleImpl::PlayoutTimestamp(
    uint32_t* timestamp) {
  WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceAudioCoding, id_,
               "PlayoutTimestamp()");
  {
    CriticalSectionScoped lock(acm_crit_sect_);
    if (track_neteq_buffer_) {
      *timestamp = playout_ts_;
      return 0;
    }
  }
  return neteq_.PlayoutTimestamp(*timestamp);
}

bool AudioCodingModuleImpl::HaveValidEncoder(const char* caller_name) const {
  if ((!send_codec_registered_) || (current_send_codec_idx_ < 0) ||
      (current_send_codec_idx_ >= ACMCodecDB::kNumCodecs)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "%s failed: No send codec is registered.", caller_name);
    return false;
  }
  if ((current_send_codec_idx_ < 0) ||
      (current_send_codec_idx_ >= ACMCodecDB::kNumCodecs)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "%s failed: Send codec index out of range.", caller_name);
    return false;
  }
  if (codecs_[current_send_codec_idx_] == NULL) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "%s failed: Send codec is NULL pointer.", caller_name);
    return false;
  }
  return true;
}

int32_t AudioCodingModuleImpl::UnregisterReceiveCodec(
    const int16_t payload_type) {
  CriticalSectionScoped lock(acm_crit_sect_);
  int id;

  // Search through the list of registered payload types.
  for (id = 0; id < ACMCodecDB::kMaxNumCodecs; id++) {
    if (registered_pltypes_[id] == payload_type) {
      // We have found the id registered with the payload type.
      break;
    }
  }

  if (id >= ACMCodecDB::kNumCodecs) {
    // Payload type was not registered. No need to unregister.
    return 0;
  }

  // Unregister the codec with the given payload type.
  return UnregisterReceiveCodecSafe(id);
}

int32_t AudioCodingModuleImpl::UnregisterReceiveCodecSafe(
    const int16_t codec_id) {
  const WebRtcNetEQDecoder *neteq_decoder = ACMCodecDB::NetEQDecoders();
  int16_t mirror_id = ACMCodecDB::MirrorID(codec_id);
  bool stereo_receiver = false;

  if (codecs_[codec_id] != NULL) {
    if (registered_pltypes_[codec_id] != -1) {
      // Store stereo information for future use.
      stereo_receiver = stereo_receive_[codec_id];

      // Before deleting the decoder instance unregister from NetEQ.
      if (neteq_.RemoveCodec(neteq_decoder[codec_id],
                              stereo_receive_[codec_id])  < 0) {
        CodecInst codec;
        ACMCodecDB::Codec(codec_id, &codec);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                     "Unregistering %s-%d from NetEQ failed.", codec.plname,
                     codec.plfreq);
        return -1;
      }

      // CN is a special case for NetEQ, all three sampling frequencies
      // are unregistered if one is deleted.
      if (IsCodecCN(codec_id)) {
        for (int i = 0; i < ACMCodecDB::kNumCodecs; i++) {
          if (IsCodecCN(i)) {
            stereo_receive_[i] = false;
            registered_pltypes_[i] = -1;
          }
        }
      } else {
        if (codec_id == mirror_id) {
          codecs_[codec_id]->DestructDecoder();
          if (stereo_receive_[codec_id]) {
            slave_codecs_[codec_id]->DestructDecoder();
            stereo_receive_[codec_id] = false;
          }
        }
      }

      // Check if this is the last registered stereo receive codec.
      if (stereo_receiver) {
        bool no_stereo = true;

        for (int i = 0; i < ACMCodecDB::kNumCodecs; i++) {
          if (stereo_receive_[i]) {
            // We still have stereo codecs registered.
            no_stereo = false;
            break;
          }
        }

        // If we don't have any stereo codecs left, change status.
        if (no_stereo) {
          neteq_.RemoveSlaves();  // No longer need the slave.
          stereo_receive_registered_ = false;
        }
      }
    }
  }

  if (registered_pltypes_[codec_id] == receive_red_pltype_) {
    // RED is going to be unregistered, set to an invalid value.
    receive_red_pltype_ = 255;
  }
  registered_pltypes_[codec_id] = -1;

  return 0;
}

int32_t AudioCodingModuleImpl::REDPayloadISAC(
    const int32_t isac_rate, const int16_t isac_bw_estimate,
    uint8_t* payload, int16_t* length_bytes) {
  if (!HaveValidEncoder("EncodeData")) {
    return -1;
  }
  int16_t status;
  status = codecs_[current_send_codec_idx_]->REDPayloadISAC(isac_rate,
                                                            isac_bw_estimate,
                                                            payload,
                                                            length_bytes);
  return status;
}

void AudioCodingModuleImpl::ResetFragmentation(int vector_size) {
  for (int n = 0; n < kMaxNumFragmentationVectors; n++) {
    fragmentation_.fragmentationOffset[n] = n * MAX_PAYLOAD_SIZE_BYTE;
  }
  memset(fragmentation_.fragmentationLength, 0, kMaxNumFragmentationVectors *
         sizeof(fragmentation_.fragmentationLength[0]));
  memset(fragmentation_.fragmentationTimeDiff, 0, kMaxNumFragmentationVectors *
         sizeof(fragmentation_.fragmentationTimeDiff[0]));
  memset(fragmentation_.fragmentationPlType, 0, kMaxNumFragmentationVectors *
         sizeof(fragmentation_.fragmentationPlType[0]));
  fragmentation_.fragmentationVectorSize =
      static_cast<uint16_t>(vector_size);
}

// TODO(turajs): Add second parameter to enable/disable AV-sync.
int AudioCodingModuleImpl::SetInitialPlayoutDelay(int delay_ms) {
  if (delay_ms < 0 || delay_ms > 10000) {
    return -1;
  }

  CriticalSectionScoped lock(acm_crit_sect_);

  // Receiver should be initialized before this call processed.
  if (!receiver_initialized_) {
    InitializeReceiverSafe();
  }

  if (first_payload_received_) {
    // Too late for this API. Only works before a call is started.
    return -1;
  }
  initial_delay_ms_ = delay_ms;

  // If initial delay is zero, NetEq buffer should not be tracked, also we
  // don't want to be in AV-sync mode.
  track_neteq_buffer_ = delay_ms > 0;
  av_sync_ = delay_ms > 0;

  neteq_.EnableAVSync(av_sync_);
  return neteq_.SetMinimumDelay(delay_ms);
}

bool AudioCodingModuleImpl::GetSilence(int desired_sample_rate_hz,
                                       AudioFrame* frame) {
  CriticalSectionScoped lock(acm_crit_sect_);
  if (initial_delay_ms_ == 0 || !track_neteq_buffer_) {
    return false;
  }

  if (accumulated_audio_ms_ >= initial_delay_ms_) {
    // We have enough data stored that match our initial delay target.
    track_neteq_buffer_ = false;
    return false;
  }

  // We stop accumulating packets, if the number of packets or the total size
  // exceeds a threshold.
  int max_num_packets;
  int buffer_size_bytes;
  int per_payload_overhead_bytes;
  neteq_.BufferSpec(max_num_packets, buffer_size_bytes,
                     per_payload_overhead_bytes);
  int total_bytes_accumulated = num_bytes_accumulated_ +
      num_packets_accumulated_ * per_payload_overhead_bytes;
  if (num_packets_accumulated_ > max_num_packets * 0.9 ||
      total_bytes_accumulated > buffer_size_bytes * 0.9) {
    WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceAudioCoding, id_,
                 "GetSilence: Initial delay couldn't be achieved."
                 " num_packets_accumulated=%d, total_bytes_accumulated=%d",
                 num_packets_accumulated_, num_bytes_accumulated_);
    track_neteq_buffer_ = false;
    return false;
  }

  if (desired_sample_rate_hz > 0) {
    frame->sample_rate_hz_ = desired_sample_rate_hz;
  } else {
    frame->sample_rate_hz_ = 0;
    if (current_receive_codec_idx_ >= 0) {
      frame->sample_rate_hz_ =
          ACMCodecDB::database_[current_receive_codec_idx_].plfreq;
    } else {
      // No payload received yet, use the default sampling rate of NetEq.
      frame->sample_rate_hz_ = neteq_.CurrentSampFreqHz();
    }
  }
  frame->num_channels_ = expected_channels_;
  frame->samples_per_channel_ = frame->sample_rate_hz_ / 100;  // Always 10 ms.
  frame->speech_type_ = AudioFrame::kCNG;
  frame->vad_activity_ = AudioFrame::kVadPassive;
  frame->energy_ = 0;
  int samples = frame->samples_per_channel_ * frame->num_channels_;
  memset(frame->data_, 0, samples * sizeof(int16_t));
  return true;
}

// Must be called within the scope of ACM critical section.
int AudioCodingModuleImpl::PushSyncPacketSafe() {
  assert(av_sync_);
  last_sequence_number_++;
  last_incoming_send_timestamp_ += last_timestamp_diff_;
  last_receive_timestamp_ += last_timestamp_diff_;

  WebRtcRTPHeader rtp_info;
  rtp_info.header.payloadType = last_recv_audio_codec_pltype_;
  rtp_info.header.ssrc = last_ssrc_;
  rtp_info.header.markerBit = false;
  rtp_info.header.sequenceNumber = last_sequence_number_;
  rtp_info.header.timestamp = last_incoming_send_timestamp_;
  rtp_info.type.Audio.channel = stereo_receive_[current_receive_codec_idx_] ?
      2 : 1;
  last_packet_was_sync_ = true;
  int payload_len_bytes = neteq_.RecIn(rtp_info, last_receive_timestamp_);

  if (payload_len_bytes < 0)
    return -1;

  // This is to account for sync packets inserted during the buffering phase.
  if (track_neteq_buffer_)
    UpdateBufferingSafe(rtp_info, payload_len_bytes);

  return 0;
}

// Must be called within the scope of ACM critical section.
void AudioCodingModuleImpl::UpdateBufferingSafe(const WebRtcRTPHeader& rtp_info,
                                                int payload_len_bytes) {
  const int in_sample_rate_khz =
      (ACMCodecDB::database_[current_receive_codec_idx_].plfreq / 1000);
  if (first_payload_received_ &&
      rtp_info.header.timestamp > last_incoming_send_timestamp_) {
      accumulated_audio_ms_ += (rtp_info.header.timestamp -
          last_incoming_send_timestamp_) / in_sample_rate_khz;
  }

  num_packets_accumulated_++;
  num_bytes_accumulated_ += payload_len_bytes;

  playout_ts_ = static_cast<uint32_t>(
      rtp_info.header.timestamp - static_cast<uint32_t>(
          initial_delay_ms_ * in_sample_rate_khz));
}

uint32_t AudioCodingModuleImpl::NowTimestamp(int codec_id) {
  // Down-cast the time to (32-6)-bit since we only care about
  // the least significant bits. (32-6) bits cover 2^(32-6) = 67108864 ms.
  // we masked 6 most significant bits of 32-bit so we don't lose resolution
  // when do the following multiplication.
  int sample_rate_khz = ACMCodecDB::database_[codec_id].plfreq / 1000;
  const uint32_t now_in_ms = static_cast<uint32_t>(
      clock_->TimeInMilliseconds() & kMaskTimestamp);
  return static_cast<uint32_t>(sample_rate_khz * now_in_ms);
}

std::vector<uint16_t> AudioCodingModuleImpl::GetNackList(
    int round_trip_time_ms) const {
  CriticalSectionScoped lock(acm_crit_sect_);
  if (round_trip_time_ms < 0) {
    WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceAudioCoding, id_,
                 "GetNackList: round trip time cannot be negative."
                 " round_trip_time_ms=%d", round_trip_time_ms);
  }
  if (nack_enabled_ && round_trip_time_ms >= 0) {
    assert(nack_.get());
    return nack_->GetNackList(round_trip_time_ms);
  }
  std::vector<uint16_t> empty_list;
  return empty_list;
}

int AudioCodingModuleImpl::LeastRequiredDelayMs() const {
  return std::max(neteq_.LeastRequiredDelayMs(), initial_delay_ms_);
}

int AudioCodingModuleImpl::EnableNack(size_t max_nack_list_size) {
  // Don't do anything if |max_nack_list_size| is out of range.
  if (max_nack_list_size == 0 || max_nack_list_size > Nack::kNackListSizeLimit)
    return -1;

  CriticalSectionScoped lock(acm_crit_sect_);
  if (!nack_enabled_) {
    nack_.reset(Nack::Create(kNackThresholdPackets));
    nack_enabled_ = true;

    // Sampling rate might need to be updated if we change from disable to
    // enable. Do it if the receive codec is valid.
    if (current_receive_codec_idx_ >= 0) {
      nack_->UpdateSampleRate(
          ACMCodecDB::database_[current_receive_codec_idx_].plfreq);
    }
  }
  return nack_->SetMaxNackListSize(max_nack_list_size);
}

void AudioCodingModuleImpl::DisableNack() {
  CriticalSectionScoped lock(acm_crit_sect_);
  nack_.reset();  // Memory is released.
  nack_enabled_ = false;
}

}  // namespace webrtc
