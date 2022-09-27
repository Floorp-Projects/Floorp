/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_ANALYZER_VIDEO_DEFAULT_VIDEO_QUALITY_ANALYZER_STREAM_STATE_H_
#define TEST_PC_E2E_ANALYZER_VIDEO_DEFAULT_VIDEO_QUALITY_ANALYZER_STREAM_STATE_H_

#include <map>

#include "absl/types/optional.h"
#include "api/units/timestamp.h"
#include "test/pc/e2e/analyzer/video/multi_reader_queue.h"

namespace webrtc {

// Represents a current state of video stream inside
// DefaultVideoQualityAnalyzer.
class StreamState {
 public:
  StreamState(size_t owner,
              size_t peers_count,
              bool enable_receive_own_stream,
              Timestamp stream_started_time)
      : owner_(owner),
        enable_receive_own_stream_(enable_receive_own_stream),
        stream_started_time_(stream_started_time),
        frame_ids_(enable_receive_own_stream ? peers_count + 1 : peers_count) {}

  size_t owner() const { return owner_; }
  Timestamp stream_started_time() const { return stream_started_time_; }

  void PushBack(uint16_t frame_id) { frame_ids_.PushBack(frame_id); }
  // Crash if state is empty. Guarantees that there can be no alive frames
  // that are not in the owner queue
  uint16_t PopFront(size_t peer);
  bool IsEmpty(size_t peer) const {
    return frame_ids_.IsEmpty(GetPeerQueueIndex(peer));
  }
  // Crash if state is empty.
  uint16_t Front(size_t peer) const {
    return frame_ids_.Front(GetPeerQueueIndex(peer)).value();
  }

  // When new peer is added - all current alive frames will be sent to it as
  // well. So we need to register them as expected by copying owner_ head to
  // the new head.
  void AddPeer() { frame_ids_.AddReader(GetAliveFramesQueueIndex()); }

  size_t GetAliveFramesCount() const {
    return frame_ids_.size(GetAliveFramesQueueIndex());
  }
  uint16_t MarkNextAliveFrameAsDead();

  void SetLastRenderedFrameTime(size_t peer, Timestamp time);
  absl::optional<Timestamp> last_rendered_frame_time(size_t peer) const;

 private:
  // Returns index of the `frame_ids_` queue which is used for specified
  // `peer_index`.
  size_t GetPeerQueueIndex(size_t peer_index) const;

  // Returns index of the `frame_ids_` queue which is used to track alive
  // frames for this stream. The frame is alive if it contains VideoFrame
  // payload in `captured_frames_in_flight_`.
  size_t GetAliveFramesQueueIndex() const;

  // Index of the owner. Owner's queue in `frame_ids_` will keep alive frames.
  const size_t owner_;
  const bool enable_receive_own_stream_;
  const Timestamp stream_started_time_;
  // To correctly determine dropped frames we have to know sequence of frames
  // in each stream so we will keep a list of frame ids inside the stream.
  // This list is represented by multi head queue of frame ids with separate
  // head for each receiver. When the frame is rendered, we will pop ids from
  // the corresponding head until id will match with rendered one. All ids
  // before matched one can be considered as dropped:
  //
  // | frame_id1 |->| frame_id2 |->| frame_id3 |->| frame_id4 |
  //
  // If we received frame with id frame_id3, then we will pop frame_id1 and
  // frame_id2 and consider those frames as dropped and then compare received
  // frame with the one from `FrameInFlight` with id frame_id3.
  MultiReaderQueue<uint16_t> frame_ids_;
  std::map<size_t, Timestamp> last_rendered_frame_time_;
};

}  // namespace webrtc

#endif  // TEST_PC_E2E_ANALYZER_VIDEO_DEFAULT_VIDEO_QUALITY_ANALYZER_STREAM_STATE_H_
