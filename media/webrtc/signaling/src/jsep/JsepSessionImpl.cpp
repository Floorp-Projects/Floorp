/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "signaling/src/jsep/JsepSessionImpl.h"

#include <string>
#include <set>
#include <bitset>
#include <stdlib.h>

#include "plarena.h"
#include "nspr.h"
#include "nss.h"
#include "pk11pub.h"
#include "nsDebug.h"
#include "logging.h"

#include "mozilla/Move.h"
#include "mozilla/UniquePtr.h"

#include "webrtc/config.h"

#include "signaling/src/jsep/JsepTrack.h"
#include "signaling/src/jsep/JsepTrack.h"
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

static std::bitset<128> GetForbiddenSdpPayloadTypes() {
  std::bitset<128> forbidden(0);
  forbidden[1] = true;
  forbidden[2] = true;
  forbidden[19] = true;
  for (uint16_t i = 64; i < 96; ++i) {
    forbidden[i] = true;
  }
  return forbidden;
}

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
  MOZ_ASSERT(track->GetDirection() == sdp::kSend);
  MOZ_MTLOG(ML_DEBUG, "Adding track.");
  if (track->GetMediaType() != SdpMediaSection::kApplication) {
    track->SetCNAME(mCNAME);
    // Establish minimum number of required SSRCs
    // Note that AddTrack is only for send direction
    size_t minimumSsrcCount = 0;
    std::vector<JsepTrack::JsConstraints> constraints;
    track->GetJsConstraints(&constraints);
    for (auto constraint : constraints) {
      if (constraint.rid != "") {
        minimumSsrcCount++;
      }
    }
    // We need at least 1 SSRC
    minimumSsrcCount = std::max<size_t>(1, minimumSsrcCount);
    size_t currSsrcCount = track->GetSsrcs().size();
    if (currSsrcCount < minimumSsrcCount ) {
      MOZ_MTLOG(ML_DEBUG,
                "Adding " << (minimumSsrcCount - currSsrcCount) << " SSRCs.");
    }
    while (track->GetSsrcs().size() < minimumSsrcCount) {
      uint32_t ssrc=0;
      nsresult rv = CreateSsrc(&ssrc);
      NS_ENSURE_SUCCESS(rv, rv);
      // Don't add duplicate ssrcs
      std::vector<uint32_t> ssrcs = track->GetSsrcs();
      if (std::find(ssrcs.begin(), ssrcs.end(), ssrc) == ssrcs.end()) {
        track->AddSsrc(ssrc);
      }
    }
  }

  track->PopulateCodecs(mSupportedCodecs.values);

  JsepSendingTrack strack;
  strack.mTrack = track;

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
JsepSessionImpl::SetBundlePolicy(JsepBundlePolicy policy)
{
  mLastError.clear();
  if (mCurrentLocalDescription) {
    JSEP_SET_ERROR("Changing the bundle policy is only supported before the "
                   "first SetLocalDescription.");
    return NS_ERROR_UNEXPECTED;
  }

  mBundlePolicy = policy;
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
JsepSessionImpl::AddRtpExtension(std::vector<SdpExtmapAttributeList::Extmap>& extensions,
                                 const std::string& extensionName,
                                 SdpDirectionAttribute::Direction direction)
{
  mLastError.clear();

  if (extensions.size() + 1 > UINT16_MAX) {
    JSEP_SET_ERROR("Too many rtp extensions have been added");
    return NS_ERROR_FAILURE;
  }

  SdpExtmapAttributeList::Extmap extmap =
      { static_cast<uint16_t>(extensions.size() + 1),
        direction,
        direction != SdpDirectionAttribute::kSendrecv, // do we want to specify direction?
        extensionName,
        "" };

  extensions.push_back(extmap);
  return NS_OK;
}

nsresult
JsepSessionImpl::AddAudioRtpExtension(const std::string& extensionName,
                                      SdpDirectionAttribute::Direction direction)
{
  return AddRtpExtension(mAudioRtpExtensions, extensionName, direction);
}

nsresult
JsepSessionImpl::AddVideoRtpExtension(const std::string& extensionName,
                                      SdpDirectionAttribute::Direction direction)
{
  return AddRtpExtension(mVideoRtpExtensions, extensionName, direction);
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

nsresult
JsepSessionImpl::SetParameters(const std::string& streamId,
                               const std::string& trackId,
                               const std::vector<JsepTrack::JsConstraints>& constraints)
{
  auto it = FindTrackByIds(mLocalTracks, streamId, trackId);

  if (it == mLocalTracks.end()) {
    JSEP_SET_ERROR("Track " << streamId << "/" << trackId << " was never added.");
    return NS_ERROR_INVALID_ARG;
  }

  // Add RtpStreamId Extmap
  // SdpDirectionAttribute::Direction is a bitmask
  SdpDirectionAttribute::Direction addVideoExt = SdpDirectionAttribute::kInactive;
  SdpDirectionAttribute::Direction addAudioExt = SdpDirectionAttribute::kInactive;
  for (auto constraintEntry: constraints) {
    if (constraintEntry.rid != "") {
      switch (it->mTrack->GetMediaType()) {
        case SdpMediaSection::kVideo: {
          addVideoExt = static_cast<SdpDirectionAttribute::Direction>(addVideoExt
                                                                      | it->mTrack->GetDirection());
          break;
        }
        case SdpMediaSection::kAudio: {
          addAudioExt = static_cast<SdpDirectionAttribute::Direction>(addAudioExt
                                                                      | it->mTrack->GetDirection());
          break;
        }
        default: {
          MOZ_ASSERT(false);
          return NS_ERROR_INVALID_ARG;
        }
      }
    }
  }
  if (addVideoExt != SdpDirectionAttribute::kInactive) {
    AddVideoRtpExtension("urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id", addVideoExt);
  }

  it->mTrack->SetJsConstraints(constraints);

  auto track = it->mTrack;
  if (track->GetDirection() == sdp::kSend) {
    // Establish minimum number of required SSRCs
    // Note that AddTrack is only for send direction
    size_t minimumSsrcCount = 0;
    std::vector<JsepTrack::JsConstraints> constraints;
    track->GetJsConstraints(&constraints);
    for (auto constraint : constraints) {
      if (constraint.rid != "") {
        minimumSsrcCount++;
      }
    }
    // We need at least 1 SSRC
    minimumSsrcCount = std::max<size_t>(1, minimumSsrcCount);
    size_t currSsrcCount = track->GetSsrcs().size();
    if (currSsrcCount < minimumSsrcCount ) {
      MOZ_MTLOG(ML_DEBUG,
                "Adding " << (minimumSsrcCount - currSsrcCount) << " SSRCs.");
    }
    while (track->GetSsrcs().size() < minimumSsrcCount) {
      uint32_t ssrc=0;
      nsresult rv = CreateSsrc(&ssrc);
      NS_ENSURE_SUCCESS(rv, rv);
      // Don't add duplicate ssrcs
      std::vector<uint32_t> ssrcs = track->GetSsrcs();
      if (std::find(ssrcs.begin(), ssrcs.end(), ssrc) == ssrcs.end()) {
        track->AddSsrc(ssrc);
      }
    }
  }
  return NS_OK;
}

nsresult
JsepSessionImpl::GetParameters(const std::string& streamId,
                               const std::string& trackId,
                               std::vector<JsepTrack::JsConstraints>* outConstraints)
{
  auto it = FindTrackByIds(mLocalTracks, streamId, trackId);

  if (it == mLocalTracks.end()) {
    JSEP_SET_ERROR("Track " << streamId << "/" << trackId << " was never added.");
    return NS_ERROR_INVALID_ARG;
  }

  it->mTrack->GetJsConstraints(outConstraints);
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

nsresult
JsepSessionImpl::SetupOfferMSections(const JsepOfferOptions& options, Sdp* sdp)
{
  // First audio, then video, then datachannel, for interop
  // TODO(bug 1121756): We need to group these by stream-id, _then_ by media
  // type, according to the spec. However, this is not going to interop with
  // older versions of Firefox if a video-only stream is added before an
  // audio-only stream.
  // We should probably wait until 38 is ESR before trying to do this.
  nsresult rv = SetupOfferMSectionsByType(
      SdpMediaSection::kAudio, options.mOfferToReceiveAudio, sdp);

  NS_ENSURE_SUCCESS(rv, rv);

  rv = SetupOfferMSectionsByType(
      SdpMediaSection::kVideo, options.mOfferToReceiveVideo, sdp);

  NS_ENSURE_SUCCESS(rv, rv);

  if (!(options.mDontOfferDataChannel.isSome() &&
        *options.mDontOfferDataChannel)) {
    rv = SetupOfferMSectionsByType(
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
JsepSessionImpl::SetupOfferMSectionsByType(SdpMediaSection::MediaType mediatype,
                                           const Maybe<size_t>& offerToReceiveMaybe,
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
  rv = BindRemoteTracks(mediatype, sdp, offerToReceiveCountPtr);
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
JsepSessionImpl::BindLocalTracks(SdpMediaSection::MediaType mediatype, Sdp* sdp)
{
  for (JsepSendingTrack& track : mLocalTracks) {
    if (mediatype != track.mTrack->GetMediaType()) {
      continue;
    }

    SdpMediaSection* msection;
    if (track.mAssignedMLine.isSome()) {
      msection = &sdp->GetMediaSection(*track.mAssignedMLine);
    } else {
      nsresult rv = GetFreeMsectionForSend(track.mTrack->GetMediaType(),
                                           sdp,
                                           &msection);
      NS_ENSURE_SUCCESS(rv, rv);
      track.mAssignedMLine = Some(msection->GetLevel());
    }

    track.mTrack->AddToOffer(msection);
  }
  return NS_OK;
}

nsresult
JsepSessionImpl::BindRemoteTracks(SdpMediaSection::MediaType mediatype,
                                  Sdp* sdp,
                                  size_t* offerToReceive)
{
  for (JsepReceivingTrack& track : mRemoteTracks) {
    if (mediatype != track.mTrack->GetMediaType()) {
      continue;
    }

    if (!track.mAssignedMLine.isSome()) {
      MOZ_ASSERT(false);
      continue;
    }

    auto& msection = sdp->GetMediaSection(*track.mAssignedMLine);

    if (mSdpHelper.MsectionIsDisabled(msection)) {
      // TODO(bug 1095226) Content probably disabled this? Should we allow
      // content to do this?
      continue;
    }

    track.mTrack->AddToOffer(&msection);

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

    if (mSdpHelper.MsectionIsDisabled(msection) ||
        msection.GetMediaType() != mediatype ||
        msection.IsReceiving()) {
      continue;
    }

    if (offerToRecv) {
      if (*offerToRecv) {
        SetupOfferToReceiveMsection(&msection);
        --(*offerToRecv);
        continue;
      }
    } else if (msection.IsSending()) {
      SetupOfferToReceiveMsection(&msection);
      continue;
    }

    if (!msection.IsSending()) {
      // Unused m-section, and no reason to offer to recv on it
      mSdpHelper.DisableMsection(sdp, &msection);
    }
  }

  return NS_OK;
}

void
JsepSessionImpl::SetupOfferToReceiveMsection(SdpMediaSection* offer)
{
  // Create a dummy recv track, and have it add codecs, set direction, etc.
  RefPtr<JsepTrack> dummy = new JsepTrack(offer->GetMediaType(),
                                          "",
                                          "",
                                          sdp::kRecv);
  dummy->PopulateCodecs(mSupportedCodecs.values);
  dummy->AddToOffer(offer);
}

nsresult
JsepSessionImpl::AddRecvonlyMsections(SdpMediaSection::MediaType mediatype,
                                      size_t count,
                                      Sdp* sdp)
{
  while (count--) {
    nsresult rv = CreateOfferMSection(
        mediatype,
        mSdpHelper.GetProtocolForMediaType(mediatype),
        SdpDirectionAttribute::kRecvonly,
        sdp);

    NS_ENSURE_SUCCESS(rv, rv);
    SetupOfferToReceiveMsection(
        &sdp->GetMediaSection(sdp->GetMediaSectionCount() - 1));
  }
  return NS_OK;
}

// This function creates a skeleton SDP based on the old descriptions
// (ie; all m-sections are inactive).
nsresult
JsepSessionImpl::AddReofferMsections(const Sdp& oldLocalSdp,
                                     const Sdp& oldAnswer,
                                     Sdp* newSdp)
{
  nsresult rv;

  for (size_t i = 0; i < oldLocalSdp.GetMediaSectionCount(); ++i) {
    // We do not set the direction in this function (or disable when previously
    // disabled), that happens in |SetupOfferMSectionsByType|
    rv = CreateOfferMSection(oldLocalSdp.GetMediaSection(i).GetMediaType(),
                             oldLocalSdp.GetMediaSection(i).GetProtocol(),
                             SdpDirectionAttribute::kInactive,
                             newSdp);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mSdpHelper.CopyStickyParams(oldAnswer.GetMediaSection(i),
                                     &newSdp->GetMediaSection(i));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

void
JsepSessionImpl::SetupBundle(Sdp* sdp) const
{
  std::vector<std::string> mids;
  std::set<SdpMediaSection::MediaType> observedTypes;

  // This has the effect of changing the bundle level if the first m-section
  // goes from disabled to enabled. This is kinda inefficient.

  for (size_t i = 0; i < sdp->GetMediaSectionCount(); ++i) {
    auto& attrs = sdp->GetMediaSection(i).GetAttributeList();
    if (attrs.HasAttribute(SdpAttribute::kMidAttribute)) {
      bool useBundleOnly = false;
      switch (mBundlePolicy) {
        case kBundleMaxCompat:
          // We don't use bundle-only for max-compat
          break;
        case kBundleBalanced:
          // balanced means we use bundle-only on everything but the first
          // m-section of a given type
          if (observedTypes.count(sdp->GetMediaSection(i).GetMediaType())) {
            useBundleOnly = true;
          }
          observedTypes.insert(sdp->GetMediaSection(i).GetMediaType());
          break;
        case kBundleMaxBundle:
          // max-bundle means we use bundle-only on everything but the first
          // m-section
          useBundleOnly = !mids.empty();
          break;
      }

      if (useBundleOnly) {
        attrs.SetAttribute(
            new SdpFlagAttribute(SdpAttribute::kBundleOnlyAttribute));
        // Set port to 0 for sections with bundle-only attribute. (mjf)
        sdp->GetMediaSection(i).SetPort(0);
      }

      mids.push_back(attrs.GetMid());
    }
  }

  if (mids.size() >= 1) {
    UniquePtr<SdpGroupAttributeList> groupAttr(new SdpGroupAttributeList);
    groupAttr->PushEntry(SdpGroupAttributeList::kBundle, mids);
    sdp->GetAttributeList().SetAttribute(groupAttr.release());
  }
}

nsresult
JsepSessionImpl::GetRemoteIds(const Sdp& sdp,
                              const SdpMediaSection& msection,
                              std::string* streamId,
                              std::string* trackId)
{
  nsresult rv = mSdpHelper.GetIdsFromMsid(sdp, msection, streamId, trackId);
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
JsepSessionImpl::CreateOffer(const JsepOfferOptions& options,
                             std::string* offer)
{
  mLastError.clear();

  if (mState != kJsepStateStable) {
    JSEP_SET_ERROR("Cannot create offer in state " << GetStateStr(mState));
    return NS_ERROR_UNEXPECTED;
  }

  // Undo track assignments from a previous call to CreateOffer
  // (ie; if the track has not been negotiated yet, it doesn't necessarily need
  // to stay in the same m-section that it was in)
  for (JsepSendingTrack& trackWrapper : mLocalTracks) {
    if (!trackWrapper.mTrack->GetNegotiatedDetails()) {
      trackWrapper.mAssignedMLine.reset();
    }
  }

  UniquePtr<Sdp> sdp;

  // Make the basic SDP that is common to offer/answer.
  nsresult rv = CreateGenericSDP(&sdp);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mCurrentLocalDescription) {
    rv = AddReofferMsections(*mCurrentLocalDescription,
                             *GetAnswer(),
                             sdp.get());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Ensure that we have all the m-sections we need, and disable extras
  rv = SetupOfferMSections(options, sdp.get());
  NS_ENSURE_SUCCESS(rv, rv);

  SetupBundle(sdp.get());

  if (mCurrentLocalDescription) {
    rv = CopyPreviousTransportParams(*GetAnswer(),
                                     *mCurrentLocalDescription,
                                     *sdp,
                                     sdp.get());
    NS_ENSURE_SUCCESS(rv,rv);
  }

  *offer = sdp->ToString();
  mGeneratedLocalDescription = Move(sdp);
  ++mSessionVersion;

  return NS_OK;
}

std::string
JsepSessionImpl::GetLocalDescription() const
{
  std::ostringstream os;
  mozilla::Sdp* sdp = GetParsedLocalDescription();
  if (sdp) {
    sdp->Serialize(os);
  }
  return os.str();
}

std::string
JsepSessionImpl::GetRemoteDescription() const
{
  std::ostringstream os;
  mozilla::Sdp* sdp =  GetParsedRemoteDescription();
  if (sdp) {
    sdp->Serialize(os);
  }
  return os.str();
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

void
JsepSessionImpl::AddCommonExtmaps(const SdpMediaSection& remoteMsection,
                                  SdpMediaSection* msection)
{
  auto* ourExtensions = GetRtpExtensions(remoteMsection.GetMediaType());

  if (ourExtensions) {
    mSdpHelper.AddCommonExtmaps(remoteMsection, *ourExtensions, msection);
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

  // Copy the bundle groups into our answer
  UniquePtr<SdpGroupAttributeList> groupAttr(new SdpGroupAttributeList);
  mSdpHelper.GetBundleGroups(offer, &groupAttr->mGroups);
  sdp->GetAttributeList().SetAttribute(groupAttr.release());

  // Disable send for local tracks if the offer no longer allows it
  // (i.e., the m-section is recvonly, inactive or disabled)
  for (JsepSendingTrack& trackWrapper : mLocalTracks) {
    if (!trackWrapper.mAssignedMLine.isSome()) {
      continue;
    }

    // Get rid of all m-line assignments that have not been negotiated
    if (!trackWrapper.mTrack->GetNegotiatedDetails()) {
      trackWrapper.mAssignedMLine.reset();
      continue;
    }

    if (!offer.GetMediaSection(*trackWrapper.mAssignedMLine).IsReceiving()) {
      trackWrapper.mAssignedMLine.reset();
    }
  }

  size_t numMsections = offer.GetMediaSectionCount();

  for (size_t i = 0; i < numMsections; ++i) {
    const SdpMediaSection& remoteMsection = offer.GetMediaSection(i);
    rv = CreateAnswerMSection(options, i, remoteMsection, sdp.get());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (mCurrentLocalDescription) {
    // per discussion with bwc, 3rd parm here should be offer, not *sdp. (mjf)
    rv = CopyPreviousTransportParams(*GetAnswer(),
                                     *mCurrentRemoteDescription,
                                     offer,
                                     sdp.get());
    NS_ENSURE_SUCCESS(rv,rv);
  }

  *answer = sdp->ToString();
  mGeneratedLocalDescription = Move(sdp);
  ++mSessionVersion;

  return NS_OK;
}

nsresult
JsepSessionImpl::CreateOfferMSection(SdpMediaSection::MediaType mediatype,
                                     SdpMediaSection::Protocol proto,
                                     SdpDirectionAttribute::Direction dir,
                                     Sdp* sdp)
{
  SdpMediaSection* msection =
      &sdp->AddMediaSection(mediatype, dir, 0, proto, sdp::kIPv4, "0.0.0.0");

  return EnableOfferMsection(msection);
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

    if (mSdpHelper.MsectionIsDisabled(msection)) {
      // Was disabled; revive
      nsresult rv = EnableOfferMsection(&msection);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    *msectionOut = &msection;
    return NS_OK;
  }

  // Ok, no pre-existing m-section. Make a new one.
  nsresult rv = CreateOfferMSection(type,
                                    mSdpHelper.GetProtocolForMediaType(type),
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
                                      Sdp* sdp)
{
  SdpMediaSection& msection =
      sdp->AddMediaSection(remoteMsection.GetMediaType(),
                           SdpDirectionAttribute::kInactive,
                           9,
                           remoteMsection.GetProtocol(),
                           sdp::kIPv4,
                           "0.0.0.0");

  nsresult rv = mSdpHelper.CopyStickyParams(remoteMsection, &msection);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mSdpHelper.MsectionIsDisabled(remoteMsection)) {
    mSdpHelper.DisableMsection(sdp, &msection);
    return NS_OK;
  }

  SdpSetupAttribute::Role role;
  rv = DetermineAnswererSetupRole(remoteMsection, &role);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AddTransportAttributes(&msection, role);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = SetRecvonlySsrc(&msection);
  NS_ENSURE_SUCCESS(rv, rv);

  // Only attempt to match up local tracks if the offerer has elected to
  // receive traffic.
  if (remoteMsection.IsReceiving()) {
    rv = BindMatchingLocalTrackToAnswer(&msection);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (remoteMsection.IsSending()) {
    BindMatchingRemoteTrackToAnswer(&msection);
  }

  // Add extmap attributes.
  AddCommonExtmaps(remoteMsection, &msection);

  if (msection.GetFormats().empty()) {
    // Could not negotiate anything. Disable m-section.
    mSdpHelper.DisableMsection(sdp, &msection);
  }

  return NS_OK;
}

nsresult
JsepSessionImpl::SetRecvonlySsrc(SdpMediaSection* msection)
{
  if (msection->GetMediaType() == SdpMediaSection::kApplication) {
    return NS_OK;
  }

  // If previous m-sections are disabled, we do not call this function for them
  while (mRecvonlySsrcs.size() <= msection->GetLevel()) {
    uint32_t ssrc;
    nsresult rv = CreateSsrc(&ssrc);
    NS_ENSURE_SUCCESS(rv, rv);
    mRecvonlySsrcs.push_back(ssrc);
  }

  std::vector<uint32_t> ssrcs;
  ssrcs.push_back(mRecvonlySsrcs[msection->GetLevel()]);
  msection->SetSsrcs(ssrcs, mCNAME);
  return NS_OK;
}

nsresult
JsepSessionImpl::BindMatchingLocalTrackToAnswer(SdpMediaSection* msection)
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
    track->mAssignedMLine = Some(msection->GetLevel());
    track->mTrack->AddToAnswer(
        mPendingRemoteDescription->GetMediaSection(msection->GetLevel()),
        msection);
  }

  return NS_OK;
}

nsresult
JsepSessionImpl::BindMatchingRemoteTrackToAnswer(SdpMediaSection* msection)
{
  auto it = FindTrackByLevel(mRemoteTracks, msection->GetLevel());
  if (it == mRemoteTracks.end()) {
    MOZ_ASSERT(false);
    JSEP_SET_ERROR("Failed to find remote track for local answer m-section");
    return NS_ERROR_FAILURE;
  }

  it->mTrack->AddToAnswer(
      mPendingRemoteDescription->GetMediaSection(msection->GetLevel()),
      msection);
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

nsresult
JsepSessionImpl::SetLocalDescription(JsepSdpType type, const std::string& sdp)
{
  mLastError.clear();

  MOZ_MTLOG(ML_DEBUG, "SetLocalDescription type=" << type << "\nSDP=\n"
                                                  << sdp);

  if (type == kJsepSdpRollback) {
    if (mState != kJsepStateHaveLocalOffer) {
      JSEP_SET_ERROR("Cannot rollback local description in "
                     << GetStateStr(mState));
      return NS_ERROR_UNEXPECTED;
    }

    mPendingLocalDescription.reset();
    SetState(kJsepStateStable);
    mTransports = mOldTransports;
    mOldTransports.clear();
    return NS_OK;
  }

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
  mOldTransports = mTransports; // Save in case we need to rollback
  mTransports.clear();
  for (size_t t = 0; t < parsed->GetMediaSectionCount(); ++t) {
    mTransports.push_back(RefPtr<JsepTransport>(new JsepTransport));
    InitTransport(parsed->GetMediaSection(t), mTransports[t].get());
  }

  switch (type) {
    case kJsepSdpOffer:
      rv = SetLocalDescriptionOffer(Move(parsed));
      break;
    case kJsepSdpAnswer:
    case kJsepSdpPranswer:
      rv = SetLocalDescriptionAnswer(type, Move(parsed));
      break;
    case kJsepSdpRollback:
      MOZ_CRASH(); // Handled above
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
  mWasOffererLastTime = mIsOfferer;

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

  if (type == kJsepSdpRollback) {
    if (mState != kJsepStateHaveRemoteOffer) {
      JSEP_SET_ERROR("Cannot rollback remote description in "
                     << GetStateStr(mState));
      return NS_ERROR_UNEXPECTED;
    }

    mPendingRemoteDescription.reset();
    SetState(kJsepStateStable);

    // Update the remote tracks to what they were before the SetRemote
    return SetRemoteTracksFromDescription(mCurrentRemoteDescription.get());
  }

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

  // check for mismatch ufrag/pwd indicating ice restart
  // can't just check the first one because it might be disabled
  bool iceRestarting = false;
  if (mCurrentRemoteDescription.get()) {
    for (size_t i = 0;
         !iceRestarting &&
           i < mCurrentRemoteDescription->GetMediaSectionCount();
         ++i) {

      const SdpMediaSection& newMsection = parsed->GetMediaSection(i);
      const SdpMediaSection& oldMsection =
        mCurrentRemoteDescription->GetMediaSection(i);

      if (mSdpHelper.MsectionIsDisabled(newMsection) ||
          mSdpHelper.MsectionIsDisabled(oldMsection)) {
        continue;
      }

      iceRestarting = mSdpHelper.IceCredentialsDiffer(newMsection, oldMsection);
    }
  }

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
    case kJsepSdpRollback:
      MOZ_CRASH(); // Handled above
  }

  if (NS_SUCCEEDED(rv)) {
    mRemoteIsIceLite = iceLite;
    mIceOptions = iceOptions;
    mRemoteIceIsRestarting = iceRestarting;
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

  SdpHelper::BundledMids bundledMids;
  nsresult rv = mSdpHelper.GetBundledMids(answer, &bundledMids);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mTransports.size() < local->GetMediaSectionCount()) {
    JSEP_SET_ERROR("Fewer transports set up than m-lines");
    MOZ_ASSERT(false);
    return NS_ERROR_FAILURE;
  }

  for (JsepSendingTrack& trackWrapper : mLocalTracks) {
    trackWrapper.mTrack->ClearNegotiatedDetails();
  }

  for (JsepReceivingTrack& trackWrapper : mRemoteTracks) {
    trackWrapper.mTrack->ClearNegotiatedDetails();
  }

  std::vector<JsepTrackPair> trackPairs;

  // Now walk through the m-sections, make sure they match, and create
  // track pairs that describe the media to be set up.
  for (size_t i = 0; i < local->GetMediaSectionCount(); ++i) {
    // Skip disabled m-sections.
    if (answer.GetMediaSection(i).GetPort() == 0) {
      mTransports[i]->Close();
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
        if (bundledMids.count(answerMsection.GetAttributeList().GetMid())) {
          const SdpMediaSection* masterBundleMsection =
            bundledMids[answerMsection.GetAttributeList().GetMid()];
          transportLevel = masterBundleMsection->GetLevel();
          usingBundle = true;
          if (i != transportLevel) {
            mTransports[i]->Close();
          }
        }
      }
    }

    RefPtr<JsepTransport> transport = mTransports[transportLevel];

    rv = FinalizeTransport(
        remote->GetMediaSection(transportLevel).GetAttributeList(),
        answer.GetMediaSection(transportLevel).GetAttributeList(),
        transport);
    NS_ENSURE_SUCCESS(rv, rv);

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

  JsepTrack::SetUniquePayloadTypes(GetTracks(mRemoteTracks));

  // Ouch, this probably needs some dirty bit instead of just clearing
  // stuff for renegotiation.
  mNegotiatedTrackPairs = trackPairs;

  mGeneratedLocalDescription.reset();

  mNegotiations++;
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
  MOZ_ASSERT(transport->mComponents);
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

  if (local.GetMediaType() != SdpMediaSection::kApplication) {
    MOZ_ASSERT(mRecvonlySsrcs.size() > local.GetLevel(),
               "Failed to set the default ssrc for an active m-section");
    trackPairOut->mRecvonlySsrc = mRecvonlySsrcs[local.GetLevel()];
  }

  if (usingBundle) {
    trackPairOut->SetBundleLevel(transportLevel);
  }

  auto sendTrack = FindTrackByLevel(mLocalTracks, local.GetLevel());
  if (sendTrack != mLocalTracks.end()) {
    sendTrack->mTrack->Negotiate(answer, remote);
    sendTrack->mTrack->SetActive(sending);
    trackPairOut->mSending = sendTrack->mTrack;
  } else if (sending) {
    JSEP_SET_ERROR("Failed to find local track for level " <<
                   local.GetLevel()
                   << " in local SDP. This should never happen.");
    NS_ASSERTION(false, "Failed to find local track for level");
    return NS_ERROR_FAILURE;
  }

  auto recvTrack = FindTrackByLevel(mRemoteTracks, local.GetLevel());
  if (recvTrack != mRemoteTracks.end()) {
    recvTrack->mTrack->Negotiate(answer, remote);
    recvTrack->mTrack->SetActive(receiving);
    trackPairOut->mReceiving = recvTrack->mTrack;

    if (receiving &&
        trackPairOut->HasBundleLevel() &&
        recvTrack->mTrack->GetSsrcs().empty() &&
        recvTrack->mTrack->GetMediaType() != SdpMediaSection::kApplication) {
      MOZ_MTLOG(ML_ERROR, "Bundled m-section has no ssrc attributes. "
                          "This may cause media packets to be dropped.");
    }
  } else if (receiving) {
    JSEP_SET_ERROR("Failed to find remote track for level "
                   << local.GetLevel()
                   << " in remote SDP. This should never happen.");
    NS_ASSERTION(false, "Failed to find remote track for level");
    return NS_ERROR_FAILURE;
  }

  trackPairOut->mRtpTransport = transport;

  if (transport->mComponents == 2) {
    // RTCP MUX or not.
    // TODO(bug 1095743): verify that the PTs are consistent with mux.
    MOZ_MTLOG(ML_DEBUG, "RTCP-MUX is off");
    trackPairOut->mRtcpTransport = transport;
  }

  return NS_OK;
}

void
JsepSessionImpl::InitTransport(const SdpMediaSection& msection,
                               JsepTransport* transport)
{
  if (mSdpHelper.MsectionIsDisabled(msection)) {
    transport->Close();
    return;
  }

  if (mSdpHelper.HasRtcp(msection.GetProtocol())) {
    transport->mComponents = 2;
  } else {
    transport->mComponents = 1;
  }

  if (msection.GetAttributeList().HasAttribute(SdpAttribute::kMidAttribute)) {
    transport->mTransportId = msection.GetAttributeList().GetMid();
  } else {
    std::ostringstream os;
    os << "level_" << msection.GetLevel() << "(no mid)";
    transport->mTransportId = os.str();
  }
}

nsresult
JsepSessionImpl::FinalizeTransport(const SdpAttributeList& remote,
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
JsepSessionImpl::CopyPreviousTransportParams(const Sdp& oldAnswer,
                                             const Sdp& offerersPreviousSdp,
                                             const Sdp& newOffer,
                                             Sdp* newLocal)
{
  for (size_t i = 0; i < oldAnswer.GetMediaSectionCount(); ++i) {
    if (!mSdpHelper.MsectionIsDisabled(newLocal->GetMediaSection(i)) &&
        mSdpHelper.AreOldTransportParamsValid(oldAnswer,
                                              offerersPreviousSdp,
                                              newOffer,
                                              i) &&
        !mRemoteIceIsRestarting
       ) {
      // If newLocal is an offer, this will be the number of components we used
      // last time, and if it is an answer, this will be the number of
      // components we've decided we're using now.
      size_t numComponents = mTransports[i]->mComponents;
      nsresult rv = mSdpHelper.CopyTransportParams(
          numComponents,
          mCurrentLocalDescription->GetMediaSection(i),
          &newLocal->GetMediaSection(i));
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

nsresult
JsepSessionImpl::ParseSdp(const std::string& sdp, UniquePtr<Sdp>* parsedp)
{
  UniquePtr<Sdp> parsed = mParser.Parse(sdp);
  if (!parsed) {
    std::string error = "Failed to parse SDP: ";
    mSdpHelper.appendSdpParseErrors(mParser.GetParseErrors(), &error);
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
    if (mSdpHelper.MsectionIsDisabled(parsed->GetMediaSection(i))) {
      // Disabled, let this stuff slide.
      continue;
    }

    const SdpMediaSection& msection(parsed->GetMediaSection(i));
    auto& mediaAttrs = msection.GetAttributeList();

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

    if (mediaAttrs.HasAttribute(SdpAttribute::kSetupAttribute, true) &&
        mediaAttrs.GetSetup().mRole == SdpSetupAttribute::kHoldconn) {
      JSEP_SET_ERROR("Description has illegal setup attribute "
                     "\"holdconn\" in m-section at level "
                     << i);
      return NS_ERROR_INVALID_ARG;
    }

    std::string streamId;
    std::string trackId;
    nsresult rv = mSdpHelper.GetIdsFromMsid(*parsed,
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

    static const std::bitset<128> forbidden = GetForbiddenSdpPayloadTypes();
    if (msection.GetMediaType() == SdpMediaSection::kAudio ||
        msection.GetMediaType() == SdpMediaSection::kVideo) {
      // Sanity-check that payload type can work with RTP
      for (const std::string& fmt : msection.GetFormats()) {
        uint16_t payloadType;
        if (!SdpHelper::GetPtAsInt(fmt, &payloadType)) {
          JSEP_SET_ERROR("Payload type \"" << fmt <<
                         "\" is not a 16-bit unsigned int at level " << i);
          return NS_ERROR_INVALID_ARG;
        }
        if (payloadType > 127) {
          JSEP_SET_ERROR("audio/video payload type \"" << fmt <<
                         "\" is too large at level " << i);
          return NS_ERROR_INVALID_ARG;
        }
        if (forbidden.test(payloadType)) {
          JSEP_SET_ERROR("Illegal audio/video payload type \"" << fmt <<
                         "\" at level " << i);
          return NS_ERROR_INVALID_ARG;
        }
      }
    }
  }

  *parsedp = Move(parsed);
  return NS_OK;
}

nsresult
JsepSessionImpl::SetRemoteDescriptionOffer(UniquePtr<Sdp> offer)
{
  MOZ_ASSERT(mState == kJsepStateStable);

  nsresult rv = ValidateOffer(*offer);
  NS_ENSURE_SUCCESS(rv, rv);

  // TODO(bug 1095780): Note that we create remote tracks even when
  // They contain only codecs we can't negotiate or other craziness.
  rv = SetRemoteTracksFromDescription(offer.get());
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
  rv = SetRemoteTracksFromDescription(mPendingRemoteDescription.get());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = HandleNegotiatedSession(mPendingLocalDescription,
                               mPendingRemoteDescription);
  NS_ENSURE_SUCCESS(rv, rv);

  mCurrentRemoteDescription = Move(mPendingRemoteDescription);
  mCurrentLocalDescription = Move(mPendingLocalDescription);
  mWasOffererLastTime = mIsOfferer;

  SetState(kJsepStateStable);
  return NS_OK;
}

nsresult
JsepSessionImpl::SetRemoteTracksFromDescription(const Sdp* remoteDescription)
{
  // Unassign all remote tracks
  for (auto& remoteTrack : mRemoteTracks) {
    remoteTrack.mAssignedMLine.reset();
  }

  // This will not exist if we're rolling back the first remote description
  if (remoteDescription) {
    size_t numMlines = remoteDescription->GetMediaSectionCount();
    nsresult rv;

    // Iterate over the sdp, re-assigning or creating remote tracks as we go
    for (size_t i = 0; i < numMlines; ++i) {
      const SdpMediaSection& msection = remoteDescription->GetMediaSection(i);

      if (mSdpHelper.MsectionIsDisabled(msection) || !msection.IsSending()) {
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
        rv = GetRemoteIds(*remoteDescription, msection, &streamId, &trackId);
        NS_ENSURE_SUCCESS(rv, rv);

        track = FindTrackByIds(mRemoteTracks, streamId, trackId);
      }

      if (track == mRemoteTracks.end()) {
        RefPtr<JsepTrack> track;
        rv = CreateReceivingTrack(i, *remoteDescription, msection, &track);
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
  if (!mGeneratedLocalDescription) {
    JSEP_SET_ERROR("Calling SetLocal without first calling CreateOffer/Answer"
                   " is not supported.");
    return NS_ERROR_UNEXPECTED;
  }

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

  // These are solely to check that bundle is valid
  SdpHelper::BundledMids bundledMids;
  nsresult rv = GetNegotiatedBundledMids(&bundledMids);
  NS_ENSURE_SUCCESS(rv, rv);

  SdpHelper::BundledMids newBundledMids;
  rv = mSdpHelper.GetBundledMids(description, &newBundledMids);
  NS_ENSURE_SUCCESS(rv, rv);

  // check for partial ice restart, which is not supported
  Maybe<bool> iceCredsDiffer;
  for (size_t i = 0;
       i < mCurrentRemoteDescription->GetMediaSectionCount();
       ++i) {

    const SdpMediaSection& newMsection = description.GetMediaSection(i);
    const SdpMediaSection& oldMsection =
      mCurrentRemoteDescription->GetMediaSection(i);

    if (mSdpHelper.MsectionIsDisabled(newMsection) ||
        mSdpHelper.MsectionIsDisabled(oldMsection)) {
      continue;
    }

    if (oldMsection.GetMediaType() != newMsection.GetMediaType()) {
      JSEP_SET_ERROR("Remote description changes the media type of m-line "
                     << i);
      return NS_ERROR_INVALID_ARG;
    }

    bool differ = mSdpHelper.IceCredentialsDiffer(newMsection, oldMsection);
    // Detect whether all the creds are the same or all are different
    if (!iceCredsDiffer.isSome()) {
      // for the first msection capture whether creds are different or same
      iceCredsDiffer = mozilla::Some(differ);
    } else if (iceCredsDiffer.isSome() && *iceCredsDiffer != differ) {
      // subsequent msections must match the first sections
      JSEP_SET_ERROR("Partial ICE restart is unsupported at this time "
                     "(new remote description changes either the ice-ufrag "
                     "or ice-pwd on fewer than all msections)");
      return NS_ERROR_INVALID_ARG;
    }
  }

  return NS_OK;
}

nsresult
JsepSessionImpl::ValidateOffer(const Sdp& offer)
{
  for (size_t i = 0; i < offer.GetMediaSectionCount(); ++i) {
    const SdpMediaSection& offerMsection = offer.GetMediaSection(i);
    if (mSdpHelper.MsectionIsDisabled(offerMsection)) {
      continue;
    }

    const SdpAttributeList& offerAttrs(offerMsection.GetAttributeList());
    if (!offerAttrs.HasAttribute(SdpAttribute::kSetupAttribute, true)) {
      JSEP_SET_ERROR("Offer is missing required setup attribute "
                     " at level " << i);
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

    if (answerAttrs.HasAttribute(SdpAttribute::kSetupAttribute, true) &&
        answerAttrs.GetSetup().mRole == SdpSetupAttribute::kActpass) {
      JSEP_SET_ERROR("Answer contains illegal setup attribute \"actpass\""
                     " at level " << i);
      return NS_ERROR_INVALID_ARG;
    }

    // Sanity check extmap
    if (answerAttrs.HasAttribute(SdpAttribute::kExtmapAttribute)) {
      if (!offerAttrs.HasAttribute(SdpAttribute::kExtmapAttribute)) {
        JSEP_SET_ERROR("Answer adds extmap attributes to level " << i);
        return NS_ERROR_INVALID_ARG;
      }

      for (const auto& ansExt : answerAttrs.GetExtmap().mExtmaps) {
        bool found = false;
        for (const auto& offExt : offerAttrs.GetExtmap().mExtmaps) {
          if (ansExt.extensionname == offExt.extensionname) {
            if ((ansExt.direction & reverse(offExt.direction))
                  != ansExt.direction) {
              // FIXME we do not return an error here, because Chrome up to
              // version 57 is actually tripping over this if they are the
              // answerer. See bug 1355010 for details.
              MOZ_MTLOG(ML_WARNING, "Answer has inconsistent direction on extmap "
                             "attribute at level " << i << " ("
                             << ansExt.extensionname << "). Offer had "
                             << offExt.direction << ", answer had "
                             << ansExt.direction << ".");
              // return NS_ERROR_INVALID_ARG;
            }

            if (offExt.entry < 4096 && (offExt.entry != ansExt.entry)) {
              // FIXME we do not return an error here, because Cisco Spark
              // actually does respond with different extension ID's then we
              // offer. See bug 1361206 for details.
              MOZ_MTLOG(ML_WARNING, "Answer changed id for extmap attribute at"
                        " level " << i << " (" << offExt.extensionname << ") "
                        "from " << offExt.entry << " to "
                        << ansExt.entry << ".");
              // return NS_ERROR_INVALID_ARG;
            }

            if (ansExt.entry >= 4096) {
              JSEP_SET_ERROR("Answer used an invalid id (" << ansExt.entry
                             << ") for extmap attribute at level " << i
                             << " (" << ansExt.extensionname << ").");
              return NS_ERROR_INVALID_ARG;
            }

            found = true;
            break;
          }
        }

        if (!found) {
          JSEP_SET_ERROR("Answer has extmap " << ansExt.extensionname << " at "
                         "level " << i << " that was not present in offer.");
          return NS_ERROR_INVALID_ARG;
        }
      }
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
                         sdp::kRecv);

  (*track)->SetCNAME(mSdpHelper.GetCNAME(msection));
  (*track)->PopulateCodecs(mSupportedCodecs.values);

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
  for (auto& dtlsFingerprint : mDtlsFingerprints) {
    fpl->PushEntry(dtlsFingerprint.mAlgorithm, dtlsFingerprint.mValue);
  }
  sdp->GetAttributeList().SetAttribute(fpl.release());

  auto* iceOpts = new SdpOptionsAttribute(SdpAttribute::kIceOptionsAttribute);
  iceOpts->PushEntry("trickle");
  sdp->GetAttributeList().SetAttribute(iceOpts);

  // This assumes content doesn't add a bunch of msid attributes with a
  // different semantic in mind.
  std::vector<std::string> msids;
  msids.push_back("*");
  mSdpHelper.SetupMsidSemantic(msids, sdp.get());

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

nsresult
JsepSessionImpl::CreateSsrc(uint32_t* ssrc)
{
  do {
    SECStatus rv = PK11_GenerateRandom(
        reinterpret_cast<unsigned char*>(ssrc), sizeof(uint32_t));
    if (rv != SECSuccess) {
      JSEP_SET_ERROR("Failed to generate SSRC, error=" << rv);
      return NS_ERROR_FAILURE;
    }
  } while (mSsrcs.count(*ssrc));
  mSsrcs.insert(*ssrc);

  return NS_OK;
}

void
JsepSessionImpl::SetupDefaultCodecs()
{
  // Supported audio codecs.
  // Per jmspeex on IRC:
  // For 32KHz sampling, 28 is ok, 32 is good, 40 should be really good
  // quality.  Note that 1-2Kbps will be wasted on a stereo Opus channel
  // with mono input compared to configuring it for mono.
  // If we reduce bitrate enough Opus will low-pass us; 16000 will kill a
  // 9KHz tone.  This should be adaptive when we're at the low-end of video
  // bandwidth (say <100Kbps), and if we're audio-only, down to 8 or
  // 12Kbps.
  mSupportedCodecs.values.push_back(new JsepAudioCodecDescription(
      "109",
      "opus",
      48000,
      2,
      960,
#ifdef WEBRTC_GONK
      // TODO Move this elsewhere to be adaptive to rate - Bug 1207925
      16000 // B2G uses lower capture sampling rate
#else
      40000
#endif
      ));

  mSupportedCodecs.values.push_back(new JsepAudioCodecDescription(
      "9",
      "G722",
      8000,
      1,
      320,
      64000));

  // packet size and bitrate values below copied from sipcc.
  // May need reevaluation from a media expert.
  mSupportedCodecs.values.push_back(
      new JsepAudioCodecDescription("0",
                                    "PCMU",
                                    8000,
                                    1,
                                    8000 / 50,   // frequency / 50
                                    8 * 8000 * 1 // 8 * frequency * channels
                                    ));

  mSupportedCodecs.values.push_back(
      new JsepAudioCodecDescription("8",
                                    "PCMA",
                                    8000,
                                    1,
                                    8000 / 50,   // frequency / 50
                                    8 * 8000 * 1 // 8 * frequency * channels
                                    ));

  // note: because telephone-event is effectively a marker codec that indicates
  // that dtmf rtp packets may be passed, the packetSize and bitRate fields
  // don't make sense here.  For now, use zero. (mjf)
  mSupportedCodecs.values.push_back(
      new JsepAudioCodecDescription("101",
                                    "telephone-event",
                                    8000,
                                    1,
                                    0, // packetSize doesn't make sense here
                                    0  // bitRate doesn't make sense here
                                    ));

  // Supported video codecs.
  // Note: order here implies priority for building offers!
  JsepVideoCodecDescription* vp8 = new JsepVideoCodecDescription(
      "120",
      "VP8",
      90000
      );
  // Defaults for mandatory params
  vp8->mConstraints.maxFs = 12288; // Enough for 2048x1536
  vp8->mConstraints.maxFps = 60;
  mSupportedCodecs.values.push_back(vp8);

  JsepVideoCodecDescription* vp9 = new JsepVideoCodecDescription(
      "121",
      "VP9",
      90000
      );
  // Defaults for mandatory params
  vp9->mConstraints.maxFs = 12288; // Enough for 2048x1536
  vp9->mConstraints.maxFps = 60;
  mSupportedCodecs.values.push_back(vp9);

  JsepVideoCodecDescription* h264_1 = new JsepVideoCodecDescription(
      "126",
      "H264",
      90000
      );
  h264_1->mPacketizationMode = 1;
  // Defaults for mandatory params
  h264_1->mProfileLevelId = 0x42E00D;
  mSupportedCodecs.values.push_back(h264_1);

  JsepVideoCodecDescription* h264_0 = new JsepVideoCodecDescription(
      "97",
      "H264",
      90000
      );
  h264_0->mPacketizationMode = 0;
  // Defaults for mandatory params
  h264_0->mProfileLevelId = 0x42E00D;
  mSupportedCodecs.values.push_back(h264_0);

  JsepVideoCodecDescription* red = new JsepVideoCodecDescription(
      "122", // payload type
      "red", // codec name
      90000  // clock rate (match other video codecs)
      );
  mSupportedCodecs.values.push_back(red);

  JsepVideoCodecDescription* ulpfec = new JsepVideoCodecDescription(
      "123",    // payload type
      "ulpfec", // codec name
      90000     // clock rate (match other video codecs)
      );
  mSupportedCodecs.values.push_back(ulpfec);

  mSupportedCodecs.values.push_back(new JsepApplicationCodecDescription(
      "webrtc-datachannel",
      WEBRTC_DATACHANNEL_STREAMS_DEFAULT,
      WEBRTC_DATACHANNEL_PORT_DEFAULT,
      // TODO: Bug 979417 needs to change this to
      // WEBRTC_DATACHANELL_MAX_MESSAGE_SIZE_DEFAULT
      0
      ));

  // Update the redundant encodings for the RED codec with the supported
  // codecs.  Note: only uses the video codecs.
  red->UpdateRedundantEncodings(mSupportedCodecs.values);
}

void
JsepSessionImpl::SetupDefaultRtpExtensions()
{
  AddAudioRtpExtension("urn:ietf:params:rtp-hdrext:ssrc-audio-level",
                       SdpDirectionAttribute::Direction::kSendonly);
  AddVideoRtpExtension(
    "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time",
                       SdpDirectionAttribute::Direction::kSendrecv);
  AddVideoRtpExtension("urn:ietf:params:rtp-hdrext:toffset",
                       SdpDirectionAttribute::Direction::kSendrecv);
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
JsepSessionImpl::AddRemoteIceCandidate(const std::string& candidate,
                                       const std::string& mid,
                                       uint16_t level)
{
  mLastError.clear();

  mozilla::Sdp* sdp = GetParsedRemoteDescription();

  if (!sdp) {
    JSEP_SET_ERROR("Cannot add ICE candidate in state " << GetStateStr(mState));
    return NS_ERROR_UNEXPECTED;
  }

  return mSdpHelper.AddCandidateToSdp(sdp, candidate, mid, level);
}

nsresult
JsepSessionImpl::AddLocalIceCandidate(const std::string& candidate,
                                      uint16_t level,
                                      std::string* mid,
                                      bool* skipped)
{
  mLastError.clear();

  mozilla::Sdp* sdp = GetParsedLocalDescription();

  if (!sdp) {
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
    if (mSdpHelper.IsBundleSlave(*answer, level)) {
      // We do not add candidate attributes to bundled m-sections unless they
      // are the "master" bundle m-section.
      *skipped = true;
      return NS_OK;
    }
  }

  nsresult rv = mSdpHelper.GetMidFromLevel(*sdp, level, mid);
  if (NS_FAILED(rv)) {
    return rv;
  }

  *skipped = false;

  return mSdpHelper.AddCandidateToSdp(sdp, candidate, *mid, level);
}

nsresult
JsepSessionImpl::UpdateDefaultCandidate(
    const std::string& defaultCandidateAddr,
    uint16_t defaultCandidatePort,
    const std::string& defaultRtcpCandidateAddr,
    uint16_t defaultRtcpCandidatePort,
    uint16_t level)
{
  mLastError.clear();

  mozilla::Sdp* sdp = GetParsedLocalDescription();

  if (!sdp) {
    JSEP_SET_ERROR("Cannot add ICE candidate in state " << GetStateStr(mState));
    return NS_ERROR_UNEXPECTED;
  }

  if (level >= sdp->GetMediaSectionCount()) {
    return NS_OK;
  }

  std::string defaultRtcpCandidateAddrCopy(defaultRtcpCandidateAddr);
  if (mState == kJsepStateStable && mTransports[level]->mComponents == 1) {
    // We know we're doing rtcp-mux by now. Don't create an rtcp attr.
    defaultRtcpCandidateAddrCopy = "";
    defaultRtcpCandidatePort = 0;
  }

  // If offer/answer isn't done, it is too early to tell whether these defaults
  // need to be applied to other m-sections.
  SdpHelper::BundledMids bundledMids;
  if (mState == kJsepStateStable) {
    nsresult rv = GetNegotiatedBundledMids(&bundledMids);
    if (NS_FAILED(rv)) {
      MOZ_ASSERT(false);
      mLastError += " (This should have been caught sooner!)";
      return NS_ERROR_FAILURE;
    }
  }

  mSdpHelper.SetDefaultAddresses(
      defaultCandidateAddr,
      defaultCandidatePort,
      defaultRtcpCandidateAddrCopy,
      defaultRtcpCandidatePort,
      sdp,
      level,
      bundledMids);

  return NS_OK;
}

nsresult
JsepSessionImpl::EndOfLocalCandidates(uint16_t level)
{
  mLastError.clear();

  mozilla::Sdp* sdp = GetParsedLocalDescription();

  if (!sdp) {
    JSEP_SET_ERROR("Cannot mark end of local ICE candidates in state "
                   << GetStateStr(mState));
    return NS_ERROR_UNEXPECTED;
  }

  if (level >= sdp->GetMediaSectionCount()) {
    return NS_OK;
  }

  // If offer/answer isn't done, it is too early to tell whether this update
  // needs to be applied to other m-sections.
  SdpHelper::BundledMids bundledMids;
  if (mState == kJsepStateStable) {
    nsresult rv = GetNegotiatedBundledMids(&bundledMids);
    if (NS_FAILED(rv)) {
      MOZ_ASSERT(false);
      mLastError += " (This should have been caught sooner!)";
      return NS_ERROR_FAILURE;
    }
  }

  mSdpHelper.SetIceGatheringComplete(sdp,
                                     level,
                                     bundledMids);

  return NS_OK;
}

nsresult
JsepSessionImpl::GetNegotiatedBundledMids(SdpHelper::BundledMids* bundledMids)
{
  const Sdp* answerSdp = GetAnswer();

  if (!answerSdp) {
    return NS_OK;
  }

  return mSdpHelper.GetBundledMids(*answerSdp, bundledMids);
}

nsresult
JsepSessionImpl::EnableOfferMsection(SdpMediaSection* msection)
{
  // We assert here because adding rtcp-mux to a non-disabled m-section that
  // did not already have rtcp-mux can cause problems.
  MOZ_ASSERT(mSdpHelper.MsectionIsDisabled(*msection));

  msection->SetPort(9);

  // We don't do this in AddTransportAttributes because that is also used for
  // making answers, and we don't want to unconditionally set rtcp-mux there.
  if (mSdpHelper.HasRtcp(msection->GetProtocol())) {
    // Set RTCP-MUX.
    msection->GetAttributeList().SetAttribute(
        new SdpFlagAttribute(SdpAttribute::kRtcpMuxAttribute));
  }

  nsresult rv = AddTransportAttributes(msection, SdpSetupAttribute::kActpass);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = SetRecvonlySsrc(msection);
  NS_ENSURE_SUCCESS(rv, rv);

  AddExtmap(msection);

  std::ostringstream osMid;
  osMid << "sdparta_" << msection->GetLevel();
  AddMid(osMid.str(), msection);

  return NS_OK;
}

mozilla::Sdp*
JsepSessionImpl::GetParsedLocalDescription() const
{
  if (mPendingLocalDescription) {
    return mPendingLocalDescription.get();
  }
  return mCurrentLocalDescription.get();
}

mozilla::Sdp*
JsepSessionImpl::GetParsedRemoteDescription() const
{
  if (mPendingRemoteDescription) {
    return mPendingRemoteDescription.get();
  }
  return mCurrentRemoteDescription.get();
}

const Sdp*
JsepSessionImpl::GetAnswer() const
{
  return mWasOffererLastTime ? mCurrentRemoteDescription.get()
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
  for (const auto& localTrack : mLocalTracks) {
    if (!localTrack.mAssignedMLine.isSome()) {
      return false;
    }
  }

  return true;
}

} // namespace mozilla
