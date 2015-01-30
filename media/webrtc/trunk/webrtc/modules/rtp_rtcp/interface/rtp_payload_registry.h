/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_INTERFACE_RTP_PAYLOAD_REGISTRY_H_
#define WEBRTC_MODULES_RTP_RTCP_INTERFACE_RTP_PAYLOAD_REGISTRY_H_

#include "webrtc/modules/rtp_rtcp/source/rtp_receiver_strategy.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

// This strategy deals with the audio/video-specific aspects
// of payload handling.
class RTPPayloadStrategy {
 public:
  virtual ~RTPPayloadStrategy() {}

  virtual bool CodecsMustBeUnique() const = 0;

  virtual bool PayloadIsCompatible(const RtpUtility::Payload& payload,
                                   const uint32_t frequency,
                                   const uint8_t channels,
                                   const uint32_t rate) const = 0;

  virtual void UpdatePayloadRate(RtpUtility::Payload* payload,
                                 const uint32_t rate) const = 0;

  virtual RtpUtility::Payload* CreatePayloadType(
      const char payloadName[RTP_PAYLOAD_NAME_SIZE],
      const int8_t payloadType,
      const uint32_t frequency,
      const uint8_t channels,
      const uint32_t rate) const = 0;

  virtual int GetPayloadTypeFrequency(
      const RtpUtility::Payload& payload) const = 0;

  static RTPPayloadStrategy* CreateStrategy(const bool handling_audio);

 protected:
  RTPPayloadStrategy() {}
};

class RTPPayloadRegistry {
 public:
  // The registry takes ownership of the strategy.
  RTPPayloadRegistry(RTPPayloadStrategy* rtp_payload_strategy);
  ~RTPPayloadRegistry();

  int32_t RegisterReceivePayload(
      const char payload_name[RTP_PAYLOAD_NAME_SIZE],
      const int8_t payload_type,
      const uint32_t frequency,
      const uint8_t channels,
      const uint32_t rate,
      bool* created_new_payload_type);

  int32_t DeRegisterReceivePayload(
      const int8_t payload_type);

  int32_t ReceivePayloadType(
      const char payload_name[RTP_PAYLOAD_NAME_SIZE],
      const uint32_t frequency,
      const uint8_t channels,
      const uint32_t rate,
      int8_t* payload_type) const;

  bool RtxEnabled() const;

  void SetRtxSsrc(uint32_t ssrc);

  void SetRtxPayloadType(int payload_type);

  bool IsRtx(const RTPHeader& header) const;

  bool RestoreOriginalPacket(uint8_t** restored_packet,
                             const uint8_t* packet,
                             int* packet_length,
                             uint32_t original_ssrc,
                             const RTPHeader& header) const;

  bool IsRed(const RTPHeader& header) const;

  // Returns true if the media of this RTP packet is encapsulated within an
  // extra header, such as RTX or RED.
  bool IsEncapsulated(const RTPHeader& header) const;

  bool GetPayloadSpecifics(uint8_t payload_type, PayloadUnion* payload) const;

  int GetPayloadTypeFrequency(uint8_t payload_type) const;

  bool PayloadTypeToPayload(const uint8_t payload_type,
                            RtpUtility::Payload*& payload) const;

  void ResetLastReceivedPayloadTypes() {
    CriticalSectionScoped cs(crit_sect_.get());
    last_received_payload_type_ = -1;
    last_received_media_payload_type_ = -1;
  }

  // This sets the payload type of the packets being received from the network
  // on the media SSRC. For instance if packets are encapsulated with RED, this
  // payload type will be the RED payload type.
  void SetIncomingPayloadType(const RTPHeader& header);

  // Returns true if the new media payload type has not changed.
  bool ReportMediaPayloadType(uint8_t media_payload_type);

  int8_t red_payload_type() const {
    CriticalSectionScoped cs(crit_sect_.get());
    return red_payload_type_;
  }
  int8_t ulpfec_payload_type() const {
    CriticalSectionScoped cs(crit_sect_.get());
    return ulpfec_payload_type_;
  }
  int8_t last_received_payload_type() const {
    CriticalSectionScoped cs(crit_sect_.get());
    return last_received_payload_type_;
  }
  void set_last_received_payload_type(int8_t last_received_payload_type) {
    CriticalSectionScoped cs(crit_sect_.get());
    last_received_payload_type_ = last_received_payload_type;
  }

  int8_t last_received_media_payload_type() const {
    CriticalSectionScoped cs(crit_sect_.get());
    return last_received_media_payload_type_;
  };

 private:
  // Prunes the payload type map of the specific payload type, if it exists.
  void DeregisterAudioCodecOrRedTypeRegardlessOfPayloadType(
      const char payload_name[RTP_PAYLOAD_NAME_SIZE],
      const size_t payload_name_length,
      const uint32_t frequency,
      const uint8_t channels,
      const uint32_t rate);

  bool IsRtxInternal(const RTPHeader& header) const;

  scoped_ptr<CriticalSectionWrapper> crit_sect_;
  RtpUtility::PayloadTypeMap payload_type_map_;
  scoped_ptr<RTPPayloadStrategy> rtp_payload_strategy_;
  int8_t  red_payload_type_;
  int8_t ulpfec_payload_type_;
  int8_t incoming_payload_type_;
  int8_t  last_received_payload_type_;
  int8_t  last_received_media_payload_type_;
  bool rtx_;
  int8_t payload_type_rtx_;
  uint32_t ssrc_rtx_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_INTERFACE_RTP_PAYLOAD_REGISTRY_H_
