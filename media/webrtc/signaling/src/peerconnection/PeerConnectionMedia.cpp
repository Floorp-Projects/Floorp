/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ostream>
#include <string>
#include <vector>

#include "CSFLog.h"

#include "nspr.h"

#include "nricectx.h"
#include "nricemediastream.h"
#include "MediaPipelineFactory.h"
#include "PeerConnectionImpl.h"
#include "PeerConnectionMedia.h"
#include "AudioConduit.h"
#include "VideoConduit.h"
#include "runnable_utils.h"
#include "transportlayerice.h"
#include "transportlayerdtls.h"
#include "signaling/src/jsep/JsepSession.h"
#include "signaling/src/jsep/JsepTransport.h"

#ifdef USE_FAKE_STREAMS
#include "DOMMediaStream.h"
#include "FakeMediaStreams.h"
#else
#include "MediaSegment.h"
#ifdef MOZILLA_INTERNAL_API
#include "MediaStreamGraph.h"
#endif
#endif

#ifndef USE_FAKE_MEDIA_STREAMS
#include "MediaStreamGraphImpl.h"
#endif

#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsIURI.h"
#include "nsIScriptSecurityManager.h"
#include "nsICancelable.h"
#include "nsILoadInfo.h"
#include "nsIContentPolicy.h"
#include "nsIProxyInfo.h"
#include "nsIProtocolProxyService.h"

#include "nsProxyRelease.h"

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
#include "MediaStreamList.h"
#include "nsIScriptGlobalObject.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/RTCStatsReportBinding.h"
#include "MediaStreamTrack.h"
#include "VideoStreamTrack.h"
#include "MediaStreamError.h"
#include "MediaManager.h"
#endif



namespace mozilla {
using namespace dom;

static const char* logTag = "PeerConnectionMedia";

//XXX(pkerr) What about bitrate settings? Going with the defaults for now.
RefPtr<WebRtcCallWrapper>
CreateCall()
{
  WebRtcCallWrapper::Config call_config;
  return WebRtcCallWrapper::Create(call_config);
}

nsresult
PeerConnectionMedia::ReplaceTrack(const std::string& aOldStreamId,
                                  const std::string& aOldTrackId,
                                  MediaStreamTrack& aNewTrack,
                                  const std::string& aNewStreamId,
                                  const std::string& aNewTrackId)
{
  RefPtr<LocalSourceStreamInfo> oldInfo(GetLocalStreamById(aOldStreamId));

  if (!oldInfo) {
    CSFLogError(logTag, "Failed to find stream id %s", aOldStreamId.c_str());
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsresult rv = AddTrack(*aNewTrack.mOwningStream, aNewStreamId,
                         aNewTrack, aNewTrackId);
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<LocalSourceStreamInfo> newInfo(GetLocalStreamById(aNewStreamId));

  if (!newInfo) {
    CSFLogError(logTag, "Failed to add track id %s", aNewTrackId.c_str());
    MOZ_ASSERT(false);
    return NS_ERROR_FAILURE;
  }

  rv = newInfo->TakePipelineFrom(oldInfo, aOldTrackId, aNewTrack, aNewTrackId);
  NS_ENSURE_SUCCESS(rv, rv);

  return RemoveLocalTrack(aOldStreamId, aOldTrackId);
}

static void
PipelineReleaseRef_m(RefPtr<MediaPipeline> pipeline)
{}

static void
PipelineDetachTransport_s(RefPtr<MediaPipeline> pipeline,
                          nsCOMPtr<nsIThread> mainThread)
{
  pipeline->DetachTransport_s();
  mainThread->Dispatch(
      // Make sure we let go of our reference before dispatching
      // If the dispatch fails, well, we're hosed anyway.
      WrapRunnableNM(PipelineReleaseRef_m, pipeline.forget()),
      NS_DISPATCH_NORMAL);
}

void
SourceStreamInfo::EndTrack(MediaStream* stream, dom::MediaStreamTrack* track)
{
  if (!stream || !stream->AsSourceStream()) {
    return;
  }

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  class Message : public ControlMessage {
   public:
    Message(MediaStream* stream, TrackID track)
      : ControlMessage(stream),
        track_id_(track) {}

    virtual void Run() override {
      mStream->AsSourceStream()->EndTrack(track_id_);
    }
   private:
    TrackID track_id_;
  };

  stream->GraphImpl()->AppendMessage(
      MakeUnique<Message>(stream, track->mTrackID));
#endif

}

void
SourceStreamInfo::RemoveTrack(const std::string& trackId)
{
  mTracks.erase(trackId);

  RefPtr<MediaPipeline> pipeline = GetPipelineByTrackId_m(trackId);
  if (pipeline) {
    mPipelines.erase(trackId);
    pipeline->ShutdownMedia_m();
    mParent->GetSTSThread()->Dispatch(
        WrapRunnableNM(PipelineDetachTransport_s,
                       pipeline.forget(),
                       mParent->GetMainThread()),
        NS_DISPATCH_NORMAL);
  }
}

void SourceStreamInfo::DetachTransport_s()
{
  ASSERT_ON_THREAD(mParent->GetSTSThread());
  // walk through all the MediaPipelines and call the shutdown
  // transport functions. Must be on the STS thread.
  for (auto it = mPipelines.begin(); it != mPipelines.end(); ++it) {
    it->second->DetachTransport_s();
  }
}

void SourceStreamInfo::DetachMedia_m()
{
  ASSERT_ON_THREAD(mParent->GetMainThread());

  // walk through all the MediaPipelines and call the shutdown
  // media functions. Must be on the main thread.
  for (auto it = mPipelines.begin(); it != mPipelines.end(); ++it) {
    it->second->ShutdownMedia_m();
  }
  mMediaStream = nullptr;
}

already_AddRefed<PeerConnectionImpl>
PeerConnectionImpl::Constructor(const dom::GlobalObject& aGlobal, ErrorResult& rv)
{
  RefPtr<PeerConnectionImpl> pc = new PeerConnectionImpl(&aGlobal);

  CSFLogDebug(logTag, "Created PeerConnection: %p", pc.get());

  return pc.forget();
}

PeerConnectionImpl* PeerConnectionImpl::CreatePeerConnection()
{
  PeerConnectionImpl *pc = new PeerConnectionImpl();

  CSFLogDebug(logTag, "Created PeerConnection: %p", pc);

  return pc;
}

NS_IMETHODIMP PeerConnectionMedia::ProtocolProxyQueryHandler::
OnProxyAvailable(nsICancelable *request,
                 nsIChannel *aChannel,
                 nsIProxyInfo *proxyinfo,
                 nsresult result) {

  if (!pcm_->mProxyRequest) {
    // PeerConnectionMedia is no longer waiting
    return NS_OK;
  }

  CSFLogInfo(logTag, "%s: Proxy Available: %d", __FUNCTION__, (int)result);

  if (NS_SUCCEEDED(result) && proxyinfo) {
    SetProxyOnPcm(*proxyinfo);
  }

  pcm_->mProxyResolveCompleted = true;
  pcm_->mProxyRequest = nullptr;
  pcm_->FlushIceCtxOperationQueueIfReady();

  return NS_OK;
}

void
PeerConnectionMedia::ProtocolProxyQueryHandler::SetProxyOnPcm(
    nsIProxyInfo& proxyinfo)
{
  CSFLogInfo(logTag, "%s: Had proxyinfo", __FUNCTION__);
  nsresult rv;
  nsCString httpsProxyHost;
  int32_t httpsProxyPort;

  rv = proxyinfo.GetHost(httpsProxyHost);
  if (NS_FAILED(rv)) {
    CSFLogError(logTag, "%s: Failed to get proxy server host", __FUNCTION__);
    return;
  }

  rv = proxyinfo.GetPort(&httpsProxyPort);
  if (NS_FAILED(rv)) {
    CSFLogError(logTag, "%s: Failed to get proxy server port", __FUNCTION__);
    return;
  }

  if (pcm_->mIceCtxHdlr.get()) {
    assert(httpsProxyPort >= 0 && httpsProxyPort < (1 << 16));
    // Note that this could check if PrivacyRequested() is set on the PC and
    // remove "webrtc" from the ALPN list.  But that would only work if the PC
    // was constructed with a peerIdentity constraint, not when isolated
    // streams are added.  If we ever need to signal to the proxy that the
    // media is isolated, then we would need to restructure this code.
    pcm_->mProxyServer.reset(
      new NrIceProxyServer(httpsProxyHost.get(),
                           static_cast<uint16_t>(httpsProxyPort),
                           "webrtc,c-webrtc"));
  } else {
    CSFLogError(logTag, "%s: Failed to set proxy server (ICE ctx unavailable)",
        __FUNCTION__);
  }
}

NS_IMPL_ISUPPORTS(PeerConnectionMedia::ProtocolProxyQueryHandler, nsIProtocolProxyCallback)

PeerConnectionMedia::PeerConnectionMedia(PeerConnectionImpl *parent)
    : mParent(parent),
      mParentHandle(parent->GetHandle()),
      mParentName(parent->GetName()),
      mIceCtxHdlr(nullptr),
      mDNSResolver(new NrIceResolver()),
      mUuidGen(MakeUnique<PCUuidGenerator>()),
      mMainThread(mParent->GetMainThread()),
      mSTSThread(mParent->GetSTSThread()),
      mProxyResolveCompleted(false),
      mIceRestartState(ICE_RESTART_NONE) {
}

nsresult
PeerConnectionMedia::InitProxy()
{
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  // Allow mochitests to disable this, since mochitest configures a fake proxy
  // that serves up content.
  bool disable = Preferences::GetBool("media.peerconnection.disable_http_proxy",
                                      false);
  if (disable) {
    mProxyResolveCompleted = true;
    return NS_OK;
  }
#endif

  nsresult rv;
  nsCOMPtr<nsIProtocolProxyService> pps =
    do_GetService(NS_PROTOCOLPROXYSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    CSFLogError(logTag, "%s: Failed to get proxy service: %d", __FUNCTION__, (int)rv);
    return NS_ERROR_FAILURE;
  }

  // We use the following URL to find the "default" proxy address for all HTTPS
  // connections.  We will only attempt one HTTP(S) CONNECT per peer connection.
  // "example.com" is guaranteed to be unallocated and should return the best default.
  nsCOMPtr<nsIURI> fakeHttpsLocation;
  rv = NS_NewURI(getter_AddRefs(fakeHttpsLocation), "https://example.com");
  if (NS_FAILED(rv)) {
    CSFLogError(logTag, "%s: Failed to set URI: %d", __FUNCTION__, (int)rv);
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIScriptSecurityManager> secMan(
      do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv));
  if (NS_FAILED(rv)) {
    CSFLogError(logTag, "%s: Failed to get IOService: %d",
        __FUNCTION__, (int)rv);
    CSFLogError(logTag, "%s: Failed to get securityManager: %d", __FUNCTION__, (int)rv);
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIPrincipal> systemPrincipal;
  rv = secMan->GetSystemPrincipal(getter_AddRefs(systemPrincipal));
  if (NS_FAILED(rv)) {
    CSFLogError(logTag, "%s: Failed to get systemPrincipal: %d", __FUNCTION__, (int)rv);
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel),
                     fakeHttpsLocation,
                     systemPrincipal,
                     nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
                     nsIContentPolicy::TYPE_OTHER);

  if (NS_FAILED(rv)) {
    CSFLogError(logTag, "%s: Failed to get channel from URI: %d",
                __FUNCTION__, (int)rv);
    return NS_ERROR_FAILURE;
  }

  RefPtr<ProtocolProxyQueryHandler> handler = new ProtocolProxyQueryHandler(this);
  rv = pps->AsyncResolve(channel,
                         nsIProtocolProxyService::RESOLVE_PREFER_HTTPS_PROXY |
                         nsIProtocolProxyService::RESOLVE_ALWAYS_TUNNEL,
                         handler, getter_AddRefs(mProxyRequest));
  if (NS_FAILED(rv)) {
    CSFLogError(logTag, "%s: Failed to resolve protocol proxy: %d", __FUNCTION__, (int)rv);
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult PeerConnectionMedia::Init(const std::vector<NrIceStunServer>& stun_servers,
                                   const std::vector<NrIceTurnServer>& turn_servers,
                                   NrIceCtx::Policy policy)
{
  nsresult rv = InitProxy();
  NS_ENSURE_SUCCESS(rv, rv);

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  bool ice_tcp = Preferences::GetBool("media.peerconnection.ice.tcp", false);
#else
  bool ice_tcp = false;
#endif

  // TODO(ekr@rtfm.com): need some way to set not offerer later
  // Looks like a bug in the NrIceCtx API.
  mIceCtxHdlr = NrIceCtxHandler::Create("PC:" + mParentName,
                                        true, // Offerer
                                        mParent->GetAllowIceLoopback(),
                                        ice_tcp,
                                        mParent->GetAllowIceLinkLocal(),
                                        policy);
  if(!mIceCtxHdlr) {
    CSFLogError(logTag, "%s: Failed to create Ice Context", __FUNCTION__);
    return NS_ERROR_FAILURE;
  }

  if (NS_FAILED(rv = mIceCtxHdlr->ctx()->SetStunServers(stun_servers))) {
    CSFLogError(logTag, "%s: Failed to set stun servers", __FUNCTION__);
    return rv;
  }
  // Give us a way to globally turn off TURN support
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  bool disabled = Preferences::GetBool("media.peerconnection.turn.disable", false);
#else
  bool disabled = false;
#endif
  if (!disabled) {
    if (NS_FAILED(rv = mIceCtxHdlr->ctx()->SetTurnServers(turn_servers))) {
      CSFLogError(logTag, "%s: Failed to set turn servers", __FUNCTION__);
      return rv;
    }
  } else if (turn_servers.size() != 0) {
    CSFLogError(logTag, "%s: Setting turn servers disabled", __FUNCTION__);
  }
  if (NS_FAILED(rv = mDNSResolver->Init())) {
    CSFLogError(logTag, "%s: Failed to initialize dns resolver", __FUNCTION__);
    return rv;
  }
  if (NS_FAILED(rv =
      mIceCtxHdlr->ctx()->SetResolver(mDNSResolver->AllocateResolver()))) {
    CSFLogError(logTag, "%s: Failed to get dns resolver", __FUNCTION__);
    return rv;
  }
  ConnectSignals(mIceCtxHdlr->ctx().get());

  // This webrtc:Call instance will be shared by audio and video media conduits.
  mCall = CreateCall();

  return NS_OK;
}

void
PeerConnectionMedia::EnsureTransports(const JsepSession& aSession)
{
  auto transports = aSession.GetTransports();
  for (size_t i = 0; i < transports.size(); ++i) {
    RefPtr<JsepTransport> transport = transports[i];
    RUN_ON_THREAD(
        GetSTSThread(),
        WrapRunnable(RefPtr<PeerConnectionMedia>(this),
                     &PeerConnectionMedia::EnsureTransport_s,
                     i,
                     transport->mComponents),
        NS_DISPATCH_NORMAL);
  }

  GatherIfReady();
}

void
PeerConnectionMedia::EnsureTransport_s(size_t aLevel, size_t aComponentCount)
{
  RefPtr<NrIceMediaStream> stream(mIceCtxHdlr->ctx()->GetStream(aLevel));
  if (!stream) {
    CSFLogDebug(logTag, "%s: Creating ICE media stream=%u components=%u",
                mParentHandle.c_str(),
                static_cast<unsigned>(aLevel),
                static_cast<unsigned>(aComponentCount));

    std::ostringstream os;
    os << mParentName << " aLevel=" << aLevel;
    RefPtr<NrIceMediaStream> stream =
      mIceCtxHdlr->CreateStream(os.str().c_str(),
                                aComponentCount);

    if (!stream) {
      CSFLogError(logTag, "Failed to create ICE stream.");
      return;
    }

    stream->SetLevel(aLevel);
    stream->SignalReady.connect(this, &PeerConnectionMedia::IceStreamReady_s);
    stream->SignalCandidate.connect(this,
                                    &PeerConnectionMedia::OnCandidateFound_s);
    mIceCtxHdlr->ctx()->SetStream(aLevel, stream);
  }
}

void
PeerConnectionMedia::ActivateOrRemoveTransports(const JsepSession& aSession)
{
  auto transports = aSession.GetTransports();
  for (size_t i = 0; i < transports.size(); ++i) {
    RefPtr<JsepTransport> transport = transports[i];

    std::string ufrag;
    std::string pwd;
    std::vector<std::string> candidates;

    if (transport->mComponents) {
      MOZ_ASSERT(transport->mIce);
      CSFLogDebug(logTag, "Transport %u is active", static_cast<unsigned>(i));
      ufrag = transport->mIce->GetUfrag();
      pwd = transport->mIce->GetPassword();
      candidates = transport->mIce->GetCandidates();
    } else {
      CSFLogDebug(logTag, "Transport %u is disabled", static_cast<unsigned>(i));
      // Make sure the MediaPipelineFactory doesn't try to use these.
      RemoveTransportFlow(i, false);
      RemoveTransportFlow(i, true);
    }

    RUN_ON_THREAD(
        GetSTSThread(),
        WrapRunnable(RefPtr<PeerConnectionMedia>(this),
                     &PeerConnectionMedia::ActivateOrRemoveTransport_s,
                     i,
                     transport->mComponents,
                     ufrag,
                     pwd,
                     candidates),
        NS_DISPATCH_NORMAL);
  }

  // We can have more streams than m-lines due to rollback.
  RUN_ON_THREAD(
      GetSTSThread(),
      WrapRunnable(RefPtr<PeerConnectionMedia>(this),
                   &PeerConnectionMedia::RemoveTransportsAtOrAfter_s,
                   transports.size()),
      NS_DISPATCH_NORMAL);
}

void
PeerConnectionMedia::ActivateOrRemoveTransport_s(
    size_t aMLine,
    size_t aComponentCount,
    const std::string& aUfrag,
    const std::string& aPassword,
    const std::vector<std::string>& aCandidateList) {

  if (!aComponentCount) {
    CSFLogDebug(logTag, "%s: Removing ICE media stream=%u",
                mParentHandle.c_str(),
                static_cast<unsigned>(aMLine));
    mIceCtxHdlr->ctx()->SetStream(aMLine, nullptr);
    return;
  }

  RefPtr<NrIceMediaStream> stream(mIceCtxHdlr->ctx()->GetStream(aMLine));
  if (!stream) {
    MOZ_ASSERT(false);
    return;
  }

  if (!stream->HasParsedAttributes()) {
    CSFLogDebug(logTag, "%s: Activating ICE media stream=%u components=%u",
                mParentHandle.c_str(),
                static_cast<unsigned>(aMLine),
                static_cast<unsigned>(aComponentCount));

    std::vector<std::string> attrs;
    for (auto i = aCandidateList.begin(); i != aCandidateList.end(); ++i) {
      attrs.push_back("candidate:" + *i);
    }
    attrs.push_back("ice-ufrag:" + aUfrag);
    attrs.push_back("ice-pwd:" + aPassword);

    nsresult rv = stream->ParseAttributes(attrs);
    if (NS_FAILED(rv)) {
      CSFLogError(logTag, "Couldn't parse ICE attributes, rv=%u",
                          static_cast<unsigned>(rv));
    }

    for (size_t c = aComponentCount; c < stream->components(); ++c) {
      // components are 1-indexed
      stream->DisableComponent(c + 1);
    }
  }
}

void
PeerConnectionMedia::RemoveTransportsAtOrAfter_s(size_t aMLine)
{
  for (size_t i = aMLine; i < mIceCtxHdlr->ctx()->GetStreamCount(); ++i) {
    mIceCtxHdlr->ctx()->SetStream(i, nullptr);
  }
}

nsresult PeerConnectionMedia::UpdateMediaPipelines(
    const JsepSession& session) {
  auto trackPairs = session.GetNegotiatedTrackPairs();
  MediaPipelineFactory factory(this);
  nsresult rv;

  for (auto i = trackPairs.begin(); i != trackPairs.end(); ++i) {
    JsepTrackPair pair = *i;

    if (pair.mReceiving) {

      rv = factory.CreateOrUpdateMediaPipeline(pair, *pair.mReceiving);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }

    if (pair.mSending) {
      rv = factory.CreateOrUpdateMediaPipeline(pair, *pair.mSending);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }

  for (auto& stream : mRemoteSourceStreams) {
    stream->StartReceiving();
  }

  return NS_OK;
}

void
PeerConnectionMedia::StartIceChecks(const JsepSession& aSession)
{
  nsCOMPtr<nsIRunnable> runnable(
      WrapRunnable(
        RefPtr<PeerConnectionMedia>(this),
        &PeerConnectionMedia::StartIceChecks_s,
        aSession.IsIceControlling(),
        aSession.RemoteIsIceLite(),
        // Copy, just in case API changes to return a ref
        std::vector<std::string>(aSession.GetIceOptions())));

  PerformOrEnqueueIceCtxOperation(runnable);
}

void
PeerConnectionMedia::StartIceChecks_s(
    bool aIsControlling,
    bool aIsIceLite,
    const std::vector<std::string>& aIceOptionsList) {

  CSFLogDebug(logTag, "Starting ICE Checking");

  std::vector<std::string> attributes;
  if (aIsIceLite) {
    attributes.push_back("ice-lite");
  }

  if (!aIceOptionsList.empty()) {
    attributes.push_back("ice-options:");
    for (auto i = aIceOptionsList.begin(); i != aIceOptionsList.end(); ++i) {
      attributes.back() += *i + ' ';
    }
  }

  nsresult rv = mIceCtxHdlr->ctx()->ParseGlobalAttributes(attributes);
  if (NS_FAILED(rv)) {
    CSFLogError(logTag, "%s: couldn't parse global parameters", __FUNCTION__ );
  }

  mIceCtxHdlr->ctx()->SetControlling(aIsControlling ?
                                     NrIceCtx::ICE_CONTROLLING :
                                     NrIceCtx::ICE_CONTROLLED);

  mIceCtxHdlr->ctx()->StartChecks();
}

bool
PeerConnectionMedia::IsIceRestarting() const
{
  ASSERT_ON_THREAD(mMainThread);

  return (mIceRestartState != ICE_RESTART_NONE);
}

PeerConnectionMedia::IceRestartState
PeerConnectionMedia::GetIceRestartState() const
{
  ASSERT_ON_THREAD(mMainThread);

  return mIceRestartState;
}

void
PeerConnectionMedia::BeginIceRestart(const std::string& ufrag,
                                     const std::string& pwd)
{
  ASSERT_ON_THREAD(mMainThread);
  if (IsIceRestarting()) {
    return;
  }

  RefPtr<NrIceCtx> new_ctx = mIceCtxHdlr->CreateCtx(ufrag, pwd);

  RUN_ON_THREAD(GetSTSThread(),
                WrapRunnable(
                    RefPtr<PeerConnectionMedia>(this),
                    &PeerConnectionMedia::BeginIceRestart_s,
                    new_ctx),
                NS_DISPATCH_NORMAL);

  mIceRestartState = ICE_RESTART_PROVISIONAL;
}

void
PeerConnectionMedia::BeginIceRestart_s(RefPtr<NrIceCtx> new_ctx)
{
  ASSERT_ON_THREAD(mSTSThread);

  // hold the original context so we can disconnect signals if needed
  RefPtr<NrIceCtx> originalCtx = mIceCtxHdlr->ctx();

  if (mIceCtxHdlr->BeginIceRestart(new_ctx)) {
    ConnectSignals(mIceCtxHdlr->ctx().get(), originalCtx.get());
  }
}

void
PeerConnectionMedia::CommitIceRestart()
{
  ASSERT_ON_THREAD(mMainThread);
  if (mIceRestartState != ICE_RESTART_PROVISIONAL) {
    return;
  }

  mIceRestartState = ICE_RESTART_COMMITTED;
}

void
PeerConnectionMedia::FinalizeIceRestart()
{
  ASSERT_ON_THREAD(mMainThread);
  if (!IsIceRestarting()) {
    return;
  }

  RUN_ON_THREAD(GetSTSThread(),
                WrapRunnable(
                    RefPtr<PeerConnectionMedia>(this),
                    &PeerConnectionMedia::FinalizeIceRestart_s),
                NS_DISPATCH_NORMAL);

  mIceRestartState = ICE_RESTART_NONE;
}

void
PeerConnectionMedia::FinalizeIceRestart_s()
{
  ASSERT_ON_THREAD(mSTSThread);

  // reset old streams since we don't need them anymore
  for (auto i = mTransportFlows.begin();
       i != mTransportFlows.end();
       ++i) {
    RefPtr<TransportFlow> aFlow = i->second;
    if (!aFlow) continue;
    TransportLayerIce* ice =
      static_cast<TransportLayerIce*>(aFlow->GetLayer(TransportLayerIce::ID()));
    ice->ResetOldStream();
  }

  mIceCtxHdlr->FinalizeIceRestart();
}

void
PeerConnectionMedia::RollbackIceRestart()
{
  ASSERT_ON_THREAD(mMainThread);
  if (mIceRestartState != ICE_RESTART_PROVISIONAL) {
    return;
  }

  RUN_ON_THREAD(GetSTSThread(),
                WrapRunnable(
                    RefPtr<PeerConnectionMedia>(this),
                    &PeerConnectionMedia::RollbackIceRestart_s),
                NS_DISPATCH_NORMAL);

  mIceRestartState = ICE_RESTART_NONE;
}

void
PeerConnectionMedia::RollbackIceRestart_s()
{
  ASSERT_ON_THREAD(mSTSThread);

  // hold the restart context so we can disconnect signals
  RefPtr<NrIceCtx> restartCtx = mIceCtxHdlr->ctx();

  // restore old streams since we're rolling back
  for (auto i = mTransportFlows.begin();
       i != mTransportFlows.end();
       ++i) {
    RefPtr<TransportFlow> aFlow = i->second;
    if (!aFlow) continue;
    TransportLayerIce* ice =
      static_cast<TransportLayerIce*>(aFlow->GetLayer(TransportLayerIce::ID()));
    ice->RestoreOldStream();
  }

  mIceCtxHdlr->RollbackIceRestart();
  ConnectSignals(mIceCtxHdlr->ctx().get(), restartCtx.get());
}

bool
PeerConnectionMedia::GetPrefDefaultAddressOnly() const
{
  ASSERT_ON_THREAD(mMainThread); // will crash on STS thread

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  uint64_t winId = mParent->GetWindow()->WindowID();

  bool default_address_only = Preferences::GetBool(
    "media.peerconnection.ice.default_address_only", false);
  default_address_only |=
    !MediaManager::Get()->IsActivelyCapturingOrHasAPermission(winId);
#else
  bool default_address_only = true;
#endif
  return default_address_only;
}

bool
PeerConnectionMedia::GetPrefProxyOnly() const
{
  ASSERT_ON_THREAD(mMainThread); // will crash on STS thread

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  return Preferences::GetBool("media.peerconnection.ice.proxy_only", false);
#else
  return false;
#endif
}

void
PeerConnectionMedia::ConnectSignals(NrIceCtx *aCtx, NrIceCtx *aOldCtx)
{
  aCtx->SignalGatheringStateChange.connect(
      this,
      &PeerConnectionMedia::IceGatheringStateChange_s);
  aCtx->SignalConnectionStateChange.connect(
      this,
      &PeerConnectionMedia::IceConnectionStateChange_s);

  if (aOldCtx) {
    MOZ_ASSERT(aCtx != aOldCtx);
    aOldCtx->SignalGatheringStateChange.disconnect(this);
    aOldCtx->SignalConnectionStateChange.disconnect(this);

    // if the old and new connection state and/or gathering state is
    // different fire the state update.  Note: we don't fire the update
    // if the state is *INIT since updates for the INIT state aren't
    // sent during the normal flow. (mjf)
    if (aOldCtx->connection_state() != aCtx->connection_state() &&
        aCtx->connection_state() != NrIceCtx::ICE_CTX_INIT) {
      aCtx->SignalConnectionStateChange(aCtx, aCtx->connection_state());
    }

    if (aOldCtx->gathering_state() != aCtx->gathering_state() &&
        aCtx->gathering_state() != NrIceCtx::ICE_CTX_GATHER_INIT) {
      aCtx->SignalGatheringStateChange(aCtx, aCtx->gathering_state());
    }
  }
}

void
PeerConnectionMedia::AddIceCandidate(const std::string& candidate,
                                     const std::string& mid,
                                     uint32_t aMLine) {
  RUN_ON_THREAD(GetSTSThread(),
                WrapRunnable(
                    RefPtr<PeerConnectionMedia>(this),
                    &PeerConnectionMedia::AddIceCandidate_s,
                    std::string(candidate), // Make copies.
                    std::string(mid),
                    aMLine),
                NS_DISPATCH_NORMAL);
}
void
PeerConnectionMedia::AddIceCandidate_s(const std::string& aCandidate,
                                       const std::string& aMid,
                                       uint32_t aMLine) {
  RefPtr<NrIceMediaStream> stream(mIceCtxHdlr->ctx()->GetStream(aMLine));
  if (!stream) {
    CSFLogError(logTag, "No ICE stream for candidate at level %u: %s",
                        static_cast<unsigned>(aMLine), aCandidate.c_str());
    return;
  }

  nsresult rv = stream->ParseTrickleCandidate(aCandidate);
  if (NS_FAILED(rv)) {
    CSFLogError(logTag, "Couldn't process ICE candidate at level %u",
                static_cast<unsigned>(aMLine));
    return;
  }
}

void
PeerConnectionMedia::FlushIceCtxOperationQueueIfReady()
{
  ASSERT_ON_THREAD(mMainThread);

  if (IsIceCtxReady()) {
    for (auto i = mQueuedIceCtxOperations.begin();
         i != mQueuedIceCtxOperations.end();
         ++i) {
      GetSTSThread()->Dispatch(*i, NS_DISPATCH_NORMAL);
    }
    mQueuedIceCtxOperations.clear();
  }
}

void
PeerConnectionMedia::PerformOrEnqueueIceCtxOperation(nsIRunnable* runnable)
{
  ASSERT_ON_THREAD(mMainThread);

  if (IsIceCtxReady()) {
    GetSTSThread()->Dispatch(runnable, NS_DISPATCH_NORMAL);
  } else {
    mQueuedIceCtxOperations.push_back(runnable);
  }
}

void
PeerConnectionMedia::GatherIfReady() {
  ASSERT_ON_THREAD(mMainThread);

  nsCOMPtr<nsIRunnable> runnable(WrapRunnable(
        RefPtr<PeerConnectionMedia>(this),
        &PeerConnectionMedia::EnsureIceGathering_s,
        GetPrefDefaultAddressOnly(),
        GetPrefProxyOnly()));

  PerformOrEnqueueIceCtxOperation(runnable);
}

void
PeerConnectionMedia::EnsureIceGathering_s(bool aDefaultRouteOnly,
                                          bool aProxyOnly) {
  if (mProxyServer) {
    mIceCtxHdlr->ctx()->SetProxyServer(*mProxyServer);
  } else if (aProxyOnly) {
    IceGatheringStateChange_s(mIceCtxHdlr->ctx().get(),
                              NrIceCtx::ICE_CTX_GATHER_COMPLETE);
    return;
  }

  // Start gathering, but only if there are streams
  for (size_t i = 0; i < mIceCtxHdlr->ctx()->GetStreamCount(); ++i) {
    if (mIceCtxHdlr->ctx()->GetStream(i)) {
      mIceCtxHdlr->ctx()->StartGathering(aDefaultRouteOnly, aProxyOnly);
      return;
    }
  }

  // If there are no streams, we're probably in a situation where we've rolled
  // back while still waiting for our proxy configuration to come back. Make
  // sure content knows that the rollback has stuck wrt gathering.
  IceGatheringStateChange_s(mIceCtxHdlr->ctx().get(),
                            NrIceCtx::ICE_CTX_GATHER_COMPLETE);
}

nsresult
PeerConnectionMedia::AddTrack(DOMMediaStream& aMediaStream,
                              const std::string& streamId,
                              MediaStreamTrack& aTrack,
                              const std::string& trackId)
{
  ASSERT_ON_THREAD(mMainThread);

  CSFLogDebug(logTag, "%s: MediaStream: %p", __FUNCTION__, &aMediaStream);

  RefPtr<LocalSourceStreamInfo> localSourceStream =
    GetLocalStreamById(streamId);

  if (!localSourceStream) {
    localSourceStream = new LocalSourceStreamInfo(&aMediaStream, this, streamId);
    mLocalSourceStreams.AppendElement(localSourceStream);
  }

  localSourceStream->AddTrack(trackId, &aTrack);
  return NS_OK;
}

nsresult
PeerConnectionMedia::RemoveLocalTrack(const std::string& streamId,
                                      const std::string& trackId)
{
  ASSERT_ON_THREAD(mMainThread);

  CSFLogDebug(logTag, "%s: stream: %s track: %s", __FUNCTION__,
                      streamId.c_str(), trackId.c_str());

  RefPtr<LocalSourceStreamInfo> localSourceStream =
    GetLocalStreamById(streamId);
  if (!localSourceStream) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  localSourceStream->RemoveTrack(trackId);
  if (!localSourceStream->GetTrackCount()) {
    mLocalSourceStreams.RemoveElement(localSourceStream);
  }
  return NS_OK;
}

nsresult
PeerConnectionMedia::RemoveRemoteTrack(const std::string& streamId,
                                       const std::string& trackId)
{
  ASSERT_ON_THREAD(mMainThread);

  CSFLogDebug(logTag, "%s: stream: %s track: %s", __FUNCTION__,
                      streamId.c_str(), trackId.c_str());

  RefPtr<RemoteSourceStreamInfo> remoteSourceStream =
    GetRemoteStreamById(streamId);
  if (!remoteSourceStream) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  remoteSourceStream->RemoveTrack(trackId);
  if (!remoteSourceStream->GetTrackCount()) {
    mRemoteSourceStreams.RemoveElement(remoteSourceStream);
  }
  return NS_OK;
}

void
PeerConnectionMedia::SelfDestruct()
{
  ASSERT_ON_THREAD(mMainThread);

  CSFLogDebug(logTag, "%s: ", __FUNCTION__);

  // Shut down the media
  for (uint32_t i=0; i < mLocalSourceStreams.Length(); ++i) {
    mLocalSourceStreams[i]->DetachMedia_m();
  }

  for (uint32_t i=0; i < mRemoteSourceStreams.Length(); ++i) {
    mRemoteSourceStreams[i]->DetachMedia_m();
  }

  if (mProxyRequest) {
    mProxyRequest->Cancel(NS_ERROR_ABORT);
    mProxyRequest = nullptr;
  }

  // Shutdown the transport (async)
  RUN_ON_THREAD(mSTSThread, WrapRunnable(
      this, &PeerConnectionMedia::ShutdownMediaTransport_s),
                NS_DISPATCH_NORMAL);

  CSFLogDebug(logTag, "%s: Media shut down", __FUNCTION__);
}

void
PeerConnectionMedia::SelfDestruct_m()
{
  CSFLogDebug(logTag, "%s: ", __FUNCTION__);

  ASSERT_ON_THREAD(mMainThread);

  mLocalSourceStreams.Clear();
  mRemoteSourceStreams.Clear();

  mMainThread = nullptr;

  // Final self-destruct.
  this->Release();
}

void
PeerConnectionMedia::ShutdownMediaTransport_s()
{
  ASSERT_ON_THREAD(mSTSThread);

  CSFLogDebug(logTag, "%s: ", __FUNCTION__);

  // Here we access m{Local|Remote}SourceStreams off the main thread.
  // That's OK because by here PeerConnectionImpl has forgotten about us,
  // so there is no chance of getting a call in here from outside.
  // The dispatches from SelfDestruct() and to SelfDestruct_m() provide
  // memory barriers that protect us from badness.
  for (uint32_t i=0; i < mLocalSourceStreams.Length(); ++i) {
    mLocalSourceStreams[i]->DetachTransport_s();
  }

  for (uint32_t i=0; i < mRemoteSourceStreams.Length(); ++i) {
    mRemoteSourceStreams[i]->DetachTransport_s();
  }

  disconnect_all();
  mTransportFlows.clear();
  mIceCtxHdlr = nullptr;

  mMainThread->Dispatch(WrapRunnable(this, &PeerConnectionMedia::SelfDestruct_m),
                        NS_DISPATCH_NORMAL);
}

LocalSourceStreamInfo*
PeerConnectionMedia::GetLocalStreamByIndex(int aIndex)
{
  ASSERT_ON_THREAD(mMainThread);
  if(aIndex < 0 || aIndex >= (int) mLocalSourceStreams.Length()) {
    return nullptr;
  }

  MOZ_ASSERT(mLocalSourceStreams[aIndex]);
  return mLocalSourceStreams[aIndex];
}

LocalSourceStreamInfo*
PeerConnectionMedia::GetLocalStreamById(const std::string& id)
{
  ASSERT_ON_THREAD(mMainThread);
  for (size_t i = 0; i < mLocalSourceStreams.Length(); ++i) {
    if (id == mLocalSourceStreams[i]->GetId()) {
      return mLocalSourceStreams[i];
    }
  }

  return nullptr;
}

LocalSourceStreamInfo*
PeerConnectionMedia::GetLocalStreamByTrackId(const std::string& id)
{
  ASSERT_ON_THREAD(mMainThread);
  for (RefPtr<LocalSourceStreamInfo>& info : mLocalSourceStreams) {
    if (info->HasTrack(id)) {
      return info;
    }
  }

  return nullptr;
}

RemoteSourceStreamInfo*
PeerConnectionMedia::GetRemoteStreamByIndex(size_t aIndex)
{
  ASSERT_ON_THREAD(mMainThread);
  MOZ_ASSERT(mRemoteSourceStreams.SafeElementAt(aIndex));
  return mRemoteSourceStreams.SafeElementAt(aIndex);
}

RemoteSourceStreamInfo*
PeerConnectionMedia::GetRemoteStreamById(const std::string& id)
{
  ASSERT_ON_THREAD(mMainThread);
  for (size_t i = 0; i < mRemoteSourceStreams.Length(); ++i) {
    if (id == mRemoteSourceStreams[i]->GetId()) {
      return mRemoteSourceStreams[i];
    }
  }

  return nullptr;
}

RemoteSourceStreamInfo*
PeerConnectionMedia::GetRemoteStreamByTrackId(const std::string& id)
{
  ASSERT_ON_THREAD(mMainThread);
  for (RefPtr<RemoteSourceStreamInfo>& info : mRemoteSourceStreams) {
    if (info->HasTrack(id)) {
      return info;
    }
  }

  return nullptr;
}


nsresult
PeerConnectionMedia::AddRemoteStream(RefPtr<RemoteSourceStreamInfo> aInfo)
{
  ASSERT_ON_THREAD(mMainThread);

  mRemoteSourceStreams.AppendElement(aInfo);

  return NS_OK;
}

void
PeerConnectionMedia::IceGatheringStateChange_s(NrIceCtx* ctx,
                                               NrIceCtx::GatheringState state)
{
  ASSERT_ON_THREAD(mSTSThread);

  if (state == NrIceCtx::ICE_CTX_GATHER_COMPLETE) {
    // Fire off EndOfLocalCandidates for each stream
    for (size_t i = 0; ; ++i) {
      RefPtr<NrIceMediaStream> stream(ctx->GetStream(i));
      if (!stream) {
        break;
      }

      NrIceCandidate candidate;
      NrIceCandidate rtcpCandidate;
      GetDefaultCandidates(*stream, &candidate, &rtcpCandidate);
      EndOfLocalCandidates(candidate.cand_addr.host,
                           candidate.cand_addr.port,
                           rtcpCandidate.cand_addr.host,
                           rtcpCandidate.cand_addr.port,
                           i);
    }
  }

  // ShutdownMediaTransport_s has not run yet because it unhooks this function
  // from its signal, which means that SelfDestruct_m has not been dispatched
  // yet either, so this PCMedia will still be around when this dispatch reaches
  // main.
  GetMainThread()->Dispatch(
    WrapRunnable(this,
                 &PeerConnectionMedia::IceGatheringStateChange_m,
                 ctx,
                 state),
    NS_DISPATCH_NORMAL);
}

void
PeerConnectionMedia::IceConnectionStateChange_s(NrIceCtx* ctx,
                                                NrIceCtx::ConnectionState state)
{
  ASSERT_ON_THREAD(mSTSThread);
  // ShutdownMediaTransport_s has not run yet because it unhooks this function
  // from its signal, which means that SelfDestruct_m has not been dispatched
  // yet either, so this PCMedia will still be around when this dispatch reaches
  // main.
  GetMainThread()->Dispatch(
    WrapRunnable(this,
                 &PeerConnectionMedia::IceConnectionStateChange_m,
                 ctx,
                 state),
    NS_DISPATCH_NORMAL);
}

void
PeerConnectionMedia::OnCandidateFound_s(NrIceMediaStream *aStream,
                                        const std::string &aCandidateLine)
{
  ASSERT_ON_THREAD(mSTSThread);
  MOZ_ASSERT(aStream);
  MOZ_RELEASE_ASSERT(mIceCtxHdlr);

  CSFLogDebug(logTag, "%s: %s", __FUNCTION__, aStream->name().c_str());

  NrIceCandidate candidate;
  NrIceCandidate rtcpCandidate;
  GetDefaultCandidates(*aStream, &candidate, &rtcpCandidate);

  // ShutdownMediaTransport_s has not run yet because it unhooks this function
  // from its signal, which means that SelfDestruct_m has not been dispatched
  // yet either, so this PCMedia will still be around when this dispatch reaches
  // main.
  GetMainThread()->Dispatch(
    WrapRunnable(this,
                 &PeerConnectionMedia::OnCandidateFound_m,
                 aCandidateLine,
                 candidate.cand_addr.host,
                 candidate.cand_addr.port,
                 rtcpCandidate.cand_addr.host,
                 rtcpCandidate.cand_addr.port,
                 aStream->GetLevel()),
    NS_DISPATCH_NORMAL);
}

void
PeerConnectionMedia::EndOfLocalCandidates(const std::string& aDefaultAddr,
                                          uint16_t aDefaultPort,
                                          const std::string& aDefaultRtcpAddr,
                                          uint16_t aDefaultRtcpPort,
                                          uint16_t aMLine)
{
  GetMainThread()->Dispatch(
    WrapRunnable(this,
                 &PeerConnectionMedia::EndOfLocalCandidates_m,
                 aDefaultAddr,
                 aDefaultPort,
                 aDefaultRtcpAddr,
                 aDefaultRtcpPort,
                 aMLine),
    NS_DISPATCH_NORMAL);
}

void
PeerConnectionMedia::GetDefaultCandidates(const NrIceMediaStream& aStream,
                                          NrIceCandidate* aCandidate,
                                          NrIceCandidate* aRtcpCandidate)
{
  nsresult res = aStream.GetDefaultCandidate(1, aCandidate);
  // Optional; component won't exist if doing rtcp-mux
  if (NS_FAILED(aStream.GetDefaultCandidate(2, aRtcpCandidate))) {
    aRtcpCandidate->cand_addr.host.clear();
    aRtcpCandidate->cand_addr.port = 0;
  }
  if (NS_FAILED(res)) {
    aCandidate->cand_addr.host.clear();
    aCandidate->cand_addr.port = 0;
    CSFLogError(logTag, "%s: GetDefaultCandidates failed for level %u, "
                        "res=%u",
                        __FUNCTION__,
                        static_cast<unsigned>(aStream.GetLevel()),
                        static_cast<unsigned>(res));
  }
}

void
PeerConnectionMedia::IceGatheringStateChange_m(NrIceCtx* ctx,
                                               NrIceCtx::GatheringState state)
{
  ASSERT_ON_THREAD(mMainThread);
  SignalIceGatheringStateChange(ctx, state);
}

void
PeerConnectionMedia::IceConnectionStateChange_m(NrIceCtx* ctx,
                                                NrIceCtx::ConnectionState state)
{
  ASSERT_ON_THREAD(mMainThread);
  SignalIceConnectionStateChange(ctx, state);
}

void
PeerConnectionMedia::IceStreamReady_s(NrIceMediaStream *aStream)
{
  MOZ_ASSERT(aStream);

  CSFLogDebug(logTag, "%s: %s", __FUNCTION__, aStream->name().c_str());
}

void
PeerConnectionMedia::OnCandidateFound_m(const std::string& aCandidateLine,
                                        const std::string& aDefaultAddr,
                                        uint16_t aDefaultPort,
                                        const std::string& aDefaultRtcpAddr,
                                        uint16_t aDefaultRtcpPort,
                                        uint16_t aMLine)
{
  ASSERT_ON_THREAD(mMainThread);
  if (!aDefaultAddr.empty()) {
    SignalUpdateDefaultCandidate(aDefaultAddr,
                                 aDefaultPort,
                                 aDefaultRtcpAddr,
                                 aDefaultRtcpPort,
                                 aMLine);
  }
  SignalCandidate(aCandidateLine, aMLine);
}

void
PeerConnectionMedia::EndOfLocalCandidates_m(const std::string& aDefaultAddr,
                                            uint16_t aDefaultPort,
                                            const std::string& aDefaultRtcpAddr,
                                            uint16_t aDefaultRtcpPort,
                                            uint16_t aMLine) {
  ASSERT_ON_THREAD(mMainThread);
  if (!aDefaultAddr.empty()) {
    SignalUpdateDefaultCandidate(aDefaultAddr,
                                 aDefaultPort,
                                 aDefaultRtcpAddr,
                                 aDefaultRtcpPort,
                                 aMLine);
  }
  SignalEndOfLocalCandidates(aMLine);
}

void
PeerConnectionMedia::DtlsConnected_s(TransportLayer *layer,
                                     TransportLayer::State state)
{
  MOZ_ASSERT(layer->id() == "dtls");
  TransportLayerDtls* dtlsLayer = static_cast<TransportLayerDtls*>(layer);
  dtlsLayer->SignalStateChange.disconnect(this);

  bool privacyRequested = (dtlsLayer->GetNegotiatedAlpn() == "c-webrtc");
  GetMainThread()->Dispatch(
    WrapRunnableNM(&PeerConnectionMedia::DtlsConnected_m,
                   mParentHandle, privacyRequested),
    NS_DISPATCH_NORMAL);
}

void
PeerConnectionMedia::DtlsConnected_m(const std::string& aParentHandle,
                                     bool aPrivacyRequested)
{
  PeerConnectionWrapper pcWrapper(aParentHandle);
  PeerConnectionImpl* pc = pcWrapper.impl();
  if (pc) {
    pc->SetDtlsConnected(aPrivacyRequested);
  }
}

void
PeerConnectionMedia::AddTransportFlow(int aIndex, bool aRtcp,
                                      const RefPtr<TransportFlow> &aFlow)
{
  int index_inner = GetTransportFlowIndex(aIndex, aRtcp);

  MOZ_ASSERT(!mTransportFlows[index_inner]);
  mTransportFlows[index_inner] = aFlow;

  GetSTSThread()->Dispatch(
    WrapRunnable(this, &PeerConnectionMedia::ConnectDtlsListener_s, aFlow),
    NS_DISPATCH_NORMAL);
}

void
PeerConnectionMedia::RemoveTransportFlow(int aIndex, bool aRtcp)
{
  int index_inner = GetTransportFlowIndex(aIndex, aRtcp);
  NS_ProxyRelease(GetSTSThread(), mTransportFlows[index_inner].forget());
}

void
PeerConnectionMedia::ConnectDtlsListener_s(const RefPtr<TransportFlow>& aFlow)
{
  TransportLayer* dtls = aFlow->GetLayer(TransportLayerDtls::ID());
  if (dtls) {
    dtls->SignalStateChange.connect(this, &PeerConnectionMedia::DtlsConnected_s);
  }
}

nsresult
LocalSourceStreamInfo::TakePipelineFrom(RefPtr<LocalSourceStreamInfo>& info,
                                        const std::string& oldTrackId,
                                        MediaStreamTrack& aNewTrack,
                                        const std::string& newTrackId)
{
  if (mPipelines.count(newTrackId)) {
    CSFLogError(logTag, "%s: Pipeline already exists for %s/%s",
                __FUNCTION__, mId.c_str(), newTrackId.c_str());
    return NS_ERROR_INVALID_ARG;
  }

  RefPtr<MediaPipeline> pipeline(info->ForgetPipelineByTrackId_m(oldTrackId));

  if (!pipeline) {
    // Replacetrack can potentially happen in the middle of offer/answer, before
    // the pipeline has been created.
    CSFLogInfo(logTag, "%s: Replacing track before the pipeline has been "
                       "created, nothing to do.", __FUNCTION__);
    return NS_OK;
  }

  nsresult rv =
    static_cast<MediaPipelineTransmit*>(pipeline.get())->ReplaceTrack(aNewTrack);
  NS_ENSURE_SUCCESS(rv, rv);

  mPipelines[newTrackId] = pipeline;

  return NS_OK;
}

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
/**
 * Tells you if any local track is isolated to a specific peer identity.
 * Obviously, we want all the tracks to be isolated equally so that they can
 * all be sent or not.  We check once when we are setting a local description
 * and that determines if we flip the "privacy requested" bit on.  Once the bit
 * is on, all media originating from this peer connection is isolated.
 *
 * @returns true if any track has a peerIdentity set on it
 */
bool
PeerConnectionMedia::AnyLocalTrackHasPeerIdentity() const
{
  ASSERT_ON_THREAD(mMainThread);

  for (uint32_t u = 0; u < mLocalSourceStreams.Length(); u++) {
    for (auto pair : mLocalSourceStreams[u]->GetMediaStreamTracks()) {
      if (pair.second->GetPeerIdentity() != nullptr) {
        return true;
      }
    }
  }
  return false;
}

void
PeerConnectionMedia::UpdateRemoteStreamPrincipals_m(nsIPrincipal* aPrincipal)
{
  ASSERT_ON_THREAD(mMainThread);

  for (uint32_t u = 0; u < mRemoteSourceStreams.Length(); u++) {
    mRemoteSourceStreams[u]->UpdatePrincipal_m(aPrincipal);
  }
}

void
PeerConnectionMedia::UpdateSinkIdentity_m(MediaStreamTrack* aTrack,
                                          nsIPrincipal* aPrincipal,
                                          const PeerIdentity* aSinkIdentity)
{
  ASSERT_ON_THREAD(mMainThread);

  for (uint32_t u = 0; u < mLocalSourceStreams.Length(); u++) {
    mLocalSourceStreams[u]->UpdateSinkIdentity_m(aTrack, aPrincipal,
                                                 aSinkIdentity);
  }
}

void
LocalSourceStreamInfo::UpdateSinkIdentity_m(MediaStreamTrack* aTrack,
                                            nsIPrincipal* aPrincipal,
                                            const PeerIdentity* aSinkIdentity)
{
  for (auto it = mPipelines.begin(); it != mPipelines.end(); ++it) {
    MediaPipelineTransmit* pipeline =
      static_cast<MediaPipelineTransmit*>((*it).second.get());
    pipeline->UpdateSinkIdentity_m(aTrack, aPrincipal, aSinkIdentity);
  }
}

void RemoteSourceStreamInfo::UpdatePrincipal_m(nsIPrincipal* aPrincipal)
{
  // This blasts away the existing principal.
  // We only do this when we become certain that the all tracks are safe to make
  // accessible to the script principal.
  for (auto& trackPair : mTracks) {
    MOZ_RELEASE_ASSERT(trackPair.second);
    RemoteTrackSource& source =
      static_cast<RemoteTrackSource&>(trackPair.second->GetSource());
    source.SetPrincipal(aPrincipal);

    RefPtr<MediaPipeline> pipeline = GetPipelineByTrackId_m(trackPair.first);
    if (pipeline) {
      MOZ_ASSERT(pipeline->direction() == MediaPipeline::RECEIVE);
      static_cast<MediaPipelineReceive*>(pipeline.get())
        ->SetPrincipalHandle_m(MakePrincipalHandle(aPrincipal));
    }
  }
}
#endif // MOZILLA_INTERNAL_API

bool
PeerConnectionMedia::AnyCodecHasPluginID(uint64_t aPluginID)
{
  for (uint32_t i=0; i < mLocalSourceStreams.Length(); ++i) {
    if (mLocalSourceStreams[i]->AnyCodecHasPluginID(aPluginID)) {
      return true;
    }
  }
  for (uint32_t i=0; i < mRemoteSourceStreams.Length(); ++i) {
    if (mRemoteSourceStreams[i]->AnyCodecHasPluginID(aPluginID)) {
      return true;
    }
  }
  return false;
}

bool
SourceStreamInfo::AnyCodecHasPluginID(uint64_t aPluginID)
{
  // Scan the videoConduits for this plugin ID
  for (auto it = mPipelines.begin(); it != mPipelines.end(); ++it) {
    if (it->second->Conduit()->CodecPluginID() == aPluginID) {
      return true;
    }
  }
  return false;
}

nsresult
SourceStreamInfo::StorePipeline(
    const std::string& trackId,
    const RefPtr<mozilla::MediaPipeline>& aPipeline)
{
  MOZ_ASSERT(mPipelines.find(trackId) == mPipelines.end());
  if (mPipelines.find(trackId) != mPipelines.end()) {
    CSFLogError(logTag, "%s: Storing duplicate track", __FUNCTION__);
    return NS_ERROR_FAILURE;
  }

  mPipelines[trackId] = aPipeline;
  return NS_OK;
}

void
RemoteSourceStreamInfo::DetachMedia_m()
{
  for (auto& webrtcIdAndTrack : mTracks) {
    EndTrack(mMediaStream->GetInputStream(), webrtcIdAndTrack.second);
  }
  SourceStreamInfo::DetachMedia_m();
}

void
RemoteSourceStreamInfo::RemoveTrack(const std::string& trackId)
{
  auto it = mTracks.find(trackId);
  if (it != mTracks.end()) {
    EndTrack(mMediaStream->GetInputStream(), it->second);
  }

  SourceStreamInfo::RemoveTrack(trackId);
}

void
RemoteSourceStreamInfo::SyncPipeline(
  RefPtr<MediaPipelineReceive> aPipeline)
{
  // See if we have both audio and video here, and if so cross the streams and
  // sync them
  // TODO: Do we need to prevent multiple syncs if there is more than one audio
  // or video track in a single media stream? What are we supposed to do in this
  // case?
  for (auto i = mPipelines.begin(); i != mPipelines.end(); ++i) {
    if (i->second->IsVideo() != aPipeline->IsVideo()) {
      // Ok, we have one video, one non-video - cross the streams!
      WebrtcAudioConduit *audio_conduit =
        static_cast<WebrtcAudioConduit*>(aPipeline->IsVideo() ?
                                                  i->second->Conduit() :
                                                  aPipeline->Conduit());
      WebrtcVideoConduit *video_conduit =
        static_cast<WebrtcVideoConduit*>(aPipeline->IsVideo() ?
                                                  aPipeline->Conduit() :
                                                  i->second->Conduit());
      video_conduit->SyncTo(audio_conduit);
      CSFLogDebug(logTag, "Syncing %p to %p, %s to %s",
                          video_conduit, audio_conduit,
                          i->first.c_str(), aPipeline->trackid().c_str());
    }
  }
}

void
RemoteSourceStreamInfo::StartReceiving()
{
  if (mReceiving || mPipelines.empty()) {
    return;
  }

  mReceiving = true;

  SourceMediaStream* source = GetMediaStream()->GetInputStream()->AsSourceStream();
  source->SetPullEnabled(true);
  // AdvanceKnownTracksTicksTime(HEAT_DEATH_OF_UNIVERSE) means that in
  // theory per the API, we can't add more tracks before that
  // time. However, the impl actually allows it, and it avoids a whole
  // bunch of locking that would be required (and potential blocking)
  // if we used smaller values and updated them on each NotifyPull.
  source->AdvanceKnownTracksTime(STREAM_TIME_MAX);
  CSFLogDebug(logTag, "Finished adding tracks to MediaStream %p", source);
}

RefPtr<MediaPipeline> SourceStreamInfo::GetPipelineByTrackId_m(
    const std::string& trackId) {
  ASSERT_ON_THREAD(mParent->GetMainThread());

  // Refuse to hand out references if we're tearing down.
  // (Since teardown involves a dispatch to and from STS before MediaPipelines
  // are released, it is safe to start other dispatches to and from STS with a
  // RefPtr<MediaPipeline>, since that reference won't be the last one
  // standing)
  if (mMediaStream) {
    if (mPipelines.count(trackId)) {
      return mPipelines[trackId];
    }
  }

  return nullptr;
}

already_AddRefed<MediaPipeline>
LocalSourceStreamInfo::ForgetPipelineByTrackId_m(const std::string& trackId)
{
  ASSERT_ON_THREAD(mParent->GetMainThread());

  // Refuse to hand out references if we're tearing down.
  // (Since teardown involves a dispatch to and from STS before MediaPipelines
  // are released, it is safe to start other dispatches to and from STS with a
  // RefPtr<MediaPipeline>, since that reference won't be the last one
  // standing)
  if (mMediaStream) {
    if (mPipelines.count(trackId)) {
      RefPtr<MediaPipeline> pipeline(mPipelines[trackId]);
      mPipelines.erase(trackId);
      return pipeline.forget();
    }
  }

  return nullptr;
}

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
auto
RemoteTrackSource::ApplyConstraints(
    nsPIDOMWindowInner* aWindow,
    const dom::MediaTrackConstraints& aConstraints) -> already_AddRefed<PledgeVoid>
{
  RefPtr<PledgeVoid> p = new PledgeVoid();
  p->Reject(new dom::MediaStreamError(aWindow,
                                      NS_LITERAL_STRING("OverconstrainedError"),
                                      NS_LITERAL_STRING("")));
  return p.forget();
}
#endif

} // namespace mozilla
