/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/main/source/acm_neteq.h"

#include <stdlib.h>  // malloc

#include <algorithm>  // sort
#include <vector>

#include "webrtc/common_audio/signal_processing/include/signal_processing_library.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/audio_coding/neteq/interface/webrtc_neteq.h"
#include "webrtc/modules/audio_coding/neteq/interface/webrtc_neteq_internal.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/rw_lock_wrapper.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/system_wrappers/interface/trace_event.h"

namespace webrtc {

#define RTP_HEADER_SIZE 12
#define NETEQ_INIT_FREQ 8000
#define NETEQ_INIT_FREQ_KHZ (NETEQ_INIT_FREQ/1000)
#define NETEQ_ERR_MSG_LEN_BYTE (WEBRTC_NETEQ_MAX_ERROR_NAME + 1)

ACMNetEQ::ACMNetEQ()
    : id_(0),
      current_samp_freq_khz_(NETEQ_INIT_FREQ_KHZ),
      avt_playout_(false),
      playout_mode_(voice),
      neteq_crit_sect_(CriticalSectionWrapper::CreateCriticalSection()),
      vad_status_(false),
      vad_mode_(VADNormal),
      decode_lock_(RWLockWrapper::CreateRWLock()),
      num_slaves_(0),
      received_stereo_(false),
      master_slave_info_(NULL),
      previous_audio_activity_(AudioFrame::kVadUnknown),
      callback_crit_sect_(CriticalSectionWrapper::CreateCriticalSection()),
      min_of_max_num_packets_(0),
      min_of_buffer_size_bytes_(0),
      per_packet_overhead_bytes_(0),
      av_sync_(false),
      minimum_delay_ms_(0) {
  for (int n = 0; n < MAX_NUM_SLAVE_NETEQ + 1; n++) {
    is_initialized_[n] = false;
    ptr_vadinst_[n] = NULL;
    inst_[n] = NULL;
    inst_mem_[n] = NULL;
    neteq_packet_buffer_[n] = NULL;
  }
}

ACMNetEQ::~ACMNetEQ() {
  {
    CriticalSectionScoped lock(neteq_crit_sect_);
    RemoveNetEQSafe(0);  // Master.
    RemoveSlavesSafe();
  }
  if (neteq_crit_sect_ != NULL) {
    delete neteq_crit_sect_;
  }

  if (decode_lock_ != NULL) {
    delete decode_lock_;
  }

  if (callback_crit_sect_ != NULL) {
    delete callback_crit_sect_;
  }
}

int32_t ACMNetEQ::Init() {
  CriticalSectionScoped lock(neteq_crit_sect_);

  for (int16_t idx = 0; idx < num_slaves_ + 1; idx++) {
    if (InitByIdxSafe(idx) < 0) {
      return -1;
    }
    // delete VAD instance and start fresh if required.
    if (ptr_vadinst_[idx] != NULL) {
      WebRtcVad_Free(ptr_vadinst_[idx]);
      ptr_vadinst_[idx] = NULL;
    }
    if (vad_status_) {
      // Has to enable VAD
      if (EnableVADByIdxSafe(idx) < 0) {
        // Failed to enable VAD.
        // Delete VAD instance, if it is created
        if (ptr_vadinst_[idx] != NULL) {
          WebRtcVad_Free(ptr_vadinst_[idx]);
          ptr_vadinst_[idx] = NULL;
        }
        // We are at initialization of NetEq, if failed to
        // enable VAD, we delete the NetEq instance.
        if (inst_mem_[idx] != NULL) {
          free(inst_mem_[idx]);
          inst_mem_[idx] = NULL;
          inst_[idx] = NULL;
        }
        is_initialized_[idx] = false;
        return -1;
      }
    }
    is_initialized_[idx] = true;
  }
  if (EnableVAD() == -1) {
    return -1;
  }
  return 0;
}

int16_t ACMNetEQ::InitByIdxSafe(const int16_t idx) {
  int memory_size_bytes;
  if (WebRtcNetEQ_AssignSize(&memory_size_bytes) != 0) {
    LogError("AssignSize", idx);
    return -1;
  }

  if (inst_mem_[idx] != NULL) {
    free(inst_mem_[idx]);
    inst_mem_[idx] = NULL;
  }
  inst_mem_[idx] = malloc(memory_size_bytes);
  if (inst_mem_[idx] == NULL) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "InitByIdxSafe: NetEq Initialization error: could not "
                 "allocate memory for NetEq");
    is_initialized_[idx] = false;
    return -1;
  }
  if (WebRtcNetEQ_Assign(&inst_[idx], inst_mem_[idx]) != 0) {
    if (inst_mem_[idx] != NULL) {
      free(inst_mem_[idx]);
      inst_mem_[idx] = NULL;
    }
    LogError("Assign", idx);
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "InitByIdxSafe: NetEq Initialization error: could not Assign");
    is_initialized_[idx] = false;
    return -1;
  }
  if (WebRtcNetEQ_Init(inst_[idx], NETEQ_INIT_FREQ) != 0) {
    if (inst_mem_[idx] != NULL) {
      free(inst_mem_[idx]);
      inst_mem_[idx] = NULL;
    }
    LogError("Init", idx);
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "InitByIdxSafe: NetEq Initialization error: could not "
                 "initialize NetEq");
    is_initialized_[idx] = false;
    return -1;
  }
  is_initialized_[idx] = true;
  return 0;
}

int16_t ACMNetEQ::EnableVADByIdxSafe(const int16_t idx) {
  if (ptr_vadinst_[idx] == NULL) {
    if (WebRtcVad_Create(&ptr_vadinst_[idx]) < 0) {
      ptr_vadinst_[idx] = NULL;
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                   "EnableVADByIdxSafe: NetEq Initialization error: could not "
                   "create VAD");
      return -1;
    }
  }

  if (WebRtcNetEQ_SetVADInstance(
      inst_[idx], ptr_vadinst_[idx],
      (WebRtcNetEQ_VADInitFunction) WebRtcVad_Init,
      (WebRtcNetEQ_VADSetmodeFunction) WebRtcVad_set_mode,
      (WebRtcNetEQ_VADFunction) WebRtcVad_Process) < 0) {
    LogError("setVADinstance", idx);
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "EnableVADByIdxSafe: NetEq Initialization error: could not "
                 "set VAD instance");
    return -1;
  }

  if (WebRtcNetEQ_SetVADMode(inst_[idx], vad_mode_) < 0) {
    LogError("setVADmode", idx);
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "EnableVADByIdxSafe: NetEq Initialization error: could not "
                 "set VAD mode");
    return -1;
  }
  return 0;
}

int32_t ACMNetEQ::AllocatePacketBuffer(
    const WebRtcNetEQDecoder* used_codecs,
    int16_t num_codecs) {
  // Due to WebRtcNetEQ_GetRecommendedBufferSize
  // the following has to be int otherwise we will have compiler error
  // if not casted

  CriticalSectionScoped lock(neteq_crit_sect_);
  for (int16_t idx = 0; idx < num_slaves_ + 1; idx++) {
    if (AllocatePacketBufferByIdxSafe(used_codecs, num_codecs, idx) < 0) {
      return -1;
    }
  }
  return 0;
}

int16_t ACMNetEQ::AllocatePacketBufferByIdxSafe(
    const WebRtcNetEQDecoder* used_codecs,
    int16_t num_codecs,
    const int16_t idx) {
  int max_num_packets;
  int buffer_size_in_bytes;
  int per_packet_overhead_bytes;

  if (!is_initialized_[idx]) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "AllocatePacketBufferByIdxSafe: NetEq is not initialized.");
    return -1;
  }
  if (WebRtcNetEQ_GetRecommendedBufferSize(inst_[idx], used_codecs,
                                           num_codecs,
                                           kTCPXLargeJitter,
                                           &max_num_packets,
                                           &buffer_size_in_bytes,
                                           &per_packet_overhead_bytes) != 0) {
    LogError("GetRecommendedBufferSize", idx);
    return -1;
  }
  if (idx == 0) {
    min_of_buffer_size_bytes_ = buffer_size_in_bytes;
    min_of_max_num_packets_ = max_num_packets;
    per_packet_overhead_bytes_ = per_packet_overhead_bytes;
  } else {
    min_of_buffer_size_bytes_ = std::min(min_of_buffer_size_bytes_,
                                        buffer_size_in_bytes);
    min_of_max_num_packets_ = std::min(min_of_max_num_packets_,
                                       max_num_packets);
  }
  if (neteq_packet_buffer_[idx] != NULL) {
    free(neteq_packet_buffer_[idx]);
    neteq_packet_buffer_[idx] = NULL;
  }

  neteq_packet_buffer_[idx] = (int16_t *) malloc(buffer_size_in_bytes);
  if (neteq_packet_buffer_[idx] == NULL) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "AllocatePacketBufferByIdxSafe: NetEq Initialization error: "
                 "could not allocate memory for NetEq Packet Buffer");
    return -1;
  }
  if (WebRtcNetEQ_AssignBuffer(inst_[idx], max_num_packets,
                               neteq_packet_buffer_[idx],
                               buffer_size_in_bytes) != 0) {
    if (neteq_packet_buffer_[idx] != NULL) {
      free(neteq_packet_buffer_[idx]);
      neteq_packet_buffer_[idx] = NULL;
    }
    LogError("AssignBuffer", idx);
    return -1;
  }
  return 0;
}

int32_t ACMNetEQ::SetAVTPlayout(const bool enable) {
  CriticalSectionScoped lock(neteq_crit_sect_);
  if (avt_playout_ != enable) {
    for (int16_t idx = 0; idx < num_slaves_ + 1; idx++) {
      if (!is_initialized_[idx]) {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                     "SetAVTPlayout: NetEq is not initialized.");
        return -1;
      }
      if (WebRtcNetEQ_SetAVTPlayout(inst_[idx], (enable) ? 1 : 0) < 0) {
        LogError("SetAVTPlayout", idx);
        return -1;
      }
    }
  }
  avt_playout_ = enable;
  return 0;
}

bool ACMNetEQ::avt_playout() const {
  CriticalSectionScoped lock(neteq_crit_sect_);
  return avt_playout_;
}

int32_t ACMNetEQ::CurrentSampFreqHz() const {
  CriticalSectionScoped lock(neteq_crit_sect_);
  if (!is_initialized_[0]) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "CurrentSampFreqHz: NetEq is not initialized.");
    return -1;
  }
  return (int32_t)(1000 * current_samp_freq_khz_);
}

int32_t ACMNetEQ::SetPlayoutMode(const AudioPlayoutMode mode) {
  CriticalSectionScoped lock(neteq_crit_sect_);
  if (playout_mode_ == mode)
    return 0;

  enum WebRtcNetEQPlayoutMode playout_mode = kPlayoutOff;
  enum WebRtcNetEQBGNMode background_noise_mode = kBGNOn;
  switch (mode) {
    case voice:
      playout_mode = kPlayoutOn;
      background_noise_mode = kBGNOn;
      break;
    case fax:
      playout_mode = kPlayoutFax;
      WebRtcNetEQ_GetBGNMode(inst_[0], &background_noise_mode);  // No change.
      break;
    case streaming:
      playout_mode = kPlayoutStreaming;
      background_noise_mode = kBGNOff;
      break;
    case off:
      playout_mode = kPlayoutOff;
      background_noise_mode = kBGNOff;
      break;
  }

  int err = 0;
  for (int16_t idx = 0; idx < num_slaves_ + 1; idx++) {
    if (!is_initialized_[idx]) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                   "SetPlayoutMode: NetEq is not initialized.");
      return -1;
    }

    if (WebRtcNetEQ_SetPlayoutMode(inst_[idx], playout_mode) < 0) {
      LogError("SetPlayoutMode", idx);
      err = -1;
    }

    if (WebRtcNetEQ_SetBGNMode(inst_[idx], kBGNOff) < 0) {
      LogError("SetPlayoutMode::SetBGNMode", idx);
      err = -1;
    }
  }
  if (err == 0)
    playout_mode_ = mode;
  return err;
}

AudioPlayoutMode ACMNetEQ::playout_mode() const {
  CriticalSectionScoped lock(neteq_crit_sect_);
  return playout_mode_;
}

int32_t ACMNetEQ::NetworkStatistics(
    ACMNetworkStatistics* statistics) const {
  WebRtcNetEQ_NetworkStatistics stats;
  CriticalSectionScoped lock(neteq_crit_sect_);
  if (!is_initialized_[0]) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "NetworkStatistics: NetEq is not initialized.");
    return -1;
  }
  if (WebRtcNetEQ_GetNetworkStatistics(inst_[0], &stats) == 0) {
    statistics->currentAccelerateRate = stats.currentAccelerateRate;
    statistics->currentBufferSize = stats.currentBufferSize;
    statistics->jitterPeaksFound = (stats.jitterPeaksFound > 0);
    statistics->currentDiscardRate = stats.currentDiscardRate;
    statistics->currentExpandRate = stats.currentExpandRate;
    statistics->currentPacketLossRate = stats.currentPacketLossRate;
    statistics->currentPreemptiveRate = stats.currentPreemptiveRate;
    statistics->preferredBufferSize = stats.preferredBufferSize;
    statistics->clockDriftPPM = stats.clockDriftPPM;
    statistics->addedSamples = stats.addedSamples;
  } else {
    LogError("getNetworkStatistics", 0);
    return -1;
  }
  const int kArrayLen = 100;
  int waiting_times[kArrayLen];
  int waiting_times_len = WebRtcNetEQ_GetRawFrameWaitingTimes(inst_[0],
                                                              kArrayLen,
                                                              waiting_times);
  if (waiting_times_len > 0) {
    std::vector<int> waiting_times_vec(waiting_times,
                                       waiting_times + waiting_times_len);
    std::sort(waiting_times_vec.begin(), waiting_times_vec.end());
    size_t size = waiting_times_vec.size();
    assert(size == static_cast<size_t>(waiting_times_len));
    if (size % 2 == 0) {
      statistics->medianWaitingTimeMs = (waiting_times_vec[size / 2 - 1] +
          waiting_times_vec[size / 2]) / 2;
    } else {
      statistics->medianWaitingTimeMs = waiting_times_vec[size / 2];
    }
    statistics->minWaitingTimeMs = waiting_times_vec.front();
    statistics->maxWaitingTimeMs = waiting_times_vec.back();
    double sum = 0;
    for (size_t i = 0; i < size; ++i) {
      sum += waiting_times_vec[i];
    }
    statistics->meanWaitingTimeMs = static_cast<int>(sum / size);
  } else if (waiting_times_len == 0) {
    statistics->meanWaitingTimeMs = -1;
    statistics->medianWaitingTimeMs = -1;
    statistics->minWaitingTimeMs = -1;
    statistics->maxWaitingTimeMs = -1;
  } else {
    LogError("getRawFrameWaitingTimes", 0);
    return -1;
  }
  return 0;
}

// Should only be called in AV-sync mode.
int ACMNetEQ::RecIn(const WebRtcRTPHeader& rtp_info,
                    uint32_t receive_timestamp) {
  assert(av_sync_);

  // Translate to NetEq structure.
  WebRtcNetEQ_RTPInfo neteq_rtpinfo;
  neteq_rtpinfo.payloadType = rtp_info.header.payloadType;
  neteq_rtpinfo.sequenceNumber = rtp_info.header.sequenceNumber;
  neteq_rtpinfo.timeStamp = rtp_info.header.timestamp;
  neteq_rtpinfo.SSRC = rtp_info.header.ssrc;
  neteq_rtpinfo.markerBit = rtp_info.header.markerBit;

  CriticalSectionScoped lock(neteq_crit_sect_);

  // Master should be initialized.
  assert(is_initialized_[0]);

  // Push into Master.
  int status = WebRtcNetEQ_RecInSyncRTP(inst_[0], &neteq_rtpinfo,
                                        receive_timestamp);
  if (status < 0) {
    LogError("RecInSyncRTP", 0);
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "RecIn (sync): NetEq, error in pushing in Master");
    return -1;
  }

  // If the received stream is stereo, insert a sync payload into slave.
  if (rtp_info.type.Audio.channel == 2) {
    // Slave should be initialized.
    assert(is_initialized_[1]);

    // PUSH into Slave
    status = WebRtcNetEQ_RecInSyncRTP(inst_[1], &neteq_rtpinfo,
                                      receive_timestamp);
    if (status < 0) {
      LogError("RecInRTPStruct", 1);
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                   "RecIn (sync): NetEq, error in pushing in Slave");
      return -1;
    }
  }
  return status;
}

int32_t ACMNetEQ::RecIn(const uint8_t* incoming_payload,
                        const int32_t length_payload,
                        const WebRtcRTPHeader& rtp_info,
                        uint32_t receive_timestamp) {
  int16_t payload_length = static_cast<int16_t>(length_payload);

  // Translate to NetEq structure.
  WebRtcNetEQ_RTPInfo neteq_rtpinfo;
  neteq_rtpinfo.payloadType = rtp_info.header.payloadType;
  neteq_rtpinfo.sequenceNumber = rtp_info.header.sequenceNumber;
  neteq_rtpinfo.timeStamp = rtp_info.header.timestamp;
  neteq_rtpinfo.SSRC = rtp_info.header.ssrc;
  neteq_rtpinfo.markerBit = rtp_info.header.markerBit;

  CriticalSectionScoped lock(neteq_crit_sect_);

  int status;
  // In case of stereo payload, first half of the data should be pushed into
  // master, and the second half into slave.
  if (rtp_info.type.Audio.channel == 2) {
    payload_length = payload_length / 2;
  }

  // Check that master is initialized.
  if (!is_initialized_[0]) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "RecIn: NetEq is not initialized.");
    return -1;
  }
  // Push into Master.
  status = WebRtcNetEQ_RecInRTPStruct(inst_[0], &neteq_rtpinfo,
                                      incoming_payload, payload_length,
                                      receive_timestamp);
  if (status < 0) {
    LogError("RecInRTPStruct", 0);
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "RecIn: NetEq, error in pushing in Master");
    return -1;
  }

  // If the received stream is stereo, insert second half of paket into slave.
  if (rtp_info.type.Audio.channel == 2) {
    if (!is_initialized_[1]) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                   "RecIn: NetEq is not initialized.");
      return -1;
    }
    // Push into Slave.
    status = WebRtcNetEQ_RecInRTPStruct(inst_[1], &neteq_rtpinfo,
                                        &incoming_payload[payload_length],
                                        payload_length, receive_timestamp);
    if (status < 0) {
      LogError("RecInRTPStruct", 1);
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                   "RecIn: NetEq, error in pushing in Slave");
      return -1;
    }
  }

  return 0;
}

int32_t ACMNetEQ::RecOut(AudioFrame& audio_frame) {
  enum WebRtcNetEQOutputType type;
  int16_t payload_len_sample;
  enum WebRtcNetEQOutputType type_master;
  enum WebRtcNetEQOutputType type_slave;

  int16_t payload_len_sample_slave;

  CriticalSectionScoped lockNetEq(neteq_crit_sect_);

  if (!received_stereo_) {
    if (!is_initialized_[0]) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                   "RecOut: NetEq is not initialized.");
      return -1;
    }
    {
      WriteLockScoped lockCodec(*decode_lock_);
      if (WebRtcNetEQ_RecOut(inst_[0], &(audio_frame.data_[0]),
                             &payload_len_sample) != 0) {
        LogError("RecOut", 0);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                     "RecOut: NetEq, error in pulling out for mono case");
        // Check for errors that can be recovered from:
        // RECOUT_ERROR_SAMPLEUNDERRUN = 2003
        int error_code = WebRtcNetEQ_GetErrorCode(inst_[0]);
        if (error_code != 2003) {
          // Cannot recover; return an error
          return -1;
        }
      }
    }
    WebRtcNetEQ_GetSpeechOutputType(inst_[0], &type);
    audio_frame.num_channels_ = 1;
  } else {
    if (!is_initialized_[0] || !is_initialized_[1]) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                   "RecOut: NetEq is not initialized.");
      return -1;
    }
    int16_t payload_master[480];
    int16_t payload_slave[480];
    {
      WriteLockScoped lockCodec(*decode_lock_);
      if (WebRtcNetEQ_RecOutMasterSlave(inst_[0], payload_master,
                                        &payload_len_sample, master_slave_info_,
                                        1) != 0) {
        LogError("RecOutMasterSlave", 0);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                     "RecOut: NetEq, error in pulling out for master");

        // Check for errors that can be recovered from:
        // RECOUT_ERROR_SAMPLEUNDERRUN = 2003
        int error_code = WebRtcNetEQ_GetErrorCode(inst_[0]);
        if (error_code != 2003) {
          // Cannot recover; return an error
          return -1;
        }
      }
      if (WebRtcNetEQ_RecOutMasterSlave(inst_[1], payload_slave,
                                        &payload_len_sample_slave,
                                        master_slave_info_, 0) != 0) {
        LogError("RecOutMasterSlave", 1);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                     "RecOut: NetEq, error in pulling out for slave");

        // Check for errors that can be recovered from:
        // RECOUT_ERROR_SAMPLEUNDERRUN = 2003
        int error_code = WebRtcNetEQ_GetErrorCode(inst_[1]);
        if (error_code != 2003) {
          // Cannot recover; return an error
          return -1;
        }
      }
    }

    if (payload_len_sample != payload_len_sample_slave) {
      WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceAudioCoding, id_,
                   "RecOut: mismatch between the lenght of the decoded audio "
                   "by Master (%d samples) and Slave (%d samples).",
                   payload_len_sample, payload_len_sample_slave);
      if (payload_len_sample > payload_len_sample_slave) {
        memset(&payload_slave[payload_len_sample_slave], 0,
               (payload_len_sample - payload_len_sample_slave) *
               sizeof(int16_t));
      }
    }

    for (int16_t n = 0; n < payload_len_sample; n++) {
      audio_frame.data_[n << 1] = payload_master[n];
      audio_frame.data_[(n << 1) + 1] = payload_slave[n];
    }
    audio_frame.num_channels_ = 2;

    WebRtcNetEQ_GetSpeechOutputType(inst_[0], &type_master);
    WebRtcNetEQ_GetSpeechOutputType(inst_[1], &type_slave);
    if ((type_master == kOutputNormal) || (type_slave == kOutputNormal)) {
      type = kOutputNormal;
    } else {
      type = type_master;
    }
  }

  audio_frame.samples_per_channel_ =
      static_cast<uint16_t>(payload_len_sample);
  // NetEq always returns 10 ms of audio.
  current_samp_freq_khz_ =
      static_cast<float>(audio_frame.samples_per_channel_) / 10.0f;
  audio_frame.sample_rate_hz_ = audio_frame.samples_per_channel_ * 100;
  if (vad_status_) {
    if (type == kOutputVADPassive) {
      audio_frame.vad_activity_ = AudioFrame::kVadPassive;
      audio_frame.speech_type_ = AudioFrame::kNormalSpeech;
    } else if (type == kOutputNormal) {
      audio_frame.vad_activity_ = AudioFrame::kVadActive;
      audio_frame.speech_type_ = AudioFrame::kNormalSpeech;
    } else if (type == kOutputPLC) {
      audio_frame.vad_activity_ = previous_audio_activity_;
      audio_frame.speech_type_ = AudioFrame::kPLC;
    } else if (type == kOutputCNG) {
      audio_frame.vad_activity_ = AudioFrame::kVadPassive;
      audio_frame.speech_type_ = AudioFrame::kCNG;
    } else {
      audio_frame.vad_activity_ = AudioFrame::kVadPassive;
      audio_frame.speech_type_ = AudioFrame::kPLCCNG;
    }
  } else {
    // Always return kVadUnknown when receive VAD is inactive
    audio_frame.vad_activity_ = AudioFrame::kVadUnknown;
    if (type == kOutputNormal) {
      audio_frame.speech_type_ = AudioFrame::kNormalSpeech;
    } else if (type == kOutputPLC) {
      audio_frame.speech_type_ = AudioFrame::kPLC;
    } else if (type == kOutputPLCtoCNG) {
      audio_frame.speech_type_ = AudioFrame::kPLCCNG;
    } else if (type == kOutputCNG) {
      audio_frame.speech_type_ = AudioFrame::kCNG;
    } else {
      // type is kOutputVADPassive which
      // we don't expect to get if vad_status_ is false
      WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceAudioCoding, id_,
                   "RecOut: NetEq returned kVadPassive while vad_status_ is "
                   "false.");
      audio_frame.vad_activity_ = AudioFrame::kVadUnknown;
      audio_frame.speech_type_ = AudioFrame::kNormalSpeech;
    }
  }
  previous_audio_activity_ = audio_frame.vad_activity_;

  WebRtcNetEQ_ProcessingActivity processing_stats;
  WebRtcNetEQ_GetProcessingActivity(inst_[0], &processing_stats);
  TRACE_EVENT2("webrtc", "ACM::RecOut",
               "accelerate bgn", processing_stats.accelerate_bgn_samples,
               "accelerate normal", processing_stats.accelerate_normal_samples);
  TRACE_EVENT2("webrtc", "ACM::RecOut",
               "expand bgn", processing_stats.expand_bgn_sampels,
               "expand normal", processing_stats.expand_normal_samples);
  TRACE_EVENT2("webrtc", "ACM::RecOut",
               "preemptive bgn", processing_stats.preemptive_expand_bgn_samples,
               "preemptive normal",
               processing_stats.preemptive_expand_normal_samples);
  TRACE_EVENT2("webrtc", "ACM::RecOut",
               "merge bgn", processing_stats.merge_expand_bgn_samples,
               "merge normal", processing_stats.merge_expand_normal_samples);
  return 0;
}

// When ACMGenericCodec has set the codec specific parameters in codec_def
// it calls AddCodec() to add the new codec to the NetEQ database.
int32_t ACMNetEQ::AddCodec(WebRtcNetEQ_CodecDef* codec_def,
                           bool to_master) {
  if (codec_def == NULL) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "ACMNetEQ::AddCodec: error, codec_def is NULL");
    return -1;
  }
  CriticalSectionScoped lock(neteq_crit_sect_);

  int16_t idx;
  if (to_master) {
    idx = 0;
  } else {
    idx = 1;
  }

  if (!is_initialized_[idx]) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "ACMNetEQ::AddCodec: NetEq is not initialized.");
    return -1;
  }
  if (WebRtcNetEQ_CodecDbAdd(inst_[idx], codec_def) < 0) {
    LogError("CodecDB_Add", idx);
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "ACMNetEQ::AddCodec: NetEq, error in adding codec");
    return -1;
  } else {
    return 0;
  }
}

// Creates a Word16 RTP packet out of a Word8 payload and an rtp info struct.
// Must be byte order safe.
void ACMNetEQ::RTPPack(int16_t* rtp_packet, const int8_t* payload,
                       const int32_t payload_length_bytes,
                       const WebRtcRTPHeader& rtp_info) {
  int32_t idx = 0;
  WEBRTC_SPL_SET_BYTE(rtp_packet, (int8_t) 0x80, idx);
  idx++;
  WEBRTC_SPL_SET_BYTE(rtp_packet, rtp_info.header.payloadType, idx);
  idx++;
  WEBRTC_SPL_SET_BYTE(rtp_packet,
                      WEBRTC_SPL_GET_BYTE(&(rtp_info.header.sequenceNumber), 1),
                      idx);
  idx++;
  WEBRTC_SPL_SET_BYTE(rtp_packet,
                      WEBRTC_SPL_GET_BYTE(&(rtp_info.header.sequenceNumber), 0),
                      idx);
  idx++;
  WEBRTC_SPL_SET_BYTE(rtp_packet,
                      WEBRTC_SPL_GET_BYTE(&(rtp_info.header.timestamp), 3),
                      idx);
  idx++;
  WEBRTC_SPL_SET_BYTE(rtp_packet,
                      WEBRTC_SPL_GET_BYTE(&(rtp_info.header.timestamp), 2),
                      idx);
  idx++;
  WEBRTC_SPL_SET_BYTE(rtp_packet,
                      WEBRTC_SPL_GET_BYTE(&(rtp_info.header.timestamp), 1),
                      idx);
  idx++;
  WEBRTC_SPL_SET_BYTE(rtp_packet,
                      WEBRTC_SPL_GET_BYTE(&(rtp_info.header.timestamp), 0),
                      idx);
  idx++;
  WEBRTC_SPL_SET_BYTE(rtp_packet,
                      WEBRTC_SPL_GET_BYTE(&(rtp_info.header.ssrc), 3), idx);
  idx++;
  WEBRTC_SPL_SET_BYTE(rtp_packet, WEBRTC_SPL_GET_BYTE(&(rtp_info.header.ssrc),
                                                      2), idx);
  idx++;
  WEBRTC_SPL_SET_BYTE(rtp_packet, WEBRTC_SPL_GET_BYTE(&(rtp_info.header.ssrc),
                                                      1), idx);
  idx++;
  WEBRTC_SPL_SET_BYTE(rtp_packet, WEBRTC_SPL_GET_BYTE(&(rtp_info.header.ssrc),
                                                      0), idx);
  idx++;
  for (int16_t i = 0; i < payload_length_bytes; i++) {
    WEBRTC_SPL_SET_BYTE(rtp_packet, payload[i], idx);
    idx++;
  }
  if (payload_length_bytes & 1) {
    // Our 16 bits buffer is one byte too large, set that
    // last byte to zero.
    WEBRTC_SPL_SET_BYTE(rtp_packet, 0x0, idx);
  }
}

int16_t ACMNetEQ::EnableVAD() {
  CriticalSectionScoped lock(neteq_crit_sect_);
  if (vad_status_) {
    return 0;
  }
  for (int16_t idx = 0; idx < num_slaves_ + 1; idx++) {
    if (!is_initialized_[idx]) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                   "SetVADStatus: NetEq is not initialized.");
      return -1;
    }
    // VAD was off and we have to turn it on
    if (EnableVADByIdxSafe(idx) < 0) {
      return -1;
    }

    // Set previous VAD status to PASSIVE
    previous_audio_activity_ = AudioFrame::kVadPassive;
  }
  vad_status_ = true;
  return 0;
}

ACMVADMode ACMNetEQ::vad_mode() const {
  CriticalSectionScoped lock(neteq_crit_sect_);
  return vad_mode_;
}

int16_t ACMNetEQ::SetVADMode(const ACMVADMode mode) {
  CriticalSectionScoped lock(neteq_crit_sect_);
  if ((mode < VADNormal) || (mode > VADVeryAggr)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "SetVADMode: NetEq error: could not set VAD mode, mode is not "
                 "supported");
    return -1;
  } else {
    for (int16_t idx = 0; idx < num_slaves_ + 1; idx++) {
      if (!is_initialized_[idx]) {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                     "SetVADMode: NetEq is not initialized.");
        return -1;
      }
      if (WebRtcNetEQ_SetVADMode(inst_[idx], mode) < 0) {
        LogError("SetVADmode", idx);
        return -1;
      }
    }
    vad_mode_ = mode;
    return 0;
  }
}

int32_t ACMNetEQ::FlushBuffers() {
  CriticalSectionScoped lock(neteq_crit_sect_);
  for (int16_t idx = 0; idx < num_slaves_ + 1; idx++) {
    if (!is_initialized_[idx]) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                   "FlushBuffers: NetEq is not initialized.");
      return -1;
    }
    if (WebRtcNetEQ_FlushBuffers(inst_[idx]) < 0) {
      LogError("FlushBuffers", idx);
      return -1;
    }
  }
  return 0;
}

int16_t ACMNetEQ::RemoveCodec(WebRtcNetEQDecoder codec_idx,
                              bool is_stereo) {
  // sanity check
  if ((codec_idx <= kDecoderReservedStart) ||
      (codec_idx >= kDecoderReservedEnd)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "RemoveCodec: NetEq error: could not Remove Codec, codec "
                 "index out of range");
    return -1;
  }
  CriticalSectionScoped lock(neteq_crit_sect_);
  if (!is_initialized_[0]) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "RemoveCodec: NetEq is not initialized.");
    return -1;
  }

  if (WebRtcNetEQ_CodecDbRemove(inst_[0], codec_idx) < 0) {
    LogError("CodecDB_Remove", 0);
    return -1;
  }

  if (is_stereo) {
    if (WebRtcNetEQ_CodecDbRemove(inst_[1], codec_idx) < 0) {
      LogError("CodecDB_Remove", 1);
      return -1;
    }
  }

  return 0;
}

int16_t ACMNetEQ::SetBackgroundNoiseMode(
    const ACMBackgroundNoiseMode mode) {
  CriticalSectionScoped lock(neteq_crit_sect_);
  for (int16_t idx = 0; idx < num_slaves_ + 1; idx++) {
    if (!is_initialized_[idx]) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                   "SetBackgroundNoiseMode: NetEq is not initialized.");
      return -1;
    }
    if (WebRtcNetEQ_SetBGNMode(inst_[idx], (WebRtcNetEQBGNMode) mode) < 0) {
      LogError("SetBGNMode", idx);
      return -1;
    }
  }
  return 0;
}

int16_t ACMNetEQ::BackgroundNoiseMode(ACMBackgroundNoiseMode& mode) {
  WebRtcNetEQBGNMode my_mode;
  CriticalSectionScoped lock(neteq_crit_sect_);
  if (!is_initialized_[0]) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "BackgroundNoiseMode: NetEq is not initialized.");
    return -1;
  }
  if (WebRtcNetEQ_GetBGNMode(inst_[0], &my_mode) < 0) {
    LogError("WebRtcNetEQ_GetBGNMode", 0);
    return -1;
  } else {
    mode = (ACMBackgroundNoiseMode) my_mode;
  }
  return 0;
}

void ACMNetEQ::set_id(int32_t id) {
  CriticalSectionScoped lock(neteq_crit_sect_);
  id_ = id;
}

void ACMNetEQ::LogError(const char* neteq_func_name,
                        const int16_t idx) const {
  char error_name[NETEQ_ERR_MSG_LEN_BYTE];
  char my_func_name[50];
  int neteq_error_code = WebRtcNetEQ_GetErrorCode(inst_[idx]);
  WebRtcNetEQ_GetErrorName(neteq_error_code, error_name,
                           NETEQ_ERR_MSG_LEN_BYTE - 1);
  strncpy(my_func_name, neteq_func_name, 49);
  error_name[NETEQ_ERR_MSG_LEN_BYTE - 1] = '\0';
  my_func_name[49] = '\0';
  WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
               "NetEq-%d Error in function %s, error-code: %d, error-string: "
               " %s", idx, my_func_name, neteq_error_code, error_name);
}

int32_t ACMNetEQ::PlayoutTimestamp(uint32_t& timestamp) {
  CriticalSectionScoped lock(neteq_crit_sect_);
  if (WebRtcNetEQ_GetSpeechTimeStamp(inst_[0], &timestamp) < 0) {
    LogError("GetSpeechTimeStamp", 0);
    return -1;
  } else {
    return 0;
  }
}

void ACMNetEQ::RemoveSlaves() {
  CriticalSectionScoped lock(neteq_crit_sect_);
  RemoveSlavesSafe();
}

void ACMNetEQ::RemoveSlavesSafe() {
  for (int i = 1; i < num_slaves_ + 1; i++) {
    RemoveNetEQSafe(i);
  }

  if (master_slave_info_ != NULL) {
    free(master_slave_info_);
    master_slave_info_ = NULL;
  }
  num_slaves_ = 0;
}

void ACMNetEQ::RemoveNetEQSafe(int index) {
  if (inst_mem_[index] != NULL) {
    free(inst_mem_[index]);
    inst_mem_[index] = NULL;
  }
  if (neteq_packet_buffer_[index] != NULL) {
    free(neteq_packet_buffer_[index]);
    neteq_packet_buffer_[index] = NULL;
  }
  if (ptr_vadinst_[index] != NULL) {
    WebRtcVad_Free(ptr_vadinst_[index]);
    ptr_vadinst_[index] = NULL;
  }
}

int16_t ACMNetEQ::AddSlave(const WebRtcNetEQDecoder* used_codecs,
                           int16_t num_codecs) {
  CriticalSectionScoped lock(neteq_crit_sect_);
  const int16_t slave_idx = 1;
  if (num_slaves_ < 1) {
    // initialize the receiver, this also sets up VAD.
    if (InitByIdxSafe(slave_idx) < 0) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                   "AddSlave: AddSlave Failed, Could not Initialize");
      return -1;
    }

    // Allocate buffer.
    if (AllocatePacketBufferByIdxSafe(used_codecs, num_codecs,
                                      slave_idx) < 0) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                   "AddSlave: AddSlave Failed, Could not Allocate Packet "
                   "Buffer");
      return -1;
    }

    if (master_slave_info_ != NULL) {
      free(master_slave_info_);
      master_slave_info_ = NULL;
    }
    int ms_info_size = WebRtcNetEQ_GetMasterSlaveInfoSize();
    master_slave_info_ = malloc(ms_info_size);

    if (master_slave_info_ == NULL) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                   "AddSlave: AddSlave Failed, Could not Allocate memory for "
                   "Master-Slave Info");
      return -1;
    }

    // We accept this as initialized NetEQ, the rest is to synchronize
    // Slave with Master.
    num_slaves_ = 1;
    is_initialized_[slave_idx] = true;

    // Set AVT
    if (WebRtcNetEQ_SetAVTPlayout(inst_[slave_idx],
                                  (avt_playout_) ? 1 : 0) < 0) {
      LogError("SetAVTPlayout", slave_idx);
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                   "AddSlave: AddSlave Failed, Could not set AVT playout.");
      return -1;
    }

    // Set Background Noise
    WebRtcNetEQBGNMode current_mode;
    if (WebRtcNetEQ_GetBGNMode(inst_[0], &current_mode) < 0) {
      LogError("GetBGNMode", 0);
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                   "AAddSlave: AddSlave Failed, Could not Get BGN form "
                   "Master.");
      return -1;
    }

    if (WebRtcNetEQ_SetBGNMode(inst_[slave_idx],
                               (WebRtcNetEQBGNMode) current_mode) < 0) {
      LogError("SetBGNMode", slave_idx);
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                   "AddSlave: AddSlave Failed, Could not set BGN mode.");
      return -1;
    }

    enum WebRtcNetEQPlayoutMode playout_mode = kPlayoutOff;
    switch (playout_mode_) {
      case voice:
        playout_mode = kPlayoutOn;
        break;
      case fax:
        playout_mode = kPlayoutFax;
        break;
      case streaming:
        playout_mode = kPlayoutStreaming;
        break;
      case off:
        playout_mode = kPlayoutOff;
        break;
    }
    if (WebRtcNetEQ_SetPlayoutMode(inst_[slave_idx], playout_mode) < 0) {
      LogError("SetPlayoutMode", 1);
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                   "AddSlave: AddSlave Failed, Could not Set Playout Mode.");
      return -1;
    }

    // Set AV-sync for the slave.
    WebRtcNetEQ_EnableAVSync(inst_[slave_idx], av_sync_ ? 1 : 0);

    // Set minimum delay.
    if (minimum_delay_ms_ > 0)
      WebRtcNetEQ_SetMinimumDelay(inst_[slave_idx], minimum_delay_ms_);
  }

  return 0;
}

void ACMNetEQ::set_received_stereo(bool received_stereo) {
  CriticalSectionScoped lock(neteq_crit_sect_);
  received_stereo_ = received_stereo;
}

uint8_t ACMNetEQ::num_slaves() {
  CriticalSectionScoped lock(neteq_crit_sect_);
  return num_slaves_;
}

void ACMNetEQ::EnableAVSync(bool enable) {
  CriticalSectionScoped lock(neteq_crit_sect_);
  av_sync_ = enable;
  for (int i = 0; i < num_slaves_ + 1; ++i) {
    assert(is_initialized_[i]);
    WebRtcNetEQ_EnableAVSync(inst_[i], enable ? 1 : 0);
  }
}

int ACMNetEQ::SetMinimumDelay(int minimum_delay_ms) {
  CriticalSectionScoped lock(neteq_crit_sect_);
  for (int i = 0; i < num_slaves_ + 1; ++i) {
    assert(is_initialized_[i]);
    if (WebRtcNetEQ_SetMinimumDelay(inst_[i], minimum_delay_ms) < 0)
      return -1;
  }
  minimum_delay_ms_ = minimum_delay_ms;
  return 0;
}

int ACMNetEQ::LeastRequiredDelayMs() const {
  CriticalSectionScoped lock(neteq_crit_sect_);
  assert(is_initialized_[0]);

  // Sufficient to query the master.
  return WebRtcNetEQ_GetRequiredDelayMs(inst_[0]);
}

bool ACMNetEQ::DecodedRtpInfo(int* sequence_number, uint32_t* timestamp) const {
  CriticalSectionScoped lock(neteq_crit_sect_);
  if (WebRtcNetEQ_DecodedRtpInfo(inst_[0], sequence_number, timestamp) < 0)
    return false;
  return true;
}

}  // namespace webrtc
