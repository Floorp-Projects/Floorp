/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_RTP_HEADER_EXTENSION_H_
#define WEBRTC_MODULES_RTP_RTCP_RTP_HEADER_EXTENSION_H_

#include <map>

#include "rtp_rtcp_defines.h"
#include "typedefs.h"

namespace webrtc {

enum {RTP_ONE_BYTE_HEADER_EXTENSION = 0xbede};

enum ExtensionLength {
   RTP_ONE_BYTE_HEADER_LENGTH_IN_BYTES = 4,
   TRANSMISSION_TIME_OFFSET_LENGTH_IN_BYTES = 4
};

struct HeaderExtension {
  HeaderExtension(RTPExtensionType extension_type)
    : type(extension_type),
      length(0) {
     if (type == kRtpExtensionTransmissionTimeOffset) {
       length = TRANSMISSION_TIME_OFFSET_LENGTH_IN_BYTES;
     }
   }

   const RTPExtensionType type;
   uint8_t length;
};

class RtpHeaderExtensionMap {
 public:
  RtpHeaderExtensionMap();
  ~RtpHeaderExtensionMap();

  void Erase();

  int32_t Register(const RTPExtensionType type, const uint8_t id);

  int32_t Deregister(const RTPExtensionType type);

  int32_t GetType(const uint8_t id, RTPExtensionType* type) const;

  int32_t GetId(const RTPExtensionType type, uint8_t* id) const;

  uint16_t GetTotalLengthInBytes() const;

  int32_t GetLengthUntilBlockStartInBytes(const RTPExtensionType type) const;

  void GetCopy(RtpHeaderExtensionMap* map) const;

  int32_t Size() const;

  RTPExtensionType First() const;

  RTPExtensionType Next(RTPExtensionType type) const;

 private:
  std::map<uint8_t, HeaderExtension*> extensionMap_;
};
}
#endif // WEBRTC_MODULES_RTP_RTCP_RTP_HEADER_EXTENSION_H_
