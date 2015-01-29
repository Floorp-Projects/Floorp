/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_RAMPUP_TESTS_H_
#define WEBRTC_VIDEO_RAMPUP_TESTS_H_

#include <map>
#include <string>
#include <vector>

#include "webrtc/call.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/test/call_test.h"
#include "webrtc/video/transport_adapter.h"

namespace webrtc {

static const int kTransmissionTimeOffsetExtensionId = 6;
static const int kAbsSendTimeExtensionId = 7;
static const unsigned int kSingleStreamTargetBps = 1000000;

class Clock;
class CriticalSectionWrapper;
class ReceiveStatistics;
class RtpHeaderParser;
class RTPPayloadRegistry;
class RtpRtcp;

class StreamObserver : public newapi::Transport, public RemoteBitrateObserver {
 public:
  typedef std::map<uint32_t, int> BytesSentMap;
  typedef std::map<uint32_t, uint32_t> SsrcMap;
  StreamObserver(const SsrcMap& rtx_media_ssrcs,
                 newapi::Transport* feedback_transport,
                 Clock* clock,
                 RemoteBitrateEstimatorFactory* rbe_factory,
                 RateControlType control_type);

  void set_expected_bitrate_bps(unsigned int expected_bitrate_bps);

  void set_start_bitrate_bps(unsigned int start_bitrate_bps);

  virtual void OnReceiveBitrateChanged(const std::vector<unsigned int>& ssrcs,
                                       unsigned int bitrate) OVERRIDE;

  virtual bool SendRtp(const uint8_t* packet, size_t length) OVERRIDE;

  virtual bool SendRtcp(const uint8_t* packet, size_t length) OVERRIDE;

  EventTypeWrapper Wait();

 private:
  void ReportResult(const std::string& measurement,
                    size_t value,
                    const std::string& units);
  void TriggerTestDone() EXCLUSIVE_LOCKS_REQUIRED(crit_);

  Clock* const clock_;
  const scoped_ptr<EventWrapper> test_done_;
  const scoped_ptr<RtpHeaderParser> rtp_parser_;
  scoped_ptr<RtpRtcp> rtp_rtcp_;
  internal::TransportAdapter feedback_transport_;
  const scoped_ptr<ReceiveStatistics> receive_stats_;
  const scoped_ptr<RTPPayloadRegistry> payload_registry_;
  scoped_ptr<RemoteBitrateEstimator> remote_bitrate_estimator_;

  const scoped_ptr<CriticalSectionWrapper> crit_;
  unsigned int expected_bitrate_bps_ GUARDED_BY(crit_);
  unsigned int start_bitrate_bps_ GUARDED_BY(crit_);
  SsrcMap rtx_media_ssrcs_ GUARDED_BY(crit_);
  size_t total_sent_ GUARDED_BY(crit_);
  size_t padding_sent_ GUARDED_BY(crit_);
  size_t rtx_media_sent_ GUARDED_BY(crit_);
  int total_packets_sent_ GUARDED_BY(crit_);
  int padding_packets_sent_ GUARDED_BY(crit_);
  int rtx_media_packets_sent_ GUARDED_BY(crit_);
  int64_t test_start_ms_ GUARDED_BY(crit_);
  int64_t ramp_up_finished_ms_ GUARDED_BY(crit_);
};

class LowRateStreamObserver : public test::DirectTransport,
                              public RemoteBitrateObserver,
                              public PacketReceiver {
 public:
  LowRateStreamObserver(newapi::Transport* feedback_transport,
                        Clock* clock,
                        size_t number_of_streams,
                        bool rtx_used);

  virtual void SetSendStream(const VideoSendStream* send_stream);

  virtual void OnReceiveBitrateChanged(const std::vector<unsigned int>& ssrcs,
                                       unsigned int bitrate);

  virtual bool SendRtp(const uint8_t* data, size_t length) OVERRIDE;

  virtual DeliveryStatus DeliverPacket(const uint8_t* packet,
                                       size_t length) OVERRIDE;

  virtual bool SendRtcp(const uint8_t* packet, size_t length) OVERRIDE;

  // Produces a string similar to "1stream_nortx", depending on the values of
  // number_of_streams_ and rtx_used_;
  std::string GetModifierString();

  // This method defines the state machine for the ramp up-down-up test.
  void EvolveTestState(unsigned int bitrate_bps);

  EventTypeWrapper Wait();

 private:
  static const unsigned int kHighBandwidthLimitBps = 80000;
  static const unsigned int kExpectedHighBitrateBps = 60000;
  static const unsigned int kLowBandwidthLimitBps = 20000;
  static const unsigned int kExpectedLowBitrateBps = 20000;
  enum TestStates { kFirstRampup, kLowRate, kSecondRampup };

  Clock* const clock_;
  const size_t number_of_streams_;
  const bool rtx_used_;
  const scoped_ptr<EventWrapper> test_done_;
  const scoped_ptr<RtpHeaderParser> rtp_parser_;
  scoped_ptr<RtpRtcp> rtp_rtcp_;
  internal::TransportAdapter feedback_transport_;
  const scoped_ptr<ReceiveStatistics> receive_stats_;
  scoped_ptr<RemoteBitrateEstimator> remote_bitrate_estimator_;

  scoped_ptr<CriticalSectionWrapper> crit_;
  const VideoSendStream* send_stream_ GUARDED_BY(crit_);
  FakeNetworkPipe::Config forward_transport_config_ GUARDED_BY(crit_);
  TestStates test_state_ GUARDED_BY(crit_);
  int64_t state_start_ms_ GUARDED_BY(crit_);
  int64_t interval_start_ms_ GUARDED_BY(crit_);
  unsigned int last_remb_bps_ GUARDED_BY(crit_);
  size_t sent_bytes_ GUARDED_BY(crit_);
  size_t total_overuse_bytes_ GUARDED_BY(crit_);
  bool suspended_in_stats_ GUARDED_BY(crit_);
};

class RampUpTest : public test::CallTest {
 protected:
  void RunRampUpTest(bool rtx,
                     size_t num_streams,
                     unsigned int start_bitrate_bps,
                     const std::string& extension_type);

  void RunRampUpDownUpTest(size_t number_of_streams, bool rtx);
};
}  // namespace webrtc
#endif  // WEBRTC_VIDEO_RAMPUP_TESTS_H_
