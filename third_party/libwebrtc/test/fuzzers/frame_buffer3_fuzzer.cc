/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/array_view.h"
#include "api/video/encoded_frame.h"
#include "modules/video_coding/frame_buffer3.h"
#include "test/fuzzers/fuzz_data_helper.h"

namespace webrtc {
namespace {
class FuzzyFrameObject : public EncodedFrame {
 public:
  int64_t ReceivedTime() const override { return 0; }
  int64_t RenderTime() const override { return 0; }
};
}  // namespace

void FuzzOneInput(const uint8_t* data, size_t size) {
  if (size > 10000) {
    return;
  }

  FrameBuffer buffer(/*max_frame_slots=*/100, /*max_decode_history=*/1000);
  test::FuzzDataHelper helper(rtc::MakeArrayView(data, size));

  while (helper.BytesLeft() > 0) {
    int action = helper.ReadOrDefaultValue<uint8_t>(0) % 7;

    switch (action) {
      case 0: {
        buffer.LastContinuousFrameId();
        break;
      }
      case 1: {
        buffer.LastContinuousTemporalUnitFrameId();
        break;
      }
      case 2: {
        buffer.NextDecodableTemporalUnitRtpTimestamp();
        break;
      }
      case 3: {
        buffer.LastDecodableTemporalUnitRtpTimestamp();
        break;
      }
      case 4: {
        buffer.ExtractNextDecodableTemporalUnit();
        break;
      }
      case 5: {
        buffer.DropNextDecodableTemporalUnit();
        break;
      }
      case 6: {
        auto frame = std::make_unique<FuzzyFrameObject>();
        frame->SetTimestamp(helper.ReadOrDefaultValue<uint32_t>(0));
        frame->SetId(helper.ReadOrDefaultValue<int64_t>(0));
        frame->is_last_spatial_layer = helper.ReadOrDefaultValue<bool>(false);

        frame->num_references = helper.ReadOrDefaultValue<uint8_t>(0) %
                                EncodedFrame::kMaxFrameReferences;

        for (uint8_t i = 0; i < frame->num_references; ++i) {
          frame->references[i] = helper.ReadOrDefaultValue<int64_t>(0);
        }

        buffer.InsertFrame(std::move(frame));
        break;
      }
    }
  }
}

}  // namespace webrtc
