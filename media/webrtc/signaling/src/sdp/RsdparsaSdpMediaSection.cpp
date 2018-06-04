/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Bug 1433534 tracks cleaning up the TODOs in the file

#include "signaling/src/sdp/SdpMediaSection.h"
#include "signaling/src/sdp/RsdparsaSdpMediaSection.h"

#include "signaling/src/sdp/RsdparsaSdpGlue.h"
#include "signaling/src/sdp/RsdparsaSdpInc.h"

#include <ostream>
#include "signaling/src/sdp/SdpErrorHolder.h"

#ifdef CRLF
#undef CRLF
#endif
#define CRLF "\r\n"

namespace mozilla
{

RsdparsaSdpMediaSection::RsdparsaSdpMediaSection(size_t level,
      RsdparsaSessionHandle session, const RustMediaSection* const section,
      const RsdparsaSdpAttributeList* sessionLevel)
  : SdpMediaSection(level), mSession(std::move(session)),
    mSection(section)
{
  switch(sdp_rust_get_media_type(section)) {
    case RustSdpMediaValue::kRustAudio:
      mMediaType = kAudio;
      break;
    case RustSdpMediaValue::kRustVideo:
      mMediaType = kVideo;
      break;
    case RustSdpMediaValue::kRustApplication:
      mMediaType = kApplication;
      break;
  }

  RsdparsaSessionHandle attributeSession(sdp_new_reference(mSession.get()));
  mAttributeList.reset(new RsdparsaSdpAttributeList(std::move(attributeSession),
                                                    section,
                                                    sessionLevel));

  LoadFormats();
  LoadConnection();
}

unsigned int
RsdparsaSdpMediaSection::GetPort() const
{
  return sdp_get_media_port(mSection);
}

void
RsdparsaSdpMediaSection::SetPort(unsigned int port)
{
  sdp_set_media_port(mSection, port);
}

unsigned int
RsdparsaSdpMediaSection::GetPortCount() const
{
  return sdp_get_media_port_count(mSection);
}

SdpMediaSection::Protocol
RsdparsaSdpMediaSection::GetProtocol() const
{
  switch(sdp_get_media_protocol(mSection)) {
    case RustSdpProtocolValue::kRustRtpSavpf:
      return kRtpSavpf;
    case RustSdpProtocolValue::kRustUdpTlsRtpSavpf:
      return kUdpTlsRtpSavpf;
    case RustSdpProtocolValue::kRustTcpTlsRtpSavpf:
      return kTcpTlsRtpSavpf;
    case RustSdpProtocolValue::kRustDtlsSctp:
      return kDtlsSctp;
    case RustSdpProtocolValue::kRustUdpDtlsSctp:
      return kUdpDtlsSctp;
    case RustSdpProtocolValue::kRustTcpDtlsSctp:
      return kTcpDtlsSctp;
  }

  MOZ_CRASH("invalid media protocol");
}

const SdpConnection&
RsdparsaSdpMediaSection::GetConnection() const
{
  MOZ_ASSERT(mConnection);
  return *mConnection;
}

SdpConnection&
RsdparsaSdpMediaSection::GetConnection()
{
  MOZ_ASSERT(mConnection);
  return *mConnection;
}

uint32_t
RsdparsaSdpMediaSection::GetBandwidth(const std::string& type) const
{
  return sdp_get_media_bandwidth(mSection, type.c_str());
}

const std::vector<std::string>&
RsdparsaSdpMediaSection::GetFormats() const
{
  return mFormats;
}

const SdpAttributeList&
RsdparsaSdpMediaSection::GetAttributeList() const
{
  return *mAttributeList;
}

SdpAttributeList&
RsdparsaSdpMediaSection::GetAttributeList()
{
  return *mAttributeList;
}

SdpDirectionAttribute
RsdparsaSdpMediaSection::GetDirectionAttribute() const
{
  return SdpDirectionAttribute(mAttributeList->GetDirection());
}

void
RsdparsaSdpMediaSection::AddCodec(const std::string& pt,
                                  const std::string& name,
                                  uint32_t clockrate, uint16_t channels)
{
  //TODO: see Bug 1438289
}

void
RsdparsaSdpMediaSection::ClearCodecs()
{

  //TODO: see Bug 1438289
}

void
RsdparsaSdpMediaSection::AddDataChannel(const std::string& name, uint16_t port,
                                        uint16_t streams, uint32_t message_size)
{
  //TODO: See 1438290
}

void
RsdparsaSdpMediaSection::Serialize(std::ostream& os) const
{
  os << "m=" << mMediaType << " " << GetPort();
  if (GetPortCount()) {
    os << "/" << GetPortCount();
  }
  os << " " << GetProtocol();
  for (auto i = mFormats.begin(); i != mFormats.end(); ++i) {
    os << " " << (*i);
  }
  os << CRLF;

  // We dont do i=

  if (mConnection) {
    os << *mConnection;
  }

  BandwidthVec* bwVec = sdp_get_media_bandwidth_vec(mSection);
  char* bwString = sdp_serialize_bandwidth(bwVec);
  if (bwString) {
    os << bwString;
    sdp_free_string(bwString);
  }

  // We dont do k= because they're evil

  os << *mAttributeList;
}

void
RsdparsaSdpMediaSection::LoadFormats()
{
  RustSdpFormatType formatType = sdp_get_format_type(mSection);
  if (formatType == RustSdpFormatType::kRustIntegers) {
    U32Vec* vec = sdp_get_format_u32_vec(mSection);
    size_t len = u32_vec_len(vec);
    for (size_t i = 0; i < len; i++) {
      uint32_t val;
      u32_vec_get(vec, i, &val);
      mFormats.push_back(std::to_string(val));
    }
  } else {
    StringVec* vec = sdp_get_format_string_vec(mSection);
    mFormats = convertStringVec(vec);
  }
}

UniquePtr<SdpConnection> convertRustConnection(RustSdpConnection conn)
{
  std::string addr(conn.addr.unicastAddr);
  sdp::AddrType type = convertAddressType(conn.addr.addrType);
  return MakeUnique<SdpConnection>(type, addr, conn.ttl, conn.amount);
}

void
RsdparsaSdpMediaSection::LoadConnection()
{
  RustSdpConnection conn;
  nsresult nr;
  if (sdp_media_has_connection(mSection)) {
    nr = sdp_get_media_connection(mSection, &conn);
    if (NS_SUCCEEDED(nr)) {
      mConnection = convertRustConnection(conn);
    }
  } else if (sdp_session_has_connection(mSession.get())){
    // TODO: rsdparsa needs to ensure there is a connection at the session level
    // if it is missing at a media level. See Bug 1438539.
    nr = sdp_get_session_connection(mSession.get(), &conn);
    if (NS_SUCCEEDED(nr)) {
      mConnection = convertRustConnection(conn);
    }
  }
}

} // namespace mozilla
