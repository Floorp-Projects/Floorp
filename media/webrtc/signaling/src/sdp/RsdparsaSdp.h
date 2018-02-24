/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _RSDPARSA_SDP_H_
#define _RSDPARSA_SDP_H_

#include "mozilla/UniquePtr.h"
#include "mozilla/Attributes.h"

#include "signaling/src/common/PtrVector.h"

#include "signaling/src/sdp/Sdp.h"

#include "signaling/src/sdp/RsdparsaSdpMediaSection.h"
#include "signaling/src/sdp/RsdparsaSdpAttributeList.h"
#include "signaling/src/sdp/RsdparsaSdpInc.h"
#include "signaling/src/sdp/RsdparsaSdpGlue.h"


namespace mozilla
{

class RsdparsaSdpParser;
class SdpErrorHolder;

class RsdparsaSdp final : public Sdp
{
  friend class RsdparsaSdpParser;

public:
  explicit RsdparsaSdp(RsdparsaSessionHandle session, const SdpOrigin& origin);

  const SdpOrigin& GetOrigin() const override;

  // Note: connection information is always retrieved from media sections
  uint32_t GetBandwidth(const std::string& type) const override;

  size_t
  GetMediaSectionCount() const override
  {
    return sdp_media_section_count(mSession.get());
  }

  const SdpAttributeList&
  GetAttributeList() const override
  {
    return *mAttributeList;
  }

  SdpAttributeList&
  GetAttributeList() override
  {
    return *mAttributeList;
  }

  const SdpMediaSection& GetMediaSection(size_t level) const
      override;

  SdpMediaSection& GetMediaSection(size_t level) override;

  SdpMediaSection& AddMediaSection(
      SdpMediaSection::MediaType media, SdpDirectionAttribute::Direction dir,
      uint16_t port, SdpMediaSection::Protocol proto, sdp::AddrType addrType,
      const std::string& addr) override;

  void Serialize(std::ostream&) const override;

private:
  RsdparsaSdp() : mOrigin("", 0, 0, sdp::kIPv4, "") {}

  RsdparsaSessionHandle mSession;
  SdpOrigin mOrigin;
  UniquePtr<RsdparsaSdpAttributeList> mAttributeList;
  PtrVector<RsdparsaSdpMediaSection> mMediaSections;
};

} // namespace mozilla

#endif
