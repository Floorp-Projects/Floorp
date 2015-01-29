/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_MAIN_SOURCE_RECEIVER_H_
#define WEBRTC_MODULES_VIDEO_CODING_MAIN_SOURCE_RECEIVER_H_

#include "webrtc/modules/video_coding/main/source/jitter_buffer.h"
#include "webrtc/modules/video_coding/main/source/packet.h"
#include "webrtc/modules/video_coding/main/source/timing.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/modules/video_coding/main/interface/video_coding.h"
#include "webrtc/modules/video_coding/main/interface/video_coding_defines.h"

namespace webrtc {

class Clock;
class VCMEncodedFrame;

enum VCMNackStatus {
  kNackOk,
  kNackKeyFrameRequest
};

enum VCMReceiverState {
  kReceiving,
  kPassive,
  kWaitForPrimaryDecode
};

class VCMReceiver {
 public:
  VCMReceiver(VCMTiming* timing,
              Clock* clock,
              EventFactory* event_factory,
              bool master);
  ~VCMReceiver();

  void Reset();
  int32_t Initialize();
  void UpdateRtt(uint32_t rtt);
  int32_t InsertPacket(const VCMPacket& packet,
                       uint16_t frame_width,
                       uint16_t frame_height);
  VCMEncodedFrame* FrameForDecoding(uint16_t max_wait_time_ms,
                                    int64_t& next_render_time_ms,
                                    bool render_timing = true,
                                    VCMReceiver* dual_receiver = NULL);
  void ReleaseFrame(VCMEncodedFrame* frame);
  void ReceiveStatistics(uint32_t* bitrate, uint32_t* framerate);
  void ReceivedFrameCount(VCMFrameCount* frame_count) const;
  uint32_t DiscardedPackets() const;

  // NACK.
  void SetNackMode(VCMNackMode nackMode,
                   int low_rtt_nack_threshold_ms,
                   int high_rtt_nack_threshold_ms);
  void SetNackSettings(size_t max_nack_list_size,
                       int max_packet_age_to_nack,
                       int max_incomplete_time_ms);
  VCMNackMode NackMode() const;
  VCMNackStatus NackList(uint16_t* nackList, uint16_t size,
                         uint16_t* nack_list_length);

  // Dual decoder.
  bool DualDecoderCaughtUp(VCMEncodedFrame* dual_frame,
                           VCMReceiver& dual_receiver) const;
  VCMReceiverState State() const;
  VideoReceiveState ReceiveState() const;

  // Receiver video delay.
  int SetMinReceiverDelay(int desired_delay_ms);

  // Decoding with errors.
  void SetDecodeErrorMode(VCMDecodeErrorMode decode_error_mode);
  VCMDecodeErrorMode DecodeErrorMode() const;

  // Returns size in time (milliseconds) of complete continuous frames in the
  // jitter buffer. The render time is estimated based on the render delay at
  // the time this function is called.
  int RenderBufferSizeMs();

 private:
  void CopyJitterBufferStateFromReceiver(const VCMReceiver& receiver);
  void UpdateState(VCMReceiverState new_state);
  void UpdateReceiveState(const VCMEncodedFrame& frame);
  void UpdateDualState(const VCMEncodedFrame& frame);
  static int32_t GenerateReceiverId();

  CriticalSectionWrapper* crit_sect_;
  Clock* clock_;
  bool master_;
  VCMJitterBuffer jitter_buffer_;
  VCMTiming* timing_;
  scoped_ptr<EventWrapper> render_wait_event_;
  VCMReceiverState state_;
  VideoReceiveState receiveState_;
  int max_video_delay_ms_;

  static int32_t receiver_id_counter_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_MAIN_SOURCE_RECEIVER_H_
