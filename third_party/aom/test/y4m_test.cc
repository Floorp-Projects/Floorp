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

#include <string>

#include "config/aom_config.h"

#include "common/y4menc.h"
#include "test/md5_helper.h"
#include "test/util.h"
#include "test/y4m_video_source.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace {

using std::string;

static const unsigned int kWidth = 160;
static const unsigned int kHeight = 90;
static const unsigned int kFrames = 10;

struct Y4mTestParam {
  const char *filename;
  unsigned int bit_depth;
  aom_img_fmt format;
  const char *md5raw;
};

const Y4mTestParam kY4mTestVectors[] = {
  { "park_joy_90p_8_420.y4m", 8, AOM_IMG_FMT_I420,
    "e5406275b9fc6bb3436c31d4a05c1cab" },
  { "park_joy_90p_8_420_monochrome.y4m", 8, AOM_IMG_FMT_I420,
    "95ef5bf6218580588be24a5271bb6a7f" },
  { "park_joy_90p_8_420_vertical_csp.y4m", 8, AOM_IMG_FMT_I420,
    "f53a40fec15254ac312527339d9c686b" },
  { "park_joy_90p_8_422.y4m", 8, AOM_IMG_FMT_I422,
    "284a47a47133b12884ec3a14e959a0b6" },
  { "park_joy_90p_8_444.y4m", 8, AOM_IMG_FMT_I444,
    "90517ff33843d85de712fd4fe60dbed0" },
  { "park_joy_90p_10_420.y4m", 10, AOM_IMG_FMT_I42016,
    "63f21f9f717d8b8631bd2288ee87137b" },
  { "park_joy_90p_10_422.y4m", 10, AOM_IMG_FMT_I42216,
    "48ab51fb540aed07f7ff5af130c9b605" },
  { "park_joy_90p_10_444.y4m", 10, AOM_IMG_FMT_I44416,
    "067bfd75aa85ff9bae91fa3e0edd1e3e" },
  { "park_joy_90p_12_420.y4m", 12, AOM_IMG_FMT_I42016,
    "9e6d8f6508c6e55625f6b697bc461cef" },
  { "park_joy_90p_12_422.y4m", 12, AOM_IMG_FMT_I42216,
    "b239c6b301c0b835485be349ca83a7e3" },
  { "park_joy_90p_12_444.y4m", 12, AOM_IMG_FMT_I44416,
    "5a6481a550821dab6d0192f5c63845e9" },
};

static const int PLANES_YUV[] = { AOM_PLANE_Y, AOM_PLANE_U, AOM_PLANE_V };

class Y4mVideoSourceTest : public ::testing::TestWithParam<Y4mTestParam>,
                           public ::libaom_test::Y4mVideoSource {
 protected:
  Y4mVideoSourceTest() : Y4mVideoSource("", 0, 0) {}

  virtual ~Y4mVideoSourceTest() { CloseSource(); }

  virtual void Init(const std::string &file_name, int limit) {
    file_name_ = file_name;
    start_ = 0;
    limit_ = limit;
    frame_ = 0;
    Begin();
  }

  // Checks y4m header information
  void HeaderChecks(unsigned int bit_depth, aom_img_fmt_t fmt) {
    ASSERT_TRUE(input_file_ != NULL);
    ASSERT_EQ(y4m_.pic_w, (int)kWidth);
    ASSERT_EQ(y4m_.pic_h, (int)kHeight);
    ASSERT_EQ(img()->d_w, kWidth);
    ASSERT_EQ(img()->d_h, kHeight);
    ASSERT_EQ(y4m_.bit_depth, bit_depth);
    ASSERT_EQ(y4m_.aom_fmt, fmt);
    if (fmt == AOM_IMG_FMT_I420 || fmt == AOM_IMG_FMT_I42016) {
      ASSERT_EQ(y4m_.bps, (int)y4m_.bit_depth * 3 / 2);
      ASSERT_EQ(img()->x_chroma_shift, 1U);
      ASSERT_EQ(img()->y_chroma_shift, 1U);
    }
    if (fmt == AOM_IMG_FMT_I422 || fmt == AOM_IMG_FMT_I42216) {
      ASSERT_EQ(y4m_.bps, (int)y4m_.bit_depth * 2);
      ASSERT_EQ(img()->x_chroma_shift, 1U);
      ASSERT_EQ(img()->y_chroma_shift, 0U);
    }
    if (fmt == AOM_IMG_FMT_I444 || fmt == AOM_IMG_FMT_I44416) {
      ASSERT_EQ(y4m_.bps, (int)y4m_.bit_depth * 3);
      ASSERT_EQ(img()->x_chroma_shift, 0U);
      ASSERT_EQ(img()->y_chroma_shift, 0U);
    }
  }

  // Checks MD5 of the raw frame data
  void Md5Check(const string &expected_md5) {
    ASSERT_TRUE(input_file_ != NULL);
    libaom_test::MD5 md5;
    for (unsigned int i = start_; i < limit_; i++) {
      md5.Add(img());
      Next();
    }
    ASSERT_EQ(string(md5.Get()), expected_md5);
  }
};

TEST_P(Y4mVideoSourceTest, SourceTest) {
  const Y4mTestParam t = GetParam();
  Init(t.filename, kFrames);
  HeaderChecks(t.bit_depth, t.format);
  Md5Check(t.md5raw);
}

INSTANTIATE_TEST_CASE_P(C, Y4mVideoSourceTest,
                        ::testing::ValuesIn(kY4mTestVectors));

class Y4mVideoWriteTest : public Y4mVideoSourceTest {
 protected:
  Y4mVideoWriteTest() : tmpfile_(NULL) {}

  virtual ~Y4mVideoWriteTest() {
    delete tmpfile_;
    input_file_ = NULL;
  }

  void ReplaceInputFile(FILE *input_file) {
    CloseSource();
    frame_ = 0;
    input_file_ = input_file;
    rewind(input_file_);
    ReadSourceToStart();
  }

  // Writes out a y4m file and then reads it back
  void WriteY4mAndReadBack() {
    ASSERT_TRUE(input_file_ != NULL);
    char buf[Y4M_BUFFER_SIZE] = { 0 };
    const struct AvxRational framerate = { y4m_.fps_n, y4m_.fps_d };
    tmpfile_ = new libaom_test::TempOutFile;
    ASSERT_TRUE(tmpfile_->file() != NULL);
    y4m_write_file_header(buf, sizeof(buf), kWidth, kHeight, &framerate,
                          img()->monochrome, img()->csp, y4m_.aom_fmt,
                          y4m_.bit_depth);
    fputs(buf, tmpfile_->file());
    for (unsigned int i = start_; i < limit_; i++) {
      y4m_write_frame_header(buf, sizeof(buf));
      fputs(buf, tmpfile_->file());
      y4m_write_image_file(img(), PLANES_YUV, tmpfile_->file());
      Next();
    }
    ReplaceInputFile(tmpfile_->file());
  }

  virtual void Init(const std::string &file_name, int limit) {
    Y4mVideoSourceTest::Init(file_name, limit);
    WriteY4mAndReadBack();
  }
  libaom_test::TempOutFile *tmpfile_;
};

TEST_P(Y4mVideoWriteTest, WriteTest) {
  const Y4mTestParam t = GetParam();
  Init(t.filename, kFrames);
  HeaderChecks(t.bit_depth, t.format);
  Md5Check(t.md5raw);
}

INSTANTIATE_TEST_CASE_P(C, Y4mVideoWriteTest,
                        ::testing::ValuesIn(kY4mTestVectors));
}  // namespace
