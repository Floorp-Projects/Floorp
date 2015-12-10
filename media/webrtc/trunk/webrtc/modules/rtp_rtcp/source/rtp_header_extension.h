/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
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

#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/typedefs.h"

namespace webrtc {

const uint16_t kRtpOneByteHeaderExtensionId = 0xBEDE;

const size_t kRtpOneByteHeaderLength = 4;
const size_t kTransmissionTimeOffsetLength = 4;
const size_t kAudioLevelLength = 2;
const size_t kAbsoluteSendTimeLength = 4;
const size_t kVideoRotationLength = 2;
const size_t kTransportSequenceNumberLength = 3;
// kRIDLength is variable
const size_t kRIDLength = 4; // max 1-byte header extension length

struct HeaderExtension {
  HeaderExtension(RTPExtensionType extension_type)
      : type(extension_type), length(0), active(true) {
    Init();
  }

  HeaderExtension(RTPExtensionType extension_type, bool active)
      : type(extension_type), length(0), active(active) {
    Init();
  }

  void Init() {
    // TODO(solenberg): Create handler classes for header extensions so we can
    // get rid of switches like these as well as handling code spread out all
    // over.
    switch (type) {
      case kRtpExtensionTransmissionTimeOffset:
        length = kTransmissionTimeOffsetLength;
        break;
      case kRtpExtensionAudioLevel:
        length = kAudioLevelLength;
        break;
      case kRtpExtensionAbsoluteSendTime:
        length = kAbsoluteSendTimeLength;
        break;
      case kRtpExtensionVideoRotation:
        length = kVideoRotationLength;
        break;
      case kRtpExtensionTransportSequenceNumber:
        length = kTransportSequenceNumberLength;
        break;
      case kRtpExtensionRID:
        length = kRIDLength;
        break;
      default:
        assert(false);
    }
  }

  const RTPExtensionType type;
  uint8_t length;
  bool active;
};

class RtpHeaderExtensionMap {
 public:
  RtpHeaderExtensionMap();
  ~RtpHeaderExtensionMap();

  void Erase();

  int32_t Register(const RTPExtensionType type, const uint8_t id);

  // Active is a concept for a registered rtp header extension which doesn't
  // take effect yet until being activated. Inactive RTP header extensions do
  // not take effect and should not be included in size calculations until they
  // are activated.
  int32_t RegisterInactive(const RTPExtensionType type, const uint8_t id);
  bool SetActive(const RTPExtensionType type, bool active);

  int32_t Deregister(const RTPExtensionType type);

  bool IsRegistered(RTPExtensionType type) const;

  int32_t GetType(const uint8_t id, RTPExtensionType* type) const;

  int32_t GetId(const RTPExtensionType type, uint8_t* id) const;

  //
  // Methods below ignore any inactive rtp header extensions.
  //

  size_t GetTotalLengthInBytes() const;

  int32_t GetLengthUntilBlockStartInBytes(const RTPExtensionType type) const;

  void GetCopy(RtpHeaderExtensionMap* map) const;

  int32_t Size() const;

  RTPExtensionType First() const;

  RTPExtensionType Next(RTPExtensionType type) const;

 private:
  int32_t Register(const RTPExtensionType type, const uint8_t id, bool active);
  std::map<uint8_t, HeaderExtension*> extensionMap_;
};
}

#endif // WEBRTC_MODULES_RTP_RTCP_RTP_HEADER_EXTENSION_H_
