/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _SDPMEDIASECTION_H_
#define _SDPMEDIASECTION_H_

#include "mozilla/Maybe.h"
#include "signaling/src/sdp/SdpEnum.h"
#include "signaling/src/sdp/SdpAttributeList.h"
#include <string>
#include <vector>
#include <sstream>

namespace mozilla
{

class SdpAttributeList;

class SdpConnection;

class SdpMediaSection
{
public:
  enum MediaType { kAudio, kVideo, kText, kApplication, kMessage };
  // don't add to enum to avoid warnings about unhandled enum values
  static const size_t kMediaTypes = static_cast<size_t>(kMessage) + 1;

  enum Protocol {
    kRtpAvp,            // RTP/AVP [RFC4566]
    kUdp,               // udp [RFC4566]
    kVat,               // vat [historic]
    kRtp,               // rtp [historic]
    kUdptl,             // udptl [ITU-T]
    kTcp,               // TCP [RFC4145]
    kRtpAvpf,           // RTP/AVPF [RFC4585]
    kTcpRtpAvp,         // TCP/RTP/AVP [RFC4571]
    kRtpSavp,           // RTP/SAVP [RFC3711]
    kTcpBfcp,           // TCP/BFCP [RFC4583]
    kTcpTlsBfcp,        // TCP/TLS/BFCP [RFC4583]
    kTcpTls,            // TCP/TLS [RFC4572]
    kFluteUdp,          // FLUTE/UDP [RFC-mehta-rmt-flute-sdp-05]
    kTcpMsrp,           // TCP/MSRP [RFC4975]
    kTcpTlsMsrp,        // TCP/TLS/MSRP [RFC4975]
    kDccp,              // DCCP [RFC5762]
    kDccpRtpAvp,        // DCCP/RTP/AVP [RFC5762]
    kDccpRtpSavp,       // DCCP/RTP/SAVP [RFC5762]
    kDccpRtpAvpf,       // DCCP/RTP/AVPF [RFC5762]
    kDccpRtpSavpf,      // DCCP/RTP/SAVPF [RFC5762]
    kRtpSavpf,          // RTP/SAVPF [RFC5124]
    kUdpTlsRtpSavp,     // UDP/TLS/RTP/SAVP [RFC5764]
    kTcpTlsRtpSavp,     // TCP/TLS/RTP/SAVP [JSEP-TBD]
    kDccpTlsRtpSavp,    // DCCP/TLS/RTP/SAVP [RFC5764]
    kUdpTlsRtpSavpf,    // UDP/TLS/RTP/SAVPF [RFC5764]
    kTcpTlsRtpSavpf,    // TCP/TLS/RTP/SAVPF [JSEP-TBD]
    kDccpTlsRtpSavpf,   // DCCP/TLS/RTP/SAVPF [RFC5764]
    kUdpMbmsFecRtpAvp,  // UDP/MBMS-FEC/RTP/AVP [RFC6064]
    kUdpMbmsFecRtpSavp, // UDP/MBMS-FEC/RTP/SAVP [RFC6064]
    kUdpMbmsRepair,     // UDP/MBMS-REPAIR [RFC6064]
    kFecUdp,            // FEC/UDP [RFC6364]
    kUdpFec,            // UDP/FEC [RFC6364]
    kTcpMrcpv2,         // TCP/MRCPv2 [RFC6787]
    kTcpTlsMrcpv2,      // TCP/TLS/MRCPv2 [RFC6787]
    kPstn,              // PSTN [RFC7195]
    kUdpTlsUdptl,       // UDP/TLS/UDPTL [RFC7345]
    kSctp,              // SCTP [draft-ietf-mmusic-sctp-sdp-07]
    kDtlsSctp,          // DTLS/SCTP [draft-ietf-mmusic-sctp-sdp-07]
    kUdpDtlsSctp,       // UDP/DTLS/SCTP [draft-ietf-mmusic-sctp-sdp-21]
    kTcpDtlsSctp        // TCP/DTLS/SCTP [draft-ietf-mmusic-sctp-sdp-21]
  };

  explicit SdpMediaSection(size_t level) : mLevel(level) {}

  virtual MediaType GetMediaType() const = 0;
  virtual unsigned int GetPort() const = 0;
  virtual void SetPort(unsigned int port) = 0;
  virtual unsigned int GetPortCount() const = 0;
  virtual Protocol GetProtocol() const = 0;
  virtual const SdpConnection& GetConnection() const = 0;
  virtual SdpConnection& GetConnection() = 0;
  virtual uint32_t GetBandwidth(const std::string& type) const = 0;
  virtual const std::vector<std::string>& GetFormats() const = 0;

  std::vector<std::string> GetFormatsForSimulcastVersion(
      size_t simulcastVersion, bool send, bool recv) const;
  virtual const SdpAttributeList& GetAttributeList() const = 0;
  virtual SdpAttributeList& GetAttributeList() = 0;

  virtual SdpDirectionAttribute GetDirectionAttribute() const = 0;

  virtual void Serialize(std::ostream&) const = 0;

  virtual void AddCodec(const std::string& pt, const std::string& name,
                        uint32_t clockrate, uint16_t channels) = 0;
  virtual void ClearCodecs() = 0;

  virtual void AddDataChannel(const std::string& name, uint16_t port,
                              uint16_t streams, uint32_t message_size) = 0;

  size_t
  GetLevel() const
  {
    return mLevel;
  }

  inline bool
  IsReceiving() const
  {
    return GetDirection() & sdp::kRecv;
  }

  inline bool
  IsSending() const
  {
    return GetDirection() & sdp::kSend;
  }

  inline void
  SetReceiving(bool receiving)
  {
    auto direction = GetDirection();
    if (direction & sdp::kSend) {
      SetDirection(receiving ?
                   SdpDirectionAttribute::kSendrecv :
                   SdpDirectionAttribute::kSendonly);
    } else {
      SetDirection(receiving ?
                   SdpDirectionAttribute::kRecvonly :
                   SdpDirectionAttribute::kInactive);
    }
  }

  inline void
  SetSending(bool sending)
  {
    auto direction = GetDirection();
    if (direction & sdp::kRecv) {
      SetDirection(sending ?
                   SdpDirectionAttribute::kSendrecv :
                   SdpDirectionAttribute::kRecvonly);
    } else {
      SetDirection(sending ?
                   SdpDirectionAttribute::kSendonly :
                   SdpDirectionAttribute::kInactive);
    }
  }

  inline void SetDirection(SdpDirectionAttribute::Direction direction)
  {
    GetAttributeList().SetAttribute(new SdpDirectionAttribute(direction));
  }

  inline SdpDirectionAttribute::Direction GetDirection() const
  {
    return GetDirectionAttribute().mValue;
  }

  const SdpFmtpAttributeList::Parameters* FindFmtp(const std::string& pt) const;
  void SetFmtp(const SdpFmtpAttributeList::Fmtp& fmtp);
  void RemoveFmtp(const std::string& pt);
  const SdpRtpmapAttributeList::Rtpmap* FindRtpmap(const std::string& pt) const;
  const SdpSctpmapAttributeList::Sctpmap* GetSctpmap() const;
  uint32_t GetSctpPort() const;
  bool GetMaxMessageSize(uint32_t* size) const;
  bool HasRtcpFb(const std::string& pt,
                 SdpRtcpFbAttributeList::Type type,
                 const std::string& subType) const;
  SdpRtcpFbAttributeList GetRtcpFbs() const;
  void SetRtcpFbs(const SdpRtcpFbAttributeList& rtcpfbs);
  bool HasFormat(const std::string& format) const
  {
    return std::find(GetFormats().begin(), GetFormats().end(), format) !=
        GetFormats().end();
  }
  void SetSsrcs(const std::vector<uint32_t>& ssrcs,
                const std::string& cname);
  void AddMsid(const std::string& id, const std::string& appdata);
  const SdpRidAttributeList::Rid* FindRid(const std::string& id) const;

private:
  size_t mLevel;
};

inline std::ostream& operator<<(std::ostream& os, const SdpMediaSection& ms)
{
  ms.Serialize(os);
  return os;
}

inline std::ostream& operator<<(std::ostream& os, SdpMediaSection::MediaType t)
{
  switch (t) {
    case SdpMediaSection::kAudio:
      return os << "audio";
    case SdpMediaSection::kVideo:
      return os << "video";
    case SdpMediaSection::kText:
      return os << "text";
    case SdpMediaSection::kApplication:
      return os << "application";
    case SdpMediaSection::kMessage:
      return os << "message";
  }
  MOZ_ASSERT(false, "Unknown MediaType");
  return os << "?";
}

inline std::ostream& operator<<(std::ostream& os, SdpMediaSection::Protocol p)
{
  switch (p) {
    case SdpMediaSection::kRtpAvp:
      return os << "RTP/AVP";
    case SdpMediaSection::kUdp:
      return os << "udp";
    case SdpMediaSection::kVat:
      return os << "vat";
    case SdpMediaSection::kRtp:
      return os << "rtp";
    case SdpMediaSection::kUdptl:
      return os << "udptl";
    case SdpMediaSection::kTcp:
      return os << "TCP";
    case SdpMediaSection::kRtpAvpf:
      return os << "RTP/AVPF";
    case SdpMediaSection::kTcpRtpAvp:
      return os << "TCP/RTP/AVP";
    case SdpMediaSection::kRtpSavp:
      return os << "RTP/SAVP";
    case SdpMediaSection::kTcpBfcp:
      return os << "TCP/BFCP";
    case SdpMediaSection::kTcpTlsBfcp:
      return os << "TCP/TLS/BFCP";
    case SdpMediaSection::kTcpTls:
      return os << "TCP/TLS";
    case SdpMediaSection::kFluteUdp:
      return os << "FLUTE/UDP";
    case SdpMediaSection::kTcpMsrp:
      return os << "TCP/MSRP";
    case SdpMediaSection::kTcpTlsMsrp:
      return os << "TCP/TLS/MSRP";
    case SdpMediaSection::kDccp:
      return os << "DCCP";
    case SdpMediaSection::kDccpRtpAvp:
      return os << "DCCP/RTP/AVP";
    case SdpMediaSection::kDccpRtpSavp:
      return os << "DCCP/RTP/SAVP";
    case SdpMediaSection::kDccpRtpAvpf:
      return os << "DCCP/RTP/AVPF";
    case SdpMediaSection::kDccpRtpSavpf:
      return os << "DCCP/RTP/SAVPF";
    case SdpMediaSection::kRtpSavpf:
      return os << "RTP/SAVPF";
    case SdpMediaSection::kUdpTlsRtpSavp:
      return os << "UDP/TLS/RTP/SAVP";
    case SdpMediaSection::kTcpTlsRtpSavp:
      return os << "TCP/TLS/RTP/SAVP";
    case SdpMediaSection::kDccpTlsRtpSavp:
      return os << "DCCP/TLS/RTP/SAVP";
    case SdpMediaSection::kUdpTlsRtpSavpf:
      return os << "UDP/TLS/RTP/SAVPF";
    case SdpMediaSection::kTcpTlsRtpSavpf:
      return os << "TCP/TLS/RTP/SAVPF";
    case SdpMediaSection::kDccpTlsRtpSavpf:
      return os << "DCCP/TLS/RTP/SAVPF";
    case SdpMediaSection::kUdpMbmsFecRtpAvp:
      return os << "UDP/MBMS-FEC/RTP/AVP";
    case SdpMediaSection::kUdpMbmsFecRtpSavp:
      return os << "UDP/MBMS-FEC/RTP/SAVP";
    case SdpMediaSection::kUdpMbmsRepair:
      return os << "UDP/MBMS-REPAIR";
    case SdpMediaSection::kFecUdp:
      return os << "FEC/UDP";
    case SdpMediaSection::kUdpFec:
      return os << "UDP/FEC";
    case SdpMediaSection::kTcpMrcpv2:
      return os << "TCP/MRCPv2";
    case SdpMediaSection::kTcpTlsMrcpv2:
      return os << "TCP/TLS/MRCPv2";
    case SdpMediaSection::kPstn:
      return os << "PSTN";
    case SdpMediaSection::kUdpTlsUdptl:
      return os << "UDP/TLS/UDPTL";
    case SdpMediaSection::kSctp:
      return os << "SCTP";
    case SdpMediaSection::kDtlsSctp:
      return os << "DTLS/SCTP";
    case SdpMediaSection::kUdpDtlsSctp:
      return os << "UDP/DTLS/SCTP";
    case SdpMediaSection::kTcpDtlsSctp:
      return os << "TCP/DTLS/SCTP";
  }
  MOZ_ASSERT(false, "Unknown Protocol");
  return os << "?";
}

class SdpConnection
{
public:
  SdpConnection(sdp::AddrType addrType, std::string addr, uint8_t ttl = 0,
                uint32_t count = 0)
      : mAddrType(addrType), mAddr(addr), mTtl(ttl), mCount(count)
  {
  }
  ~SdpConnection() {}

  sdp::AddrType
  GetAddrType() const
  {
    return mAddrType;
  }
  const std::string&
  GetAddress() const
  {
    return mAddr;
  }
  void
  SetAddress(const std::string& address)
  {
    mAddr = address;
    if (mAddr.find(':') != std::string::npos) {
      mAddrType = sdp::kIPv6;
    } else {
      mAddrType = sdp::kIPv4;
    }
  }
  uint8_t
  GetTtl() const
  {
    return mTtl;
  }
  uint32_t
  GetCount() const
  {
    return mCount;
  }

  void
  Serialize(std::ostream& os) const
  {
    sdp::NetType netType = sdp::kInternet;

    os << "c=" << netType << " " << mAddrType << " " << mAddr;

    if (mTtl) {
      os << "/" << static_cast<uint32_t>(mTtl);
      if (mCount) {
        os << "/" << mCount;
      }
    }
    os << "\r\n";
  }

private:
  sdp::AddrType mAddrType;
  std::string mAddr;
  uint8_t mTtl;    // 0-255; 0 when unset
  uint32_t mCount; // 0 when unset
};

inline std::ostream& operator<<(std::ostream& os, const SdpConnection& c)
{
  c.Serialize(os);
  return os;
}

} // namespace mozilla

#endif
