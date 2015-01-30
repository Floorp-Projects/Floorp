/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/vie_sender.h"

#include <assert.h>
#include "webrtc/modules/rtp_rtcp/source/rtp_sender.h"

#include "webrtc/modules/utility/interface/rtp_dump.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc {

ViESender::ViESender(int channel_id)
    : channel_id_(channel_id),
      critsect_(CriticalSectionWrapper::CreateCriticalSection()),
      transport_(NULL),
      rtp_dump_(NULL) {
}

ViESender::~ViESender() {
  if (rtp_dump_) {
    rtp_dump_->Stop();
    RtpDump::DestroyRtpDump(rtp_dump_);
    rtp_dump_ = NULL;
  }
}

int ViESender::RegisterSendTransport(Transport* transport) {
  CriticalSectionScoped cs(critsect_.get());
  if (transport_) {
    return -1;
  }
  transport_ = transport;
  return 0;
}

int ViESender::DeregisterSendTransport() {
  CriticalSectionScoped cs(critsect_.get());
  if (transport_ == NULL) {
    return -1;
  }
  transport_ = NULL;
  return 0;
}

int ViESender::StartRTPDump(const char file_nameUTF8[1024]) {
  CriticalSectionScoped cs(critsect_.get());
  if (rtp_dump_) {
    // Packet dump is already started, restart it.
    rtp_dump_->Stop();
  } else {
    rtp_dump_ = RtpDump::CreateRtpDump();
    if (rtp_dump_ == NULL) {
      return -1;
    }
  }
  if (rtp_dump_->Start(file_nameUTF8) != 0) {
    RtpDump::DestroyRtpDump(rtp_dump_);
    rtp_dump_ = NULL;
    return -1;
  }
  return 0;
}

int ViESender::StopRTPDump() {
  CriticalSectionScoped cs(critsect_.get());
  if (rtp_dump_) {
    if (rtp_dump_->IsActive()) {
      rtp_dump_->Stop();
    }
    RtpDump::DestroyRtpDump(rtp_dump_);
    rtp_dump_ = NULL;
  } else {
    return -1;
  }
  return 0;
}

int ViESender::SendPacket(int vie_id, const void* data, int len) {
  CriticalSectionScoped cs(critsect_.get());
  if (!transport_) {
    // No transport
    return -1;
  }
  assert(ChannelId(vie_id) == channel_id_);

  if (rtp_dump_) {
    rtp_dump_->DumpPacket(static_cast<const uint8_t*>(data),
                          static_cast<uint16_t>(len));
  }

  return transport_->SendPacket(channel_id_, data, len);
}

int ViESender::SendRTCPPacket(int vie_id, const void* data, int len) {
  CriticalSectionScoped cs(critsect_.get());
  if (!transport_) {
    return -1;
  }
  assert(ChannelId(vie_id) == channel_id_);

  if (rtp_dump_) {
    rtp_dump_->DumpPacket(static_cast<const uint8_t*>(data),
                          static_cast<uint16_t>(len));
  }

  return transport_->SendRTCPPacket(channel_id_, data, len);
}

}  // namespace webrtc
