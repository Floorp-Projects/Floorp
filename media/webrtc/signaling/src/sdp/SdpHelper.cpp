/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "signaling/src/sdp/SdpHelper.h"

#include "signaling/src/sdp/Sdp.h"
#include "signaling/src/sdp/SdpMediaSection.h"
#include "logging.h"

#include "nsDebug.h"
#include "nsError.h"
#include "prprf.h"

#include <string.h>
#include <set>

namespace mozilla {
MOZ_MTLOG_MODULE("sdp")

#define SDP_SET_ERROR(error)                                                   \
  do {                                                                         \
    std::ostringstream os;                                                     \
    os << error;                                                               \
    mLastError = os.str();                                                     \
    MOZ_MTLOG(ML_ERROR, mLastError);                                           \
  } while (0);

nsresult
SdpHelper::CopyTransportParams(size_t numComponents,
                               const SdpMediaSection& oldLocal,
                               SdpMediaSection* newLocal)
{
  // Copy over m-section details
  newLocal->SetPort(oldLocal.GetPort());
  newLocal->GetConnection() = oldLocal.GetConnection();

  const SdpAttributeList& oldLocalAttrs = oldLocal.GetAttributeList();
  SdpAttributeList& newLocalAttrs = newLocal->GetAttributeList();

  // Now we copy over attributes that won't be added by the usual logic
  if (oldLocalAttrs.HasAttribute(SdpAttribute::kCandidateAttribute) &&
      numComponents) {
    UniquePtr<SdpMultiStringAttribute> candidateAttrs(
      new SdpMultiStringAttribute(SdpAttribute::kCandidateAttribute));
    for (const std::string& candidate : oldLocalAttrs.GetCandidate()) {
      size_t component;
      nsresult rv = GetComponent(candidate, &component);
      NS_ENSURE_SUCCESS(rv, rv);
      if (numComponents >= component) {
        candidateAttrs->mValues.push_back(candidate);
      }
    }
    if (candidateAttrs->mValues.size()) {
      newLocalAttrs.SetAttribute(candidateAttrs.release());
    }
  }

  if (numComponents == 2 &&
      oldLocalAttrs.HasAttribute(SdpAttribute::kRtcpAttribute)) {
    // copy rtcp attribute if we had one that we are using
    newLocalAttrs.SetAttribute(new SdpRtcpAttribute(oldLocalAttrs.GetRtcp()));
  }

  return NS_OK;
}

bool
SdpHelper::AreOldTransportParamsValid(const Sdp& oldAnswer,
                                      const Sdp& offerersPreviousSdp,
                                      const Sdp& newOffer,
                                      size_t level)
{
  if (MsectionIsDisabled(oldAnswer.GetMediaSection(level)) ||
      MsectionIsDisabled(newOffer.GetMediaSection(level))) {
    // Obvious
    return false;
  }

  if (IsBundleSlave(oldAnswer, level)) {
    // The transport attributes on this m-section were thrown away, because it
    // was bundled.
    return false;
  }

  if (newOffer.GetMediaSection(level).GetAttributeList().HasAttribute(
        SdpAttribute::kBundleOnlyAttribute) &&
      IsBundleSlave(newOffer, level)) {
    // It never makes sense to put transport attributes in a bundle-only
    // m-section
    return false;
  }

  if (IceCredentialsDiffer(newOffer.GetMediaSection(level),
                           offerersPreviousSdp.GetMediaSection(level))) {
    return false;
  }

  return true;
}

bool
SdpHelper::IceCredentialsDiffer(const SdpMediaSection& msection1,
                                const SdpMediaSection& msection2)
{
  const SdpAttributeList& attrs1(msection1.GetAttributeList());
  const SdpAttributeList& attrs2(msection2.GetAttributeList());

  if ((attrs1.GetIceUfrag() != attrs2.GetIceUfrag()) ||
      (attrs1.GetIcePwd() != attrs2.GetIcePwd())) {
    return true;
  }

  return false;
}

nsresult
SdpHelper::GetComponent(const std::string& candidate, size_t* component)
{
  unsigned int temp;
  int32_t result = PR_sscanf(candidate.c_str(), "%*s %u", &temp);
  if (result == 1) {
    *component = temp;
    return NS_OK;
  }
  SDP_SET_ERROR("Malformed ICE candidate: " << candidate);
  return NS_ERROR_INVALID_ARG;
}

bool
SdpHelper::MsectionIsDisabled(const SdpMediaSection& msection) const
{
  return !msection.GetPort() &&
         !msection.GetAttributeList().HasAttribute(
             SdpAttribute::kBundleOnlyAttribute);
}

void
SdpHelper::DisableMsection(Sdp* sdp, SdpMediaSection* msection)
{
  // Make sure to remove the mid from any group attributes
  if (msection->GetAttributeList().HasAttribute(SdpAttribute::kMidAttribute)) {
    std::string mid = msection->GetAttributeList().GetMid();
    if (sdp->GetAttributeList().HasAttribute(SdpAttribute::kGroupAttribute)) {
      UniquePtr<SdpGroupAttributeList> newGroupAttr(new SdpGroupAttributeList(
            sdp->GetAttributeList().GetGroup()));
      newGroupAttr->RemoveMid(mid);
      sdp->GetAttributeList().SetAttribute(newGroupAttr.release());
    }
  }

  // Clear out attributes.
  msection->GetAttributeList().Clear();

  auto* direction =
    new SdpDirectionAttribute(SdpDirectionAttribute::kInactive);
  msection->GetAttributeList().SetAttribute(direction);
  msection->SetPort(0);

  msection->ClearCodecs();

  auto mediaType = msection->GetMediaType();
  switch (mediaType) {
    case SdpMediaSection::kAudio:
      msection->AddCodec("0", "PCMU", 8000, 1);
      break;
    case SdpMediaSection::kVideo:
      msection->AddCodec("120", "VP8", 90000, 1);
      break;
    case SdpMediaSection::kApplication:
      msection->AddDataChannel("5000", "rejected", 0);
      break;
    default:
      // We need to have something here to fit the grammar, this seems safe
      // and 19 is a reserved payload type which should not be used by anyone.
      msection->AddCodec("19", "reserved", 8000, 1);
  }
}

void
SdpHelper::GetBundleGroups(
    const Sdp& sdp,
    std::vector<SdpGroupAttributeList::Group>* bundleGroups) const
{
  if (sdp.GetAttributeList().HasAttribute(SdpAttribute::kGroupAttribute)) {
    for (auto& group : sdp.GetAttributeList().GetGroup().mGroups) {
      if (group.semantics == SdpGroupAttributeList::kBundle) {
        bundleGroups->push_back(group);
      }
    }
  }
}

nsresult
SdpHelper::GetBundledMids(const Sdp& sdp, BundledMids* bundledMids)
{
  std::vector<SdpGroupAttributeList::Group> bundleGroups;
  GetBundleGroups(sdp, &bundleGroups);

  for (SdpGroupAttributeList::Group& group : bundleGroups) {
    if (group.tags.empty()) {
      SDP_SET_ERROR("Empty BUNDLE group");
      return NS_ERROR_INVALID_ARG;
    }

    const SdpMediaSection* masterBundleMsection(
        FindMsectionByMid(sdp, group.tags[0]));

    if (!masterBundleMsection) {
      SDP_SET_ERROR("mid specified for bundle transport in group attribute"
          " does not exist in the SDP. (mid=" << group.tags[0] << ")");
      return NS_ERROR_INVALID_ARG;
    }

    if (MsectionIsDisabled(*masterBundleMsection)) {
      SDP_SET_ERROR("mid specified for bundle transport in group attribute"
          " points at a disabled m-section. (mid=" << group.tags[0] << ")");
      return NS_ERROR_INVALID_ARG;
    }

    for (const std::string& mid : group.tags) {
      if (bundledMids->count(mid)) {
        SDP_SET_ERROR("mid \'" << mid << "\' appears more than once in a "
                       "BUNDLE group");
        return NS_ERROR_INVALID_ARG;
      }

      (*bundledMids)[mid] = masterBundleMsection;
    }
  }

  return NS_OK;
}

bool
SdpHelper::IsBundleSlave(const Sdp& sdp, uint16_t level)
{
  auto& msection = sdp.GetMediaSection(level);

  if (!msection.GetAttributeList().HasAttribute(SdpAttribute::kMidAttribute)) {
    // No mid, definitely no bundle for this m-section
    return false;
  }
  std::string mid(msection.GetAttributeList().GetMid());

  BundledMids bundledMids;
  nsresult rv = GetBundledMids(sdp, &bundledMids);
  if (NS_FAILED(rv)) {
    // Should have been caught sooner.
    MOZ_ASSERT(false);
    return false;
  }

  if (bundledMids.count(mid) && level != bundledMids[mid]->GetLevel()) {
    // mid is bundled, and isn't the bundle m-section
    return true;
  }

  return false;
}

nsresult
SdpHelper::GetMidFromLevel(const Sdp& sdp,
                           uint16_t level,
                           std::string* mid)
{
  if (level >= sdp.GetMediaSectionCount()) {
    SDP_SET_ERROR("Index " << level << " out of range");
    return NS_ERROR_INVALID_ARG;
  }

  const SdpMediaSection& msection = sdp.GetMediaSection(level);
  const SdpAttributeList& attrList = msection.GetAttributeList();

  // grab the mid and set the outparam
  if (attrList.HasAttribute(SdpAttribute::kMidAttribute)) {
    *mid = attrList.GetMid();
  }

  return NS_OK;
}

nsresult
SdpHelper::AddCandidateToSdp(Sdp* sdp,
                             const std::string& candidateUntrimmed,
                             const std::string& mid,
                             uint16_t level)
{

  if (level >= sdp->GetMediaSectionCount()) {
    SDP_SET_ERROR("Index " << level << " out of range");
    return NS_ERROR_INVALID_ARG;
  }

  // Trim off '[a=]candidate:'
  size_t begin = candidateUntrimmed.find(':');
  if (begin == std::string::npos) {
    SDP_SET_ERROR("Invalid candidate, no ':' (" << candidateUntrimmed << ")");
    return NS_ERROR_INVALID_ARG;
  }
  ++begin;

  std::string candidate = candidateUntrimmed.substr(begin);

  // https://tools.ietf.org/html/draft-ietf-rtcweb-jsep-11#section-3.4.2.1
  // Implementations receiving an ICE Candidate object MUST use the MID if
  // present, or the m= line index, if not (as it could have come from a
  // non-JSEP endpoint). (bug 1095793)
  SdpMediaSection* msection = 0;
  if (!mid.empty()) {
    // FindMsectionByMid could return nullptr
    msection = FindMsectionByMid(*sdp, mid);

    // Check to make sure mid matches what we'd get by
    // looking up the m= line using the level. (mjf)
    std::string checkMid;
    nsresult rv = GetMidFromLevel(*sdp, level, &checkMid);
    if (NS_FAILED(rv)) {
      return rv;
    }
    if (mid != checkMid) {
      SDP_SET_ERROR("Mismatch between mid and level - \"" << mid
                     << "\" is not the mid for level " << level
                     << "; \"" << checkMid << "\" is");
      return NS_ERROR_INVALID_ARG;
    }
  }
  if (!msection) {
    msection = &(sdp->GetMediaSection(level));
  }

  SdpAttributeList& attrList = msection->GetAttributeList();

  UniquePtr<SdpMultiStringAttribute> candidates;
  if (!attrList.HasAttribute(SdpAttribute::kCandidateAttribute)) {
    // Create new
    candidates.reset(
        new SdpMultiStringAttribute(SdpAttribute::kCandidateAttribute));
  } else {
    // Copy existing
    candidates.reset(new SdpMultiStringAttribute(
        *static_cast<const SdpMultiStringAttribute*>(
            attrList.GetAttribute(SdpAttribute::kCandidateAttribute))));
  }
  candidates->PushEntry(candidate);
  attrList.SetAttribute(candidates.release());

  return NS_OK;
}

void
SdpHelper::SetIceGatheringComplete(Sdp* sdp,
                                   uint16_t level,
                                   BundledMids bundledMids)
{
  SdpMediaSection& msection = sdp->GetMediaSection(level);

  if (kSlaveBundle == GetMsectionBundleType(*sdp,
                                            level,
                                            bundledMids,
                                            nullptr)) {
    return; // Slave bundle m-section. Skip.
  }

  SdpAttributeList& attrs = msection.GetAttributeList();
  attrs.SetAttribute(
      new SdpFlagAttribute(SdpAttribute::kEndOfCandidatesAttribute));
  // Remove trickle-ice option
  attrs.RemoveAttribute(SdpAttribute::kIceOptionsAttribute);
}

void
SdpHelper::SetDefaultAddresses(const std::string& defaultCandidateAddr,
                               uint16_t defaultCandidatePort,
                               const std::string& defaultRtcpCandidateAddr,
                               uint16_t defaultRtcpCandidatePort,
                               Sdp* sdp,
                               uint16_t level,
                               BundledMids bundledMids)
{
  SdpMediaSection& msection = sdp->GetMediaSection(level);
  std::string masterMid;

  MsectionBundleType bundleType = GetMsectionBundleType(*sdp,
                                                        level,
                                                        bundledMids,
                                                        &masterMid);
  if (kSlaveBundle == bundleType) {
    return; // Slave bundle m-section. Skip.
  }
  if (kMasterBundle == bundleType) {
    // Master bundle m-section. Set defaultCandidateAddr and
    // defaultCandidatePort on all bundled m-sections.
    const SdpMediaSection* masterBundleMsection(bundledMids[masterMid]);
    for (auto i = bundledMids.begin(); i != bundledMids.end(); ++i) {
      if (i->second != masterBundleMsection) {
        continue;
      }
      SdpMediaSection* bundledMsection = FindMsectionByMid(*sdp, i->first);
      if (!bundledMsection) {
        MOZ_ASSERT(false);
        continue;
      }
      SetDefaultAddresses(defaultCandidateAddr,
                          defaultCandidatePort,
                          defaultRtcpCandidateAddr,
                          defaultRtcpCandidatePort,
                          bundledMsection);
    }
  }

  SetDefaultAddresses(defaultCandidateAddr,
                      defaultCandidatePort,
                      defaultRtcpCandidateAddr,
                      defaultRtcpCandidatePort,
                      &msection);
}

void
SdpHelper::SetDefaultAddresses(const std::string& defaultCandidateAddr,
                               uint16_t defaultCandidatePort,
                               const std::string& defaultRtcpCandidateAddr,
                               uint16_t defaultRtcpCandidatePort,
                               SdpMediaSection* msection)
{
  msection->GetConnection().SetAddress(defaultCandidateAddr);
  SdpAttributeList& attrList = msection->GetAttributeList();

  // only set the port if there is no bundle-only attribute
  if (!attrList.HasAttribute(SdpAttribute::kBundleOnlyAttribute)) {
    msection->SetPort(defaultCandidatePort);
  }

  if (!defaultRtcpCandidateAddr.empty()) {
    sdp::AddrType ipVersion = sdp::kIPv4;
    if (defaultRtcpCandidateAddr.find(':') != std::string::npos) {
      ipVersion = sdp::kIPv6;
    }
    attrList.SetAttribute(new SdpRtcpAttribute(
          defaultRtcpCandidatePort,
          sdp::kInternet,
          ipVersion,
          defaultRtcpCandidateAddr));
  }
}

nsresult
SdpHelper::GetIdsFromMsid(const Sdp& sdp,
                                const SdpMediaSection& msection,
                                std::string* streamId,
                                std::string* trackId)
{
  if (!sdp.GetAttributeList().HasAttribute(
        SdpAttribute::kMsidSemanticAttribute)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  auto& msidSemantics = sdp.GetAttributeList().GetMsidSemantic().mMsidSemantics;
  std::vector<SdpMsidAttributeList::Msid> allMsids;
  nsresult rv = GetMsids(msection, &allMsids);
  NS_ENSURE_SUCCESS(rv, rv);

  bool allMsidsAreWebrtc = false;
  std::set<std::string> webrtcMsids;

  for (auto i = msidSemantics.begin(); i != msidSemantics.end(); ++i) {
    if (i->semantic == "WMS") {
      for (auto j = i->msids.begin(); j != i->msids.end(); ++j) {
        if (*j == "*") {
          allMsidsAreWebrtc = true;
        } else {
          webrtcMsids.insert(*j);
        }
      }
      break;
    }
  }

  bool found = false;

  for (auto i = allMsids.begin(); i != allMsids.end(); ++i) {
    if (allMsidsAreWebrtc || webrtcMsids.count(i->identifier)) {
      if (i->appdata.empty()) {
        SDP_SET_ERROR("Invalid webrtc msid at level " << msection.GetLevel()
                       << ": Missing track id.");
        return NS_ERROR_INVALID_ARG;
      }
      if (!found) {
        *streamId = i->identifier;
        *trackId = i->appdata;
        found = true;
      } else if ((*streamId != i->identifier) || (*trackId != i->appdata)) {
        SDP_SET_ERROR("Found multiple different webrtc msids in m-section "
                       << msection.GetLevel() << ". The behavior here is "
                       "undefined.");
        return NS_ERROR_INVALID_ARG;
      }
    }
  }

  if (!found) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return NS_OK;
}

nsresult
SdpHelper::GetMsids(const SdpMediaSection& msection,
                    std::vector<SdpMsidAttributeList::Msid>* msids)
{
  if (msection.GetAttributeList().HasAttribute(SdpAttribute::kMsidAttribute)) {
    *msids = msection.GetAttributeList().GetMsid().mMsids;
  }

  // Can we find some additional msids in ssrc attributes?
  // (Chrome does not put plain-old msid attributes in its SDP)
  if (msection.GetAttributeList().HasAttribute(SdpAttribute::kSsrcAttribute)) {
    auto& ssrcs = msection.GetAttributeList().GetSsrc().mSsrcs;

    for (auto i = ssrcs.begin(); i != ssrcs.end(); ++i) {
      if (i->attribute.find("msid:") == 0) {
        std::string streamId;
        std::string trackId;
        nsresult rv = ParseMsid(i->attribute, &streamId, &trackId);
        NS_ENSURE_SUCCESS(rv, rv);
        msids->push_back({streamId, trackId});
      }
    }
  }

  return NS_OK;
}

nsresult
SdpHelper::ParseMsid(const std::string& msidAttribute,
                     std::string* streamId,
                     std::string* trackId)
{
  // Would be nice if SdpSsrcAttributeList could parse out the contained
  // attribute, but at least the parse here is simple.
  // We are being very forgiving here wrt whitespace; tabs are not actually
  // allowed, nor is leading/trailing whitespace.
  size_t streamIdStart = msidAttribute.find_first_not_of(" \t", 5);
  // We do not assume the appdata token is here, since this is not
  // necessarily a webrtc msid
  if (streamIdStart == std::string::npos) {
    SDP_SET_ERROR("Malformed source-level msid attribute: "
        << msidAttribute);
    return NS_ERROR_INVALID_ARG;
  }

  size_t streamIdEnd = msidAttribute.find_first_of(" \t", streamIdStart);
  if (streamIdEnd == std::string::npos) {
    streamIdEnd = msidAttribute.size();
  }

  size_t trackIdStart =
    msidAttribute.find_first_not_of(" \t", streamIdEnd);
  if (trackIdStart == std::string::npos) {
    trackIdStart = msidAttribute.size();
  }

  size_t trackIdEnd = msidAttribute.find_first_of(" \t", trackIdStart);
  if (trackIdEnd == std::string::npos) {
    trackIdEnd = msidAttribute.size();
  }

  size_t streamIdSize = streamIdEnd - streamIdStart;
  size_t trackIdSize = trackIdEnd - trackIdStart;

  *streamId = msidAttribute.substr(streamIdStart, streamIdSize);
  *trackId = msidAttribute.substr(trackIdStart, trackIdSize);
  return NS_OK;
}

void
SdpHelper::SetupMsidSemantic(const std::vector<std::string>& msids,
                             Sdp* sdp) const
{
  if (!msids.empty()) {
    UniquePtr<SdpMsidSemanticAttributeList> msidSemantics(
        new SdpMsidSemanticAttributeList);
    msidSemantics->PushEntry("WMS", msids);
    sdp->GetAttributeList().SetAttribute(msidSemantics.release());
  }
}

std::string
SdpHelper::GetCNAME(const SdpMediaSection& msection) const
{
  if (msection.GetAttributeList().HasAttribute(SdpAttribute::kSsrcAttribute)) {
    auto& ssrcs = msection.GetAttributeList().GetSsrc().mSsrcs;
    for (auto i = ssrcs.begin(); i != ssrcs.end(); ++i) {
      if (i->attribute.find("cname:") == 0) {
        return i->attribute.substr(6);
      }
    }
  }
  return "";
}

const SdpMediaSection*
SdpHelper::FindMsectionByMid(const Sdp& sdp,
                             const std::string& mid) const
{
  for (size_t i = 0; i < sdp.GetMediaSectionCount(); ++i) {
    auto& attrs = sdp.GetMediaSection(i).GetAttributeList();
    if (attrs.HasAttribute(SdpAttribute::kMidAttribute) &&
        attrs.GetMid() == mid) {
      return &sdp.GetMediaSection(i);
    }
  }
  return nullptr;
}

SdpMediaSection*
SdpHelper::FindMsectionByMid(Sdp& sdp,
                             const std::string& mid) const
{
  for (size_t i = 0; i < sdp.GetMediaSectionCount(); ++i) {
    auto& attrs = sdp.GetMediaSection(i).GetAttributeList();
    if (attrs.HasAttribute(SdpAttribute::kMidAttribute) &&
        attrs.GetMid() == mid) {
      return &sdp.GetMediaSection(i);
    }
  }
  return nullptr;
}

nsresult
SdpHelper::CopyStickyParams(const SdpMediaSection& source,
                            SdpMediaSection* dest)
{
  auto& sourceAttrs = source.GetAttributeList();
  auto& destAttrs = dest->GetAttributeList();

  // There's no reason to renegotiate rtcp-mux
  if (sourceAttrs.HasAttribute(SdpAttribute::kRtcpMuxAttribute)) {
    destAttrs.SetAttribute(
        new SdpFlagAttribute(SdpAttribute::kRtcpMuxAttribute));
  }

  // mid should stay the same
  if (sourceAttrs.HasAttribute(SdpAttribute::kMidAttribute)) {
    destAttrs.SetAttribute(
        new SdpStringAttribute(SdpAttribute::kMidAttribute,
          sourceAttrs.GetMid()));
  }

  return NS_OK;
}

bool
SdpHelper::HasRtcp(SdpMediaSection::Protocol proto) const
{
  switch (proto) {
    case SdpMediaSection::kRtpAvpf:
    case SdpMediaSection::kDccpRtpAvpf:
    case SdpMediaSection::kDccpRtpSavpf:
    case SdpMediaSection::kRtpSavpf:
    case SdpMediaSection::kUdpTlsRtpSavpf:
    case SdpMediaSection::kTcpTlsRtpSavpf:
    case SdpMediaSection::kDccpTlsRtpSavpf:
      return true;
    case SdpMediaSection::kRtpAvp:
    case SdpMediaSection::kUdp:
    case SdpMediaSection::kVat:
    case SdpMediaSection::kRtp:
    case SdpMediaSection::kUdptl:
    case SdpMediaSection::kTcp:
    case SdpMediaSection::kTcpRtpAvp:
    case SdpMediaSection::kRtpSavp:
    case SdpMediaSection::kTcpBfcp:
    case SdpMediaSection::kTcpTlsBfcp:
    case SdpMediaSection::kTcpTls:
    case SdpMediaSection::kFluteUdp:
    case SdpMediaSection::kTcpMsrp:
    case SdpMediaSection::kTcpTlsMsrp:
    case SdpMediaSection::kDccp:
    case SdpMediaSection::kDccpRtpAvp:
    case SdpMediaSection::kDccpRtpSavp:
    case SdpMediaSection::kUdpTlsRtpSavp:
    case SdpMediaSection::kTcpTlsRtpSavp:
    case SdpMediaSection::kDccpTlsRtpSavp:
    case SdpMediaSection::kUdpMbmsFecRtpAvp:
    case SdpMediaSection::kUdpMbmsFecRtpSavp:
    case SdpMediaSection::kUdpMbmsRepair:
    case SdpMediaSection::kFecUdp:
    case SdpMediaSection::kUdpFec:
    case SdpMediaSection::kTcpMrcpv2:
    case SdpMediaSection::kTcpTlsMrcpv2:
    case SdpMediaSection::kPstn:
    case SdpMediaSection::kUdpTlsUdptl:
    case SdpMediaSection::kSctp:
    case SdpMediaSection::kSctpDtls:
    case SdpMediaSection::kDtlsSctp:
      return false;
  }
  MOZ_CRASH("Unknown protocol, probably corruption.");
}

SdpMediaSection::Protocol
SdpHelper::GetProtocolForMediaType(SdpMediaSection::MediaType type)
{
  if (type == SdpMediaSection::kApplication) {
    return SdpMediaSection::kDtlsSctp;
  }

  return SdpMediaSection::kUdpTlsRtpSavpf;
}

void
SdpHelper::appendSdpParseErrors(
    const std::vector<std::pair<size_t, std::string> >& aErrors,
    std::string* aErrorString)
{
  std::ostringstream os;
  for (auto i = aErrors.begin(); i != aErrors.end(); ++i) {
    os << "SDP Parse Error on line " << i->first << ": " + i->second
       << std::endl;
  }
  *aErrorString += os.str();
}

/* static */ bool
SdpHelper::GetPtAsInt(const std::string& ptString, uint16_t* ptOutparam)
{
  char* end;
  unsigned long pt = strtoul(ptString.c_str(), &end, 10);
  size_t length = static_cast<size_t>(end - ptString.c_str());
  if ((pt > UINT16_MAX) || (length != ptString.size())) {
    return false;
  }
  *ptOutparam = pt;
  return true;
}

void
SdpHelper::AddCommonExtmaps(
    const SdpMediaSection& remoteMsection,
    const std::vector<SdpExtmapAttributeList::Extmap>& localExtensions,
    SdpMediaSection* localMsection)
{
  if (!remoteMsection.GetAttributeList().HasAttribute(
        SdpAttribute::kExtmapAttribute)) {
    return;
  }

  UniquePtr<SdpExtmapAttributeList> localExtmap(new SdpExtmapAttributeList);
  auto& theirExtmap = remoteMsection.GetAttributeList().GetExtmap().mExtmaps;
  for (auto i = theirExtmap.begin(); i != theirExtmap.end(); ++i) {
    for (auto j = localExtensions.begin(); j != localExtensions.end(); ++j) {
      // verify we have a valid combination of directions.  For kInactive
      // we'll just not add the response
      if (i->extensionname == j->extensionname &&
          (((i->direction == SdpDirectionAttribute::Direction::kSendrecv ||
             i->direction == SdpDirectionAttribute::Direction::kSendonly) &&
            (j->direction == SdpDirectionAttribute::Direction::kSendrecv ||
             j->direction == SdpDirectionAttribute::Direction::kRecvonly)) ||

           ((i->direction == SdpDirectionAttribute::Direction::kSendrecv ||
             i->direction == SdpDirectionAttribute::Direction::kRecvonly) &&
            (j->direction == SdpDirectionAttribute::Direction::kSendrecv ||
             j->direction == SdpDirectionAttribute::Direction::kSendonly)))) {
        auto k = *i; // we need to modify it
        if (j->direction == SdpDirectionAttribute::Direction::kSendonly) {
          k.direction = SdpDirectionAttribute::Direction::kRecvonly;
        } else if (j->direction == SdpDirectionAttribute::Direction::kRecvonly) {
          k.direction = SdpDirectionAttribute::Direction::kSendonly;
        }
        localExtmap->mExtmaps.push_back(k);

        // RFC 5285 says that ids >= 4096 can be used by the offerer to
        // force the answerer to pick, otherwise the value in the offer is
        // used.
        if (localExtmap->mExtmaps.back().entry >= 4096) {
          localExtmap->mExtmaps.back().entry = j->entry;
        }
      }
    }
  }

  if (!localExtmap->mExtmaps.empty()) {
    localMsection->GetAttributeList().SetAttribute(localExtmap.release());
  }
}

SdpHelper::MsectionBundleType
SdpHelper::GetMsectionBundleType(const Sdp& sdp,
                                 uint16_t level,
                                 BundledMids& bundledMids,
                                 std::string* masterMid) const
{
  const SdpMediaSection& msection = sdp.GetMediaSection(level);
  if (msection.GetAttributeList().HasAttribute(SdpAttribute::kMidAttribute)) {
    std::string mid(msection.GetAttributeList().GetMid());
    if (bundledMids.count(mid)) {
      const SdpMediaSection* masterBundleMsection(bundledMids[mid]);
      if (msection.GetLevel() != masterBundleMsection->GetLevel()) {
        return kSlaveBundle;
      }

      // allow the caller not to care about the masterMid
      if (masterMid) {
        *masterMid = mid;
      }
      return kMasterBundle;
    }
  }
  return kNoBundle;
}

} // namespace mozilla


