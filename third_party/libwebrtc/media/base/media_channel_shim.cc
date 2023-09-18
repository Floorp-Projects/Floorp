/*
 *  Copyright 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/base/media_channel_shim.h"

namespace cricket {

// Note: The VideoMediaChannel default implementations are not used here, and
// should be removed from that interface.
// TODO(bugs.webrtc.org/13931): Remove them.
VideoMediaShimChannel::VideoMediaShimChannel(
    std::unique_ptr<VideoMediaSendChannelInterface> send_impl,
    std::unique_ptr<VideoMediaReceiveChannelInterface> receive_impl)
    : VideoMediaChannel(MediaChannel::Role::kBoth, nullptr, false),
      send_impl_(std::move(send_impl)),
      receive_impl_(std::move(receive_impl)) {
  if (send_impl_ && receive_impl_) {
    send_impl_->SetSendCodecChangedCallback([this]() {
      // Adjust receive streams based on send codec.
      receive_impl_->SetReceiverFeedbackParameters(
          send_impl_->SendCodecHasLntf(), send_impl_->SendCodecHasNack(),
          send_impl_->SendCodecRtcpMode(), send_impl_->SendCodecRtxTime());
    });
    send_impl_->SetSsrcListChangedCallback(
        [this](const std::set<uint32_t>& choices) {
          receive_impl_->ChooseReceiverReportSsrc(choices);
        });
  }
}

}  // namespace cricket
