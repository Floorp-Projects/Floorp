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
#include "mozilla/Preferences.h"
#include "mozilla/Telemetry.h"

#include "webrtc/api/rtpparameters.h"

#include "signaling/src/jsep/JsepTrack.h"
#include "signaling/src/jsep/JsepTransport.h"
#include "signaling/src/sdp/RsdparsaSdpParser.h"
#include "signaling/src/sdp/Sdp.h"
#include "signaling/src/sdp/SipccSdp.h"
#include "signaling/src/sdp/SipccSdpParser.h"
#include "mozilla/net/DataChannelProtocol.h"
#include "signaling/src/sdp/ParsingResultComparer.h"

namespace mozilla {

MOZ_MTLOG_MODULE("jsep")

#define JSEP_SET_ERROR(error)                                 \
  do {                                                        \
    std::ostringstream os;                                    \
    os << error;                                              \
    mLastError = os.str();                                    \
    MOZ_MTLOG(ML_ERROR, "[" << mName << "]: " << mLastError); \
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

static std::string GetRandomHex(size_t words) {
  std::ostringstream os;

  for (size_t i = 0; i < words; ++i) {
    uint32_t rand;
    SECStatus rv = PK11_GenerateRandom(reinterpret_cast<unsigned char*>(&rand),
                                       sizeof(rand));
    if (rv != SECSuccess) {
      MOZ_CRASH();
      return "";
    }

    os << std::hex << std::setfill('0') << std::setw(8) << rand;
  }
  return os.str();
}

nsresult JsepSessionImpl::Init() {
  mLastError.clear();

  MOZ_ASSERT(!mSessionId, "Init called more than once");

  nsresult rv = SetupIds();
  NS_ENSURE_SUCCESS(rv, rv);

  SetupDefaultCodecs();
  SetupDefaultRtpExtensions();

  mRunRustParser =
      Preferences::GetBool("media.peerconnection.sdp.rust.enabled", false);
  mRunSdpComparer =
      Preferences::GetBool("media.peerconnection.sdp.rust.compare", false);

  mEncodeTrackId =
      Preferences::GetBool("media.peerconnection.sdp.encode_track_id", true);

  mIceUfrag = GetRandomHex(1);
  mIcePwd = GetRandomHex(4);
  return NS_OK;
}

nsresult JsepSessionImpl::AddTransceiver(RefPtr<JsepTransceiver> transceiver) {
  mLastError.clear();
  MOZ_MTLOG(ML_DEBUG, "[" << mName << "]: Adding transceiver.");

  if (transceiver->GetMediaType() != SdpMediaSection::kApplication) {
    // Make sure we have an ssrc. Might already be set.
    transceiver->mSendTrack.EnsureSsrcs(mSsrcGenerator);
    transceiver->mSendTrack.SetCNAME(mCNAME);

    // Make sure we have identifiers for send track, just in case.
    // (man I hate this)
    if (mEncodeTrackId) {
      std::string trackId;
      if (!mUuidGen->Generate(&trackId)) {
        JSEP_SET_ERROR("Failed to generate UUID for JsepTrack");
        return NS_ERROR_FAILURE;
      }

      transceiver->mSendTrack.SetTrackId(trackId);
    }
  } else {
    // Datachannel transceivers should always be sendrecv. Just set it instead
    // of asserting.
    transceiver->mJsDirection = SdpDirectionAttribute::kSendrecv;
#ifdef DEBUG
    for (auto& transceiver : mTransceivers) {
      MOZ_ASSERT(transceiver->GetMediaType() != SdpMediaSection::kApplication);
    }
#endif
  }

  transceiver->mSendTrack.PopulateCodecs(mSupportedCodecs);
  transceiver->mRecvTrack.PopulateCodecs(mSupportedCodecs);
  // We do not set mLevel yet, we do that either on createOffer, or setRemote

  mTransceivers.push_back(transceiver);
  return NS_OK;
}

nsresult JsepSessionImpl::SetBundlePolicy(JsepBundlePolicy policy) {
  mLastError.clear();
  if (mCurrentLocalDescription) {
    JSEP_SET_ERROR(
        "Changing the bundle policy is only supported before the "
        "first SetLocalDescription.");
    return NS_ERROR_UNEXPECTED;
  }

  mBundlePolicy = policy;
  return NS_OK;
}

nsresult JsepSessionImpl::AddDtlsFingerprint(
    const std::string& algorithm, const std::vector<uint8_t>& value) {
  mLastError.clear();
  JsepDtlsFingerprint fp;

  fp.mAlgorithm = algorithm;
  fp.mValue = value;

  mDtlsFingerprints.push_back(fp);

  return NS_OK;
}

nsresult JsepSessionImpl::AddRtpExtension(
    JsepMediaType mediaType, const std::string& extensionName,
    SdpDirectionAttribute::Direction direction) {
  mLastError.clear();

  if (mRtpExtensions.size() + 1 > UINT16_MAX) {
    JSEP_SET_ERROR("Too many rtp extensions have been added");
    return NS_ERROR_FAILURE;
  }

  for (auto ext = mRtpExtensions.begin(); ext != mRtpExtensions.end(); ++ext) {
    if (ext->mExtmap.direction == direction &&
        ext->mExtmap.extensionname == extensionName) {
      if (ext->mMediaType != mediaType) {
        ext->mMediaType = JsepMediaType::kAudioVideo;
      }
      return NS_OK;
    }
  }

  JsepExtmapMediaType extMediaType = {
      mediaType,
      {static_cast<uint16_t>(mRtpExtensions.size() + 1), direction,
       // do we want to specify direction?
       direction != SdpDirectionAttribute::kSendrecv, extensionName, ""}};

  mRtpExtensions.push_back(extMediaType);
  return NS_OK;
}

nsresult JsepSessionImpl::AddAudioRtpExtension(
    const std::string& extensionName,
    SdpDirectionAttribute::Direction direction) {
  return AddRtpExtension(JsepMediaType::kAudio, extensionName, direction);
}

nsresult JsepSessionImpl::AddVideoRtpExtension(
    const std::string& extensionName,
    SdpDirectionAttribute::Direction direction) {
  return AddRtpExtension(JsepMediaType::kVideo, extensionName, direction);
}

nsresult JsepSessionImpl::AddAudioVideoRtpExtension(
    const std::string& extensionName,
    SdpDirectionAttribute::Direction direction) {
  return AddRtpExtension(JsepMediaType::kAudioVideo, extensionName, direction);
}

nsresult JsepSessionImpl::CreateOfferMsection(const JsepOfferOptions& options,
                                              JsepTransceiver& transceiver,
                                              Sdp* local) {
  SdpMediaSection::Protocol protocol(
      SdpHelper::GetProtocolForMediaType(transceiver.GetMediaType()));

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
      transceiver.GetMediaType(), transceiver.mJsDirection, 0, protocol,
      sdp::kIPv4, "0.0.0.0");

  // Some of this stuff (eg; mid) sticks around even if disabled
  if (lastAnswerMsection) {
    MOZ_ASSERT(lastAnswerMsection->GetMediaType() ==
               transceiver.GetMediaType());
    nsresult rv = mSdpHelper.CopyStickyParams(*lastAnswerMsection, msection);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (transceiver.IsStopped()) {
    SdpHelper::DisableMsection(local, msection);
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

  transceiver.mSendTrack.AddToOffer(mSsrcGenerator, msection);
  transceiver.mRecvTrack.AddToOffer(mSsrcGenerator, msection);

  AddExtmap(msection);

  std::string mid;
  // We do not set the mid on the transceiver, that happens when a description
  // is set.
  if (transceiver.IsAssociated()) {
    mid = transceiver.GetMid();
  } else {
    mid = GetNewMid();
  }

  msection->GetAttributeList().SetAttribute(
      new SdpStringAttribute(SdpAttribute::kMidAttribute, mid));

  return NS_OK;
}

void JsepSessionImpl::SetupBundle(Sdp* sdp) const {
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

JsepSession::Result JsepSessionImpl::CreateOffer(
    const JsepOfferOptions& options, std::string* offer) {
  mLastError.clear();

  if (mState != kJsepStateStable && mState != kJsepStateHaveLocalOffer) {
    JSEP_SET_ERROR("Cannot create offer in state " << GetStateStr(mState));
    // Spec doesn't seem to say this is an error. It probably should.
    return dom::PCError::InvalidStateError;
  }

  // This is one of those places where CreateOffer sets some state.
  SetIceRestarting(options.mIceRestart.isSome() && *(options.mIceRestart));

  UniquePtr<Sdp> sdp;

  // Make the basic SDP that is common to offer/answer.
  nsresult rv = CreateGenericSDP(&sdp);
  NS_ENSURE_SUCCESS(rv, dom::PCError::OperationError);

  for (size_t level = 0;
       JsepTransceiver* transceiver = GetTransceiverForLocal(level); ++level) {
    rv = CreateOfferMsection(options, *transceiver, sdp.get());
    NS_ENSURE_SUCCESS(rv, dom::PCError::OperationError);
  }

  SetupBundle(sdp.get());

  if (mCurrentLocalDescription) {
    rv = CopyPreviousTransportParams(*GetAnswer(), *mCurrentLocalDescription,
                                     *sdp, sdp.get());
    NS_ENSURE_SUCCESS(rv, dom::PCError::OperationError);
  }

  *offer = sdp->ToString();
  mGeneratedOffer = std::move(sdp);
  ++mSessionVersion;
  MOZ_MTLOG(ML_DEBUG, "[" << mName << "]: CreateOffer \nSDP=\n" << *offer);

  return Result();
}

std::string JsepSessionImpl::GetLocalDescription(
    JsepDescriptionPendingOrCurrent type) const {
  std::ostringstream os;
  mozilla::Sdp* sdp = GetParsedLocalDescription(type);
  if (sdp) {
    sdp->Serialize(os);
  }
  return os.str();
}

std::string JsepSessionImpl::GetRemoteDescription(
    JsepDescriptionPendingOrCurrent type) const {
  std::ostringstream os;
  mozilla::Sdp* sdp = GetParsedRemoteDescription(type);
  if (sdp) {
    sdp->Serialize(os);
  }
  return os.str();
}

void JsepSessionImpl::AddExtmap(SdpMediaSection* msection) {
  auto extensions = GetRtpExtensions(*msection);

  if (!extensions.empty()) {
    SdpExtmapAttributeList* extmap = new SdpExtmapAttributeList;
    extmap->mExtmaps = extensions;
    msection->GetAttributeList().SetAttribute(extmap);
  }
}

std::vector<SdpExtmapAttributeList::Extmap> JsepSessionImpl::GetRtpExtensions(
    const SdpMediaSection& msection) {
  std::vector<SdpExtmapAttributeList::Extmap> result;
  JsepMediaType mediaType = JsepMediaType::kNone;
  switch (msection.GetMediaType()) {
    case SdpMediaSection::kAudio:
      mediaType = JsepMediaType::kAudio;
      break;
    case SdpMediaSection::kVideo:
      mediaType = JsepMediaType::kVideo;
      if (msection.GetAttributeList().HasAttribute(
              SdpAttribute::kRidAttribute)) {
        // We need RID support
        // TODO: Would it be worth checking that the direction is sane?
        AddVideoRtpExtension(webrtc::RtpExtension::kRtpStreamIdUri,
                             SdpDirectionAttribute::kSendonly);
      }
      break;
    default:;
  }
  if (mediaType != JsepMediaType::kNone) {
    for (auto ext = mRtpExtensions.begin(); ext != mRtpExtensions.end();
         ++ext) {
      if (ext->mMediaType == mediaType ||
          ext->mMediaType == JsepMediaType::kAudioVideo) {
        result.push_back(ext->mExtmap);
      }
    }
  }
  return result;
}

std::string JsepSessionImpl::GetNewMid() {
  std::string mid;

  do {
    std::ostringstream osMid;
    osMid << mMidCounter++;
    mid = osMid.str();
  } while (mUsedMids.count(mid));

  mUsedMids.insert(mid);
  return mid;
}

void JsepSessionImpl::AddCommonExtmaps(const SdpMediaSection& remoteMsection,
                                       SdpMediaSection* msection) {
  mSdpHelper.AddCommonExtmaps(remoteMsection, GetRtpExtensions(*msection),
                              msection);
}

JsepSession::Result JsepSessionImpl::CreateAnswer(
    const JsepAnswerOptions& options, std::string* answer) {
  mLastError.clear();

  if (mState != kJsepStateHaveRemoteOffer) {
    JSEP_SET_ERROR("Cannot create answer in state " << GetStateStr(mState));
    return dom::PCError::InvalidStateError;
  }

  UniquePtr<Sdp> sdp;

  // Make the basic SDP that is common to offer/answer.
  nsresult rv = CreateGenericSDP(&sdp);
  NS_ENSURE_SUCCESS(rv, dom::PCError::OperationError);

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
      return dom::PCError::OperationError;
    }
    rv = CreateAnswerMsection(options, *transceiver, offer.GetMediaSection(i),
                              sdp.get());
    NS_ENSURE_SUCCESS(rv, dom::PCError::OperationError);
  }

  if (mCurrentLocalDescription) {
    // per discussion with bwc, 3rd parm here should be offer, not *sdp. (mjf)
    rv = CopyPreviousTransportParams(*GetAnswer(), *mCurrentRemoteDescription,
                                     offer, sdp.get());
    NS_ENSURE_SUCCESS(rv, dom::PCError::OperationError);
  }

  *answer = sdp->ToString();
  mGeneratedAnswer = std::move(sdp);
  ++mSessionVersion;
  MOZ_MTLOG(ML_DEBUG, "[" << mName << "]: CreateAnswer \nSDP=\n" << *answer);

  return Result();
}

nsresult JsepSessionImpl::CreateAnswerMsection(
    const JsepAnswerOptions& options, JsepTransceiver& transceiver,
    const SdpMediaSection& remoteMsection, Sdp* sdp) {
  MOZ_ASSERT(transceiver.GetMediaType() == remoteMsection.GetMediaType());
  SdpDirectionAttribute::Direction direction =
      reverse(remoteMsection.GetDirection()) & transceiver.mJsDirection;
  SdpMediaSection& msection =
      sdp->AddMediaSection(remoteMsection.GetMediaType(), direction, 9,
                           remoteMsection.GetProtocol(), sdp::kIPv4, "0.0.0.0");

  nsresult rv = mSdpHelper.CopyStickyParams(remoteMsection, &msection);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mSdpHelper.MsectionIsDisabled(remoteMsection) ||
      // JS might have stopped this
      transceiver.IsStopped()) {
    SdpHelper::DisableMsection(sdp, &msection);
    return NS_OK;
  }

  MOZ_ASSERT(transceiver.IsAssociated());
  if (msection.GetAttributeList().GetMid().empty()) {
    msection.GetAttributeList().SetAttribute(new SdpStringAttribute(
        SdpAttribute::kMidAttribute, transceiver.GetMid()));
  }

  MOZ_ASSERT(transceiver.GetMid() == msection.GetAttributeList().GetMid());

  SdpSetupAttribute::Role role;
  if (transceiver.mTransport.mDtls && !IsIceRestarting()) {
    role = (transceiver.mTransport.mDtls->mRole ==
            JsepDtlsTransport::kJsepDtlsClient)
               ? SdpSetupAttribute::kActive
               : SdpSetupAttribute::kPassive;
  } else {
    rv = DetermineAnswererSetupRole(remoteMsection, &role);
    NS_ENSURE_SUCCESS(rv, rv);
  }

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
    SdpHelper::DisableMsection(sdp, &msection);
  }

  return NS_OK;
}

nsresult JsepSessionImpl::DetermineAnswererSetupRole(
    const SdpMediaSection& remoteMsection, SdpSetupAttribute::Role* rolep) {
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
        JSEP_SET_ERROR(
            "The other side used an illegal setup attribute"
            " (\"holdconn\").");
        return NS_ERROR_INVALID_ARG;
    }
  }

  *rolep = role;
  return NS_OK;
}

JsepSession::Result JsepSessionImpl::SetLocalDescription(
    JsepSdpType type, const std::string& constSdp) {
  mLastError.clear();
  std::string sdp = constSdp;

  MOZ_MTLOG(ML_DEBUG, "[" << mName << "]: SetLocalDescription type=" << type
                          << "\nSDP=\n"
                          << sdp);

  switch (type) {
    case kJsepSdpOffer:
      if (!mGeneratedOffer) {
        JSEP_SET_ERROR(
            "Cannot set local offer when createOffer has not been called.");
        return dom::PCError::InvalidModificationError;
      }
      if (sdp.empty()) {
        sdp = mGeneratedOffer->ToString();
      }
      if (mState == kJsepStateHaveLocalOffer) {
        // Rollback previous offer before applying the new one.
        SetLocalDescription(kJsepSdpRollback, "");
        MOZ_ASSERT(mState == kJsepStateStable);
      }
      break;
    case kJsepSdpAnswer:
    case kJsepSdpPranswer:
      if (!mGeneratedAnswer) {
        JSEP_SET_ERROR(
            "Cannot set local answer when createAnswer has not been called.");
        return dom::PCError::InvalidModificationError;
      }
      if (sdp.empty()) {
        sdp = mGeneratedAnswer->ToString();
      }
      break;
    case kJsepSdpRollback:
      if (mState != kJsepStateHaveLocalOffer) {
        JSEP_SET_ERROR("Cannot rollback local description in "
                       << GetStateStr(mState));
        // Currently, spec allows this in any state except stable, and
        // sRD(rollback) and sLD(rollback) do exactly the same thing.
        return dom::PCError::InvalidStateError;
      }

      mPendingLocalDescription.reset();
      SetState(kJsepStateStable);
      RollbackLocalOffer();
      return Result();
  }

  switch (mState) {
    case kJsepStateStable:
      if (type != kJsepSdpOffer) {
        JSEP_SET_ERROR("Cannot set local answer in state "
                       << GetStateStr(mState));
        return dom::PCError::InvalidStateError;
      }
      mIsOfferer = true;
      break;
    case kJsepStateHaveRemoteOffer:
      if (type != kJsepSdpAnswer && type != kJsepSdpPranswer) {
        JSEP_SET_ERROR("Cannot set local offer in state "
                       << GetStateStr(mState));
        return dom::PCError::InvalidStateError;
      }
      break;
    default:
      JSEP_SET_ERROR("Cannot set local offer or answer in state "
                     << GetStateStr(mState));
      return dom::PCError::InvalidStateError;
  }

  UniquePtr<Sdp> parsed;
  nsresult rv = ParseSdp(sdp, &parsed);
  // Needs to be RTCError with sdp-syntax-error
  NS_ENSURE_SUCCESS(rv, dom::PCError::OperationError);

  // Check that content hasn't done anything unsupported with the SDP
  rv = ValidateLocalDescription(*parsed, type);
  NS_ENSURE_SUCCESS(rv, dom::PCError::InvalidModificationError);

  switch (type) {
    case kJsepSdpOffer:
      rv = ValidateOffer(*parsed);
      break;
    case kJsepSdpAnswer:
    case kJsepSdpPranswer:
      rv = ValidateAnswer(*mPendingRemoteDescription, *parsed);
      break;
    case kJsepSdpRollback:
      MOZ_CRASH();  // Handled above
  }
  NS_ENSURE_SUCCESS(rv, dom::PCError::InvalidAccessError);

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
      return dom::PCError::OperationError;
    }
    transceiver->Associate(
        parsed->GetMediaSection(i).GetAttributeList().GetMid());

    if (mSdpHelper.MsectionIsDisabled(parsed->GetMediaSection(i)) ||
        parsed->GetMediaSection(i).GetAttributeList().HasAttribute(
            SdpAttribute::kBundleOnlyAttribute)) {
      transceiver->mTransport.Close();
      continue;
    }

    EnsureHasOwnTransport(parsed->GetMediaSection(i), transceiver);
  }

  switch (type) {
    case kJsepSdpOffer:
      rv = SetLocalDescriptionOffer(std::move(parsed));
      break;
    case kJsepSdpAnswer:
    case kJsepSdpPranswer:
      rv = SetLocalDescriptionAnswer(type, std::move(parsed));
      break;
    case kJsepSdpRollback:
      MOZ_CRASH();  // Handled above
  }

  NS_ENSURE_SUCCESS(rv, dom::PCError::OperationError);
  return Result();
}

nsresult JsepSessionImpl::SetLocalDescriptionOffer(UniquePtr<Sdp> offer) {
  MOZ_ASSERT(mState == kJsepStateStable);
  mPendingLocalDescription = std::move(offer);
  SetState(kJsepStateHaveLocalOffer);
  return NS_OK;
}

nsresult JsepSessionImpl::SetLocalDescriptionAnswer(JsepSdpType type,
                                                    UniquePtr<Sdp> answer) {
  MOZ_ASSERT(mState == kJsepStateHaveRemoteOffer);
  mPendingLocalDescription = std::move(answer);

  nsresult rv = HandleNegotiatedSession(mPendingLocalDescription,
                                        mPendingRemoteDescription);
  NS_ENSURE_SUCCESS(rv, rv);

  mCurrentRemoteDescription = std::move(mPendingRemoteDescription);
  mCurrentLocalDescription = std::move(mPendingLocalDescription);
  MOZ_ASSERT(!mIsOfferer);
  mWasOffererLastTime = false;

  SetState(kJsepStateStable);
  return NS_OK;
}

JsepSession::Result JsepSessionImpl::SetRemoteDescription(
    JsepSdpType type, const std::string& sdp) {
  mLastError.clear();

  MOZ_MTLOG(ML_DEBUG, "[" << mName << "]: SetRemoteDescription type=" << type
                          << "\nSDP=\n"
                          << sdp);

  if (mState == kJsepStateHaveRemoteOffer && type == kJsepSdpOffer) {
    // Rollback previous offer before applying the new one.
    SetRemoteDescription(kJsepSdpRollback, "");
    MOZ_ASSERT(mState == kJsepStateStable);
  }

  if (type == kJsepSdpRollback) {
    if (mState != kJsepStateHaveRemoteOffer) {
      JSEP_SET_ERROR("Cannot rollback remote description in "
                     << GetStateStr(mState));
      return dom::PCError::InvalidStateError;
    }

    mPendingRemoteDescription.reset();
    SetState(kJsepStateStable);
    RollbackRemoteOffer();

    return Result();
  }

  switch (mState) {
    case kJsepStateStable:
      if (type != kJsepSdpOffer) {
        JSEP_SET_ERROR("Cannot set remote answer in state "
                       << GetStateStr(mState));
        return dom::PCError::InvalidStateError;
      }
      mIsOfferer = false;
      break;
    case kJsepStateHaveLocalOffer:
    case kJsepStateHaveRemotePranswer:
      if (type != kJsepSdpAnswer && type != kJsepSdpPranswer) {
        JSEP_SET_ERROR("Cannot set remote offer in state "
                       << GetStateStr(mState));
        return dom::PCError::InvalidStateError;
      }
      break;
    default:
      JSEP_SET_ERROR("Cannot set remote offer or answer in current state "
                     << GetStateStr(mState));
      return dom::PCError::InvalidStateError;
  }

  // Parse.
  UniquePtr<Sdp> parsed;
  nsresult rv = ParseSdp(sdp, &parsed);
  // Needs to be RTCError with sdp-syntax-error
  NS_ENSURE_SUCCESS(rv, dom::PCError::OperationError);

  rv = ValidateRemoteDescription(*parsed);
  NS_ENSURE_SUCCESS(rv, dom::PCError::InvalidAccessError);

  switch (type) {
    case kJsepSdpOffer:
      rv = ValidateOffer(*parsed);
      break;
    case kJsepSdpAnswer:
    case kJsepSdpPranswer:
      rv = ValidateAnswer(*mPendingLocalDescription, *parsed);
      break;
    case kJsepSdpRollback:
      MOZ_CRASH();  // Handled above
  }
  NS_ENSURE_SUCCESS(rv, dom::PCError::InvalidAccessError);

  bool iceLite =
      parsed->GetAttributeList().HasAttribute(SdpAttribute::kIceLiteAttribute);

  // check for mismatch ufrag/pwd indicating ice restart
  // can't just check the first one because it might be disabled
  bool iceRestarting = false;
  if (mCurrentRemoteDescription.get()) {
    for (size_t i = 0; !iceRestarting &&
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
      if (!transceiver->IsNegotiated()) {
        // We chose a level for this transceiver, but never negotiated it.
        // Discard this state.
        transceiver->ClearLevel();
      }
    }
  }

  // TODO(bug 1095780): Note that we create remote tracks even when
  // They contain only codecs we can't negotiate or other craziness.
  rv = UpdateTransceiversFromRemoteDescription(*parsed);
  NS_ENSURE_SUCCESS(rv, dom::PCError::OperationError);

  for (size_t i = 0; i < parsed->GetMediaSectionCount(); ++i) {
    MOZ_ASSERT(GetTransceiverForLevel(i));
  }

  switch (type) {
    case kJsepSdpOffer:
      rv = SetRemoteDescriptionOffer(std::move(parsed));
      break;
    case kJsepSdpAnswer:
    case kJsepSdpPranswer:
      rv = SetRemoteDescriptionAnswer(type, std::move(parsed));
      break;
    case kJsepSdpRollback:
      MOZ_CRASH();  // Handled above
  }

  NS_ENSURE_SUCCESS(rv, dom::PCError::OperationError);

  mRemoteIsIceLite = iceLite;
  mIceOptions = iceOptions;
  SetIceRestarting(iceRestarting);
  return Result();
}

nsresult JsepSessionImpl::HandleNegotiatedSession(
    const UniquePtr<Sdp>& local, const UniquePtr<Sdp>& remote) {
  // local ufrag/pwd has been negotiated; we will never go back to the old ones
  mOldIceUfrag.clear();
  mOldIcePwd.clear();

  bool remoteIceLite =
      remote->GetAttributeList().HasAttribute(SdpAttribute::kIceLiteAttribute);

  mIceControlling = remoteIceLite || mIsOfferer;

  const Sdp& answer = mIsOfferer ? *remote : *local;

  SdpHelper::BundledMids bundledMids;
  nsresult rv = mSdpHelper.GetBundledMids(answer, &bundledMids);
  NS_ENSURE_SUCCESS(rv, rv);

  // First, set the bundle level on the transceivers
  for (auto& midAndMaster : bundledMids) {
    JsepTransceiver* bundledTransceiver =
        GetTransceiverForMid(midAndMaster.first);
    if (!bundledTransceiver) {
      JSEP_SET_ERROR("No transceiver for bundled mid " << midAndMaster.first);
      return NS_ERROR_INVALID_ARG;
    }
    bundledTransceiver->SetBundleLevel(midAndMaster.second->GetLevel());
  }

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
      transceiver->mTransport.Close();
      transceiver->Stop();
      transceiver->Disassociate();
      transceiver->ClearBundleLevel();
      transceiver->mSendTrack.SetActive(false);
      transceiver->mRecvTrack.SetActive(false);
      transceiver->SetCanRecycle();
      // Do not clear mLevel yet! That will happen on the next negotiation.
      continue;
    }

    rv = MakeNegotiatedTransceiver(remote->GetMediaSection(i),
                                   local->GetMediaSection(i), transceiver);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  std::vector<JsepTrack*> remoteTracks;
  for (const RefPtr<JsepTransceiver>& transceiver : mTransceivers) {
    remoteTracks.push_back(&transceiver->mRecvTrack);
  }
  JsepTrack::SetUniquePayloadTypes(remoteTracks);

  mNegotiations++;

  mGeneratedAnswer.reset();
  mGeneratedOffer.reset();

  return NS_OK;
}

nsresult JsepSessionImpl::MakeNegotiatedTransceiver(
    const SdpMediaSection& remote, const SdpMediaSection& local,
    JsepTransceiver* transceiver) {
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

  MOZ_MTLOG(ML_DEBUG, "[" << mName << "]: Negotiated m= line"
                          << " index=" << local.GetLevel() << " type="
                          << local.GetMediaType() << " sending=" << sending
                          << " receiving=" << receiving);

  transceiver->SetNegotiated();

  JsepTransceiver* transportTransceiver = transceiver;
  if (transceiver->HasBundleLevel()) {
    size_t transportLevel = transceiver->BundleLevel();
    transportTransceiver = GetTransceiverForLevel(transportLevel);
    if (!transportTransceiver) {
      MOZ_ASSERT(false);
      JSEP_SET_ERROR("No transceiver for level " << transportLevel);
      return NS_ERROR_FAILURE;
    }
  }

  // Ensure that this is finalized in case we need to copy it below
  nsresult rv =
      FinalizeTransport(remote.GetAttributeList(), answer.GetAttributeList(),
                        &transceiver->mTransport);
  NS_ENSURE_SUCCESS(rv, rv);

  if (transportTransceiver != transceiver) {
    transceiver->mTransport = transportTransceiver->mTransport;
  }

  transceiver->mSendTrack.SetActive(sending);
  transceiver->mSendTrack.Negotiate(answer, remote);

  JsepTrack& recvTrack = transceiver->mRecvTrack;
  recvTrack.SetActive(receiving);
  recvTrack.Negotiate(answer, remote);

  if (transceiver->HasBundleLevel() && recvTrack.GetSsrcs().empty() &&
      recvTrack.GetMediaType() != SdpMediaSection::kApplication) {
    // TODO(bug 1105005): Once we have urn:ietf:params:rtp-hdrext:sdes:mid
    // support, we should only fire this warning if that extension was not
    // negotiated.
    MOZ_MTLOG(ML_ERROR, "[" << mName
                            << "]: Bundled m-section has no ssrc "
                               "attributes. This may cause media packets to be "
                               "dropped.");
  }

  if (transceiver->mTransport.mComponents == 2) {
    // RTCP MUX or not.
    // TODO(bug 1095743): verify that the PTs are consistent with mux.
    MOZ_MTLOG(ML_DEBUG, "[" << mName << "]: RTCP-MUX is off");
  }

  if (local.GetMediaType() != SdpMediaSection::kApplication) {
    Telemetry::Accumulate(Telemetry::WEBRTC_RTCP_MUX,
                          transceiver->mTransport.mComponents == 1);
  }

  return NS_OK;
}

void JsepSessionImpl::EnsureHasOwnTransport(const SdpMediaSection& msection,
                                            JsepTransceiver* transceiver) {
  JsepTransport& transport = transceiver->mTransport;

  if (!transceiver->HasOwnTransport()) {
    // Transceiver didn't own this transport last time, it won't now either
    transport.Close();
  }

  transport.mLocalUfrag = msection.GetAttributeList().GetIceUfrag();
  transport.mLocalPwd = msection.GetAttributeList().GetIcePwd();

  transceiver->ClearBundleLevel();

  if (!transport.mComponents) {
    if (mSdpHelper.HasRtcp(msection.GetProtocol())) {
      transport.mComponents = 2;
    } else {
      transport.mComponents = 1;
    }
  }

  if (transport.mTransportId.empty()) {
    // TODO: Once we use different ICE ufrag/pass for each m-section, we can
    // use that here.
    std::ostringstream os;
    os << "transport_" << mTransportIdCounter++;
    transport.mTransportId = os.str();
  }
}

nsresult JsepSessionImpl::FinalizeTransport(const SdpAttributeList& remote,
                                            const SdpAttributeList& answer,
                                            JsepTransport* transport) {
  if (!transport->mComponents) {
    return NS_OK;
  }

  if (!transport->mIce || transport->mIce->mUfrag != remote.GetIceUfrag() ||
      transport->mIce->mPwd != remote.GetIcePwd()) {
    UniquePtr<JsepIceTransport> ice = MakeUnique<JsepIceTransport>();
    transport->mDtls = nullptr;

    // We do sanity-checking for these in ParseSdp
    ice->mUfrag = remote.GetIceUfrag();
    ice->mPwd = remote.GetIcePwd();
    if (remote.HasAttribute(SdpAttribute::kCandidateAttribute)) {
      ice->mCandidates = remote.GetCandidate();
    }

    transport->mIce = std::move(ice);
  }

  if (!transport->mDtls) {
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

    transport->mDtls = std::move(dtls);
  }

  if (answer.HasAttribute(SdpAttribute::kRtcpMuxAttribute)) {
    transport->mComponents = 1;
  }

  return NS_OK;
}

nsresult JsepSessionImpl::AddTransportAttributes(
    SdpMediaSection* msection, SdpSetupAttribute::Role dtlsRole) {
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

nsresult JsepSessionImpl::CopyPreviousTransportParams(
    const Sdp& oldAnswer, const Sdp& offerersPreviousSdp, const Sdp& newOffer,
    Sdp* newLocal) {
  for (size_t i = 0; i < oldAnswer.GetMediaSectionCount(); ++i) {
    if (!mSdpHelper.MsectionIsDisabled(newLocal->GetMediaSection(i)) &&
        mSdpHelper.AreOldTransportParamsValid(oldAnswer, offerersPreviousSdp,
                                              newOffer, i)) {
      // If newLocal is an offer, this will be the number of components we used
      // last time, and if it is an answer, this will be the number of
      // components we've decided we're using now.
      JsepTransceiver* transceiver(GetTransceiverForLevel(i));
      if (!transceiver) {
        MOZ_ASSERT(false);
        JSEP_SET_ERROR("No transceiver for level " << i);
        return NS_ERROR_FAILURE;
      }
      size_t numComponents = transceiver->mTransport.mComponents;
      nsresult rv = mSdpHelper.CopyTransportParams(
          numComponents, mCurrentLocalDescription->GetMediaSection(i),
          &newLocal->GetMediaSection(i));
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

nsresult JsepSessionImpl::ParseSdp(const std::string& sdp,
                                   UniquePtr<Sdp>* parsedp) {
  UniquePtr<Sdp> parsed = mSipccParser.Parse(sdp);
  if (!parsed) {
    std::string error = "Failed to parse SDP: ";
    mSdpHelper.appendSdpParseErrors(mSipccParser.GetParseErrors(), &error);
    JSEP_SET_ERROR(error);
    return NS_ERROR_INVALID_ARG;
  }

  if (mRunRustParser) {
    UniquePtr<Sdp> rustParsed = mRsdparsaParser.Parse(sdp);
    if (mRunSdpComparer) {
      ParsingResultComparer comparer;
      if (rustParsed) {
        comparer.Compare(*rustParsed, *parsed, sdp);
      } else {
        comparer.TrackRustParsingFailed(mSipccParser.GetParseErrors().size());
      }
    }
  }

  // Verify that the JSEP rules for all SDP are followed
  for (size_t i = 0; i < parsed->GetMediaSectionCount(); ++i) {
    if (mSdpHelper.MsectionIsDisabled(parsed->GetMediaSection(i))) {
      // Disabled, let this stuff slide.
      continue;
    }

    const SdpMediaSection& msection(parsed->GetMediaSection(i));
    auto& mediaAttrs = msection.GetAttributeList();

    if (mediaAttrs.HasAttribute(SdpAttribute::kMidAttribute) &&
        mediaAttrs.GetMid().length() > 16) {
      JSEP_SET_ERROR(
          "Invalid description, mid length greater than 16 "
          "unsupported until 2-byte rtp header extensions are "
          "supported in webrtc.org");
      return NS_ERROR_INVALID_ARG;
    }

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
      JSEP_SET_ERROR(
          "Invalid description, no supported fingerprint algorithms "
          "present");
      return NS_ERROR_INVALID_ARG;
    }

    if (mediaAttrs.HasAttribute(SdpAttribute::kSetupAttribute, true) &&
        mediaAttrs.GetSetup().mRole == SdpSetupAttribute::kHoldconn) {
      JSEP_SET_ERROR(
          "Description has illegal setup attribute "
          "\"holdconn\" in m-section at level "
          << i);
      return NS_ERROR_INVALID_ARG;
    }

    static const std::bitset<128> forbidden = GetForbiddenSdpPayloadTypes();
    if (msection.GetMediaType() == SdpMediaSection::kAudio ||
        msection.GetMediaType() == SdpMediaSection::kVideo) {
      // Sanity-check that payload type can work with RTP
      for (const std::string& fmt : msection.GetFormats()) {
        uint16_t payloadType;
        if (!SdpHelper::GetPtAsInt(fmt, &payloadType)) {
          JSEP_SET_ERROR("Payload type \""
                         << fmt << "\" is not a 16-bit unsigned int at level "
                         << i);
          return NS_ERROR_INVALID_ARG;
        }
        if (payloadType > 127) {
          JSEP_SET_ERROR("audio/video payload type \""
                         << fmt << "\" is too large at level " << i);
          return NS_ERROR_INVALID_ARG;
        }
        if (forbidden.test(payloadType)) {
          JSEP_SET_ERROR("Illegal audio/video payload type \""
                         << fmt << "\" at level " << i);
          return NS_ERROR_INVALID_ARG;
        }
      }
    }
  }

  *parsedp = std::move(parsed);
  return NS_OK;
}

nsresult JsepSessionImpl::SetRemoteDescriptionOffer(UniquePtr<Sdp> offer) {
  MOZ_ASSERT(mState == kJsepStateStable);

  mPendingRemoteDescription = std::move(offer);

  SetState(kJsepStateHaveRemoteOffer);
  return NS_OK;
}

nsresult JsepSessionImpl::SetRemoteDescriptionAnswer(JsepSdpType type,
                                                     UniquePtr<Sdp> answer) {
  MOZ_ASSERT(mState == kJsepStateHaveLocalOffer ||
             mState == kJsepStateHaveRemotePranswer);

  mPendingRemoteDescription = std::move(answer);

  nsresult rv = HandleNegotiatedSession(mPendingLocalDescription,
                                        mPendingRemoteDescription);
  NS_ENSURE_SUCCESS(rv, rv);

  mCurrentRemoteDescription = std::move(mPendingRemoteDescription);
  mCurrentLocalDescription = std::move(mPendingLocalDescription);
  MOZ_ASSERT(mIsOfferer);
  mWasOffererLastTime = true;

  SetState(kJsepStateStable);
  return NS_OK;
}

JsepTransceiver* JsepSessionImpl::GetTransceiverForLevel(size_t level) {
  for (RefPtr<JsepTransceiver>& transceiver : mTransceivers) {
    if (transceiver->HasLevel() && (transceiver->GetLevel() == level)) {
      return transceiver.get();
    }
  }

  return nullptr;
}

JsepTransceiver* JsepSessionImpl::GetTransceiverForMid(const std::string& mid) {
  for (RefPtr<JsepTransceiver>& transceiver : mTransceivers) {
    if (transceiver->IsAssociated() && (transceiver->GetMid() == mid)) {
      return transceiver.get();
    }
  }

  return nullptr;
}

JsepTransceiver* JsepSessionImpl::GetTransceiverForLocal(size_t level) {
  if (JsepTransceiver* transceiver = GetTransceiverForLevel(level)) {
    if (transceiver->CanRecycle() &&
        transceiver->GetMediaType() != SdpMediaSection::kApplication) {
      // Attempt to recycle. If this fails, the old transceiver stays put.
      transceiver->Disassociate();
      JsepTransceiver* newTransceiver =
          FindUnassociatedTransceiver(transceiver->GetMediaType(), false);
      if (newTransceiver) {
        newTransceiver->SetLevel(level);
        transceiver->ClearLevel();
        return newTransceiver;
      }
    }

    return transceiver;
  }

  // There is no transceiver for |level| right now.

  // Look for an RTP transceiver
  for (RefPtr<JsepTransceiver>& transceiver : mTransceivers) {
    if (transceiver->GetMediaType() != SdpMediaSection::kApplication &&
        !transceiver->IsStopped() && !transceiver->HasLevel()) {
      transceiver->SetLevel(level);
      return transceiver.get();
    }
  }

  // Ok, look for a datachannel
  for (RefPtr<JsepTransceiver>& transceiver : mTransceivers) {
    if (!transceiver->IsStopped() && !transceiver->HasLevel()) {
      transceiver->SetLevel(level);
      return transceiver.get();
    }
  }

  return nullptr;
}

JsepTransceiver* JsepSessionImpl::GetTransceiverForRemote(
    const SdpMediaSection& msection) {
  size_t level = msection.GetLevel();
  if (JsepTransceiver* transceiver = GetTransceiverForLevel(level)) {
    if (!transceiver->CanRecycle()) {
      return transceiver;
    }
    transceiver->Disassociate();
    transceiver->ClearLevel();
  }

  // No transceiver for |level|

  JsepTransceiver* transceiver =
      FindUnassociatedTransceiver(msection.GetMediaType(), true /*magic!*/);
  if (transceiver) {
    transceiver->SetLevel(level);
    return transceiver;
  }

  // Make a new transceiver
  RefPtr<JsepTransceiver> newTransceiver(new JsepTransceiver(
      msection.GetMediaType(), SdpDirectionAttribute::kRecvonly));
  newTransceiver->SetLevel(level);
  newTransceiver->SetCreatedBySetRemote();
  nsresult rv = AddTransceiver(newTransceiver);
  NS_ENSURE_SUCCESS(rv, nullptr);
  return mTransceivers.back().get();
}

JsepTransceiver* JsepSessionImpl::GetTransceiverWithTransport(
    const std::string& transportId) {
  for (const auto& transceiver : mTransceivers) {
    if (transceiver->HasOwnTransport() &&
        (transceiver->mTransport.mTransportId == transportId)) {
      MOZ_ASSERT(transceiver->HasLevel(),
                 "Transceiver has a transport, but no level!");
      return transceiver.get();
    }
  }

  return nullptr;
}

nsresult JsepSessionImpl::UpdateTransceiversFromRemoteDescription(
    const Sdp& remote) {
  // Iterate over the sdp, updating remote tracks as we go
  for (size_t i = 0; i < remote.GetMediaSectionCount(); ++i) {
    const SdpMediaSection& msection = remote.GetMediaSection(i);

    JsepTransceiver* transceiver(GetTransceiverForRemote(msection));
    if (!transceiver) {
      return NS_ERROR_FAILURE;
    }

    if (!mSdpHelper.MsectionIsDisabled(msection)) {
      transceiver->Associate(msection.GetAttributeList().GetMid());
      if (!transceiver->IsAssociated()) {
        transceiver->Associate(GetNewMid());
      } else {
        mUsedMids.insert(transceiver->GetMid());
      }
    } else {
      transceiver->mTransport.Close();
      transceiver->Disassociate();
      // This cannot be rolled back.
      transceiver->Stop();
      continue;
    }

    if (msection.GetMediaType() == SdpMediaSection::MediaType::kApplication) {
      continue;
    }

    // Interop workaround for endpoints that don't support msid.
    // Ensures that there is a default stream id set, provided the remote is
    // sending.
    // TODO(bug 1426005): Remove this, or at least move it to JsepTrack.
    transceiver->mRecvTrack.UpdateStreamIds({mDefaultRemoteStreamId});

    // This will process a=msid if present, or clear the stream ids if the
    // msection is not sending. If the msection is sending, and there are no
    // a=msid, the previously set default will stay.
    transceiver->mRecvTrack.UpdateRecvTrack(remote, msection);
  }

  return NS_OK;
}

JsepTransceiver* JsepSessionImpl::FindUnassociatedTransceiver(
    SdpMediaSection::MediaType type, bool magic) {
  // Look through transceivers that are not mapped to an m-section
  for (RefPtr<JsepTransceiver>& transceiver : mTransceivers) {
    if (type == SdpMediaSection::kApplication &&
        type == transceiver->GetMediaType()) {
      transceiver->RestartDatachannelTransceiver();
      return transceiver.get();
    }
    if (!transceiver->IsStopped() && !transceiver->HasLevel() &&
        (!magic || transceiver->HasAddTrackMagic()) &&
        (transceiver->GetMediaType() == type)) {
      return transceiver.get();
    }
  }

  return nullptr;
}

void JsepSessionImpl::RollbackLocalOffer() {
  for (size_t i = 0; i < mTransceivers.size(); ++i) {
    RefPtr<JsepTransceiver>& transceiver(mTransceivers[i]);
    if (i < mOldTransceivers.size()) {
      transceiver->Rollback(*mOldTransceivers[i], false);
      continue;
    }

    RefPtr<JsepTransceiver> temp(
        new JsepTransceiver(transceiver->GetMediaType()));
    temp->mSendTrack.PopulateCodecs(mSupportedCodecs);
    temp->mRecvTrack.PopulateCodecs(mSupportedCodecs);
    transceiver->Rollback(*temp, false);
  }

  mOldTransceivers.clear();
}

void JsepSessionImpl::RollbackRemoteOffer() {
  for (size_t i = 0; i < mTransceivers.size(); ++i) {
    RefPtr<JsepTransceiver>& transceiver(mTransceivers[i]);
    if (i < mOldTransceivers.size()) {
      transceiver->Rollback(*mOldTransceivers[i], true);
      continue;
    }

    // New transceiver!
    bool shouldRemove = !transceiver->HasAddTrackMagic() &&
                        transceiver->WasCreatedBySetRemote();

    // We rollback even for transceivers we will remove, just to ensure we end
    // up at the starting state.
    RefPtr<JsepTransceiver> temp(
        new JsepTransceiver(transceiver->GetMediaType()));
    temp->mSendTrack.PopulateCodecs(mSupportedCodecs);
    temp->mRecvTrack.PopulateCodecs(mSupportedCodecs);
    transceiver->Rollback(*temp, true);

    if (shouldRemove) {
      transceiver->Stop();
      transceiver->SetRemoved();
      mTransceivers.erase(mTransceivers.begin() + i);
      --i;
    }
  }

  mOldTransceivers.clear();
}

nsresult JsepSessionImpl::ValidateLocalDescription(const Sdp& description,
                                                   JsepSdpType type) {
  Sdp* generated = nullptr;
  // TODO(bug 1095226): Better checking.
  if (type == kJsepSdpOffer) {
    generated = mGeneratedOffer.get();
  } else {
    generated = mGeneratedAnswer.get();
  }

  if (!generated) {
    JSEP_SET_ERROR(
        "Calling SetLocal without first calling CreateOffer/Answer"
        " is not supported.");
    return NS_ERROR_UNEXPECTED;
  }

  if (description.GetMediaSectionCount() != generated->GetMediaSectionCount()) {
    JSEP_SET_ERROR("Changing the number of m-sections is not allowed");
    return NS_ERROR_INVALID_ARG;
  }

  for (size_t i = 0; i < description.GetMediaSectionCount(); ++i) {
    auto& origMsection = generated->GetMediaSection(i);
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

    if (mSdpHelper.MsectionIsDisabled(finalMsection)) {
      continue;
    }

    if (!finalMsection.GetAttributeList().HasAttribute(
            SdpAttribute::kMidAttribute)) {
      JSEP_SET_ERROR("Local descriptions must have a=mid attributes.");
      return NS_ERROR_INVALID_ARG;
    }

    if (finalMsection.GetAttributeList().GetMid() !=
        origMsection.GetAttributeList().GetMid()) {
      JSEP_SET_ERROR("Changing the mid of m-sections is not allowed.");
      return NS_ERROR_INVALID_ARG;
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

nsresult JsepSessionImpl::ValidateRemoteDescription(const Sdp& description) {
  if (!mCurrentRemoteDescription || !mCurrentLocalDescription) {
    // Not renegotiation; checks for whether a remote answer are consistent
    // with our offer are handled in ValidateAnswer()
    return NS_OK;
  }

  if (mCurrentRemoteDescription->GetMediaSectionCount() >
      description.GetMediaSectionCount()) {
    JSEP_SET_ERROR(
        "New remote description has fewer m-sections than the "
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
  for (size_t i = 0; i < mCurrentRemoteDescription->GetMediaSectionCount();
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

    if (mIsOfferer && differ && !IsIceRestarting()) {
      JSEP_SET_ERROR(
          "Remote description indicates ICE restart but offer did not "
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
      JSEP_SET_ERROR(
          "Partial ICE restart is unsupported at this time "
          "(new remote description changes either the ice-ufrag "
          "or ice-pwd on fewer than all msections)");
      return NS_ERROR_INVALID_ARG;
    }
  }

  return NS_OK;
}

nsresult JsepSessionImpl::ValidateOffer(const Sdp& offer) {
  for (size_t i = 0; i < offer.GetMediaSectionCount(); ++i) {
    const SdpMediaSection& offerMsection = offer.GetMediaSection(i);
    if (mSdpHelper.MsectionIsDisabled(offerMsection)) {
      continue;
    }

    const SdpAttributeList& offerAttrs(offerMsection.GetAttributeList());
    if (!offerAttrs.HasAttribute(SdpAttribute::kSetupAttribute, true)) {
      JSEP_SET_ERROR(
          "Offer is missing required setup attribute "
          " at level "
          << i);
      return NS_ERROR_INVALID_ARG;
    }
  }

  return NS_OK;
}

nsresult JsepSessionImpl::ValidateAnswer(const Sdp& offer, const Sdp& answer) {
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
      JSEP_SET_ERROR("Answer and offer have different media types at m-line "
                     << i);
      return NS_ERROR_INVALID_ARG;
    }

    if (mSdpHelper.MsectionIsDisabled(answerMsection)) {
      continue;
    }

    if (mSdpHelper.MsectionIsDisabled(offerMsection)) {
      JSEP_SET_ERROR(
          "Answer tried to enable an m-section that was disabled in the offer");
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
      JSEP_SET_ERROR(
          "Answer contains illegal setup attribute \"actpass\""
          " at level "
          << i);
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
            if ((ansExt.direction & reverse(offExt.direction)) !=
                ansExt.direction) {
              // FIXME we do not return an error here, because Chrome up to
              // version 57 is actually tripping over this if they are the
              // answerer. See bug 1355010 for details.
              MOZ_MTLOG(ML_WARNING,
                        "[" << mName
                            << "]: Answer has inconsistent"
                               " direction on extmap attribute at level "
                            << i << " (" << ansExt.extensionname
                            << "). Offer had " << offExt.direction
                            << ", answer had " << ansExt.direction << ".");
              // return NS_ERROR_INVALID_ARG;
            }

            if (offExt.entry < 4096 && (offExt.entry != ansExt.entry)) {
              JSEP_SET_ERROR("Answer changed id for extmap attribute at level "
                             << i << " (" << offExt.extensionname << ") from "
                             << offExt.entry << " to " << ansExt.entry << ".");
              return NS_ERROR_INVALID_ARG;
            }

            if (ansExt.entry >= 4096) {
              JSEP_SET_ERROR("Answer used an invalid id ("
                             << ansExt.entry
                             << ") for extmap attribute at level " << i << " ("
                             << ansExt.extensionname << ").");
              return NS_ERROR_INVALID_ARG;
            }

            found = true;
            break;
          }
        }

        if (!found) {
          JSEP_SET_ERROR("Answer has extmap "
                         << ansExt.extensionname
                         << " at "
                            "level "
                         << i << " that was not present in offer.");
          return NS_ERROR_INVALID_ARG;
        }
      }
    }
  }

  return NS_OK;
}

nsresult JsepSessionImpl::CreateGenericSDP(UniquePtr<Sdp>* sdpp) {
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

  auto origin = SdpOrigin("mozilla...THIS_IS_SDPARTA-" MOZ_APP_UA_VERSION,
                          mSessionId, mSessionVersion, sdp::kIPv4, "0.0.0.0");

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

  *sdpp = std::move(sdp);
  return NS_OK;
}

nsresult JsepSessionImpl::SetupIds() {
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

void JsepSessionImpl::SetupDefaultCodecs() {
  // Supported audio codecs.
  // Per jmspeex on IRC:
  // For 32KHz sampling, 28 is ok, 32 is good, 40 should be really good
  // quality.  Note that 1-2Kbps will be wasted on a stereo Opus channel
  // with mono input compared to configuring it for mono.
  // If we reduce bitrate enough Opus will low-pass us; 16000 will kill a
  // 9KHz tone.  This should be adaptive when we're at the low-end of video
  // bandwidth (say <100Kbps), and if we're audio-only, down to 8 or
  // 12Kbps.
  mSupportedCodecs.emplace_back(
      new JsepAudioCodecDescription("109", "opus", 48000, 2));

  mSupportedCodecs.emplace_back(
      new JsepAudioCodecDescription("9", "G722", 8000, 1));

  mSupportedCodecs.emplace_back(
      new JsepAudioCodecDescription("0", "PCMU", 8000, 1));

  mSupportedCodecs.emplace_back(
      new JsepAudioCodecDescription("8", "PCMA", 8000, 1));

  mSupportedCodecs.emplace_back(
      new JsepAudioCodecDescription("101", "telephone-event", 8000, 1));

  // Supported video codecs.
  // Note: order here implies priority for building offers!
  UniquePtr<JsepVideoCodecDescription> vp8(
      new JsepVideoCodecDescription("120", "VP8", 90000));
  // Defaults for mandatory params
  vp8->mConstraints.maxFs = 12288;  // Enough for 2048x1536
  vp8->mConstraints.maxFps = 60;
  mSupportedCodecs.push_back(std::move(vp8));

  UniquePtr<JsepVideoCodecDescription> vp9(
      new JsepVideoCodecDescription("121", "VP9", 90000));
  // Defaults for mandatory params
  vp9->mConstraints.maxFs = 12288;  // Enough for 2048x1536
  vp9->mConstraints.maxFps = 60;
  mSupportedCodecs.push_back(std::move(vp9));

  UniquePtr<JsepVideoCodecDescription> h264_1(
      new JsepVideoCodecDescription("126", "H264", 90000));
  h264_1->mPacketizationMode = 1;
  // Defaults for mandatory params
  h264_1->mProfileLevelId = 0x42E00D;
  mSupportedCodecs.push_back(std::move(h264_1));

  UniquePtr<JsepVideoCodecDescription> h264_0(
      new JsepVideoCodecDescription("97", "H264", 90000));
  h264_0->mPacketizationMode = 0;
  // Defaults for mandatory params
  h264_0->mProfileLevelId = 0x42E00D;
  mSupportedCodecs.push_back(std::move(h264_0));

  UniquePtr<JsepVideoCodecDescription> ulpfec(new JsepVideoCodecDescription(
      "123",     // payload type
      "ulpfec",  // codec name
      90000      // clock rate (match other video codecs)
      ));
  mSupportedCodecs.push_back(std::move(ulpfec));

  mSupportedCodecs.emplace_back(new JsepApplicationCodecDescription(
      "webrtc-datachannel", WEBRTC_DATACHANNEL_STREAMS_DEFAULT,
      WEBRTC_DATACHANNEL_PORT_DEFAULT,
      WEBRTC_DATACHANNEL_MAX_MESSAGE_SIZE_LOCAL));

  UniquePtr<JsepVideoCodecDescription> red(new JsepVideoCodecDescription(
      "122",  // payload type
      "red",  // codec name
      90000   // clock rate (match other video codecs)
      ));
  // Update the redundant encodings for the RED codec with the supported
  // codecs.  Note: only uses the video codecs.
  red->UpdateRedundantEncodings(mSupportedCodecs);
  mSupportedCodecs.push_back(std::move(red));
}

void JsepSessionImpl::SetupDefaultRtpExtensions() {
  AddAudioRtpExtension(webrtc::RtpExtension::kAudioLevelUri,
                       SdpDirectionAttribute::Direction::kSendrecv);
  AddAudioRtpExtension(webrtc::RtpExtension::kCsrcAudioLevelUri,
                       SdpDirectionAttribute::Direction::kRecvonly);
  AddAudioVideoRtpExtension(webrtc::RtpExtension::kMIdUri,
                            SdpDirectionAttribute::Direction::kSendrecv);
  AddVideoRtpExtension(webrtc::RtpExtension::kAbsSendTimeUri,
                       SdpDirectionAttribute::Direction::kSendrecv);
  AddVideoRtpExtension(webrtc::RtpExtension::kTimestampOffsetUri,
                       SdpDirectionAttribute::Direction::kSendrecv);
}

void JsepSessionImpl::SetState(JsepSignalingState state) {
  if (state == mState) return;

  MOZ_MTLOG(ML_NOTICE, "[" << mName << "]: " << GetStateStr(mState) << " -> "
                           << GetStateStr(state));
  mState = state;
}

JsepSession::Result JsepSessionImpl::AddRemoteIceCandidate(
    const std::string& candidate, const std::string& mid,
    const Maybe<uint16_t>& level, const std::string& ufrag,
    std::string* transportId) {
  mLastError.clear();
  if (!mCurrentRemoteDescription && !mPendingRemoteDescription) {
    JSEP_SET_ERROR("Cannot add ICE candidate when there is no remote SDP");
    return dom::PCError::InvalidStateError;
  }

  if (mid.empty() && !level.isSome() && candidate.empty()) {
    // Set end-of-candidates on SDP
    if (mCurrentRemoteDescription) {
      nsresult rv = mSdpHelper.SetIceGatheringComplete(
          mCurrentRemoteDescription.get(), ufrag);
      NS_ENSURE_SUCCESS(rv, dom::PCError::OperationError);
    }

    if (mPendingRemoteDescription) {
      // If we had an error when adding the candidate to the current
      // description, we stomp them here. This is deliberate.
      nsresult rv = mSdpHelper.SetIceGatheringComplete(
          mPendingRemoteDescription.get(), ufrag);
      NS_ENSURE_SUCCESS(rv, dom::PCError::OperationError);
    }
    return Result();
  }

  JsepTransceiver* transceiver = nullptr;
  if (!mid.empty()) {
    transceiver = GetTransceiverForMid(mid);
  } else if (level.isSome()) {
    transceiver = GetTransceiverForLevel(level.value());
  }

  if (!transceiver) {
    JSEP_SET_ERROR("Cannot set ICE candidate for level="
                   << level << " mid=" << mid << ": No such transceiver.");
    return dom::PCError::OperationError;
  }

  if (level.isSome() && transceiver->GetLevel() != level.value()) {
    MOZ_MTLOG(ML_WARNING, "Mismatch between mid and level - \""
                              << mid << "\" is not the mid for level "
                              << level);
  }

  *transportId = transceiver->mTransport.mTransportId;
  nsresult rv = NS_ERROR_UNEXPECTED;
  if (mCurrentRemoteDescription) {
    rv =
        mSdpHelper.AddCandidateToSdp(mCurrentRemoteDescription.get(), candidate,
                                     transceiver->GetLevel(), ufrag);
  }

  if (mPendingRemoteDescription) {
    // If we had an error when adding the candidate to the current description,
    // we stomp them here. This is deliberate.
    rv =
        mSdpHelper.AddCandidateToSdp(mPendingRemoteDescription.get(), candidate,
                                     transceiver->GetLevel(), ufrag);
  }

  NS_ENSURE_SUCCESS(rv, dom::PCError::OperationError);
  return Result();
}

nsresult JsepSessionImpl::AddLocalIceCandidate(const std::string& candidate,
                                               const std::string& transportId,
                                               const std::string& ufrag,
                                               uint16_t* level,
                                               std::string* mid,
                                               bool* skipped) {
  mLastError.clear();
  if (!mCurrentLocalDescription && !mPendingLocalDescription) {
    JSEP_SET_ERROR("Cannot add ICE candidate when there is no local SDP");
    return NS_ERROR_UNEXPECTED;
  }

  JsepTransceiver* transceiver = GetTransceiverWithTransport(transportId);
  *skipped = !transceiver;
  if (*skipped) {
    // mainly here to make some testing less complicated, but also just in case
    return NS_OK;
  }

  MOZ_ASSERT(
      transceiver->IsAssociated(),
      "ICE candidate was gathered before the transceiver was associated! "
      "This should never happen.");
  *level = transceiver->GetLevel();
  *mid = transceiver->GetMid();

  nsresult rv = NS_ERROR_INVALID_ARG;
  if (mCurrentLocalDescription) {
    rv = mSdpHelper.AddCandidateToSdp(mCurrentLocalDescription.get(), candidate,
                                      *level, ufrag);
  }

  if (mPendingLocalDescription) {
    // If we had an error when adding the candidate to the current description,
    // we stomp them here. This is deliberate.
    rv = mSdpHelper.AddCandidateToSdp(mPendingLocalDescription.get(), candidate,
                                      *level, ufrag);
  }

  return rv;
}

nsresult JsepSessionImpl::UpdateDefaultCandidate(
    const std::string& defaultCandidateAddr, uint16_t defaultCandidatePort,
    const std::string& defaultRtcpCandidateAddr,
    uint16_t defaultRtcpCandidatePort, const std::string& transportId) {
  mLastError.clear();

  mozilla::Sdp* sdp =
      GetParsedLocalDescription(kJsepDescriptionPendingOrCurrent);

  if (!sdp) {
    JSEP_SET_ERROR("Cannot add ICE candidate in state " << GetStateStr(mState));
    return NS_ERROR_UNEXPECTED;
  }

  for (const auto& transceiver : mTransceivers) {
    // We set the default address for bundled m-sections, but not candidate
    // attributes. Ugh.
    if (transceiver->mTransport.mTransportId == transportId) {
      MOZ_ASSERT(transceiver->HasLevel(),
                 "Transceiver has a transport, but no level! "
                 "This should never happen.");
      std::string defaultRtcpCandidateAddrCopy(defaultRtcpCandidateAddr);
      if (mState == kJsepStateStable) {
        if (transceiver->mTransport.mComponents == 1) {
          // We know we're doing rtcp-mux by now. Don't create an rtcp attr.
          defaultRtcpCandidateAddrCopy = "";
          defaultRtcpCandidatePort = 0;
        }
      }

      size_t level = transceiver->GetLevel();
      if (level >= sdp->GetMediaSectionCount()) {
        MOZ_ASSERT(false, "Transceiver's level is too large!");
        JSEP_SET_ERROR("Transceiver's level is too large!");
        return NS_ERROR_FAILURE;
      }

      mSdpHelper.SetDefaultAddresses(defaultCandidateAddr, defaultCandidatePort,
                                     defaultRtcpCandidateAddrCopy,
                                     defaultRtcpCandidatePort,
                                     &sdp->GetMediaSection(level));
    }
  }

  return NS_OK;
}

nsresult JsepSessionImpl::GetNegotiatedBundledMids(
    SdpHelper::BundledMids* bundledMids) {
  const Sdp* answerSdp = GetAnswer();

  if (!answerSdp) {
    return NS_OK;
  }

  return mSdpHelper.GetBundledMids(*answerSdp, bundledMids);
}

mozilla::Sdp* JsepSessionImpl::GetParsedLocalDescription(
    JsepDescriptionPendingOrCurrent type) const {
  if (type == kJsepDescriptionPending) {
    return mPendingLocalDescription.get();
  } else if (mPendingLocalDescription &&
             type == kJsepDescriptionPendingOrCurrent) {
    return mPendingLocalDescription.get();
  }
  return mCurrentLocalDescription.get();
}

mozilla::Sdp* JsepSessionImpl::GetParsedRemoteDescription(
    JsepDescriptionPendingOrCurrent type) const {
  if (type == kJsepDescriptionPending) {
    return mPendingRemoteDescription.get();
  } else if (mPendingRemoteDescription &&
             type == kJsepDescriptionPendingOrCurrent) {
    return mPendingRemoteDescription.get();
  }
  return mCurrentRemoteDescription.get();
}

const Sdp* JsepSessionImpl::GetAnswer() const {
  return mWasOffererLastTime ? mCurrentRemoteDescription.get()
                             : mCurrentLocalDescription.get();
}

void JsepSessionImpl::SetIceRestarting(bool restarting) {
  if (restarting) {
    // not restarting -> restarting
    if (!IsIceRestarting()) {
      // We don't set this more than once, so the old ufrag/pwd is preserved
      // even if we CreateOffer({iceRestart:true}) multiple times in a row.
      mOldIceUfrag = mIceUfrag;
      mOldIcePwd = mIcePwd;
    }
    mIceUfrag = GetRandomHex(1);
    mIcePwd = GetRandomHex(4);
  } else if (IsIceRestarting()) {
    // restarting -> not restarting, restore old ufrag/pwd
    mIceUfrag = mOldIceUfrag;
    mIcePwd = mOldIcePwd;
    mOldIceUfrag.clear();
    mOldIcePwd.clear();
  }
}

nsresult JsepSessionImpl::Close() {
  mLastError.clear();
  SetState(kJsepStateClosed);
  return NS_OK;
}

const std::string JsepSessionImpl::GetLastError() const { return mLastError; }

bool JsepSessionImpl::CheckNegotiationNeeded() const {
  MOZ_ASSERT(mState == kJsepStateStable);

  for (const auto& transceiver : mTransceivers) {
    if (transceiver->IsStopped()) {
      if (transceiver->IsAssociated()) {
        MOZ_MTLOG(ML_DEBUG, "[" << mName
                                << "]: Negotiation needed because of "
                                   "stopped transceiver that still has a mid.");
        return true;
      }
      continue;
    }

    if (!transceiver->IsAssociated()) {
      MOZ_MTLOG(ML_DEBUG, "[" << mName
                              << "]: Negotiation needed because of "
                                 "unassociated (but not stopped) transceiver.");
      return true;
    }

    if (!mCurrentLocalDescription || !mCurrentRemoteDescription) {
      MOZ_CRASH(
          "Transceivers should not be associated if we're in stable "
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
      MOZ_MTLOG(ML_DEBUG, "[" << mName
                              << "]: Negotiation needed because of "
                                 "lack of a=msid, and transceiver is sending.");
      return true;
    }

    if (IsOfferer()) {
      if ((local.GetDirection() != transceiver->mJsDirection) &&
          reverse(remote.GetDirection()) != transceiver->mJsDirection) {
        MOZ_MTLOG(ML_DEBUG, "[" << mName
                                << "]: Negotiation needed because "
                                   "the direction on our offer, and the remote "
                                   "answer, does not "
                                   "match the direction on a transceiver.");
        return true;
      }
    } else if (local.GetDirection() !=
               (transceiver->mJsDirection & reverse(remote.GetDirection()))) {
      MOZ_MTLOG(
          ML_DEBUG,
          "[" << mName
              << "]: Negotiation needed because "
                 "the direction on our answer doesn't match the direction on a "
                 "transceiver, even though the remote offer would have allowed "
                 "it.");
      return true;
    }
  }

  return false;
}

}  // namespace mozilla
