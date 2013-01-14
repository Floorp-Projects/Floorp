/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_VIDEO_ENGINE_TEST_AUTO_TEST_INTERFACE_VIE_COMPARISON_TESTS_H_
#define SRC_VIDEO_ENGINE_TEST_AUTO_TEST_INTERFACE_VIE_COMPARISON_TESTS_H_

#include <string>

#include "webrtc/video_engine/test/auto_test/primitives/general_primitives.h"

class FrameDropDetector;
struct NetworkParameters;
class ViEToFileRenderer;

// This class contains comparison tests, which will exercise video engine
// functionality and then run comparison tests on the result using PSNR and
// SSIM algorithms. These tests are intended mostly as sanity checks so that
// we know we are outputting roughly the right thing and not random noise or
// black screens.
//
// We will set up a fake ExternalCapture device which will pose as a webcam
// and read the input from the provided raw YUV file. Output will be written
// as a local preview in the local file renderer; the remote side output gets
// written to the provided remote file renderer.
//
// The local preview is a straight, unaltered copy of the input. This can be
// useful for comparisons if the test method contains several stages where the
// input is restarted between stages.
class ViEFileBasedComparisonTests {
 public:
  // Test a typical simple call setup. Returns false if the input file
  // could not be opened; reports errors using googletest macros otherwise.
  bool TestCallSetup(
      const std::string& i420_test_video_path,
      int width,
      int height,
      ViEToFileRenderer* local_file_renderer,
      ViEToFileRenderer* remote_file_renderer);

  // Runs a full stack test using the VP8 codec. Tests the full stack and uses
  // RTP timestamps to sync frames between the endpoints.
  void TestFullStack(
      const std::string& i420_video_file,
      int width,
      int height,
      int bit_rate_kbps,
      ProtectionMethod protection_method,
      const NetworkParameters& network,
      ViEToFileRenderer* local_file_renderer,
      ViEToFileRenderer* remote_file_renderer,
      FrameDropDetector* frame_drop_detector);
};

#endif  // SRC_VIDEO_ENGINE_TEST_AUTO_TEST_INTERFACE_VIE_COMPARISON_TESTS_H_
