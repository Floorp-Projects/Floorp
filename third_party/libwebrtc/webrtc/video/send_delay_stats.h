/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_SEND_DELAY_STATS_H_
#define VIDEO_SEND_DELAY_STATS_H_

#include <map>
#include <memory>
#include <set>

#include "common_types.h"  // NOLINT(build/include)
#include "modules/include/module_common_types.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/thread_annotations.h"
#include "system_wrappers/include/clock.h"
#include "video/stats_counter.h"
#include "call/video_send_stream.h"

namespace webrtc {

class SendDelayStats : public SendPacketObserver {
 public:
  explicit SendDelayStats(Clock* clock);
  virtual ~SendDelayStats();

  // Adds the configured ssrcs for the rtp streams.
  // Stats will be calculated for these streams.
  void AddSsrcs(const VideoSendStream::Config& config);

  // Called when a packet is sent (leaving socket).
  bool OnSentPacket(int packet_id, int64_t time_ms);

 protected:
  // From SendPacketObserver.
  // Called when a packet is sent to the transport.
  void OnSendPacket(uint16_t packet_id,
                    int64_t capture_time_ms,
                    uint32_t ssrc) override;

 private:
  // Map holding sent packets (mapped by sequence number).
  struct SequenceNumberOlderThan {
    bool operator()(uint16_t seq1, uint16_t seq2) const {
      return IsNewerSequenceNumber(seq2, seq1);
    }
  };
  struct Packet {
    Packet(uint32_t ssrc, int64_t capture_time_ms, int64_t send_time_ms)
        : ssrc(ssrc),
          capture_time_ms(capture_time_ms),
          send_time_ms(send_time_ms) {}
    uint32_t ssrc;
    int64_t capture_time_ms;
    int64_t send_time_ms;
  };
  typedef std::map<uint16_t, Packet, SequenceNumberOlderThan> PacketMap;

  void UpdateHistograms();
  void RemoveOld(int64_t now, PacketMap* packets)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_);
  AvgCounter* GetSendDelayCounter(uint32_t ssrc)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_);

  Clock* const clock_;
  rtc::CriticalSection crit_;

  PacketMap packets_ RTC_GUARDED_BY(crit_);
  size_t num_old_packets_ RTC_GUARDED_BY(crit_);
  size_t num_skipped_packets_ RTC_GUARDED_BY(crit_);

  std::set<uint32_t> ssrcs_ RTC_GUARDED_BY(crit_);

  // Mapped by SSRC.
  std::map<uint32_t, std::unique_ptr<AvgCounter>> send_delay_counters_
      RTC_GUARDED_BY(crit_);
};

}  // namespace webrtc
#endif  // VIDEO_SEND_DELAY_STATS_H_
