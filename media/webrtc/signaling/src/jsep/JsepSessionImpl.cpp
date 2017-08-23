/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "signaling/src/jsep/JsepSessionImpl.h"

#include <iterator>
#include <string>
#include <set>
#include <bitset>
#include <stdlib.h>

#include "nspr.h"
#include "nss.h"
#include "pk11pub.h"
#include "nsDebug.h"
#include "logging.h"

#include "mozilla/Move.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Telemetry.h"

#include "webrtc/config.h"

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

nsresult
JsepSessionImpl::AddTransceiver(RefPtr<JsepTransceiver> transceiver)
{
  mLastError.clear();
  MOZ_MTLOG(ML_DEBUG, "Adding transceiver.");

  if (transceiver->GetMediaType() != SdpMediaSection::kApplication) {
    // Make sure we have an ssrc. Might already be set.
    transceiver->mSendTrack.EnsureSsrcs(mSsrcGenerator);
    transceiver->mSendTrack.SetCNAME(mCNAME);

    // Make sure we have identifiers for send track, just in case.
    // (man I hate this)
    if (transceiver->mSendTrack.GetTrackId().empty()) {
      std::string trackId;
      if (!mUuidGen->Generate(&trackId)) {
        JSEP_SET_ERROR("Failed to generate UUID for JsepTrack");
        return NS_ERROR_FAILURE;
      }

      transceiver->mSendTrack.UpdateTrackIds(std::vector<std::string>(), trackId);
    }
  } else {
    // Datachannel transceivers should always be sendrecv. Just set it instead
    // of asserting.
    transceiver->mJsDirection = SdpDirectionAttribute::kSendrecv;
  }

  transceiver->mSendTrack.PopulateCodecs(mSupportedCodecs.values);
  transceiver->mRecvTrack.PopulateCodecs(mSupportedCodecs.values);
  // We do not set mLevel yet, we do that either on createOffer, or setRemote

  mTransceivers.push_back(transceiver);
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

  // Avoid adding duplicate entries
  for (auto ext = extensions.begin(); ext != extensions.end(); ++ext) {
    if (ext->direction == direction && ext->extensionname == extensionName) {
      return NS_OK;
    }
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

std::vector<JsepTrack>
JsepSessionImpl::GetRemoteTracksAdded() const
{
  return mRemoteTracksAdded;
}

std::vector<JsepTrack>
JsepSessionImpl::GetRemoteTracksRemoved() const
{
  return mRemoteTracksRemoved;
}

nsresult
JsepSessionImpl::CreateOfferMsection(const JsepOfferOptions& options,
                                     JsepTransceiver& transceiver,
                                     Sdp* local)
{
  JsepTrack& sendTrack(transceiver.mSendTrack);
  JsepTrack& recvTrack(transceiver.mRecvTrack);

  SdpMediaSection::Protocol protocol(
      mSdpHelper.GetProtocolForMediaType(sendTrack.GetMediaType()));

  const Sdp* answer(GetAnswer());
  const SdpMediaSection* lastAnswerMsection = nullptr;

  if (answer &&
      (local->GetMediaSectionCount() < answer->GetMediaSectionCount())) {
    lastAnswerMsection =
      &answer->GetMediaSection(local->GetMediaSectionCount());
    // Use the protocol the answer used, even if it is not what we would have
    // used.
    protocol = lastAnswerMsection->GetProtocol();
  }

  SdpMediaSection* msection = &local->AddMediaSection(
      sendTrack.GetMediaType(),
      transceiver.mJsDirection,
      0,
      protocol,
      sdp::kIPv4,
      "0.0.0.0");

  // Some of this stuff (eg; mid) sticks around even if disabled
  if (lastAnswerMsection) {
    nsresult rv = mSdpHelper.CopyStickyParams(*lastAnswerMsection, msection);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (transceiver.IsStopped()) {
    mSdpHelper.DisableMsection(local, msection);
    return NS_OK;
  }

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

  sendTrack.AddToOffer(mSsrcGenerator, msection);
  recvTrack.AddToOffer(mSsrcGenerator, msection);

  AddExtmap(msection);

  if (lastAnswerMsection && lastAnswerMsection->GetPort()) {
    MOZ_ASSERT(transceiver.IsAssociated());
    MOZ_ASSERT(transceiver.GetMid() ==
               lastAnswerMsection->GetAttributeList().GetMid());
  } else {
    std::string mid;
    // We do not set the mid on the transceiver, that happens when a description
    // is set.
    if (transceiver.IsAssociated()) {
      mid = transceiver.GetMid();
    } else {
      std::ostringstream osMid;
      osMid << "sdparta_" << msection->GetLevel();
      mid = osMid.str();
    }

    msection->GetAttributeList().SetAttribute(
        new SdpStringAttribute(SdpAttribute::kMidAttribute, mid));
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
    if ((sdp->GetMediaSection(i).GetPort() != 0) &&
        attrs.HasAttribute(SdpAttribute::kMidAttribute)) {
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

  if (!mids.empty()) {
    UniquePtr<SdpGroupAttributeList> groupAttr(new SdpGroupAttributeList);
    groupAttr->PushEntry(SdpGroupAttributeList::kBundle, mids);
    sdp->GetAttributeList().SetAttribute(groupAttr.release());
  }
}

nsresult
JsepSessionImpl::GetRemoteIds(const Sdp& sdp,
                              const SdpMediaSection& msection,
                              std::vector<std::string>* streamIds,
                              std::string* trackId)
{
  nsresult rv = mSdpHelper.GetIdsFromMsid(sdp, msection, streamIds, trackId);
  if (rv == NS_ERROR_NOT_AVAILABLE) {
    streamIds->push_back(mDefaultRemoteStreamId);

    // Generate random track ids.
    if (!mUuidGen->Generate(trackId)) {
      JSEP_SET_ERROR("Failed to generate UUID for JsepTrack");
      return NS_ERROR_FAILURE;
    }

    return NS_OK;
  }

  return rv;
}

nsresult
JsepSessionImpl::CreateOffer(const JsepOfferOptions& options,
                             std::string* offer)
{
  mLastError.clear();
  mLocalIceIsRestarting = options.mIceRestart.isSome() && *(options.mIceRestart);

  if (mState != kJsepStateStable) {
    JSEP_SET_ERROR("Cannot create offer in state " << GetStateStr(mState));
    return NS_ERROR_UNEXPECTED;
  }

  UniquePtr<Sdp> sdp;

  // Make the basic SDP that is common to offer/answer.
  nsresult rv = CreateGenericSDP(&sdp);
  NS_ENSURE_SUCCESS(rv, rv);

  for (size_t level = 0;
       JsepTransceiver* transceiver = GetTransceiverForLocal(level);
       ++level) {
    rv = CreateOfferMsection(options, *transceiver, sdp.get());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (!sdp->GetMediaSectionCount()) {
    JSEP_SET_ERROR("Cannot create offer when there are no valid transceivers.");
    return NS_ERROR_UNEXPECTED;
  }

  SetupBundle(sdp.get());

  if (mCurrentLocalDescription) {
    rv = CopyPreviousTransportParams(*GetAnswer(),
                                     *mCurrentLocalDescription,
                                     *sdp,
                                     sdp.get());
    NS_ENSURE_SUCCESS(rv,rv);
    CopyPreviousMsid(*mCurrentLocalDescription, sdp.get());
  }

  *offer = sdp->ToString();
  mGeneratedLocalDescription = Move(sdp);
  ++mSessionVersion;

  return NS_OK;
}

std::string
JsepSessionImpl::GetLocalDescription(JsepDescriptionPendingOrCurrent type) const
{
  std::ostringstream os;
  mozilla::Sdp* sdp = GetParsedLocalDescription(type);
  if (sdp) {
    sdp->Serialize(os);
  }
  return os.str();
}

std::string
JsepSessionImpl::GetRemoteDescription(JsepDescriptionPendingOrCurrent type) const
{
  std::ostringstream os;
  mozilla::Sdp* sdp =  GetParsedRemoteDescription(type);
  if (sdp) {
    sdp->Serialize(os);
  }
  return os.str();
}

void
JsepSessionImpl::AddExtmap(SdpMediaSection* msection)
{
  auto extensions = GetRtpExtensions(*msection);

  if (!extensions.empty()) {
    SdpExtmapAttributeList* extmap = new SdpExtmapAttributeList;
    extmap->mExtmaps = extensions;
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

std::vector<SdpExtmapAttributeList::Extmap>
JsepSessionImpl::GetRtpExtensions(const SdpMediaSection& msection)
{
  std::vector<SdpExtmapAttributeList::Extmap> result;
  switch (msection.GetMediaType()) {
    case SdpMediaSection::kAudio:
      result = mAudioRtpExtensions;
      break;
    case SdpMediaSection::kVideo:
      result = mVideoRtpExtensions;
      if (msection.GetAttributeList().HasAttribute(
            SdpAttribute::kRidAttribute)) {
        // We need RID support
        // TODO: Would it be worth checking that the direction is sane?
        AddRtpExtension(result,
                        webrtc::RtpExtension::kRtpStreamIdUri,
                        SdpDirectionAttribute::kSendonly);
      }
      break;
    default:
      ;
  }
  return result;
}

void
JsepSessionImpl::AddCommonExtmaps(const SdpMediaSection& remoteMsection,
                                  SdpMediaSection* msection)
{
  mSdpHelper.AddCommonExtmaps(
      remoteMsection, GetRtpExtensions(*msection), msection);
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

  UniquePtr<Sdp> sdp;

  // Make the basic SDP that is common to offer/answer.
  nsresult rv = CreateGenericSDP(&sdp);
  NS_ENSURE_SUCCESS(rv, rv);

  const Sdp& offer = *mPendingRemoteDescription;

  // Copy the bundle groups into our answer
  UniquePtr<SdpGroupAttributeList> groupAttr(new SdpGroupAttributeList);
  mSdpHelper.GetBundleGroups(offer, &groupAttr->mGroups);
  sdp->GetAttributeList().SetAttribute(groupAttr.release());

  for (size_t i = 0; i < offer.GetMediaSectionCount(); ++i) {
    // The transceivers are already in place, due to setRemote
    JsepTransceiver* transceiver(GetTransceiverForLevel(i));
    if (!transceiver) {
      JSEP_SET_ERROR("No transceiver for level " << i);
      MOZ_ASSERT(false);
      return NS_ERROR_FAILURE;
    }
    rv = CreateAnswerMsection(options,
                              *transceiver,
                              offer.GetMediaSection(i),
                              sdp.get());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (mCurrentLocalDescription) {
    // per discussion with bwc, 3rd parm here should be offer, not *sdp. (mjf)
    rv = CopyPreviousTransportParams(*GetAnswer(),
                                     *mCurrentRemoteDescription,
                                     offer,
                                     sdp.get());
    NS_ENSURE_SUCCESS(rv,rv);
    CopyPreviousMsid(*mCurrentLocalDescription, sdp.get());
  }

  *answer = sdp->ToString();
  mGeneratedLocalDescription = Move(sdp);
  ++mSessionVersion;

  return NS_OK;
}

nsresult
JsepSessionImpl::CreateAnswerMsection(const JsepAnswerOptions& options,
                                      JsepTransceiver& transceiver,
                                      const SdpMediaSection& remoteMsection,
                                      Sdp* sdp)
{
  SdpDirectionAttribute::Direction direction =
    reverse(remoteMsection.GetDirection()) & transceiver.mJsDirection;
  SdpMediaSection& msection =
      sdp->AddMediaSection(remoteMsection.GetMediaType(),
                           direction,
                           9,
                           remoteMsection.GetProtocol(),
                           sdp::kIPv4,
                           "0.0.0.0");

  nsresult rv = mSdpHelper.CopyStickyParams(remoteMsection, &msection);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mSdpHelper.MsectionIsDisabled(remoteMsection) ||
      // JS might have stopped this
      transceiver.IsStopped()) {
    mSdpHelper.DisableMsection(sdp, &msection);
    return NS_OK;
  }

  SdpSetupAttribute::Role role;
  rv = DetermineAnswererSetupRole(remoteMsection, &role);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AddTransportAttributes(&msection, role);
  NS_ENSURE_SUCCESS(rv, rv);

  transceiver.mSendTrack.AddToAnswer(remoteMsection, mSsrcGenerator, &msection);
  transceiver.mRecvTrack.AddToAnswer(remoteMsection, mSsrcGenerator, &msection);

  // Add extmap attributes. This logic will probably be moved to the track,
  // since it can be specified on a per-sender basis in JS.
  // We will need some validation to ensure that the ids are identical for
  // RTP streams that are bundled together, though (bug 1406529).
  AddCommonExtmaps(remoteMsection, &msection);

  if (msection.GetFormats().empty()) {
    // Could not negotiate anything. Disable m-section.
    mSdpHelper.DisableMsection(sdp, &msection);
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
    RollbackLocalOffer();
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

  if (type == kJsepSdpOffer) {
    // Save in case we need to rollback
    mOldTransceivers.clear();
    for (const auto& transceiver : mTransceivers) {
      mOldTransceivers.push_back(new JsepTransceiver(*transceiver));
    }
  }

  for (size_t i = 0; i < parsed->GetMediaSectionCount(); ++i) {
    JsepTransceiver* transceiver(GetTransceiverForLevel(i));
    if (!transceiver) {
      MOZ_ASSERT(false);
      JSEP_SET_ERROR("No transceiver for level " << i);
      return NS_ERROR_FAILURE;
    }
    transceiver->Associate(
        parsed->GetMediaSection(i).GetAttributeList().GetMid());
    transceiver->mTransport = new JsepTransport;
    InitTransport(parsed->GetMediaSection(i), transceiver->mTransport.get());
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
  MOZ_ASSERT(!mIsOfferer);
  mWasOffererLastTime = false;

  SetState(kJsepStateStable);
  return NS_OK;
}

nsresult
JsepSessionImpl::SetRemoteDescription(JsepSdpType type, const std::string& sdp)
{
  mLastError.clear();

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
    RollbackRemoteOffer();

    return NS_OK;
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

  // Save in case we need to rollback.
  if (type == kJsepSdpOffer) {
    mOldTransceivers.clear();
    for (const auto& transceiver : mTransceivers) {
      mOldTransceivers.push_back(new JsepTransceiver(*transceiver));
    }
  }

  // TODO(bug 1095780): Note that we create remote tracks even when
  // They contain only codecs we can't negotiate or other craziness.
  rv = UpdateTransceiversFromRemoteDescription(*parsed);
  NS_ENSURE_SUCCESS(rv, rv);

  for (size_t i = 0; i < parsed->GetMediaSectionCount(); ++i) {
    MOZ_ASSERT(GetTransceiverForLevel(i));
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

  // Now walk through the m-sections, perform negotiation, and update the
  // transceivers.
  for (size_t i = 0; i < local->GetMediaSectionCount(); ++i) {
    JsepTransceiver* transceiver(GetTransceiverForLevel(i));
    if (!transceiver) {
      MOZ_ASSERT(false);
      JSEP_SET_ERROR("No transceiver for level " << i);
      return NS_ERROR_FAILURE;
    }

    // Skip disabled m-sections.
    if (answer.GetMediaSection(i).GetPort() == 0) {
      transceiver->mTransport->Close();
      transceiver->Stop();
      transceiver->Disassociate();
      transceiver->ClearBundleLevel();
      transceiver->mSendTrack.SetActive(false);
      transceiver->mRecvTrack.SetActive(false);
      // Do not clear mLevel yet! That will happen on the next negotiation.
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
        }
      }
    }

    rv = MakeNegotiatedTransceiver(remote->GetMediaSection(i),
                                   local->GetMediaSection(i),
                                   usingBundle,
                                   transportLevel,
                                   transceiver);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  std::vector<JsepTrack*> remoteTracks;
  for (const RefPtr<JsepTransceiver>& transceiver : mTransceivers) {
    remoteTracks.push_back(&transceiver->mRecvTrack);
  }
  JsepTrack::SetUniquePayloadTypes(remoteTracks);

  mGeneratedLocalDescription.reset();

  mNegotiations++;
  return NS_OK;
}

nsresult
JsepSessionImpl::MakeNegotiatedTransceiver(const SdpMediaSection& remote,
                                           const SdpMediaSection& local,
                                           bool usingBundle,
                                           size_t transportLevel,
                                           JsepTransceiver* transceiver)
{
  const SdpMediaSection& answer = mIsOfferer ? remote : local;

  bool sending = false;
  bool receiving = false;

  // JS could stop the transceiver after the answer was created.
  if (!transceiver->IsStopped()) {
    if (mIsOfferer) {
      receiving = answer.IsSending();
      sending = answer.IsReceiving();
    } else {
      sending = answer.IsSending();
      receiving = answer.IsReceiving();
    }
  }

  MOZ_MTLOG(ML_DEBUG, "Negotiated m= line"
                          << " index=" << local.GetLevel()
                          << " type=" << local.GetMediaType()
                          << " sending=" << sending
                          << " receiving=" << receiving);

  transceiver->SetNegotiated();

  if (usingBundle) {
    transceiver->SetBundleLevel(transportLevel);
  } else {
    transceiver->ClearBundleLevel();
  }

  if (transportLevel != remote.GetLevel()) {
    JsepTransceiver* bundleTransceiver(GetTransceiverForLevel(transportLevel));
    if (!bundleTransceiver) {
      MOZ_ASSERT(false);
      JSEP_SET_ERROR("No transceiver for level " << transportLevel);
      return NS_ERROR_FAILURE;
    }
    transceiver->mTransport = bundleTransceiver->mTransport;
  } else {
    // Ensures we only finalize once, when we process the master level
    nsresult rv = FinalizeTransport(
        remote.GetAttributeList(),
        answer.GetAttributeList(),
        transceiver->mTransport);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  transceiver->mSendTrack.SetActive(sending);
  if (sending) {
    transceiver->mSendTrack.Negotiate(answer, remote);
  }

  JsepTrack& recvTrack = transceiver->mRecvTrack;
  recvTrack.SetActive(receiving);
  if (receiving) {
    recvTrack.Negotiate(answer, remote);

    if (transceiver->HasBundleLevel() &&
        recvTrack.GetSsrcs().empty() &&
        recvTrack.GetMediaType() != SdpMediaSection::kApplication) {
      // TODO(bug 1105005): Once we have urn:ietf:params:rtp-hdrext:sdes:mid
      // support, we should only fire this warning if that extension was not
      // negotiated.
      MOZ_MTLOG(ML_ERROR, "Bundled m-section has no ssrc attributes. "
                          "This may cause media packets to be dropped.");
    }
  }

  if (transceiver->mTransport->mComponents == 2) {
    // RTCP MUX or not.
    // TODO(bug 1095743): verify that the PTs are consistent with mux.
    MOZ_MTLOG(ML_DEBUG, "RTCP-MUX is off");
  }

  if (local.GetMediaType() != SdpMediaSection::kApplication) {
    Telemetry::Accumulate(Telemetry::WEBRTC_RTCP_MUX,
        transceiver->mTransport->mComponents == 1);
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
      JsepTransceiver* transceiver(GetTransceiverForLevel(i));
      if (!transceiver) {
        MOZ_ASSERT(false);
        JSEP_SET_ERROR("No transceiver for level " << i);
        return NS_ERROR_FAILURE;
      }
      size_t numComponents = transceiver->mTransport->mComponents;
      nsresult rv = mSdpHelper.CopyTransportParams(
          numComponents,
          mCurrentLocalDescription->GetMediaSection(i),
          &newLocal->GetMediaSection(i));
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

void
JsepSessionImpl::CopyPreviousMsid(const Sdp& oldLocal, Sdp* newLocal)
{
  for (size_t i = 0; i < oldLocal.GetMediaSectionCount(); ++i) {
    const SdpMediaSection& oldMsection(oldLocal.GetMediaSection(i));
    SdpMediaSection& newMsection(newLocal->GetMediaSection(i));
    if (oldMsection.GetAttributeList().HasAttribute(
          SdpAttribute::kMsidAttribute) &&
        !mSdpHelper.MsectionIsDisabled(newMsection)) {
      // JSEP says this cannot change, no matter what is happening in JS land.
      // It can only be updated if there is an intermediate SDP that clears the
      // msid.
      newMsection.GetAttributeList().SetAttribute(new SdpMsidAttributeList(
            oldMsection.GetAttributeList().GetMsid()));
    }
  }
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

    std::vector<std::string> streamIds;
    std::string trackId;
    nsresult rv = mSdpHelper.GetIdsFromMsid(*parsed,
                                            parsed->GetMediaSection(i),
                                            &streamIds,
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

  rv = HandleNegotiatedSession(mPendingLocalDescription,
                               mPendingRemoteDescription);
  NS_ENSURE_SUCCESS(rv, rv);

  mCurrentRemoteDescription = Move(mPendingRemoteDescription);
  mCurrentLocalDescription = Move(mPendingLocalDescription);
  MOZ_ASSERT(mIsOfferer);
  mWasOffererLastTime = true;

  SetState(kJsepStateStable);
  return NS_OK;
}

static bool
TrackIdCompare(const JsepTrack& t1, const JsepTrack& t2)
{
  return t1.GetTrackId() < t2.GetTrackId();
}

JsepTransceiver*
JsepSessionImpl::GetTransceiverForLevel(size_t level)
{
  for (RefPtr<JsepTransceiver>& transceiver : mTransceivers) {
    if (transceiver->HasLevel() && (transceiver->GetLevel() == level)) {
      return transceiver.get();
    }
  }

  return nullptr;
}

JsepTransceiver*
JsepSessionImpl::GetTransceiverForLocal(size_t level)
{
  if (JsepTransceiver* transceiver = GetTransceiverForLevel(level)) {
    if (WasMsectionDisabledLastNegotiation(level) && transceiver->IsStopped()) {
      // Attempt to recycle. If this fails, the old transceiver stays put.
      transceiver->Disassociate();
      JsepTransceiver* newTransceiver = FindUnassociatedTransceiver(
          transceiver->GetMediaType(), false);
      if (newTransceiver) {
        newTransceiver->SetLevel(level);
        transceiver->ClearLevel();
        return newTransceiver;
      }
    }

    return transceiver;
  }

  // There is no transceiver for |level| right now.

  for (RefPtr<JsepTransceiver>& transceiver : mTransceivers) {
    if (!transceiver->IsStopped() && !transceiver->HasLevel()) {
      transceiver->SetLevel(level);
      return transceiver.get();
    }
  }

  return nullptr;
}

JsepTransceiver*
JsepSessionImpl::GetTransceiverForRemote(const SdpMediaSection& msection)
{
  size_t level = msection.GetLevel();
  if (JsepTransceiver* transceiver = GetTransceiverForLevel(level)) {
    if (!WasMsectionDisabledLastNegotiation(level) ||
        !transceiver->IsStopped()) {
      return transceiver;
    }
    transceiver->Disassociate();
    transceiver->ClearLevel();
  }

  // No transceiver for |level|

  JsepTransceiver* transceiver = FindUnassociatedTransceiver(
      msection.GetMediaType(), true /*magic!*/);
  if (transceiver) {
    transceiver->SetLevel(level);
    return transceiver;
  }

  // Make a new transceiver
  RefPtr<JsepTransceiver> newTransceiver(
      new JsepTransceiver(msection.GetMediaType(),
                          SdpDirectionAttribute::kRecvonly));
  newTransceiver->SetLevel(level);
  newTransceiver->SetCreatedBySetRemote();
  nsresult rv = AddTransceiver(newTransceiver);
  NS_ENSURE_SUCCESS(rv, nullptr);
  return mTransceivers.back().get();
}

nsresult
JsepSessionImpl::UpdateTransceiversFromRemoteDescription(const Sdp& remote)
{
  std::vector<JsepTrack> oldRemoteTracks;
  std::vector<JsepTrack> newRemoteTracks;

  // Iterate over the sdp, updating remote tracks as we go
  for (size_t i = 0; i < remote.GetMediaSectionCount(); ++i) {
    const SdpMediaSection& msection = remote.GetMediaSection(i);

    JsepTransceiver* transceiver(GetTransceiverForRemote(msection));
    if (!transceiver) {
      return NS_ERROR_FAILURE;
    }

    bool isRtp =
      msection.GetMediaType() != SdpMediaSection::MediaType::kApplication;

    if (isRtp && transceiver->mRecvTrack.GetActive()) {
      oldRemoteTracks.push_back(transceiver->mRecvTrack);
    }

    if (!mSdpHelper.MsectionIsDisabled(msection)) {
      transceiver->Associate(msection.GetAttributeList().GetMid());
    } else {
      transceiver->Disassociate();
      // This cannot be rolled back.
      transceiver->Stop();
      continue;
    }

    if (!isRtp) {
      continue;
    }

    // Interop workaround for endpoints that don't support msid.
    // If the receiver has no ids, set some initial values, one way or another.
    if (msection.IsSending() && transceiver->mRecvTrack.GetTrackId().empty()) {
      std::vector<std::string> streamIds;
      std::string trackId;

      nsresult rv = GetRemoteIds(remote, msection, &streamIds, &trackId);
      NS_ENSURE_SUCCESS(rv, rv);
      transceiver->mRecvTrack.UpdateTrackIds(streamIds, trackId);
    }

    transceiver->mRecvTrack.UpdateRecvTrack(remote, msection);

    if (msection.IsSending()) {
      newRemoteTracks.push_back(transceiver->mRecvTrack);
    }
  }

  std::sort(oldRemoteTracks.begin(), oldRemoteTracks.end(), TrackIdCompare);
  std::sort(newRemoteTracks.begin(), newRemoteTracks.end(), TrackIdCompare);

  mRemoteTracksAdded.clear();
  mRemoteTracksRemoved.clear();

  std::set_difference(
      oldRemoteTracks.begin(),
      oldRemoteTracks.end(),
      newRemoteTracks.begin(),
      newRemoteTracks.end(),
      std::inserter(mRemoteTracksRemoved, mRemoteTracksRemoved.begin()),
      TrackIdCompare);

  std::set_difference(
      newRemoteTracks.begin(),
      newRemoteTracks.end(),
      oldRemoteTracks.begin(),
      oldRemoteTracks.end(),
      std::inserter(mRemoteTracksAdded, mRemoteTracksAdded.begin()),
      TrackIdCompare);

  return NS_OK;
}


bool
JsepSessionImpl::WasMsectionDisabledLastNegotiation(size_t level) const
{
  const Sdp* answer(GetAnswer());

  if (answer && (level < answer->GetMediaSectionCount())) {
    return mSdpHelper.MsectionIsDisabled(answer->GetMediaSection(level));
  }

  return false;
}

JsepTransceiver*
JsepSessionImpl::FindUnassociatedTransceiver(
    SdpMediaSection::MediaType type, bool magic)
{
  // Look through transceivers that are not mapped to an m-section
  for (RefPtr<JsepTransceiver>& transceiver : mTransceivers) {
    if (!transceiver->IsStopped() &&
        !transceiver->HasLevel() &&
        (!magic || transceiver->HasAddTrackMagic()) &&
        (transceiver->GetMediaType() == type)) {
      return transceiver.get();
    }
  }

  return nullptr;
}

void
JsepSessionImpl::RollbackLocalOffer()
{
  for (size_t i = 0; i < mTransceivers.size(); ++i) {
    RefPtr<JsepTransceiver>& transceiver(mTransceivers[i]);
    if (i < mOldTransceivers.size()) {
      transceiver->Rollback(*mOldTransceivers[i]);
      continue;
    }

    RefPtr<JsepTransceiver> temp(
        new JsepTransceiver(transceiver->GetMediaType()));
    transceiver->Rollback(*temp);
  }

  mOldTransceivers.clear();
}

void
JsepSessionImpl::RollbackRemoteOffer()
{
  for (size_t i = 0; i < mTransceivers.size(); ++i) {
    RefPtr<JsepTransceiver>& transceiver(mTransceivers[i]);
    if (i < mOldTransceivers.size()) {
      transceiver->Rollback(*mOldTransceivers[i]);
      continue;
    }

    // New transceiver!
    if (!transceiver->HasAddTrackMagic() &&
        transceiver->WasCreatedBySetRemote()) {
      transceiver->Stop();
      transceiver->Disassociate();
      transceiver->ClearLevel();
      transceiver->SetRemoved();
      mTransceivers.erase(mTransceivers.begin() + i);
      --i;
      continue;
    }

    // Transceiver has been "touched" by addTrack; let it live, but unhook it
    // from everything.
    RefPtr<JsepTransceiver> temp(
        new JsepTransceiver(transceiver->GetMediaType()));
    transceiver->Rollback(*temp);
  }

  mOldTransceivers.clear();
  std::swap(mRemoteTracksAdded, mRemoteTracksRemoved);
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

    // Detect bad answer ICE restart when offer doesn't request ICE restart
    if (mIsOfferer && differ && !mLocalIceIsRestarting) {
      JSEP_SET_ERROR("Remote description indicates ICE restart but offer did not "
                     "request ICE restart (new remote description changes either "
                     "the ice-ufrag or ice-pwd)");
      return NS_ERROR_INVALID_ARG;
    }

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
      // TODO Move this elsewhere to be adaptive to rate - Bug 1207925
      40000
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
      WEBRTC_DATACHANNEL_MAX_MESSAGE_SIZE_LOCAL
      ));

  // Update the redundant encodings for the RED codec with the supported
  // codecs.  Note: only uses the video codecs.
  red->UpdateRedundantEncodings(mSupportedCodecs.values);
}

void
JsepSessionImpl::SetupDefaultRtpExtensions()
{
  AddAudioRtpExtension(webrtc::RtpExtension::kAudioLevelUri,
                       SdpDirectionAttribute::Direction::kSendonly);
  AddAudioRtpExtension(webrtc::RtpExtension::kMIdUri,
                       SdpDirectionAttribute::Direction::kSendrecv);
  AddVideoRtpExtension(webrtc::RtpExtension::kAbsSendTimeUri,
                       SdpDirectionAttribute::Direction::kSendrecv);
  AddVideoRtpExtension(webrtc::RtpExtension::kTimestampOffsetUri,
                       SdpDirectionAttribute::Direction::kSendrecv);
  AddVideoRtpExtension(webrtc::RtpExtension::kMIdUri,
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

  mozilla::Sdp* sdp = GetParsedRemoteDescription(kJsepDescriptionPendingOrCurrent);

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

  mozilla::Sdp* sdp = GetParsedLocalDescription(kJsepDescriptionPendingOrCurrent);

  if (!sdp) {
    JSEP_SET_ERROR("Cannot add ICE candidate in state " << GetStateStr(mState));
    return NS_ERROR_UNEXPECTED;
  }

  if (sdp->GetMediaSectionCount() <= level) {
    // mainly here to make some testing less complicated, but also just in case
    *skipped = true;
    return NS_OK;
  }

  if (mSdpHelper.MsectionIsDisabled(sdp->GetMediaSection(level))) {
    // If m-section has port 0, don't update
    // (either it is disabled, or bundle-only)
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

  mozilla::Sdp* sdp = GetParsedLocalDescription(kJsepDescriptionPendingOrCurrent);

  if (!sdp) {
    JSEP_SET_ERROR("Cannot add ICE candidate in state " << GetStateStr(mState));
    return NS_ERROR_UNEXPECTED;
  }

  if (level >= sdp->GetMediaSectionCount()) {
    return NS_OK;
  }

  if (mSdpHelper.MsectionIsDisabled(sdp->GetMediaSection(level))) {
    // If m-section has port 0, don't update
    // (either it is disabled, or bundle-only)
    return NS_OK;
  }

  std::string defaultRtcpCandidateAddrCopy(defaultRtcpCandidateAddr);
  if (mState == kJsepStateStable) {
    JsepTransceiver* transceiver(GetTransceiverForLevel(level));
    if (!transceiver) {
      MOZ_ASSERT(false);
      JSEP_SET_ERROR("No transceiver for level " << level);
      return NS_ERROR_FAILURE;
    }

    if (transceiver->mTransport->mComponents == 1) {
      // We know we're doing rtcp-mux by now. Don't create an rtcp attr.
      defaultRtcpCandidateAddrCopy = "";
      defaultRtcpCandidatePort = 0;
    }
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

  mozilla::Sdp* sdp = GetParsedLocalDescription(kJsepDescriptionPendingOrCurrent);

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

mozilla::Sdp*
JsepSessionImpl::GetParsedLocalDescription(JsepDescriptionPendingOrCurrent type) const
{
  if (type == kJsepDescriptionPending) {
    return mPendingLocalDescription.get();
  } else if (mPendingLocalDescription &&
             type == kJsepDescriptionPendingOrCurrent) {
    return mPendingLocalDescription.get();
  }
  return mCurrentLocalDescription.get();
}

mozilla::Sdp*
JsepSessionImpl::GetParsedRemoteDescription(JsepDescriptionPendingOrCurrent type) const
{
  if (type == kJsepDescriptionPending) {
    return mPendingRemoteDescription.get();
  } else if (mPendingRemoteDescription &&
             type == kJsepDescriptionPendingOrCurrent) {
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
JsepSessionImpl::CheckNegotiationNeeded() const
{
  MOZ_ASSERT(mState == kJsepStateStable);

  for (const auto& transceiver : mTransceivers) {
    if (transceiver->IsStopped()) {
      if (transceiver->IsAssociated()) {
        MOZ_MTLOG(ML_DEBUG, "[" << mName << "]: Negotiation needed because of "
                  "stopped transceiver that still has a mid.");
        return true;
      }
      continue;
    }

    if (!transceiver->IsAssociated()) {
      MOZ_MTLOG(ML_DEBUG, "[" << mName << "]: Negotiation needed because of "
                "unassociated (but not stopped) transceiver.");
      return true;
    }

    if (!mCurrentLocalDescription || !mCurrentRemoteDescription) {
      MOZ_CRASH("Transceivers should not be associated if we're in stable "
                "before the first negotiation.");
      continue;
    }

    if (!transceiver->HasLevel()) {
      MOZ_CRASH("Associated transceivers should always have a level.");
      continue;
    }

    if (transceiver->GetMediaType() == SdpMediaSection::kApplication) {
      continue;
    }

    size_t level = transceiver->GetLevel();
    const SdpMediaSection& local =
      mCurrentLocalDescription->GetMediaSection(level);
    const SdpMediaSection& remote =
      mCurrentRemoteDescription->GetMediaSection(level);

    if (!local.GetAttributeList().HasAttribute(SdpAttribute::kMsidAttribute) &&
        (transceiver->mJsDirection & sdp::kSend)) {
      MOZ_MTLOG(ML_DEBUG, "[" << mName << "]: Negotiation needed because of "
                "lack of a=msid, and transceiver is sending.");
      return true;
    }

    if (IsOfferer()) {
      if ((local.GetDirection() != transceiver->mJsDirection) &&
          reverse(remote.GetDirection()) != transceiver->mJsDirection) {
        MOZ_MTLOG(ML_DEBUG, "[" << mName << "]: Negotiation needed because "
                  "the direction on our offer, and the remote answer, does not "
                  "match the direction on a transceiver.");
        return true;
      }
    } else if (local.GetDirection() !=
          (transceiver->mJsDirection & reverse(remote.GetDirection()))) {
      MOZ_MTLOG(ML_DEBUG, "[" << mName << "]: Negotiation needed because "
                "the direction on our answer doesn't match the direction on a "
                "transceiver, even though the remote offer would have allowed "
                "it.");
      return true;
    }
  }

  return false;
}

} // namespace mozilla
