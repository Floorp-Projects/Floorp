/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/pc/e2e/test_peer.h"

#include <string>
#include <utility>

#include "absl/memory/memory.h"
#include "api/scoped_refptr.h"
#include "modules/audio_processing/include/audio_processing.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

class SetRemoteDescriptionCallback
    : public webrtc::SetRemoteDescriptionObserverInterface {
 public:
  void OnSetRemoteDescriptionComplete(webrtc::RTCError error) override {
    is_called_ = true;
    error_ = error;
  }

  bool is_called() const { return is_called_; }

  webrtc::RTCError error() const { return error_; }

 private:
  bool is_called_ = false;
  webrtc::RTCError error_;
};

}  // namespace

bool TestPeer::SetRemoteDescription(
    std::unique_ptr<SessionDescriptionInterface> desc,
    std::string* error_out) {
  RTC_CHECK(wrapper_) << "TestPeer is already closed";

  auto observer = rtc::make_ref_counted<SetRemoteDescriptionCallback>();
  // We're assuming (and asserting) that the PeerConnection implementation of
  // SetRemoteDescription is synchronous when called on the signaling thread.
  pc()->SetRemoteDescription(std::move(desc), observer);
  RTC_CHECK(observer->is_called());
  if (!observer->error().ok()) {
    RTC_LOG(LS_ERROR) << *params_.name << ": Failed to set remote description: "
                      << observer->error().message();
    if (error_out) {
      *error_out = observer->error().message();
    }
  }
  return observer->error().ok();
}

bool TestPeer::AddIceCandidates(
    std::vector<std::unique_ptr<IceCandidateInterface>> candidates) {
  RTC_CHECK(wrapper_) << "TestPeer is already closed";
  bool success = true;
  for (auto& candidate : candidates) {
    if (!pc()->AddIceCandidate(candidate.get())) {
      std::string candidate_str;
      bool res = candidate->ToString(&candidate_str);
      RTC_CHECK(res);
      RTC_LOG(LS_ERROR) << "Failed to add ICE candidate, candidate_str="
                        << candidate_str;
      success = false;
    } else {
      remote_ice_candidates_.push_back(std::move(candidate));
    }
  }
  return success;
}

void TestPeer::Close() {
  wrapper_->pc()->Close();
  remote_ice_candidates_.clear();
  audio_processing_ = nullptr;
  video_sources_.clear();
  wrapper_ = nullptr;
  worker_thread_ = nullptr;
}

TestPeer::TestPeer(
    rtc::scoped_refptr<PeerConnectionFactoryInterface> pc_factory,
    rtc::scoped_refptr<PeerConnectionInterface> pc,
    std::unique_ptr<MockPeerConnectionObserver> observer,
    std::unique_ptr<Params> params,
    std::vector<PeerConfigurerImpl::VideoSource> video_sources,
    rtc::scoped_refptr<AudioProcessing> audio_processing,
    std::unique_ptr<rtc::Thread> worker_thread)
    : params_(std::move(*params)),
      worker_thread_(std::move(worker_thread)),
      wrapper_(std::make_unique<PeerConnectionWrapper>(std::move(pc_factory),
                                                       std::move(pc),
                                                       std::move(observer))),
      video_sources_(std::move(video_sources)),
      audio_processing_(audio_processing) {}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
