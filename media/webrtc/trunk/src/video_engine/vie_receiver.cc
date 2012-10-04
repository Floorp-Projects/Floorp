/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_engine/vie_receiver.h"

#include "modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "modules/utility/interface/rtp_dump.h"
#include "modules/video_coding/main/interface/video_coding.h"
#include "system_wrappers/interface/critical_section_wrapper.h"
#include "system_wrappers/interface/trace.h"

namespace webrtc {

ViEReceiver::ViEReceiver(const int32_t channel_id,
                         VideoCodingModule* module_vcm)
    : receive_cs_(CriticalSectionWrapper::CreateCriticalSection()),
      channel_id_(channel_id),
      rtp_rtcp_(NULL),
      vcm_(module_vcm),
      external_decryption_(NULL),
      decryption_buffer_(NULL),
      rtp_dump_(NULL),
      receiving_(false) {
}

ViEReceiver::~ViEReceiver() {
  if (decryption_buffer_) {
    delete[] decryption_buffer_;
    decryption_buffer_ = NULL;
  }
  if (rtp_dump_) {
    rtp_dump_->Stop();
    RtpDump::DestroyRtpDump(rtp_dump_);
    rtp_dump_ = NULL;
  }
}

int ViEReceiver::RegisterExternalDecryption(Encryption* decryption) {
  CriticalSectionScoped cs(receive_cs_.get());
  if (external_decryption_) {
    return -1;
  }
  decryption_buffer_ = new WebRtc_UWord8[kViEMaxMtu];
  if (decryption_buffer_ == NULL) {
    return -1;
  }
  external_decryption_ = decryption;
  return 0;
}

int ViEReceiver::DeregisterExternalDecryption() {
  CriticalSectionScoped cs(receive_cs_.get());
  if (external_decryption_ == NULL) {
    return -1;
  }
  external_decryption_ = NULL;
  return 0;
}

void ViEReceiver::SetRtpRtcpModule(RtpRtcp* module) {
  rtp_rtcp_ = module;
}

void ViEReceiver::RegisterSimulcastRtpRtcpModules(
    const std::list<RtpRtcp*>& rtp_modules) {
  CriticalSectionScoped cs(receive_cs_.get());
  rtp_rtcp_simulcast_.clear();

  if (!rtp_modules.empty()) {
    rtp_rtcp_simulcast_.insert(rtp_rtcp_simulcast_.begin(),
                               rtp_modules.begin(),
                               rtp_modules.end());
  }
}

void ViEReceiver::IncomingRTPPacket(const WebRtc_Word8* rtp_packet,
                                    const WebRtc_Word32 rtp_packet_length,
                                    const char* from_ip,
                                    const WebRtc_UWord16 from_port) {
  InsertRTPPacket(rtp_packet, rtp_packet_length);
}

void ViEReceiver::IncomingRTCPPacket(const WebRtc_Word8* rtcp_packet,
                                     const WebRtc_Word32 rtcp_packet_length,
                                     const char* from_ip,
                                     const WebRtc_UWord16 from_port) {
  InsertRTCPPacket(rtcp_packet, rtcp_packet_length);
}

int ViEReceiver::ReceivedRTPPacket(const void* rtp_packet,
                                   int rtp_packet_length) {
  if (!receiving_) {
    return -1;
  }
  return InsertRTPPacket((const WebRtc_Word8*) rtp_packet, rtp_packet_length);
}

int ViEReceiver::ReceivedRTCPPacket(const void* rtcp_packet,
                                    int rtcp_packet_length) {
  if (!receiving_) {
    return -1;
  }
  return InsertRTCPPacket((const WebRtc_Word8*) rtcp_packet,
                          rtcp_packet_length);
}

WebRtc_Word32 ViEReceiver::OnReceivedPayloadData(
    const WebRtc_UWord8* payload_data, const WebRtc_UWord16 payload_size,
    const WebRtcRTPHeader* rtp_header) {
  if (rtp_header == NULL) {
    return 0;
  }

  if (vcm_->IncomingPacket(payload_data, payload_size, *rtp_header) != 0) {
    // Check this...
    return -1;
  }
  return 0;
}

int ViEReceiver::InsertRTPPacket(const WebRtc_Word8* rtp_packet,
                                 int rtp_packet_length) {
  // TODO(mflodman) Change decrypt to get rid of this cast.
  WebRtc_Word8* tmp_ptr = const_cast<WebRtc_Word8*>(rtp_packet);
  unsigned char* received_packet = reinterpret_cast<unsigned char*>(tmp_ptr);
  int received_packet_length = rtp_packet_length;

  {
    CriticalSectionScoped cs(receive_cs_.get());

    if (external_decryption_) {
      int decrypted_length = 0;
      external_decryption_->decrypt(channel_id_, received_packet,
                                    decryption_buffer_, received_packet_length,
                                    &decrypted_length);
      if (decrypted_length <= 0) {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, channel_id_,
                     "RTP decryption failed");
        return -1;
      } else if (decrypted_length > kViEMaxMtu) {
        WEBRTC_TRACE(webrtc::kTraceCritical, webrtc::kTraceVideo, channel_id_,
                     "InsertRTPPacket: %d bytes is allocated as RTP decrytption"
                     " output, external decryption used %d bytes. => memory is "
                     " now corrupted", kViEMaxMtu, decrypted_length);
        return -1;
      }
      received_packet = decryption_buffer_;
      received_packet_length = decrypted_length;
    }

    if (rtp_dump_) {
      rtp_dump_->DumpPacket(received_packet,
                           static_cast<WebRtc_UWord16>(received_packet_length));
    }
  }
  assert(rtp_rtcp_);  // Should be set by owner at construction time.
  return rtp_rtcp_->IncomingPacket(received_packet, received_packet_length);
}

int ViEReceiver::InsertRTCPPacket(const WebRtc_Word8* rtcp_packet,
                                  int rtcp_packet_length) {
  // TODO(mflodman) Change decrypt to get rid of this cast.
  WebRtc_Word8* tmp_ptr = const_cast<WebRtc_Word8*>(rtcp_packet);
  unsigned char* received_packet = reinterpret_cast<unsigned char*>(tmp_ptr);
  int received_packet_length = rtcp_packet_length;
  {
    CriticalSectionScoped cs(receive_cs_.get());

    if (external_decryption_) {
      int decrypted_length = 0;
      external_decryption_->decrypt_rtcp(channel_id_, received_packet,
                                         decryption_buffer_,
                                         received_packet_length,
                                         &decrypted_length);
      if (decrypted_length <= 0) {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, channel_id_,
                     "RTP decryption failed");
        return -1;
      } else if (decrypted_length > kViEMaxMtu) {
        WEBRTC_TRACE(webrtc::kTraceCritical, webrtc::kTraceVideo, channel_id_,
                     "InsertRTCPPacket: %d bytes is allocated as RTP "
                     " decrytption output, external decryption used %d bytes. "
                     " => memory is now corrupted",
                     kViEMaxMtu, decrypted_length);
        return -1;
      }
      received_packet = decryption_buffer_;
      received_packet_length = decrypted_length;
    }

    if (rtp_dump_) {
      rtp_dump_->DumpPacket(
          received_packet, static_cast<WebRtc_UWord16>(received_packet_length));
    }
  }
  {
    CriticalSectionScoped cs(receive_cs_.get());
    std::list<RtpRtcp*>::iterator it = rtp_rtcp_simulcast_.begin();
    while (it != rtp_rtcp_simulcast_.end()) {
      RtpRtcp* rtp_rtcp = *it++;
      rtp_rtcp->IncomingPacket(received_packet, received_packet_length);
    }
  }
  assert(rtp_rtcp_);  // Should be set by owner at construction time.
  return rtp_rtcp_->IncomingPacket(received_packet, received_packet_length);
}

void ViEReceiver::StartReceive() {
  receiving_ = true;
}

void ViEReceiver::StopReceive() {
  receiving_ = false;
}

int ViEReceiver::StartRTPDump(const char file_nameUTF8[1024]) {
  CriticalSectionScoped cs(receive_cs_.get());
  if (rtp_dump_) {
    // Restart it if it already exists and is started
    rtp_dump_->Stop();
  } else {
    rtp_dump_ = RtpDump::CreateRtpDump();
    if (rtp_dump_ == NULL) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, channel_id_,
                   "StartRTPDump: Failed to create RTP dump");
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

int ViEReceiver::StopRTPDump() {
  CriticalSectionScoped cs(receive_cs_.get());
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

}  // namespace webrtc
