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

#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/modules/video_coding/main/source/jitter_buffer.h"
#include "webrtc/modules/video_coding/main/source/packet.h"
#include "webrtc/modules/video_coding/main/source/tick_time_base.h"
#include "webrtc/modules/video_coding/main/source/timing.h"

namespace webrtc {

class VCMEncodedFrame;

enum VCMNackStatus {
  kNackOk,
  kNackNeedMoreMemory,
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
              TickTimeBase* clock,
              int32_t vcm_id = -1,
              int32_t receiver_id = -1,
              bool master = true);
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
  void SetNackMode(VCMNackMode nackMode);
  VCMNackMode NackMode() const;
  VCMNackStatus NackList(uint16_t* nackList, uint16_t* size);

  // Dual decoder.
  bool DualDecoderCaughtUp(VCMEncodedFrame* dual_frame,
                           VCMReceiver& dual_receiver) const;
  VCMReceiverState State() const;

 private:
  VCMEncodedFrame* FrameForDecoding(uint16_t max_wait_time_ms,
                                    int64_t nextrender_time_ms,
                                    VCMReceiver* dual_receiver);
  VCMEncodedFrame* FrameForRendering(uint16_t max_wait_time_ms,
                                     int64_t nextrender_time_ms,
                                     VCMReceiver* dual_receiver);
  void CopyJitterBufferStateFromReceiver(const VCMReceiver& receiver);
  void UpdateState(VCMReceiverState new_state);
  void UpdateState(const VCMEncodedFrame& frame);
  static int32_t GenerateReceiverId();

  CriticalSectionWrapper* crit_sect_;
  int32_t vcm_id_;
  TickTimeBase* clock_;
  int32_t receiver_id_;
  bool master_;
  VCMJitterBuffer jitter_buffer_;
  VCMTiming* timing_;
  VCMEvent render_wait_event_;
  VCMReceiverState state_;

  static int32_t receiver_id_counter_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_MAIN_SOURCE_RECEIVER_H_
