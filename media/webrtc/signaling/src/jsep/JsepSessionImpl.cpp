/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "logging.h"

#include "signaling/src/jsep/JsepSessionImpl.h"
#include <string>
#include <set>
#include <stdlib.h>

#include "nspr.h"
#include "nss.h"
#include "pk11pub.h"
#include "nsDebug.h"

#include <mozilla/Move.h>
#include <mozilla/UniquePtr.h>

#include "signaling/src/jsep/JsepTrack.h"
#include "signaling/src/jsep/JsepTrack.h"
#include "signaling/src/jsep/JsepTrackImpl.h"
#include "signaling/src/jsep/JsepTransport.h"
#include "signaling/src/sdp/Sdp.h"
#include "signaling/src/sdp/SipccSdp.h"
#include "signaling/src/sdp/SipccSdpParser.h"
#include "mozilla/net/DataChannelProtocol.h"

namespace mozilla {

MOZ_MTLOG_MODULE("jsep")

#define JSEP_SET_ERROR(error)                                                  \
  do {                                                                         \
    std::ostringstream os;                                                     \
    os << error;                                                               \
    mLastError = os.str();                                                     \
    MOZ_MTLOG(ML_ERROR, mLastError);                                           \
  } while (0);

nsresult
JsepSessionImpl::Init()
{
  mLastError.clear();

  MOZ_ASSERT(!mSessionId, "Init called more than once");

  nsresult rv = SetupIds();
  NS_ENSURE_SUCCESS(rv, rv);

  SetupDefaultCodecs();
  SetupDefaultRtpExtensions();

  return NS_OK;
}

// Helper function to find the track for a given m= section.
template <class T>
typename std::vector<T>::iterator
FindTrackByLevel(std::vector<T>& tracks, size_t level)
{
  for (auto t = tracks.begin(); t != tracks.end(); ++t) {
    if (t->mAssignedMLine.isSome() &&
        (*t->mAssignedMLine == level)) {
      return t;
    }
  }

  return tracks.end();
}

template <class T>
typename std::vector<T>::iterator
FindTrackByIds(std::vector<T>& tracks,
               const std::string& streamId,
               const std::string& trackId)
{
  for (auto t = tracks.begin(); t != tracks.end(); ++t) {
    if (t->mTrack->GetStreamId() == streamId &&
        (t->mTrack->GetTrackId() == trackId)) {
      return t;
    }
  }

  return tracks.end();
}

template <class T>
typename std::vector<T>::iterator
FindUnassignedTrackByType(std::vector<T>& tracks,
                          SdpMediaSection::MediaType type)
{
  for (auto t = tracks.begin(); t != tracks.end(); ++t) {
    if (!t->mAssignedMLine.isSome() &&
        (t->mTrack->GetMediaType() == type)) {
      return t;
    }
  }

  return tracks.end();
}

nsresult
JsepSessionImpl::AddTrack(const RefPtr<JsepTrack>& track)
{
  mLastError.clear();
  MOZ_ASSERT(track->GetDirection() == JsepTrack::kJsepTrackSending);

  if (track->GetMediaType() != SdpMediaSection::kApplication) {
    track->SetCNAME(mCNAME);

    if (track->GetSsrcs().empty()) {
      uint32_t ssrc;
      do {
        SECStatus rv = PK11_GenerateRandom(
            reinterpret_cast<unsigned char*>(&ssrc), sizeof(ssrc));
        if (rv != SECSuccess) {
          JSEP_SET_ERROR("Failed to generate SSRC, error=" << rv);
          return NS_ERROR_FAILURE;
        }
      } while (mSsrcs.count(ssrc));

      mSsrcs.insert(ssrc);
      track->AddSsrc(ssrc);
    }
  }

  JsepSendingTrack strack;
  strack.mTrack = track;
  strack.mSetInLocalDescription = false;

  mLocalTracks.push_back(strack);

  return NS_OK;
}

nsresult
JsepSessionImpl::RemoveTrack(const std::string& streamId,
                             const std::string& trackId)
{
  if (mState != kJsepStateStable) {
    JSEP_SET_ERROR("Removing tracks outside of stable is unsupported.");
    return NS_ERROR_UNEXPECTED;
  }

  auto track = FindTrackByIds(mLocalTracks, streamId, trackId);

  if (track == mLocalTracks.end()) {
    return NS_ERROR_INVALID_ARG;
  }

  mLocalTracks.erase(track);
  return NS_OK;
}

nsresult
JsepSessionImpl::SetIceCredentials(const std::string& ufrag,
                                   const std::string& pwd)
{
  mLastError.clear();
  mIceUfrag = ufrag;
  mIcePwd = pwd;

  return NS_OK;
}

nsresult
JsepSessionImpl::AddDtlsFingerprint(const std::string& algorithm,
                                    const std::vector<uint8_t>& value)
{
  mLastError.clear();
  JsepDtlsFingerprint fp;

  fp.mAlgorithm = algorithm;
  fp.mValue = value;

  mDtlsFingerprints.push_back(fp);

  return NS_OK;
}

nsresult
JsepSessionImpl::AddAudioRtpExtension(const std::string& extensionName)
{
  mLastError.clear();

  if (mAudioRtpExtensions.size() + 1 > UINT16_MAX) {
    JSEP_SET_ERROR("Too many audio rtp extensions have been added");
    return NS_ERROR_FAILURE;
  }

  SdpExtmapAttributeList::Extmap extmap =
      { static_cast<uint16_t>(mAudioRtpExtensions.size() + 1),
        SdpDirectionAttribute::kSendrecv,
        false, // don't actually specify direction
        extensionName,
        "" };

  mAudioRtpExtensions.push_back(extmap);
  return NS_OK;
}

nsresult
JsepSessionImpl::AddVideoRtpExtension(const std::string& extensionName)
{
  mLastError.clear();

  if (mVideoRtpExtensions.size() + 1 > UINT16_MAX) {
    JSEP_SET_ERROR("Too many video rtp extensions have been added");
    return NS_ERROR_FAILURE;
  }

  SdpExtmapAttributeList::Extmap extmap =
      { static_cast<uint16_t>(mVideoRtpExtensions.size() + 1),
        SdpDirectionAttribute::kSendrecv,
        false, // don't actually specify direction
        extensionName, "" };

  mVideoRtpExtensions.push_back(extmap);
  return NS_OK;
}

template<class T>
std::vector<RefPtr<JsepTrack>>
GetTracks(const std::vector<T>& wrappedTracks)
{
  std::vector<RefPtr<JsepTrack>> result;
  for (auto i = wrappedTracks.begin(); i != wrappedTracks.end(); ++i) {
    result.push_back(i->mTrack);
  }
  return result;
}

nsresult
JsepSessionImpl::ReplaceTrack(const std::string& oldStreamId,
                              const std::string& oldTrackId,
                              const std::string& newStreamId,
                              const std::string& newTrackId)
{
  auto it = FindTrackByIds(mLocalTracks, oldStreamId, oldTrackId);

  if (it == mLocalTracks.end()) {
    JSEP_SET_ERROR("Track " << oldStreamId << "/" << oldTrackId
                   << " was never added.");
    return NS_ERROR_INVALID_ARG;
  }

  if (FindTrackByIds(mLocalTracks, newStreamId, newTrackId) !=
      mLocalTracks.end()) {
    JSEP_SET_ERROR("Track " << newStreamId << "/" << newTrackId
                   << " was already added.");
    return NS_ERROR_INVALID_ARG;
  }

  it->mTrack->SetStreamId(newStreamId);
  it->mTrack->SetTrackId(newTrackId);

  return NS_OK;
}

std::vector<RefPtr<JsepTrack>>
JsepSessionImpl::GetLocalTracks() const
{
  return GetTracks(mLocalTracks);
}

std::vector<RefPtr<JsepTrack>>
JsepSessionImpl::GetRemoteTracks() const
{
  return GetTracks(mRemoteTracks);
}

std::vector<RefPtr<JsepTrack>>
JsepSessionImpl::GetRemoteTracksAdded() const
{
  return GetTracks(mRemoteTracksAdded);
}

std::vector<RefPtr<JsepTrack>>
JsepSessionImpl::GetRemoteTracksRemoved() const
{
  return GetTracks(mRemoteTracksRemoved);
}

static SdpMediaSection::Protocol
GetProtocolForMediaType(SdpMediaSection::MediaType type)
{
  if (type == SdpMediaSection::kApplication) {
    return SdpMediaSection::kDtlsSctp;
  }

  // TODO(bug 1094447): Use kUdpTlsRtpSavpf once it interops well
  return SdpMediaSection::kRtpSavpf;
}

nsresult
JsepSessionImpl::AddOfferMSections(const JsepOfferOptions& options, Sdp* sdp)
{
  // First audio, then video, then datachannel, for interop
  // TODO(bug 1121756): We need to group these by stream-id, _then_ by media
  // type, according to the spec. However, this is not going to interop with
  // older versions of Firefox if a video-only stream is added before an
  // audio-only stream.
  // We should probably wait until 38 is ESR before trying to do this.
  nsresult rv = AddOfferMSectionsByType(
      SdpMediaSection::kAudio, options.mOfferToReceiveAudio, sdp);

  NS_ENSURE_SUCCESS(rv, rv);

  rv = AddOfferMSectionsByType(
      SdpMediaSection::kVideo, options.mOfferToReceiveVideo, sdp);

  NS_ENSURE_SUCCESS(rv, rv);

  if (!(options.mDontOfferDataChannel.isSome() &&
        *options.mDontOfferDataChannel)) {
    rv = AddOfferMSectionsByType(
        SdpMediaSection::kApplication, Maybe<size_t>(), sdp);

    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (!sdp->GetMediaSectionCount()) {
    JSEP_SET_ERROR("Cannot create an offer with no local tracks, "
                   "no offerToReceiveAudio/Video, and no DataChannel.");
    return NS_ERROR_INVALID_ARG;
  }

  return NS_OK;
}

nsresult
JsepSessionImpl::AddOfferMSectionsByType(SdpMediaSection::MediaType mediatype,
                                         Maybe<size_t> offerToReceiveMaybe,
                                         Sdp* sdp)
{
  // Convert the Maybe into a size_t*, since that is more readable, especially
  // when using it as an in/out param.
  size_t offerToReceiveCount;
  size_t* offerToReceiveCountPtr = nullptr;

  if (offerToReceiveMaybe) {
    offerToReceiveCount = *offerToReceiveMaybe;
    offerToReceiveCountPtr = &offerToReceiveCount;
  }

  // Make sure every local track has an m-section
  nsresult rv = BindLocalTracks(mediatype, sdp);
  NS_ENSURE_SUCCESS(rv, rv);

  // Make sure that m-sections that previously had a remote track have the
  // recv bit set. Only matters for renegotiation.
  rv = EnsureRecvForRemoteTracks(mediatype, sdp, offerToReceiveCountPtr);
  NS_ENSURE_SUCCESS(rv, rv);

  // If we need more recv sections, start setting the recv bit on other
  // msections. If not, disable msections that have no tracks.
  rv = SetRecvAsNeededOrDisable(mediatype,
                                sdp,
                                offerToReceiveCountPtr);
  NS_ENSURE_SUCCESS(rv, rv);

  // If we still don't have enough recv m-sections, add some.
  if (offerToReceiveCountPtr && *offerToReceiveCountPtr) {
    rv = AddRecvonlyMsections(mediatype, *offerToReceiveCountPtr, sdp);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
JsepSessionImpl::BindLocalTracks(SdpMediaSection::MediaType mediatype,
                                 Sdp* sdp)
{
  for (auto track = mLocalTracks.begin(); track != mLocalTracks.end();
       ++track) {
    if (mediatype != track->mTrack->GetMediaType()) {
      continue;
    }

    SdpMediaSection* msection;

    if (track->mAssignedMLine.isSome()) {
      // Renegotiation
      msection = &sdp->GetMediaSection(*track->mAssignedMLine);
    } else {
      nsresult rv = GetFreeMsectionForSend(track->mTrack->GetMediaType(),
                                           sdp,
                                           &msection);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    nsresult rv = BindTrackToMsection(&(*track), msection);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

nsresult
JsepSessionImpl::BindTrackToMsection(
    JsepSendingTrack* track,
    SdpMediaSection* msection)
{
  if (msection->GetMediaType() != SdpMediaSection::kApplication) {
    AddLocalSsrcs(*track->mTrack, msection);
    AddLocalIds(*track->mTrack, msection);
  }
  msection->SetSending(true);
  track->mAssignedMLine = Some(msection->GetLevel());
  track->mSetInLocalDescription = false;
  return NS_OK;
}

nsresult
JsepSessionImpl::EnsureRecvForRemoteTracks(SdpMediaSection::MediaType mediatype,
                                           Sdp* sdp,
                                           size_t* offerToReceive)
{
  for (auto track = mRemoteTracks.begin(); track != mRemoteTracks.end();
       ++track) {
    if (mediatype != track->mTrack->GetMediaType()) {
      continue;
    }

    if (!track->mAssignedMLine.isSome()) {
      MOZ_ASSERT(false);
      continue;
    }

    auto& msection = sdp->GetMediaSection(*track->mAssignedMLine);

    if (MsectionIsDisabled(msection)) {
      // TODO(bug 1095226) Content probably disabled this? Should we allow
      // content to do this?
      continue;
    }

    msection.SetReceiving(true);
    if (offerToReceive && *offerToReceive) {
      --(*offerToReceive);
    }
  }

  return NS_OK;
}

nsresult
JsepSessionImpl::SetRecvAsNeededOrDisable(SdpMediaSection::MediaType mediatype,
                                          Sdp* sdp,
                                          size_t* offerToRecv)
{
  for (size_t i = 0; i < sdp->GetMediaSectionCount(); ++i) {
    auto& msection = sdp->GetMediaSection(i);

    if (MsectionIsDisabled(msection) ||
        msection.GetMediaType() != mediatype ||
        msection.IsReceiving()) {
      continue;
    }

    if (offerToRecv) {
      if (*offerToRecv) {
        msection.SetReceiving(true);
        --(*offerToRecv);
        continue;
      }
    } else if (msection.IsSending()) {
      msection.SetReceiving(true);
      continue;
    }

    if (!msection.IsSending()) {
      // Unused m-section, and no reason to offer to recv on it
      DisableMsection(sdp, &msection);
    }
  }

  return NS_OK;
}

nsresult
JsepSessionImpl::AddRecvonlyMsections(SdpMediaSection::MediaType mediatype,
                                      size_t count,
                                      Sdp* sdp)
{
  while (count--) {
    nsresult rv = CreateOfferMSection(
        mediatype, SdpDirectionAttribute::kRecvonly, sdp);

    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

// This function creates a skeleton SDP based on the old descriptions
// (ie; all m-sections are inactive).
nsresult
JsepSessionImpl::CreateReoffer(const Sdp& oldLocalSdp,
                               const Sdp& oldAnswer,
                               Sdp* newSdp)
{
  nsresult rv;

  for (size_t i = 0; i < oldLocalSdp.GetMediaSectionCount(); ++i) {
    // We do not set the direction in this function (or disable when previously
    // disabled), that happens in |AddOfferMSectionsByType|
    rv = CreateOfferMSection(oldLocalSdp.GetMediaSection(i).GetMediaType(),
                             SdpDirectionAttribute::kInactive,
                             newSdp);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = CopyStickyParams(oldAnswer.GetMediaSection(i),
                          &newSdp->GetMediaSection(i));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

void
JsepSessionImpl::SetupBundle(Sdp* sdp) const
{
  std::vector<std::string> mids;

  // This has the effect of changing the bundle level if the first m-section
  // goes from disabled to enabled. This is kinda inefficient.

  for (size_t i = 0; i < sdp->GetMediaSectionCount(); ++i) {
    auto& attrs = sdp->GetMediaSection(i).GetAttributeList();
    if (attrs.HasAttribute(SdpAttribute::kMidAttribute)) {
      mids.push_back(attrs.GetMid());
    }
  }

  if (!mids.empty()) {
    UniquePtr<SdpGroupAttributeList> groupAttr(new SdpGroupAttributeList);
    groupAttr->PushEntry(SdpGroupAttributeList::kBundle, mids);
    sdp->GetAttributeList().SetAttribute(groupAttr.release());
  }
}

nsresult
JsepSessionImpl::SetupTransportAttributes(Sdp* offer)
{
  const Sdp* oldAnswer = GetAnswer();

  if (oldAnswer) {
    // Renegotiation, we might have transport attributes to copy over
    for (size_t i = 0; i < oldAnswer->GetMediaSectionCount(); ++i) {
      if (!MsectionIsDisabled(offer->GetMediaSection(i)) &&
          !MsectionIsDisabled(oldAnswer->GetMediaSection(i)) &&
          !IsBundleSlave(*oldAnswer, i)) {
        nsresult rv = CopyTransportParams(
            mCurrentLocalDescription->GetMediaSection(i),
            &offer->GetMediaSection(i));
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
  }

  return NS_OK;
}

void
JsepSessionImpl::SetupMsidSemantic(const std::vector<std::string>& msids,
                                   Sdp* sdp) const
{
  if (!msids.empty()) {
    UniquePtr<SdpMsidSemanticAttributeList> msidSemantics(
        new SdpMsidSemanticAttributeList);
    msidSemantics->PushEntry("WMS", msids);
    sdp->GetAttributeList().SetAttribute(msidSemantics.release());
  }
}

nsresult
JsepSessionImpl::GetIdsFromMsid(const Sdp& sdp,
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
      if (!found) {
        *streamId = i->identifier;
        *trackId = i->appdata;
        found = true;
      } else if ((*streamId != i->identifier) || (*trackId != i->appdata)) {
        JSEP_SET_ERROR("Found multiple different webrtc msids in m-section "
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
JsepSessionImpl::GetRemoteIds(const Sdp& sdp,
                              const SdpMediaSection& msection,
                              std::string* streamId,
                              std::string* trackId)
{
  nsresult rv = GetIdsFromMsid(sdp, msection, streamId, trackId);
  if (rv == NS_ERROR_NOT_AVAILABLE) {
      *streamId = mDefaultRemoteStreamId;

    if (!mDefaultRemoteTrackIdsByLevel.count(msection.GetLevel())) {
      // Generate random track ids.
      if (!mUuidGen->Generate(trackId)) {
        JSEP_SET_ERROR("Failed to generate UUID for JsepTrack");
        return NS_ERROR_FAILURE;
      }

      mDefaultRemoteTrackIdsByLevel[msection.GetLevel()] = *trackId;
    } else {
      *trackId = mDefaultRemoteTrackIdsByLevel[msection.GetLevel()];
    }
    return NS_OK;
  }

  if (NS_SUCCEEDED(rv)) {
    // If, for whatever reason, the other end renegotiates with an msid where
    // there wasn't one before, don't allow the old default to pop up again
    // later.
    mDefaultRemoteTrackIdsByLevel.erase(msection.GetLevel());
  }

  return rv;
}

nsresult
JsepSessionImpl::GetMsids(
    const SdpMediaSection& msection,
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
JsepSessionImpl::ParseMsid(const std::string& msidAttribute,
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
    JSEP_SET_ERROR("Malformed source-level msid attribute: "
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

nsresult
JsepSessionImpl::CreateOffer(const JsepOfferOptions& options,
                             std::string* offer)
{
  mLastError.clear();

  if (mState != kJsepStateStable) {
    JSEP_SET_ERROR("Cannot create offer in state " << GetStateStr(mState));
    return NS_ERROR_UNEXPECTED;
  }

  UniquePtr<Sdp> sdp;

  // Make the basic SDP that is common to offer/answer.
  nsresult rv = CreateGenericSDP(&sdp);
  NS_ENSURE_SUCCESS(rv, rv);
  ++mSessionVersion;

  if (mCurrentLocalDescription) {
    rv = CreateReoffer(*mCurrentLocalDescription, *GetAnswer(), sdp.get());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Get rid of all m-line assignments that have not been executed by a call
  // to SetLocalDescription.
  if (NS_SUCCEEDED(rv)) {
    for (auto i = mLocalTracks.begin(); i != mLocalTracks.end(); ++i) {
      if (!i->mSetInLocalDescription) {
        i->mAssignedMLine.reset();
      }
    }
  }

  // Now add all the m-lines that we are attempting to negotiate.
  rv = AddOfferMSections(options, sdp.get());
  NS_ENSURE_SUCCESS(rv, rv);

  SetupBundle(sdp.get());

  rv = SetupTransportAttributes(sdp.get());
  NS_ENSURE_SUCCESS(rv,rv);

  *offer = sdp->ToString();
  mGeneratedLocalDescription = Move(sdp);

  return NS_OK;
}

std::string
JsepSessionImpl::GetLocalDescription() const
{
  std::ostringstream os;
  if (mPendingLocalDescription) {
    mPendingLocalDescription->Serialize(os);
  } else if (mCurrentLocalDescription) {
    mCurrentLocalDescription->Serialize(os);
  }
  return os.str();
}

std::string
JsepSessionImpl::GetRemoteDescription() const
{
  std::ostringstream os;
  if (mPendingRemoteDescription) {
    mPendingRemoteDescription->Serialize(os);
  } else if (mCurrentRemoteDescription) {
    mCurrentRemoteDescription->Serialize(os);
  }
  return os.str();
}

void
JsepSessionImpl::AddCodecs(SdpMediaSection* msection) const
{
  msection->ClearCodecs();
  for (const JsepCodecDescription* codec : mCodecs.values) {
    codec->AddToMediaSection(*msection);
  }
}

void
JsepSessionImpl::AddExtmap(SdpMediaSection* msection) const
{
  const auto* extensions = GetRtpExtensions(msection->GetMediaType());

  if (extensions && !extensions->empty()) {
    SdpExtmapAttributeList* extmap = new SdpExtmapAttributeList;
    extmap->mExtmaps = *extensions;
    msection->GetAttributeList().SetAttribute(extmap);
  }
}

void
JsepSessionImpl::AddMid(const std::string& mid,
                        SdpMediaSection* msection) const
{
  msection->GetAttributeList().SetAttribute(new SdpStringAttribute(
        SdpAttribute::kMidAttribute, mid));
}

void
JsepSessionImpl::AddLocalSsrcs(const JsepTrack& track,
                               SdpMediaSection* msection) const
{
  UniquePtr<SdpSsrcAttributeList> ssrcs(new SdpSsrcAttributeList);
  for (auto i = track.GetSsrcs().begin(); i != track.GetSsrcs().end(); ++i) {
    // When using ssrc attributes, we are required to at least have a cname.
    // (See https://tools.ietf.org/html/rfc5576#section-6.1)
    std::string cnameAttr("cname:");
    cnameAttr += track.GetCNAME();
    ssrcs->PushEntry(*i, cnameAttr);
  }

  if (!ssrcs->mSsrcs.empty()) {
    msection->GetAttributeList().SetAttribute(ssrcs.release());
  }
}

void
JsepSessionImpl::AddLocalIds(const JsepTrack& track,
                             SdpMediaSection* msection) const
{
  if (track.GetMediaType() == SdpMediaSection::kApplication) {
    return;
  }

  UniquePtr<SdpMsidAttributeList> msids(new SdpMsidAttributeList);
  if (msection->GetAttributeList().HasAttribute(SdpAttribute::kMsidAttribute)) {
    msids->mMsids = msection->GetAttributeList().GetMsid().mMsids;
  }

  msids->PushEntry(track.GetStreamId(), track.GetTrackId());

  msection->GetAttributeList().SetAttribute(msids.release());
}

JsepCodecDescription*
JsepSessionImpl::FindMatchingCodec(const std::string& fmt,
                                   const SdpMediaSection& msection) const
{
  for (JsepCodecDescription* codec : mCodecs.values) {
    if (codec->mEnabled && codec->Matches(fmt, msection)) {
      return codec;
    }
  }

  return nullptr;
}

const std::vector<SdpExtmapAttributeList::Extmap>*
JsepSessionImpl::GetRtpExtensions(SdpMediaSection::MediaType type) const
{
  switch (type) {
    case SdpMediaSection::kAudio:
      return &mAudioRtpExtensions;
    case SdpMediaSection::kVideo:
      return &mVideoRtpExtensions;
    default:
      return nullptr;
  }
}

static bool
CompareCodec(const JsepCodecDescription* lhs, const JsepCodecDescription* rhs)
{
  return lhs->mStronglyPreferred && !rhs->mStronglyPreferred;
}

PtrVector<JsepCodecDescription>
JsepSessionImpl::GetCommonCodecs(const SdpMediaSection& remoteMsection)
{
  PtrVector<JsepCodecDescription> matchingCodecs;
  for (const std::string& fmt : remoteMsection.GetFormats()) {
    JsepCodecDescription* codec = FindMatchingCodec(fmt, remoteMsection);
    if (codec) {
      codec->mDefaultPt = fmt; // Remember the other side's PT
      matchingCodecs.values.push_back(codec->Clone());
    }
  }

  std::stable_sort(matchingCodecs.values.begin(),
                   matchingCodecs.values.end(),
                   CompareCodec);

  return matchingCodecs;
}

void
JsepSessionImpl::AddCommonExtmaps(const SdpMediaSection& remoteMsection,
                                  SdpMediaSection* msection)
{
  if (!remoteMsection.GetAttributeList().HasAttribute(
        SdpAttribute::kExtmapAttribute)) {
    return;
  }

  auto* ourExtensions = GetRtpExtensions(remoteMsection.GetMediaType());

  if (!ourExtensions) {
    return;
  }

  UniquePtr<SdpExtmapAttributeList> ourExtmap(new SdpExtmapAttributeList);
  auto& theirExtmap = remoteMsection.GetAttributeList().GetExtmap().mExtmaps;
  for (auto i = theirExtmap.begin(); i != theirExtmap.end(); ++i) {
    for (auto j = ourExtensions->begin(); j != ourExtensions->end(); ++j) {
      if (i->extensionname == j->extensionname) {
        ourExtmap->mExtmaps.push_back(*i);

        // RFC 5285 says that ids >= 4096 can be used by the offerer to
        // force the answerer to pick, otherwise the value in the offer is
        // used.
        if (ourExtmap->mExtmaps.back().entry >= 4096) {
          ourExtmap->mExtmaps.back().entry = j->entry;
        }
      }
    }
  }

  if (!ourExtmap->mExtmaps.empty()) {
    msection->GetAttributeList().SetAttribute(ourExtmap.release());
  }
}

nsresult
JsepSessionImpl::CreateAnswer(const JsepAnswerOptions& options,
                              std::string* answer)
{
  mLastError.clear();

  if (mState != kJsepStateHaveRemoteOffer) {
    JSEP_SET_ERROR("Cannot create answer in state " << GetStateStr(mState));
    return NS_ERROR_UNEXPECTED;
  }

  // This is the heart of the negotiation code. Depressing that it's
  // so bad.
  //
  // Here's the current algorithm:
  // 1. Walk through all the m-lines on the other side.
  // 2. For each m-line, walk through all of our local tracks
  //    in sequence and see if any are unassigned. If so, assign
  //    them and mark it sendrecv, otherwise it's recvonly.
  // 3. Just replicate their media attributes.
  // 4. Profit.
  UniquePtr<Sdp> sdp;

  // Make the basic SDP that is common to offer/answer.
  nsresult rv = CreateGenericSDP(&sdp);
  NS_ENSURE_SUCCESS(rv, rv);

  const Sdp& offer = *mPendingRemoteDescription;

  auto* group = FindBundleGroup(offer);
  if (group) {
    // Copy the bundle group into our answer
    UniquePtr<SdpGroupAttributeList> groupAttr(new SdpGroupAttributeList);
    groupAttr->mGroups.push_back(*group);
    sdp->GetAttributeList().SetAttribute(groupAttr.release());
  }

  // Disable send for local tracks if the offer no longer allows it
  // (i.e., the m-section is recvonly, inactive or disabled)
  for (auto i = mLocalTracks.begin(); i != mLocalTracks.end(); ++i) {
    if (!i->mAssignedMLine.isSome()) {
      continue;
    }

    // Get rid of all m-line assignments that have not been executed by a call
    // to SetLocalDescription.
    if (!i->mSetInLocalDescription) {
      i->mAssignedMLine.reset();
      continue;
    }

    if (!offer.GetMediaSection(*i->mAssignedMLine).IsReceiving()) {
      i->mAssignedMLine.reset();
    }
  }

  size_t numMsections = offer.GetMediaSectionCount();

  for (size_t i = 0; i < numMsections; ++i) {
    const SdpMediaSection& remoteMsection = offer.GetMediaSection(i);
    SdpMediaSection& msection =
        sdp->AddMediaSection(remoteMsection.GetMediaType(),
                             SdpDirectionAttribute::kInactive,
                             9,
                             remoteMsection.GetProtocol(),
                             sdp::kIPv4,
                             "0.0.0.0");

    rv = CreateAnswerMSection(options, i, remoteMsection, &msection, sdp.get());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  *answer = sdp->ToString();
  mGeneratedLocalDescription = Move(sdp);

  return NS_OK;
}

static bool
HasRtcp(SdpMediaSection::Protocol proto)
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

nsresult
JsepSessionImpl::CreateOfferMSection(SdpMediaSection::MediaType mediatype,
                                     SdpDirectionAttribute::Direction dir,
                                     Sdp* sdp)
{
  SdpMediaSection::Protocol proto = GetProtocolForMediaType(mediatype);

  SdpMediaSection* msection =
      &sdp->AddMediaSection(mediatype, dir, 0, proto, sdp::kIPv4, "0.0.0.0");

  return EnableMsection(msection);
}

nsresult
JsepSessionImpl::GetFreeMsectionForSend(
    SdpMediaSection::MediaType type,
    Sdp* sdp,
    SdpMediaSection** msectionOut)
{
  for (size_t i = 0; i < sdp->GetMediaSectionCount(); ++i) {
    SdpMediaSection& msection = sdp->GetMediaSection(i);
    // draft-ietf-rtcweb-jsep-08 says we should reclaim disabled m-sections
    // regardless of media type. This breaks some pretty fundamental rules of
    // SDP offer/answer, so we probably should not do it.
    if (msection.GetMediaType() != type) {
      continue;
    }

    if (FindTrackByLevel(mLocalTracks, i) != mLocalTracks.end()) {
      // Not free
      continue;
    }

    if (MsectionIsDisabled(msection)) {
      // Was disabled; revive
      nsresult rv = EnableMsection(&msection);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    *msectionOut = &msection;
    return NS_OK;
  }

  // Ok, no pre-existing m-section. Make a new one.
  nsresult rv = CreateOfferMSection(type,
                                    SdpDirectionAttribute::kInactive,
                                    sdp);
  NS_ENSURE_SUCCESS(rv, rv);

  *msectionOut = &sdp->GetMediaSection(sdp->GetMediaSectionCount() - 1);
  return NS_OK;
}

nsresult
JsepSessionImpl::CreateAnswerMSection(const JsepAnswerOptions& options,
                                      size_t mlineIndex,
                                      const SdpMediaSection& remoteMsection,
                                      SdpMediaSection* msection,
                                      Sdp* sdp)
{
  nsresult rv = CopyStickyParams(remoteMsection, msection);
  NS_ENSURE_SUCCESS(rv, rv);

  if (MsectionIsDisabled(remoteMsection)) {
    DisableMsection(sdp, msection);
    return NS_OK;
  }

  SdpSetupAttribute::Role role;
  rv = DetermineAnswererSetupRole(remoteMsection, &role);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AddTransportAttributes(msection, role);
  NS_ENSURE_SUCCESS(rv, rv);

  // Only attempt to match up local tracks if the offerer has elected to
  // receive traffic.
  if (remoteMsection.IsReceiving()) {
    rv = BindMatchingLocalTrackForAnswer(msection);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (remoteMsection.IsSending()) {
    msection->SetReceiving(true);
  }

  // Now add the codecs.
  PtrVector<JsepCodecDescription> matchingCodecs(
      GetCommonCodecs(remoteMsection));

  for (const JsepCodecDescription* codec : matchingCodecs.values) {
    UniquePtr<JsepCodecDescription> negotiated(
        codec->MakeNegotiatedCodec(remoteMsection));
    if (negotiated) {
      negotiated->AddToMediaSection(*msection);
      // TODO(bug 1099351): Once bug 1073475 is fixed on all supported
      // versions, we can remove this limitation.
      break;
    }
  }

  // Add extmap attributes.
  AddCommonExtmaps(remoteMsection, msection);

  if (!msection->IsReceiving() && !msection->IsSending()) {
    DisableMsection(sdp, msection);
    return NS_OK;
  }

  if (msection->GetFormats().empty()) {
    // Could not negotiate anything. Disable m-section.
    DisableMsection(sdp, msection);
  }

  return NS_OK;
}

nsresult
JsepSessionImpl::BindMatchingLocalTrackForAnswer(SdpMediaSection* msection)
{
  auto track = FindTrackByLevel(mLocalTracks, msection->GetLevel());

  if (track == mLocalTracks.end()) {
    track = FindUnassignedTrackByType(mLocalTracks, msection->GetMediaType());
  }

  if (track == mLocalTracks.end() &&
      msection->GetMediaType() == SdpMediaSection::kApplication) {
    // If we are offered datachannel, we need to play along even if no track
    // for it has been added yet.
    std::string streamId;
    std::string trackId;

    if (!mUuidGen->Generate(&streamId) || !mUuidGen->Generate(&trackId)) {
      JSEP_SET_ERROR("Failed to generate UUIDs for datachannel track");
      return NS_ERROR_FAILURE;
    }

    AddTrack(RefPtr<JsepTrack>(
          new JsepTrack(SdpMediaSection::kApplication, streamId, trackId)));
    track = FindUnassignedTrackByType(mLocalTracks, msection->GetMediaType());
    MOZ_ASSERT(track != mLocalTracks.end());
  }

  if (track != mLocalTracks.end()) {
    nsresult rv = BindTrackToMsection(&(*track), msection);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
JsepSessionImpl::DetermineAnswererSetupRole(
    const SdpMediaSection& remoteMsection,
    SdpSetupAttribute::Role* rolep)
{
  // Determine the role.
  // RFC 5763 says:
  //
  //   The endpoint MUST use the setup attribute defined in [RFC4145].
  //   The endpoint that is the offerer MUST use the setup attribute
  //   value of setup:actpass and be prepared to receive a client_hello
  //   before it receives the answer.  The answerer MUST use either a
  //   setup attribute value of setup:active or setup:passive.  Note that
  //   if the answerer uses setup:passive, then the DTLS handshake will
  //   not begin until the answerer is received, which adds additional
  //   latency. setup:active allows the answer and the DTLS handshake to
  //   occur in parallel.  Thus, setup:active is RECOMMENDED.  Whichever
  //   party is active MUST initiate a DTLS handshake by sending a
  //   ClientHello over each flow (host/port quartet).
  //
  //   We default to assuming that the offerer is passive and we are active.
  SdpSetupAttribute::Role role = SdpSetupAttribute::kActive;

  if (remoteMsection.GetAttributeList().HasAttribute(
          SdpAttribute::kSetupAttribute)) {
    switch (remoteMsection.GetAttributeList().GetSetup().mRole) {
      case SdpSetupAttribute::kActive:
        role = SdpSetupAttribute::kPassive;
        break;
      case SdpSetupAttribute::kPassive:
      case SdpSetupAttribute::kActpass:
        role = SdpSetupAttribute::kActive;
        break;
      case SdpSetupAttribute::kHoldconn:
        // This should have been caught by ParseSdp
        MOZ_ASSERT(false);
        JSEP_SET_ERROR("The other side used an illegal setup attribute"
                       " (\"holdconn\").");
        return NS_ERROR_INVALID_ARG;
    }
  }

  *rolep = role;
  return NS_OK;
}

static void
appendSdpParseErrors(
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

nsresult
JsepSessionImpl::SetLocalDescription(JsepSdpType type, const std::string& sdp)
{
  mLastError.clear();

  switch (mState) {
    case kJsepStateStable:
      if (type != kJsepSdpOffer) {
        JSEP_SET_ERROR("Cannot set local answer in state "
                       << GetStateStr(mState));
        return NS_ERROR_UNEXPECTED;
      }
      mIsOfferer = true;
      break;
    case kJsepStateHaveRemoteOffer:
      if (type != kJsepSdpAnswer && type != kJsepSdpPranswer) {
        JSEP_SET_ERROR("Cannot set local offer in state "
                       << GetStateStr(mState));
        return NS_ERROR_UNEXPECTED;
      }
      break;
    default:
      JSEP_SET_ERROR("Cannot set local offer or answer in state "
                     << GetStateStr(mState));
      return NS_ERROR_UNEXPECTED;
  }

  UniquePtr<Sdp> parsed;
  nsresult rv = ParseSdp(sdp, &parsed);
  NS_ENSURE_SUCCESS(rv, rv);

  // Check that content hasn't done anything unsupported with the SDP
  rv = ValidateLocalDescription(*parsed);
  NS_ENSURE_SUCCESS(rv, rv);

  // Create transport objects.
  size_t numMsections = parsed->GetMediaSectionCount();
  for (size_t t = 0; t < numMsections; ++t) {
    if (t < mTransports.size()) {
      mTransports[t]->mState = JsepTransport::kJsepTransportOffered;
      continue; // This transport already exists (assume we are renegotiating).
    }

    RefPtr<JsepTransport> transport;
    nsresult rv = CreateTransport(parsed->GetMediaSection(t), &transport);
    NS_ENSURE_SUCCESS(rv, rv);

    mTransports.push_back(transport);
  }

  switch (type) {
    case kJsepSdpOffer:
      rv = SetLocalDescriptionOffer(Move(parsed));
      break;
    case kJsepSdpAnswer:
    case kJsepSdpPranswer:
      rv = SetLocalDescriptionAnswer(type, Move(parsed));
      break;
  }

  // Mark all assigned local tracks as added to the local description
  if (NS_SUCCEEDED(rv)) {
    for (auto i = mLocalTracks.begin(); i != mLocalTracks.end(); ++i) {
      if (i->mAssignedMLine.isSome()) {
        i->mSetInLocalDescription = true;
      }
    }
  }

  return rv;
}

nsresult
JsepSessionImpl::SetLocalDescriptionOffer(UniquePtr<Sdp> offer)
{
  MOZ_ASSERT(mState == kJsepStateStable);
  mPendingLocalDescription = Move(offer);
  SetState(kJsepStateHaveLocalOffer);
  return NS_OK;
}

nsresult
JsepSessionImpl::SetLocalDescriptionAnswer(JsepSdpType type,
                                           UniquePtr<Sdp> answer)
{
  MOZ_ASSERT(mState == kJsepStateHaveRemoteOffer);
  mPendingLocalDescription = Move(answer);

  nsresult rv = ValidateAnswer(*mPendingRemoteDescription,
                               *mPendingLocalDescription);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = HandleNegotiatedSession(mPendingLocalDescription,
                               mPendingRemoteDescription);
  NS_ENSURE_SUCCESS(rv, rv);

  mCurrentRemoteDescription = Move(mPendingRemoteDescription);
  mCurrentLocalDescription = Move(mPendingLocalDescription);

  SetState(kJsepStateStable);
  return NS_OK;
}

nsresult
JsepSessionImpl::SetRemoteDescription(JsepSdpType type, const std::string& sdp)
{
  mLastError.clear();
  mRemoteTracksAdded.clear();
  mRemoteTracksRemoved.clear();

  MOZ_MTLOG(ML_DEBUG, "SetRemoteDescription type=" << type << "\nSDP=\n"
                                                   << sdp);
  switch (mState) {
    case kJsepStateStable:
      if (type != kJsepSdpOffer) {
        JSEP_SET_ERROR("Cannot set remote answer in state "
                       << GetStateStr(mState));
        return NS_ERROR_UNEXPECTED;
      }
      mIsOfferer = false;
      break;
    case kJsepStateHaveLocalOffer:
    case kJsepStateHaveRemotePranswer:
      if (type != kJsepSdpAnswer && type != kJsepSdpPranswer) {
        JSEP_SET_ERROR("Cannot set remote offer in state "
                       << GetStateStr(mState));
        return NS_ERROR_UNEXPECTED;
      }
      break;
    default:
      JSEP_SET_ERROR("Cannot set remote offer or answer in current state "
                     << GetStateStr(mState));
      return NS_ERROR_UNEXPECTED;
  }

  // Parse.
  UniquePtr<Sdp> parsed;
  nsresult rv = ParseSdp(sdp, &parsed);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = ValidateRemoteDescription(*parsed);
  NS_ENSURE_SUCCESS(rv, rv);

  bool iceLite =
      parsed->GetAttributeList().HasAttribute(SdpAttribute::kIceLiteAttribute);

  std::vector<std::string> iceOptions;
  if (parsed->GetAttributeList().HasAttribute(
          SdpAttribute::kIceOptionsAttribute)) {
    iceOptions = parsed->GetAttributeList().GetIceOptions().mValues;
  }

  switch (type) {
    case kJsepSdpOffer:
      rv = SetRemoteDescriptionOffer(Move(parsed));
      break;
    case kJsepSdpAnswer:
    case kJsepSdpPranswer:
      rv = SetRemoteDescriptionAnswer(type, Move(parsed));
      break;
  }

  if (NS_SUCCEEDED(rv)) {
    mRemoteIsIceLite = iceLite;
    mIceOptions = iceOptions;
  }

  return rv;
}

nsresult
JsepSessionImpl::HandleNegotiatedSession(const UniquePtr<Sdp>& local,
                                         const UniquePtr<Sdp>& remote)
{
  bool remoteIceLite =
      remote->GetAttributeList().HasAttribute(SdpAttribute::kIceLiteAttribute);

  mIceControlling = remoteIceLite || mIsOfferer;

  const Sdp& answer = mIsOfferer ? *remote : *local;

  std::set<std::string> bundleMids;
  const SdpMediaSection* bundleMsection = nullptr;
  // TODO(bug 1112692): Support more than one bundle group
  nsresult rv = GetBundleInfo(answer, &bundleMids, &bundleMsection);
  NS_ENSURE_SUCCESS(rv, rv);

  std::vector<JsepTrackPair> trackPairs;

  // Now walk through the m-sections, make sure they match, and create
  // track pairs that describe the media to be set up.
  for (size_t i = 0; i < local->GetMediaSectionCount(); ++i) {
    // Skip disabled m-sections.
    if (answer.GetMediaSection(i).GetPort() == 0) {
      continue;
    }

    // The transport details are not necessarily on the m-section we're
    // currently processing.
    size_t transportLevel = i;
    bool usingBundle = false;
    {
      const SdpMediaSection& answerMsection(answer.GetMediaSection(i));
      if (answerMsection.GetAttributeList().HasAttribute(
            SdpAttribute::kMidAttribute)) {
        if (bundleMids.count(answerMsection.GetAttributeList().GetMid())) {
          transportLevel = bundleMsection->GetLevel();
          usingBundle = true;
        }
      }
    }

    // Transports are created in SetLocal.
    if (mTransports.size() < transportLevel) {
      JSEP_SET_ERROR("Fewer transports set up than m-lines");
      MOZ_ASSERT(false);
      return NS_ERROR_FAILURE;
    }

    RefPtr<JsepTransport> transport = mTransports[transportLevel];

    rv = SetupTransport(
        remote->GetMediaSection(transportLevel).GetAttributeList(),
        answer.GetMediaSection(transportLevel).GetAttributeList(),
        transport);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!answer.GetMediaSection(i).IsSending() &&
        !answer.GetMediaSection(i).IsReceiving()) {
      MOZ_MTLOG(ML_DEBUG, "Inactive m-section, skipping creation of negotiated "
                          "track pair.");
      continue;
    }

    JsepTrackPair trackPair;
    rv = MakeNegotiatedTrackPair(remote->GetMediaSection(i),
                                 local->GetMediaSection(i),
                                 transport,
                                 usingBundle,
                                 transportLevel,
                                 &trackPair);
    NS_ENSURE_SUCCESS(rv, rv);

    trackPairs.push_back(trackPair);
  }

  rv = SetUniquePayloadTypes();
  NS_ENSURE_SUCCESS(rv, rv);

  // Ouch, this probably needs some dirty bit instead of just clearing
  // stuff for renegotiation.
  mNegotiatedTrackPairs = trackPairs;

  // Mark transports that weren't accepted as closed
  for (auto i = mTransports.begin(); i != mTransports.end(); ++i) {
    if ((*i)->mState != JsepTransport::kJsepTransportAccepted) {
      (*i)->mState = JsepTransport::kJsepTransportClosed;
    }
  }
  return NS_OK;
}

nsresult
JsepSessionImpl::MakeNegotiatedTrackPair(const SdpMediaSection& remote,
                                         const SdpMediaSection& local,
                                         const RefPtr<JsepTransport>& transport,
                                         bool usingBundle,
                                         size_t transportLevel,
                                         JsepTrackPair* trackPairOut)
{
  const SdpMediaSection& answer = mIsOfferer ? remote : local;

  bool sending;
  bool receiving;

  if (mIsOfferer) {
    receiving = answer.IsSending();
    sending = answer.IsReceiving();
  } else {
    sending = answer.IsSending();
    receiving = answer.IsReceiving();
  }

  MOZ_MTLOG(ML_DEBUG, "Negotiated m= line"
                          << " index=" << local.GetLevel()
                          << " type=" << local.GetMediaType()
                          << " sending=" << sending
                          << " receiving=" << receiving);

  trackPairOut->mLevel = local.GetLevel();

  if (usingBundle) {
    trackPairOut->mBundleLevel = Some(transportLevel);
  }

  if (sending) {
    auto sendTrack = FindTrackByLevel(mLocalTracks, local.GetLevel());
    if (sendTrack == mLocalTracks.end()) {
      JSEP_SET_ERROR("Failed to find local track for level " <<
                     local.GetLevel()
                     << " in local SDP. This should never happen.");
      NS_ASSERTION(false, "Failed to find local track for level");
      return NS_ERROR_FAILURE;
    }

    nsresult rv = NegotiateTrack(remote,
                                 local,
                                 JsepTrack::kJsepTrackSending,
                                 &sendTrack->mTrack);
    NS_ENSURE_SUCCESS(rv, rv);

    trackPairOut->mSending = sendTrack->mTrack;
  }

  if (receiving) {
    auto recvTrack = FindTrackByLevel(mRemoteTracks, local.GetLevel());
    if (recvTrack == mRemoteTracks.end()) {
      JSEP_SET_ERROR("Failed to find remote track for level "
                     << local.GetLevel()
                     << " in remote SDP. This should never happen.");
      NS_ASSERTION(false, "Failed to find remote track for level");
      return NS_ERROR_FAILURE;
    }

    nsresult rv = NegotiateTrack(remote,
                                 local,
                                 JsepTrack::kJsepTrackReceiving,
                                 &recvTrack->mTrack);
    NS_ENSURE_SUCCESS(rv, rv);

    if (remote.GetAttributeList().HasAttribute(SdpAttribute::kSsrcAttribute)) {
      auto& ssrcs = remote.GetAttributeList().GetSsrc().mSsrcs;
      for (auto i = ssrcs.begin(); i != ssrcs.end(); ++i) {
        recvTrack->mTrack->AddSsrc(i->ssrc);
      }
    }

    if (trackPairOut->mBundleLevel.isSome() &&
        recvTrack->mTrack->GetSsrcs().empty() &&
        recvTrack->mTrack->GetMediaType() != SdpMediaSection::kApplication) {
      MOZ_MTLOG(ML_ERROR, "Bundled m-section has no ssrc attributes. "
                          "This may cause media packets to be dropped.");
    }

    trackPairOut->mReceiving = recvTrack->mTrack;
  }

  trackPairOut->mRtpTransport = transport;

  const SdpAttributeList& remoteAttrs(remote.GetAttributeList());
  const SdpAttributeList& localAttrs(local.GetAttributeList());

  if (HasRtcp(local.GetProtocol())) {
    // RTCP MUX or not.
    // TODO(bug 1095743): verify that the PTs are consistent with mux.
    if (remoteAttrs.HasAttribute(SdpAttribute::kRtcpMuxAttribute) &&
        localAttrs.HasAttribute(SdpAttribute::kRtcpMuxAttribute)) {
      trackPairOut->mRtcpTransport = nullptr; // We agree on mux.
      MOZ_MTLOG(ML_DEBUG, "RTCP-MUX is on");
    } else {
      MOZ_MTLOG(ML_DEBUG, "RTCP-MUX is off");
      trackPairOut->mRtcpTransport = transport;
    }
  }

  return NS_OK;
}

nsresult
JsepSessionImpl::NegotiateTrack(const SdpMediaSection& remoteMsection,
                                const SdpMediaSection& localMsection,
                                JsepTrack::Direction direction,
                                RefPtr<JsepTrack>* track)
{
  UniquePtr<JsepTrackNegotiatedDetailsImpl> negotiatedDetails =
      MakeUnique<JsepTrackNegotiatedDetailsImpl>();
  negotiatedDetails->mProtocol = remoteMsection.GetProtocol();

  // Insert all the codecs we jointly support.
  PtrVector<JsepCodecDescription> commonCodecs(
      GetCommonCodecs(remoteMsection));

  for (const JsepCodecDescription* codec : commonCodecs.values) {
    bool sending = (direction == JsepTrack::kJsepTrackSending);

    // Everywhere else in JsepSessionImpl, a JsepCodecDescription describes
    // what one side puts in its SDP. However, we don't want that here; we want
    // a JsepCodecDescription that instead encapsulates all the parameters
    // that deal with sending (or receiving). For sending, some of these
    // parameters will come from the local codec config (eg; rtcp-fb), others
    // will come from the remote SDP (eg; max-fps), and still others can only be
    // determined by inspecting both local config and remote SDP (eg;
    // profile-level-id when level-asymmetry-allowed is 0).
    UniquePtr<JsepCodecDescription> sendOrReceiveCodec;

    if (sending) {
      sendOrReceiveCodec = Move(codec->MakeSendCodec(remoteMsection));
    } else {
      sendOrReceiveCodec = Move(codec->MakeRecvCodec(remoteMsection));
    }

    if (!sendOrReceiveCodec) {
      continue;
    }

    if (remoteMsection.GetMediaType() == SdpMediaSection::kAudio ||
        remoteMsection.GetMediaType() == SdpMediaSection::kVideo) {
      // Sanity-check that payload type can work with RTP
      uint16_t payloadType;
      if (!sendOrReceiveCodec->GetPtAsInt(&payloadType) ||
          payloadType > UINT8_MAX) {
        JSEP_SET_ERROR("audio/video payload type is not an 8 bit unsigned int: "
                       << codec->mDefaultPt);
        return NS_ERROR_INVALID_ARG;
      }
    }

    negotiatedDetails->mCodecs.values.push_back(sendOrReceiveCodec.release());
    break;
  }

  if (negotiatedDetails->mCodecs.values.empty()) {
    JSEP_SET_ERROR("Failed to negotiate codec details for all codecs");
    return NS_ERROR_INVALID_ARG;
  }

  auto& answerMsection = mIsOfferer ? remoteMsection : localMsection;

  if (answerMsection.GetAttributeList().HasAttribute(
        SdpAttribute::kExtmapAttribute)) {
    auto& extmap = answerMsection.GetAttributeList().GetExtmap().mExtmaps;
    for (auto i = extmap.begin(); i != extmap.end(); ++i) {
      negotiatedDetails->mExtmap[i->extensionname] = *i;
    }
  }

  (*track)->SetNegotiatedDetails(Move(negotiatedDetails));
  return NS_OK;
}

nsresult
JsepSessionImpl::CreateTransport(const SdpMediaSection& msection,
                                 RefPtr<JsepTransport>* transport)
{
  size_t components;

  if (HasRtcp(msection.GetProtocol())) {
    components = 2;
  } else {
    components = 1;
  }

  std::string id;
  if (msection.GetAttributeList().HasAttribute(SdpAttribute::kMidAttribute)) {
    id = msection.GetAttributeList().GetMid();
  } else {
    std::ostringstream os;
    os << "level_" << msection.GetLevel() << "(no mid)";
    id = os.str();
  }

  *transport = new JsepTransport(id, components);

  return NS_OK;
}

nsresult
JsepSessionImpl::SetupTransport(const SdpAttributeList& remote,
                                const SdpAttributeList& answer,
                                const RefPtr<JsepTransport>& transport)
{
  UniquePtr<JsepIceTransport> ice = MakeUnique<JsepIceTransport>();

  // We do sanity-checking for these in ParseSdp
  ice->mUfrag = remote.GetIceUfrag();
  ice->mPwd = remote.GetIcePwd();
  if (remote.HasAttribute(SdpAttribute::kCandidateAttribute)) {
    ice->mCandidates = remote.GetCandidate();
  }

  // RFC 5763 says:
  //
  //   The endpoint MUST use the setup attribute defined in [RFC4145].
  //   The endpoint that is the offerer MUST use the setup attribute
  //   value of setup:actpass and be prepared to receive a client_hello
  //   before it receives the answer.  The answerer MUST use either a
  //   setup attribute value of setup:active or setup:passive.  Note that
  //   if the answerer uses setup:passive, then the DTLS handshake will
  //   not begin until the answerer is received, which adds additional
  //   latency. setup:active allows the answer and the DTLS handshake to
  //   occur in parallel.  Thus, setup:active is RECOMMENDED.  Whichever
  //   party is active MUST initiate a DTLS handshake by sending a
  //   ClientHello over each flow (host/port quartet).
  UniquePtr<JsepDtlsTransport> dtls = MakeUnique<JsepDtlsTransport>();
  dtls->mFingerprints = remote.GetFingerprint();
  if (!answer.HasAttribute(mozilla::SdpAttribute::kSetupAttribute)) {
    dtls->mRole = mIsOfferer ? JsepDtlsTransport::kJsepDtlsServer
                             : JsepDtlsTransport::kJsepDtlsClient;
  } else {
    if (mIsOfferer) {
      dtls->mRole = (answer.GetSetup().mRole == SdpSetupAttribute::kActive)
                        ? JsepDtlsTransport::kJsepDtlsServer
                        : JsepDtlsTransport::kJsepDtlsClient;
    } else {
      dtls->mRole = (answer.GetSetup().mRole == SdpSetupAttribute::kActive)
                        ? JsepDtlsTransport::kJsepDtlsClient
                        : JsepDtlsTransport::kJsepDtlsServer;
    }
  }

  transport->mIce = Move(ice);
  transport->mDtls = Move(dtls);

  if (answer.HasAttribute(SdpAttribute::kRtcpMuxAttribute)) {
    transport->mComponents = 1;
  }

  transport->mState = JsepTransport::kJsepTransportAccepted;

  return NS_OK;
}

nsresult
JsepSessionImpl::AddTransportAttributes(SdpMediaSection* msection,
                                        SdpSetupAttribute::Role dtlsRole)
{
  if (mIceUfrag.empty() || mIcePwd.empty()) {
    JSEP_SET_ERROR("Missing ICE ufrag or password");
    return NS_ERROR_FAILURE;
  }

  SdpAttributeList& attrList = msection->GetAttributeList();
  attrList.SetAttribute(
      new SdpStringAttribute(SdpAttribute::kIceUfragAttribute, mIceUfrag));
  attrList.SetAttribute(
      new SdpStringAttribute(SdpAttribute::kIcePwdAttribute, mIcePwd));

  msection->GetAttributeList().SetAttribute(new SdpSetupAttribute(dtlsRole));

  return NS_OK;
}

nsresult
JsepSessionImpl::CopyTransportParams(const SdpMediaSection& source,
                                     SdpMediaSection* dest)
{
  // Copy over m-section details
  dest->SetPort(source.GetPort());
  dest->GetConnection() = source.GetConnection();

  auto& sourceAttrs = source.GetAttributeList();
  auto& destAttrs = dest->GetAttributeList();

  // Now we copy over attributes that won't be added by the usual logic
  if (sourceAttrs.HasAttribute(SdpAttribute::kCandidateAttribute)) {
    auto* candidateAttrs =
      new SdpMultiStringAttribute(SdpAttribute::kCandidateAttribute);
    candidateAttrs->mValues = sourceAttrs.GetCandidate();
    destAttrs.SetAttribute(candidateAttrs);
  }

  if (sourceAttrs.HasAttribute(SdpAttribute::kEndOfCandidatesAttribute)) {
    destAttrs.SetAttribute(
        new SdpFlagAttribute(SdpAttribute::kEndOfCandidatesAttribute));
  }

  if (!destAttrs.HasAttribute(SdpAttribute::kRtcpMuxAttribute) &&
      sourceAttrs.HasAttribute(SdpAttribute::kRtcpAttribute)) {
    // copy rtcp attribute
    destAttrs.SetAttribute(new SdpRtcpAttribute(sourceAttrs.GetRtcp()));
  }

  return NS_OK;
}

nsresult
JsepSessionImpl::CopyStickyParams(const SdpMediaSection& source,
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

nsresult
JsepSessionImpl::ParseSdp(const std::string& sdp, UniquePtr<Sdp>* parsedp)
{
  UniquePtr<Sdp> parsed = mParser.Parse(sdp);
  if (!parsed) {
    std::string error = "Failed to parse SDP: ";
    appendSdpParseErrors(mParser.GetParseErrors(), &error);
    JSEP_SET_ERROR(error);
    return NS_ERROR_INVALID_ARG;
  }

  // Verify that the JSEP rules for all SDP are followed
  if (!parsed->GetMediaSectionCount()) {
    JSEP_SET_ERROR("Description has no media sections");
    return NS_ERROR_INVALID_ARG;
  }

  std::set<std::string> trackIds;

  for (size_t i = 0; i < parsed->GetMediaSectionCount(); ++i) {
    if (MsectionIsDisabled(parsed->GetMediaSection(i))) {
      // Disabled, let this stuff slide.
      continue;
    }

    auto& mediaAttrs = parsed->GetMediaSection(i).GetAttributeList();

    if (mediaAttrs.GetIceUfrag().empty()) {
      JSEP_SET_ERROR("Invalid description, no ice-ufrag attribute");
      return NS_ERROR_INVALID_ARG;
    }

    if (mediaAttrs.GetIcePwd().empty()) {
      JSEP_SET_ERROR("Invalid description, no ice-pwd attribute");
      return NS_ERROR_INVALID_ARG;
    }

    if (!mediaAttrs.HasAttribute(SdpAttribute::kFingerprintAttribute)) {
      JSEP_SET_ERROR("Invalid description, no fingerprint attribute");
      return NS_ERROR_INVALID_ARG;
    }

    const SdpFingerprintAttributeList& fingerprints(
        mediaAttrs.GetFingerprint());
    if (fingerprints.mFingerprints.empty()) {
      JSEP_SET_ERROR("Invalid description, no supported fingerprint algorithms "
                     "present");
      return NS_ERROR_INVALID_ARG;
    }

    if (mediaAttrs.HasAttribute(SdpAttribute::kSetupAttribute) &&
        mediaAttrs.GetSetup().mRole == SdpSetupAttribute::kHoldconn) {
      JSEP_SET_ERROR("Description has illegal setup attribute "
                     "\"holdconn\" at level "
                     << i);
      return NS_ERROR_INVALID_ARG;
    }

    auto& formats = parsed->GetMediaSection(i).GetFormats();
    for (auto f = formats.begin(); f != formats.end(); ++f) {
      uint16_t pt;
      if (!JsepCodecDescription::GetPtAsInt(*f, &pt)) {
        JSEP_SET_ERROR("Payload type \""
                       << *f << "\" is not a 16-bit unsigned int at level "
                       << i);
        return NS_ERROR_INVALID_ARG;
      }
    }

    std::string streamId;
    std::string trackId;
    nsresult rv = GetIdsFromMsid(*parsed,
                                 parsed->GetMediaSection(i),
                                 &streamId,
                                 &trackId);

    if (NS_SUCCEEDED(rv)) {
      if (trackIds.count(trackId)) {
        JSEP_SET_ERROR("track id:" << trackId
                       << " appears in more than one m-section at level " << i);
        return NS_ERROR_INVALID_ARG;
      }

      trackIds.insert(trackId);
    } else if (rv != NS_ERROR_NOT_AVAILABLE) {
      // Error has already been set
      return rv;
    }
  }

  *parsedp = Move(parsed);
  return NS_OK;
}

nsresult
JsepSessionImpl::SetRemoteDescriptionOffer(UniquePtr<Sdp> offer)
{
  MOZ_ASSERT(mState == kJsepStateStable);

  // TODO(bug 1095780): Note that we create remote tracks even when
  // They contain only codecs we can't negotiate or other craziness.
  nsresult rv = SetRemoteTracksFromDescription(*offer);
  NS_ENSURE_SUCCESS(rv, rv);

  mPendingRemoteDescription = Move(offer);

  SetState(kJsepStateHaveRemoteOffer);
  return NS_OK;
}

nsresult
JsepSessionImpl::SetRemoteDescriptionAnswer(JsepSdpType type,
                                            UniquePtr<Sdp> answer)
{
  MOZ_ASSERT(mState == kJsepStateHaveLocalOffer ||
             mState == kJsepStateHaveRemotePranswer);

  mPendingRemoteDescription = Move(answer);

  nsresult rv = ValidateAnswer(*mPendingLocalDescription,
                               *mPendingRemoteDescription);
  NS_ENSURE_SUCCESS(rv, rv);

  // TODO(bug 1095780): Note that this creates remote tracks even if
  // we offered sendonly and other side offered sendrecv or recvonly.
  rv = SetRemoteTracksFromDescription(*mPendingRemoteDescription);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = HandleNegotiatedSession(mPendingLocalDescription,
                               mPendingRemoteDescription);
  NS_ENSURE_SUCCESS(rv, rv);

  mCurrentRemoteDescription = Move(mPendingRemoteDescription);
  mCurrentLocalDescription = Move(mPendingLocalDescription);

  SetState(kJsepStateStable);
  return NS_OK;
}

nsresult
JsepSessionImpl::SetRemoteTracksFromDescription(const Sdp& remoteDescription)
{
  // Unassign all remote tracks
  for (auto i = mRemoteTracks.begin(); i != mRemoteTracks.end(); ++i) {
    i->mAssignedMLine.reset();
  }

  size_t numMlines = remoteDescription.GetMediaSectionCount();
  nsresult rv;

  // Iterate over the sdp, re-assigning or creating remote tracks as we go
  for (size_t i = 0; i < numMlines; ++i) {
    const SdpMediaSection& msection = remoteDescription.GetMediaSection(i);

    if (MsectionIsDisabled(msection) || !msection.IsSending()) {
      continue;
    }

    std::vector<JsepReceivingTrack>::iterator track;

    if (msection.GetMediaType() == SdpMediaSection::kApplication) {
      // Datachannel doesn't have msid, just search by type
      track = FindUnassignedTrackByType(mRemoteTracks,
                                        msection.GetMediaType());
    } else {
      std::string streamId;
      std::string trackId;
      rv = GetRemoteIds(remoteDescription, msection, &streamId, &trackId);
      NS_ENSURE_SUCCESS(rv, rv);

      track = FindTrackByIds(mRemoteTracks, streamId, trackId);
    }

    if (track == mRemoteTracks.end()) {
      RefPtr<JsepTrack> track;
      rv = CreateReceivingTrack(i, remoteDescription, msection, &track);
      NS_ENSURE_SUCCESS(rv, rv);

      JsepReceivingTrack rtrack;
      rtrack.mTrack = track;
      rtrack.mAssignedMLine = Some(i);
      mRemoteTracks.push_back(rtrack);
      mRemoteTracksAdded.push_back(rtrack);
    } else {
      track->mAssignedMLine = Some(i);
    }
  }

  // Remove any unassigned remote track ids
  for (size_t i = 0; i < mRemoteTracks.size();) {
    if (!mRemoteTracks[i].mAssignedMLine.isSome()) {
      mRemoteTracksRemoved.push_back(mRemoteTracks[i]);
      mRemoteTracks.erase(mRemoteTracks.begin() + i);
    } else {
      ++i;
    }
  }

  return NS_OK;
}

nsresult
JsepSessionImpl::ValidateLocalDescription(const Sdp& description)
{
  // TODO(bug 1095226): Better checking.
  // (Also, at what point do we clear out the generated offer?)

  if (description.GetMediaSectionCount() !=
      mGeneratedLocalDescription->GetMediaSectionCount()) {
    JSEP_SET_ERROR("Changing the number of m-sections is not allowed");
    return NS_ERROR_INVALID_ARG;
  }

  for (size_t i = 0; i < description.GetMediaSectionCount(); ++i) {
    auto& origMsection = mGeneratedLocalDescription->GetMediaSection(i);
    auto& finalMsection = description.GetMediaSection(i);
    if (origMsection.GetMediaType() != finalMsection.GetMediaType()) {
      JSEP_SET_ERROR("Changing the media-type of m-sections is not allowed");
      return NS_ERROR_INVALID_ARG;
    }

    // These will be present in reoffer
    if (!mCurrentLocalDescription) {
      if (finalMsection.GetAttributeList().HasAttribute(
              SdpAttribute::kCandidateAttribute)) {
        JSEP_SET_ERROR("Adding your own candidate attributes is not supported");
        return NS_ERROR_INVALID_ARG;
      }

      if (finalMsection.GetAttributeList().HasAttribute(
              SdpAttribute::kEndOfCandidatesAttribute)) {
        JSEP_SET_ERROR("Why are you trying to set a=end-of-candidates?");
        return NS_ERROR_INVALID_ARG;
      }
    }

    // TODO(bug 1095218): Check msid
    // TODO(bug 1095226): Check ice-ufrag and ice-pwd
    // TODO(bug 1095226): Check fingerprints
    // TODO(bug 1095226): Check payload types (at least ensure that payload
    // types we don't actually support weren't added)
    // TODO(bug 1095226): Check ice-options?
  }

  if (description.GetAttributeList().HasAttribute(
          SdpAttribute::kIceLiteAttribute)) {
    JSEP_SET_ERROR("Running ICE in lite mode is unsupported");
    return NS_ERROR_INVALID_ARG;
  }

  return NS_OK;
}

nsresult
JsepSessionImpl::ValidateRemoteDescription(const Sdp& description)
{
  if (!mCurrentRemoteDescription || !mCurrentLocalDescription) {
    // Not renegotiation; checks for whether a remote answer are consistent
    // with our offer are handled in ValidateAnswer()
    return NS_OK;
  }

  if (mCurrentRemoteDescription->GetMediaSectionCount() >
      description.GetMediaSectionCount()) {
    JSEP_SET_ERROR("New remote description has fewer m-sections than the "
                   "previous remote description.");
    return NS_ERROR_INVALID_ARG;
  }

  std::set<std::string> oldBundleMids;
  const SdpMediaSection* oldBundleMsection;
  nsresult rv = GetNegotiatedBundleInfo(&oldBundleMids, &oldBundleMsection);
  NS_ENSURE_SUCCESS(rv, rv);

  std::set<std::string> newBundleMids;
  const SdpMediaSection* newBundleMsection;
  rv = GetBundleInfo(description, &newBundleMids, &newBundleMsection);
  NS_ENSURE_SUCCESS(rv, rv);

  for (size_t i = 0;
       i < mCurrentRemoteDescription->GetMediaSectionCount();
       ++i) {
    if (MsectionIsDisabled(description.GetMediaSection(i)) ||
        MsectionIsDisabled(mCurrentRemoteDescription->GetMediaSection(i))) {
      continue;
    }

    const SdpAttributeList& newAttrs(
        description.GetMediaSection(i).GetAttributeList());
    const SdpAttributeList& oldAttrs(
        mCurrentRemoteDescription->GetMediaSection(i).GetAttributeList());

    if ((newAttrs.GetIceUfrag() != oldAttrs.GetIceUfrag()) ||
        (newAttrs.GetIcePwd() != oldAttrs.GetIcePwd())) {
      JSEP_SET_ERROR("ICE restart is unsupported at this time "
                     "(new remote description changes either the ice-ufrag "
                     "or ice-pwd)" <<
                     "ice-ufrag (old): " << oldAttrs.GetIceUfrag() <<
                     "ice-ufrag (new): " << newAttrs.GetIceUfrag() <<
                     "ice-pwd (old): " << oldAttrs.GetIcePwd() <<
                     "ice-pwd (new): " << newAttrs.GetIcePwd());
      return NS_ERROR_INVALID_ARG;
    }
  }

  return NS_OK;
}

nsresult
JsepSessionImpl::ValidateAnswer(const Sdp& offer, const Sdp& answer)
{
  if (offer.GetMediaSectionCount() != answer.GetMediaSectionCount()) {
    JSEP_SET_ERROR("Offer and answer have different number of m-lines "
                   << "(" << offer.GetMediaSectionCount() << " vs "
                   << answer.GetMediaSectionCount() << ")");
    return NS_ERROR_INVALID_ARG;
  }

  for (size_t i = 0; i < offer.GetMediaSectionCount(); ++i) {
    const SdpMediaSection& offerMsection = offer.GetMediaSection(i);
    const SdpMediaSection& answerMsection = answer.GetMediaSection(i);

    if (offerMsection.GetMediaType() != answerMsection.GetMediaType()) {
      JSEP_SET_ERROR(
          "Answer and offer have different media types at m-line " << i);
      return NS_ERROR_INVALID_ARG;
    }

    if (!offerMsection.IsSending() && answerMsection.IsReceiving()) {
      JSEP_SET_ERROR("Answer tried to set recv when offer did not set send");
      return NS_ERROR_INVALID_ARG;
    }

    if (!offerMsection.IsReceiving() && answerMsection.IsSending()) {
      JSEP_SET_ERROR("Answer tried to set send when offer did not set recv");
      return NS_ERROR_INVALID_ARG;
    }

    const SdpAttributeList& answerAttrs(answerMsection.GetAttributeList());
    const SdpAttributeList& offerAttrs(offerMsection.GetAttributeList());
    if (answerAttrs.HasAttribute(SdpAttribute::kMidAttribute) &&
        offerAttrs.HasAttribute(SdpAttribute::kMidAttribute) &&
        offerAttrs.GetMid() != answerAttrs.GetMid()) {
      JSEP_SET_ERROR("Answer changes mid for level, was \'"
                     << offerMsection.GetAttributeList().GetMid()
                     << "\', now \'"
                     << answerMsection.GetAttributeList().GetMid() << "\'");
      return NS_ERROR_INVALID_ARG;
    }
  }

  return NS_OK;
}

nsresult
JsepSessionImpl::CreateReceivingTrack(size_t mline,
                                      const Sdp& sdp,
                                      const SdpMediaSection& msection,
                                      RefPtr<JsepTrack>* track)
{
  std::string streamId;
  std::string trackId;

  nsresult rv = GetRemoteIds(sdp, msection, &streamId, &trackId);
  NS_ENSURE_SUCCESS(rv, rv);

  *track = new JsepTrack(msection.GetMediaType(),
                            streamId,
                            trackId,
                            JsepTrack::kJsepTrackReceiving);

  (*track)->SetCNAME(GetCNAME(msection));

  return NS_OK;
}

nsresult
JsepSessionImpl::CreateGenericSDP(UniquePtr<Sdp>* sdpp)
{
  // draft-ietf-rtcweb-jsep-08 Section 5.2.1:
  //  o  The second SDP line MUST be an "o=" line, as specified in
  //     [RFC4566], Section 5.2.  The value of the <username> field SHOULD
  //     be "-".  The value of the <sess-id> field SHOULD be a
  //     cryptographically random number.  To ensure uniqueness, this
  //     number SHOULD be at least 64 bits long.  The value of the <sess-
  //     version> field SHOULD be zero.  The value of the <nettype>
  //     <addrtype> <unicast-address> tuple SHOULD be set to a non-
  //     meaningful address, such as IN IP4 0.0.0.0, to prevent leaking the
  //     local address in this field.  As mentioned in [RFC4566], the
  //     entire o= line needs to be unique, but selecting a random number
  //     for <sess-id> is sufficient to accomplish this.

  auto origin =
      SdpOrigin("mozilla...THIS_IS_SDPARTA-" MOZ_APP_UA_VERSION,
                mSessionId,
                mSessionVersion,
                sdp::kIPv4,
                "0.0.0.0");

  UniquePtr<Sdp> sdp = MakeUnique<SipccSdp>(origin);

  if (mDtlsFingerprints.empty()) {
    JSEP_SET_ERROR("Missing DTLS fingerprint");
    return NS_ERROR_FAILURE;
  }

  UniquePtr<SdpFingerprintAttributeList> fpl =
      MakeUnique<SdpFingerprintAttributeList>();
  for (auto fp = mDtlsFingerprints.begin(); fp != mDtlsFingerprints.end();
       ++fp) {
    fpl->PushEntry(fp->mAlgorithm, fp->mValue);
  }
  sdp->GetAttributeList().SetAttribute(fpl.release());

  auto* iceOpts = new SdpOptionsAttribute(SdpAttribute::kIceOptionsAttribute);
  iceOpts->PushEntry("trickle");
  sdp->GetAttributeList().SetAttribute(iceOpts);

  // This assumes content doesn't add a bunch of msid attributes with a
  // different semantic in mind.
  std::vector<std::string> msids;
  msids.push_back("*");
  SetupMsidSemantic(msids, sdp.get());

  *sdpp = Move(sdp);
  return NS_OK;
}

nsresult
JsepSessionImpl::SetupIds()
{
  SECStatus rv = PK11_GenerateRandom(
      reinterpret_cast<unsigned char*>(&mSessionId), sizeof(mSessionId));
  // RFC 3264 says that session-ids MUST be representable as a _signed_
  // 64 bit number, meaning the MSB cannot be set.
  mSessionId = mSessionId >> 1;
  if (rv != SECSuccess) {
    JSEP_SET_ERROR("Failed to generate session id: " << rv);
    return NS_ERROR_FAILURE;
  }

  if (!mUuidGen->Generate(&mDefaultRemoteStreamId)) {
    JSEP_SET_ERROR("Failed to generate default uuid for streams");
    return NS_ERROR_FAILURE;
  }

  if (!mUuidGen->Generate(&mCNAME)) {
    JSEP_SET_ERROR("Failed to generate CNAME");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

void
JsepSessionImpl::SetupDefaultCodecs()
{
  // Supported audio codecs.
  mCodecs.values.push_back(new JsepAudioCodecDescription(
      "109",
      "opus",
      48000,
      2,
      960,
      16000));

  mCodecs.values.push_back(new JsepAudioCodecDescription(
      "9",
      "G722",
      8000,
      1,
      320,
      64000));

  // packet size and bitrate values below copied from sipcc.
  // May need reevaluation from a media expert.
  mCodecs.values.push_back(
      new JsepAudioCodecDescription("0",
                                    "PCMU",
                                    8000,
                                    1,
                                    8000 / 50,   // frequency / 50
                                    8 * 8000 * 1 // 8 * frequency * channels
                                    ));

  mCodecs.values.push_back(
      new JsepAudioCodecDescription("8",
                                    "PCMA",
                                    8000,
                                    1,
                                    8000 / 50,   // frequency / 50
                                    8 * 8000 * 1 // 8 * frequency * channels
                                    ));

  // Supported video codecs.
  JsepVideoCodecDescription* vp8 = new JsepVideoCodecDescription(
      "120",
      "VP8",
      90000
      );
  // Defaults for mandatory params
  vp8->mMaxFs = 12288;
  vp8->mMaxFr = 60;
  mCodecs.values.push_back(vp8);

  JsepVideoCodecDescription* vp9 = new JsepVideoCodecDescription(
      "121",
      "VP9",
      90000
      );
  // Defaults for mandatory params
  vp9->mMaxFs = 12288;
  vp9->mMaxFr = 60;
  mCodecs.values.push_back(vp9);

  JsepVideoCodecDescription* h264_1 = new JsepVideoCodecDescription(
      "126",
      "H264",
      90000
      );
  h264_1->mPacketizationMode = 1;
  // Defaults for mandatory params
  h264_1->mProfileLevelId = 0x42E00D;
  mCodecs.values.push_back(h264_1);

  JsepVideoCodecDescription* h264_0 = new JsepVideoCodecDescription(
      "97",
      "H264",
      90000
      );
  h264_0->mPacketizationMode = 0;
  // Defaults for mandatory params
  h264_0->mProfileLevelId = 0x42E00D;
  mCodecs.values.push_back(h264_0);

  mCodecs.values.push_back(new JsepApplicationCodecDescription(
      "5000",
      "webrtc-datachannel",
      WEBRTC_DATACHANNEL_STREAMS_DEFAULT
      ));
}

void
JsepSessionImpl::SetupDefaultRtpExtensions()
{
  AddAudioRtpExtension("urn:ietf:params:rtp-hdrext:ssrc-audio-level");
}

void
JsepSessionImpl::SetState(JsepSignalingState state)
{
  if (state == mState)
    return;

  MOZ_MTLOG(ML_NOTICE, "[" << mName << "]: " <<
            GetStateStr(mState) << " -> " << GetStateStr(state));
  mState = state;
}

nsresult
JsepSessionImpl::AddCandidateToSdp(Sdp* sdp,
                                   const std::string& candidateUntrimmed,
                                   const std::string& mid,
                                   uint16_t level)
{

  if (level >= sdp->GetMediaSectionCount()) {
    JSEP_SET_ERROR("Index " << level << " out of range");
    return NS_ERROR_INVALID_ARG;
  }

  // Trim off '[a=]candidate:'
  size_t begin = candidateUntrimmed.find(':');
  if (begin == std::string::npos) {
    JSEP_SET_ERROR("Invalid candidate, no ':' (" << candidateUntrimmed << ")");
    return NS_ERROR_INVALID_ARG;
  }
  ++begin;

  std::string candidate = candidateUntrimmed.substr(begin);

  // TODO(bug 1095793): mid

  SdpMediaSection& msection = sdp->GetMediaSection(level);
  SdpAttributeList& attrList = msection.GetAttributeList();

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

nsresult
JsepSessionImpl::AddRemoteIceCandidate(const std::string& candidate,
                                       const std::string& mid,
                                       uint16_t level)
{
  mLastError.clear();

  mozilla::Sdp* sdp = 0;

  if (mPendingRemoteDescription) {
    sdp = mPendingRemoteDescription.get();
  } else if (mCurrentRemoteDescription) {
    sdp = mCurrentRemoteDescription.get();
  } else {
    JSEP_SET_ERROR("Cannot add ICE candidate in state " << GetStateStr(mState));
    return NS_ERROR_UNEXPECTED;
  }

  return AddCandidateToSdp(sdp, candidate, mid, level);
}

nsresult
JsepSessionImpl::AddLocalIceCandidate(const std::string& candidate,
                                      const std::string& mid,
                                      uint16_t level,
                                      bool* skipped)
{
  mLastError.clear();

  mozilla::Sdp* sdp = 0;

  if (mPendingLocalDescription) {
    sdp = mPendingLocalDescription.get();
  } else if (mCurrentLocalDescription) {
    sdp = mCurrentLocalDescription.get();
  } else {
    JSEP_SET_ERROR("Cannot add ICE candidate in state " << GetStateStr(mState));
    return NS_ERROR_UNEXPECTED;
  }

  if (sdp->GetMediaSectionCount() <= level) {
    // mainly here to make some testing less complicated, but also just in case
    *skipped = true;
    return NS_OK;
  }

  if (mState == kJsepStateStable) {
    const Sdp* answer(GetAnswer());
    if (IsBundleSlave(*answer, level)) {
      // We do not add candidate attributes to bundled m-sections unless they
      // are the "master" bundle m-section.
      *skipped = true;
      return NS_OK;
    }
  }

  *skipped = false;

  return AddCandidateToSdp(sdp, candidate, mid, level);
}

static void SetDefaultAddresses(const std::string& defaultCandidateAddr,
                                uint16_t defaultCandidatePort,
                                const std::string& defaultRtcpCandidateAddr,
                                uint16_t defaultRtcpCandidatePort,
                                SdpMediaSection* msection)
{
  msection->GetConnection().SetAddress(defaultCandidateAddr);
  msection->SetPort(defaultCandidatePort);
  if (!defaultRtcpCandidateAddr.empty()) {
    sdp::AddrType ipVersion = sdp::kIPv4;
    if (defaultRtcpCandidateAddr.find(':') != std::string::npos) {
      ipVersion = sdp::kIPv6;
    }
    msection->GetAttributeList().SetAttribute(new SdpRtcpAttribute(
          defaultRtcpCandidatePort,
          sdp::kInternet,
          ipVersion,
          defaultRtcpCandidateAddr));
  }
}

nsresult
JsepSessionImpl::EndOfLocalCandidates(const std::string& defaultCandidateAddr,
                                      uint16_t defaultCandidatePort,
                                      const std::string& defaultRtcpCandidateAddr,
                                      uint16_t defaultRtcpCandidatePort,
                                      uint16_t level)
{
  mLastError.clear();

  mozilla::Sdp* sdp = 0;

  if (mPendingLocalDescription) {
    sdp = mPendingLocalDescription.get();
  } else if (mCurrentLocalDescription) {
    sdp = mCurrentLocalDescription.get();
  } else {
    JSEP_SET_ERROR("Cannot add ICE candidate in state " << GetStateStr(mState));
    return NS_ERROR_UNEXPECTED;
  }

  std::set<std::string> bundleMids;
  const SdpMediaSection* bundleMsection;
  nsresult rv = GetNegotiatedBundleInfo(&bundleMids, &bundleMsection);
  if (NS_FAILED(rv)) {
    MOZ_ASSERT(false);
    mLastError += " (This should have been caught sooner!)";
    return NS_ERROR_FAILURE;
  }

  if (level < sdp->GetMediaSectionCount()) {
    SdpMediaSection& msection = sdp->GetMediaSection(level);

    if (msection.GetAttributeList().HasAttribute(SdpAttribute::kMidAttribute) &&
        bundleMids.count(msection.GetAttributeList().GetMid())) {
      if (msection.GetLevel() != bundleMsection->GetLevel()) {
        // Slave bundle m-section. Skip.
        return NS_OK;
      }

      // Master bundle m-section. Set defaultCandidateAddr and
      // defaultCandidatePort on all bundled m-sections.
      for (auto i = bundleMids.begin(); i != bundleMids.end(); ++i) {
        SdpMediaSection* bundledMsection = FindMsectionByMid(*sdp, *i);
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

    // TODO(bug 1095793): Will this have an mid someday?

    SdpAttributeList& attrs = msection.GetAttributeList();
    attrs.SetAttribute(
        new SdpFlagAttribute(SdpAttribute::kEndOfCandidatesAttribute));
    if (!mIsOfferer) {
      attrs.RemoveAttribute(SdpAttribute::kIceOptionsAttribute);
    }
  }

  return NS_OK;
}

SdpMediaSection*
JsepSessionImpl::FindMsectionByMid(Sdp& sdp,
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

const SdpMediaSection*
JsepSessionImpl::FindMsectionByMid(const Sdp& sdp,
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

const SdpGroupAttributeList::Group*
JsepSessionImpl::FindBundleGroup(const Sdp& sdp) const
{
  if (sdp.GetAttributeList().HasAttribute(SdpAttribute::kGroupAttribute)) {
    auto& groups = sdp.GetAttributeList().GetGroup().mGroups;
    for (auto i = groups.begin(); i != groups.end(); ++i) {
      if (i->semantics == SdpGroupAttributeList::kBundle) {
        return &(*i);
      }
    }
  }

  return nullptr;
}

nsresult
JsepSessionImpl::GetNegotiatedBundleInfo(
    std::set<std::string>* bundleMids,
    const SdpMediaSection** bundleMsection)
{
  mozilla::Sdp* answerSdp = 0;
  *bundleMsection = nullptr;

  if (IsOfferer()) {
    if (!mCurrentRemoteDescription) {
      // Offer/answer not done.
      return NS_OK;
    }

    answerSdp = mCurrentRemoteDescription.get();
  } else {
    if (mPendingLocalDescription) {
      answerSdp = mPendingLocalDescription.get();
    } else if (mCurrentLocalDescription) {
      answerSdp = mCurrentLocalDescription.get();
    } else {
      MOZ_ASSERT(false);
      JSEP_SET_ERROR("Is answerer, but no local description. This is a bug.");
      return NS_ERROR_FAILURE;
    }
  }

  return GetBundleInfo(*answerSdp, bundleMids, bundleMsection);
}

nsresult
JsepSessionImpl::GetBundleInfo(const Sdp& sdp,
                               std::set<std::string>* bundleMids,
                               const SdpMediaSection** bundleMsection)
{
  *bundleMsection = nullptr;

  auto* group = FindBundleGroup(sdp);
  if (group && !group->tags.empty()) {
    bundleMids->insert(group->tags.begin(), group->tags.end());
    *bundleMsection = FindMsectionByMid(sdp, group->tags[0]);
  }

  if (!bundleMids->empty()) {
    if (!*bundleMsection) {
      JSEP_SET_ERROR("mid specified for bundle transport in group attribute"
          " does not exist in the SDP. (mid="
          << group->tags[0] << ")");
      return NS_ERROR_INVALID_ARG;
    }

    if (MsectionIsDisabled(**bundleMsection)) {
      JSEP_SET_ERROR("mid specified for bundle transport in group attribute"
          " points at a disabled m-section. (mid="
          << group->tags[0] << ")");
      return NS_ERROR_INVALID_ARG;
    }
  }

  return NS_OK;
}

bool
JsepSessionImpl::IsBundleSlave(const Sdp& sdp, uint16_t level)
{
  auto& msection = sdp.GetMediaSection(level);

  if (!msection.GetAttributeList().HasAttribute(SdpAttribute::kMidAttribute)) {
    // No mid, definitely no bundle for this m-section
    return false;
  }

  std::set<std::string> bundleMids;
  const SdpMediaSection* bundleMsection;
  nsresult rv = GetBundleInfo(sdp, &bundleMids, &bundleMsection);
  if (NS_FAILED(rv)) {
    // Should have been caught sooner.
    MOZ_ASSERT(false);
    return false;
  }

  if (!bundleMsection) {
    return false;
  }

  std::string mid(msection.GetAttributeList().GetMid());

  if (bundleMids.count(mid) && level != bundleMsection->GetLevel()) {
    // mid is bundled, and isn't the bundle m-section
    return true;
  }

  return false;
}

void
JsepSessionImpl::DisableMsection(Sdp* sdp, SdpMediaSection* msection) const
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
  msection->ClearCodecs();

  // We need to have something here to fit the grammar
  // TODO(bcampen@mozilla.com): What's the accepted way of doing this? Just
  // add the codecs we do support? Does it matter?
  msection->AddCodec("111", "NULL", 0, 0);

  auto* direction =
    new SdpDirectionAttribute(SdpDirectionAttribute::kInactive);
  msection->GetAttributeList().SetAttribute(direction);
  msection->SetPort(0);
}

nsresult
JsepSessionImpl::EnableMsection(SdpMediaSection* msection)
{
  // We assert here because adding rtcp-mux to a non-disabled m-section that
  // did not already have rtcp-mux can cause problems.
  MOZ_ASSERT(MsectionIsDisabled(*msection));

  msection->SetPort(9);

  // We don't do this in AddTransportAttributes because that is also used for
  // making answers, and we don't want to unconditionally set rtcp-mux there.
  if (HasRtcp(msection->GetProtocol())) {
    // Set RTCP-MUX.
    msection->GetAttributeList().SetAttribute(
        new SdpFlagAttribute(SdpAttribute::kRtcpMuxAttribute));
  }

  nsresult rv = AddTransportAttributes(msection, SdpSetupAttribute::kActpass);
  NS_ENSURE_SUCCESS(rv, rv);

  AddCodecs(msection);

  AddExtmap(msection);

  std::ostringstream osMid;
  osMid << "sdparta_" << msection->GetLevel();
  AddMid(osMid.str(), msection);

  return NS_OK;
}

nsresult
JsepSessionImpl::GetAllPayloadTypes(
    const JsepTrackNegotiatedDetails& trackDetails,
    std::vector<uint8_t>* payloadTypesOut)
{
  for (size_t j = 0; j < trackDetails.GetCodecCount(); ++j) {
    const JsepCodecDescription* codec;
    nsresult rv = trackDetails.GetCodec(j, &codec);
    if (NS_FAILED(rv)) {
      JSEP_SET_ERROR("GetCodec failed in GetAllPayloadTypes. rv="
                     << static_cast<uint32_t>(rv));
      MOZ_ASSERT(false);
      return NS_ERROR_FAILURE;
    }

    uint16_t payloadType;
    if (!codec->GetPtAsInt(&payloadType) || payloadType > UINT8_MAX) {
      JSEP_SET_ERROR("Non-UINT8 payload type in GetAllPayloadTypes ("
                     << codec->mType
                     << "), this should have been caught sooner.");
      MOZ_ASSERT(false);
      return NS_ERROR_INVALID_ARG;
    }

    payloadTypesOut->push_back(payloadType);
  }

  return NS_OK;
}

// When doing bundle, if all else fails we can try to figure out which m-line a
// given RTP packet belongs to by looking at the payload type field. This only
// works, however, if that payload type appeared in only one m-section.
// We figure that out here.
nsresult
JsepSessionImpl::SetUniquePayloadTypes()
{
  // Maps to track details if no other track contains the payload type,
  // otherwise maps to nullptr.
  std::map<uint8_t, JsepTrackNegotiatedDetails*> payloadTypeToDetailsMap;

  for (size_t i = 0; i < mRemoteTracks.size(); ++i) {
    auto track = mRemoteTracks[i].mTrack;

    if (track->GetMediaType() == SdpMediaSection::kApplication) {
      continue;
    }

    auto* details = track->GetNegotiatedDetails();
    if (!details) {
      // Can happen if negotiation fails on a track
      continue;
    }

    // Renegotiation might cause a PT to no longer be unique
    details->ClearUniquePayloadTypes();

    std::vector<uint8_t> payloadTypesForTrack;
    nsresult rv = GetAllPayloadTypes(*details, &payloadTypesForTrack);
    NS_ENSURE_SUCCESS(rv, rv);

    for (auto j = payloadTypesForTrack.begin();
         j != payloadTypesForTrack.end();
         ++j) {

      if (payloadTypeToDetailsMap.count(*j)) {
        // Found in more than one track, not unique
        payloadTypeToDetailsMap[*j] = nullptr;
      } else {
        payloadTypeToDetailsMap[*j] = details;
      }
    }
  }

  for (auto i = payloadTypeToDetailsMap.begin();
       i != payloadTypeToDetailsMap.end();
       ++i) {
    uint8_t uniquePt = i->first;
    auto trackDetails = i->second;

    if (!trackDetails) {
      continue;
    }

    trackDetails->AddUniquePayloadType(uniquePt);
  }

  return NS_OK;
}

std::string
JsepSessionImpl::GetCNAME(const SdpMediaSection& msection) const
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

bool
JsepSessionImpl::MsectionIsDisabled(const SdpMediaSection& msection) const
{
  return !msection.GetPort() &&
         !msection.GetAttributeList().HasAttribute(
             SdpAttribute::kBundleOnlyAttribute);
}

const Sdp*
JsepSessionImpl::GetAnswer() const
{
  MOZ_ASSERT(mState == kJsepStateStable);

  return mIsOfferer ? mCurrentRemoteDescription.get()
                    : mCurrentLocalDescription.get();
}

nsresult
JsepSessionImpl::Close()
{
  mLastError.clear();
  SetState(kJsepStateClosed);
  return NS_OK;
}

const std::string
JsepSessionImpl::GetLastError() const
{
  return mLastError;
}

bool
JsepSessionImpl::AllLocalTracksAreAssigned() const
{
  for (auto i = mLocalTracks.begin(); i != mLocalTracks.end(); ++i) {
    if (!i->mAssignedMLine.isSome()) {
      return false;
    }
  }

  return true;
}

} // namespace mozilla
