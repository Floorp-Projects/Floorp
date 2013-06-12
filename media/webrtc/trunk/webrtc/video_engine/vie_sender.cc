/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_engine/vie_sender.h"

#include <cassert>

#include "modules/utility/interface/rtp_dump.h"
#include "system_wrappers/interface/critical_section_wrapper.h"
#include "system_wrappers/interface/trace.h"

namespace webrtc {

ViESender::ViESender(int channel_id)
    : channel_id_(channel_id),
      critsect_(CriticalSectionWrapper::CreateCriticalSection()),
      external_encryption_(NULL),
      encryption_buffer_(NULL),
      transport_(NULL),
      rtp_dump_(NULL) {
}

ViESender::~ViESender() {
  if (encryption_buffer_) {
    delete[] encryption_buffer_;
    encryption_buffer_ = NULL;
  }

  if (rtp_dump_) {
    rtp_dump_->Stop();
    RtpDump::DestroyRtpDump(rtp_dump_);
    rtp_dump_ = NULL;
  }
}

int ViESender::RegisterExternalEncryption(Encryption* encryption) {
  CriticalSectionScoped cs(critsect_.get());
  if (external_encryption_) {
    return -1;
  }
  encryption_buffer_ = new uint8_t[kViEMaxMtu];
  if (encryption_buffer_ == NULL) {
    return -1;
  }
  external_encryption_ = encryption;
  return 0;
}

int ViESender::DeregisterExternalEncryption() {
  CriticalSectionScoped cs(critsect_.get());
  if (external_encryption_ == NULL) {
    return -1;
  }
  if (encryption_buffer_) {
    delete[] encryption_buffer_;
    encryption_buffer_ = NULL;
  }
  external_encryption_ = NULL;
  return 0;
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
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, channel_id_,
                   "StartSRTPDump: Failed to create RTP dump");
      return -1;
    }
  }
  if (rtp_dump_->Start(file_nameUTF8) != 0) {
    RtpDump::DestroyRtpDump(rtp_dump_);
    rtp_dump_ = NULL;
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, channel_id_,
                 "StartRTPDump: Failed to start RTP dump");
    return -1;
  }
  return 0;
}

int ViESender::StopRTPDump() {
  CriticalSectionScoped cs(critsect_.get());
  if (rtp_dump_) {
    if (rtp_dump_->IsActive()) {
      rtp_dump_->Stop();
    } else {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, channel_id_,
                   "StopRTPDump: Dump not active");
    }
    RtpDump::DestroyRtpDump(rtp_dump_);
    rtp_dump_ = NULL;
  } else {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, channel_id_,
                 "StopRTPDump: RTP dump not started");
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

  // TODO(mflodman) Change decrypt to get rid of this cast.
  void* tmp_ptr = const_cast<void*>(data);
  unsigned char* send_packet = static_cast<unsigned char*>(tmp_ptr);

  // Data length for packets sent to possible encryption and to the transport.
  int send_packet_length = len;

  if (rtp_dump_) {
    rtp_dump_->DumpPacket(send_packet, send_packet_length);
  }

  if (external_encryption_) {
    // Encryption buffer size.
    int encrypted_packet_length = kViEMaxMtu;

    external_encryption_->encrypt(channel_id_, send_packet, encryption_buffer_,
                                  send_packet_length, &encrypted_packet_length);
    send_packet = encryption_buffer_;
    send_packet_length = encrypted_packet_length;
  }
  const int bytes_sent = transport_->SendPacket(channel_id_, send_packet,
                                                send_packet_length);
  if (bytes_sent != send_packet_length) {
    WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo, channel_id_,
                 "ViESender::SendPacket - Transport failed to send RTP packet");
  }
  return bytes_sent;
}

int ViESender::SendRTCPPacket(int vie_id, const void* data, int len) {
  CriticalSectionScoped cs(critsect_.get());

  if (!transport_) {
    return -1;
  }

  assert(ChannelId(vie_id) == channel_id_);

  // Prepare for possible encryption and sending.
  // TODO(mflodman) Change decrypt to get rid of this cast.
  void* tmp_ptr = const_cast<void*>(data);
  unsigned char* send_packet = static_cast<unsigned char*>(tmp_ptr);

  // Data length for packets sent to possible encryption and to the transport.
  int send_packet_length = len;

  if (rtp_dump_) {
    rtp_dump_->DumpPacket(send_packet, send_packet_length);
  }

  if (external_encryption_) {
    // Encryption buffer size.
    int encrypted_packet_length = kViEMaxMtu;

    external_encryption_->encrypt_rtcp(
        channel_id_, send_packet, encryption_buffer_, send_packet_length,
        &encrypted_packet_length);
    send_packet = encryption_buffer_;
    send_packet_length = encrypted_packet_length;
  }

  const int bytes_sent = transport_->SendRTCPPacket(channel_id_, send_packet,
                                                    send_packet_length);
  if (bytes_sent != send_packet_length) {
    WEBRTC_TRACE(
        webrtc::kTraceWarning, webrtc::kTraceVideo, channel_id_,
        "ViESender::SendRTCPPacket - Transport failed to send RTCP packet");
  }
  return bytes_sent;
}

}  // namespace webrtc
