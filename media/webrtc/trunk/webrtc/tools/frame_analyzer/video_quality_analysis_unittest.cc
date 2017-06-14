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

#include <stdio.h>
#include <fstream>
#include <string>

#include "webrtc/test/gtest.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/tools/frame_analyzer/video_quality_analysis.h"

namespace webrtc {
namespace test {

// Setup a log file to write the output to instead of stdout because we don't
// want those numbers to be picked up as perf numbers.
class VideoQualityAnalysisTest : public ::testing::Test {
 protected:
  static void SetUpTestCase() {
    std::string log_filename = webrtc::test::OutputPath() +
        "VideoQualityAnalysisTest.log";
    logfile_ = fopen(log_filename.c_str(), "w");
    ASSERT_TRUE(logfile_ != NULL);
  }
  static void TearDownTestCase() {
    ASSERT_EQ(0, fclose(logfile_));
  }
  static FILE* logfile_;
};
FILE* VideoQualityAnalysisTest::logfile_ = NULL;

TEST_F(VideoQualityAnalysisTest, MatchExtractedY4mFrame) {
  std::string video_file =
         webrtc::test::ResourcePath("reference_less_video_test_file", "y4m");

  std::string extracted_frame_from_video_file =
         webrtc::test::ResourcePath("video_quality_analysis_frame", "txt");

  int frame_height = 720, frame_width = 1280;
  int frame_number = 2;
  int size = GetI420FrameSize(frame_width, frame_height);
  uint8_t* result_frame = new uint8_t[size];
  uint8_t* expected_frame = new uint8_t[size];

  FILE* input_file = fopen(extracted_frame_from_video_file.c_str(), "rb");
  fread(expected_frame, 1, size, input_file);

  ExtractFrameFromY4mFile(video_file.c_str(),
                          frame_width, frame_height,
                          frame_number, result_frame);

  EXPECT_EQ(*expected_frame, *result_frame);
  fclose(input_file);
  delete[] result_frame;
  delete[] expected_frame;
}

TEST_F(VideoQualityAnalysisTest, PrintAnalysisResultsEmpty) {
  ResultsContainer result;
  PrintAnalysisResults(logfile_, "Empty", &result);
}

TEST_F(VideoQualityAnalysisTest, PrintAnalysisResultsOneFrame) {
  ResultsContainer result;
  result.frames.push_back(AnalysisResult(0, 35.0, 0.9));
  PrintAnalysisResults(logfile_, "OneFrame", &result);
}

TEST_F(VideoQualityAnalysisTest, PrintAnalysisResultsThreeFrames) {
  ResultsContainer result;
  result.frames.push_back(AnalysisResult(0, 35.0, 0.9));
  result.frames.push_back(AnalysisResult(1, 34.0, 0.8));
  result.frames.push_back(AnalysisResult(2, 33.0, 0.7));
  PrintAnalysisResults(logfile_, "ThreeFrames", &result);
}

TEST_F(VideoQualityAnalysisTest, PrintMaxRepeatedAndSkippedFramesInvalidFile) {
  std::string stats_filename_ref =
      OutputPath() + "non-existing-stats-file-1.txt";
  std::string stats_filename = OutputPath() + "non-existing-stats-file-2.txt";
  remove(stats_filename.c_str());
  PrintMaxRepeatedAndSkippedFrames(logfile_, "NonExistingStatsFile",
                                   stats_filename_ref, stats_filename);
}

TEST_F(VideoQualityAnalysisTest,
       PrintMaxRepeatedAndSkippedFramesEmptyStatsFile) {
  std::string stats_filename_ref = OutputPath() + "empty-stats-1.txt";
  std::string stats_filename = OutputPath() + "empty-stats-2.txt";
  std::ofstream stats_file;
  stats_file.open(stats_filename_ref.c_str());
  stats_file.close();
  stats_file.open(stats_filename.c_str());
  stats_file.close();
  PrintMaxRepeatedAndSkippedFrames(logfile_, "EmptyStatsFile",
                                   stats_filename_ref, stats_filename);
}

TEST_F(VideoQualityAnalysisTest, PrintMaxRepeatedAndSkippedFramesNormalFile) {
  std::string stats_filename_ref = OutputPath() + "stats-1.txt";
  std::string stats_filename = OutputPath() + "stats-2.txt";
  std::ofstream stats_file;

  stats_file.open(stats_filename_ref.c_str());
  stats_file << "frame_0001 0100\n";
  stats_file << "frame_0002 0101\n";
  stats_file << "frame_0003 0102\n";
  stats_file << "frame_0004 0103\n";
  stats_file << "frame_0005 0106\n";
  stats_file << "frame_0006 0107\n";
  stats_file << "frame_0007 0108\n";
  stats_file.close();

  stats_file.open(stats_filename.c_str());
  stats_file << "frame_0001 0100\n";
  stats_file << "frame_0002 0101\n";
  stats_file << "frame_0003 0101\n";
  stats_file << "frame_0004 0106\n";
  stats_file.close();

  PrintMaxRepeatedAndSkippedFrames(logfile_, "NormalStatsFile",
                                   stats_filename_ref, stats_filename);
}


}  // namespace test
}  // namespace webrtc
