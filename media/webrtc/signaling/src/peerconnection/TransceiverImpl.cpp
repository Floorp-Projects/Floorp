/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TransceiverImpl.h"
#include "mtransport/runnable_utils.h"
#include "mozilla/UniquePtr.h"
#include <sstream>
#include <string>
#include <vector>
#include <queue>
#include "AudioConduit.h"
#include "VideoConduit.h"
#include "MediaStreamGraph.h"
#include "MediaPipeline.h"
#include "MediaPipelineFilter.h"
#include "signaling/src/jsep/JsepTrack.h"
#include "MediaStreamGraphImpl.h"
#include "logging.h"
#include "MediaEngine.h"
#include "nsIPrincipal.h"
#include "MediaSegment.h"
#include "RemoteTrackSource.h"
#include "MediaConduitInterface.h"
#include "PeerConnectionMedia.h"
#include "mozilla/dom/RTCRtpReceiverBinding.h"
#include "mozilla/dom/RTCRtpSenderBinding.h"
#include "mozilla/dom/RTCRtpTransceiverBinding.h"
#include "mozilla/dom/TransceiverImplBinding.h"

namespace mozilla {

MOZ_MTLOG_MODULE("transceiverimpl")

TransceiverImpl::TransceiverImpl(
    const std::string& aPCHandle,
    JsepTransceiver* aJsepTransceiver,
    nsIEventTarget* aMainThread,
    nsIEventTarget* aStsThread,
    DOMMediaStream& aReceiveStream,
    dom::MediaStreamTrack* aSendTrack,
    WebRtcCallWrapper* aCallWrapper) :
  mPCHandle(aPCHandle),
  mJsepTransceiver(aJsepTransceiver),
  mHaveStartedReceiving(false),
  mHaveSetupTransport(false),
  mMainThread(aMainThread),
  mStsThread(aStsThread),
  mReceiveStream(&aReceiveStream),
  mSendTrack(aSendTrack),
  mCallWrapper(aCallWrapper)
{
  if (IsVideo()) {
    InitVideo();
  } else {
    InitAudio();
  }

  if (!IsValid()) {
    return;
  }

  mConduit->SetPCHandle(mPCHandle);

  StartReceiveStream();

  mTransmitPipeline = new MediaPipelineTransmit(
      mPCHandle,
      mMainThread.get(),
      mStsThread.get(),
      IsVideo(),
      mSendTrack,
      mConduit);
}

TransceiverImpl::~TransceiverImpl() = default;

NS_IMPL_ISUPPORTS0(TransceiverImpl)

void
TransceiverImpl::InitAudio()
{
  mConduit = AudioSessionConduit::Create();

  if (!mConduit) {
    MOZ_MTLOG(ML_ERROR, mPCHandle << "[" << mMid << "]: " << __FUNCTION__ <<
                        ": Failed to create AudioSessionConduit");
    // TODO(bug 1422897): We need a way to record this when it happens in the
    // wild.
    return;
  }

  mReceivePipeline = new MediaPipelineReceiveAudio(
      mPCHandle,
      mMainThread.get(),
      mStsThread.get(),
      static_cast<AudioSessionConduit*>(mConduit.get()),
      mReceiveStream->GetInputStream()->AsSourceStream());
}

void
TransceiverImpl::InitVideo()
{
  mConduit = VideoSessionConduit::Create(mCallWrapper);

  if (!mConduit) {
    MOZ_MTLOG(ML_ERROR, mPCHandle << "[" << mMid << "]: " << __FUNCTION__ <<
                        ": Failed to create VideoSessionConduit");
    // TODO(bug 1422897): We need a way to record this when it happens in the
    // wild.
    return;
  }

  mReceivePipeline = new MediaPipelineReceiveVideo(
      mPCHandle,
      mMainThread.get(),
      mStsThread.get(),
      static_cast<VideoSessionConduit*>(mConduit.get()),
      mReceiveStream->GetInputStream()->AsSourceStream());
}

nsresult
TransceiverImpl::UpdateSinkIdentity(dom::MediaStreamTrack* aTrack,
                                    nsIPrincipal* aPrincipal,
                                    const PeerIdentity* aSinkIdentity)
{
  if (mJsepTransceiver->IsStopped()) {
    return NS_OK;
  }

  mTransmitPipeline->UpdateSinkIdentity_m(aTrack, aPrincipal, aSinkIdentity);
  return NS_OK;
}

void
TransceiverImpl::Shutdown_m()
{
  mReceivePipeline->Shutdown_m();
  mTransmitPipeline->Shutdown_m();
  mReceivePipeline = nullptr;
  mTransmitPipeline = nullptr;
  mSendTrack = nullptr;
  mConduit = nullptr;
  RUN_ON_THREAD(mStsThread, WrapRelease(mRtpFlow.forget()), NS_DISPATCH_NORMAL);
  RUN_ON_THREAD(mStsThread, WrapRelease(mRtcpFlow.forget()), NS_DISPATCH_NORMAL);
}

nsresult
TransceiverImpl::UpdateSendTrack(dom::MediaStreamTrack* aSendTrack)
{
  if (mJsepTransceiver->IsStopped()) {
    return NS_ERROR_UNEXPECTED;
  }

  MOZ_MTLOG(ML_DEBUG, mPCHandle << "[" << mMid << "]: " << __FUNCTION__ <<
                      "(" << aSendTrack << ")");
  mSendTrack = aSendTrack;
  return mTransmitPipeline->ReplaceTrack(mSendTrack);
}

nsresult
TransceiverImpl::UpdateTransport(PeerConnectionMedia& aTransportManager)
{
  if (!mJsepTransceiver->HasLevel()) {
    return NS_OK;
  }

  if (!mHaveSetupTransport) {
    mReceivePipeline->SetLevel(mJsepTransceiver->GetLevel());
    mTransmitPipeline->SetLevel(mJsepTransceiver->GetLevel());
    mHaveSetupTransport = true;
  }

  ASSERT_ON_THREAD(mMainThread);
  nsAutoPtr<MediaPipelineFilter> filter;

  mRtpFlow = aTransportManager.GetTransportFlow(
      mJsepTransceiver->GetTransportLevel(), false);
  mRtcpFlow = aTransportManager.GetTransportFlow(
      mJsepTransceiver->GetTransportLevel(), true);

  if (mJsepTransceiver->HasBundleLevel() &&
      mJsepTransceiver->mRecvTrack.GetNegotiatedDetails()) {
    filter = new MediaPipelineFilter;

    // Add remote SSRCs so we can distinguish which RTP packets actually
    // belong to this pipeline (also RTCP sender reports).
    for (unsigned int ssrc : mJsepTransceiver->mRecvTrack.GetSsrcs()) {
      filter->AddRemoteSSRC(ssrc);
    }

    // TODO(bug 1105005): Tell the filter about the mid for this track

    // Add unique payload types as a last-ditch fallback
    auto uniquePts =
      mJsepTransceiver->mRecvTrack.GetNegotiatedDetails()->GetUniquePayloadTypes();
    for (unsigned char& uniquePt : uniquePts) {
      filter->AddUniquePT(uniquePt);
    }
  }

  mReceivePipeline->UpdateTransport_m(mRtpFlow, mRtcpFlow, filter);
  mTransmitPipeline->UpdateTransport_m(mRtpFlow, mRtcpFlow, nsAutoPtr<MediaPipelineFilter>());
  return NS_OK;
}

nsresult
TransceiverImpl::UpdateConduit()
{
  if (mJsepTransceiver->IsStopped()) {
    return NS_OK;
  }

  if (mJsepTransceiver->IsAssociated()) {
    mMid = mJsepTransceiver->GetMid();
  } else {
    mMid.clear();
  }

  mReceivePipeline->Stop();
  mTransmitPipeline->Stop();

  // NOTE(pkerr) - the Call API requires the both local_ssrc and remote_ssrc be
  // set to a non-zero value or the CreateVideo...Stream call will fail.
  if (mJsepTransceiver->mSendTrack.GetSsrcs().empty()) {
    MOZ_MTLOG(ML_ERROR, mPCHandle << "[" << mMid << "]: " << __FUNCTION__ <<
                        " No local SSRC set! (Should be set regardless of "
                        "whether we're sending RTP; we need a local SSRC in "
                        "all cases)");
    return NS_ERROR_FAILURE;
  }

  if(!mConduit->SetLocalSSRCs(mJsepTransceiver->mSendTrack.GetSsrcs())) {
    MOZ_MTLOG(ML_ERROR, mPCHandle << "[" << mMid << "]: " << __FUNCTION__ <<
                        " SetLocalSSRCs failed");
    return NS_ERROR_FAILURE;
  }

  mConduit->SetLocalCNAME(mJsepTransceiver->mSendTrack.GetCNAME().c_str());
  mConduit->SetLocalMID(mJsepTransceiver->mTransport->mTransportId);

  nsresult rv;

  if (IsVideo()) {
    rv = UpdateVideoConduit();
  } else {
    rv = UpdateAudioConduit();
  }

  if (NS_FAILED(rv)) {
    return rv;
  }

  if (mJsepTransceiver->mRecvTrack.GetActive()) {
    MOZ_ASSERT(mReceiveStream);
    mReceivePipeline->Start();
    mHaveStartedReceiving = true;
  }

  if (mJsepTransceiver->mSendTrack.GetActive()) {
    if (!mSendTrack) {
      MOZ_MTLOG(ML_WARNING, mPCHandle << "[" << mMid << "]: " << __FUNCTION__ <<
                            " Starting transmit conduit without send track!");
    }
    mTransmitPipeline->Start();
  }

  return NS_OK;
}

nsresult
TransceiverImpl::UpdatePrincipal(nsIPrincipal* aPrincipal)
{
  if (mJsepTransceiver->IsStopped()) {
    return NS_OK;
  }

  nsTArray<RefPtr<dom::MediaStreamTrack>> receiveTracks;
  mReceiveStream->GetTracks(receiveTracks);
  if (receiveTracks.Length() != 1) {
    MOZ_MTLOG(ML_ERROR, mPCHandle << "[" << mMid << "]: " << __FUNCTION__ <<
                        " mReceiveStream doesn't have exactly one track "
                        "(it has " << receiveTracks.Length() << ")");
    MOZ_CRASH();
    return NS_ERROR_FAILURE;
  }

  // This blasts away the existing principal.
  // We only do this when we become certain that the all tracks are safe to make
  // accessible to the script principal.
  RemoteTrackSource& source =
    static_cast<RemoteTrackSource&>(receiveTracks[0]->GetSource());
  source.SetPrincipal(aPrincipal);

  mReceivePipeline->SetPrincipalHandle_m(MakePrincipalHandle(aPrincipal));
  return NS_OK;
}

nsresult
TransceiverImpl::SyncWithMatchingVideoConduits(
    std::vector<RefPtr<TransceiverImpl>>& transceivers)
{
  if (mJsepTransceiver->IsStopped()) {
    return NS_OK;
  }

  if (IsVideo()) {
    MOZ_MTLOG(ML_ERROR, mPCHandle << "[" << mMid << "]: " << __FUNCTION__ <<
                        " called when transceiver is not "
                        "video! This should never happen.");
    MOZ_CRASH();
    return NS_ERROR_UNEXPECTED;
  }

  std::set<std::string> myReceiveStreamIds;
  myReceiveStreamIds.insert(mJsepTransceiver->mRecvTrack.GetStreamIds().begin(),
                            mJsepTransceiver->mRecvTrack.GetStreamIds().end());

  for (RefPtr<TransceiverImpl>& transceiver : transceivers) {
    if (!transceiver->IsVideo()) {
      // |this| is an audio transceiver, so we skip other audio transceivers
      continue;
    }

    // Maybe could make this more efficient by cacheing this set, but probably
    // not worth it.
    for (const std::string& streamId :
         transceiver->mJsepTransceiver->mRecvTrack.GetStreamIds()) {
      if (myReceiveStreamIds.count(streamId)) {
        // Ok, we have one video, one non-video - cross the streams!
        WebrtcAudioConduit *audio_conduit =
          static_cast<WebrtcAudioConduit*>(mConduit.get());
        WebrtcVideoConduit *video_conduit =
          static_cast<WebrtcVideoConduit*>(transceiver->mConduit.get());

        video_conduit->SyncTo(audio_conduit);
        MOZ_MTLOG(ML_DEBUG, mPCHandle << "[" << mMid << "]: " << __FUNCTION__ <<
                            " Syncing " << video_conduit << " to "
                            << audio_conduit);
      }
    }
  }

  return NS_OK;
}

bool
TransceiverImpl::ConduitHasPluginID(uint64_t aPluginID)
{
  return mConduit->CodecPluginID() == aPluginID;
}

bool
TransceiverImpl::HasSendTrack(const dom::MediaStreamTrack* aSendTrack) const
{
  if (!mSendTrack) {
    return false;
  }

  if (!aSendTrack) {
    return true;
  }

  return mSendTrack.get() == aSendTrack;
}

void
TransceiverImpl::SyncWithJS(dom::RTCRtpTransceiver& aJsTransceiver,
                            ErrorResult& aRv)
{
  MOZ_MTLOG(ML_DEBUG, mPCHandle << "[" << mMid << "]: " << __FUNCTION__
                      << " Syncing with JS transceiver");

  // Update stopped, both ways, since either JSEP or JS can stop these
  if (mJsepTransceiver->IsStopped()) {
    // We don't call RTCRtpTransceiver::Stop(), because that causes another sync
    aJsTransceiver.SetStopped(aRv);
    Stop();
  } else if (aJsTransceiver.GetStopped(aRv)) {
    mJsepTransceiver->Stop();
    Stop();
  }

  // Lots of this in here for simple getters that should never fail. Lame.
  // Just propagate the exception and let JS log it.
  if (aRv.Failed()) {
    return;
  }

  // Update direction from JS only
  dom::RTCRtpTransceiverDirection direction = aJsTransceiver.GetDirection(aRv);

  if (aRv.Failed()) {
    return;
  }

  switch (direction) {
    case dom::RTCRtpTransceiverDirection::Sendrecv:
      mJsepTransceiver->mJsDirection =
        SdpDirectionAttribute::Direction::kSendrecv;
      break;
    case dom::RTCRtpTransceiverDirection::Sendonly:
      mJsepTransceiver->mJsDirection =
        SdpDirectionAttribute::Direction::kSendonly;
      break;
    case dom::RTCRtpTransceiverDirection::Recvonly:
      mJsepTransceiver->mJsDirection =
        SdpDirectionAttribute::Direction::kRecvonly;
      break;
    case dom::RTCRtpTransceiverDirection::Inactive:
      mJsepTransceiver->mJsDirection =
        SdpDirectionAttribute::Direction::kInactive;
      break;
    default:
      MOZ_ASSERT(false);
      aRv = NS_ERROR_INVALID_ARG;
      return;
  }

  // Update send track ids in JSEP
  RefPtr<dom::RTCRtpSender> sender = aJsTransceiver.GetSender(aRv);
  if (aRv.Failed()) {
    return;
  }

  RefPtr<dom::MediaStreamTrack> sendTrack = sender->GetTrack(aRv);
  if (aRv.Failed()) {
    return;
  }

  if (sendTrack) {
    nsString wideTrackId;
    sendTrack->GetId(wideTrackId);
    std::string trackId = NS_ConvertUTF16toUTF8(wideTrackId).get();
    MOZ_ASSERT(!trackId.empty());

    nsTArray<RefPtr<DOMMediaStream>> streams;
    sender->GetStreams(streams, aRv);
    if (aRv.Failed()) {
      return;
    }

    std::vector<std::string> streamIds;
    for (const auto& stream : streams) {
      nsString wideStreamId;
      stream->GetId(wideStreamId);
      std::string streamId = NS_ConvertUTF16toUTF8(wideStreamId).get();
      MOZ_ASSERT(!streamId.empty());
      streamIds.push_back(streamId);
    }

    mJsepTransceiver->mSendTrack.UpdateTrackIds(streamIds, trackId);
  }

  // Update RTCRtpParameters
  // TODO: Both ways for things like ssrc, codecs, header extensions, etc

  dom::RTCRtpParameters parameters;
  sender->GetParameters(parameters, aRv);

  if (aRv.Failed()) {
    return;
  }

  std::vector<JsepTrack::JsConstraints> constraints;

  if (parameters.mEncodings.WasPassed()) {
    for (auto& encoding : parameters.mEncodings.Value()) {
      JsepTrack::JsConstraints constraint;
      if (encoding.mRid.WasPassed()) {
        // TODO: Either turn on the RID RTP header extension in JsepSession, or
        // just leave that extension on all the time?
        constraint.rid = NS_ConvertUTF16toUTF8(encoding.mRid.Value()).get();
      }
      if (encoding.mMaxBitrate.WasPassed()) {
        constraint.constraints.maxBr = encoding.mMaxBitrate.Value();
      }
      constraint.constraints.scaleDownBy = encoding.mScaleResolutionDownBy;
      constraints.push_back(constraint);
    }
  }

  // TODO: Update conduits?

  mJsepTransceiver->mSendTrack.SetJsConstraints(constraints);

  // Update webrtc track id in JS; the ids in SDP are not surfaced to content,
  // because they don't follow the rules that track/stream ids must. Our JS
  // code must be able to map the SDP ids to the actual tracks/streams, and
  // this is how the mapping for track ids is updated.
  nsString webrtcTrackId =
    NS_ConvertUTF8toUTF16(mJsepTransceiver->mRecvTrack.GetTrackId().c_str());
  MOZ_MTLOG(ML_DEBUG, mPCHandle << "[" << mMid << "]: " << __FUNCTION__
                      << " Setting webrtc track id: "
                      << mJsepTransceiver->mRecvTrack.GetTrackId().c_str());
  aJsTransceiver.SetRemoteTrackId(webrtcTrackId, aRv);

  if (aRv.Failed()) {
    return;
  }

  // mid from JSEP
  if (mJsepTransceiver->IsAssociated()) {
    aJsTransceiver.SetMid(
        NS_ConvertUTF8toUTF16(mJsepTransceiver->GetMid().c_str()),
        aRv);
  } else {
    aJsTransceiver.UnsetMid(aRv);
  }

  if (aRv.Failed()) {
    return;
  }

  // currentDirection from JSEP, but not if "this transceiver has never been
  // represented in an offer/answer exchange"
  if (mJsepTransceiver->HasLevel()) {
    dom::RTCRtpTransceiverDirection currentDirection;
    if (mJsepTransceiver->mSendTrack.GetActive()) {
      if (mJsepTransceiver->mRecvTrack.GetActive()) {
        currentDirection = dom::RTCRtpTransceiverDirection::Sendrecv;
      } else {
        currentDirection = dom::RTCRtpTransceiverDirection::Sendonly;
      }
    } else {
      if (mJsepTransceiver->mRecvTrack.GetActive()) {
        currentDirection = dom::RTCRtpTransceiverDirection::Recvonly;
      } else {
        currentDirection = dom::RTCRtpTransceiverDirection::Inactive;
      }
    }

    if (mJsepTransceiver->IsNegotiated()) {
      aJsTransceiver.SetCurrentDirection(currentDirection, aRv);
    }

    if (aRv.Failed()) {
      return;
    }
  }

  // AddTrack magic from JS
  if (aJsTransceiver.GetAddTrackMagic(aRv)) {
    mJsepTransceiver->SetAddTrackMagic();
  }

  if (aRv.Failed()) {
    return;
  }

  if (mJsepTransceiver->IsRemoved()) {
    aJsTransceiver.Remove(aRv);
  }
}

void
TransceiverImpl::InsertDTMFTone(int tone, uint32_t duration)
{
  if (mJsepTransceiver->IsStopped()) {
    return;
  }

  RefPtr<AudioSessionConduit> conduit(static_cast<AudioSessionConduit*>(
        mConduit.get()));
  mStsThread->Dispatch(WrapRunnableNM([conduit, tone, duration] () {
        //Note: We default to channel 0, not inband, and 6dB attenuation.
        //      here. We might want to revisit these choices in the future.
        conduit->InsertDTMFTone(0, tone, true, duration, 6);
        }), NS_DISPATCH_NORMAL);
}

bool
TransceiverImpl::HasReceiveTrack(const dom::MediaStreamTrack* aRecvTrack) const
{
  if (!mHaveStartedReceiving) {
    return false;
  }

  if (!aRecvTrack) {
    return true;
  }

  return mReceiveStream->HasTrack(*aRecvTrack);
}

bool
TransceiverImpl::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto,
                            JS::MutableHandle<JSObject*> aReflector)
{
  return dom::TransceiverImplBinding::Wrap(aCx, this, aGivenProto, aReflector);
}

already_AddRefed<dom::MediaStreamTrack>
TransceiverImpl::GetReceiveTrack()
{
  nsTArray<RefPtr<dom::MediaStreamTrack>> receiveTracks;
  mReceiveStream->GetTracks(receiveTracks);
  if (receiveTracks.Length() != 1) {
    return nullptr;
  }

  return receiveTracks[0].forget();
}

RefPtr<MediaPipeline>
TransceiverImpl::GetSendPipeline()
{
  return mTransmitPipeline;
}

RefPtr<MediaPipeline>
TransceiverImpl::GetReceivePipeline()
{
  return mReceivePipeline;
}

void
TransceiverImpl::AddRIDExtension(unsigned short aExtensionId)
{
  if (mJsepTransceiver->IsStopped()) {
    return;
  }

  mReceivePipeline->AddRIDExtension_m(aExtensionId);
}

void
TransceiverImpl::AddRIDFilter(const nsAString& aRid)
{
  if (mJsepTransceiver->IsStopped()) {
    return;
  }

  mReceivePipeline->AddRIDFilter_m(NS_ConvertUTF16toUTF8(aRid).get());
}

static std::vector<JsepCodecDescription*>
GetCodecs(const JsepTrackNegotiatedDetails& aDetails)
{
  // We do not try to handle cases where a codec is not used on the primary
  // encoding.
  if (aDetails.GetEncodingCount()) {
    return aDetails.GetEncoding(0).GetCodecs();
  }
  return std::vector<JsepCodecDescription*>();
}

static nsresult
JsepCodecDescToAudioCodecConfig(const JsepCodecDescription& aCodec,
                                AudioCodecConfig** aConfig)
{
  MOZ_ASSERT(aCodec.mType == SdpMediaSection::kAudio);
  if (aCodec.mType != SdpMediaSection::kAudio)
    return NS_ERROR_INVALID_ARG;

  const JsepAudioCodecDescription& desc =
      static_cast<const JsepAudioCodecDescription&>(aCodec);

  uint16_t pt;

  if (!desc.GetPtAsInt(&pt)) {
    MOZ_MTLOG(ML_ERROR, "Invalid payload type: " << desc.mDefaultPt);
    return NS_ERROR_INVALID_ARG;
  }

  *aConfig = new AudioCodecConfig(pt,
                                  desc.mName,
                                  desc.mClock,
                                  desc.mPacketSize,
                                  desc.mForceMono ? 1 : desc.mChannels,
                                  desc.mBitrate,
                                  desc.mFECEnabled);
  (*aConfig)->mMaxPlaybackRate = desc.mMaxPlaybackRate;
  (*aConfig)->mDtmfEnabled = desc.mDtmfEnabled;

  return NS_OK;
}

static nsresult
NegotiatedDetailsToAudioCodecConfigs(const JsepTrackNegotiatedDetails& aDetails,
                                     PtrVector<AudioCodecConfig>* aConfigs)
{
  std::vector<JsepCodecDescription*> codecs(GetCodecs(aDetails));
  for (const JsepCodecDescription* codec : codecs) {
    AudioCodecConfig* config;
    if (NS_FAILED(JsepCodecDescToAudioCodecConfig(*codec, &config))) {
      return NS_ERROR_INVALID_ARG;
    }
    aConfigs->values.push_back(config);
  }

  if (aConfigs->values.empty()) {
    MOZ_MTLOG(ML_ERROR, "Can't set up a conduit with 0 codecs");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
TransceiverImpl::UpdateAudioConduit()
{
  RefPtr<AudioSessionConduit> conduit = static_cast<AudioSessionConduit*>(
      mConduit.get());

  if (mJsepTransceiver->mRecvTrack.GetNegotiatedDetails() &&
      mJsepTransceiver->mRecvTrack.GetActive()) {
    const auto& details(*mJsepTransceiver->mRecvTrack.GetNegotiatedDetails());
    PtrVector<AudioCodecConfig> configs;
    nsresult rv = NegotiatedDetailsToAudioCodecConfigs(details, &configs);

    if (NS_FAILED(rv)) {
      MOZ_MTLOG(ML_ERROR, mPCHandle << "[" << mMid << "]: " << __FUNCTION__ <<
                          " Failed to convert JsepCodecDescriptions to "
                          "AudioCodecConfigs (recv).");
      return rv;
    }

    auto error = conduit->ConfigureRecvMediaCodecs(configs.values);

    if (error) {
      MOZ_MTLOG(ML_ERROR, mPCHandle << "[" << mMid << "]: " << __FUNCTION__ <<
                          " ConfigureRecvMediaCodecs failed: " << error);
      return NS_ERROR_FAILURE;
    }

    const SdpExtmapAttributeList::Extmap* audioLevelExt =
        details.GetExt(webrtc::RtpExtension::kAudioLevelUri);
    if (audioLevelExt) {
      MOZ_MTLOG(ML_DEBUG, "Calling EnableAudioLevelExtension");
      error = conduit->EnableAudioLevelExtension(true,
                                                 audioLevelExt->entry,
                                                 true);

      if (error) {
        MOZ_MTLOG(ML_ERROR, mPCHandle << "[" << mMid << "]: " << __FUNCTION__ <<
                            " EnableAudioLevelExtension failed: " << error);
        return NS_ERROR_FAILURE;
      }
    }
  }

  if (mJsepTransceiver->mSendTrack.GetNegotiatedDetails() &&
      mJsepTransceiver->mSendTrack.GetActive()) {
    const auto& details(*mJsepTransceiver->mSendTrack.GetNegotiatedDetails());
    PtrVector<AudioCodecConfig> configs;
    nsresult rv = NegotiatedDetailsToAudioCodecConfigs(details, &configs);

    if (NS_FAILED(rv)) {
      MOZ_MTLOG(ML_ERROR, mPCHandle << "[" << mMid << "]: " << __FUNCTION__ <<
                          " Failed to convert JsepCodecDescriptions to "
                          "AudioCodecConfigs (send).");
      return rv;
    }

    for (auto value: configs.values) {
      if (value->mName == "telephone-event") {
        // we have a telephone event codec, so we need to make sure
        // the dynamic pt is set properly
        conduit->SetDtmfPayloadType(value->mType, value->mFreq);
        break;
      }
    }

    auto error = conduit->ConfigureSendMediaCodec(configs.values[0]);
    if (error) {
      MOZ_MTLOG(ML_ERROR, mPCHandle << "[" << mMid << "]: " << __FUNCTION__ <<
                          " ConfigureSendMediaCodec failed: " << error);
      return NS_ERROR_FAILURE;
    }

    // Should these be genericized like they are in the video conduit case?
    const SdpExtmapAttributeList::Extmap* audioLevelExt =
        details.GetExt(webrtc::RtpExtension::kAudioLevelUri);

    if (audioLevelExt) {
      MOZ_MTLOG(ML_DEBUG, "Calling EnableAudioLevelExtension");
      error = conduit->EnableAudioLevelExtension(true,
                                                 audioLevelExt->entry,
                                                 false);

      if (error) {
        MOZ_MTLOG(ML_ERROR, mPCHandle << "[" << mMid << "]: " << __FUNCTION__ <<
                            " EnableAudioLevelExtension failed: " << error);
        return NS_ERROR_FAILURE;
      }
    }

    const SdpExtmapAttributeList::Extmap* midExt =
        details.GetExt(webrtc::RtpExtension::kMIdUri);

    if (midExt) {
      MOZ_MTLOG(ML_DEBUG, "Calling EnableMIDExtension");
      error = conduit->EnableMIDExtension(true, midExt->entry);

      if (error) {
        MOZ_MTLOG(ML_ERROR, "EnableMIDExtension failed: " << error);
        return NS_ERROR_FAILURE;
      }
    }
  }

  return NS_OK;
}

static nsresult
JsepCodecDescToVideoCodecConfig(const JsepCodecDescription& aCodec,
                                VideoCodecConfig** aConfig)
{
  MOZ_ASSERT(aCodec.mType == SdpMediaSection::kVideo);
  if (aCodec.mType != SdpMediaSection::kVideo) {
    MOZ_ASSERT(false, "JsepCodecDescription has wrong type");
    return NS_ERROR_INVALID_ARG;
  }

  const JsepVideoCodecDescription& desc =
      static_cast<const JsepVideoCodecDescription&>(aCodec);

  uint16_t pt;

  if (!desc.GetPtAsInt(&pt)) {
    MOZ_MTLOG(ML_ERROR, "Invalid payload type: " << desc.mDefaultPt);
    return NS_ERROR_INVALID_ARG;
  }

  UniquePtr<VideoCodecConfigH264> h264Config;

  if (desc.mName == "H264") {
    h264Config = MakeUnique<VideoCodecConfigH264>();
    size_t spropSize = sizeof(h264Config->sprop_parameter_sets);
    strncpy(h264Config->sprop_parameter_sets,
            desc.mSpropParameterSets.c_str(),
            spropSize);
    h264Config->sprop_parameter_sets[spropSize - 1] = '\0';
    h264Config->packetization_mode = desc.mPacketizationMode;
    h264Config->profile_level_id = desc.mProfileLevelId;
    h264Config->tias_bw = 0; // TODO(bug 1403206)
  }

  VideoCodecConfig* configRaw;
  configRaw = new VideoCodecConfig(
      pt, desc.mName, desc.mConstraints, h264Config.get());

  configRaw->mAckFbTypes = desc.mAckFbTypes;
  configRaw->mNackFbTypes = desc.mNackFbTypes;
  configRaw->mCcmFbTypes = desc.mCcmFbTypes;
  configRaw->mRembFbSet = desc.RtcpFbRembIsSet();
  configRaw->mFECFbSet = desc.mFECEnabled;
  if (desc.mFECEnabled) {
    configRaw->mREDPayloadType = desc.mREDPayloadType;
    configRaw->mULPFECPayloadType = desc.mULPFECPayloadType;
  }

  *aConfig = configRaw;
  return NS_OK;
}

static nsresult
NegotiatedDetailsToVideoCodecConfigs(const JsepTrackNegotiatedDetails& aDetails,
                                     PtrVector<VideoCodecConfig>* aConfigs)
{
  std::vector<JsepCodecDescription*> codecs(GetCodecs(aDetails));
  for (const JsepCodecDescription* codec : codecs) {
    VideoCodecConfig* config;
    if (NS_FAILED(JsepCodecDescToVideoCodecConfig(*codec, &config))) {
      return NS_ERROR_INVALID_ARG;
    }

    config->mTias = aDetails.GetTias();

    for (size_t i = 0; i < aDetails.GetEncodingCount(); ++i) {
      const JsepTrackEncoding& jsepEncoding(aDetails.GetEncoding(i));
      if (jsepEncoding.HasFormat(codec->mDefaultPt)) {
        VideoCodecConfig::SimulcastEncoding encoding;
        encoding.rid = jsepEncoding.mRid;
        encoding.constraints = jsepEncoding.mConstraints;
        config->mSimulcastEncodings.push_back(encoding);
      }
    }

    aConfigs->values.push_back(config);
  }

  return NS_OK;
}

nsresult
TransceiverImpl::UpdateVideoConduit()
{
  RefPtr<VideoSessionConduit> conduit = static_cast<VideoSessionConduit*>(
      mConduit.get());

  // NOTE(pkerr) - this is new behavior. Needed because the CreateVideoReceiveStream
  // method of the Call API will assert (in debug) and fail if a value is not provided
  // for the remote_ssrc that will be used by the far-end sender.
  if (!mJsepTransceiver->mRecvTrack.GetSsrcs().empty()) {
    MOZ_MTLOG(ML_DEBUG, mPCHandle << "[" << mMid << "]: " << __FUNCTION__ <<
              " Setting remote SSRC " <<
              mJsepTransceiver->mRecvTrack.GetSsrcs().front());
    conduit->SetRemoteSSRC(mJsepTransceiver->mRecvTrack.GetSsrcs().front());
  }

  if (mJsepTransceiver->mRecvTrack.GetNegotiatedDetails() &&
      mJsepTransceiver->mRecvTrack.GetActive()) {
    const auto& details(*mJsepTransceiver->mRecvTrack.GetNegotiatedDetails());

    UpdateVideoExtmap(details, false);

    PtrVector<VideoCodecConfig> configs;
    nsresult rv = NegotiatedDetailsToVideoCodecConfigs(details, &configs);

    if (NS_FAILED(rv)) {
      MOZ_MTLOG(ML_ERROR, mPCHandle << "[" << mMid << "]: " << __FUNCTION__ <<
                          " Failed to convert JsepCodecDescriptions to "
                          "VideoCodecConfigs (recv).");
      return rv;
    }

    auto error = conduit->ConfigureRecvMediaCodecs(configs.values);

    if (error) {
      MOZ_MTLOG(ML_ERROR, mPCHandle << "[" << mMid << "]: " << __FUNCTION__ <<
                          " ConfigureRecvMediaCodecs failed: " << error);
      return NS_ERROR_FAILURE;
    }
  }

  // It is possible for SDP to signal that there is a send track, but there not
  // actually be a send track, according to the specification; all that needs to
  // happen is for the transceiver to be configured to send...
  if (mJsepTransceiver->mSendTrack.GetNegotiatedDetails() &&
      mJsepTransceiver->mSendTrack.GetActive() &&
      mSendTrack) {
    const auto& details(*mJsepTransceiver->mSendTrack.GetNegotiatedDetails());

    UpdateVideoExtmap(details, true);

    nsresult rv = ConfigureVideoCodecMode(*conduit);
    if (NS_FAILED(rv)) {
      return rv;
    }

    PtrVector<VideoCodecConfig> configs;
    rv = NegotiatedDetailsToVideoCodecConfigs(details, &configs);

    if (NS_FAILED(rv)) {
      MOZ_MTLOG(ML_ERROR, mPCHandle << "[" << mMid << "]: " << __FUNCTION__ <<
                          " Failed to convert JsepCodecDescriptions to "
                          "VideoCodecConfigs (send).");
      return rv;
    }

    auto error = conduit->ConfigureSendMediaCodec(configs.values[0]);
    if (error) {
      MOZ_MTLOG(ML_ERROR, mPCHandle << "[" << mMid << "]: " << __FUNCTION__ <<
                          " ConfigureSendMediaCodec failed: " << error);
      return NS_ERROR_FAILURE;
    }
  }

  return NS_OK;
}

nsresult
TransceiverImpl::ConfigureVideoCodecMode(VideoSessionConduit& aConduit)
{
  RefPtr<mozilla::dom::VideoStreamTrack> videotrack =
    mSendTrack->AsVideoStreamTrack();

  if (!videotrack) {
    MOZ_MTLOG(ML_ERROR, mPCHandle << "[" << mMid << "]: " << __FUNCTION__ <<
                        " mSendTrack is not video! This should never happen!");
    MOZ_CRASH();
    return NS_ERROR_FAILURE;
  }

  dom::MediaSourceEnum source = videotrack->GetSource().GetMediaSource();
  webrtc::VideoCodecMode mode = webrtc::kRealtimeVideo;
  switch (source) {
    case dom::MediaSourceEnum::Browser:
    case dom::MediaSourceEnum::Screen:
    case dom::MediaSourceEnum::Application:
    case dom::MediaSourceEnum::Window:
      mode = webrtc::kScreensharing;
      break;

    case dom::MediaSourceEnum::Camera:
    default:
      mode = webrtc::kRealtimeVideo;
      break;
  }

  auto error = aConduit.ConfigureCodecMode(mode);
  if (error) {
    MOZ_MTLOG(ML_ERROR, mPCHandle << "[" << mMid << "]: " << __FUNCTION__ <<
                        " ConfigureCodecMode failed: " << error);
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

void
TransceiverImpl::UpdateVideoExtmap(const JsepTrackNegotiatedDetails& aDetails,
                                   bool aSending)
{
  std::vector<webrtc::RtpExtension> extmaps;
  // @@NG read extmap from track
  aDetails.ForEachRTPHeaderExtension(
    [&extmaps](const SdpExtmapAttributeList::Extmap& extmap)
  {
    extmaps.emplace_back(extmap.extensionname, extmap.entry);
  });

  RefPtr<VideoSessionConduit> conduit = static_cast<VideoSessionConduit*>(
      mConduit.get());

  if (!extmaps.empty()) {
    conduit->SetLocalRTPExtensions(aSending, extmaps);
  }
}

static void StartTrack(MediaStream* aSource,
                       nsAutoPtr<MediaSegment>&& aSegment)
{
  class Message : public ControlMessage {
   public:
    Message(MediaStream* aStream, nsAutoPtr<MediaSegment>&& aSegment)
      : ControlMessage(aStream),
        segment_(aSegment) {}

    void Run() override {
      TrackRate track_rate = segment_->GetType() == MediaSegment::AUDIO ?
        WEBRTC_DEFAULT_SAMPLE_RATE : mStream->GraphRate();
      StreamTime current_end = mStream->GetTracksEnd();
      MOZ_MTLOG(ML_DEBUG, "current_end = " << current_end);
      TrackTicks current_ticks =
        mStream->TimeToTicksRoundUp(track_rate, current_end);

      // Add a track 'now' to avoid possible underrun, especially if we add
      // a track "later".

      if (current_end != 0L) {
        MOZ_MTLOG(ML_DEBUG, "added track @ " << current_end << " -> "
                  << mStream->StreamTimeToSeconds(current_end));
      }

      // To avoid assertions, we need to insert a dummy segment that covers up
      // to the "start" time for the track
      segment_->AppendNullData(current_ticks);
      MOZ_MTLOG(ML_DEBUG, "segment_->GetDuration() = " << segment_->GetDuration());
      if (segment_->GetType() == MediaSegment::AUDIO) {
        MOZ_MTLOG(ML_DEBUG, "Calling AddAudioTrack");
        mStream->AsSourceStream()->AddAudioTrack(
            kAudioTrack,
            WEBRTC_DEFAULT_SAMPLE_RATE,
            0,
            static_cast<AudioSegment*>(segment_.forget()));
      } else {
        mStream->AsSourceStream()->AddTrack(kVideoTrack, 0, segment_.forget());
      }

      mStream->AsSourceStream()->SetPullEnabled(true);
      mStream->AsSourceStream()->AdvanceKnownTracksTime(STREAM_TIME_MAX);
    }
   private:
    nsAutoPtr<MediaSegment> segment_;
  };

  aSource->GraphImpl()->AppendMessage(
      MakeUnique<Message>(aSource, Move(aSegment)));
  MOZ_MTLOG(ML_INFO, "Dispatched track-add on stream " << aSource);
}

void
TransceiverImpl::StartReceiveStream()
{
  MOZ_MTLOG(ML_DEBUG, mPCHandle << "[" << mMid << "]: " << __FUNCTION__);
  // TODO: Can this be simplified? There's an awful lot of moving pieces here.
  SourceMediaStream* source(mReceiveStream->GetInputStream()->AsSourceStream());
  mReceiveStream->SetLogicalStreamStartTime(
      mReceiveStream->GetPlaybackStream()->GetCurrentTime());

  nsAutoPtr<MediaSegment> segment;
  if (IsVideo()) {
    segment = new VideoSegment;
  } else {
    segment = new AudioSegment;
  }

  StartTrack(source, Move(segment));
}

void
TransceiverImpl::Stop()
{
  mTransmitPipeline->Stop();
  mTransmitPipeline->DetachMedia();
  mReceivePipeline->Stop();
  mReceivePipeline->DetachMedia();
  // Make sure that stats queries stop working on this transceiver.
  UpdateSendTrack(nullptr);
  mHaveStartedReceiving = false;
}

bool
TransceiverImpl::IsVideo() const
{
  return mJsepTransceiver->GetMediaType() == SdpMediaSection::MediaType::kVideo;
}

void TransceiverImpl::GetRtpSources(const int64_t aTimeNow,
    nsTArray<dom::RTCRtpSourceEntry>& outSources) const
{
  if (IsVideo()) {
    return;
  }
  WebrtcAudioConduit *audio_conduit =
    static_cast<WebrtcAudioConduit*>(mConduit.get());
  audio_conduit->GetRtpSources(aTimeNow, outSources);
}


void TransceiverImpl::InsertAudioLevelForContributingSource(uint32_t aSource,
                                                            int64_t aTimestamp,
                                                            bool aHasLevel,
                                                            uint8_t aLevel)
{
  if (IsVideo()) {
    return;
  }
  WebrtcAudioConduit *audio_conduit =
    static_cast<WebrtcAudioConduit*>(mConduit.get());
  audio_conduit->InsertAudioLevelForContributingSource(aSource,
                                                       aTimestamp,
                                                       aHasLevel,
                                                       aLevel);
}

} // namespace mozilla
