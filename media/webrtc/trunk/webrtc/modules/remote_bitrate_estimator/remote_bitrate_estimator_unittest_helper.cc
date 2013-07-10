/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/modules/remote_bitrate_estimator/remote_bitrate_estimator_unittest_helper.h"

#include <algorithm>
#include <utility>

namespace webrtc {
namespace testing {

RtpStream::RtpStream(int fps,
                     int bitrate_bps,
                     unsigned int ssrc,
                     unsigned int frequency,
                     uint32_t timestamp_offset,
                     int64_t rtcp_receive_time)
    : fps_(fps),
      bitrate_bps_(bitrate_bps),
      ssrc_(ssrc),
      frequency_(frequency),
      next_rtp_time_(0),
      next_rtcp_time_(rtcp_receive_time),
      rtp_timestamp_offset_(timestamp_offset),
      kNtpFracPerMs(4.294967296E6) {
  assert(fps_ > 0);
}

void RtpStream::set_rtp_timestamp_offset(uint32_t offset) {
  rtp_timestamp_offset_ = offset;
}

// Generates a new frame for this stream. If called too soon after the
// previous frame, no frame will be generated. The frame is split into
// packets.
int64_t RtpStream::GenerateFrame(int64_t time_now_us, PacketList* packets) {
  if (time_now_us < next_rtp_time_) {
    return next_rtp_time_;
  }
  assert(packets != NULL);
  int bits_per_frame = (bitrate_bps_ + fps_ / 2) / fps_;
  int n_packets = std::max((bits_per_frame + 4 * kMtu) / (8 * kMtu), 1);
  int packet_size = (bits_per_frame + 4 * n_packets) / (8 * n_packets);
  assert(n_packets >= 0);
  for (int i = 0; i < n_packets; ++i) {
    RtpPacket* packet = new RtpPacket;
    packet->send_time = time_now_us + kSendSideOffsetUs;
    packet->size = packet_size;
    packet->rtp_timestamp = rtp_timestamp_offset_ + static_cast<uint32_t>(
        ((frequency_ / 1000) * packet->send_time + 500) / 1000);
    packet->ssrc = ssrc_;
    packets->push_back(packet);
  }
  next_rtp_time_ = time_now_us + (1000000 + fps_ / 2) / fps_;
  return next_rtp_time_;
}

// The send-side time when the next frame can be generated.
double RtpStream::next_rtp_time() const {
  return next_rtp_time_;
}

// Generates an RTCP packet.
RtpStream::RtcpPacket* RtpStream::Rtcp(int64_t time_now_us) {
  if (time_now_us < next_rtcp_time_) {
    return NULL;
  }
  RtcpPacket* rtcp = new RtcpPacket;
  int64_t send_time_us = RtpStream::kSendSideOffsetUs + time_now_us;
  rtcp->timestamp = rtp_timestamp_offset_ + static_cast<uint32_t>(
          ((frequency_ / 1000) * send_time_us + 500) / 1000);
  rtcp->ntp_secs = send_time_us / 1000000;
  rtcp->ntp_frac = (send_time_us % 1000000) * kNtpFracPerMs;
  rtcp->ssrc = ssrc_;
  next_rtcp_time_ = time_now_us + kRtcpIntervalUs;
  return rtcp;
}

void RtpStream::set_bitrate_bps(int bitrate_bps) {
  ASSERT_GE(bitrate_bps, 0);
  bitrate_bps_ = bitrate_bps;
}

int RtpStream::bitrate_bps() const {
  return bitrate_bps_;
}

unsigned int RtpStream::ssrc() const {
  return ssrc_;
}

bool RtpStream::Compare(const std::pair<unsigned int, RtpStream*>& left,
                        const std::pair<unsigned int, RtpStream*>& right) {
  return left.second->next_rtp_time_ < right.second->next_rtp_time_;
}

StreamGenerator::StreamGenerator(int capacity, double time_now)
    : capacity_(capacity),
      prev_arrival_time_us_(time_now) {}

StreamGenerator::~StreamGenerator() {
  for (StreamMap::iterator it = streams_.begin(); it != streams_.end();
      ++it) {
    delete it->second;
  }
  streams_.clear();
}

// Add a new stream.
void StreamGenerator::AddStream(RtpStream* stream) {
  streams_[stream->ssrc()] = stream;
}

// Set the link capacity.
void StreamGenerator::set_capacity_bps(int capacity_bps) {
  ASSERT_GT(capacity_bps, 0);
  capacity_ = capacity_bps;
}

// Divides |bitrate_bps| among all streams. The allocated bitrate per stream
// is decided by the initial allocation ratios.
void StreamGenerator::set_bitrate_bps(int bitrate_bps) {
  ASSERT_GE(streams_.size(), 0u);
  double total_bitrate_before = 0;
  for (StreamMap::iterator it = streams_.begin(); it != streams_.end();
          ++it) {
    total_bitrate_before += it->second->bitrate_bps();
  }
  int total_bitrate_after = 0;
  for (StreamMap::iterator it = streams_.begin(); it != streams_.end();
      ++it) {
    double ratio = it->second->bitrate_bps() / total_bitrate_before;
    it->second->set_bitrate_bps(ratio * bitrate_bps + 0.5);
    total_bitrate_after += it->second->bitrate_bps();
  }
  EXPECT_NEAR(total_bitrate_after, bitrate_bps, 1);
}

// Set the RTP timestamp offset for the stream identified by |ssrc|.
void StreamGenerator::set_rtp_timestamp_offset(unsigned int ssrc,
                                               uint32_t offset) {
  streams_[ssrc]->set_rtp_timestamp_offset(offset);
}

// TODO(holmer): Break out the channel simulation part from this class to make
// it possible to simulate different types of channels.
int64_t StreamGenerator::GenerateFrame(RtpStream::PacketList* packets,
                                       int64_t time_now_us) {
  assert(packets != NULL);
  assert(packets->empty());
  assert(capacity_ > 0);
  StreamMap::iterator it = std::min_element(streams_.begin(), streams_.end(),
                                            RtpStream::Compare);
  (*it).second->GenerateFrame(time_now_us, packets);
  int i = 0;
  for (RtpStream::PacketList::iterator packet_it = packets->begin();
      packet_it != packets->end(); ++packet_it) {
    int capacity_bpus = capacity_ / 1000;
    int64_t required_network_time_us =
        (8 * 1000 * (*packet_it)->size + capacity_bpus / 2) / capacity_bpus;
    prev_arrival_time_us_ = std::max(time_now_us + required_network_time_us,
        prev_arrival_time_us_ + required_network_time_us);
    (*packet_it)->arrival_time = prev_arrival_time_us_;
    ++i;
  }
  it = std::min_element(streams_.begin(), streams_.end(), RtpStream::Compare);
  return (*it).second->next_rtp_time();
}

void StreamGenerator::Rtcps(RtcpList* rtcps, int64_t time_now_us) const {
  for (StreamMap::const_iterator it = streams_.begin(); it != streams_.end();
      ++it) {
    RtpStream::RtcpPacket* rtcp = it->second->Rtcp(time_now_us);
    if (rtcp) {
      rtcps->push_front(rtcp);
    }
  }
}
}  // namespace testing

RemoteBitrateEstimatorTest::RemoteBitrateEstimatorTest()
    : clock_(0),
      align_streams_(false) {}

RemoteBitrateEstimatorTest::RemoteBitrateEstimatorTest(bool align_streams)
    : clock_(0),
      align_streams_(align_streams) {}

void RemoteBitrateEstimatorTest::SetUp() {
  bitrate_observer_.reset(new testing::TestBitrateObserver);
  bitrate_estimator_.reset(
      RemoteBitrateEstimator::Create(
          overuse_detector_options_,
          RemoteBitrateEstimator::kSingleStreamEstimation,
          bitrate_observer_.get(),
          &clock_));
  stream_generator_.reset(new testing::StreamGenerator(
      1e6,  // Capacity.
      clock_.TimeInMicroseconds()));
}

void RemoteBitrateEstimatorTest::AddDefaultStream() {
  stream_generator_->AddStream(new testing::RtpStream(
    30,          // Frames per second.
    3e5,         // Bitrate.
    1,           // SSRC.
    90000,       // RTP frequency.
    0xFFFFF000,  // Timestamp offset.
    0));         // RTCP receive time.
}

// Generates a frame of packets belonging to a stream at a given bitrate and
// with a given ssrc. The stream is pushed through a very simple simulated
// network, and is then given to the receive-side bandwidth estimator.
// Returns true if an over-use was seen, false otherwise.
// The StreamGenerator::updated() should be used to check for any changes in
// target bitrate after the call to this function.
bool RemoteBitrateEstimatorTest::GenerateAndProcessFrame(unsigned int ssrc,
    unsigned int bitrate_bps) {
  stream_generator_->set_bitrate_bps(bitrate_bps);
  testing::RtpStream::PacketList packets;
  int64_t next_time_us = stream_generator_->GenerateFrame(
      &packets, clock_.TimeInMicroseconds());
  bool overuse = false;
  while (!packets.empty()) {
    testing::RtpStream::RtpPacket* packet = packets.front();
    if (align_streams_) {
      testing::StreamGenerator::RtcpList rtcps;
      stream_generator_->Rtcps(&rtcps, clock_.TimeInMicroseconds());
      for (testing::StreamGenerator::RtcpList::iterator it = rtcps.begin();
          it != rtcps.end(); ++it) {
        bitrate_estimator_->IncomingRtcp((*it)->ssrc,
                                         (*it)->ntp_secs,
                                         (*it)->ntp_frac,
                                         (*it)->timestamp);
        delete *it;
      }
    }
    bitrate_observer_->Reset();
    bitrate_estimator_->IncomingPacket(packet->ssrc,
                                       packet->size,
                                       (packet->arrival_time + 500) / 1000,
                                       packet->rtp_timestamp);
    if (bitrate_observer_->updated()) {
      // Verify that new estimates only are triggered by an overuse and a
      // rate decrease.
      overuse = true;
      EXPECT_LE(bitrate_observer_->latest_bitrate(), bitrate_bps);
    }
    clock_.AdvanceTimeMicroseconds(packet->arrival_time -
                                   clock_.TimeInMicroseconds());
    delete packet;
    packets.pop_front();
  }
  bitrate_estimator_->Process();
  clock_.AdvanceTimeMicroseconds(next_time_us - clock_.TimeInMicroseconds());
  return overuse;
}

// Run the bandwidth estimator with a stream of |number_of_frames| frames, or
// until it reaches |target_bitrate|.
// Can for instance be used to run the estimator for some time to get it
// into a steady state.
unsigned int RemoteBitrateEstimatorTest::SteadyStateRun(
    unsigned int ssrc,
    int max_number_of_frames,
    unsigned int start_bitrate,
    unsigned int min_bitrate,
    unsigned int max_bitrate,
    unsigned int target_bitrate) {
  unsigned int bitrate_bps = start_bitrate;
  bool bitrate_update_seen = false;
  // Produce |number_of_frames| frames and give them to the estimator.
  for (int i = 0; i < max_number_of_frames; ++i) {
    bool overuse = GenerateAndProcessFrame(ssrc, bitrate_bps);
    if (overuse) {
      EXPECT_LT(bitrate_observer_->latest_bitrate(), max_bitrate);
      EXPECT_GT(bitrate_observer_->latest_bitrate(), min_bitrate);
      bitrate_bps = bitrate_observer_->latest_bitrate();
      bitrate_update_seen = true;
    } else if (bitrate_observer_->updated()) {
      bitrate_bps = bitrate_observer_->latest_bitrate();
      bitrate_observer_->Reset();
    }
    if (bitrate_update_seen && bitrate_bps > target_bitrate) {
      break;
    }
  }
  EXPECT_TRUE(bitrate_update_seen);
  return bitrate_bps;
}
}  // namespace webrtc
