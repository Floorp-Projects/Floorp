/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CSFLog.h"

#include "nr_socket_proxy_config.h"
#include "MediaPipelineFilter.h"
#include "MediaPipeline.h"
#include "PeerConnectionImpl.h"
#include "PeerConnectionMedia.h"
#include "runnable_utils.h"
#include "signaling/src/jsep/JsepSession.h"
#include "signaling/src/jsep/JsepTransport.h"

#include "nsContentUtils.h"
#include "nsIURI.h"
#include "nsIScriptSecurityManager.h"
#include "nsICancelable.h"
#include "nsILoadInfo.h"
#include "nsIContentPolicy.h"
#include "nsIProxyInfo.h"
#include "nsIProtocolProxyService.h"
#include "nsIPrincipal.h"
#include "mozilla/LoadInfo.h"
#include "nsProxyRelease.h"

#include "nsIScriptGlobalObject.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/BrowserChild.h"
#include "MediaManager.h"
#include "WebrtcGmpVideoCodec.h"

namespace mozilla {
using namespace dom;

static const char* pcmLogTag = "PeerConnectionMedia";
#ifdef LOGTAG
#  undef LOGTAG
#endif
#define LOGTAG pcmLogTag

NS_IMETHODIMP PeerConnectionMedia::ProtocolProxyQueryHandler::OnProxyAvailable(
    nsICancelable* request, nsIChannel* aChannel, nsIProxyInfo* proxyinfo,
    nsresult result) {
  if (!pcm_->mProxyRequest) {
    // PeerConnectionMedia is no longer waiting
    return NS_OK;
  }

  CSFLogInfo(LOGTAG, "%s: Proxy Available: %d", __FUNCTION__, (int)result);

  if (NS_SUCCEEDED(result) && proxyinfo) {
    SetProxyOnPcm(*proxyinfo);
  }

  pcm_->mProxyResolveCompleted = true;
  pcm_->mProxyRequest = nullptr;
  pcm_->FlushIceCtxOperationQueueIfReady();

  return NS_OK;
}

void PeerConnectionMedia::ProtocolProxyQueryHandler::SetProxyOnPcm(
    nsIProxyInfo& proxyinfo) {
  CSFLogInfo(LOGTAG, "%s: Had proxyinfo", __FUNCTION__);

  nsCString alpn = NS_LITERAL_CSTRING("webrtc,c-webrtc");
  auto browserChild = BrowserChild::GetFrom(pcm_->GetWindow());
  if (!browserChild) {
    // Android doesn't have browser child apparently...
    return;
  }
  TabId id = browserChild->GetTabId();
  nsCOMPtr<nsILoadInfo> loadInfo = new net::LoadInfo(
      nsContentUtils::GetSystemPrincipal(), nullptr, nullptr, 0, 0);

  Maybe<net::LoadInfoArgs> loadInfoArgs;
  MOZ_ALWAYS_SUCCEEDS(
      mozilla::ipc::LoadInfoToLoadInfoArgs(loadInfo, &loadInfoArgs));
  pcm_->mProxyConfig.reset(new NrSocketProxyConfig(id, alpn, *loadInfoArgs));
}

NS_IMPL_ISUPPORTS(PeerConnectionMedia::ProtocolProxyQueryHandler,
                  nsIProtocolProxyCallback)

void PeerConnectionMedia::StunAddrsHandler::OnStunAddrsAvailable(
    const mozilla::net::NrIceStunAddrArray& addrs) {
  CSFLogInfo(LOGTAG, "%s: receiving (%d) stun addrs", __FUNCTION__,
             (int)addrs.Length());
  if (pcm_) {
    pcm_->mStunAddrs = addrs;
    pcm_->mLocalAddrsCompleted = true;
    pcm_->mStunAddrsRequest = nullptr;
    pcm_->FlushIceCtxOperationQueueIfReady();
    // If parent process returns 0 STUN addresses, change ICE connection
    // state to failed.
    if (!pcm_->mStunAddrs.Length()) {
      pcm_->SignalIceConnectionStateChange(
          dom::PCImplIceConnectionState::Failed);
    }

    pcm_ = nullptr;
  }
}

PeerConnectionMedia::PeerConnectionMedia(PeerConnectionImpl* parent)
    : mTransportHandler(parent->GetTransportHandler()),
      mParent(parent),
      mParentHandle(parent->GetHandle()),
      mParentName(parent->GetName()),
      mMainThread(mParent->GetMainThread()),
      mSTSThread(mParent->GetSTSThread()),
      mProxyResolveCompleted(false),
      mProxyConfig(nullptr),
      mLocalAddrsCompleted(false) {}

PeerConnectionMedia::~PeerConnectionMedia() {
  MOZ_RELEASE_ASSERT(!mMainThread);
}

void PeerConnectionMedia::InitLocalAddrs() {
  if (XRE_IsContentProcess()) {
    CSFLogDebug(LOGTAG, "%s: Get stun addresses via IPC",
                mParentHandle.c_str());

    nsCOMPtr<nsIEventTarget> target =
        mParent->GetWindow()
            ? mParent->GetWindow()->EventTargetFor(TaskCategory::Other)
            : nullptr;

    // We're in the content process, so send a request over IPC for the
    // stun address discovery.
    mStunAddrsRequest =
        new net::StunAddrsRequestChild(new StunAddrsHandler(this), target);
    mStunAddrsRequest->SendGetStunAddrs();
  } else {
    // No content process, so don't need to hold up the ice event queue
    // until completion of stun address discovery. We can let the
    // discovery of stun addresses happen in the same process.
    mLocalAddrsCompleted = true;
  }
}

nsresult PeerConnectionMedia::InitProxy() {
  // Allow mochitests to disable this, since mochitest configures a fake proxy
  // that serves up content.
  bool disable =
      Preferences::GetBool("media.peerconnection.disable_http_proxy", false);
  if (disable) {
    mProxyResolveCompleted = true;
    return NS_OK;
  }

  nsresult rv;
  nsCOMPtr<nsIProtocolProxyService> pps =
      do_GetService(NS_PROTOCOLPROXYSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    CSFLogError(LOGTAG, "%s: Failed to get proxy service: %d", __FUNCTION__,
                (int)rv);
    return NS_ERROR_FAILURE;
  }

  // We use the following URL to find the "default" proxy address for all HTTPS
  // connections.  We will only attempt one HTTP(S) CONNECT per peer connection.
  // "example.com" is guaranteed to be unallocated and should return the best
  // default.
  nsCOMPtr<nsIURI> fakeHttpsLocation;
  rv = NS_NewURI(getter_AddRefs(fakeHttpsLocation), "https://example.com");
  if (NS_FAILED(rv)) {
    CSFLogError(LOGTAG, "%s: Failed to set URI: %d", __FUNCTION__, (int)rv);
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel), fakeHttpsLocation,
                     nsContentUtils::GetSystemPrincipal(),
                     nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
                     nsIContentPolicy::TYPE_OTHER);

  if (NS_FAILED(rv)) {
    CSFLogError(LOGTAG, "%s: Failed to get channel from URI: %d", __FUNCTION__,
                (int)rv);
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIEventTarget> target =
      mParent->GetWindow()
          ? mParent->GetWindow()->EventTargetFor(TaskCategory::Network)
          : nullptr;
  RefPtr<ProtocolProxyQueryHandler> handler =
      new ProtocolProxyQueryHandler(this);
  rv = pps->AsyncResolve(channel,
                         nsIProtocolProxyService::RESOLVE_PREFER_HTTPS_PROXY |
                             nsIProtocolProxyService::RESOLVE_ALWAYS_TUNNEL,
                         handler, target, getter_AddRefs(mProxyRequest));
  if (NS_FAILED(rv)) {
    CSFLogError(LOGTAG, "%s: Failed to resolve protocol proxy: %d",
                __FUNCTION__, (int)rv);
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult PeerConnectionMedia::Init() {
  nsresult rv = InitProxy();
  NS_ENSURE_SUCCESS(rv, rv);

  // setup the stun local addresses IPC async call
  InitLocalAddrs();

  ConnectSignals();
  return NS_OK;
}

void PeerConnectionMedia::EnsureTransports(const JsepSession& aSession) {
  for (const auto& transceiver : aSession.GetTransceivers()) {
    if (transceiver->HasOwnTransport()) {
      RUN_ON_THREAD(
          GetSTSThread(),
          WrapRunnable(mTransportHandler,
                       &MediaTransportHandler::EnsureProvisionalTransport,
                       transceiver->mTransport.mTransportId,
                       transceiver->mTransport.mLocalUfrag,
                       transceiver->mTransport.mLocalPwd,
                       transceiver->mTransport.mComponents),
          NS_DISPATCH_NORMAL);
    }
  }

  GatherIfReady();
}

nsresult PeerConnectionMedia::UpdateTransports(const JsepSession& aSession,
                                               const bool forceIceTcp) {
  std::set<std::string> finalTransports;
  for (const auto& transceiver : aSession.GetTransceivers()) {
    if (transceiver->HasOwnTransport()) {
      finalTransports.insert(transceiver->mTransport.mTransportId);
      UpdateTransport(*transceiver, forceIceTcp);
    }
  }

  RUN_ON_THREAD(GetSTSThread(),
                WrapRunnable(mTransportHandler,
                             &MediaTransportHandler::RemoveTransportsExcept,
                             finalTransports),
                NS_DISPATCH_NORMAL);

  for (const auto& transceiverImpl : mTransceivers) {
    transceiverImpl->UpdateTransport();
  }

  return NS_OK;
}

void PeerConnectionMedia::UpdateTransport(const JsepTransceiver& aTransceiver,
                                          bool aForceIceTcp) {
  std::string ufrag;
  std::string pwd;
  std::vector<std::string> candidates;
  size_t components = 0;

  const JsepTransport& transport = aTransceiver.mTransport;
  unsigned level = aTransceiver.GetLevel();

  CSFLogDebug(LOGTAG, "ACTIVATING TRANSPORT! - PC %s: level=%u components=%u",
              mParentHandle.c_str(), (unsigned)level,
              (unsigned)transport.mComponents);

  ufrag = transport.mIce->GetUfrag();
  pwd = transport.mIce->GetPassword();
  candidates = transport.mIce->GetCandidates();
  components = transport.mComponents;
  if (aForceIceTcp) {
    candidates.erase(
        std::remove_if(candidates.begin(), candidates.end(),
                       [](const std::string& s) {
                         return s.find(" UDP ") != std::string::npos ||
                                s.find(" udp ") != std::string::npos;
                       }),
        candidates.end());
  }

  nsTArray<uint8_t> keyDer;
  nsTArray<uint8_t> certDer;
  nsresult rv = mParent->Identity()->Serialize(&keyDer, &certDer);
  if (NS_FAILED(rv)) {
    CSFLogError(LOGTAG, "%s: Failed to serialize DTLS identity: %d",
                __FUNCTION__, (int)rv);
    return;
  }

  DtlsDigestList digests;
  for (const auto& fingerprint :
       transport.mDtls->GetFingerprints().mFingerprints) {
    std::ostringstream ss;
    ss << fingerprint.hashFunc;
    digests.emplace_back(ss.str(), fingerprint.fingerprint);
  }

  RUN_ON_THREAD(
      GetSTSThread(),
      WrapRunnable(
          mTransportHandler, &MediaTransportHandler::ActivateTransport,
          transport.mTransportId, transport.mLocalUfrag, transport.mLocalPwd,
          components, ufrag, pwd, keyDer, certDer,
          mParent->Identity()->auth_type(),
          transport.mDtls->GetRole() == JsepDtlsTransport::kJsepDtlsClient,
          digests, mParent->PrivacyRequested()),
      NS_DISPATCH_NORMAL);

  for (auto& candidate : candidates) {
    AddIceCandidate("candidate:" + candidate, transport.mTransportId, ufrag);
  }
}

nsresult PeerConnectionMedia::UpdateMediaPipelines() {
  // The GMP code is all the way on the other side of webrtc.org, and it is not
  // feasible to plumb error information all the way back. So, we set up a
  // handle to the PC (for the duration of this call) in a global variable.
  // This allows the GMP code to report errors to the PC.
  WebrtcGmpPCHandleSetter setter(mParentHandle);

  for (RefPtr<TransceiverImpl>& transceiver : mTransceivers) {
    transceiver->ResetSync();
  }

  for (RefPtr<TransceiverImpl>& transceiver : mTransceivers) {
    if (!transceiver->IsVideo()) {
      nsresult rv = transceiver->SyncWithMatchingVideoConduits(mTransceivers);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }

    nsresult rv = transceiver->UpdateConduit();
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  return NS_OK;
}

void PeerConnectionMedia::StartIceChecks(const JsepSession& aSession) {
  nsCOMPtr<nsIRunnable> runnable(WrapRunnable(
      RefPtr<PeerConnectionMedia>(this), &PeerConnectionMedia::StartIceChecks_s,
      aSession.IsIceControlling(), aSession.IsOfferer(),
      aSession.RemoteIsIceLite(),
      // Copy, just in case API changes to return a ref
      std::vector<std::string>(aSession.GetIceOptions())));

  PerformOrEnqueueIceCtxOperation(runnable);
}

void PeerConnectionMedia::StartIceChecks_s(
    bool aIsControlling, bool aIsOfferer, bool aIsIceLite,
    const std::vector<std::string>& aIceOptionsList) {
  CSFLogDebug(LOGTAG, "Starting ICE Checking");

  std::vector<std::string> attributes;
  if (aIsIceLite) {
    attributes.push_back("ice-lite");
  }

  if (!aIceOptionsList.empty()) {
    attributes.push_back("ice-options:");
    for (const auto& option : aIceOptionsList) {
      attributes.back() += option + ' ';
    }
  }

  mTransportHandler->StartIceChecks(aIsControlling, aIsOfferer, attributes);
}

bool PeerConnectionMedia::GetPrefDefaultAddressOnly() const {
  ASSERT_ON_THREAD(mMainThread);  // will crash on STS thread

  uint64_t winId = mParent->GetWindow()->WindowID();

  bool default_address_only = Preferences::GetBool(
      "media.peerconnection.ice.default_address_only", false);
  default_address_only |=
      !MediaManager::Get()->IsActivelyCapturingOrHasAPermission(winId);
  return default_address_only;
}

void PeerConnectionMedia::ConnectSignals() {
  mTransportHandler->SignalGatheringStateChange.connect(
      this, &PeerConnectionMedia::IceGatheringStateChange_s);
  mTransportHandler->SignalConnectionStateChange.connect(
      this, &PeerConnectionMedia::IceConnectionStateChange_s);
  mTransportHandler->SignalCandidate.connect(
      this, &PeerConnectionMedia::OnCandidateFound_s);
  mTransportHandler->SignalAlpnNegotiated.connect(
      this, &PeerConnectionMedia::AlpnNegotiated_s);
}

void PeerConnectionMedia::AddIceCandidate(const std::string& aCandidate,
                                          const std::string& aTransportId,
                                          const std::string& aUfrag) {
  MOZ_ASSERT(!aTransportId.empty());
  RUN_ON_THREAD(
      GetSTSThread(),
      WrapRunnable(mTransportHandler, &MediaTransportHandler::AddIceCandidate,
                   aTransportId, aCandidate, aUfrag),
      NS_DISPATCH_NORMAL);
}

void PeerConnectionMedia::UpdateNetworkState(bool online) {
  RUN_ON_THREAD(
      GetSTSThread(),
      WrapRunnable(mTransportHandler,
                   &MediaTransportHandler::UpdateNetworkState, online),
      NS_DISPATCH_NORMAL);
}

void PeerConnectionMedia::FlushIceCtxOperationQueueIfReady() {
  ASSERT_ON_THREAD(mMainThread);

  if (IsIceCtxReady()) {
    for (auto& mQueuedIceCtxOperation : mQueuedIceCtxOperations) {
      GetSTSThread()->Dispatch(mQueuedIceCtxOperation, NS_DISPATCH_NORMAL);
    }
    mQueuedIceCtxOperations.clear();
  }
}

void PeerConnectionMedia::PerformOrEnqueueIceCtxOperation(
    nsIRunnable* runnable) {
  ASSERT_ON_THREAD(mMainThread);

  if (IsIceCtxReady()) {
    GetSTSThread()->Dispatch(runnable, NS_DISPATCH_NORMAL);
  } else {
    mQueuedIceCtxOperations.push_back(runnable);
  }
}

void PeerConnectionMedia::GatherIfReady() {
  ASSERT_ON_THREAD(mMainThread);

  // If we had previously queued gathering or ICE start, unqueue them
  mQueuedIceCtxOperations.clear();
  nsCOMPtr<nsIRunnable> runnable(WrapRunnable(
      RefPtr<PeerConnectionMedia>(this),
      &PeerConnectionMedia::EnsureIceGathering_s, GetPrefDefaultAddressOnly()));

  PerformOrEnqueueIceCtxOperation(runnable);
}

void PeerConnectionMedia::EnsureIceGathering_s(bool aDefaultRouteOnly) {
  if (mProxyConfig) {
    // Note that this could check if PrivacyRequested() is set on the PC and
    // remove "webrtc" from the ALPN list.  But that would only work if the PC
    // was constructed with a peerIdentity constraint, not when isolated
    // streams are added.  If we ever need to signal to the proxy that the
    // media is isolated, then we would need to restructure this code.
    mTransportHandler->SetProxyServer(std::move(*mProxyConfig));
    mProxyConfig.reset();
  }

  // Make sure we don't call StartIceGathering if we're in e10s mode
  // and we received no STUN addresses from the parent process.  In the
  // absence of previously provided STUN addresses, StartIceGathering will
  // attempt to gather them (as in non-e10s mode), and this will cause a
  // sandboxing exception in e10s mode.
  if (!mStunAddrs.Length() && XRE_IsContentProcess()) {
    CSFLogInfo(LOGTAG, "%s: No STUN addresses returned from parent process",
               __FUNCTION__);
    return;
  }

  mTransportHandler->StartIceGathering(aDefaultRouteOnly, mStunAddrs);
}

void PeerConnectionMedia::SelfDestruct() {
  ASSERT_ON_THREAD(mMainThread);

  CSFLogDebug(LOGTAG, "%s: ", __FUNCTION__);

  if (mStunAddrsRequest) {
    mStunAddrsRequest->Cancel();
    mStunAddrsRequest = nullptr;
  }

  if (mProxyRequest) {
    mProxyRequest->Cancel(NS_ERROR_ABORT);
    mProxyRequest = nullptr;
  }

  for (auto& transceiver : mTransceivers) {
    // transceivers are garbage-collected, so we need to poke them to perform
    // cleanup right now so the appropriate events fire.
    transceiver->Shutdown_m();
  }

  mTransceivers.clear();

  mQueuedIceCtxOperations.clear();

  // Shutdown the transport (async)
  RUN_ON_THREAD(
      mSTSThread,
      WrapRunnable(this, &PeerConnectionMedia::ShutdownMediaTransport_s),
      NS_DISPATCH_NORMAL);

  CSFLogDebug(LOGTAG, "%s: Media shut down", __FUNCTION__);
}

void PeerConnectionMedia::SelfDestruct_m() {
  CSFLogDebug(LOGTAG, "%s: ", __FUNCTION__);

  ASSERT_ON_THREAD(mMainThread);

  mMainThread = nullptr;

  // Final self-destruct.
  this->Release();
}

void PeerConnectionMedia::ShutdownMediaTransport_s() {
  ASSERT_ON_THREAD(mSTSThread);

  CSFLogDebug(LOGTAG, "%s: ", __FUNCTION__);

  disconnect_all();

  mTransportHandler->Destroy();
  mTransportHandler = nullptr;

  // we're holding a ref to 'this' that's released by SelfDestruct_m
  mMainThread->Dispatch(
      WrapRunnable(this, &PeerConnectionMedia::SelfDestruct_m),
      NS_DISPATCH_NORMAL);
}

nsresult PeerConnectionMedia::AddTransceiver(
    JsepTransceiver* aJsepTransceiver, dom::MediaStreamTrack& aReceiveTrack,
    dom::MediaStreamTrack* aSendTrack,
    RefPtr<TransceiverImpl>* aTransceiverImpl) {
  if (!mCall) {
    mCall = WebRtcCallWrapper::Create();
  }

  RefPtr<TransceiverImpl> transceiver =
      new TransceiverImpl(mParent->GetHandle(), mTransportHandler,
                          aJsepTransceiver, mMainThread.get(), mSTSThread.get(),
                          &aReceiveTrack, aSendTrack, mCall.get());

  if (!transceiver->IsValid()) {
    return NS_ERROR_FAILURE;
  }

  if (aSendTrack) {
    // implement checking for peerIdentity (where failure == black/silence)
    Document* doc = mParent->GetWindow()->GetExtantDoc();
    if (doc) {
      transceiver->UpdateSinkIdentity(nullptr, doc->NodePrincipal(),
                                      mParent->GetPeerIdentity());
    } else {
      MOZ_CRASH();
      return NS_ERROR_FAILURE;  // Don't remove this till we know it's safe.
    }
  }

  mTransceivers.push_back(transceiver);
  *aTransceiverImpl = transceiver;

  return NS_OK;
}

void PeerConnectionMedia::GetTransmitPipelinesMatching(
    const MediaStreamTrack* aTrack,
    nsTArray<RefPtr<MediaPipeline>>* aPipelines) {
  for (RefPtr<TransceiverImpl>& transceiver : mTransceivers) {
    if (transceiver->HasSendTrack(aTrack)) {
      aPipelines->AppendElement(transceiver->GetSendPipeline());
    }
  }
}

void PeerConnectionMedia::GetReceivePipelinesMatching(
    const MediaStreamTrack* aTrack,
    nsTArray<RefPtr<MediaPipeline>>* aPipelines) {
  for (RefPtr<TransceiverImpl>& transceiver : mTransceivers) {
    if (transceiver->HasReceiveTrack(aTrack)) {
      aPipelines->AppendElement(transceiver->GetReceivePipeline());
    }
  }
}

std::string PeerConnectionMedia::GetTransportIdMatching(
    const dom::MediaStreamTrack& aTrack) const {
  for (const RefPtr<TransceiverImpl>& transceiver : mTransceivers) {
    if (transceiver->HasReceiveTrack(&aTrack)) {
      return transceiver->GetTransportId();
    }
  }
  return std::string();
}

nsresult PeerConnectionMedia::AddRIDExtension(MediaStreamTrack& aRecvTrack,
                                              unsigned short aExtensionId) {
  DebugOnly<bool> trackFound = false;
  for (RefPtr<TransceiverImpl>& transceiver : mTransceivers) {
    if (transceiver->HasReceiveTrack(&aRecvTrack)) {
      transceiver->AddRIDExtension(aExtensionId);
      trackFound = true;
    }
  }
  MOZ_ASSERT(trackFound);
  return NS_OK;
}

nsresult PeerConnectionMedia::AddRIDFilter(MediaStreamTrack& aRecvTrack,
                                           const nsAString& aRid) {
  DebugOnly<bool> trackFound = false;
  for (RefPtr<TransceiverImpl>& transceiver : mTransceivers) {
    MOZ_ASSERT(transceiver->HasReceiveTrack(&aRecvTrack));
    if (transceiver->HasReceiveTrack(&aRecvTrack)) {
      transceiver->AddRIDFilter(aRid);
      trackFound = true;
    }
  }
  MOZ_ASSERT(trackFound);
  return NS_OK;
}

void PeerConnectionMedia::IceGatheringStateChange_s(
    dom::PCImplIceGatheringState aState) {
  ASSERT_ON_THREAD(mSTSThread);

  // ShutdownMediaTransport_s has not run yet because it unhooks this function
  // from its signal, which means that SelfDestruct_m has not been dispatched
  // yet either, so this PCMedia will still be around when this dispatch reaches
  // main.
  GetMainThread()->Dispatch(
      WrapRunnable(this, &PeerConnectionMedia::IceGatheringStateChange_m,
                   aState),
      NS_DISPATCH_NORMAL);
}

void PeerConnectionMedia::IceConnectionStateChange_s(
    dom::PCImplIceConnectionState aState) {
  ASSERT_ON_THREAD(mSTSThread);
  // ShutdownMediaTransport_s has not run yet because it unhooks this function
  // from its signal, which means that SelfDestruct_m has not been dispatched
  // yet either, so this PCMedia will still be around when this dispatch reaches
  // main.
  GetMainThread()->Dispatch(
      WrapRunnable(this, &PeerConnectionMedia::IceConnectionStateChange_m,
                   aState),
      NS_DISPATCH_NORMAL);
}

void PeerConnectionMedia::OnCandidateFound_s(
    const std::string& aTransportId, const CandidateInfo& aCandidateInfo) {
  ASSERT_ON_THREAD(mSTSThread);
  MOZ_RELEASE_ASSERT(mTransportHandler);

  CSFLogDebug(LOGTAG, "%s: %s", __FUNCTION__, aTransportId.c_str());

  MOZ_ASSERT(!aCandidateInfo.mUfrag.empty());

  // ShutdownMediaTransport_s has not run yet because it unhooks this function
  // from its signal, which means that SelfDestruct_m has not been dispatched
  // yet either, so this PCMedia will still be around when this dispatch reaches
  // main.
  GetMainThread()->Dispatch(
      WrapRunnable(this, &PeerConnectionMedia::OnCandidateFound_m, aTransportId,
                   aCandidateInfo),
      NS_DISPATCH_NORMAL);
}

void PeerConnectionMedia::IceGatheringStateChange_m(
    dom::PCImplIceGatheringState aState) {
  ASSERT_ON_THREAD(mMainThread);
  SignalIceGatheringStateChange(aState);
}

void PeerConnectionMedia::IceConnectionStateChange_m(
    dom::PCImplIceConnectionState aState) {
  ASSERT_ON_THREAD(mMainThread);
  SignalIceConnectionStateChange(aState);
}

void PeerConnectionMedia::OnCandidateFound_m(
    const std::string& aTransportId, const CandidateInfo& aCandidateInfo) {
  ASSERT_ON_THREAD(mMainThread);
  if (!aCandidateInfo.mDefaultHostRtp.empty()) {
    SignalUpdateDefaultCandidate(aCandidateInfo.mDefaultHostRtp,
                                 aCandidateInfo.mDefaultPortRtp,
                                 aCandidateInfo.mDefaultHostRtcp,
                                 aCandidateInfo.mDefaultPortRtcp, aTransportId);
  }
  SignalCandidate(aCandidateInfo.mCandidate, aTransportId,
                  aCandidateInfo.mUfrag);
}

void PeerConnectionMedia::AlpnNegotiated_s(const std::string& aAlpn) {
  GetMainThread()->Dispatch(
      WrapRunnableNM(&PeerConnectionMedia::AlpnNegotiated_m, mParentHandle,
                     aAlpn),
      NS_DISPATCH_NORMAL);
}

void PeerConnectionMedia::AlpnNegotiated_m(const std::string& aParentHandle,
                                           const std::string& aAlpn) {
  PeerConnectionWrapper pcWrapper(aParentHandle);
  PeerConnectionImpl* pc = pcWrapper.impl();
  if (pc) {
    pc->OnAlpnNegotiated(aAlpn);
  }
}

/**
 * Tells you if any local track is isolated to a specific peer identity.
 * Obviously, we want all the tracks to be isolated equally so that they can
 * all be sent or not.  We check once when we are setting a local description
 * and that determines if we flip the "privacy requested" bit on.  Once the bit
 * is on, all media originating from this peer connection is isolated.
 *
 * @returns true if any track has a peerIdentity set on it
 */
bool PeerConnectionMedia::AnyLocalTrackHasPeerIdentity() const {
  ASSERT_ON_THREAD(mMainThread);

  for (const RefPtr<TransceiverImpl>& transceiver : mTransceivers) {
    if (transceiver->GetSendTrack() &&
        transceiver->GetSendTrack()->GetPeerIdentity()) {
      return true;
    }
  }
  return false;
}

void PeerConnectionMedia::UpdateRemoteStreamPrincipals_m(
    nsIPrincipal* aPrincipal) {
  ASSERT_ON_THREAD(mMainThread);

  for (RefPtr<TransceiverImpl>& transceiver : mTransceivers) {
    transceiver->UpdatePrincipal(aPrincipal);
  }
}

void PeerConnectionMedia::UpdateSinkIdentity_m(
    const MediaStreamTrack* aTrack, nsIPrincipal* aPrincipal,
    const PeerIdentity* aSinkIdentity) {
  ASSERT_ON_THREAD(mMainThread);

  for (RefPtr<TransceiverImpl>& transceiver : mTransceivers) {
    transceiver->UpdateSinkIdentity(aTrack, aPrincipal, aSinkIdentity);
  }
}

bool PeerConnectionMedia::AnyCodecHasPluginID(uint64_t aPluginID) {
  for (RefPtr<TransceiverImpl>& transceiver : mTransceivers) {
    if (transceiver->ConduitHasPluginID(aPluginID)) {
      return true;
    }
  }
  return false;
}

nsPIDOMWindowInner* PeerConnectionMedia::GetWindow() const {
  return mParent->GetWindow();
}
}  // namespace mozilla
