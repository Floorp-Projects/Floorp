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
#include <cstdlib>
#include <string>
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "./aom_config.h"
#include "test/acm_random.h"
#include "test/codec_factory.h"
#include "test/decode_test_driver.h"
#include "test/ivf_video_source.h"
#include "test/md5_helper.h"
#include "test/util.h"
#if CONFIG_WEBM_IO
#include "test/webm_video_source.h"
#endif
#include "aom_mem/aom_mem.h"
#include "aom/aom.h"

namespace {

using std::string;
using libaom_test::ACMRandom;

#if CONFIG_WEBM_IO

void CheckUserPrivateData(void *user_priv, int *target) {
  // actual pointer value should be the same as expected.
  EXPECT_EQ(reinterpret_cast<void *>(target), user_priv)
      << "user_priv pointer value does not match.";
}

// Decodes |filename|. Passes in user_priv data when calling DecodeFrame and
// compares the user_priv from return img with the original user_priv to see if
// they match. Both the pointer values and the values inside the addresses
// should match.
string DecodeFile(const string &filename) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  libaom_test::WebMVideoSource video(filename);
  video.Init();

  aom_codec_dec_cfg_t cfg = aom_codec_dec_cfg_t();
  cfg.allow_lowbitdepth = 1;
  libaom_test::AV1Decoder decoder(cfg, 0);

  libaom_test::MD5 md5;
  int frame_num = 0;
  for (video.Begin(); !::testing::Test::HasFailure() && video.cxdata();
       video.Next()) {
    void *user_priv = reinterpret_cast<void *>(&frame_num);
    const aom_codec_err_t res =
        decoder.DecodeFrame(video.cxdata(), video.frame_size(),
                            (frame_num == 0) ? NULL : user_priv);
    if (res != AOM_CODEC_OK) {
      EXPECT_EQ(AOM_CODEC_OK, res) << decoder.DecodeError();
      break;
    }
    libaom_test::DxDataIterator dec_iter = decoder.GetDxData();
    const aom_image_t *img = NULL;

    // Get decompressed data.
    while ((img = dec_iter.Next())) {
      if (frame_num == 0) {
        CheckUserPrivateData(img->user_priv, NULL);
      } else {
        CheckUserPrivateData(img->user_priv, &frame_num);

        // Also test ctrl_get_reference api.
        struct av1_ref_frame ref;
        // Randomly fetch a reference frame.
        ref.idx = rnd.Rand8() % 3;
        decoder.Control(AV1_GET_REFERENCE, &ref);

        CheckUserPrivateData(ref.img.user_priv, NULL);
      }
      md5.Add(img);
    }

    frame_num++;
  }
  return string(md5.Get());
}

TEST(UserPrivTest, VideoDecode) {
  // no tiles or frame parallel; this exercises the decoding to test the
  // user_priv.
  EXPECT_STREQ("b35a1b707b28e82be025d960aba039bc",
               DecodeFile("av10-2-03-size-226x226.webm").c_str());
}

#endif  // CONFIG_WEBM_IO

}  // namespace
