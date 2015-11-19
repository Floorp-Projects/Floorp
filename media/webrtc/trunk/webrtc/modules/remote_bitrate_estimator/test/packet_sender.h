/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_PACKET_SENDER_H_
#define WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_PACKET_SENDER_H_

#include <list>
#include <string>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/interface/module.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe_test_framework.h"

namespace webrtc {
namespace testing {
namespace bwe {

class PacketSender : public PacketProcessor, public BitrateObserver {
 public:
  PacketSender(PacketProcessorListener* listener,
               VideoSource* source,
               BandwidthEstimatorType estimator);
  virtual ~PacketSender();

  // Call GiveFeedback() with the returned interval in milliseconds, provided
  // there is a new estimate available.
  // Note that changing the feedback interval affects the timing of when the
  // output of the estimators is sampled and therefore the baseline files may
  // have to be regenerated.
  virtual int GetFeedbackIntervalMs() const;
  void RunFor(int64_t time_ms, Packets* in_out) override;

  virtual VideoSource* source() const { return source_; }

  // Implements BitrateObserver.
  void OnNetworkChanged(uint32_t target_bitrate_bps,
                        uint8_t fraction_lost,
                        int64_t rtt) override;

 protected:
  void ProcessFeedbackAndGeneratePackets(int64_t time_ms,
                                         std::list<FeedbackPacket*>* feedbacks,
                                         Packets* generated);
  std::list<FeedbackPacket*> GetFeedbackPackets(Packets* in_out,
                                                int64_t end_time_ms);

  SimulatedClock clock_;
  VideoSource* source_;
  rtc::scoped_ptr<BweSender> bwe_;
  int64_t start_of_run_ms_;
  std::list<Module*> modules_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PacketSender);
};

class PacedVideoSender : public PacketSender, public PacedSender::Callback {
 public:
  PacedVideoSender(PacketProcessorListener* listener,
                   VideoSource* source,
                   BandwidthEstimatorType estimator);
  virtual ~PacedVideoSender();

  void RunFor(int64_t time_ms, Packets* in_out) override;

  // Implements PacedSender::Callback.
  bool TimeToSendPacket(uint32_t ssrc,
                        uint16_t sequence_number,
                        int64_t capture_time_ms,
                        bool retransmission) override;
  size_t TimeToSendPadding(size_t bytes) override;

  // Implements BitrateObserver.
  void OnNetworkChanged(uint32_t target_bitrate_bps,
                        uint8_t fraction_lost,
                        int64_t rtt) override;

 private:
  int64_t TimeUntilNextProcess(const std::list<Module*>& modules);
  void CallProcess(const std::list<Module*>& modules);
  void QueuePackets(Packets* batch, int64_t end_of_batch_time_us);

  PacedSender pacer_;
  Packets queue_;
  Packets pacer_queue_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(PacedVideoSender);
};
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
#endif  // WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_PACKET_SENDER_H_
