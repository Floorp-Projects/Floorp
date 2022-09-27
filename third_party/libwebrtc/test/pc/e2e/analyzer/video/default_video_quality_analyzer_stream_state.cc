/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer_stream_state.h"

#include <map>

#include "absl/types/optional.h"
#include "api/units/timestamp.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace {

template <typename T>
absl::optional<T> MaybeGetValue(const std::map<size_t, T>& map, size_t key) {
  auto it = map.find(key);
  if (it == map.end()) {
    return absl::nullopt;
  }
  return it->second;
}

}  // namespace

uint16_t StreamState::PopFront(size_t peer) {
  size_t peer_queue = GetPeerQueueIndex(peer);
  size_t alive_frames_queue = GetAliveFramesQueueIndex();
  absl::optional<uint16_t> frame_id = frame_ids_.PopFront(peer_queue);
  RTC_DCHECK(frame_id.has_value());

  // If alive's frame queue is longer than all others, than also pop frame from
  // it, because that frame is received by all receivers.
  size_t alive_size = frame_ids_.size(alive_frames_queue);
  size_t other_size = 0;
  for (size_t i = 0; i < frame_ids_.readers_count(); ++i) {
    size_t cur_size = frame_ids_.size(i);
    if (i != alive_frames_queue && cur_size > other_size) {
      other_size = cur_size;
    }
  }
  // Pops frame from alive queue if alive's queue is the longest one.
  if (alive_size > other_size) {
    absl::optional<uint16_t> alive_frame_id =
        frame_ids_.PopFront(alive_frames_queue);
    RTC_DCHECK(alive_frame_id.has_value());
    RTC_DCHECK_EQ(frame_id.value(), alive_frame_id.value());
  }

  return frame_id.value();
}

uint16_t StreamState::MarkNextAliveFrameAsDead() {
  absl::optional<uint16_t> frame_id =
      frame_ids_.PopFront(GetAliveFramesQueueIndex());
  RTC_DCHECK(frame_id.has_value());
  return frame_id.value();
}

void StreamState::SetLastRenderedFrameTime(size_t peer, Timestamp time) {
  auto it = last_rendered_frame_time_.find(peer);
  if (it == last_rendered_frame_time_.end()) {
    last_rendered_frame_time_.insert({peer, time});
  } else {
    it->second = time;
  }
}

absl::optional<Timestamp> StreamState::last_rendered_frame_time(
    size_t peer) const {
  return MaybeGetValue(last_rendered_frame_time_, peer);
}

size_t StreamState::GetPeerQueueIndex(size_t peer_index) const {
  // When sender isn't expecting to receive its own stream we will use their
  // queue for tracking alive frames. Otherwise we will use the queue #0 to
  // track alive frames and will shift all other queues for peers on 1.
  // It means when `enable_receive_own_stream_` is true peer's queue will have
  // index equal to `peer_index` + 1 and when `enable_receive_own_stream_` is
  // false peer's queue will have index equal to `peer_index`.
  if (!enable_receive_own_stream_) {
    return peer_index;
  }
  return peer_index + 1;
}

size_t StreamState::GetAliveFramesQueueIndex() const {
  // When sender isn't expecting to receive its own stream we will use their
  // queue for tracking alive frames. Otherwise we will use the queue #0 to
  // track alive frames and will shift all other queues for peers on 1.
  if (!enable_receive_own_stream_) {
    return owner_;
  }
  return 0;
}

}  // namespace webrtc
