/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "signaling/src/jsep/JsepTrack.h"
#include "signaling/src/jsep/JsepCodecDescription.h"
#include "signaling/src/jsep/JsepTrackEncoding.h"

#include <algorithm>

namespace mozilla
{
void
JsepTrack::GetNegotiatedPayloadTypes(std::vector<uint16_t>* payloadTypes)
{
  if (!mNegotiatedDetails) {
    return;
  }

  for (const auto* encoding : mNegotiatedDetails->mEncodings.values) {
    GetPayloadTypes(encoding->GetCodecs(), payloadTypes);
  }

  // Prune out dupes
  std::sort(payloadTypes->begin(), payloadTypes->end());
  auto newEnd = std::unique(payloadTypes->begin(), payloadTypes->end());
  payloadTypes->erase(newEnd, payloadTypes->end());
}

/* static */
void
JsepTrack::GetPayloadTypes(
    const std::vector<JsepCodecDescription*>& codecs,
    std::vector<uint16_t>* payloadTypes)
{
  for (JsepCodecDescription* codec : codecs) {
    uint16_t pt;
    if (!codec->GetPtAsInt(&pt)) {
      MOZ_ASSERT(false);
      continue;
    }
    payloadTypes->push_back(pt);
  }
}

void
JsepTrack::EnsureNoDuplicatePayloadTypes(
    std::vector<JsepCodecDescription*>* codecs)
{
  std::set<uint16_t> uniquePayloadTypes;

  for (JsepCodecDescription* codec : *codecs) {
    // We assume there are no dupes in negotiated codecs; unnegotiated codecs
    // need to change if there is a clash.
    if (!codec->mEnabled) {
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

void
JsepTrack::PopulateCodecs(const std::vector<JsepCodecDescription*>& prototype)
{
  for (const JsepCodecDescription* prototypeCodec : prototype) {
    if (prototypeCodec->mType == mType) {
      mPrototypeCodecs.values.push_back(prototypeCodec->Clone());
      mPrototypeCodecs.values.back()->mDirection = mDirection;
    }
  }

  EnsureNoDuplicatePayloadTypes(&mPrototypeCodecs.values);
}

void
JsepTrack::AddToOffer(SdpMediaSection* offer) const
{
  AddToMsection(mPrototypeCodecs.values, offer);
  if (mDirection == sdp::kSend) {
    AddToMsection(mJsEncodeConstraints, sdp::kSend, offer);
  }
}

void
JsepTrack::AddToAnswer(const SdpMediaSection& offer,
                       SdpMediaSection* answer) const
{
  // We do not modify mPrototypeCodecs here, since we're only creating an
  // answer. Once offer/answer concludes, we will update mPrototypeCodecs.
  PtrVector<JsepCodecDescription> codecs;
  codecs.values = GetCodecClones();
  NegotiateCodecs(offer, &codecs.values);
  if (codecs.values.empty()) {
    return;
  }

  AddToMsection(codecs.values, answer);

  if (mDirection == sdp::kSend) {
    std::vector<JsConstraints> constraints;
    std::vector<SdpRidAttributeList::Rid> rids;
    GetRids(offer, sdp::kRecv, &rids);
    NegotiateRids(rids, &constraints);
    AddToMsection(constraints, sdp::kSend, answer);
  }
}

void
JsepTrack::AddToMsection(const std::vector<JsepCodecDescription*>& codecs,
                         SdpMediaSection* msection) const
{
  MOZ_ASSERT(msection->GetMediaType() == mType);
  MOZ_ASSERT(!codecs.empty());

  for (const JsepCodecDescription* codec : codecs) {
    codec->AddToMediaSection(*msection);
  }

  if (mDirection == sdp::kSend) {
    if (msection->GetMediaType() != SdpMediaSection::kApplication) {
      msection->SetSsrcs(mSsrcs, mCNAME);
      msection->AddMsid(mStreamId, mTrackId);
    }
    msection->SetSending(true);
  } else {
    msection->SetReceiving(true);
  }
}

// Updates the |id| values in |constraintsList| with the rid values in |rids|,
// where necessary.
void
JsepTrack::NegotiateRids(const std::vector<SdpRidAttributeList::Rid>& rids,
                         std::vector<JsConstraints>* constraintsList) const
{
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

/* static */
void
JsepTrack::AddToMsection(const std::vector<JsConstraints>& constraintsList,
                         sdp::Direction direction,
                         SdpMediaSection* msection)
{
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

  if (!rids->mRids.empty()) {
    msection->GetAttributeList().SetAttribute(simulcast.release());
    msection->GetAttributeList().SetAttribute(rids.release());
  }
}

void
JsepTrack::GetRids(const SdpMediaSection& msection,
                   sdp::Direction direction,
                   std::vector<SdpRidAttributeList::Rid>* rids) const
{
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

JsepTrack::JsConstraints*
JsepTrack::FindConstraints(const std::string& id,
                           std::vector<JsConstraints>& constraintsList) const
{
  for (JsConstraints& constraints : constraintsList) {
    if (constraints.rid == id) {
      return &constraints;
    }
  }
  return nullptr;
}

void
JsepTrack::CreateEncodings(
    const SdpMediaSection& remote,
    const std::vector<JsepCodecDescription*>& negotiatedCodecs,
    JsepTrackNegotiatedDetails* negotiatedDetails)
{
  std::vector<SdpRidAttributeList::Rid> rids;
  GetRids(remote, sdp::kRecv, &rids); // Get rids we will send
  NegotiateRids(rids, &mJsEncodeConstraints);
  if (rids.empty()) {
    // Add dummy value with an empty id to make sure we get a single unicast
    // stream.
    rids.push_back(SdpRidAttributeList::Rid());
  }

  // For each rid in the remote, make sure we have an encoding, and configure
  // that encoding appropriately.
  for (size_t i = 0; i < rids.size(); ++i) {
    if (i == negotiatedDetails->mEncodings.values.size()) {
      negotiatedDetails->mEncodings.values.push_back(new JsepTrackEncoding);
    }

    JsepTrackEncoding* encoding = negotiatedDetails->mEncodings.values[i];

    for (const JsepCodecDescription* codec : negotiatedCodecs) {
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

    encoding->UpdateMaxBitrate(remote);
  }
}

std::vector<JsepCodecDescription*>
JsepTrack::GetCodecClones() const
{
  std::vector<JsepCodecDescription*> clones;
  for (const JsepCodecDescription* codec : mPrototypeCodecs.values) {
    clones.push_back(codec->Clone());
  }
  return clones;
}

static bool
CompareCodec(const JsepCodecDescription* lhs, const JsepCodecDescription* rhs)
{
  return lhs->mStronglyPreferred && !rhs->mStronglyPreferred;
}

void
JsepTrack::NegotiateCodecs(
    const SdpMediaSection& remote,
    std::vector<JsepCodecDescription*>* codecs,
    std::map<std::string, std::string>* formatChanges) const
{
  PtrVector<JsepCodecDescription> unnegotiatedCodecs;
  std::swap(unnegotiatedCodecs.values, *codecs);

  // Outer loop establishes the remote side's preference
  for (const std::string& fmt : remote.GetFormats()) {
    for (size_t i = 0; i < unnegotiatedCodecs.values.size(); ++i) {
      JsepCodecDescription* codec = unnegotiatedCodecs.values[i];
      if (!codec || !codec->mEnabled || !codec->Matches(fmt, remote)) {
        continue;
      }

      std::string originalFormat = codec->mDefaultPt;
      if(codec->Negotiate(fmt, remote)) {
        codecs->push_back(codec);
        unnegotiatedCodecs.values[i] = nullptr;
        if (formatChanges) {
          (*formatChanges)[originalFormat] = codec->mDefaultPt;
        }
        break;
      }
    }
  }

  // Find the (potential) red codec and ulpfec codec
  JsepVideoCodecDescription* red = nullptr;
  JsepVideoCodecDescription* ulpfec = nullptr;
  for (auto codec : *codecs) {
    if (codec->mName == "red") {
      red = static_cast<JsepVideoCodecDescription*>(codec);
      break;
    }
    if (codec->mName == "ulpfec") {
      ulpfec = static_cast<JsepVideoCodecDescription*>(codec);
      break;
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
      for (auto codec : *codecs) {
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
    for (auto codec : *codecs) {
      if (codec->mName != "red" && codec->mName != "ulpfec") {
        JsepVideoCodecDescription* videoCodec =
            static_cast<JsepVideoCodecDescription*>(codec);
        videoCodec->EnableFec();
      }
    }
  }

  // Make sure strongly preferred codecs are up front, overriding the remote
  // side's preference.
  std::stable_sort(codecs->begin(), codecs->end(), CompareCodec);

  // TODO(bug 814227): Remove this once we're ready to put multiple codecs in an
  // answer.  For now, remove all but the first codec unless the red codec
  // exists, and then we include the others per RFC 5109, section 14.2.
  if (!codecs->empty() && !red) {
    for (size_t i = 1; i < codecs->size(); ++i) {
      delete (*codecs)[i];
      (*codecs)[i] = nullptr;
    }
    codecs->resize(1);
  }
}

void
JsepTrack::Negotiate(const SdpMediaSection& answer,
                     const SdpMediaSection& remote)
{
  PtrVector<JsepCodecDescription> negotiatedCodecs;
  negotiatedCodecs.values = GetCodecClones();

  std::map<std::string, std::string> formatChanges;
  NegotiateCodecs(remote,
                  &negotiatedCodecs.values,
                  &formatChanges);

  // Use |formatChanges| to update mPrototypeCodecs
  size_t insertPos = 0;
  for (size_t i = 0; i < mPrototypeCodecs.values.size(); ++i) {
    if (formatChanges.count(mPrototypeCodecs.values[i]->mDefaultPt)) {
      // Update the payload type to what was negotiated
      mPrototypeCodecs.values[i]->mDefaultPt =
        formatChanges[mPrototypeCodecs.values[i]->mDefaultPt];
      // Move this negotiated codec up front
      std::swap(mPrototypeCodecs.values[insertPos],
                mPrototypeCodecs.values[i]);
      ++insertPos;
    }
  }

  EnsureNoDuplicatePayloadTypes(&mPrototypeCodecs.values);

  UniquePtr<JsepTrackNegotiatedDetails> negotiatedDetails =
      MakeUnique<JsepTrackNegotiatedDetails>();

  CreateEncodings(remote, negotiatedCodecs.values, negotiatedDetails.get());

  if (answer.GetAttributeList().HasAttribute(SdpAttribute::kExtmapAttribute)) {
    for (auto& extmapAttr : answer.GetAttributeList().GetExtmap().mExtmaps) {
      negotiatedDetails->mExtmap[extmapAttr.extensionname] = extmapAttr;
    }
  }

  if (mDirection == sdp::kRecv) {
    mSsrcs.clear();
    if (remote.GetAttributeList().HasAttribute(SdpAttribute::kSsrcAttribute)) {
      for (auto& ssrcAttr : remote.GetAttributeList().GetSsrc().mSsrcs) {
        AddSsrc(ssrcAttr.ssrc);
      }
    }
  }

  mNegotiatedDetails = Move(negotiatedDetails);
}

// When doing bundle, if all else fails we can try to figure out which m-line a
// given RTP packet belongs to by looking at the payload type field. This only
// works, however, if that payload type appeared in only one m-section.
// We figure that out here.
/* static */
void
JsepTrack::SetUniquePayloadTypes(const std::vector<RefPtr<JsepTrack>>& tracks)
{
  // Maps to track details if no other track contains the payload type,
  // otherwise maps to nullptr.
  std::map<uint16_t, JsepTrackNegotiatedDetails*> payloadTypeToDetailsMap;

  for (const RefPtr<JsepTrack>& track : tracks) {
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

} // namespace mozilla

