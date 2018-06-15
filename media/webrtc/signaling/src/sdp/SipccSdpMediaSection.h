/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _SIPCCSDPMEDIASECTION_H_
#define _SIPCCSDPMEDIASECTION_H_

#include "mozilla/Attributes.h"
#include "mozilla/UniquePtr.h"
#include "signaling/src/sdp/SdpMediaSection.h"
#include "signaling/src/sdp/SipccSdpAttributeList.h"

#include <map>

extern "C" {
#include "signaling/src/sdp/sipcc/sdp.h"
}

namespace mozilla
{

class SipccSdp;
class SdpErrorHolder;

class SipccSdpBandwidths final : public std::map<std::string, uint32_t>
{
public:
  bool Load(sdp_t* sdp, uint16_t level, SdpErrorHolder& errorHolder);
  void Serialize(std::ostream& os) const;
};

class SipccSdpMediaSection final : public SdpMediaSection
{
  friend class SipccSdp;

public:
  ~SipccSdpMediaSection() {}

  virtual MediaType
  GetMediaType() const override
  {
    return mMediaType;
  }

  virtual unsigned int GetPort() const override;
  virtual void SetPort(unsigned int port) override;
  virtual unsigned int GetPortCount() const override;
  virtual Protocol GetProtocol() const override;
  virtual const SdpConnection& GetConnection() const override;
  virtual SdpConnection& GetConnection() override;
  virtual uint32_t GetBandwidth(const std::string& type) const override;
  virtual const std::vector<std::string>& GetFormats() const override;

  virtual const SdpAttributeList& GetAttributeList() const override;
  virtual SdpAttributeList& GetAttributeList() override;
  virtual SdpDirectionAttribute GetDirectionAttribute() const override;

  virtual void AddCodec(const std::string& pt, const std::string& name,
                        uint32_t clockrate, uint16_t channels) override;
  virtual void ClearCodecs() override;

  virtual void AddDataChannel(const std::string& name, uint16_t port,
                              uint16_t streams, uint32_t message_size) override;

  virtual void Serialize(std::ostream&) const override;

private:
  SipccSdpMediaSection(size_t level, const SipccSdpAttributeList* sessionLevel)
      : SdpMediaSection(level),
        mMediaType(static_cast<MediaType>(0)),
        mPort(0),
        mPortCount(0),
        mProtocol(static_cast<Protocol>(0)),
        mAttributeList(sessionLevel)
  {
  }

  bool Load(sdp_t* sdp, uint16_t level, SdpErrorHolder& errorHolder);
  bool LoadConnection(sdp_t* sdp, uint16_t level, SdpErrorHolder& errorHolder);
  bool LoadProtocol(sdp_t* sdp, uint16_t level, SdpErrorHolder& errorHolder);
  bool LoadFormats(sdp_t* sdp, uint16_t level, SdpErrorHolder& errorHolder);
  bool ValidateSimulcast(sdp_t* sdp, uint16_t level,
                         SdpErrorHolder& errorHolder) const;
  bool ValidateSimulcastVersions(
      sdp_t* sdp,
      uint16_t level,
      const SdpSimulcastAttribute::Versions& versions,
      sdp::Direction direction,
      SdpErrorHolder& errorHolder) const;

  // the following values are cached on first get
  MediaType mMediaType;
  uint16_t mPort;
  uint16_t mPortCount;
  Protocol mProtocol;
  std::vector<std::string> mFormats;

  UniquePtr<SdpConnection> mConnection;
  SipccSdpBandwidths mBandwidths;

  SipccSdpAttributeList mAttributeList;
};
}

#endif
