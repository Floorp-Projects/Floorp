/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "signaling/src/jsep/JsepTrack.h"
#include "signaling/src/jsep/JsepCodecDescription.h"
#include "signaling/src/jsep/JsepTrackEncoding.h"

#include <algorithm>
#include <iostream>

namespace mozilla {
void JsepTrack::GetNegotiatedPayloadTypes(
    std::vector<uint16_t>* payloadTypes) const {
  if (!mNegotiatedDetails) {
    return;
  }

  for (const auto& encoding : mNegotiatedDetails->mEncodings) {
    GetPayloadTypes(encoding->GetCodecs(), payloadTypes);
  }

  // Prune out dupes
  std::sort(payloadTypes->begin(), payloadTypes->end());
  auto newEnd = std::unique(payloadTypes->begin(), payloadTypes->end());
  payloadTypes->erase(newEnd, payloadTypes->end());
}

/* static */
void JsepTrack::GetPayloadTypes(
    const std::vector<UniquePtr<JsepCodecDescription>>& codecs,
    std::vector<uint16_t>* payloadTypes) {
  for (const auto& codec : codecs) {
    uint16_t pt;
    if (!codec->GetPtAsInt(&pt)) {
      MOZ_ASSERT(false);
      continue;
    }
    payloadTypes->push_back(pt);
  }
}

void JsepTrack::EnsureNoDuplicatePayloadTypes(
    std::vector<UniquePtr<JsepCodecDescription>>* codecs) {
  std::set<uint16_t> uniquePayloadTypes;

  for (auto& codec : *codecs) {
    // We assume there are no dupes in negotiated codecs; unnegotiated codecs
    // need to change if there is a clash.
    if (!codec->mEnabled ||
        // We only support one datachannel per m-section
        !codec->mName.compare("webrtc-datachannel")) {
      continue;
    }

    // Disable, and only re-enable if we can ensure it has a unique pt.
    codec->mEnabled = false;

    uint16_t currentPt;
    if (!codec->GetPtAsInt(&currentPt)) {
      MOZ_ASSERT(false);
      continue;
    }

    if (!uniquePayloadTypes.count(currentPt)) {
      codec->mEnabled = true;
      uniquePayloadTypes.insert(currentPt);
      continue;
    }

    // |codec| cannot use its current payload type. Try to find another.
    for (uint16_t freePt = 96; freePt <= 127; ++freePt) {
      // Not super efficient, but readability is probably more important.
      if (!uniquePayloadTypes.count(freePt)) {
        uniquePayloadTypes.insert(freePt);
        codec->mEnabled = true;
        std::ostringstream os;
        os << freePt;
        codec->mDefaultPt = os.str();
        break;
      }
    }
  }
}

void JsepTrack::EnsureSsrcs(SsrcGenerator& ssrcGenerator) {
  if (mSsrcs.empty()) {
    uint32_t ssrc;
    if (!ssrcGenerator.GenerateSsrc(&ssrc)) {
      return;
    }
    mSsrcs.push_back(ssrc);
  }
}

void JsepTrack::PopulateCodecs(
    const std::vector<UniquePtr<JsepCodecDescription>>& prototype) {
  mPrototypeCodecs.clear();
  for (const auto& prototypeCodec : prototype) {
    if (prototypeCodec->mType == mType) {
      mPrototypeCodecs.emplace_back(prototypeCodec->Clone());
      mPrototypeCodecs.back()->mDirection = mDirection;
    }
  }

  EnsureNoDuplicatePayloadTypes(&mPrototypeCodecs);
}

void JsepTrack::AddToOffer(SsrcGenerator& ssrcGenerator,
                           SdpMediaSection* offer) {
  AddToMsection(mPrototypeCodecs, offer);

  if (mDirection == sdp::kSend) {
    std::vector<JsConstraints> constraints;
    if (offer->IsSending()) {
      constraints = mJsEncodeConstraints;
    }
    AddToMsection(constraints, sdp::kSend, ssrcGenerator, offer);
  }
}

void JsepTrack::AddToAnswer(const SdpMediaSection& offer,
                            SsrcGenerator& ssrcGenerator,
                            SdpMediaSection* answer) {
  // We do not modify mPrototypeCodecs here, since we're only creating an
  // answer. Once offer/answer concludes, we will update mPrototypeCodecs.
  std::vector<UniquePtr<JsepCodecDescription>> codecs =
      NegotiateCodecs(offer, true);
  if (codecs.empty()) {
    return;
  }

  AddToMsection(codecs, answer);

  if (mDirection == sdp::kSend) {
    std::vector<JsConstraints> constraints;
    if (answer->IsSending()) {
      constraints = mJsEncodeConstraints;
      std::vector<SdpRidAttributeList::Rid> rids;
      GetRids(offer, sdp::kRecv, &rids);
      NegotiateRids(rids, &constraints);
    }
    AddToMsection(constraints, sdp::kSend, ssrcGenerator, answer);
  }
}

bool JsepTrack::SetJsConstraints(
    const std::vector<JsConstraints>& constraintsList) {
  bool constraintsChanged = mJsEncodeConstraints != constraintsList;
  mJsEncodeConstraints = constraintsList;

  // Also update negotiated details with constraints, as these can change
  // without negotiation.

  if (!mNegotiatedDetails) {
    return constraintsChanged;
  }

  for (auto& encoding : mNegotiatedDetails->mEncodings) {
    for (const JsConstraints& jsConstraints : mJsEncodeConstraints) {
      if (jsConstraints.rid == encoding->mRid) {
        encoding->mConstraints = jsConstraints.constraints;
      }
    }
  }

  return constraintsChanged;
}

void JsepTrack::AddToMsection(
    const std::vector<UniquePtr<JsepCodecDescription>>& codecs,
    SdpMediaSection* msection) {
  MOZ_ASSERT(msection->GetMediaType() == mType);
  MOZ_ASSERT(!codecs.empty());

  for (const auto& codec : codecs) {
    codec->AddToMediaSection(*msection);
  }

  if ((mDirection == sdp::kSend) && (mType != SdpMediaSection::kApplication) &&
      msection->IsSending()) {
    if (mStreamIds.empty()) {
      msection->AddMsid("-", mTrackId);
    } else {
      for (const std::string& streamId : mStreamIds) {
        msection->AddMsid(streamId, mTrackId);
      }
    }
  }
}

// Updates the |id| values in |constraintsList| with the rid values in |rids|,
// where necessary.
void JsepTrack::NegotiateRids(
    const std::vector<SdpRidAttributeList::Rid>& rids,
    std::vector<JsConstraints>* constraintsList) const {
  for (const SdpRidAttributeList::Rid& rid : rids) {
    if (!FindConstraints(rid.id, *constraintsList)) {
      // Pair up the first JsConstraints with an empty id, if it exists.
      JsConstraints* constraints = FindConstraints("", *constraintsList);
      if (constraints) {
        constraints->rid = rid.id;
      }
    }
  }
}

void JsepTrack::UpdateSsrcs(SsrcGenerator& ssrcGenerator, size_t encodings) {
  MOZ_ASSERT(mDirection == sdp::kSend);
  MOZ_ASSERT(mType != SdpMediaSection::kApplication);
  size_t numSsrcs = std::max<size_t>(encodings, 1U);

  // Right now, the spec does not permit changing the number of encodings after
  // the initial creation of the sender, so we don't need to worry about things
  // like a new encoding inserted in between two pre-existing encodings.
  while (mSsrcs.size() < numSsrcs) {
    uint32_t ssrc;
    if (!ssrcGenerator.GenerateSsrc(&ssrc)) {
      return;
    }
    mSsrcs.push_back(ssrc);
  }

  mSsrcs.resize(numSsrcs);
  MOZ_ASSERT(!mSsrcs.empty());
}

void JsepTrack::AddToMsection(const std::vector<JsConstraints>& constraintsList,
                              sdp::Direction direction,
                              SsrcGenerator& ssrcGenerator,
                              SdpMediaSection* msection) {
  UniquePtr<SdpSimulcastAttribute> simulcast(new SdpSimulcastAttribute);
  UniquePtr<SdpRidAttributeList> rids(new SdpRidAttributeList);
  for (const JsConstraints& constraints : constraintsList) {
    if (!constraints.rid.empty()) {
      SdpRidAttributeList::Rid rid;
      rid.id = constraints.rid;
      rid.direction = direction;
      rids->mRids.push_back(rid);

      SdpSimulcastAttribute::Version version;
      version.choices.push_back(constraints.rid);
      if (direction == sdp::kSend) {
        simulcast->sendVersions.push_back(version);
      } else {
        simulcast->recvVersions.push_back(version);
      }
    }
  }

  if (rids->mRids.size() > 1) {
    msection->GetAttributeList().SetAttribute(simulcast.release());
    msection->GetAttributeList().SetAttribute(rids.release());
  }

  if (mType != SdpMediaSection::kApplication && mDirection == sdp::kSend) {
    UpdateSsrcs(ssrcGenerator, constraintsList.size());
    msection->SetSsrcs(mSsrcs, mCNAME);
  }
}

void JsepTrack::GetRids(const SdpMediaSection& msection,
                        sdp::Direction direction,
                        std::vector<SdpRidAttributeList::Rid>* rids) const {
  rids->clear();
  if (!msection.GetAttributeList().HasAttribute(
          SdpAttribute::kSimulcastAttribute)) {
    return;
  }

  const SdpSimulcastAttribute& simulcast(
      msection.GetAttributeList().GetSimulcast());

  const SdpSimulcastAttribute::Versions* versions = nullptr;
  switch (direction) {
    case sdp::kSend:
      versions = &simulcast.sendVersions;
      break;
    case sdp::kRecv:
      versions = &simulcast.recvVersions;
      break;
  }

  if (!versions->IsSet()) {
    return;
  }

  if (versions->type != SdpSimulcastAttribute::Versions::kRid) {
    // No support for PT-based simulcast, yet.
    return;
  }

  for (const SdpSimulcastAttribute::Version& version : *versions) {
    if (!version.choices.empty()) {
      // We validate that rids are present (and sane) elsewhere.
      rids->push_back(*msection.FindRid(version.choices[0]));
    }
  }
}

JsepTrack::JsConstraints* JsepTrack::FindConstraints(
    const std::string& id, std::vector<JsConstraints>& constraintsList) const {
  for (JsConstraints& constraints : constraintsList) {
    if (constraints.rid == id) {
      return &constraints;
    }
  }
  return nullptr;
}

void JsepTrack::CreateEncodings(
    const SdpMediaSection& remote,
    const std::vector<UniquePtr<JsepCodecDescription>>& negotiatedCodecs,
    JsepTrackNegotiatedDetails* negotiatedDetails) {
  negotiatedDetails->mTias = remote.GetBandwidth("TIAS");
  // TODO add support for b=AS if TIAS is not set (bug 976521)

  std::vector<SdpRidAttributeList::Rid> rids;
  GetRids(remote, sdp::kRecv, &rids);  // Get rids we will send
  NegotiateRids(rids, &mJsEncodeConstraints);
  if (rids.empty()) {
    // Add dummy value with an empty id to make sure we get a single unicast
    // stream.
    rids.push_back(SdpRidAttributeList::Rid());
  }

  size_t max_streams = 1;

  if (!mJsEncodeConstraints.empty()) {
    max_streams = std::min(rids.size(), mJsEncodeConstraints.size());
  }
  // Drop SSRCs if less RIDs were offered than we have encoding constraints
  // Just in case.
  if (mSsrcs.size() > max_streams) {
    mSsrcs.resize(max_streams);
  }

  // For each stream make sure we have an encoding, and configure
  // that encoding appropriately.
  for (size_t i = 0; i < max_streams; ++i) {
    if (i == negotiatedDetails->mEncodings.size()) {
      negotiatedDetails->mEncodings.emplace_back(new JsepTrackEncoding);
    }

    auto& encoding = negotiatedDetails->mEncodings[i];

    for (const auto& codec : negotiatedCodecs) {
      if (rids[i].HasFormat(codec->mDefaultPt)) {
        encoding->AddCodec(*codec);
      }
    }

    encoding->mRid = rids[i].id;
    // If we end up supporting params for rid, we would handle that here.

    // Incorporate the corresponding JS encoding constraints, if they exist
    for (const JsConstraints& jsConstraints : mJsEncodeConstraints) {
      if (jsConstraints.rid == rids[i].id) {
        encoding->mConstraints = jsConstraints.constraints;
      }
    }
  }
}

std::vector<UniquePtr<JsepCodecDescription>> JsepTrack::GetCodecClones() const {
  std::vector<UniquePtr<JsepCodecDescription>> clones;
  for (const auto& codec : mPrototypeCodecs) {
    clones.emplace_back(codec->Clone());
  }
  return clones;
}

static bool CompareCodec(const UniquePtr<JsepCodecDescription>& lhs,
                         const UniquePtr<JsepCodecDescription>& rhs) {
  return lhs->mStronglyPreferred && !rhs->mStronglyPreferred;
}

std::vector<UniquePtr<JsepCodecDescription>> JsepTrack::NegotiateCodecs(
    const SdpMediaSection& remote, bool isOffer) {
  std::vector<UniquePtr<JsepCodecDescription>> negotiatedCodecs;
  std::vector<UniquePtr<JsepCodecDescription>> newPrototypeCodecs;

  // Outer loop establishes the remote side's preference
  for (const std::string& fmt : remote.GetFormats()) {
    for (auto& codec : mPrototypeCodecs) {
      if (!codec || !codec->mEnabled || !codec->Matches(fmt, remote)) {
        continue;
      }

      // First codec of ours that matches. See if we can negotiate it.
      UniquePtr<JsepCodecDescription> clone(codec->Clone());
      if (clone->Negotiate(fmt, remote, isOffer)) {
        // If negotiation changed the payload type, remember that for later
        codec->mDefaultPt = clone->mDefaultPt;
        // Moves the codec out of mPrototypeCodecs, leaving an empty
        // UniquePtr, so we don't use it again. Also causes successfully
        // negotiated codecs to be placed up front in the future.
        newPrototypeCodecs.emplace_back(std::move(codec));
        negotiatedCodecs.emplace_back(std::move(clone));
        break;
      }
    }
  }

  // newPrototypeCodecs contains just the negotiated stuff so far. Add the rest.
  for (auto& codec : mPrototypeCodecs) {
    if (codec) {
      newPrototypeCodecs.emplace_back(std::move(codec));
    }
  }

  // Negotiated stuff is up front, so it will take precedence when ensuring
  // there are no duplicate payload types.
  EnsureNoDuplicatePayloadTypes(&newPrototypeCodecs);
  std::swap(newPrototypeCodecs, mPrototypeCodecs);

  // Find the (potential) red codec and ulpfec codec or telephone-event
  JsepVideoCodecDescription* red = nullptr;
  JsepVideoCodecDescription* ulpfec = nullptr;
  JsepAudioCodecDescription* dtmf = nullptr;
  // We can safely cast here since JsepTrack has a MediaType and only codecs
  // that match that MediaType (kAudio or kVideo) are added.
  for (auto& codec : negotiatedCodecs) {
    if (codec->mName == "red") {
      red = static_cast<JsepVideoCodecDescription*>(codec.get());
    } else if (codec->mName == "ulpfec") {
      ulpfec = static_cast<JsepVideoCodecDescription*>(codec.get());
    } else if (codec->mName == "telephone-event") {
      dtmf = static_cast<JsepAudioCodecDescription*>(codec.get());
    }
  }
  // if we have a red codec remove redundant encodings that don't exist
  if (red) {
    // Since we could have an externally specified redundant endcodings
    // list, we shouldn't simply rebuild the redundant encodings list
    // based on the current list of codecs.
    std::vector<uint8_t> unnegotiatedEncodings;
    std::swap(unnegotiatedEncodings, red->mRedundantEncodings);
    for (auto redundantPt : unnegotiatedEncodings) {
      std::string pt = std::to_string(redundantPt);
      for (const auto& codec : negotiatedCodecs) {
        if (pt == codec->mDefaultPt) {
          red->mRedundantEncodings.push_back(redundantPt);
          break;
        }
      }
    }
  }
  // Video FEC is indicated by the existence of the red and ulpfec
  // codecs and not an attribute on the particular video codec (like in
  // a rtcpfb attr). If we see both red and ulpfec codecs, we enable FEC
  // on all the other codecs.
  if (red && ulpfec) {
    for (auto& codec : negotiatedCodecs) {
      if (codec->mName != "red" && codec->mName != "ulpfec") {
        JsepVideoCodecDescription* videoCodec =
            static_cast<JsepVideoCodecDescription*>(codec.get());
        videoCodec->EnableFec(red->mDefaultPt, ulpfec->mDefaultPt);
      }
    }
  }

  // Dtmf support is indicated by the existence of the telephone-event
  // codec, and not an attribute on the particular audio codec (like in a
  // rtcpfb attr). If we see the telephone-event codec, we enabled dtmf
  // support on all the other audio codecs.
  if (dtmf) {
    for (auto& codec : negotiatedCodecs) {
      JsepAudioCodecDescription* audioCodec =
          static_cast<JsepAudioCodecDescription*>(codec.get());
      audioCodec->mDtmfEnabled = true;
    }
  }

  // Make sure strongly preferred codecs are up front, overriding the remote
  // side's preference.
  std::stable_sort(negotiatedCodecs.begin(), negotiatedCodecs.end(),
                   CompareCodec);

  // TODO(bug 814227): Remove this once we're ready to put multiple codecs in an
  // answer.  For now, remove all but the first codec unless the red codec
  // exists, in which case we include the others per RFC 5109, section 14.2.
  if (!negotiatedCodecs.empty() && !red) {
    std::vector<UniquePtr<JsepCodecDescription>> codecsToKeep;

    bool foundPreferredCodec = false;
    for (auto& codec : negotiatedCodecs) {
      if (codec.get() == dtmf) {
        codecsToKeep.push_back(std::move(codec));
        // TODO: keep ulpfec when we enable it in Bug 875922
        // } else if (codec.get() == ulpfec) {
        //   codecsToKeep.push_back(std::move(codec));
      } else if (!foundPreferredCodec) {
        codecsToKeep.insert(codecsToKeep.begin(), std::move(codec));
        foundPreferredCodec = true;
      }
    }

    negotiatedCodecs = std::move(codecsToKeep);
  }

  return negotiatedCodecs;
}

void JsepTrack::Negotiate(const SdpMediaSection& answer,
                          const SdpMediaSection& remote) {
  std::vector<UniquePtr<JsepCodecDescription>> negotiatedCodecs =
      NegotiateCodecs(remote, &answer != &remote);

  UniquePtr<JsepTrackNegotiatedDetails> negotiatedDetails =
      MakeUnique<JsepTrackNegotiatedDetails>();

  CreateEncodings(remote, negotiatedCodecs, negotiatedDetails.get());

  if (answer.GetAttributeList().HasAttribute(SdpAttribute::kExtmapAttribute)) {
    for (auto& extmapAttr : answer.GetAttributeList().GetExtmap().mExtmaps) {
      SdpDirectionAttribute::Direction direction = extmapAttr.direction;
      if (&remote == &answer) {
        // Answer is remote, we need to flip this.
        direction = reverse(direction);
      }

      if (direction & mDirection) {
        negotiatedDetails->mExtmap[extmapAttr.extensionname] = extmapAttr;
      }
    }
  }

  mNegotiatedDetails = std::move(negotiatedDetails);
}

// When doing bundle, if all else fails we can try to figure out which m-line a
// given RTP packet belongs to by looking at the payload type field. This only
// works, however, if that payload type appeared in only one m-section.
// We figure that out here.
/* static */
void JsepTrack::SetUniquePayloadTypes(std::vector<JsepTrack*>& tracks) {
  // Maps to track details if no other track contains the payload type,
  // otherwise maps to nullptr.
  std::map<uint16_t, JsepTrackNegotiatedDetails*> payloadTypeToDetailsMap;

  for (JsepTrack* track : tracks) {
    if (track->GetMediaType() == SdpMediaSection::kApplication) {
      continue;
    }

    auto* details = track->GetNegotiatedDetails();
    if (!details) {
      // Can happen if negotiation fails on a track
      continue;
    }

    std::vector<uint16_t> payloadTypesForTrack;
    track->GetNegotiatedPayloadTypes(&payloadTypesForTrack);

    for (uint16_t pt : payloadTypesForTrack) {
      if (payloadTypeToDetailsMap.count(pt)) {
        // Found in more than one track, not unique
        payloadTypeToDetailsMap[pt] = nullptr;
      } else {
        payloadTypeToDetailsMap[pt] = details;
      }
    }
  }

  for (auto ptAndDetails : payloadTypeToDetailsMap) {
    uint16_t uniquePt = ptAndDetails.first;
    MOZ_ASSERT(uniquePt <= UINT8_MAX);
    auto trackDetails = ptAndDetails.second;

    if (trackDetails) {
      trackDetails->mUniquePayloadTypes.push_back(
          static_cast<uint8_t>(uniquePt));
    }
  }
}

}  // namespace mozilla
