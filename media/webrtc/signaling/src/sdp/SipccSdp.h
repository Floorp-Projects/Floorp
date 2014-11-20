/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _SIPCCSDP_H_
#define _SIPCCSDP_H_

#include <map>
#include <vector>
#include "mozilla/Attributes.h"
#include "mozilla/UniquePtr.h"

#include "signaling/src/sdp/Sdp.h"
#include "signaling/src/sdp/SipccSdpMediaSection.h"
#include "signaling/src/sdp/SipccSdpAttributeList.h"
extern "C" {
#include "signaling/src/sdp/sipcc/sdp.h"
}

namespace mozilla
{

class SipccSdpParser;
class SdpErrorHolder;

class SipccSdp MOZ_FINAL : public Sdp
{
  friend class SipccSdpParser;

public:
  explicit SipccSdp(const SdpOrigin& origin)
      : mOrigin(origin), mAttributeList(nullptr)
  {
  }
  ~SipccSdp();

  virtual const SdpOrigin& GetOrigin() const MOZ_OVERRIDE;

  // Note: connection information is always retrieved from media sections
  virtual uint32_t GetBandwidth(const std::string& type) const MOZ_OVERRIDE;

  virtual size_t
  GetMediaSectionCount() const MOZ_OVERRIDE
  {
    return mMediaSections.size();
  }

  virtual const SdpAttributeList&
  GetAttributeList() const MOZ_OVERRIDE
  {
    return mAttributeList;
  }

  virtual SdpAttributeList&
  GetAttributeList() MOZ_OVERRIDE
  {
    return mAttributeList;
  }

  virtual const SdpMediaSection& GetMediaSection(size_t level) const
      MOZ_OVERRIDE;

  virtual SdpMediaSection& GetMediaSection(size_t level) MOZ_OVERRIDE;

  virtual SdpMediaSection& AddMediaSection(
      SdpMediaSection::MediaType media, SdpDirectionAttribute::Direction dir,
      uint16_t port, SdpMediaSection::Protocol proto, sdp::AddrType addrType,
      const std::string& addr) MOZ_OVERRIDE;

  virtual void Serialize(std::ostream&) const MOZ_OVERRIDE;

private:
  SipccSdp() : mOrigin("", 0, 0, sdp::kIPv4, ""), mAttributeList(nullptr) {}

  bool Load(sdp_t* sdp, SdpErrorHolder& errorHolder);
  bool LoadOrigin(sdp_t* sdp, SdpErrorHolder& errorHolder);

  SdpOrigin mOrigin;
  SipccSdpBandwidths mBandwidths;
  SipccSdpAttributeList mAttributeList;
  std::vector<SipccSdpMediaSection*> mMediaSections;
};

} // namespace mozilla

#endif // _sdp_h_
