/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_types.h"

#include <string.h>

namespace webrtc {

int InStream::Rewind() { return -1; }

int OutStream::Rewind() { return -1; }

StreamDataCounters::StreamDataCounters() : first_packet_time_ms(-1) {}

RTPHeaderExtension::RTPHeaderExtension()
    : hasTransmissionTimeOffset(false),
      transmissionTimeOffset(0),
      hasAbsoluteSendTime(false),
      absoluteSendTime(0),
      hasTransportSequenceNumber(false),
      transportSequenceNumber(0),
      hasAudioLevel(false),
      voiceActivity(false),
      audioLevel(0),
      hasVideoRotation(false),
      videoRotation(0),
      hasRID(false),
      rid(nullptr) {
}

RTPHeaderExtension::RTPHeaderExtension(const RTPHeaderExtension& rhs) {
  *this = rhs;
}

RTPHeaderExtension&
RTPHeaderExtension::operator=(const RTPHeaderExtension& rhs) {
  hasTransmissionTimeOffset = rhs.hasTransmissionTimeOffset;
  transmissionTimeOffset = rhs.transmissionTimeOffset;
  hasAbsoluteSendTime = rhs.hasAbsoluteSendTime;
  absoluteSendTime = rhs.absoluteSendTime;
  hasTransportSequenceNumber = rhs.hasTransportSequenceNumber;
  transportSequenceNumber = rhs.transportSequenceNumber;

  hasAudioLevel = rhs.hasAudioLevel;
  voiceActivity = rhs.voiceActivity;
  audioLevel = rhs.audioLevel;

  hasVideoRotation = rhs.hasVideoRotation;
  videoRotation = rhs.videoRotation;

  hasRID = rhs.hasRID;
  if (rhs.rid) {
    rid.reset(new char[strlen(rhs.rid.get())+1]);
    strcpy(rid.get(), rhs.rid.get());
  }
  return *this;
}

RTPHeader::RTPHeader()
    : markerBit(false),
      payloadType(0),
      sequenceNumber(0),
      timestamp(0),
      ssrc(0),
      numCSRCs(0),
      paddingLength(0),
      headerLength(0),
      payload_type_frequency(0),
      extension() {
  memset(&arrOfCSRCs, 0, sizeof(arrOfCSRCs));
}

}  // namespace webrtc
