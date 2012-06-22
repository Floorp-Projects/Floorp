/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_TEST_TEST_UTIL_H_
#define WEBRTC_MODULES_VIDEO_CODING_TEST_TEST_UTIL_H_

/*
 * General declarations used through out VCM offline tests.
 */

#include <string.h>
#include <fstream>
#include <cstdlib>

#include "module_common_types.h"
#include "testsupport/fileutils.h"

// Class used for passing command line arguments to tests
class CmdArgs
{
 public:
  CmdArgs()
      : codecName("VP8"),
        codecType(webrtc::kVideoCodecVP8),
        width(352),
        height(288),
        bitRate(500),
        frameRate(30),
        packetLoss(0),
        rtt(0),
        protectionMode(0),
        camaEnable(0),
        inputFile(webrtc::test::ProjectRootPath() +
                  "/resources/foreman_cif.yuv"),
        outputFile(webrtc::test::OutputPath() +
                   "video_coding_test_output_352x288.yuv"),
        testNum(11) {}
     std::string codecName;
     webrtc::VideoCodecType codecType;
     int width;
     int height;
     int bitRate;
     int frameRate;
     int packetLoss;
     int rtt;
     int protectionMode;
     int camaEnable;
     std::string inputFile;
     std::string outputFile;
     int testNum;
};

// forward declaration
int MTRxTxTest(CmdArgs& args);
double NormalDist(double mean, double stdDev);

struct RtpPacket {
  WebRtc_Word8 data[1650]; // max packet size
  WebRtc_Word32 length;
  WebRtc_Word64 receiveTime;
};


// Codec type conversion
webrtc::RTPVideoCodecTypes
ConvertCodecType(const char* plname);

#endif
