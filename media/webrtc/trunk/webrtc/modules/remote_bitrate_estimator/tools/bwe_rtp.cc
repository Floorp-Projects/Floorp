/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/remote_bitrate_estimator/tools/bwe_rtp.h"

#include <stdio.h>
#include <string>

#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_header_parser.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_payload_registry.h"
#include "webrtc/modules/video_coding/main/test/rtp_file_reader.h"
#include "webrtc/modules/video_coding/main/test/rtp_player.h"

using webrtc::rtpplayer::RtpPacketSourceInterface;

const int kMinBitrateBps = 30000;

bool ParseArgsAndSetupEstimator(int argc,
                                char** argv,
                                webrtc::Clock* clock,
                                webrtc::RemoteBitrateObserver* observer,
                                RtpPacketSourceInterface** rtp_reader,
                                webrtc::RtpHeaderParser** parser,
                                webrtc::RemoteBitrateEstimator** estimator,
                                std::string* estimator_used) {
  *rtp_reader = webrtc::rtpplayer::CreateRtpFileReader(argv[3]);
  if (!*rtp_reader) {
    printf("Cannot open input file %s\n", argv[3]);
    return false;
  }
  printf("Input file: %s\n\n", argv[3]);
  webrtc::RTPExtensionType extension = webrtc::kRtpExtensionAbsoluteSendTime;

  if (strncmp("tsoffset", argv[1], 8) == 0) {
    extension = webrtc::kRtpExtensionTransmissionTimeOffset;
    printf("Extension: toffset\n");
  } else {
    printf("Extension: abs\n");
  }
  int id = atoi(argv[2]);

  // Setup the RTP header parser and the bitrate estimator.
  *parser = webrtc::RtpHeaderParser::Create();
  (*parser)->RegisterRtpHeaderExtension(extension, id);
  if (estimator) {
    switch (extension) {
      case webrtc::kRtpExtensionAbsoluteSendTime: {
          webrtc::AbsoluteSendTimeRemoteBitrateEstimatorFactory factory;
          *estimator = factory.Create(observer, clock, kMinBitrateBps);
          *estimator_used = "AbsoluteSendTimeRemoteBitrateEstimator";
          break;
        }
      case webrtc::kRtpExtensionTransmissionTimeOffset: {
          webrtc::RemoteBitrateEstimatorFactory factory;
          *estimator = factory.Create(observer, clock, kMinBitrateBps);
          *estimator_used = "RemoteBitrateEstimator";
          break;
        }
      default:
        assert(false);
    }
  }
  return true;
}
