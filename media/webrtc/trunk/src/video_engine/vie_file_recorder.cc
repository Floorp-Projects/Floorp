/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_engine/vie_file_recorder.h"

#include "modules/utility/interface/file_player.h"
#include "modules/utility/interface/file_recorder.h"
#include "system_wrappers/interface/critical_section_wrapper.h"
#include "system_wrappers/interface/tick_util.h"
#include "system_wrappers/interface/trace.h"
#include "video_engine/vie_defines.h"

namespace webrtc {

ViEFileRecorder::ViEFileRecorder(int instanceID)
    : recorder_cs_(CriticalSectionWrapper::CreateCriticalSection()),
      file_recorder_(NULL),
      is_first_frame_recorded_(false),
      is_out_stream_started_(false),
      instance_id_(instanceID),
      frame_delay_(0),
      audio_channel_(-1),
      audio_source_(NO_AUDIO),
      voe_file_interface_(NULL) {
}

ViEFileRecorder::~ViEFileRecorder() {
  StopRecording();
  delete recorder_cs_;
}

int ViEFileRecorder::StartRecording(const char* file_nameUTF8,
                                    const VideoCodec& codec_inst,
                                    AudioSource audio_source,
                                    int audio_channel,
                                    const CodecInst& audio_codec_inst,
                                    VoiceEngine* voe_ptr,
                                    const FileFormats file_format) {
  CriticalSectionScoped lock(*recorder_cs_);

  if (file_recorder_) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, instance_id_,
                 "ViEFileRecorder::StartRecording() - already recording.");
    return -1;
  }
  file_recorder_ = FileRecorder::CreateFileRecorder(instance_id_, file_format);
  if (!file_recorder_) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, instance_id_,
                 "ViEFileRecorder::StartRecording() failed to create recoder.");
    return -1;
  }

  int error = file_recorder_->StartRecordingVideoFile(file_nameUTF8,
                                                      audio_codec_inst,
                                                      codec_inst,
                                                      AMRFileStorage,
                                                      audio_source == NO_AUDIO);
  if (error) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, instance_id_,
                 "ViEFileRecorder::StartRecording() failed to "
                 "StartRecordingVideoFile.");
    FileRecorder::DestroyFileRecorder(file_recorder_);
    file_recorder_ = NULL;
    return -1;
  }

  audio_source_ = audio_source;
  if (voe_ptr && audio_source != NO_AUDIO) {
    // VoE interface has been provided and we want to record audio.
    voe_file_interface_ = VoEFile::GetInterface(voe_ptr);
    if (!voe_file_interface_) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, instance_id_,
                   "ViEFileRecorder::StartRecording() failed to get VEFile "
                   "interface");
      return -1;
    }

    // Always L16.
    CodecInst engine_audio_codec_inst = {96, "L16", audio_codec_inst.plfreq,
                                         audio_codec_inst.plfreq / 100, 1,
                                         audio_codec_inst.plfreq * 16 };

    switch (audio_source) {
      // case NO_AUDIO is checked above.
      case MICROPHONE:
        error = voe_file_interface_->StartRecordingMicrophone(
            this, &engine_audio_codec_inst);
        break;
      case PLAYOUT:
        error = voe_file_interface_->StartRecordingPlayout(
            audio_channel, this, &engine_audio_codec_inst);
        break;
      default:
        assert(false && "Unknown audio_source");
    }
    if (error != 0) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, instance_id_,
                   "ViEFileRecorder::StartRecording() failed to start recording"
                   " audio");
      FileRecorder::DestroyFileRecorder(file_recorder_);
      file_recorder_ = NULL;
      return -1;
    }
    is_out_stream_started_ = true;
    audio_channel_ = audio_channel;
  }
  is_first_frame_recorded_ = false;
  return 0;
}

int ViEFileRecorder::StopRecording() {
  int error = 0;
  // We can not hold the ptr_cs_ while accessing VoE functions. It might cause
  // deadlock in Write.
  if (voe_file_interface_) {
    switch (audio_source_) {
      case MICROPHONE:
        error = voe_file_interface_->StopRecordingMicrophone();
        break;
      case PLAYOUT:
        error = voe_file_interface_->StopRecordingPlayout(audio_channel_);
        break;
      case NO_AUDIO:
        break;
      default:
        assert(false && "Unknown audio_source");
    }
    if (error != 0) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, instance_id_,
                   "ViEFileRecorder::StopRecording() failed to stop recording "
                   "audio");
    }
  }
  CriticalSectionScoped lock(*recorder_cs_);
  if (voe_file_interface_) {
    voe_file_interface_->Release();
    voe_file_interface_ = NULL;
  }

  if (file_recorder_) {
    if (file_recorder_->IsRecording()) {
      int error = file_recorder_->StopRecording();
      if (error) {
        return -1;
      }
    }
    FileRecorder::DestroyFileRecorder(file_recorder_);
    file_recorder_ = NULL;
  }
  is_first_frame_recorded_ = false;
  is_out_stream_started_ = false;
  return 0;
}

void ViEFileRecorder::SetFrameDelay(int frame_delay) {
  CriticalSectionScoped lock(*recorder_cs_);
  frame_delay_ = frame_delay;
}

bool ViEFileRecorder::RecordingStarted() {
  CriticalSectionScoped lock(*recorder_cs_);
  return file_recorder_ && file_recorder_->IsRecording();
}

bool ViEFileRecorder::FirstFrameRecorded() {
  CriticalSectionScoped lock(*recorder_cs_);
  return is_first_frame_recorded_;
}

bool ViEFileRecorder::IsRecordingFileFormat(const FileFormats file_format) {
  CriticalSectionScoped lock(*recorder_cs_);
  return (file_recorder_->RecordingFileFormat() == file_format) ? true : false;
}

void ViEFileRecorder::RecordVideoFrame(const VideoFrame& video_frame) {
  CriticalSectionScoped lock(*recorder_cs_);

  if (file_recorder_ && file_recorder_->IsRecording()) {
    if (!IsRecordingFileFormat(kFileFormatAviFile))
      return;

    // Compensate for frame delay in order to get audio/video sync when
    // recording local video.
    const WebRtc_UWord32 time_stamp = video_frame.TimeStamp();
    const WebRtc_Word64 render_time_stamp = video_frame.RenderTimeMs();
    VideoFrame& unconst_video_frame = const_cast<VideoFrame&>(video_frame);
    unconst_video_frame.SetTimeStamp(time_stamp - 90 * frame_delay_);
    unconst_video_frame.SetRenderTime(render_time_stamp - frame_delay_);

    file_recorder_->RecordVideoToFile(unconst_video_frame);

    unconst_video_frame.SetRenderTime(render_time_stamp);
    unconst_video_frame.SetTimeStamp(time_stamp);
  }
}

bool ViEFileRecorder::Write(const void* buf, int len) {
  if (!is_out_stream_started_)
    return true;

  // Always 10 ms L16 from VoE.
  if (len % (2 * 80)) {
    // Not 2 bytes 80 samples.
    WEBRTC_TRACE(kTraceError, kTraceVideo, audio_channel_,
                 "Audio length not supported: %d.", len);
    return true;
  }

  AudioFrame audio_frame;
  WebRtc_UWord16 length_in_samples = len / 2;
  audio_frame.UpdateFrame(audio_channel_, 0,
                          static_cast<const WebRtc_Word16*>(buf),
                          length_in_samples, length_in_samples * 100,
                          AudioFrame::kUndefined,
                          AudioFrame::kVadUnknown);

  CriticalSectionScoped lock(*recorder_cs_);
  if (file_recorder_ && file_recorder_->IsRecording()) {
    TickTime tick_time = TickTime::Now();
    file_recorder_->RecordAudioToFile(audio_frame, &tick_time);
  }

  // Always return true to continue recording.
  return true;
}

int ViEFileRecorder::Rewind() {
  // Not supported!
  return -1;
}

}  // namespace webrtc
