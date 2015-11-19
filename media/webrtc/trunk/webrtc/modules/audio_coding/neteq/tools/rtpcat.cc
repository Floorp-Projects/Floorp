/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>

#include "webrtc/base/checks.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/test/rtp_file_reader.h"
#include "webrtc/test/rtp_file_writer.h"

using rtc::scoped_ptr;
using webrtc::test::RtpFileReader;
using webrtc::test::RtpFileWriter;

int main(int argc, char* argv[]) {
  if (argc < 3) {
    printf("Concatenates multiple rtpdump files into one.\n");
    printf("Usage: rtpcat in1.rtp int2.rtp [...] out.rtp\n");
    exit(1);
  }

  scoped_ptr<RtpFileWriter> output(
      RtpFileWriter::Create(RtpFileWriter::kRtpDump, argv[argc - 1]));
  CHECK(output.get() != NULL) << "Cannot open output file.";
  printf("Output RTP file: %s\n", argv[argc - 1]);

  for (int i = 1; i < argc - 1; i++) {
    scoped_ptr<RtpFileReader> input(
        RtpFileReader::Create(RtpFileReader::kRtpDump, argv[i]));
    CHECK(input.get() != NULL) << "Cannot open input file " << argv[i];
    printf("Input RTP file: %s\n", argv[i]);

    webrtc::test::RtpPacket packet;
    while (input->NextPacket(&packet))
      CHECK(output->WritePacket(&packet));
  }
  return 0;
}
