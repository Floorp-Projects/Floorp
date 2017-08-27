/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
*/

#include <algorithm>
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/util.h"
#include "test/y4m_video_source.h"

namespace {

// Check if any pixel in a 16x16 macroblock varies between frames.
int CheckMb(const aom_image_t &current, const aom_image_t &previous, int mb_r,
            int mb_c) {
  for (int plane = 0; plane < 3; plane++) {
    int r = 16 * mb_r;
    int c0 = 16 * mb_c;
    int r_top = std::min(r + 16, static_cast<int>(current.d_h));
    int c_top = std::min(c0 + 16, static_cast<int>(current.d_w));
    r = std::max(r, 0);
    c0 = std::max(c0, 0);
    if (plane > 0 && current.x_chroma_shift) {
      c_top = (c_top + 1) >> 1;
      c0 >>= 1;
    }
    if (plane > 0 && current.y_chroma_shift) {
      r_top = (r_top + 1) >> 1;
      r >>= 1;
    }
    for (; r < r_top; ++r) {
      for (int c = c0; c < c_top; ++c) {
        if (current.planes[plane][current.stride[plane] * r + c] !=
            previous.planes[plane][previous.stride[plane] * r + c])
          return 1;
      }
    }
  }
  return 0;
}

void GenerateMap(int mb_rows, int mb_cols, const aom_image_t &current,
                 const aom_image_t &previous, uint8_t *map) {
  for (int mb_r = 0; mb_r < mb_rows; ++mb_r) {
    for (int mb_c = 0; mb_c < mb_cols; ++mb_c) {
      map[mb_r * mb_cols + mb_c] = CheckMb(current, previous, mb_r, mb_c);
    }
  }
}

const int kAqModeCyclicRefresh = 3;

class ActiveMapRefreshTest
    : public ::libaom_test::CodecTestWith2Params<libaom_test::TestMode, int>,
      public ::libaom_test::EncoderTest {
 protected:
  ActiveMapRefreshTest() : EncoderTest(GET_PARAM(0)) {}
  virtual ~ActiveMapRefreshTest() {}

  virtual void SetUp() {
    InitializeConfig();
    SetMode(GET_PARAM(1));
    cpu_used_ = GET_PARAM(2);
  }

  virtual void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                                  ::libaom_test::Encoder *encoder) {
    ::libaom_test::Y4mVideoSource *y4m_video =
        static_cast<libaom_test::Y4mVideoSource *>(video);
    if (video->frame() == 1) {
      encoder->Control(AOME_SET_CPUUSED, cpu_used_);
      encoder->Control(AV1E_SET_AQ_MODE, kAqModeCyclicRefresh);
    } else if (video->frame() >= 2 && video->img()) {
      aom_image_t *current = video->img();
      aom_image_t *previous = y4m_holder_->img();
      ASSERT_TRUE(previous != NULL);
      aom_active_map_t map = aom_active_map_t();
      const int width = static_cast<int>(current->d_w);
      const int height = static_cast<int>(current->d_h);
      const int mb_width = (width + 15) / 16;
      const int mb_height = (height + 15) / 16;
      uint8_t *active_map = new uint8_t[mb_width * mb_height];
      GenerateMap(mb_height, mb_width, *current, *previous, active_map);
      map.cols = mb_width;
      map.rows = mb_height;
      map.active_map = active_map;
      encoder->Control(AOME_SET_ACTIVEMAP, &map);
      delete[] active_map;
    }
    if (video->img()) {
      y4m_video->SwapBuffers(y4m_holder_);
    }
  }

  int cpu_used_;
  ::libaom_test::Y4mVideoSource *y4m_holder_;
};

TEST_P(ActiveMapRefreshTest, Test) {
  cfg_.g_lag_in_frames = 0;
  cfg_.g_profile = 1;
  cfg_.rc_target_bitrate = 600;
  cfg_.rc_resize_mode = 0;
  cfg_.rc_min_quantizer = 8;
  cfg_.rc_max_quantizer = 30;
  cfg_.g_pass = AOM_RC_ONE_PASS;
  cfg_.rc_end_usage = AOM_CBR;
  cfg_.kf_max_dist = 90000;

  ::libaom_test::Y4mVideoSource video("desktop_credits.y4m", 0, 10);
  ::libaom_test::Y4mVideoSource video_holder("desktop_credits.y4m", 0, 10);
  video_holder.Begin();
  y4m_holder_ = &video_holder;

  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
}

AV1_INSTANTIATE_TEST_CASE(ActiveMapRefreshTest,
                          ::testing::Values(::libaom_test::kRealTime),
                          ::testing::Range(5, 6));
}  // namespace
