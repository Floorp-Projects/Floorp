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

#if !defined(MOZILLA_XPCOMRT_API)
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsIURI.h"
#include "nsIScriptSecurityManager.h"
#include "nsICancelable.h"
#include "nsIDocument.h"
#include "nsILoadInfo.h"
#include "nsIContentPolicy.h"
#include "nsIProxyInfo.h"
#include "nsIProtocolProxyService.h"
#endif // !defined(MOZILLA_XPCOMRT_API)

#include "nsProxyRelease.h"

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
#include "MediaStreamList.h"
#include "nsIScriptGlobalObject.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/RTCStatsReportBinding.h"
#include "MediaStreamTrack.h"
#include "VideoStreamTrack.h"
#endif



namespace mozilla {
using namespace dom;

static const char* logTag = "PeerConnectionMedia";

nsresult
PeerConnectionMedia::ReplaceTrack(const std::string& aOldStreamId,
                                  const std::string& aOldTrackId,
                                  DOMMediaStream* aNewStream,
                                  const std::string& aNewStreamId,
                                  const std::string& aNewTrackId)
{
  RefPtr<LocalSourceStreamInfo> oldInfo(GetLocalStreamById(aOldStreamId));

  if (!oldInfo) {
    CSFLogError(logTag, "Failed to find stream id %s", aOldStreamId.c_str());
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsresult rv = AddTrack(aNewStream, aNewStreamId, aNewTrackId);
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<LocalSourceStreamInfo> newInfo(GetLocalStreamById(aNewStreamId));

  if (!newInfo) {
    CSFLogError(logTag, "Failed to add track id %s", aNewTrackId.c_str());
    MOZ_ASSERT(false);
    return NS_ERROR_FAILURE;
  }

  rv = newInfo->TakePipelineFrom(oldInfo, aOldTrackId, aNewTrackId);
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
  pipeline->ShutdownTransport_s();
  mainThread->Dispatch(
      // Make sure we let go of our reference before dispatching
      // If the dispatch fails, well, we're hosed anyway.
      WrapRunnableNM(PipelineReleaseRef_m, pipeline.forget()),
      NS_DISPATCH_NORMAL);
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
    it->second->ShutdownTransport_s();
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
  nsRefPtr<PeerConnectionImpl> pc = new PeerConnectionImpl(&aGlobal);

  CSFLogDebug(logTag, "Created PeerConnection: %p", pc.get());

  return pc.forget();
}

PeerConnectionImpl* PeerConnectionImpl::CreatePeerConnection()
{
  PeerConnectionImpl *pc = new PeerConnectionImpl();

  CSFLogDebug(logTag, "Created PeerConnection: %p", pc);

  return pc;
}

#if !defined(MOZILLA_XPCOMRT_API)
NS_IMETHODIMP PeerConnectionMedia::ProtocolProxyQueryHandler::
OnProxyAvailable(nsICancelable *request,
                 nsIChannel *aChannel,
                 nsIProxyInfo *proxyinfo,
                 nsresult result) {

  if (result == NS_ERROR_ABORT) {
    // NS_ERROR_ABORT means that the PeerConnectionMedia is no longer waiting
    return NS_OK;
  }

  CSFLogInfo(logTag, "%s: Proxy Available: %d", __FUNCTION__, (int)result);

  if (NS_SUCCEEDED(result) && proxyinfo) {
    SetProxyOnPcm(*proxyinfo);
  }

  pcm_->mProxyResolveCompleted = true;
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

  if (pcm_->mIceCtx.get()) {
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
#endif // !defined(MOZILLA_XPCOMRT_API)

PeerConnectionMedia::PeerConnectionMedia(PeerConnectionImpl *parent)
    : mParent(parent),
      mParentHandle(parent->GetHandle()),
      mParentName(parent->GetName()),
      mIceCtx(nullptr),
      mDNSResolver(new NrIceResolver()),
      mUuidGen(MakeUnique<PCUuidGenerator>()),
      mMainThread(mParent->GetMainThread()),
      mSTSThread(mParent->GetSTSThread()),
      mProxyResolveCompleted(false) {
}

nsresult PeerConnectionMedia::Init(const std::vector<NrIceStunServer>& stun_servers,
                                   const std::vector<NrIceTurnServer>& turn_servers,
                                   NrIceCtx::Policy policy)
{
  nsresult rv;
#if defined(MOZILLA_XPCOMRT_API)
  // TODO(Bug 1126039) Standalone XPCOMRT does not currently support nsIProtocolProxyService or nsIIOService
  mProxyResolveCompleted = true;
#else

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

  nsCOMPtr<nsIChannel> channel;

#if defined(MOZILLA_INTERNAL_API)
  nsCOMPtr<nsIDocument> principal = mParent->GetWindow()->GetExtantDoc();
#else
  // For unit-tests
  nsCOMPtr<nsIScriptSecurityManager> secMan(
      do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv));
  if (NS_FAILED(rv)) {
    CSFLogError(logTag, "%s: Failed to get IOService: %d",
        __FUNCTION__, (int)rv);
    CSFLogError(logTag, "%s: Failed to get securityManager: %d", __FUNCTION__, (int)rv);
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIPrincipal> principal;
  rv = secMan->GetSystemPrincipal(getter_AddRefs(principal));
  if (NS_FAILED(rv)) {
    CSFLogError(logTag, "%s: Failed to get systemPrincipal: %d", __FUNCTION__, (int)rv);
    return NS_ERROR_FAILURE;
  }
#endif // defined(MOZILLA_INTERNAL_API)

  rv = NS_NewChannel(getter_AddRefs(channel),
                     fakeHttpsLocation,
                     principal,
                     nsILoadInfo::SEC_NORMAL,
                     nsIContentPolicy::TYPE_OTHER);

  if (NS_FAILED(rv)) {
    CSFLogError(logTag, "%s: Failed to get channel from URI: %d",
                __FUNCTION__, (int)rv);
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<ProtocolProxyQueryHandler> handler = new ProtocolProxyQueryHandler(this);
  rv = pps->AsyncResolve(channel,
                         nsIProtocolProxyService::RESOLVE_PREFER_HTTPS_PROXY |
                         nsIProtocolProxyService::RESOLVE_ALWAYS_TUNNEL,
                         handler, getter_AddRefs(mProxyRequest));
  if (NS_FAILED(rv)) {
    CSFLogError(logTag, "%s: Failed to resolve protocol proxy: %d", __FUNCTION__, (int)rv);
    return NS_ERROR_FAILURE;
  }
#endif // defined(MOZILLA_XPCOMRT_API)

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  bool ice_tcp = Preferences::GetBool("media.peerconnection.ice.tcp", false);
  bool default_address_only = Preferences::GetBool(
    "media.peerconnection.ice.default_address_only", false);
#else
  bool ice_tcp = false;
  bool default_address_only = false;
#endif


  // TODO(ekr@rtfm.com): need some way to set not offerer later
  // Looks like a bug in the NrIceCtx API.
  mIceCtx = NrIceCtx::Create("PC:" + mParentName,
                             true, // Offerer
                             mParent->GetAllowIceLoopback(),
                             ice_tcp,
                             mParent->GetAllowIceLinkLocal(),
                             default_address_only,
                             policy);
  if(!mIceCtx) {
    CSFLogError(logTag, "%s: Failed to create Ice Context", __FUNCTION__);
    return NS_ERROR_FAILURE;
  }

  if (NS_FAILED(rv = mIceCtx->SetStunServers(stun_servers))) {
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
    if (NS_FAILED(rv = mIceCtx->SetTurnServers(turn_servers))) {
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
  if (NS_FAILED(rv = mIceCtx->SetResolver(mDNSResolver->AllocateResolver()))) {
    CSFLogError(logTag, "%s: Failed to get dns resolver", __FUNCTION__);
    return rv;
  }
  mIceCtx->SignalGatheringStateChange.connect(
      this,
      &PeerConnectionMedia::IceGatheringStateChange_s);
  mIceCtx->SignalConnectionStateChange.connect(
      this,
      &PeerConnectionMedia::IceConnectionStateChange_s);

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
  RefPtr<NrIceMediaStream> stream(mIceCtx->GetStream(aLevel));
  if (!stream) {
    CSFLogDebug(logTag, "%s: Creating ICE media stream=%u components=%u",
                mParentHandle.c_str(),
                static_cast<unsigned>(aLevel),
                static_cast<unsigned>(aComponentCount));

    std::ostringstream os;
    os << mParentName << " aLevel=" << aLevel;
    RefPtr<NrIceMediaStream> stream = mIceCtx->CreateStream(os.str().c_str(),
                                                            aComponentCount);

    if (!stream) {
      CSFLogError(logTag, "Failed to create ICE stream.");
      return;
    }

    stream->SetLevel(aLevel);
    stream->SignalReady.connect(this, &PeerConnectionMedia::IceStreamReady_s);
    stream->SignalCandidate.connect(this,
                                    &PeerConnectionMedia::OnCandidateFound_s);
    mIceCtx->SetStream(aLevel, stream);
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
    mIceCtx->SetStream(aMLine, nullptr);
    return;
  }

  RefPtr<NrIceMediaStream> stream(mIceCtx->GetStream(aMLine));
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
  for (size_t i = aMLine; i < mIceCtx->GetStreamCount(); ++i) {
    mIceCtx->SetStream(i, nullptr);
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

  nsresult rv = mIceCtx->ParseGlobalAttributes(attributes);
  if (NS_FAILED(rv)) {
    CSFLogError(logTag, "%s: couldn't parse global parameters", __FUNCTION__ );
  }

  mIceCtx->SetControlling(aIsControlling ?
                          NrIceCtx::ICE_CONTROLLING :
                          NrIceCtx::ICE_CONTROLLED);

  mIceCtx->StartChecks();
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
  RefPtr<NrIceMediaStream> stream(mIceCtx->GetStream(aMLine));
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
        &PeerConnectionMedia::EnsureIceGathering_s));

  PerformOrEnqueueIceCtxOperation(runnable);
}

void
PeerConnectionMedia::EnsureIceGathering_s() {
  if (mProxyServer) {
    mIceCtx->SetProxyServer(*mProxyServer);
  }

  // Start gathering, but only if there are streams
  for (size_t i = 0; i < mIceCtx->GetStreamCount(); ++i) {
    if (mIceCtx->GetStream(i)) {
      mIceCtx->StartGathering();
      return;
    }
  }

  // If there are no streams, we're probably in a situation where we've rolled
  // back while still waiting for our proxy configuration to come back. Make
  // sure content knows that the rollback has stuck wrt gathering.
  IceGatheringStateChange_s(mIceCtx.get(), NrIceCtx::ICE_CTX_GATHER_COMPLETE);
}

nsresult
PeerConnectionMedia::AddTrack(DOMMediaStream* aMediaStream,
                              const std::string& streamId,
                              const std::string& trackId)
{
  ASSERT_ON_THREAD(mMainThread);

  if (!aMediaStream) {
    CSFLogError(logTag, "%s - aMediaStream is NULL", __FUNCTION__);
    return NS_ERROR_FAILURE;
  }

  CSFLogDebug(logTag, "%s: MediaStream: %p", __FUNCTION__, aMediaStream);

  nsRefPtr<LocalSourceStreamInfo> localSourceStream =
    GetLocalStreamById(streamId);

  if (!localSourceStream) {
    localSourceStream = new LocalSourceStreamInfo(aMediaStream, this, streamId);
    mLocalSourceStreams.AppendElement(localSourceStream);
  }

  localSourceStream->AddTrack(trackId);
  return NS_OK;
}

nsresult
PeerConnectionMedia::RemoveLocalTrack(const std::string& streamId,
                                      const std::string& trackId)
{
  ASSERT_ON_THREAD(mMainThread);

  CSFLogDebug(logTag, "%s: stream: %s track: %s", __FUNCTION__,
                      streamId.c_str(), trackId.c_str());

  nsRefPtr<LocalSourceStreamInfo> localSourceStream =
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

  nsRefPtr<RemoteSourceStreamInfo> remoteSourceStream =
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

nsresult
PeerConnectionMedia::GetRemoteTrackId(const std::string streamId,
                                      TrackID numericTrackId,
                                      std::string* trackId) const
{
  auto* ncThis = const_cast<PeerConnectionMedia*>(this);
  const RemoteSourceStreamInfo* info =
    ncThis->GetRemoteStreamById(streamId);

  if (!info) {
    CSFLogError(logTag, "%s: Could not find stream info", __FUNCTION__);
    return NS_ERROR_NOT_AVAILABLE;
  }

  return info->GetTrackId(numericTrackId, trackId);
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
  mIceCtx = nullptr;

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

nsresult
PeerConnectionMedia::AddRemoteStream(nsRefPtr<RemoteSourceStreamInfo> aInfo)
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
      nsresult res = stream->GetDefaultCandidate(1, &candidate);
      NrIceCandidate rtcpCandidate;
      // Optional; component won't exist if doing rtcp-mux
      if (NS_FAILED(stream->GetDefaultCandidate(2, &rtcpCandidate))) {
        rtcpCandidate.cand_addr.host.clear();
        rtcpCandidate.cand_addr.port = 0;
      }
      if (NS_SUCCEEDED(res)) {
        EndOfLocalCandidates(candidate.cand_addr.host,
                             candidate.cand_addr.port,
                             rtcpCandidate.cand_addr.host,
                             rtcpCandidate.cand_addr.port,
                             i);
      } else {
        CSFLogError(logTag, "%s: GetDefaultCandidate failed for level %u, "
                            "res=%u",
                            __FUNCTION__,
                            static_cast<unsigned>(i),
                            static_cast<unsigned>(res));
      }
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
                                        const std::string &candidate)
{
  ASSERT_ON_THREAD(mSTSThread);
  MOZ_ASSERT(aStream);

  CSFLogDebug(logTag, "%s: %s", __FUNCTION__, aStream->name().c_str());

  // ShutdownMediaTransport_s has not run yet because it unhooks this function
  // from its signal, which means that SelfDestruct_m has not been dispatched
  // yet either, so this PCMedia will still be around when this dispatch reaches
  // main.
  GetMainThread()->Dispatch(
    WrapRunnable(this,
                 &PeerConnectionMedia::OnCandidateFound_m,
                 candidate,
                 aStream->GetLevel()),
    NS_DISPATCH_NORMAL);
}

void
PeerConnectionMedia::EndOfLocalCandidates(const std::string& aDefaultAddr,
                                          uint16_t aDefaultPort,
                                          const std::string& aDefaultRtcpAddr,
                                          uint16_t aDefaultRtcpPort,
                                          uint16_t aMLine) {
  // We will still be around because we have not started teardown yet
  GetMainThread()->Dispatch(
    WrapRunnable(this,
                 &PeerConnectionMedia::EndOfLocalCandidates_m,
                 aDefaultAddr, aDefaultPort,
                 aDefaultRtcpAddr, aDefaultRtcpPort, aMLine),
    NS_DISPATCH_NORMAL);
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
PeerConnectionMedia::OnCandidateFound_m(const std::string &candidate,
                                        uint16_t aMLine)
{
  ASSERT_ON_THREAD(mMainThread);
  SignalCandidate(candidate, aMLine);
}

void
PeerConnectionMedia::EndOfLocalCandidates_m(const std::string& aDefaultAddr,
                                            uint16_t aDefaultPort,
                                            const std::string& aDefaultRtcpAddr,
                                            uint16_t aDefaultRtcpPort,
                                            uint16_t aMLine) {
  SignalEndOfLocalCandidates(aDefaultAddr,
                             aDefaultPort,
                             aDefaultRtcpAddr,
                             aDefaultRtcpPort,
                             aMLine);
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
  TransportFlow* flow = mTransportFlows[index_inner].forget().take();
  if (flow) {
    NS_ProxyRelease(GetSTSThread(), flow);
  }
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
    static_cast<MediaPipelineTransmit*>(pipeline.get())->ReplaceTrack(
        mMediaStream, newTrackId);
  NS_ENSURE_SUCCESS(rv, rv);

  mPipelines[newTrackId] = pipeline;

  return NS_OK;
}

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
/**
 * Tells you if any local streams is isolated to a specific peer identity.
 * Obviously, we want all the streams to be isolated equally so that they can
 * all be sent or not.  We check once when we are setting a local description
 * and that determines if we flip the "privacy requested" bit on.  Once the bit
 * is on, all media originating from this peer connection is isolated.
 *
 * @returns true if any stream has a peerIdentity set on it
 */
bool
PeerConnectionMedia::AnyLocalStreamHasPeerIdentity() const
{
  ASSERT_ON_THREAD(mMainThread);

  for (uint32_t u = 0; u < mLocalSourceStreams.Length(); u++) {
    // check if we should be asking for a private call for this stream
    DOMMediaStream* stream = mLocalSourceStreams[u]->GetMediaStream();
    if (stream->GetPeerIdentity()) {
      return true;
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
PeerConnectionMedia::UpdateSinkIdentity_m(nsIPrincipal* aPrincipal,
                                          const PeerIdentity* aSinkIdentity)
{
  ASSERT_ON_THREAD(mMainThread);

  for (uint32_t u = 0; u < mLocalSourceStreams.Length(); u++) {
    mLocalSourceStreams[u]->UpdateSinkIdentity_m(aPrincipal, aSinkIdentity);
  }
}

void
LocalSourceStreamInfo::UpdateSinkIdentity_m(nsIPrincipal* aPrincipal,
                                            const PeerIdentity* aSinkIdentity)
{
  for (auto it = mPipelines.begin(); it != mPipelines.end(); ++it) {
    MediaPipelineTransmit* pipeline =
      static_cast<MediaPipelineTransmit*>((*it).second.get());
    pipeline->UpdateSinkIdentity_m(aPrincipal, aSinkIdentity);
  }
}

void RemoteSourceStreamInfo::UpdatePrincipal_m(nsIPrincipal* aPrincipal)
{
  // this blasts away the existing principal
  // we only do this when we become certain that the stream is safe to make
  // accessible to the script principal
  mMediaStream->SetPrincipal(aPrincipal);
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

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
nsRefPtr<mozilla::dom::VideoStreamTrack>
SourceStreamInfo::GetVideoTrackByTrackId(const std::string& trackId)
{
  nsTArray<nsRefPtr<mozilla::dom::VideoStreamTrack>> videoTracks;

  mMediaStream->GetVideoTracks(videoTracks);

  for (size_t i = 0; i < videoTracks.Length(); ++i) {
    nsString aTrackId;
    videoTracks[i]->GetId(aTrackId);
    if (aTrackId.EqualsIgnoreCase(trackId.c_str())) {
      return videoTracks[i];
    }
  }

  return nullptr;
}
#endif

nsresult
SourceStreamInfo::StorePipeline(
    const std::string& trackId,
    const mozilla::RefPtr<mozilla::MediaPipeline>& aPipeline)
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

  SourceMediaStream* source = GetMediaStream()->GetStream()->AsSourceStream();
  source->FinishAddTracks();
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

} // namespace mozilla
