/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <climits>
#include <vector>
#include "third_party/googletest/src/include/gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/video_source.h"
#include "test/util.h"

// Enable(1) or Disable(0) writing of the compressed bitstream.
#define WRITE_COMPRESSED_STREAM 0

namespace {

#if WRITE_COMPRESSED_STREAM
static void mem_put_le16(char *const mem, const unsigned int val) {
  mem[0] = val;
  mem[1] = val >> 8;
}

static void mem_put_le32(char *const mem, const unsigned int val) {
  mem[0] = val;
  mem[1] = val >> 8;
  mem[2] = val >> 16;
  mem[3] = val >> 24;
}

static void write_ivf_file_header(const vpx_codec_enc_cfg_t *const cfg,
                                  int frame_cnt, FILE *const outfile) {
  char header[32];

  header[0] = 'D';
  header[1] = 'K';
  header[2] = 'I';
  header[3] = 'F';
  mem_put_le16(header + 4,  0);                   /* version */
  mem_put_le16(header + 6,  32);                  /* headersize */
  mem_put_le32(header + 8,  0x30395056);          /* fourcc (vp9) */
  mem_put_le16(header + 12, cfg->g_w);            /* width */
  mem_put_le16(header + 14, cfg->g_h);            /* height */
  mem_put_le32(header + 16, cfg->g_timebase.den); /* rate */
  mem_put_le32(header + 20, cfg->g_timebase.num); /* scale */
  mem_put_le32(header + 24, frame_cnt);           /* length */
  mem_put_le32(header + 28, 0);                   /* unused */

  (void)fwrite(header, 1, 32, outfile);
}

static void write_ivf_frame_size(FILE *const outfile, const size_t size) {
  char header[4];
  mem_put_le32(header, static_cast<unsigned int>(size));
  (void)fwrite(header, 1, 4, outfile);
}

static void write_ivf_frame_header(const vpx_codec_cx_pkt_t *const pkt,
                                   FILE *const outfile) {
  char header[12];
  vpx_codec_pts_t pts;

  if (pkt->kind != VPX_CODEC_CX_FRAME_PKT)
    return;

  pts = pkt->data.frame.pts;
  mem_put_le32(header, static_cast<unsigned int>(pkt->data.frame.sz));
  mem_put_le32(header + 4, pts & 0xFFFFFFFF);
  mem_put_le32(header + 8, pts >> 32);

  (void)fwrite(header, 1, 12, outfile);
}
#endif  // WRITE_COMPRESSED_STREAM

const unsigned int kInitialWidth = 320;
const unsigned int kInitialHeight = 240;

unsigned int ScaleForFrameNumber(unsigned int frame, unsigned int val) {
  if (frame < 10)
    return val;
  if (frame < 20)
    return val / 2;
  if (frame < 30)
    return val * 2 / 3;
  if (frame < 40)
    return val / 4;
  if (frame < 50)
    return val * 7 / 8;
  return val;
}

class ResizingVideoSource : public ::libvpx_test::DummyVideoSource {
 public:
  ResizingVideoSource() {
    SetSize(kInitialWidth, kInitialHeight);
    limit_ = 60;
  }

  virtual ~ResizingVideoSource() {}

 protected:
  virtual void Next() {
    ++frame_;
    SetSize(ScaleForFrameNumber(frame_, kInitialWidth),
            ScaleForFrameNumber(frame_, kInitialHeight));
    FillFrame();
  }
};

class ResizeTest : public ::libvpx_test::EncoderTest,
  public ::libvpx_test::CodecTestWithParam<libvpx_test::TestMode> {
 protected:
  ResizeTest() : EncoderTest(GET_PARAM(0)) {}

  virtual ~ResizeTest() {}

  struct FrameInfo {
    FrameInfo(vpx_codec_pts_t _pts, unsigned int _w, unsigned int _h)
        : pts(_pts), w(_w), h(_h) {}

    vpx_codec_pts_t pts;
    unsigned int w;
    unsigned int h;
  };

  virtual void SetUp() {
    InitializeConfig();
    SetMode(GET_PARAM(1));
  }

  virtual void DecompressedFrameHook(const vpx_image_t &img,
                                     vpx_codec_pts_t pts) {
    frame_info_list_.push_back(FrameInfo(pts, img.d_w, img.d_h));
  }

  std::vector< FrameInfo > frame_info_list_;
};

TEST_P(ResizeTest, TestExternalResizeWorks) {
  ResizingVideoSource video;
  cfg_.g_lag_in_frames = 0;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));

  for (std::vector<FrameInfo>::const_iterator info = frame_info_list_.begin();
       info != frame_info_list_.end(); ++info) {
    const unsigned int frame = static_cast<unsigned>(info->pts);
    const unsigned int expected_w = ScaleForFrameNumber(frame, kInitialWidth);
    const unsigned int expected_h = ScaleForFrameNumber(frame, kInitialHeight);

    EXPECT_EQ(expected_w, info->w)
        << "Frame " << frame << " had unexpected width";
    EXPECT_EQ(expected_h, info->h)
        << "Frame " << frame << " had unexpected height";
  }
}

const unsigned int kStepDownFrame = 3;
const unsigned int kStepUpFrame = 6;

class ResizeInternalTest : public ResizeTest {
 protected:
#if WRITE_COMPRESSED_STREAM
  ResizeInternalTest()
      : ResizeTest(),
        frame0_psnr_(0.0),
        outfile_(NULL),
        out_frames_(0) {}
#else
  ResizeInternalTest() : ResizeTest(), frame0_psnr_(0.0) {}
#endif

  virtual ~ResizeInternalTest() {}

  virtual void BeginPassHook(unsigned int /*pass*/) {
#if WRITE_COMPRESSED_STREAM
    outfile_ = fopen("vp90-2-05-resize.ivf", "wb");
#endif
  }

  virtual void EndPassHook() {
#if WRITE_COMPRESSED_STREAM
    if (outfile_) {
      if (!fseek(outfile_, 0, SEEK_SET))
        write_ivf_file_header(&cfg_, out_frames_, outfile_);
      fclose(outfile_);
      outfile_ = NULL;
    }
#endif
  }

  virtual void PreEncodeFrameHook(libvpx_test::VideoSource *video,
                                  libvpx_test::Encoder *encoder) {
    if (video->frame() == kStepDownFrame) {
      struct vpx_scaling_mode mode = {VP8E_FOURFIVE, VP8E_THREEFIVE};
      encoder->Control(VP8E_SET_SCALEMODE, &mode);
    }
    if (video->frame() == kStepUpFrame) {
      struct vpx_scaling_mode mode = {VP8E_NORMAL, VP8E_NORMAL};
      encoder->Control(VP8E_SET_SCALEMODE, &mode);
    }
  }

  virtual void PSNRPktHook(const vpx_codec_cx_pkt_t *pkt) {
    if (!frame0_psnr_)
      frame0_psnr_ = pkt->data.psnr.psnr[0];
    EXPECT_NEAR(pkt->data.psnr.psnr[0], frame0_psnr_, 2.0);
  }

#if WRITE_COMPRESSED_STREAM
  virtual void FramePktHook(const vpx_codec_cx_pkt_t *pkt) {
    ++out_frames_;

    // Write initial file header if first frame.
    if (pkt->data.frame.pts == 0)
      write_ivf_file_header(&cfg_, 0, outfile_);

    // Write frame header and data.
    write_ivf_frame_header(pkt, outfile_);
    (void)fwrite(pkt->data.frame.buf, 1, pkt->data.frame.sz, outfile_);
  }
#endif

  double frame0_psnr_;
#if WRITE_COMPRESSED_STREAM
  FILE *outfile_;
  unsigned int out_frames_;
#endif
};

TEST_P(ResizeInternalTest, TestInternalResizeWorks) {
  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 10);
  init_flags_ = VPX_CODEC_USE_PSNR;

  // q picked such that initial keyframe on this clip is ~30dB PSNR
  cfg_.rc_min_quantizer = cfg_.rc_max_quantizer = 48;

  // If the number of frames being encoded is smaller than g_lag_in_frames
  // the encoded frame is unavailable using the current API. Comparing
  // frames to detect mismatch would then not be possible. Set
  // g_lag_in_frames = 0 to get around this.
  cfg_.g_lag_in_frames = 0;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));

  for (std::vector<FrameInfo>::const_iterator info = frame_info_list_.begin();
       info != frame_info_list_.end(); ++info) {
    const vpx_codec_pts_t pts = info->pts;
    if (pts >= kStepDownFrame && pts < kStepUpFrame) {
      ASSERT_EQ(282U, info->w) << "Frame " << pts << " had unexpected width";
      ASSERT_EQ(173U, info->h) << "Frame " << pts << " had unexpected height";
    } else {
      EXPECT_EQ(352U, info->w) << "Frame " << pts << " had unexpected width";
      EXPECT_EQ(288U, info->h) << "Frame " << pts << " had unexpected height";
    }
  }
}

vpx_img_fmt_t CspForFrameNumber(int frame) {
  if (frame < 10)
    return VPX_IMG_FMT_I420;
  if (frame < 20)
    return VPX_IMG_FMT_I444;
  return VPX_IMG_FMT_I420;
}

class ResizeCspTest : public ResizeTest {
 protected:
#if WRITE_COMPRESSED_STREAM
  ResizeCspTest()
      : ResizeTest(),
        frame0_psnr_(0.0),
        outfile_(NULL),
        out_frames_(0) {}
#else
  ResizeCspTest() : ResizeTest(), frame0_psnr_(0.0) {}
#endif

  virtual ~ResizeCspTest() {}

  virtual void BeginPassHook(unsigned int /*pass*/) {
#if WRITE_COMPRESSED_STREAM
    outfile_ = fopen("vp91-2-05-cspchape.ivf", "wb");
#endif
  }

  virtual void EndPassHook() {
#if WRITE_COMPRESSED_STREAM
    if (outfile_) {
      if (!fseek(outfile_, 0, SEEK_SET))
        write_ivf_file_header(&cfg_, out_frames_, outfile_);
      fclose(outfile_);
      outfile_ = NULL;
    }
#endif
  }

  virtual void PreEncodeFrameHook(libvpx_test::VideoSource *video,
                                  libvpx_test::Encoder *encoder) {
    if (CspForFrameNumber(video->frame()) != VPX_IMG_FMT_I420 &&
        cfg_.g_profile != 1) {
      cfg_.g_profile = 1;
      encoder->Config(&cfg_);
    }
    if (CspForFrameNumber(video->frame()) == VPX_IMG_FMT_I420 &&
        cfg_.g_profile != 0) {
      cfg_.g_profile = 0;
      encoder->Config(&cfg_);
    }
  }

  virtual void PSNRPktHook(const vpx_codec_cx_pkt_t *pkt) {
    if (!frame0_psnr_)
      frame0_psnr_ = pkt->data.psnr.psnr[0];
    EXPECT_NEAR(pkt->data.psnr.psnr[0], frame0_psnr_, 2.0);
  }

#if WRITE_COMPRESSED_STREAM
  virtual void FramePktHook(const vpx_codec_cx_pkt_t *pkt) {
    ++out_frames_;

    // Write initial file header if first frame.
    if (pkt->data.frame.pts == 0)
      write_ivf_file_header(&cfg_, 0, outfile_);

    // Write frame header and data.
    write_ivf_frame_header(pkt, outfile_);
    (void)fwrite(pkt->data.frame.buf, 1, pkt->data.frame.sz, outfile_);
  }
#endif

  double frame0_psnr_;
#if WRITE_COMPRESSED_STREAM
  FILE *outfile_;
  unsigned int out_frames_;
#endif
};

class ResizingCspVideoSource : public ::libvpx_test::DummyVideoSource {
 public:
  ResizingCspVideoSource() {
    SetSize(kInitialWidth, kInitialHeight);
    limit_ = 30;
  }

  virtual ~ResizingCspVideoSource() {}

 protected:
  virtual void Next() {
    ++frame_;
    SetImageFormat(CspForFrameNumber(frame_));
    FillFrame();
  }
};

TEST_P(ResizeCspTest, TestResizeCspWorks) {
  ResizingCspVideoSource video;
  init_flags_ = VPX_CODEC_USE_PSNR;
  cfg_.rc_min_quantizer = cfg_.rc_max_quantizer = 48;
  cfg_.g_lag_in_frames = 0;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
}

VP8_INSTANTIATE_TEST_CASE(ResizeTest, ONE_PASS_TEST_MODES);
VP9_INSTANTIATE_TEST_CASE(ResizeTest,
                          ::testing::Values(::libvpx_test::kRealTime));
VP9_INSTANTIATE_TEST_CASE(ResizeInternalTest,
                          ::testing::Values(::libvpx_test::kOnePassBest));
VP9_INSTANTIATE_TEST_CASE(ResizeCspTest,
                          ::testing::Values(::libvpx_test::kRealTime));
}  // namespace
