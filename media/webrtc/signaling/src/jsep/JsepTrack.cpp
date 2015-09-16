/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "signaling/src/jsep/JsepTrack.h"
#include "signaling/src/jsep/JsepCodecDescription.h"

#include <algorithm>

namespace mozilla
{
void
JsepTrack::GetNegotiatedPayloadTypes(std::vector<uint16_t>* payloadTypes)
{
  if (!mNegotiatedDetails) {
    return;
  }

  GetPayloadTypes(mNegotiatedDetails->mCodecs.values, payloadTypes);
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
}

void
JsepTrack::AddToAnswer(const SdpMediaSection& offer,
                       SdpMediaSection* answer) const
{
  // We do not modify mPrototypeCodecs here, since we're only creating an answer. Once
  // offer/answer concludes, we will update mPrototypeCodecs.
  PtrVector<JsepCodecDescription> codecs;
  codecs.values = GetCodecClones();
  NegotiateCodecs(offer, &codecs.values);
  if (codecs.values.empty()) {
    return;
  }

  AddToMsection(codecs.values, answer);
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
    const SdpMediaSection* answer,
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
        if (answer) {
          // Answer's formats are authoritative, and they might be different
          for (const std::string& answerFmt : answer->GetFormats()) {
            if (codec->Matches(answerFmt, *answer)) {
              codec->mDefaultPt = answerFmt;
              break; // We found the corresponding format in |answer|, bail
            }
          }
        }
        if (formatChanges) {
          (*formatChanges)[originalFormat] = codec->mDefaultPt;
        }
        break;
      }
    }
  }

  // Make sure strongly preferred codecs are up front, overriding the remote
  // side's preference.
  std::stable_sort(codecs->begin(), codecs->end(), CompareCodec);

  // TODO(bug 814227): Remove this once we're ready to put multiple codecs in an
  // answer
  if (!codecs->empty()) {
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
  UniquePtr<JsepTrackNegotiatedDetails> negotiatedDetails =
      MakeUnique<JsepTrackNegotiatedDetails>();

  negotiatedDetails->mCodecs.values = GetCodecClones();
  std::map<std::string, std::string> formatChanges;
  NegotiateCodecs(remote,
                  &negotiatedDetails->mCodecs.values,
                  &answer,
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

  if (answer.GetAttributeList().HasAttribute(SdpAttribute::kExtmapAttribute)) {
    for (auto& extmapAttr : answer.GetAttributeList().GetExtmap().mExtmaps) {
      negotiatedDetails->mExtmap[extmapAttr.extensionname] = extmapAttr;
    }
  }

  if ((mDirection == sdp::kRecv) &&
      remote.GetAttributeList().HasAttribute(SdpAttribute::kSsrcAttribute)) {
    for (auto& ssrcAttr : remote.GetAttributeList().GetSsrc().mSsrcs) {
      AddSsrc(ssrcAttr.ssrc);
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

