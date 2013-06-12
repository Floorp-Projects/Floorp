/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_TEST_PCAP_FILE_READER_H_
#define WEBRTC_MODULES_VIDEO_CODING_TEST_PCAP_FILE_READER_H_

#include <string>

namespace webrtc {
namespace rtpplayer {

class RtpPacketSourceInterface;

RtpPacketSourceInterface* CreatePcapFileReader(const std::string& filename);

}  // namespace rtpplayer
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_TEST_PCAP_FILE_READER_H_
