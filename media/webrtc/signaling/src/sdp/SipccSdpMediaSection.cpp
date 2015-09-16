/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "signaling/src/sdp/SipccSdpMediaSection.h"

#include <ostream>
#include "signaling/src/sdp/SdpErrorHolder.h"

#ifdef CRLF
#undef CRLF
#endif
#define CRLF "\r\n"

namespace mozilla
{

unsigned int
SipccSdpMediaSection::GetPort() const
{
  return mPort;
}

void
SipccSdpMediaSection::SetPort(unsigned int port)
{
  mPort = port;
}

unsigned int
SipccSdpMediaSection::GetPortCount() const
{
  return mPortCount;
}

SdpMediaSection::Protocol
SipccSdpMediaSection::GetProtocol() const
{
  return mProtocol;
}

const SdpConnection&
SipccSdpMediaSection::GetConnection() const
{
  return *mConnection;
}

SdpConnection&
SipccSdpMediaSection::GetConnection()
{
  return *mConnection;
}

uint32_t
SipccSdpMediaSection::GetBandwidth(const std::string& type) const
{
  auto found = mBandwidths.find(type);
  if (found == mBandwidths.end()) {
    return 0;
  }
  return found->second;
}

const std::vector<std::string>&
SipccSdpMediaSection::GetFormats() const
{
  return mFormats;
}

const SdpAttributeList&
SipccSdpMediaSection::GetAttributeList() const
{
  return mAttributeList;
}

SdpAttributeList&
SipccSdpMediaSection::GetAttributeList()
{
  return mAttributeList;
}

SdpDirectionAttribute
SipccSdpMediaSection::GetDirectionAttribute() const
{
  return SdpDirectionAttribute(mAttributeList.GetDirection());
}

bool
SipccSdpMediaSection::Load(sdp_t* sdp, uint16_t level,
                           SdpErrorHolder& errorHolder)
{
  switch (sdp_get_media_type(sdp, level)) {
    case SDP_MEDIA_AUDIO:
      mMediaType = kAudio;
      break;
    case SDP_MEDIA_VIDEO:
      mMediaType = kVideo;
      break;
    case SDP_MEDIA_APPLICATION:
      mMediaType = kApplication;
      break;
    case SDP_MEDIA_TEXT:
      mMediaType = kText;
      break;

    default:
      errorHolder.AddParseError(sdp_get_media_line_number(sdp, level),
                                "Unsupported media section type");
      return false;
  }

  mPort = sdp_get_media_portnum(sdp, level);
  int32_t pc = sdp_get_media_portcount(sdp, level);
  if (pc == SDP_INVALID_VALUE) {
    // SDP_INVALID_VALUE (ie; -2) is used when there is no port count. :(
    mPortCount = 0;
  } else if (pc > static_cast<int32_t>(UINT16_MAX) || pc < 0) {
    errorHolder.AddParseError(sdp_get_media_line_number(sdp, level),
                              "Invalid port count");
    return false;
  } else {
    mPortCount = pc;
  }

  if (!LoadProtocol(sdp, level, errorHolder)) {
    return false;
  }

  if (!LoadFormats(sdp, level, errorHolder)) {
    return false;
  }

  if (!mAttributeList.Load(sdp, level, errorHolder)) {
    return false;
  }

  if (!mBandwidths.Load(sdp, level, errorHolder)) {
    return false;
  }

  return LoadConnection(sdp, level, errorHolder);
}

bool
SipccSdpMediaSection::LoadProtocol(sdp_t* sdp, uint16_t level,
                                   SdpErrorHolder& errorHolder)
{
  switch (sdp_get_media_transport(sdp, level)) {
    case SDP_TRANSPORT_RTPAVP:
      mProtocol = kRtpAvp;
      break;
    case SDP_TRANSPORT_RTPSAVP:
      mProtocol = kRtpSavp;
      break;
    case SDP_TRANSPORT_RTPAVPF:
      mProtocol = kRtpAvpf;
      break;
    case SDP_TRANSPORT_RTPSAVPF:
      mProtocol = kRtpSavpf;
      break;
    case SDP_TRANSPORT_UDPTLSRTPSAVP:
      mProtocol = kUdpTlsRtpSavp;
      break;
    case SDP_TRANSPORT_UDPTLSRTPSAVPF:
      mProtocol = kUdpTlsRtpSavpf;
      break;
    case SDP_TRANSPORT_TCPTLSRTPSAVP:
      mProtocol = kTcpTlsRtpSavp;
      break;
    case SDP_TRANSPORT_TCPTLSRTPSAVPF:
      mProtocol = kTcpTlsRtpSavpf;
      break;
    case SDP_TRANSPORT_DTLSSCTP:
      mProtocol = kDtlsSctp;
      break;

    default:
      errorHolder.AddParseError(sdp_get_media_line_number(sdp, level),
                                "Unsupported media transport type");
      return false;
  }
  return true;
}

bool
SipccSdpMediaSection::LoadFormats(sdp_t* sdp,
                                  uint16_t level,
                                  SdpErrorHolder& errorHolder)
{
  sdp_media_e mtype = sdp_get_media_type(sdp, level);

  if (mtype == SDP_MEDIA_APPLICATION) {
    uint32_t ptype = sdp_get_media_sctp_port(sdp, level);
    std::ostringstream osPayloadType;
    osPayloadType << ptype;
    mFormats.push_back(osPayloadType.str());
  } else if (mtype == SDP_MEDIA_AUDIO || mtype == SDP_MEDIA_VIDEO) {
    uint16_t count = sdp_get_media_num_payload_types(sdp, level);
    for (uint16_t i = 0; i < count; ++i) {
      sdp_payload_ind_e indicator; // we ignore this, which is fine
      uint32_t ptype =
          sdp_get_media_payload_type(sdp, level, i + 1, &indicator);

      if (GET_DYN_PAYLOAD_TYPE_VALUE(ptype) > UINT8_MAX) {
        errorHolder.AddParseError(sdp_get_media_line_number(sdp, level),
                                  "Format is too large");
        return false;
      }

      std::ostringstream osPayloadType;
      // sipcc stores payload types in a funny way. When sipcc and the SDP it
      // parsed differ on what payload type number should be used for a given
      // codec, sipcc's value goes in the lower byte, and the SDP's value in
      // the upper byte. When they do not differ, only the lower byte is used.
      // We want what was in the SDP, verbatim.
      osPayloadType << GET_DYN_PAYLOAD_TYPE_VALUE(ptype);
      mFormats.push_back(osPayloadType.str());
    }
  }

  return true;
}

bool
SipccSdpMediaSection::LoadConnection(sdp_t* sdp, uint16_t level,
                                     SdpErrorHolder& errorHolder)
{
  if (!sdp_connection_valid(sdp, level)) {
    level = SDP_SESSION_LEVEL;
    if (!sdp_connection_valid(sdp, level)) {
      errorHolder.AddParseError(sdp_get_media_line_number(sdp, level),
                                "Missing c= line");
      return false;
    }
  }

  sdp_nettype_e type = sdp_get_conn_nettype(sdp, level);
  if (type != SDP_NT_INTERNET) {
    errorHolder.AddParseError(sdp_get_media_line_number(sdp, level),
                              "Unsupported network type");
    return false;
  }

  sdp::AddrType addrType;
  switch (sdp_get_conn_addrtype(sdp, level)) {
    case SDP_AT_IP4:
      addrType = sdp::kIPv4;
      break;
    case SDP_AT_IP6:
      addrType = sdp::kIPv6;
      break;
    default:
      errorHolder.AddParseError(sdp_get_media_line_number(sdp, level),
                                "Unsupported address type");
      return false;
  }

  std::string address = sdp_get_conn_address(sdp, level);
  int16_t ttl = static_cast<uint16_t>(sdp_get_mcast_ttl(sdp, level));
  if (ttl < 0) {
    ttl = 0;
  }
  int32_t numAddr =
      static_cast<uint32_t>(sdp_get_mcast_num_of_addresses(sdp, level));
  if (numAddr < 0) {
    numAddr = 0;
  }
  mConnection = MakeUnique<SdpConnection>(addrType, address, ttl, numAddr);
  return true;
}

void
SipccSdpMediaSection::AddCodec(const std::string& pt, const std::string& name,
                               uint32_t clockrate, uint16_t channels)
{
  mFormats.push_back(pt);

  SdpRtpmapAttributeList* rtpmap = new SdpRtpmapAttributeList();
  if (mAttributeList.HasAttribute(SdpAttribute::kRtpmapAttribute)) {
    const SdpRtpmapAttributeList& old = mAttributeList.GetRtpmap();
    for (auto it = old.mRtpmaps.begin(); it != old.mRtpmaps.end(); ++it) {
      rtpmap->mRtpmaps.push_back(*it);
    }
  }
  SdpRtpmapAttributeList::CodecType codec = SdpRtpmapAttributeList::kOtherCodec;
  if (name == "opus") {
    codec = SdpRtpmapAttributeList::kOpus;
  } else if (name == "G722") {
    codec = SdpRtpmapAttributeList::kG722;
  } else if (name == "PCMU") {
    codec = SdpRtpmapAttributeList::kPCMU;
  } else if (name == "PCMA") {
    codec = SdpRtpmapAttributeList::kPCMA;
  } else if (name == "VP8") {
    codec = SdpRtpmapAttributeList::kVP8;
  } else if (name == "VP9") {
    codec = SdpRtpmapAttributeList::kVP9;
  } else if (name == "H264") {
    codec = SdpRtpmapAttributeList::kH264;
  }

  rtpmap->PushEntry(pt, codec, name, clockrate, channels);
  mAttributeList.SetAttribute(rtpmap);
}

void
SipccSdpMediaSection::ClearCodecs()
{
  mFormats.clear();
  mAttributeList.RemoveAttribute(SdpAttribute::kRtpmapAttribute);
  mAttributeList.RemoveAttribute(SdpAttribute::kFmtpAttribute);
  mAttributeList.RemoveAttribute(SdpAttribute::kSctpmapAttribute);
  mAttributeList.RemoveAttribute(SdpAttribute::kRtcpFbAttribute);
}

void
SipccSdpMediaSection::AddDataChannel(const std::string& pt,
                                     const std::string& name, uint16_t streams)
{
  // Only one allowed, for now. This may change as the specs (and deployments)
  // evolve.
  mFormats.clear();
  mFormats.push_back(pt);
  SdpSctpmapAttributeList* sctpmap = new SdpSctpmapAttributeList();
  sctpmap->PushEntry(pt, name, streams);
  mAttributeList.SetAttribute(sctpmap);
}

void
SipccSdpMediaSection::Serialize(std::ostream& os) const
{
  os << "m=" << mMediaType << " " << mPort;
  if (mPortCount) {
    os << "/" << mPortCount;
  }
  os << " " << mProtocol;
  for (auto i = mFormats.begin(); i != mFormats.end(); ++i) {
    os << " " << (*i);
  }
  os << CRLF;

  // We dont do i=

  if (mConnection) {
    os << *mConnection;
  }

  mBandwidths.Serialize(os);

  // We dont do k= because they're evil

  os << mAttributeList;
}

} // namespace mozilla
