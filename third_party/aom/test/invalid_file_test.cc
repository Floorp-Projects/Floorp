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

#include <cstdio>
#include <string>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/ivf_video_source.h"
#include "test/util.h"
#include "test/video_source.h"

namespace {

struct DecodeParam {
  int threads;
  const char *filename;
};

std::ostream &operator<<(std::ostream &os, const DecodeParam &dp) {
  return os << "threads: " << dp.threads << " file: " << dp.filename;
}

class InvalidFileTest : public ::libaom_test::DecoderTest,
                        public ::libaom_test::CodecTestWithParam<DecodeParam> {
 protected:
  InvalidFileTest() : DecoderTest(GET_PARAM(0)), res_file_(NULL) {}

  virtual ~InvalidFileTest() {
    if (res_file_ != NULL) fclose(res_file_);
  }

  void OpenResFile(const std::string &res_file_name) {
    res_file_ = libaom_test::OpenTestDataFile(res_file_name);
    ASSERT_TRUE(res_file_ != NULL)
        << "Result file open failed. Filename: " << res_file_name;
  }

  virtual bool HandleDecodeResult(
      const aom_codec_err_t res_dec,
      const libaom_test::CompressedVideoSource &video,
      libaom_test::Decoder *decoder) {
    EXPECT_TRUE(res_file_ != NULL);
    int expected_res_dec;

    // Read integer result.
    const int res = fscanf(res_file_, "%d", &expected_res_dec);
    EXPECT_NE(res, EOF) << "Read result data failed";

    // Check results match.
    const DecodeParam input = GET_PARAM(1);
    if (input.threads > 1) {
      // The serial decode check is too strict for tile-threaded decoding as
      // there is no guarantee on the decode order nor which specific error
      // will take precedence. Currently a tile-level error is not forwarded so
      // the frame will simply be marked corrupt.
      EXPECT_TRUE(res_dec == expected_res_dec ||
                  res_dec == AOM_CODEC_CORRUPT_FRAME)
          << "Results don't match: frame number = " << video.frame_number()
          << ". (" << decoder->DecodeError()
          << "). Expected: " << expected_res_dec << " or "
          << AOM_CODEC_CORRUPT_FRAME;
    } else {
      EXPECT_EQ(expected_res_dec, res_dec)
          << "Results don't match: frame number = " << video.frame_number()
          << ". (" << decoder->DecodeError() << ")";
    }

    return !HasFailure();
  }

  virtual void HandlePeekResult(libaom_test::Decoder *const /*decoder*/,
                                libaom_test::CompressedVideoSource * /*video*/,
                                const aom_codec_err_t /*res_peek*/) {}

  void RunTest() {
    const DecodeParam input = GET_PARAM(1);
    aom_codec_dec_cfg_t cfg = { 0, 0, 0, CONFIG_LOWBITDEPTH, { 1 } };
    cfg.threads = input.threads;
    const std::string filename = input.filename;
    libaom_test::IVFVideoSource decode_video(filename);
    decode_video.Init();

    // Construct result file name. The file holds a list of expected integer
    // results, one for each decoded frame.  Any result that doesn't match
    // the files list will cause a test failure.
    const std::string res_filename = filename + ".res";
    OpenResFile(res_filename);

    ASSERT_NO_FATAL_FAILURE(RunLoop(&decode_video, cfg));
  }

 private:
  FILE *res_file_;
};

TEST_P(InvalidFileTest, ReturnCode) { RunTest(); }

const DecodeParam kAV1InvalidFileTests[] = {
  { 1, "invalid-bug-1814.ivf" },
};

AV1_INSTANTIATE_TEST_CASE(InvalidFileTest,
                          ::testing::ValuesIn(kAV1InvalidFileTests));

}  // namespace
