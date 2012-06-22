/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include <stdio.h>
#include <vector>

#include "modules/audio_coding/neteq/test/NETEQTEST_RTPpacket.h"
#include "modules/audio_coding/neteq/test/NETEQTEST_DummyRTPpacket.h"

//#define WEBRTC_DUMMY_RTP

enum {
  kRedPayloadType = 127
};

int main(int argc, char* argv[]) {
  FILE* in_file = fopen(argv[1], "rb");
  if (!in_file) {
    printf("Cannot open input file %s\n", argv[1]);
    return -1;
  }
  printf("Input file: %s\n", argv[1]);

  FILE* out_file = fopen(argv[2], "wt");
  if (!out_file) {
    printf("Cannot open output file %s\n", argv[2]);
    return -1;
  }
  printf("Output file: %s\n\n", argv[2]);

  // Print file header.
  fprintf(out_file, "SeqNo  TimeStamp   SendTime  Size    PT  M\n");

  // Read file header.
  NETEQTEST_RTPpacket::skipFileHeader(in_file);
#ifdef WEBRTC_DUMMY_RTP
  NETEQTEST_DummyRTPpacket packet;
#else
  NETEQTEST_RTPpacket packet;
#endif

  while (packet.readFromFile(in_file) >= 0) {
    // Write packet data to file.
    fprintf(out_file, "%5u %10u %10u %5i %5i %2i\n",
            packet.sequenceNumber(), packet.timeStamp(), packet.time(),
            packet.dataLen(), packet.payloadType(), packet.markerBit());
    if (packet.payloadType() == kRedPayloadType) {
      WebRtcNetEQ_RTPInfo red_header;
      int len;
      int red_index = 0;
      while ((len = packet.extractRED(red_index++, red_header)) >= 0) {
        fprintf(out_file, "* %5u %10u %10u %5i %5i\n",
                red_header.sequenceNumber, red_header.timeStamp,
                packet.time(), len, red_header.payloadType);
      }
      assert(red_index > 1);  // We must get at least one payload.
    }
  }

  fclose(in_file);
  fclose(out_file);

  return 0;
}
