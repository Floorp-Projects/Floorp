/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This test doesn't actually verify the output since it's just printed
// to stdout by void functions, but it's still useful as it executes the code.

#include <fstream>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/tools/frame_analyzer/video_quality_analysis.h"

namespace webrtc {
namespace test {


TEST(VideoQualityAnalysisTest, PrintAnalysisResultsEmpty) {
  ResultsContainer result;
  PrintAnalysisResults("Empty", &result);
}

TEST(VideoQualityAnalysisTest, PrintAnalysisResultsOneFrame) {
  ResultsContainer result;
  result.frames.push_back(AnalysisResult(0, 35.0, 0.9));
  PrintAnalysisResults("OneFrame", &result);
}

TEST(VideoQualityAnalysisTest, PrintAnalysisResultsThreeFrames) {
  ResultsContainer result;
  result.frames.push_back(AnalysisResult(0, 35.0, 0.9));
  result.frames.push_back(AnalysisResult(1, 34.0, 0.8));
  result.frames.push_back(AnalysisResult(2, 33.0, 0.7));
  PrintAnalysisResults("ThreeFrames", &result);
}

TEST(VideoQualityAnalysisTest, PrintMaxRepeatedAndSkippedFramesInvalidFile) {
  std::string stats_filename = OutputPath() + "non-existing-stats-file.txt";
  remove(stats_filename.c_str());
  PrintMaxRepeatedAndSkippedFrames("NonExistingStatsFile", stats_filename);
}

TEST(VideoQualityAnalysisTest, PrintMaxRepeatedAndSkippedFramesEmptyStatsFile) {
  std::string stats_filename = OutputPath() + "empty-stats.txt";
  std::ofstream stats_file;
  stats_file.open(stats_filename.c_str());
  stats_file.close();
  PrintMaxRepeatedAndSkippedFrames("EmptyStatsFile", stats_filename);
}

TEST(VideoQualityAnalysisTest, PrintMaxRepeatedAndSkippedFramesNormalFile) {
  std::string stats_filename = OutputPath() + "stats.txt";
  std::ofstream stats_file;
  stats_file.open(stats_filename.c_str());
  stats_file << "frame_0001 0100\n";
  stats_file << "frame_0002 0101\n";
  stats_file << "frame_0003 0101\n";
  stats_file << "frame_0004 0106\n";
  stats_file.close();

  PrintMaxRepeatedAndSkippedFrames("NormalStatsFile", stats_filename);
}


}  // namespace test
}  // namespace webrtc
