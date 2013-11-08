/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include <stdlib.h>
#include <string.h>

#include "gflags/gflags.h"
#include "webrtc/modules/video_coding/main/interface/video_coding.h"
#include "webrtc/modules/video_coding/main/test/codec_database_test.h"
#include "webrtc/modules/video_coding/main/test/generic_codec_test.h"
#include "webrtc/modules/video_coding/main/test/media_opt_test.h"
#include "webrtc/modules/video_coding/main/test/normal_test.h"
#include "webrtc/modules/video_coding/main/test/quality_modes_test.h"
#include "webrtc/modules/video_coding/main/test/receiver_tests.h"
#include "webrtc/modules/video_coding/main/test/test_util.h"
#include "webrtc/test/testsupport/fileutils.h"

DEFINE_string(codec, "VP8", "Codec to use (VP8 or I420).");
DEFINE_int32(width, 352, "Width in pixels of the frames in the input file.");
DEFINE_int32(height, 288, "Height in pixels of the frames in the input file.");
DEFINE_int32(bitrate, 500, "Bit rate in kilobits/second.");
DEFINE_int32(framerate, 30, "Frame rate of the input file, in FPS "
             "(frames-per-second). ");
DEFINE_int32(packet_loss_percent, 0, "Packet loss probability, in percent.");
DEFINE_int32(rtt, 0, "RTT (round-trip time), in milliseconds.");
DEFINE_int32(protection_mode, 0, "Protection mode.");
DEFINE_int32(cama_enable, 0, "Cama enable.");
DEFINE_string(input_filename, webrtc::test::ProjectRootPath() +
              "/resources/foreman_cif.yuv", "Input file.");
DEFINE_string(output_filename, webrtc::test::OutputPath() +
              "video_coding_test_output_352x288.yuv", "Output file.");
DEFINE_string(fv_output_filename, webrtc::test::OutputPath() +
              "features.txt", "FV output file.");
DEFINE_int32(test_number, 0, "Test number.");

using namespace webrtc;

/*
 * Build with EVENT_DEBUG defined
 * to build the tests with simulated events.
 */

int vcmMacrosTests = 0;
int vcmMacrosErrors = 0;

int ParseArguments(CmdArgs& args) {
  args.width = FLAGS_width;
  args.height = FLAGS_height;
  args.bitRate = FLAGS_bitrate;
  args.frameRate = FLAGS_framerate;
  if (args.width  < 1 || args.height < 1 || args.bitRate < 1 ||
      args.frameRate < 1) {
    return -1;
  }
  args.codecName = FLAGS_codec;
  if (args.codecName == "VP8") {
    args.codecType = kVideoCodecVP8;
  } else if (args.codecName == "I420") {
    args.codecType = kVideoCodecI420;
  } else {
    printf("Invalid codec: %s\n", args.codecName.c_str());
    return -1;
  }
  args.inputFile = FLAGS_input_filename;
  args.outputFile = FLAGS_output_filename;
  args.testNum = FLAGS_test_number;
  args.packetLoss = FLAGS_packet_loss_percent;
  args.rtt = FLAGS_rtt;
  args.protectionMode = FLAGS_protection_mode;
  args.camaEnable = FLAGS_cama_enable;
  args.fv_outputfile = FLAGS_fv_output_filename;
  return 0;
}

int main(int argc, char **argv) {
  // Initialize WebRTC fileutils.h so paths to resources can be resolved.
  webrtc::test::SetExecutablePath(argv[0]);
  google::ParseCommandLineFlags(&argc, &argv, true);

  CmdArgs args;
  if (ParseArguments(args) != 0) {
    printf("Unable to parse input arguments\n");
    return -1;
  }

  printf("Running video coding tests...\n");
  int ret = 0;
  switch (args.testNum) {
    case 0:
      ret = NormalTest::RunTest(args);
      ret |= CodecDataBaseTest::RunTest(args);
      break;
    case 1:
      ret = NormalTest::RunTest(args);
      break;
    case 2:
      ret = MTRxTxTest(args);
      break;
    case 3:
      ret = GenericCodecTest::RunTest(args);
      break;
    case 4:
      ret = CodecDataBaseTest::RunTest(args);
      break;
    case 5:
      // 0- normal, 1-Release test(50 runs) 2- from file
      ret = MediaOptTest::RunTest(0, args);
      break;
    case 7:
      ret = RtpPlay(args);
      break;
    case 8:
      ret = RtpPlayMT(args);
      break;
    case 9:
      qualityModeTest(args);
      break;
    default:
      ret = -1;
      break;
  }
  return ret;
}
