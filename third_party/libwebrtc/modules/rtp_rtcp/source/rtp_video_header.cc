/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_video_header.h"

namespace webrtc {

RTPVideoHeader::GenericDescriptorInfo::GenericDescriptorInfo() = default;
RTPVideoHeader::GenericDescriptorInfo::GenericDescriptorInfo(
    const GenericDescriptorInfo& other) = default;
RTPVideoHeader::GenericDescriptorInfo::~GenericDescriptorInfo() = default;

RTPVideoHeader::RTPVideoHeader() : video_timing() {}
RTPVideoHeader::RTPVideoHeader(const RTPVideoHeader& other) = default;
RTPVideoHeader::~RTPVideoHeader() = default;

VideoFrameMetadata RTPVideoHeader::GetAsMetadata() const {
  VideoFrameMetadata metadata;
  metadata.SetWidth(width);
  metadata.SetHeight(height);
  if (generic) {
    metadata.SetFrameId(generic->frame_id);
    metadata.SetSpatialIndex(generic->spatial_index);
    metadata.SetTemporalIndex(generic->temporal_index);
    metadata.SetFrameDependencies(generic->dependencies);
    metadata.SetDecodeTargetIndications(generic->decode_target_indications);
  }
  return metadata;
}

}  // namespace webrtc
