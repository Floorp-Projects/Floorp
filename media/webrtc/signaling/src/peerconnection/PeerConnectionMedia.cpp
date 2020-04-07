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
#include "nsIIDNService.h"
#include "nsILoadInfo.h"
#include "nsIProxyInfo.h"
#include "nsIPrincipal.h"
#include "mozilla/LoadInfo.h"
#include "nsIProxiedChannel.h"

#include "nsIScriptGlobalObject.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/net/NeckoChild.h"
#include "MediaManager.h"
#include "WebrtcGmpVideoCodec.h"

namespace mozilla {
using namespace dom;

static const char* pcmLogTag = "PeerConnectionMedia";
#ifdef LOGTAG
#  undef LOGTAG
#endif
#define LOGTAG pcmLogTag

void PeerConnectionMedia::StunAddrsHandler::OnMDNSQueryComplete(
    const nsCString& hostname, const Maybe<nsCString>& address) {
  ASSERT_ON_THREAD(pcm_->mMainThread);
  auto itor = pcm_->mQueriedMDNSHostnames.find(hostname.BeginReading());
  if (itor != pcm_->mQueriedMDNSHostnames.end()) {
    if (address) {
      for (auto& cand : itor->second) {
        // Replace obfuscated address with actual address
        std::string obfuscatedAddr = cand.mTokenizedCandidate[4];
        cand.mTokenizedCandidate[4] = address->BeginReading();
        std::ostringstream o;
        for (size_t i = 0; i < cand.mTokenizedCandidate.size(); ++i) {
          o << cand.mTokenizedCandidate[i];
          if (i + 1 != cand.mTokenizedCandidate.size()) {
            o << " ";
          }
        }
        std::string mungedCandidate = o.str();
        pcm_->mTransportHandler->AddIceCandidate(
            cand.mTransportId, mungedCandidate, cand.mUfrag, obfuscatedAddr);
      }
    }
    pcm_->mQueriedMDNSHostnames.erase(itor);
  }
}

void PeerConnectionMedia::StunAddrsHandler::OnStunAddrsAvailable(
    const mozilla::net::NrIceStunAddrArray& addrs) {
  CSFLogInfo(LOGTAG, "%s: receiving (%d) stun addrs", __FUNCTION__,
             (int)addrs.Length());
  if (pcm_) {
    pcm_->mStunAddrs = addrs;
    pcm_->mLocalAddrsRequestState = STUN_ADDR_REQUEST_COMPLETE;
    pcm_->FlushIceCtxOperationQueueIfReady();
    // If parent process returns 0 STUN addresses, change ICE connection
    // state to failed.
    if (!pcm_->mStunAddrs.Length()) {
      pcm_->IceConnectionStateChange_m(dom::RTCIceConnectionState::Failed);
    }
  }
}

PeerConnectionMedia::PeerConnectionMedia(PeerConnectionImpl* parent)
    : mTransportHandler(parent->GetTransportHandler()),
      mParent(parent),
      mParentHandle(parent->GetHandle()),
      mParentName(parent->GetName()),
      mMainThread(mParent->GetMainThread()),
      mSTSThread(mParent->GetSTSThread()),
      mForceProxy(false),
      mStunAddrsRequest(nullptr),
      mLocalAddrsRequestState(STUN_ADDR_REQUEST_NONE),
      mTargetForDefaultLocalAddressLookupIsSet(false),
      mDestroyed(false) {
  if (XRE_IsContentProcess()) {
    nsCOMPtr<nsISerialEventTarget> target =
        mParent->GetWindow()
            ? mParent->GetWindow()->EventTargetFor(TaskCategory::Other)
            : nullptr;

    mStunAddrsRequest =
        new net::StunAddrsRequestChild(new StunAddrsHandler(this), target);
  }
}

PeerConnectionMedia::~PeerConnectionMedia() {
  MOZ_RELEASE_ASSERT(!mMainThread);
}

void PeerConnectionMedia::InitLocalAddrs() {
  if (mLocalAddrsRequestState == STUN_ADDR_REQUEST_PENDING) {
    return;
  }
  if (mStunAddrsRequest) {
    mLocalAddrsRequestState = STUN_ADDR_REQUEST_PENDING;
    mStunAddrsRequest->SendGetStunAddrs();
  } else {
    mLocalAddrsRequestState = STUN_ADDR_REQUEST_COMPLETE;
  }
}

bool PeerConnectionMedia::ShouldForceProxy() const {
  if (Preferences::GetBool("media.peerconnection.ice.proxy_only", false)) {
    return true;
  }

  if (!Preferences::GetBool(
          "media.peerconnection.ice.proxy_only_if_behind_proxy", false)) {
    return false;
  }

  // Ok, we're supposed to be proxy_only, but only if a proxy is configured.
  // Let's just see if the document was loaded via a proxy.

  nsCOMPtr<nsIHttpChannelInternal> httpChannelInternal = GetChannel();
  if (!httpChannelInternal) {
    return false;
  }

  nsCOMPtr<nsIProxiedChannel> proxiedChannel =
      do_QueryInterface(httpChannelInternal);
  if (!proxiedChannel) {
    return false;
  }

  nsCOMPtr<nsIProxyInfo> proxyInfo;
  proxiedChannel->GetProxyInfo(getter_AddRefs(proxyInfo));
  if (!proxyInfo) {
    return false;
  }

  nsCString proxyType;
  proxyInfo->GetType(proxyType);

  return !proxyType.IsEmpty() && !proxyType.EqualsLiteral("direct");
}

nsresult PeerConnectionMedia::Init() {
  mForceProxy = ShouldForceProxy();

  // setup the stun local addresses IPC async call
  InitLocalAddrs();

  ConnectSignals();
  return NS_OK;
}

void PeerConnectionMedia::EnsureTransports(const JsepSession& aSession) {
  for (const auto& transceiver : aSession.GetTransceivers()) {
    if (transceiver->HasOwnTransport()) {
      mTransportHandler->EnsureProvisionalTransport(
          transceiver->mTransport.mTransportId,
          transceiver->mTransport.mLocalUfrag,
          transceiver->mTransport.mLocalPwd,
          transceiver->mTransport.mComponents);
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

  mTransportHandler->RemoveTransportsExcept(finalTransports);

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

  mTransportHandler->ActivateTransport(
      transport.mTransportId, transport.mLocalUfrag, transport.mLocalPwd,
      components, ufrag, pwd, keyDer, certDer, mParent->Identity()->auth_type(),
      transport.mDtls->GetRole() == JsepDtlsTransport::kJsepDtlsClient, digests,
      mParent->PrivacyRequested());

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
  ASSERT_ON_THREAD(mMainThread);

  if (!mCanRegisterMDNSHostnamesDirectly) {
    for (auto& pair : mMDNSHostnamesToRegister) {
      mRegisteredMDNSHostnames.insert(pair.first);
      mStunAddrsRequest->SendRegisterMDNSHostname(
          nsCString(pair.first.c_str()), nsCString(pair.second.c_str()));
    }
    mMDNSHostnamesToRegister.clear();
    mCanRegisterMDNSHostnamesDirectly = true;
  }

  std::vector<std::string> attributes;
  if (aSession.RemoteIsIceLite()) {
    attributes.push_back("ice-lite");
  }

  if (!aSession.GetIceOptions().empty()) {
    attributes.push_back("ice-options:");
    for (const auto& option : aSession.GetIceOptions()) {
      attributes.back() += option + ' ';
    }
  }

  nsCOMPtr<nsIRunnable> runnable(
      WrapRunnable(mTransportHandler, &MediaTransportHandler::StartIceChecks,
                   aSession.IsIceControlling(), attributes));

  PerformOrEnqueueIceCtxOperation(runnable);
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

static bool HostInDomain(const nsCString& aHost, const nsCString& aPattern) {
  int32_t patternOffset = 0;
  int32_t hostOffset = 0;

  // Act on '*.' wildcard in the left-most position in a domain pattern.
  if (aPattern.Length() > 2 && aPattern[0] == '*' && aPattern[1] == '.') {
    patternOffset = 2;

    // Ignore the lowest level sub-domain for the hostname.
    hostOffset = aHost.FindChar('.') + 1;

    if (hostOffset <= 1) {
      // Reject a match between a wildcard and a TLD or '.foo' form.
      return false;
    }
  }

  nsDependentCString hostRoot(aHost, hostOffset);
  return hostRoot.EqualsIgnoreCase(aPattern.BeginReading() + patternOffset);
}

static bool HostInObfuscationWhitelist(nsIURI* docURI) {
  if (!docURI) {
    return false;
  }

  nsCString hostName;
  docURI->GetAsciiHost(hostName);  // normalize UTF8 to ASCII equivalent
  nsCString domainWhiteList;
  nsresult nr = Preferences::GetCString(
      "media.peerconnection.ice.obfuscate_host_addresses.whitelist",
      domainWhiteList);

  if (NS_FAILED(nr)) {
    return false;
  }

  domainWhiteList.StripWhitespace();

  if (domainWhiteList.IsEmpty() || hostName.IsEmpty()) {
    return false;
  }

  // Get UTF8 to ASCII domain name normalization service
  nsresult rv;
  nsCOMPtr<nsIIDNService> idnService =
      do_GetService("@mozilla.org/network/idn-service;1", &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  uint32_t begin = 0;
  uint32_t end = 0;
  nsCString domainName;
  /*
     Test each domain name in the comma separated list
     after converting from UTF8 to ASCII. Each domain
     must match exactly or have a single leading '*.' wildcard
  */
  do {
    end = domainWhiteList.FindChar(',', begin);
    if (end == (uint32_t)-1) {
      // Last or only domain name in the comma separated list
      end = domainWhiteList.Length();
    }

    rv = idnService->ConvertUTF8toACE(
        Substring(domainWhiteList, begin, end - begin), domainName);
    if (NS_SUCCEEDED(rv)) {
      if (HostInDomain(hostName, domainName)) {
        return true;
      }
    } else {
      NS_WARNING("Failed to convert UTF-8 host to ASCII");
    }

    begin = end + 1;
  } while (end < domainWhiteList.Length());

  return false;
}

bool PeerConnectionMedia::GetPrefObfuscateHostAddresses() const {
  ASSERT_ON_THREAD(mMainThread);  // will crash on STS thread

  uint64_t winId = mParent->GetWindow()->WindowID();

  bool obfuscate_host_addresses = Preferences::GetBool(
      "media.peerconnection.ice.obfuscate_host_addresses", false);
  obfuscate_host_addresses &=
      !MediaManager::Get()->IsActivelyCapturingOrHasAPermission(winId);
  obfuscate_host_addresses &=
      !HostInObfuscationWhitelist(mParent->GetWindow()->GetDocumentURI());
  obfuscate_host_addresses &= XRE_IsContentProcess();

  return obfuscate_host_addresses;
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
  ASSERT_ON_THREAD(mMainThread);
  MOZ_ASSERT(!aTransportId.empty());

  bool obfuscate_host_addresses = Preferences::GetBool(
      "media.peerconnection.ice.obfuscate_host_addresses", false);

  if (obfuscate_host_addresses) {
    std::vector<std::string> tokens;
    TokenizeCandidate(aCandidate, tokens);

    if (tokens.size() > 4) {
      std::string addr = tokens[4];

      // Check for address ending with .local
      size_t nPeriods = std::count(addr.begin(), addr.end(), '.');
      size_t dotLocalLength = 6;  // length of ".local"

      if (nPeriods == 1 &&
          addr.rfind(".local") + dotLocalLength == addr.length()) {
        if (mStunAddrsRequest) {
          PendingIceCandidate cand;
          cand.mTokenizedCandidate = std::move(tokens);
          cand.mTransportId = aTransportId;
          cand.mUfrag = aUfrag;
          mQueriedMDNSHostnames[addr].push_back(cand);

          mMainThread->Dispatch(NS_NewRunnableFunction(
              "PeerConnectionMedia::SendQueryMDNSHostname",
              [self = RefPtr<PeerConnectionMedia>(this), addr]() mutable {
                if (self->mStunAddrsRequest) {
                  self->mStunAddrsRequest->SendQueryMDNSHostname(
                      nsCString(nsAutoCString(addr.c_str())));
                }
                NS_ReleaseOnMainThread(
                    "PeerConnectionMedia::SendQueryMDNSHostname",
                    self.forget());
              }));
        }
        // TODO: Bug 1535690, we don't want to tell the ICE context that remote
        // trickle is done if we are waiting to resolve a mDNS candidate.
        return;
      }
    }
  }

  mTransportHandler->AddIceCandidate(aTransportId, aCandidate, aUfrag, "");
}

void PeerConnectionMedia::UpdateNetworkState(bool online) {
  mTransportHandler->UpdateNetworkState(online);
}

void PeerConnectionMedia::FlushIceCtxOperationQueueIfReady() {
  ASSERT_ON_THREAD(mMainThread);

  if (IsIceCtxReady()) {
    for (auto& queuedIceCtxOperation : mQueuedIceCtxOperations) {
      queuedIceCtxOperation->Run();
    }
    mQueuedIceCtxOperations.clear();
  }
}

void PeerConnectionMedia::PerformOrEnqueueIceCtxOperation(
    nsIRunnable* runnable) {
  ASSERT_ON_THREAD(mMainThread);

  if (IsIceCtxReady()) {
    runnable->Run();
  } else {
    mQueuedIceCtxOperations.push_back(runnable);
  }
}

void PeerConnectionMedia::GatherIfReady() {
  ASSERT_ON_THREAD(mMainThread);

  // Init local addrs here so that if we re-gather after an ICE restart
  // resulting from changing WiFi networks, we get new local addrs.
  // Otherwise, we would reuse the addrs from the original WiFi network
  // and the ICE restart will fail.
  if (!mStunAddrs.Length()) {
    InitLocalAddrs();
  }

  // If we had previously queued gathering or ICE start, unqueue them
  mQueuedIceCtxOperations.clear();
  nsCOMPtr<nsIRunnable> runnable(WrapRunnable(
      RefPtr<PeerConnectionMedia>(this),
      &PeerConnectionMedia::EnsureIceGathering, GetPrefDefaultAddressOnly(),
      GetPrefObfuscateHostAddresses()));

  PerformOrEnqueueIceCtxOperation(runnable);
}

already_AddRefed<nsIHttpChannelInternal> PeerConnectionMedia::GetChannel()
    const {
  Document* doc = mParent->GetWindow()->GetExtantDoc();
  if (NS_WARN_IF(!doc)) {
    NS_WARNING("Unable to get document from window");
    return nullptr;
  }

  if (!doc->GetDocumentURI()->SchemeIs("file")) {
    nsIChannel* channel = doc->GetChannel();
    if (!channel) {
      NS_WARNING("Unable to get channel from document");
      return nullptr;
    }

    nsCOMPtr<nsIHttpChannelInternal> httpChannelInternal =
        do_QueryInterface(channel);
    if (NS_WARN_IF(!httpChannelInternal)) {
      CSFLogInfo(LOGTAG, "%s: Document does not have an HTTP channel",
                 __FUNCTION__);
      return nullptr;
    }
    return httpChannelInternal.forget();
  }
  return nullptr;
}

nsresult PeerConnectionMedia::SetTargetForDefaultLocalAddressLookup() {
  nsCOMPtr<nsIHttpChannelInternal> httpChannelInternal = GetChannel();
  if (!httpChannelInternal) {
    return NS_OK;
  }

  nsCString remoteIp;
  nsresult rv = httpChannelInternal->GetRemoteAddress(remoteIp);
  if (NS_FAILED(rv) || remoteIp.IsEmpty()) {
    CSFLogError(LOGTAG, "%s: Failed to get remote IP address: %d", __FUNCTION__,
                (int)rv);
    return rv;
  }

  int32_t remotePort;
  rv = httpChannelInternal->GetRemotePort(&remotePort);
  if (NS_FAILED(rv)) {
    CSFLogError(LOGTAG, "%s: Failed to get remote port number: %d",
                __FUNCTION__, (int)rv);
    return rv;
  }

  mTransportHandler->SetTargetForDefaultLocalAddressLookup(remoteIp.get(),
                                                           remotePort);

  return NS_OK;
}

void PeerConnectionMedia::EnsureIceGathering(bool aDefaultRouteOnly,
                                             bool aObfuscateHostAddresses) {
  auto proxyConfig = GetProxyConfig();
  if (proxyConfig) {
    // Note that this could check if PrivacyRequested() is set on the PC and
    // remove "webrtc" from the ALPN list.  But that would only work if the PC
    // was constructed with a peerIdentity constraint, not when isolated
    // streams are added.  If we ever need to signal to the proxy that the
    // media is isolated, then we would need to restructure this code.
    mTransportHandler->SetProxyConfig(std::move(*proxyConfig));
  }

  if (!mTargetForDefaultLocalAddressLookupIsSet) {
    nsresult rv = SetTargetForDefaultLocalAddressLookup();
    if (NS_FAILED(rv)) {
      NS_WARNING("Unable to set target for default local address lookup");
    }
    mTargetForDefaultLocalAddressLookupIsSet = true;
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

  mTransportHandler->StartIceGathering(aDefaultRouteOnly,
                                       aObfuscateHostAddresses, mStunAddrs);
}

void PeerConnectionMedia::SelfDestruct() {
  ASSERT_ON_THREAD(mMainThread);

  CSFLogDebug(LOGTAG, "%s: ", __FUNCTION__);

  mDestroyed = true;

  if (mStunAddrsRequest) {
    for (auto& hostname : mRegisteredMDNSHostnames) {
      mStunAddrsRequest->SendUnregisterMDNSHostname(
          nsCString(hostname.c_str()));
    }
    mRegisteredMDNSHostnames.clear();
    mStunAddrsRequest->Cancel();
    mStunAddrsRequest = nullptr;
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
  mParent = nullptr;

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
    JsepTransceiver* aJsepTransceiver, dom::MediaStreamTrack* aSendTrack,
    RefPtr<TransceiverImpl>* aTransceiverImpl) {
  if (!mCall) {
    mCall = WebRtcCallWrapper::Create(mParent->GetTimestampMaker());
  }

  RefPtr<TransceiverImpl> transceiver = new TransceiverImpl(
      mParent->GetWindow(), mParent->PrivacyNeeded(), mParent->GetHandle(),
      mTransportHandler, aJsepTransceiver, mMainThread.get(), mSTSThread.get(),
      aSendTrack, mCall.get());

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
    nsTArray<RefPtr<MediaPipelineTransmit>>* aPipelines) {
  for (RefPtr<TransceiverImpl>& transceiver : mTransceivers) {
    if (transceiver->HasSendTrack(aTrack)) {
      aPipelines->AppendElement(transceiver->GetSendPipeline());
    }
  }
}

std::string PeerConnectionMedia::GetTransportIdMatchingSendTrack(
    const dom::MediaStreamTrack& aTrack) const {
  for (const RefPtr<TransceiverImpl>& transceiver : mTransceivers) {
    if (transceiver->HasSendTrack(&aTrack)) {
      return transceiver->GetTransportId();
    }
  }
  return std::string();
}

void PeerConnectionMedia::IceGatheringStateChange_s(
    dom::RTCIceGatheringState aState) {
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
    dom::RTCIceConnectionState aState) {
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
    dom::RTCIceGatheringState aState) {
  ASSERT_ON_THREAD(mMainThread);
  if (mParent) {
    mParent->IceGatheringStateChange(aState);
  }
}

void PeerConnectionMedia::IceConnectionStateChange_m(
    dom::RTCIceConnectionState aState) {
  ASSERT_ON_THREAD(mMainThread);
  if (mParent) {
    mParent->IceConnectionStateChange(aState);
  }
}

void PeerConnectionMedia::OnCandidateFound_m(
    const std::string& aTransportId, const CandidateInfo& aCandidateInfo) {
  ASSERT_ON_THREAD(mMainThread);
  if (mStunAddrsRequest && !aCandidateInfo.mMDNSAddress.empty()) {
    MOZ_ASSERT(!aCandidateInfo.mActualAddress.empty());

    if (mCanRegisterMDNSHostnamesDirectly) {
      auto itor = mRegisteredMDNSHostnames.find(aCandidateInfo.mMDNSAddress);

      // We'll see the address twice if we're generating both UDP and TCP
      // candidates.
      if (itor == mRegisteredMDNSHostnames.end()) {
        mRegisteredMDNSHostnames.insert(aCandidateInfo.mMDNSAddress);
        mStunAddrsRequest->SendRegisterMDNSHostname(
            nsCString(aCandidateInfo.mMDNSAddress.c_str()),
            nsCString(aCandidateInfo.mActualAddress.c_str()));
      }
    } else {
      mMDNSHostnamesToRegister.emplace(aCandidateInfo.mMDNSAddress,
                                       aCandidateInfo.mActualAddress);
    }
  }

  if (mParent) {
    mParent->OnCandidateFound(aTransportId, aCandidateInfo);
  }
}

void PeerConnectionMedia::AlpnNegotiated_s(const std::string& aAlpn,
                                           bool aPrivacyRequested) {
  MOZ_DIAGNOSTIC_ASSERT((aAlpn == "c-webrtc") == aPrivacyRequested);
  GetMainThread()->Dispatch(
      WrapRunnable(this, &PeerConnectionMedia::AlpnNegotiated_m,
                   aPrivacyRequested),
      NS_DISPATCH_NORMAL);
}

void PeerConnectionMedia::AlpnNegotiated_m(bool aPrivacyRequested) {
  if (mParent) {
    mParent->OnAlpnNegotiated(aPrivacyRequested);
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

std::unique_ptr<NrSocketProxyConfig> PeerConnectionMedia::GetProxyConfig()
    const {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mForceProxy &&
      Preferences::GetBool("media.peerconnection.disable_http_proxy", false)) {
    return nullptr;
  }

  nsCString alpn = NS_LITERAL_CSTRING("webrtc,c-webrtc");
  auto browserChild = BrowserChild::GetFrom(GetWindow());
  if (!browserChild) {
    // Android doesn't have browser child apparently...
    return nullptr;
  }

  TabId id = browserChild->GetTabId();
  nsCOMPtr<nsILoadInfo> loadInfo = new net::LoadInfo(
      nsContentUtils::GetSystemPrincipal(), nullptr, nullptr, 0, 0);

  Maybe<net::LoadInfoArgs> loadInfoArgs;
  MOZ_ALWAYS_SUCCEEDS(
      mozilla::ipc::LoadInfoToLoadInfoArgs(loadInfo, &loadInfoArgs));
  return std::unique_ptr<NrSocketProxyConfig>(new NrSocketProxyConfig(
      net::WebrtcProxyConfig(id, alpn, *loadInfoArgs, mForceProxy)));
}

}  // namespace mozilla
