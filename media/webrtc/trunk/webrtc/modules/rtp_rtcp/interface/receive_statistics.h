/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_INTERFACE_RECEIVE_STATISTICS_H_
#define WEBRTC_MODULES_RTP_RTCP_INTERFACE_RECEIVE_STATISTICS_H_

#include <map>

#include "webrtc/modules/interface/module.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class Clock;

class StreamStatistician {
 public:
  struct Statistics {
    Statistics()
        : fraction_lost(0),
          cumulative_lost(0),
          extended_max_sequence_number(0),
          jitter(0),
          max_jitter(0) {}

    uint8_t fraction_lost;
    uint32_t cumulative_lost;
    uint32_t extended_max_sequence_number;
    uint32_t jitter;
    uint32_t max_jitter;
  };

  virtual ~StreamStatistician();

  virtual bool GetStatistics(Statistics* statistics, bool reset) = 0;
  virtual void GetDataCounters(uint32_t* bytes_received,
                               uint32_t* packets_received) const = 0;
  virtual uint32_t BitrateReceived() const = 0;

  // Resets all statistics.
  virtual void ResetStatistics() = 0;

  // Returns true if the packet with RTP header |header| is likely to be a
  // retransmitted packet, false otherwise.
  virtual bool IsRetransmitOfOldPacket(const RTPHeader& header,
                                       int min_rtt) const = 0;

  // Returns true if |sequence_number| is received in order, false otherwise.
  virtual bool IsPacketInOrder(uint16_t sequence_number) const = 0;
};

typedef std::map<uint32_t, StreamStatistician*> StatisticianMap;

class ReceiveStatistics : public Module {
 public:
  virtual ~ReceiveStatistics() {}

  static ReceiveStatistics* Create(Clock* clock);

  // Updates the receive statistics with this packet.
  virtual void IncomingPacket(const RTPHeader& rtp_header, size_t bytes,
                              bool retransmitted) = 0;

  // Returns a map of all statisticians which have seen an incoming packet
  // during the last two seconds.
  virtual StatisticianMap GetActiveStatisticians() const = 0;

  // Returns a pointer to the statistician of an ssrc.
  virtual StreamStatistician* GetStatistician(uint32_t ssrc) const = 0;

  // Sets the max reordering threshold in number of packets.
  virtual void SetMaxReorderingThreshold(int max_reordering_threshold) = 0;
};

class NullReceiveStatistics : public ReceiveStatistics {
 public:
  virtual void IncomingPacket(const RTPHeader& rtp_header, size_t bytes,
                              bool retransmitted) OVERRIDE;
  virtual StatisticianMap GetActiveStatisticians() const OVERRIDE;
  virtual StreamStatistician* GetStatistician(uint32_t ssrc) const OVERRIDE;
  virtual int32_t TimeUntilNextProcess() OVERRIDE;
  virtual int32_t Process() OVERRIDE;
  virtual void SetMaxReorderingThreshold(int max_reordering_threshold) OVERRIDE;
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_INTERFACE_RECEIVE_STATISTICS_H_
