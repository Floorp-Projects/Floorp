/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
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
#include <map>
#include <vector>

#include "tools/frame_analyzer/video_quality_analysis.h"
#include "tools/simple_command_line_parser.h"

/*
 * A command line tool running PSNR and SSIM on a reference video and a test
 * video. The test video is a record of the reference video which can start at
 * an arbitrary point. It is possible that there will be repeated frames or
 * skipped frames as well. In order to have a way to compare corresponding
 * frames from the two videos, a stats file should be provided. The stats file
 * is a text file assumed to be in the format:
 * frame_xxxx yyyy
 * where xxxx is the frame number in the test video and yyyy is the
 * corresponding frame number in the original video.
 * The video files should be 1420 YUV videos.
 * The tool prints the result to the standard output in the following format:
 * BSTATS
 * <psnr_value> <ssim_value>; <psnr_value> <ssim_value>; ....
 * ESTATS
 * Unique_frames_count:<value>
 * Max_repeated:<value>
 * Max_skipped<value>
 *
 * The max value for PSNR is 48.0 (between equal frames), as for SSIM it is 1.0.
 *
 * Usage:
 * frame_analyzer --reference_file=<name_of_file> --test_file=<name_of_file>
 * --stats_file=<name_of_file> --width=<frame_width> --height=<frame_height>
 */
int main(int argc, char** argv) {
  std::string program_name = argv[0];
  std::string usage = "Compares the output video with the initially sent video."
      "\nExample usage:\n" + program_name + " --stats_file=stats.txt "
      "--reference_file=ref.yuv --test_file=test.yuv --width=320 --height=240\n"
      "Command line flags:\n"
      "  - width(int): The width of the reference and test files. Default: -1\n"
      "  - height(int): The height of the reference and test files. "
      " Default: -1\n"
      "  - stats_file(string): The full name of the file containing the stats"
      " after decoding of the received YUV video. Default: stats.txt\n"
      "  - reference_file(string): The reference YUV file to compare against."
      " Default: ref.yuv\n"
      "  - test_file(string): The test YUV file to run the analysis for."
      " Default: test_file.yuv\n";

  webrtc::test::CommandLineParser parser;

  // Init the parser and set the usage message
  parser.Init(argc, argv);
  parser.SetUsageMessage(usage);

  parser.SetFlag("width", "-1");
  parser.SetFlag("height", "-1");
  parser.SetFlag("stats_file", "stats.txt");
  parser.SetFlag("reference_file", "ref.yuv");
  parser.SetFlag("test_file", "test.yuv");
  parser.SetFlag("help", "false");

  parser.ProcessFlags();
  if (parser.GetFlag("help") == "true") {
    parser.PrintUsageMessage();
  }
  parser.PrintEnteredFlags();

  int width = strtol((parser.GetFlag("width")).c_str(), NULL, 10);
  int height = strtol((parser.GetFlag("height")).c_str(), NULL, 10);

  if (width <= 0 || height <= 0) {
    fprintf(stderr, "Error: width or height cannot be <= 0!\n");
    return -1;
  }

  webrtc::test::ResultsContainer results;

  webrtc::test::RunAnalysis(parser.GetFlag("reference_file").c_str(),
                            parser.GetFlag("test_file").c_str(),
                            parser.GetFlag("stats_file").c_str(), width, height,
                            &results);

  webrtc::test::PrintAnalysisResults(&results);
  webrtc::test::PrintMaxRepeatedAndSkippedFrames(
      parser.GetFlag("stats_file").c_str());
}
