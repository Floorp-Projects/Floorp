/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_REMOTE_BITRATE_ESTIMATOR_UNITTEST_HELPER_H_
#define WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_REMOTE_BITRATE_ESTIMATOR_UNITTEST_HELPER_H_

#include <gtest/gtest.h>

#include <list>
#include <map>
#include <utility>

#include "modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "system_wrappers/interface/constructor_magic.h"
#include "system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

enum { kMtu = 1200 };

namespace testing {

class TestBitrateObserver : public RemoteBitrateObserver {
 public:
  TestBitrateObserver() : updated_(false), latest_bitrate_(0) {}

  void OnReceiveBitrateChanged(std::vector<unsigned int>* ssrcs,
                               unsigned int bitrate) {
    latest_bitrate_ = bitrate;
    updated_ = true;
  }

  void Reset() {
    updated_ = false;
  }

  bool updated() const {
    return updated_;
  }

  unsigned int latest_bitrate() const {
    return latest_bitrate_;
  }

 private:
  bool updated_;
  unsigned int latest_bitrate_;
};


class RtpStream {
 public:
  struct RtpPacket {
    int64_t send_time;
    int64_t arrival_time;
    uint32_t rtp_timestamp;
    unsigned int size;
    unsigned int ssrc;
  };

  struct RtcpPacket {
    uint32_t ntp_secs;
    uint32_t ntp_frac;
    uint32_t timestamp;
    unsigned int ssrc;
  };

  typedef std::list<RtpPacket*> PacketList;

  enum { kSendSideOffsetMs = 1000 };

  RtpStream(int fps, int bitrate_bps, unsigned int ssrc, unsigned int frequency,
      uint32_t timestamp_offset, int64_t rtcp_receive_time);
  void set_rtp_timestamp_offset(uint32_t offset);

  // Generates a new frame for this stream. If called too soon after the
  // previous frame, no frame will be generated. The frame is split into
  // packets.
  int64_t GenerateFrame(double time_now, PacketList* packets);

  // The send-side time when the next frame can be generated.
  double next_rtp_time() const;

  // Generates an RTCP packet.
  RtcpPacket* Rtcp(double time_now);

  void set_bitrate_bps(int bitrate_bps);

  int bitrate_bps() const;

  unsigned int ssrc() const;

  static bool Compare(const std::pair<unsigned int, RtpStream*>& left,
                      const std::pair<unsigned int, RtpStream*>& right);

 private:
  enum { kRtcpIntervalMs = 1000 };

  int fps_;
  int bitrate_bps_;
  unsigned int ssrc_;
  unsigned int frequency_;
  double next_rtp_time_;
  double next_rtcp_time_;
  uint32_t rtp_timestamp_offset_;
  const double kNtpFracPerMs;

  DISALLOW_COPY_AND_ASSIGN(RtpStream);
};

class StreamGenerator {
 public:
  typedef std::list<RtpStream::RtcpPacket*> RtcpList;

  StreamGenerator(int capacity, double time_now);

  ~StreamGenerator();

  // Add a new stream.
  void AddStream(RtpStream* stream);

  // Set the link capacity.
  void set_capacity_bps(int capacity_bps);

  // Divides |bitrate_bps| among all streams. The allocated bitrate per stream
  // is decided by the initial allocation ratios.
  void set_bitrate_bps(int bitrate_bps);

  // Set the RTP timestamp offset for the stream identified by |ssrc|.
  void set_rtp_timestamp_offset(unsigned int ssrc, uint32_t offset);

  // TODO(holmer): Break out the channel simulation part from this class to make
  // it possible to simulate different types of channels.
  double GenerateFrame(RtpStream::PacketList* packets, double time_now);

  void Rtcps(RtcpList* rtcps, double time_now) const;

 private:
  typedef std::map<unsigned int, RtpStream*> StreamMap;

  // Capacity of the simulated channel in bits per second.
  int capacity_;
  // The time when the last packet arrived.
  double prev_arrival_time_;
  // All streams being transmitted on this simulated channel.
  StreamMap streams_;

  DISALLOW_COPY_AND_ASSIGN(StreamGenerator);
};
}  // namespace testing

class RemoteBitrateEstimatorTest : public ::testing::Test {
 public:
  RemoteBitrateEstimatorTest();
  explicit RemoteBitrateEstimatorTest(bool align_streams);

 protected:
  virtual void SetUp();

  void AddDefaultStream();

  // Generates a frame of packets belonging to a stream at a given bitrate and
  // with a given ssrc. The stream is pushed through a very simple simulated
  // network, and is then given to the receive-side bandwidth estimator.
  // Returns true if an over-use was seen, false otherwise.
  // The StreamGenerator::updated() should be used to check for any changes in
  // target bitrate after the call to this function.
  bool GenerateAndProcessFrame(unsigned int ssrc, unsigned int bitrate_bps);

  // Run the bandwidth estimator with a stream of |number_of_frames| frames.
  // Can for instance be used to run the estimator for some time to get it
  // into a steady state.
  unsigned int SteadyStateRun(unsigned int ssrc,
                              int number_of_frames,
                              unsigned int start_bitrate,
                              unsigned int min_bitrate,
                              unsigned int max_bitrate);

  enum { kDefaultSsrc = 1 };

  double time_now_;  // Current time at the receiver.
  OverUseDetectorOptions over_use_detector_options_;
  scoped_ptr<RemoteBitrateEstimator> bitrate_estimator_;
  scoped_ptr<testing::TestBitrateObserver> bitrate_observer_;
  scoped_ptr<testing::StreamGenerator> stream_generator_;
  const bool align_streams_;

  DISALLOW_COPY_AND_ASSIGN(RemoteBitrateEstimatorTest);
};

class RemoteBitrateEstimatorTestAlign : public RemoteBitrateEstimatorTest {
 public:
  RemoteBitrateEstimatorTestAlign() : RemoteBitrateEstimatorTest(true) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(RemoteBitrateEstimatorTestAlign);
};
}  // namespace testing

#endif  // WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_REMOTE_BITRATE_ESTIMATOR_UNITTEST_HELPER_H_
