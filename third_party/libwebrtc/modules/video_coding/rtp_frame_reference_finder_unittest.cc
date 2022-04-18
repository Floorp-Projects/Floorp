/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "modules/video_coding/frame_object.h"
#include "modules/video_coding/packet_buffer.h"
#include "modules/video_coding/rtp_frame_reference_finder.h"
#include "rtc_base/random.h"
#include "rtc_base/ref_count.h"
#include "system_wrappers/include/clock.h"
#include "test/gtest.h"

namespace webrtc {
namespace video_coding {

namespace {
std::unique_ptr<RtpFrameObject> CreateFrame(
    uint16_t seq_num_start,
    uint16_t seq_num_end,
    bool keyframe,
    VideoCodecType codec,
    const RTPVideoTypeHeader& video_type_header) {
  RTPVideoHeader video_header;
  video_header.frame_type = keyframe ? VideoFrameType::kVideoFrameKey
                                     : VideoFrameType::kVideoFrameDelta;
  video_header.video_type_header = video_type_header;

  // clang-format off
  return std::make_unique<RtpFrameObject>(
      seq_num_start,
      seq_num_end,
      /*markerBit=*/true,
      /*times_nacked=*/0,
      /*first_packet_received_time=*/0,
      /*last_packet_received_time=*/0,
      /*rtp_timestamp=*/0,
      /*ntp_time_ms=*/0,
      VideoSendTiming(),
      /*payload_type=*/0,
      codec,
      kVideoRotation_0,
      VideoContentType::UNSPECIFIED,
      video_header,
      /*color_space=*/absl::nullopt,
      RtpPacketInfos(),
      EncodedImageBuffer::Create(/*size=*/0));
  // clang-format on
}
}  // namespace

class TestRtpFrameReferenceFinder : public ::testing::Test,
                                    public OnCompleteFrameCallback {
 protected:
  TestRtpFrameReferenceFinder()
      : rand_(0x8739211),
        reference_finder_(new RtpFrameReferenceFinder(this)),
        frames_from_callback_(FrameComp()) {}

  uint16_t Rand() { return rand_.Rand<uint16_t>(); }

  void OnCompleteFrame(std::unique_ptr<EncodedFrame> frame) override {
    int64_t pid = frame->id.picture_id;
    uint16_t sidx = frame->id.spatial_layer;
    auto frame_it = frames_from_callback_.find(std::make_pair(pid, sidx));
    if (frame_it != frames_from_callback_.end()) {
      ADD_FAILURE() << "Already received frame with (pid:sidx): (" << pid << ":"
                    << sidx << ")";
      return;
    }

    frames_from_callback_.insert(
        std::make_pair(std::make_pair(pid, sidx), std::move(frame)));
  }

  void InsertGeneric(uint16_t seq_num_start,
                     uint16_t seq_num_end,
                     bool keyframe) {
    std::unique_ptr<RtpFrameObject> frame =
        CreateFrame(seq_num_start, seq_num_end, keyframe, kVideoCodecGeneric,
                    RTPVideoTypeHeader());

    reference_finder_->ManageFrame(std::move(frame));
  }

  void InsertVp8(uint16_t seq_num_start,
                 uint16_t seq_num_end,
                 bool keyframe,
                 int32_t pid = kNoPictureId,
                 uint8_t tid = kNoTemporalIdx,
                 int32_t tl0 = kNoTl0PicIdx,
                 bool sync = false) {
    RTPVideoHeaderVP8 vp8_header{};
    vp8_header.pictureId = pid % (1 << 15);
    vp8_header.temporalIdx = tid;
    vp8_header.tl0PicIdx = tl0;
    vp8_header.layerSync = sync;

    std::unique_ptr<RtpFrameObject> frame = CreateFrame(
        seq_num_start, seq_num_end, keyframe, kVideoCodecVP8, vp8_header);

    reference_finder_->ManageFrame(std::move(frame));
  }

  void InsertH264(uint16_t seq_num_start, uint16_t seq_num_end, bool keyframe) {
    std::unique_ptr<RtpFrameObject> frame =
        CreateFrame(seq_num_start, seq_num_end, keyframe, kVideoCodecH264,
                    RTPVideoTypeHeader());
    reference_finder_->ManageFrame(std::move(frame));
  }

  // Check if a frame with picture id |pid| and spatial index |sidx| has been
  // delivered from the packet buffer, and if so, if it has the references
  // specified by |refs|.
  template <typename... T>
  void CheckReferences(int64_t picture_id_offset,
                       uint16_t sidx,
                       T... refs) const {
    int64_t pid = picture_id_offset;
    auto frame_it = frames_from_callback_.find(std::make_pair(pid, sidx));
    if (frame_it == frames_from_callback_.end()) {
      ADD_FAILURE() << "Could not find frame with (pid:sidx): (" << pid << ":"
                    << sidx << ")";
      return;
    }

    std::set<int64_t> actual_refs;
    for (uint8_t r = 0; r < frame_it->second->num_references; ++r)
      actual_refs.insert(frame_it->second->references[r]);

    std::set<int64_t> expected_refs;
    RefsToSet(&expected_refs, refs...);

    ASSERT_EQ(expected_refs, actual_refs);
  }

  template <typename... T>
  void CheckReferencesGeneric(int64_t pid, T... refs) const {
    CheckReferences(pid, 0, refs...);
  }

  template <typename... T>
  void CheckReferencesVp8(int64_t pid, T... refs) const {
    CheckReferences(pid, 0, refs...);
  }

  template <typename... T>
  void CheckReferencesH264(int64_t pid, T... refs) const {
    CheckReferences(pid, 0, refs...);
  }

  template <typename... T>
  void RefsToSet(std::set<int64_t>* m, int64_t ref, T... refs) const {
    m->insert(ref);
    RefsToSet(m, refs...);
  }

  void RefsToSet(std::set<int64_t>* m) const {}

  Random rand_;
  std::unique_ptr<RtpFrameReferenceFinder> reference_finder_;
  struct FrameComp {
    bool operator()(const std::pair<int64_t, uint8_t> f1,
                    const std::pair<int64_t, uint8_t> f2) const {
      if (f1.first == f2.first)
        return f1.second < f2.second;
      return f1.first < f2.first;
    }
  };
  std::
      map<std::pair<int64_t, uint8_t>, std::unique_ptr<EncodedFrame>, FrameComp>
          frames_from_callback_;
};

TEST_F(TestRtpFrameReferenceFinder, PaddingPackets) {
  uint16_t sn = Rand();

  InsertGeneric(sn, sn, true);
  InsertGeneric(sn + 2, sn + 2, false);
  EXPECT_EQ(1UL, frames_from_callback_.size());
  reference_finder_->PaddingReceived(sn + 1);
  EXPECT_EQ(2UL, frames_from_callback_.size());
}

TEST_F(TestRtpFrameReferenceFinder, PaddingPacketsReordered) {
  uint16_t sn = Rand();

  InsertGeneric(sn, sn, true);
  reference_finder_->PaddingReceived(sn + 1);
  reference_finder_->PaddingReceived(sn + 4);
  InsertGeneric(sn + 2, sn + 3, false);

  EXPECT_EQ(2UL, frames_from_callback_.size());
  CheckReferencesGeneric(sn);
  CheckReferencesGeneric(sn + 3, sn + 0);
}

TEST_F(TestRtpFrameReferenceFinder, PaddingPacketsReorderedMultipleKeyframes) {
  uint16_t sn = Rand();

  InsertGeneric(sn, sn, true);
  reference_finder_->PaddingReceived(sn + 1);
  reference_finder_->PaddingReceived(sn + 4);
  InsertGeneric(sn + 2, sn + 3, false);
  InsertGeneric(sn + 5, sn + 5, true);
  reference_finder_->PaddingReceived(sn + 6);
  reference_finder_->PaddingReceived(sn + 9);
  InsertGeneric(sn + 7, sn + 8, false);

  EXPECT_EQ(4UL, frames_from_callback_.size());
}

TEST_F(TestRtpFrameReferenceFinder, AdvanceSavedKeyframe) {
  uint16_t sn = Rand();

  InsertGeneric(sn, sn, true);
  InsertGeneric(sn + 1, sn + 1, true);
  InsertGeneric(sn + 2, sn + 10000, false);
  InsertGeneric(sn + 10001, sn + 20000, false);
  InsertGeneric(sn + 20001, sn + 30000, false);
  InsertGeneric(sn + 30001, sn + 40000, false);

  EXPECT_EQ(6UL, frames_from_callback_.size());
}

TEST_F(TestRtpFrameReferenceFinder, ClearTo) {
  uint16_t sn = Rand();

  InsertGeneric(sn, sn + 1, true);
  InsertGeneric(sn + 4, sn + 5, false);  // stashed
  EXPECT_EQ(1UL, frames_from_callback_.size());

  InsertGeneric(sn + 6, sn + 7, true);  // keyframe
  EXPECT_EQ(2UL, frames_from_callback_.size());
  reference_finder_->ClearTo(sn + 7);

  InsertGeneric(sn + 8, sn + 9, false);  // first frame after keyframe.
  EXPECT_EQ(3UL, frames_from_callback_.size());

  InsertGeneric(sn + 2, sn + 3, false);  // late, cleared past this frame.
  EXPECT_EQ(3UL, frames_from_callback_.size());
}

TEST_F(TestRtpFrameReferenceFinder, Vp8NoPictureId) {
  uint16_t sn = Rand();

  InsertVp8(sn, sn + 2, true);
  ASSERT_EQ(1UL, frames_from_callback_.size());

  InsertVp8(sn + 3, sn + 4, false);
  ASSERT_EQ(2UL, frames_from_callback_.size());

  InsertVp8(sn + 5, sn + 8, false);
  ASSERT_EQ(3UL, frames_from_callback_.size());

  InsertVp8(sn + 9, sn + 9, false);
  ASSERT_EQ(4UL, frames_from_callback_.size());

  InsertVp8(sn + 10, sn + 11, false);
  ASSERT_EQ(5UL, frames_from_callback_.size());

  InsertVp8(sn + 12, sn + 12, true);
  ASSERT_EQ(6UL, frames_from_callback_.size());

  InsertVp8(sn + 13, sn + 17, false);
  ASSERT_EQ(7UL, frames_from_callback_.size());

  InsertVp8(sn + 18, sn + 18, false);
  ASSERT_EQ(8UL, frames_from_callback_.size());

  InsertVp8(sn + 19, sn + 20, false);
  ASSERT_EQ(9UL, frames_from_callback_.size());

  InsertVp8(sn + 21, sn + 21, false);

  ASSERT_EQ(10UL, frames_from_callback_.size());
  CheckReferencesVp8(sn + 2);
  CheckReferencesVp8(sn + 4, sn + 2);
  CheckReferencesVp8(sn + 8, sn + 4);
  CheckReferencesVp8(sn + 9, sn + 8);
  CheckReferencesVp8(sn + 11, sn + 9);
  CheckReferencesVp8(sn + 12);
  CheckReferencesVp8(sn + 17, sn + 12);
  CheckReferencesVp8(sn + 18, sn + 17);
  CheckReferencesVp8(sn + 20, sn + 18);
  CheckReferencesVp8(sn + 21, sn + 20);
}

TEST_F(TestRtpFrameReferenceFinder, Vp8NoPictureIdReordered) {
  uint16_t sn = 0xfffa;

  InsertVp8(sn, sn + 2, true);
  InsertVp8(sn + 3, sn + 4, false);
  InsertVp8(sn + 5, sn + 8, false);
  InsertVp8(sn + 9, sn + 9, false);
  InsertVp8(sn + 10, sn + 11, false);
  InsertVp8(sn + 12, sn + 12, true);
  InsertVp8(sn + 13, sn + 17, false);
  InsertVp8(sn + 18, sn + 18, false);
  InsertVp8(sn + 19, sn + 20, false);
  InsertVp8(sn + 21, sn + 21, false);

  ASSERT_EQ(10UL, frames_from_callback_.size());
  CheckReferencesVp8(sn + 2);
  CheckReferencesVp8(sn + 4, sn + 2);
  CheckReferencesVp8(sn + 8, sn + 4);
  CheckReferencesVp8(sn + 9, sn + 8);
  CheckReferencesVp8(sn + 11, sn + 9);
  CheckReferencesVp8(sn + 12);
  CheckReferencesVp8(sn + 17, sn + 12);
  CheckReferencesVp8(sn + 18, sn + 17);
  CheckReferencesVp8(sn + 20, sn + 18);
  CheckReferencesVp8(sn + 21, sn + 20);
}

TEST_F(TestRtpFrameReferenceFinder, Vp8KeyFrameReferences) {
  uint16_t sn = Rand();
  InsertVp8(sn, sn, true);

  ASSERT_EQ(1UL, frames_from_callback_.size());
  CheckReferencesVp8(sn);
}

TEST_F(TestRtpFrameReferenceFinder, Vp8RepeatedFrame_0) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();

  InsertVp8(sn, sn, true, pid, 0, 1);
  InsertVp8(sn + 1, sn + 1, false, pid + 1, 0, 2);
  InsertVp8(sn + 1, sn + 1, false, pid + 1, 0, 2);

  ASSERT_EQ(2UL, frames_from_callback_.size());
  CheckReferencesVp8(pid);
  CheckReferencesVp8(pid + 1, pid);
}

TEST_F(TestRtpFrameReferenceFinder, Vp8RepeatedFrameLayerSync_01) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();

  InsertVp8(sn, sn, true, pid, 0, 1);
  InsertVp8(sn + 1, sn + 1, false, pid + 1, 1, 1, true);
  ASSERT_EQ(2UL, frames_from_callback_.size());
  InsertVp8(sn + 1, sn + 1, false, pid + 1, 1, 1, true);

  ASSERT_EQ(2UL, frames_from_callback_.size());
  CheckReferencesVp8(pid);
  CheckReferencesVp8(pid + 1, pid);
}

TEST_F(TestRtpFrameReferenceFinder, Vp8RepeatedFrame_01) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();

  InsertVp8(sn, sn, true, pid, 0, 1);
  InsertVp8(sn + 1, sn + 1, false, pid + 1, 0, 2, true);
  InsertVp8(sn + 2, sn + 2, false, pid + 2, 0, 3);
  InsertVp8(sn + 3, sn + 3, false, pid + 3, 0, 4);
  InsertVp8(sn + 3, sn + 3, false, pid + 3, 0, 4);

  ASSERT_EQ(4UL, frames_from_callback_.size());
  CheckReferencesVp8(pid);
  CheckReferencesVp8(pid + 1, pid);
  CheckReferencesVp8(pid + 2, pid + 1);
  CheckReferencesVp8(pid + 3, pid + 2);
}

// Test with 1 temporal layer.
TEST_F(TestRtpFrameReferenceFinder, Vp8TemporalLayers_0) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();

  InsertVp8(sn, sn, true, pid, 0, 1);
  InsertVp8(sn + 1, sn + 1, false, pid + 1, 0, 2);
  InsertVp8(sn + 2, sn + 2, false, pid + 2, 0, 3);
  InsertVp8(sn + 3, sn + 3, false, pid + 3, 0, 4);

  ASSERT_EQ(4UL, frames_from_callback_.size());
  CheckReferencesVp8(pid);
  CheckReferencesVp8(pid + 1, pid);
  CheckReferencesVp8(pid + 2, pid + 1);
  CheckReferencesVp8(pid + 3, pid + 2);
}

TEST_F(TestRtpFrameReferenceFinder, Vp8DuplicateTl1Frames) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();

  InsertVp8(sn, sn, true, pid, 0, 0);
  InsertVp8(sn + 1, sn + 1, false, pid + 1, 1, 0, true);
  InsertVp8(sn + 2, sn + 2, false, pid + 2, 0, 1);
  InsertVp8(sn + 3, sn + 3, false, pid + 3, 1, 1);
  InsertVp8(sn + 3, sn + 3, false, pid + 3, 1, 1);
  InsertVp8(sn + 4, sn + 4, false, pid + 4, 0, 2);
  InsertVp8(sn + 5, sn + 5, false, pid + 5, 1, 2);

  ASSERT_EQ(6UL, frames_from_callback_.size());
  CheckReferencesVp8(pid);
  CheckReferencesVp8(pid + 1, pid);
  CheckReferencesVp8(pid + 2, pid);
  CheckReferencesVp8(pid + 3, pid + 1, pid + 2);
  CheckReferencesVp8(pid + 4, pid + 2);
  CheckReferencesVp8(pid + 5, pid + 3, pid + 4);
}

// Test with 1 temporal layer.
TEST_F(TestRtpFrameReferenceFinder, Vp8TemporalLayersReordering_0) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();

  InsertVp8(sn, sn, true, pid, 0, 1);
  InsertVp8(sn + 1, sn + 1, false, pid + 1, 0, 2);
  InsertVp8(sn + 3, sn + 3, false, pid + 3, 0, 4);
  InsertVp8(sn + 2, sn + 2, false, pid + 2, 0, 3);
  InsertVp8(sn + 5, sn + 5, false, pid + 5, 0, 6);
  InsertVp8(sn + 6, sn + 6, false, pid + 6, 0, 7);
  InsertVp8(sn + 4, sn + 4, false, pid + 4, 0, 5);

  ASSERT_EQ(7UL, frames_from_callback_.size());
  CheckReferencesVp8(pid);
  CheckReferencesVp8(pid + 1, pid);
  CheckReferencesVp8(pid + 2, pid + 1);
  CheckReferencesVp8(pid + 3, pid + 2);
  CheckReferencesVp8(pid + 4, pid + 3);
  CheckReferencesVp8(pid + 5, pid + 4);
  CheckReferencesVp8(pid + 6, pid + 5);
}

// Test with 2 temporal layers in a 01 pattern.
TEST_F(TestRtpFrameReferenceFinder, Vp8TemporalLayers_01) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();

  InsertVp8(sn, sn, true, pid, 0, 255);
  InsertVp8(sn + 1, sn + 1, false, pid + 1, 1, 255, true);
  InsertVp8(sn + 2, sn + 2, false, pid + 2, 0, 0);
  InsertVp8(sn + 3, sn + 3, false, pid + 3, 1, 0);

  ASSERT_EQ(4UL, frames_from_callback_.size());
  CheckReferencesVp8(pid);
  CheckReferencesVp8(pid + 1, pid);
  CheckReferencesVp8(pid + 2, pid);
  CheckReferencesVp8(pid + 3, pid + 1, pid + 2);
}

// Test with 2 temporal layers in a 01 pattern.
TEST_F(TestRtpFrameReferenceFinder, Vp8TemporalLayersReordering_01) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();

  InsertVp8(sn + 1, sn + 1, false, pid + 1, 1, 255, true);
  InsertVp8(sn, sn, true, pid, 0, 255);
  InsertVp8(sn + 3, sn + 3, false, pid + 3, 1, 0);
  InsertVp8(sn + 5, sn + 5, false, pid + 5, 1, 1);
  InsertVp8(sn + 2, sn + 2, false, pid + 2, 0, 0);
  InsertVp8(sn + 4, sn + 4, false, pid + 4, 0, 1);
  InsertVp8(sn + 6, sn + 6, false, pid + 6, 0, 2);
  InsertVp8(sn + 7, sn + 7, false, pid + 7, 1, 2);

  ASSERT_EQ(8UL, frames_from_callback_.size());
  CheckReferencesVp8(pid);
  CheckReferencesVp8(pid + 1, pid);
  CheckReferencesVp8(pid + 2, pid);
  CheckReferencesVp8(pid + 3, pid + 1, pid + 2);
  CheckReferencesVp8(pid + 4, pid + 2);
  CheckReferencesVp8(pid + 5, pid + 3, pid + 4);
  CheckReferencesVp8(pid + 6, pid + 4);
  CheckReferencesVp8(pid + 7, pid + 5, pid + 6);
}

// Test with 3 temporal layers in a 0212 pattern.
TEST_F(TestRtpFrameReferenceFinder, Vp8TemporalLayers_0212) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();

  InsertVp8(sn, sn, true, pid, 0, 55);
  InsertVp8(sn + 1, sn + 1, false, pid + 1, 2, 55, true);
  InsertVp8(sn + 2, sn + 2, false, pid + 2, 1, 55, true);
  InsertVp8(sn + 3, sn + 3, false, pid + 3, 2, 55);
  InsertVp8(sn + 4, sn + 4, false, pid + 4, 0, 56);
  InsertVp8(sn + 5, sn + 5, false, pid + 5, 2, 56);
  InsertVp8(sn + 6, sn + 6, false, pid + 6, 1, 56);
  InsertVp8(sn + 7, sn + 7, false, pid + 7, 2, 56);
  InsertVp8(sn + 8, sn + 8, false, pid + 8, 0, 57);
  InsertVp8(sn + 9, sn + 9, false, pid + 9, 2, 57, true);
  InsertVp8(sn + 10, sn + 10, false, pid + 10, 1, 57, true);
  InsertVp8(sn + 11, sn + 11, false, pid + 11, 2, 57);

  ASSERT_EQ(12UL, frames_from_callback_.size());
  CheckReferencesVp8(pid);
  CheckReferencesVp8(pid + 1, pid);
  CheckReferencesVp8(pid + 2, pid);
  CheckReferencesVp8(pid + 3, pid, pid + 1, pid + 2);
  CheckReferencesVp8(pid + 4, pid);
  CheckReferencesVp8(pid + 5, pid + 2, pid + 3, pid + 4);
  CheckReferencesVp8(pid + 6, pid + 2, pid + 4);
  CheckReferencesVp8(pid + 7, pid + 4, pid + 5, pid + 6);
  CheckReferencesVp8(pid + 8, pid + 4);
  CheckReferencesVp8(pid + 9, pid + 8);
  CheckReferencesVp8(pid + 10, pid + 8);
  CheckReferencesVp8(pid + 11, pid + 8, pid + 9, pid + 10);
}

// Test with 3 temporal layers in a 0212 pattern.
TEST_F(TestRtpFrameReferenceFinder, Vp8TemporalLayersMissingFrame_0212) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();

  InsertVp8(sn, sn, true, pid, 0, 55, false);
  InsertVp8(sn + 2, sn + 2, false, pid + 2, 1, 55, true);
  InsertVp8(sn + 3, sn + 3, false, pid + 3, 2, 55, false);

  ASSERT_EQ(2UL, frames_from_callback_.size());
  CheckReferencesVp8(pid);
  CheckReferencesVp8(pid + 2, pid);
}

// Test with 3 temporal layers in a 0212 pattern.
TEST_F(TestRtpFrameReferenceFinder, Vp8TemporalLayersReordering_0212) {
  uint16_t pid = 126;
  uint16_t sn = Rand();

  InsertVp8(sn + 1, sn + 1, false, pid + 1, 2, 55, true);
  InsertVp8(sn, sn, true, pid, 0, 55, false);
  InsertVp8(sn + 2, sn + 2, false, pid + 2, 1, 55, true);
  InsertVp8(sn + 4, sn + 4, false, pid + 4, 0, 56, false);
  InsertVp8(sn + 5, sn + 5, false, pid + 5, 2, 56, false);
  InsertVp8(sn + 3, sn + 3, false, pid + 3, 2, 55, false);
  InsertVp8(sn + 7, sn + 7, false, pid + 7, 2, 56, false);
  InsertVp8(sn + 9, sn + 9, false, pid + 9, 2, 57, true);
  InsertVp8(sn + 6, sn + 6, false, pid + 6, 1, 56, false);
  InsertVp8(sn + 8, sn + 8, false, pid + 8, 0, 57, false);
  InsertVp8(sn + 11, sn + 11, false, pid + 11, 2, 57, false);
  InsertVp8(sn + 10, sn + 10, false, pid + 10, 1, 57, true);

  ASSERT_EQ(12UL, frames_from_callback_.size());
  CheckReferencesVp8(pid);
  CheckReferencesVp8(pid + 1, pid);
  CheckReferencesVp8(pid + 2, pid);
  CheckReferencesVp8(pid + 3, pid, pid + 1, pid + 2);
  CheckReferencesVp8(pid + 4, pid);
  CheckReferencesVp8(pid + 5, pid + 2, pid + 3, pid + 4);
  CheckReferencesVp8(pid + 6, pid + 2, pid + 4);
  CheckReferencesVp8(pid + 7, pid + 4, pid + 5, pid + 6);
  CheckReferencesVp8(pid + 8, pid + 4);
  CheckReferencesVp8(pid + 9, pid + 8);
  CheckReferencesVp8(pid + 10, pid + 8);
  CheckReferencesVp8(pid + 11, pid + 8, pid + 9, pid + 10);
}

TEST_F(TestRtpFrameReferenceFinder, Vp8InsertManyFrames_0212) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();

  const int keyframes_to_insert = 50;
  const int frames_per_keyframe = 120;  // Should be a multiple of 4.
  uint8_t tl0 = 128;

  for (int k = 0; k < keyframes_to_insert; ++k) {
    InsertVp8(sn, sn, true, pid, 0, tl0, false);
    InsertVp8(sn + 1, sn + 1, false, pid + 1, 2, tl0, true);
    InsertVp8(sn + 2, sn + 2, false, pid + 2, 1, tl0, true);
    InsertVp8(sn + 3, sn + 3, false, pid + 3, 2, tl0, false);
    CheckReferencesVp8(pid);
    CheckReferencesVp8(pid + 1, pid);
    CheckReferencesVp8(pid + 2, pid);
    CheckReferencesVp8(pid + 3, pid, pid + 1, pid + 2);
    frames_from_callback_.clear();
    ++tl0;

    for (int f = 4; f < frames_per_keyframe; f += 4) {
      uint16_t sf = sn + f;
      int64_t pidf = pid + f;

      InsertVp8(sf, sf, false, pidf, 0, tl0, false);
      InsertVp8(sf + 1, sf + 1, false, pidf + 1, 2, tl0, false);
      InsertVp8(sf + 2, sf + 2, false, pidf + 2, 1, tl0, false);
      InsertVp8(sf + 3, sf + 3, false, pidf + 3, 2, tl0, false);
      CheckReferencesVp8(pidf, pidf - 4);
      CheckReferencesVp8(pidf + 1, pidf, pidf - 1, pidf - 2);
      CheckReferencesVp8(pidf + 2, pidf, pidf - 2);
      CheckReferencesVp8(pidf + 3, pidf, pidf + 1, pidf + 2);
      frames_from_callback_.clear();
      ++tl0;
    }

    pid += frames_per_keyframe;
    sn += frames_per_keyframe;
  }
}

TEST_F(TestRtpFrameReferenceFinder, Vp8LayerSync) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();

  InsertVp8(sn, sn, true, pid, 0, 0, false);
  InsertVp8(sn + 1, sn + 1, false, pid + 1, 1, 0, true);
  InsertVp8(sn + 2, sn + 2, false, pid + 2, 0, 1, false);
  ASSERT_EQ(3UL, frames_from_callback_.size());

  InsertVp8(sn + 4, sn + 4, false, pid + 4, 0, 2, false);
  InsertVp8(sn + 5, sn + 5, false, pid + 5, 1, 2, true);
  InsertVp8(sn + 6, sn + 6, false, pid + 6, 0, 3, false);
  InsertVp8(sn + 7, sn + 7, false, pid + 7, 1, 3, false);

  ASSERT_EQ(7UL, frames_from_callback_.size());
  CheckReferencesVp8(pid);
  CheckReferencesVp8(pid + 1, pid);
  CheckReferencesVp8(pid + 2, pid);
  CheckReferencesVp8(pid + 4, pid + 2);
  CheckReferencesVp8(pid + 5, pid + 4);
  CheckReferencesVp8(pid + 6, pid + 4);
  CheckReferencesVp8(pid + 7, pid + 6, pid + 5);
}

TEST_F(TestRtpFrameReferenceFinder, Vp8Tl1SyncFrameAfterTl1Frame) {
  InsertVp8(1000, 1000, true, 1, 0, 247, true);
  InsertVp8(1001, 1001, false, 3, 0, 248, false);
  InsertVp8(1002, 1002, false, 4, 1, 248, false);  // Will be dropped
  InsertVp8(1003, 1003, false, 5, 1, 248, true);   // due to this frame.

  ASSERT_EQ(3UL, frames_from_callback_.size());
  CheckReferencesVp8(1);
  CheckReferencesVp8(3, 1);
  CheckReferencesVp8(5, 3);
}

TEST_F(TestRtpFrameReferenceFinder, Vp8DetectMissingFrame_0212) {
  InsertVp8(1, 1, true, 1, 0, 1, false);
  InsertVp8(2, 2, false, 2, 2, 1, true);
  InsertVp8(3, 3, false, 3, 1, 1, true);
  InsertVp8(4, 4, false, 4, 2, 1, false);

  InsertVp8(6, 6, false, 6, 2, 2, false);
  InsertVp8(7, 7, false, 7, 1, 2, false);
  InsertVp8(8, 8, false, 8, 2, 2, false);
  ASSERT_EQ(4UL, frames_from_callback_.size());

  InsertVp8(5, 5, false, 5, 0, 2, false);
  ASSERT_EQ(8UL, frames_from_callback_.size());

  CheckReferencesVp8(1);
  CheckReferencesVp8(2, 1);
  CheckReferencesVp8(3, 1);
  CheckReferencesVp8(4, 3, 2, 1);

  CheckReferencesVp8(5, 1);
  CheckReferencesVp8(6, 5, 4, 3);
  CheckReferencesVp8(7, 5, 3);
  CheckReferencesVp8(8, 7, 6, 5);
}

TEST_F(TestRtpFrameReferenceFinder, H264KeyFrameReferences) {
  uint16_t sn = Rand();
  InsertH264(sn, sn, true);

  ASSERT_EQ(1UL, frames_from_callback_.size());
  CheckReferencesH264(sn);
}

TEST_F(TestRtpFrameReferenceFinder, H264SequenceNumberWrap) {
  uint16_t sn = 0xFFFF;

  InsertH264(sn - 1, sn - 1, true);
  InsertH264(sn, sn, false);
  InsertH264(sn + 1, sn + 1, false);
  InsertH264(sn + 2, sn + 2, false);

  ASSERT_EQ(4UL, frames_from_callback_.size());
  CheckReferencesH264(sn - 1);
  CheckReferencesH264(sn, sn - 1);
  CheckReferencesH264(sn + 1, sn);
  CheckReferencesH264(sn + 2, sn + 1);
}

TEST_F(TestRtpFrameReferenceFinder, H264Frames) {
  uint16_t sn = Rand();

  InsertH264(sn, sn, true);
  InsertH264(sn + 1, sn + 1, false);
  InsertH264(sn + 2, sn + 2, false);
  InsertH264(sn + 3, sn + 3, false);

  ASSERT_EQ(4UL, frames_from_callback_.size());
  CheckReferencesH264(sn);
  CheckReferencesH264(sn + 1, sn);
  CheckReferencesH264(sn + 2, sn + 1);
  CheckReferencesH264(sn + 3, sn + 2);
}

TEST_F(TestRtpFrameReferenceFinder, H264Reordering) {
  uint16_t sn = Rand();

  InsertH264(sn, sn, true);
  InsertH264(sn + 1, sn + 1, false);
  InsertH264(sn + 3, sn + 3, false);
  InsertH264(sn + 2, sn + 2, false);
  InsertH264(sn + 5, sn + 5, false);
  InsertH264(sn + 6, sn + 6, false);
  InsertH264(sn + 4, sn + 4, false);

  ASSERT_EQ(7UL, frames_from_callback_.size());
  CheckReferencesH264(sn);
  CheckReferencesH264(sn + 1, sn);
  CheckReferencesH264(sn + 2, sn + 1);
  CheckReferencesH264(sn + 3, sn + 2);
  CheckReferencesH264(sn + 4, sn + 3);
  CheckReferencesH264(sn + 5, sn + 4);
  CheckReferencesH264(sn + 6, sn + 5);
}

TEST_F(TestRtpFrameReferenceFinder, H264SequenceNumberWrapMulti) {
  uint16_t sn = 0xFFFF;

  InsertH264(sn - 3, sn - 2, true);
  InsertH264(sn - 1, sn + 1, false);
  InsertH264(sn + 2, sn + 3, false);
  InsertH264(sn + 4, sn + 7, false);

  ASSERT_EQ(4UL, frames_from_callback_.size());
  CheckReferencesH264(sn - 2);
  CheckReferencesH264(sn + 1, sn - 2);
  CheckReferencesH264(sn + 3, sn + 1);
  CheckReferencesH264(sn + 7, sn + 3);
}

TEST_F(TestRtpFrameReferenceFinder, Av1FrameNoDependencyDescriptor) {
  uint16_t sn = 0xFFFF;
  std::unique_ptr<RtpFrameObject> frame =
      CreateFrame(/*seq_num_start=*/sn, /*seq_num_end=*/sn, /*keyframe=*/true,
                  kVideoCodecAV1, RTPVideoTypeHeader());

  reference_finder_->ManageFrame(std::move(frame));

  ASSERT_EQ(1UL, frames_from_callback_.size());
  CheckReferencesGeneric(sn);
}

}  // namespace video_coding
}  // namespace webrtc
