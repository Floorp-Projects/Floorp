/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "signaling/src/sdp/SdpMediaSection.h"

namespace mozilla
{
const SdpFmtpAttributeList::Parameters*
SdpMediaSection::FindFmtp(const std::string& pt) const
{
  const SdpAttributeList& attrs = GetAttributeList();

  if (attrs.HasAttribute(SdpAttribute::kFmtpAttribute)) {
    for (auto& fmtpAttr : attrs.GetFmtp().mFmtps) {
      if (fmtpAttr.format == pt && fmtpAttr.parameters) {
        return fmtpAttr.parameters.get();
      }
    }
  }
  return nullptr;
}

void
SdpMediaSection::SetFmtp(const SdpFmtpAttributeList::Fmtp& fmtpToSet)
{
  UniquePtr<SdpFmtpAttributeList> fmtps(new SdpFmtpAttributeList);

  if (GetAttributeList().HasAttribute(SdpAttribute::kFmtpAttribute)) {
    *fmtps = GetAttributeList().GetFmtp();
  }

  bool found = false;
  for (SdpFmtpAttributeList::Fmtp& fmtp : fmtps->mFmtps) {
    if (fmtp.format == fmtpToSet.format) {
      fmtp = fmtpToSet;
      found = true;
    }
  }

  if (!found) {
    fmtps->mFmtps.push_back(fmtpToSet);
  }

  GetAttributeList().SetAttribute(fmtps.release());
}

const SdpRtpmapAttributeList::Rtpmap*
SdpMediaSection::FindRtpmap(const std::string& pt) const
{
  auto& attrs = GetAttributeList();
  if (!attrs.HasAttribute(SdpAttribute::kRtpmapAttribute)) {
    return nullptr;
  }

  const SdpRtpmapAttributeList& rtpmap = attrs.GetRtpmap();
  if (!rtpmap.HasEntry(pt)) {
    return nullptr;
  }

  return &rtpmap.GetEntry(pt);
}

const SdpSctpmapAttributeList::Sctpmap*
SdpMediaSection::FindSctpmap(const std::string& pt) const
{
  auto& attrs = GetAttributeList();
  if (!attrs.HasAttribute(SdpAttribute::kSctpmapAttribute)) {
    return nullptr;
  }

  const SdpSctpmapAttributeList& sctpmap = attrs.GetSctpmap();
  if (!sctpmap.HasEntry(pt)) {
    return nullptr;
  }

  return &sctpmap.GetEntry(pt);
}

bool
SdpMediaSection::HasRtcpFb(const std::string& pt,
                           SdpRtcpFbAttributeList::Type type,
                           const std::string& subType) const
{
  const SdpAttributeList& attrs(GetAttributeList());

  if (!attrs.HasAttribute(SdpAttribute::kRtcpFbAttribute)) {
    return false;
  }

  for (auto& rtcpfb : attrs.GetRtcpFb().mFeedbacks) {
    if (rtcpfb.type == type) {
      if (rtcpfb.pt == "*" || rtcpfb.pt == pt) {
        if (rtcpfb.parameter == subType) {
          return true;
        }
      }
    }
  }

  return false;
}

SdpRtcpFbAttributeList
SdpMediaSection::GetRtcpFbs() const
{
  SdpRtcpFbAttributeList result;
  if (GetAttributeList().HasAttribute(SdpAttribute::kRtcpFbAttribute)) {
    result = GetAttributeList().GetRtcpFb();
  }
  return result;
}

void
SdpMediaSection::SetRtcpFbs(const SdpRtcpFbAttributeList& rtcpfbs)
{
  if (rtcpfbs.mFeedbacks.empty()) {
    GetAttributeList().RemoveAttribute(SdpAttribute::kRtcpFbAttribute);
    return;
  }

  GetAttributeList().SetAttribute(new SdpRtcpFbAttributeList(rtcpfbs));
}

void
SdpMediaSection::SetSsrcs(const std::vector<uint32_t>& ssrcs,
                          const std::string& cname)
{
  if (ssrcs.empty()) {
    GetAttributeList().RemoveAttribute(SdpAttribute::kSsrcAttribute);
    return;
  }

  UniquePtr<SdpSsrcAttributeList> ssrcAttr(new SdpSsrcAttributeList);
  for (auto ssrc : ssrcs) {
    // When using ssrc attributes, we are required to at least have a cname.
    // (See https://tools.ietf.org/html/rfc5576#section-6.1)
    std::string cnameAttr("cname:");
    cnameAttr += cname;
    ssrcAttr->PushEntry(ssrc, cnameAttr);
  }

  GetAttributeList().SetAttribute(ssrcAttr.release());
}

void
SdpMediaSection::AddMsid(const std::string& id, const std::string& appdata)
{
  UniquePtr<SdpMsidAttributeList> msids(new SdpMsidAttributeList);
  if (GetAttributeList().HasAttribute(SdpAttribute::kMsidAttribute)) {
    msids->mMsids = GetAttributeList().GetMsid().mMsids;
  }
  msids->PushEntry(id, appdata);
  GetAttributeList().SetAttribute(msids.release());
}

} // namespace mozilla

