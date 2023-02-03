/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_PC_E2E_METRIC_METADATA_KEYS_H_
#define TEST_PC_E2E_METRIC_METADATA_KEYS_H_

namespace webrtc {
namespace webrtc_pc_e2e {

class MetricMetadataKey {
 public:
  static constexpr char kPeerMetadataKey[] = "peer";
  static constexpr char kStreamMetadataKey[] = "stream";
  static constexpr char kReceiverMetadataKey[] = "receiver";

 private:
  MetricMetadataKey() = default;
};

class SampleMetadataKey {
 public:
  static constexpr char kFrameIdMetadataKey[] = "frame_id";

 private:
  SampleMetadataKey() = default;
};

}  // namespace webrtc_pc_e2e
}  // namespace webrtc

#endif  // TEST_PC_E2E_METRIC_METADATA_KEYS_H_
