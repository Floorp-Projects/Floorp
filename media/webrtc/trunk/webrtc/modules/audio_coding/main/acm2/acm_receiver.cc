/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/main/acm2/acm_receiver.h"

#include <stdlib.h>  // malloc

#include <algorithm>  // sort
#include <vector>

#include "webrtc/common_audio/signal_processing/include/signal_processing_library.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/audio_coding/main/acm2/acm_common_defs.h"
#include "webrtc/modules/audio_coding/main/acm2/acm_resampler.h"
#include "webrtc/modules/audio_coding/main/acm2/call_statistics.h"
#include "webrtc/modules/audio_coding/main/acm2/nack.h"
#include "webrtc/modules/audio_coding/neteq4/interface/audio_decoder.h"
#include "webrtc/modules/audio_coding/neteq4/interface/neteq.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/system_wrappers/interface/rw_lock_wrapper.h"
#include "webrtc/system_wrappers/interface/tick_util.h"
#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc {

namespace acm2 {

namespace {

const int kNeteqInitSampleRateHz = 16000;
const int kNackThresholdPackets = 2;

// |vad_activity_| field of |audio_frame| is set to |previous_audio_activity_|
// before the call to this function.
void SetAudioFrameActivityAndType(bool vad_enabled,
                                  NetEqOutputType type,
                                  AudioFrame* audio_frame) {
  if (vad_enabled) {
    switch (type) {
      case kOutputNormal: {
        audio_frame->vad_activity_ = AudioFrame::kVadActive;
        audio_frame->speech_type_ = AudioFrame::kNormalSpeech;
        break;
      }
      case kOutputVADPassive: {
        audio_frame->vad_activity_ = AudioFrame::kVadPassive;
        audio_frame->speech_type_ = AudioFrame::kNormalSpeech;
        break;
      }
      case kOutputCNG: {
        audio_frame->vad_activity_ = AudioFrame::kVadPassive;
        audio_frame->speech_type_ = AudioFrame::kCNG;
        break;
      }
      case kOutputPLC: {
        // Don't change |audio_frame->vad_activity_|, it should be the same as
        // |previous_audio_activity_|.
        audio_frame->speech_type_ = AudioFrame::kPLC;
        break;
      }
      case kOutputPLCtoCNG: {
        audio_frame->vad_activity_ = AudioFrame::kVadPassive;
        audio_frame->speech_type_ = AudioFrame::kPLCCNG;
        break;
      }
      default:
        assert(false);
    }
  } else {
    // Always return kVadUnknown when receive VAD is inactive
    audio_frame->vad_activity_ = AudioFrame::kVadUnknown;
    switch (type) {
      case kOutputNormal: {
        audio_frame->speech_type_ = AudioFrame::kNormalSpeech;
        break;
      }
      case kOutputCNG: {
        audio_frame->speech_type_ = AudioFrame::kCNG;
        break;
      }
      case kOutputPLC: {
        audio_frame->speech_type_ = AudioFrame::kPLC;
        break;
      }
      case kOutputPLCtoCNG: {
        audio_frame->speech_type_ = AudioFrame::kPLCCNG;
        break;
      }
      case kOutputVADPassive: {
        // Normally, we should no get any VAD decision if post-decoding VAD is
        // not active. However, if post-decoding VAD has been active then
        // disabled, we might be here for couple of frames.
        audio_frame->speech_type_ = AudioFrame::kNormalSpeech;
        LOG_F(LS_WARNING) << "Post-decoding VAD is disabled but output is "
            << "labeled VAD-passive";
        break;
      }
      default:
        assert(false);
    }
  }
}

// Is the given codec a CNG codec?
bool IsCng(int codec_id) {
  return (codec_id == ACMCodecDB::kCNNB || codec_id == ACMCodecDB::kCNWB ||
      codec_id == ACMCodecDB::kCNSWB || codec_id == ACMCodecDB::kCNFB);
}

}  // namespace

AcmReceiver::AcmReceiver()
    : id_(0),
      neteq_(NetEq::Create(kNeteqInitSampleRateHz)),
      last_audio_decoder_(-1),  // Invalid value.
      decode_lock_(RWLockWrapper::CreateRWLock()),
      neteq_crit_sect_(CriticalSectionWrapper::CreateCriticalSection()),
      vad_enabled_(true),
      previous_audio_activity_(AudioFrame::kVadUnknown),
      current_sample_rate_hz_(kNeteqInitSampleRateHz),
      nack_(),
      nack_enabled_(false),
      av_sync_(false),
      initial_delay_manager_(),
      missing_packets_sync_stream_(),
      late_packets_sync_stream_() {
  for (int n = 0; n < ACMCodecDB::kMaxNumCodecs; ++n) {
    decoders_[n].registered = false;
  }

  // Make sure we are on the same page as NetEq. Post-decode VAD is disabled by
  // default in NetEq4, however, Audio Conference Mixer relies on VAD decision
  // and fails if VAD decision is not provided.
  if (vad_enabled_)
    neteq_->EnableVad();
  else
    neteq_->DisableVad();
}

AcmReceiver::~AcmReceiver() {
  delete neteq_;
  delete decode_lock_;
  delete neteq_crit_sect_;
}

int AcmReceiver::SetMinimumDelay(int delay_ms) {
  if (neteq_->SetMinimumDelay(delay_ms))
    return 0;
  LOG_FERR1(LS_ERROR, "AcmReceiver::SetExtraDelay", delay_ms);
  return -1;
}

int AcmReceiver::SetInitialDelay(int delay_ms) {
  if (delay_ms < 0 || delay_ms > 10000) {
    return -1;
  }
  CriticalSectionScoped lock(neteq_crit_sect_);

  if (delay_ms == 0) {
    av_sync_ = false;
    initial_delay_manager_.reset();
    missing_packets_sync_stream_.reset();
    late_packets_sync_stream_.reset();
    neteq_->SetMinimumDelay(0);
    return 0;
  }

  if (av_sync_ && initial_delay_manager_->PacketBuffered()) {
    // Too late for this API. Only works before a call is started.
    return -1;
  }

  // Most of places NetEq calls are not within AcmReceiver's critical section to
  // improve performance. Here, this call has to be placed before the following
  // block, therefore, we keep it inside critical section. Otherwise, we have to
  // release |neteq_crit_sect_| and acquire it again, which seems an overkill.
  if (!neteq_->SetMinimumDelay(delay_ms))
    return -1;

  const int kLatePacketThreshold = 5;
  av_sync_ = true;
  initial_delay_manager_.reset(new InitialDelayManager(delay_ms,
                                                       kLatePacketThreshold));
  missing_packets_sync_stream_.reset(new InitialDelayManager::SyncStream);
  late_packets_sync_stream_.reset(new InitialDelayManager::SyncStream);
  return 0;
}

int AcmReceiver::SetMaximumDelay(int delay_ms) {
  if (neteq_->SetMaximumDelay(delay_ms))
    return 0;
  LOG_FERR1(LS_ERROR, "AcmReceiver::SetExtraDelay", delay_ms);
  return -1;
}

int AcmReceiver::LeastRequiredDelayMs() const {
  return neteq_->LeastRequiredDelayMs();
}

int AcmReceiver::current_sample_rate_hz() const {
  CriticalSectionScoped lock(neteq_crit_sect_);
  return current_sample_rate_hz_;
}

// TODO(turajs): use one set of enumerators, e.g. the one defined in
// common_types.h
void AcmReceiver::SetPlayoutMode(AudioPlayoutMode mode) {
  enum NetEqPlayoutMode playout_mode = kPlayoutOn;
  enum NetEqBackgroundNoiseMode bgn_mode = kBgnOn;
  switch (mode) {
    case voice:
      playout_mode = kPlayoutOn;
      bgn_mode = kBgnOn;
      break;
    case fax:  // No change to background noise mode.
      playout_mode = kPlayoutFax;
      bgn_mode = neteq_->BackgroundNoiseMode();
      break;
    case streaming:
      playout_mode = kPlayoutStreaming;
      bgn_mode = kBgnOff;
      break;
    case off:
      playout_mode = kPlayoutOff;
      bgn_mode = kBgnOff;
      break;
  }
  neteq_->SetPlayoutMode(playout_mode);
  neteq_->SetBackgroundNoiseMode(bgn_mode);
}

AudioPlayoutMode AcmReceiver::PlayoutMode() const {
  AudioPlayoutMode acm_mode = voice;
  NetEqPlayoutMode mode = neteq_->PlayoutMode();
  switch (mode) {
    case kPlayoutOn:
      acm_mode = voice;
      break;
    case kPlayoutOff:
      acm_mode = off;
      break;
    case kPlayoutFax:
      acm_mode = fax;
      break;
    case kPlayoutStreaming:
      acm_mode = streaming;
      break;
    default:
      assert(false);
  }
  return acm_mode;
}

int AcmReceiver::InsertPacket(const WebRtcRTPHeader& rtp_header,
                              const uint8_t* incoming_payload,
                              int length_payload) {
  uint32_t receive_timestamp = 0;
  InitialDelayManager::PacketType packet_type =
      InitialDelayManager::kUndefinedPacket;
  bool new_codec = false;
  const RTPHeader* header = &rtp_header.header;  // Just a shorthand.

  {
    CriticalSectionScoped lock(neteq_crit_sect_);

    int codec_id = RtpHeaderToCodecIndex(*header, incoming_payload);
    if (codec_id < 0) {
      LOG_F(LS_ERROR) << "Payload-type " << header->payloadType
          << " is not registered.";
      return -1;
    }
    assert(codec_id < ACMCodecDB::kMaxNumCodecs);
    const int sample_rate_hz = ACMCodecDB::CodecFreq(codec_id);
    receive_timestamp = NowInTimestamp(sample_rate_hz);

    if (IsCng(codec_id)) {
      // If this is a CNG while the audio codec is not mono skip pushing in
      // packets into NetEq.
      if (last_audio_decoder_ >= 0 &&
        decoders_[last_audio_decoder_].channels > 1)
        return 0;
      packet_type = InitialDelayManager::kCngPacket;
    } else if (codec_id == ACMCodecDB::kAVT) {
      packet_type = InitialDelayManager::kAvtPacket;
    } else {
      if (codec_id != last_audio_decoder_) {
        // This is either the first audio packet or send codec is changed.
        // Therefore, either NetEq buffer is empty or will be flushed when this
        // packet inserted. Note that |last_audio_decoder_| is initialized to
        // an invalid value (-1), hence, the above condition is true for the
        // very first audio packet.
        new_codec = true;

        // Updating NACK'sampling rate is required, either first packet is
        // received or codec is changed. Furthermore, reset is required if codec
        // is changed (NetEq flushes its buffer so NACK should reset its list).
        if (nack_enabled_) {
          assert(nack_.get());
          nack_->Reset();
          nack_->UpdateSampleRate(sample_rate_hz);
        }
        last_audio_decoder_ = codec_id;
      }
      packet_type = InitialDelayManager::kAudioPacket;
    }

    if (nack_enabled_) {
      assert(nack_.get());
      nack_->UpdateLastReceivedPacket(header->sequenceNumber,
                                      header->timestamp);
    }

    if (av_sync_) {
      assert(initial_delay_manager_.get());
      assert(missing_packets_sync_stream_.get());
      // This updates |initial_delay_manager_| and specifies an stream of
      // sync-packets, if required to be inserted. We insert the sync-packets
      // when AcmReceiver lock is released and |decoder_lock_| is acquired.
      initial_delay_manager_->UpdateLastReceivedPacket(
          rtp_header, receive_timestamp, packet_type, new_codec, sample_rate_hz,
          missing_packets_sync_stream_.get());
    }
  }

  {
    WriteLockScoped lock_codecs(*decode_lock_);  // Lock to prevent an encoding.

    // If |missing_packets_sync_stream_| is allocated then we are in AV-sync and
    // we may need to insert sync-packets. We don't check |av_sync_| as we are
    // outside AcmReceiver's critical section.
    if (missing_packets_sync_stream_.get()) {
      InsertStreamOfSyncPackets(missing_packets_sync_stream_.get());
    }

    if (neteq_->InsertPacket(rtp_header, incoming_payload, length_payload,
                             receive_timestamp) < 0) {
      LOG_FERR1(LS_ERROR, "AcmReceiver::InsertPacket", header->payloadType) <<
          " Failed to insert packet";
      return -1;
    }
  }
  return 0;
}

int AcmReceiver::GetAudio(int desired_freq_hz, AudioFrame* audio_frame) {
  enum NetEqOutputType type;
  int16_t* ptr_audio_buffer = audio_frame->data_;
  int samples_per_channel;
  int num_channels;
  bool return_silence = false;

  {
    // Accessing members, take the lock.
    CriticalSectionScoped lock(neteq_crit_sect_);

    if (av_sync_) {
      assert(initial_delay_manager_.get());
      assert(late_packets_sync_stream_.get());
      return_silence = GetSilence(desired_freq_hz, audio_frame);
      uint32_t timestamp_now = NowInTimestamp(current_sample_rate_hz_);
      initial_delay_manager_->LatePackets(timestamp_now,
                                          late_packets_sync_stream_.get());
    }

    if (!return_silence) {
      // This is our initial guess regarding whether a resampling will be
      // required. It is based on previous sample rate of netEq. Most often,
      // this is a correct guess, however, in case that incoming payload changes
      // the resampling might might be needed. By doing so, we avoid an
      // unnecessary memcpy().
      if (desired_freq_hz != -1 &&
          current_sample_rate_hz_ != desired_freq_hz) {
        ptr_audio_buffer = audio_buffer_;
      }
    }
  }

  {
    WriteLockScoped lock_codecs(*decode_lock_);  // Lock to prevent an encoding.

    // If |late_packets_sync_stream_| is allocated then we have been in AV-sync
    // mode and we might have to insert sync-packets.
    if (late_packets_sync_stream_.get()) {
      InsertStreamOfSyncPackets(late_packets_sync_stream_.get());
      if (return_silence)  // Silence generated, don't pull from NetEq.
        return 0;
    }

    if (neteq_->GetAudio(AudioFrame::kMaxDataSizeSamples,
                         ptr_audio_buffer,
                         &samples_per_channel,
                         &num_channels, &type) != NetEq::kOK) {
      LOG_FERR0(LS_ERROR, "AcmReceiver::GetAudio") << "NetEq Failed.";
      return -1;
    }
  }

  // Accessing members, take the lock.
  CriticalSectionScoped lock(neteq_crit_sect_);

  // Update NACK.
  int decoded_sequence_num = 0;
  uint32_t decoded_timestamp = 0;
  bool update_nack = nack_enabled_ &&  // Update NACK only if it is enabled.
      neteq_->DecodedRtpInfo(&decoded_sequence_num, &decoded_timestamp);
  if (update_nack) {
    assert(nack_.get());
    nack_->UpdateLastDecodedPacket(decoded_sequence_num, decoded_timestamp);
  }

  // NetEq always returns 10 ms of audio.
  current_sample_rate_hz_ = samples_per_channel * 100;

  // Update if resampling is required.
  bool need_resampling = (desired_freq_hz != -1) &&
      (current_sample_rate_hz_ != desired_freq_hz);

  if (ptr_audio_buffer == audio_buffer_) {
    // Data is written to local buffer.
    if (need_resampling) {
      samples_per_channel = resampler_.Resample10Msec(
          audio_buffer_, current_sample_rate_hz_, desired_freq_hz,
          num_channels, audio_frame->data_);
      if (samples_per_channel < 0) {
        LOG_FERR0(LS_ERROR, "AcmReceiver::GetAudio") << "Resampler Failed.";
        return -1;
      }
    } else {
      // We might end up here ONLY if codec is changed.
      memcpy(audio_frame->data_, audio_buffer_, samples_per_channel *
             num_channels * sizeof(int16_t));
    }
  } else {
    // Data is written into |audio_frame|.
    if (need_resampling) {
      // We might end up here ONLY if codec is changed.
      samples_per_channel = resampler_.Resample10Msec(
          audio_frame->data_, current_sample_rate_hz_, desired_freq_hz,
          num_channels, audio_buffer_);
      if (samples_per_channel < 0) {
        LOG_FERR0(LS_ERROR, "AcmReceiver::GetAudio") << "Resampler Failed.";
        return -1;
      }
      memcpy(audio_frame->data_, audio_buffer_, samples_per_channel *
             num_channels * sizeof(int16_t));
    }
  }

  audio_frame->num_channels_ = num_channels;
  audio_frame->samples_per_channel_ = samples_per_channel;
  audio_frame->sample_rate_hz_ = samples_per_channel * 100;

  // Should set |vad_activity| before calling SetAudioFrameActivityAndType().
  audio_frame->vad_activity_ = previous_audio_activity_;
  SetAudioFrameActivityAndType(vad_enabled_, type, audio_frame);
  previous_audio_activity_ = audio_frame->vad_activity_;
  call_stats_.DecodedByNetEq(audio_frame->speech_type_);
  return 0;
}

int32_t AcmReceiver::AddCodec(int acm_codec_id,
                              uint8_t payload_type,
                              int channels,
                              AudioDecoder* audio_decoder) {
  assert(acm_codec_id >= 0 && acm_codec_id < ACMCodecDB::kMaxNumCodecs);
  NetEqDecoder neteq_decoder = ACMCodecDB::neteq_decoders_[acm_codec_id];

  CriticalSectionScoped lock(neteq_crit_sect_);

  // The corresponding NetEq decoder ID.
  // If this coder has been registered before.
  if (decoders_[acm_codec_id].registered) {
    if (decoders_[acm_codec_id].payload_type == payload_type) {
      // Re-registering the same codec with the same payload-type. Do nothing
      // and return.
      return 0;
    }

    // Changing the payload-type of this codec. First unregister. Then register
    // with new payload-type.
    if (neteq_->RemovePayloadType(decoders_[acm_codec_id].payload_type) !=
        NetEq::kOK) {
      LOG_F(LS_ERROR) << "Cannot remover payload "
          << decoders_[acm_codec_id].payload_type;
      return -1;
    }
  }

  int ret_val;
  if (!audio_decoder) {
    ret_val = neteq_->RegisterPayloadType(neteq_decoder, payload_type);
  } else {
    ret_val = neteq_->RegisterExternalDecoder(
        audio_decoder, neteq_decoder,
        ACMCodecDB::database_[acm_codec_id].plfreq, payload_type);
  }
  if (ret_val != NetEq::kOK) {
    LOG_FERR3(LS_ERROR, "AcmReceiver::AddCodec", acm_codec_id, payload_type,
              channels);
    // Registration failed, delete the allocated space and set the pointer to
    // NULL, for the record.
    decoders_[acm_codec_id].registered = false;
    return -1;
  }

  decoders_[acm_codec_id].registered = true;
  decoders_[acm_codec_id].payload_type = payload_type;
  decoders_[acm_codec_id].channels = channels;
  return 0;
}

void AcmReceiver::EnableVad() {
  neteq_->EnableVad();
  CriticalSectionScoped lock(neteq_crit_sect_);
  vad_enabled_ = true;
}

void AcmReceiver::DisableVad() {
  neteq_->DisableVad();
  CriticalSectionScoped lock(neteq_crit_sect_);
  vad_enabled_ = false;
}

void AcmReceiver::FlushBuffers() {
  neteq_->FlushBuffers();
}

// If failed in removing one of the codecs, this method continues to remove as
// many as it can.
int AcmReceiver::RemoveAllCodecs() {
  int ret_val = 0;
  CriticalSectionScoped lock(neteq_crit_sect_);
  for (int n = 0; n < ACMCodecDB::kMaxNumCodecs; ++n) {
    if (decoders_[n].registered) {
      if (neteq_->RemovePayloadType(decoders_[n].payload_type) == 0) {
        decoders_[n].registered = false;
      } else {
        LOG_F(LS_ERROR) << "Cannot remove payload "
            << decoders_[n].payload_type;
        ret_val = -1;
      }
    }
  }
  // No codec is registered, invalidate last audio decoder.
  last_audio_decoder_ = -1;
  return ret_val;
}

int AcmReceiver::RemoveCodec(uint8_t payload_type) {
  int codec_index = PayloadType2CodecIndex(payload_type);
  if (codec_index < 0) {  // Such a payload-type is not registered.
    LOG(LS_WARNING) << "payload_type " << payload_type << " is not registered,"
        " no action is taken.";
    return 0;
  }
  if (neteq_->RemovePayloadType(payload_type) != NetEq::kOK) {
    LOG_FERR1(LS_ERROR, "AcmReceiver::RemoveCodec", payload_type);
    return -1;
  }
  CriticalSectionScoped lock(neteq_crit_sect_);
  decoders_[codec_index].registered = false;
  if (last_audio_decoder_ == codec_index)
    last_audio_decoder_ = -1;  // Codec is removed, invalidate last decoder.
  return 0;
}

void AcmReceiver::set_id(int id) {
  CriticalSectionScoped lock(neteq_crit_sect_);
  id_ = id;
}

uint32_t AcmReceiver::PlayoutTimestamp() {
  if (av_sync_) {
    assert(initial_delay_manager_.get());
    if (initial_delay_manager_->buffering())
      return initial_delay_manager_->playout_timestamp();
  }
  return neteq_->PlayoutTimestamp();
}

int AcmReceiver::last_audio_codec_id() const {
  CriticalSectionScoped lock(neteq_crit_sect_);
  return last_audio_decoder_;
}

int AcmReceiver::last_audio_payload_type() const {
  CriticalSectionScoped lock(neteq_crit_sect_);
  if (last_audio_decoder_ < 0)
    return -1;
  assert(decoders_[last_audio_decoder_].registered);
  return decoders_[last_audio_decoder_].payload_type;
}

int AcmReceiver::RedPayloadType() const {
  CriticalSectionScoped lock(neteq_crit_sect_);
  if (ACMCodecDB::kRED < 0 ||
      !decoders_[ACMCodecDB::kRED].registered) {
    LOG_F(LS_WARNING) << "RED is not registered.";
    return -1;
  }
  return decoders_[ACMCodecDB::kRED].payload_type;
}

int AcmReceiver::LastAudioCodec(CodecInst* codec) const {
  CriticalSectionScoped lock(neteq_crit_sect_);
  if (last_audio_decoder_ < 0) {
    LOG_F(LS_WARNING) << "No audio payload is received, yet.";
    return -1;
  }
  assert(decoders_[last_audio_decoder_].registered);
  memcpy(codec, &ACMCodecDB::database_[last_audio_decoder_], sizeof(CodecInst));
  codec->pltype = decoders_[last_audio_decoder_].payload_type;
  codec->channels = decoders_[last_audio_decoder_].channels;
  return 0;
}

void AcmReceiver::NetworkStatistics(ACMNetworkStatistics* acm_stat) {
  NetEqNetworkStatistics neteq_stat;
  // NetEq function always returns zero, so we don't check the return value.
  neteq_->NetworkStatistics(&neteq_stat);

  acm_stat->currentBufferSize = neteq_stat.current_buffer_size_ms;
  acm_stat->preferredBufferSize = neteq_stat.preferred_buffer_size_ms;
  acm_stat->jitterPeaksFound = neteq_stat.jitter_peaks_found ? true : false;
  acm_stat->currentPacketLossRate = neteq_stat.packet_loss_rate;
  acm_stat->currentDiscardRate = neteq_stat.packet_discard_rate;
  acm_stat->currentExpandRate = neteq_stat.expand_rate;
  acm_stat->currentPreemptiveRate = neteq_stat.preemptive_rate;
  acm_stat->currentAccelerateRate = neteq_stat.accelerate_rate;
  acm_stat->clockDriftPPM = neteq_stat.clockdrift_ppm;

  std::vector<int> waiting_times;
  neteq_->WaitingTimes(&waiting_times);
  size_t size = waiting_times.size();
  if (size == 0) {
    acm_stat->meanWaitingTimeMs = -1;
    acm_stat->medianWaitingTimeMs = -1;
    acm_stat->minWaitingTimeMs = -1;
    acm_stat->maxWaitingTimeMs = -1;
  } else {
    std::sort(waiting_times.begin(), waiting_times.end());
    if ((size & 0x1) == 0) {
      acm_stat->medianWaitingTimeMs = (waiting_times[size / 2 - 1] +
          waiting_times[size / 2]) / 2;
    } else {
      acm_stat->medianWaitingTimeMs = waiting_times[size / 2];
    }
    acm_stat->minWaitingTimeMs = waiting_times.front();
    acm_stat->maxWaitingTimeMs = waiting_times.back();
    double sum = 0;
    for (size_t i = 0; i < size; ++i) {
      sum += waiting_times[i];
    }
    acm_stat->meanWaitingTimeMs = static_cast<int>(sum / size);
  }
}

int AcmReceiver::DecoderByPayloadType(uint8_t payload_type,
                                      CodecInst* codec) const {
  CriticalSectionScoped lock(neteq_crit_sect_);
  int codec_index = PayloadType2CodecIndex(payload_type);
  if (codec_index < 0) {
    LOG_FERR1(LS_ERROR, "AcmReceiver::DecoderByPayloadType", payload_type);
    return -1;
  }
  memcpy(codec, &ACMCodecDB::database_[codec_index], sizeof(CodecInst));
  codec->pltype = decoders_[codec_index].payload_type;
  codec->channels = decoders_[codec_index].channels;
  return 0;
}

int AcmReceiver::PayloadType2CodecIndex(uint8_t payload_type) const {
  for (int n = 0; n < ACMCodecDB::kMaxNumCodecs; ++n) {
    if (decoders_[n].registered && decoders_[n].payload_type == payload_type) {
      return n;
    }
  }
  return -1;
}

int AcmReceiver::EnableNack(size_t max_nack_list_size) {
  // Don't do anything if |max_nack_list_size| is out of range.
  if (max_nack_list_size == 0 || max_nack_list_size > Nack::kNackListSizeLimit)
    return -1;

  CriticalSectionScoped lock(neteq_crit_sect_);
  if (!nack_enabled_) {
    nack_.reset(Nack::Create(kNackThresholdPackets));
    nack_enabled_ = true;

    // Sampling rate might need to be updated if we change from disable to
    // enable. Do it if the receive codec is valid.
    if (last_audio_decoder_ >= 0) {
      nack_->UpdateSampleRate(
          ACMCodecDB::database_[last_audio_decoder_].plfreq);
    }
  }
  return nack_->SetMaxNackListSize(max_nack_list_size);
}

void AcmReceiver::DisableNack() {
  CriticalSectionScoped lock(neteq_crit_sect_);
  nack_.reset();  // Memory is released.
  nack_enabled_ = false;
}

std::vector<uint16_t> AcmReceiver::GetNackList(
    int round_trip_time_ms) const {
  CriticalSectionScoped lock(neteq_crit_sect_);
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

void AcmReceiver::ResetInitialDelay() {
  {
    CriticalSectionScoped lock(neteq_crit_sect_);
    av_sync_ = false;
    initial_delay_manager_.reset(NULL);
    missing_packets_sync_stream_.reset(NULL);
    late_packets_sync_stream_.reset(NULL);
  }
  neteq_->SetMinimumDelay(0);
  // TODO(turajs): Should NetEq Buffer be flushed?
}

// This function is called within critical section, no need to acquire a lock.
bool AcmReceiver::GetSilence(int desired_sample_rate_hz, AudioFrame* frame) {
  assert(av_sync_);
  assert(initial_delay_manager_.get());
  if (!initial_delay_manager_->buffering()) {
    return false;
  }

  // We stop accumulating packets, if the number of packets or the total size
  // exceeds a threshold.
  int num_packets;
  int max_num_packets;
  int buffer_size_byte;
  int max_buffer_size_byte;
  const float kBufferingThresholdScale = 0.9f;
  neteq_->PacketBufferStatistics(&num_packets, &max_num_packets,
                                 &buffer_size_byte, &max_buffer_size_byte);
  if (num_packets > max_num_packets * kBufferingThresholdScale ||
      buffer_size_byte > max_buffer_size_byte * kBufferingThresholdScale) {
    initial_delay_manager_->DisableBuffering();
    return false;
  }

  // Update statistics.
  call_stats_.DecodedBySilenceGenerator();

  // Set the values if already got a packet, otherwise set to default values.
  if (last_audio_decoder_ >= 0) {
    current_sample_rate_hz_ = ACMCodecDB::database_[last_audio_decoder_].plfreq;
    frame->num_channels_ = decoders_[last_audio_decoder_].channels;
  } else {
    current_sample_rate_hz_ = kNeteqInitSampleRateHz;
    frame->num_channels_ = 1;
  }

  // Set the audio frame's sampling frequency.
  if (desired_sample_rate_hz > 0) {
    frame->sample_rate_hz_ = desired_sample_rate_hz;
  } else {
    frame->sample_rate_hz_ = current_sample_rate_hz_;
  }

  frame->samples_per_channel_ = frame->sample_rate_hz_ / 100;  // Always 10 ms.
  frame->speech_type_ = AudioFrame::kCNG;
  frame->vad_activity_ = AudioFrame::kVadPassive;
  frame->energy_ = 0;
  int samples = frame->samples_per_channel_ * frame->num_channels_;
  memset(frame->data_, 0, samples * sizeof(int16_t));
  return true;
}

NetEqBackgroundNoiseMode AcmReceiver::BackgroundNoiseModeForTest() const {
  return neteq_->BackgroundNoiseMode();
}

int AcmReceiver::RtpHeaderToCodecIndex(
    const RTPHeader &rtp_header, const uint8_t* payload) const {
  uint8_t payload_type = rtp_header.payloadType;
  if (ACMCodecDB::kRED >= 0 &&  // This ensures that RED is defined in WebRTC.
      decoders_[ACMCodecDB::kRED].registered &&
      payload_type == decoders_[ACMCodecDB::kRED].payload_type) {
    // This is a RED packet, get the payload of the audio codec.
    payload_type = payload[0] & 0x7F;
  }

  // Check if the payload is registered.
  return PayloadType2CodecIndex(payload_type);
}

uint32_t AcmReceiver::NowInTimestamp(int decoder_sampling_rate) const {
  // Down-cast the time to (32-6)-bit since we only care about
  // the least significant bits. (32-6) bits cover 2^(32-6) = 67108864 ms.
  // We masked 6 most significant bits of 32-bit so there is no overflow in
  // the conversion from milliseconds to timestamp.
  const uint32_t now_in_ms = static_cast<uint32_t>(
      TickTime::MillisecondTimestamp() & 0x03ffffff);
  return static_cast<uint32_t>(
      (decoder_sampling_rate / 1000) * now_in_ms);
}

// This function only interacts with |neteq_|, therefore, it does not have to
// be within critical section of AcmReceiver. It is inserting packets
// into NetEq, so we call it when |decode_lock_| is acquired. However, this is
// not essential as sync-packets do not interact with codecs (especially BWE).
void AcmReceiver::InsertStreamOfSyncPackets(
    InitialDelayManager::SyncStream* sync_stream) {
  assert(sync_stream);
  assert(av_sync_);
  for (int n = 0; n < sync_stream->num_sync_packets; ++n) {
    neteq_->InsertSyncPacket(sync_stream->rtp_info,
                             sync_stream->receive_timestamp);
    ++sync_stream->rtp_info.header.sequenceNumber;
    sync_stream->rtp_info.header.timestamp += sync_stream->timestamp_step;
    sync_stream->receive_timestamp += sync_stream->timestamp_step;
  }
}

void AcmReceiver::GetDecodingCallStatistics(
    AudioDecodingCallStats* stats) const {
  CriticalSectionScoped lock(neteq_crit_sect_);
  *stats = call_stats_.GetDecodingStatistics();
}

}  // namespace acm2

}  // namespace webrtc
