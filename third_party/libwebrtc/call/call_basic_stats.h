#ifndef CALL_CALL_BASIC_STATS_H_
#define CALL_CALL_BASIC_STATS_H_

#include <string>

namespace webrtc {

// named to avoid conflicts with video/call_stats.h
struct CallBasicStats {
  std::string ToString(int64_t time_ms) const;

  int send_bandwidth_bps = 0;       // Estimated available send bandwidth.
  int max_padding_bitrate_bps = 0;  // Cumulative configured max padding.
  int recv_bandwidth_bps = 0;       // Estimated available receive bandwidth.
  int64_t pacer_delay_ms = 0;
  int64_t rtt_ms = -1;
};

}  // namespace webrtc

#endif  // CALL_CALL_BASIC_STATS_H_
