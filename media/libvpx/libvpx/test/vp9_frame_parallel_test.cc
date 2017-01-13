/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstdio>
#include <cstdlib>
#include <string>
#include "third_party/googletest/src/include/gtest/gtest.h"
#include "./vpx_config.h"
#include "test/codec_factory.h"
#include "test/decode_test_driver.h"
#include "test/ivf_video_source.h"
#include "test/md5_helper.h"
#include "test/util.h"
#if CONFIG_WEBM_IO
#include "test/webm_video_source.h"
#endif
#include "vpx_mem/vpx_mem.h"

namespace {

using std::string;

#if CONFIG_WEBM_IO

struct PauseFileList {
  const char *name;
  // md5 sum for decoded frames which does not include skipped frames.
  const char *expected_md5;
  const int pause_frame_num;
};

// Decodes |filename| with |num_threads|. Pause at the specified frame_num,
// seek to next key frame and then continue decoding until the end. Return
// the md5 of the decoded frames which does not include skipped frames.
string DecodeFileWithPause(const string &filename, int num_threads,
                           int pause_num) {
  libvpx_test::WebMVideoSource video(filename);
  video.Init();
  int in_frames = 0;
  int out_frames = 0;

  vpx_codec_dec_cfg_t cfg = {0};
  cfg.threads = num_threads;
  vpx_codec_flags_t flags = 0;
  flags |= VPX_CODEC_USE_FRAME_THREADING;
  libvpx_test::VP9Decoder decoder(cfg, flags, 0);

  libvpx_test::MD5 md5;
  video.Begin();

  do {
    ++in_frames;
    const vpx_codec_err_t res =
        decoder.DecodeFrame(video.cxdata(), video.frame_size());
    if (res != VPX_CODEC_OK) {
      EXPECT_EQ(VPX_CODEC_OK, res) << decoder.DecodeError();
      break;
    }

    // Pause at specified frame number.
    if (in_frames == pause_num) {
      // Flush the decoder and then seek to next key frame.
      decoder.DecodeFrame(NULL, 0);
      video.SeekToNextKeyFrame();
    } else {
      video.Next();
    }

    // Flush the decoder at the end of the video.
    if (!video.cxdata())
      decoder.DecodeFrame(NULL, 0);

    libvpx_test::DxDataIterator dec_iter = decoder.GetDxData();
    const vpx_image_t *img;

    // Get decompressed data
    while ((img = dec_iter.Next())) {
      ++out_frames;
      md5.Add(img);
    }
  } while (video.cxdata() != NULL);

  EXPECT_EQ(in_frames, out_frames) <<
      "Input frame count does not match output frame count";

  return string(md5.Get());
}

void DecodeFilesWithPause(const PauseFileList files[]) {
  for (const PauseFileList *iter = files; iter->name != NULL; ++iter) {
    SCOPED_TRACE(iter->name);
    for (int t = 2; t <= 8; ++t) {
      EXPECT_EQ(iter->expected_md5,
                DecodeFileWithPause(iter->name, t, iter->pause_frame_num))
          << "threads = " << t;
    }
  }
}

TEST(VP9MultiThreadedFrameParallel, PauseSeekResume) {
  // vp90-2-07-frame_parallel-1.webm is a 40 frame video file with
  // one key frame for every ten frames.
  static const PauseFileList files[] = {
    { "vp90-2-07-frame_parallel-1.webm",
      "6ea7c3875d67252e7caf2bc6e75b36b1", 6 },
    { "vp90-2-07-frame_parallel-1.webm",
      "4bb634160c7356a8d7d4299b6dc83a45", 12 },
    { "vp90-2-07-frame_parallel-1.webm",
      "89772591e6ef461f9fa754f916c78ed8", 26 },
    { NULL, NULL, 0 },
  };
  DecodeFilesWithPause(files);
}

struct FileList {
  const char *name;
  // md5 sum for decoded frames which does not include corrupted frames.
  const char *expected_md5;
  // Expected number of decoded frames which does not include corrupted frames.
  const int expected_frame_count;
};

// Decodes |filename| with |num_threads|. Return the md5 of the decoded
// frames which does not include corrupted frames.
string DecodeFile(const string &filename, int num_threads,
                  int expected_frame_count) {
  libvpx_test::WebMVideoSource video(filename);
  video.Init();

  vpx_codec_dec_cfg_t cfg = vpx_codec_dec_cfg_t();
  cfg.threads = num_threads;
  const vpx_codec_flags_t flags = VPX_CODEC_USE_FRAME_THREADING;
  libvpx_test::VP9Decoder decoder(cfg, flags, 0);

  libvpx_test::MD5 md5;
  video.Begin();

  int out_frames = 0;
  do {
    const vpx_codec_err_t res =
        decoder.DecodeFrame(video.cxdata(), video.frame_size());
    // TODO(hkuang): frame parallel mode should return an error on corruption.
    if (res != VPX_CODEC_OK) {
      EXPECT_EQ(VPX_CODEC_OK, res) << decoder.DecodeError();
      break;
    }

    video.Next();

    // Flush the decoder at the end of the video.
    if (!video.cxdata())
      decoder.DecodeFrame(NULL, 0);

    libvpx_test::DxDataIterator dec_iter = decoder.GetDxData();
    const vpx_image_t *img;

    // Get decompressed data
    while ((img = dec_iter.Next())) {
      ++out_frames;
      md5.Add(img);
    }
  } while (video.cxdata() != NULL);

  EXPECT_EQ(expected_frame_count, out_frames) <<
      "Input frame count does not match expected output frame count";

  return string(md5.Get());
}

void DecodeFiles(const FileList files[]) {
  for (const FileList *iter = files; iter->name != NULL; ++iter) {
    SCOPED_TRACE(iter->name);
    for (int t = 2; t <= 8; ++t) {
      EXPECT_EQ(iter->expected_md5,
                DecodeFile(iter->name, t, iter->expected_frame_count))
          << "threads = " << t;
    }
  }
}

TEST(VP9MultiThreadedFrameParallel, InvalidFileTest) {
  static const FileList files[] = {
    // invalid-vp90-2-07-frame_parallel-1.webm is a 40 frame video file with
    // one key frame for every ten frames. The 11th frame has corrupted data.
    { "invalid-vp90-2-07-frame_parallel-1.webm",
      "0549d0f45f60deaef8eb708e6c0eb6cb", 30 },
    // invalid-vp90-2-07-frame_parallel-2.webm is a 40 frame video file with
    // one key frame for every ten frames. The 1st and 31st frames have
    // corrupted data.
    { "invalid-vp90-2-07-frame_parallel-2.webm",
      "6a1f3cf6f9e7a364212fadb9580d525e", 20 },
    // invalid-vp90-2-07-frame_parallel-3.webm is a 40 frame video file with
    // one key frame for every ten frames. The 5th and 13th frames have
    // corrupted data.
    { "invalid-vp90-2-07-frame_parallel-3.webm",
      "8256544308de926b0681e04685b98677", 27 },
    { NULL, NULL, 0 },
  };
  DecodeFiles(files);
}

TEST(VP9MultiThreadedFrameParallel, ValidFileTest) {
  static const FileList files[] = {
#if CONFIG_VP9_HIGHBITDEPTH
    { "vp92-2-20-10bit-yuv420.webm",
      "a16b99df180c584e8db2ffeda987d293", 10 },
#endif
    { NULL, NULL, 0 },
  };
  DecodeFiles(files);
}
#endif  // CONFIG_WEBM_IO
}  // namespace
