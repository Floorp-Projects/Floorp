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
#include "MediaPipelineFilter.h"
#include "MediaPipeline.h"
#include "PeerConnectionImpl.h"
#include "PeerConnectionMedia.h"
#include "runnable_utils.h"
#include "transportlayerice.h"
#include "transportlayerdtls.h"
#include "transportlayersrtp.h"
#include "signaling/src/jsep/JsepSession.h"
#include "signaling/src/jsep/JsepTransport.h"
#include "signaling/src/mediapipeline/TransportLayerPacketDumper.h"
#include "signaling/src/peerconnection/PacketDumper.h"

#include "nsContentUtils.h"
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

#include "nsIScriptGlobalObject.h"
#include "mozilla/Preferences.h"
#include "mozilla/Telemetry.h"
#include "MediaManager.h"
#include "WebrtcGmpVideoCodec.h"

namespace mozilla {
using namespace dom;

static const char* pcmLogTag = "PeerConnectionMedia";
#ifdef LOGTAG
#undef LOGTAG
#endif
#define LOGTAG pcmLogTag

NS_IMETHODIMP PeerConnectionMedia::ProtocolProxyQueryHandler::
OnProxyAvailable(nsICancelable *request,
                 nsIChannel *aChannel,
                 nsIProxyInfo *proxyinfo,
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

void
PeerConnectionMedia::ProtocolProxyQueryHandler::SetProxyOnPcm(
    nsIProxyInfo& proxyinfo)
{
  CSFLogInfo(LOGTAG, "%s: Had proxyinfo", __FUNCTION__);
  nsresult rv;
  nsCString httpsProxyHost;
  int32_t httpsProxyPort;

  rv = proxyinfo.GetHost(httpsProxyHost);
  if (NS_FAILED(rv)) {
    CSFLogError(LOGTAG, "%s: Failed to get proxy server host", __FUNCTION__);
    return;
  }

  rv = proxyinfo.GetPort(&httpsProxyPort);
  if (NS_FAILED(rv)) {
    CSFLogError(LOGTAG, "%s: Failed to get proxy server port", __FUNCTION__);
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
    CSFLogError(LOGTAG, "%s: Failed to set proxy server (ICE ctx unavailable)",
        __FUNCTION__);
  }
}

NS_IMPL_ISUPPORTS(PeerConnectionMedia::ProtocolProxyQueryHandler, nsIProtocolProxyCallback)

void
PeerConnectionMedia::StunAddrsHandler::OnStunAddrsAvailable(
    const mozilla::net::NrIceStunAddrArray& addrs)
{
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
      pcm_->SignalIceConnectionStateChange(pcm_->mIceCtxHdlr->ctx().get(),
                                           NrIceCtx::ICE_CTX_FAILED);
    }

    pcm_ = nullptr;
  }
}

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
      mIceRestartState(ICE_RESTART_NONE),
      mLocalAddrsCompleted(false) {
}

PeerConnectionMedia::~PeerConnectionMedia()
{
  MOZ_RELEASE_ASSERT(!mMainThread);
}

void
PeerConnectionMedia::InitLocalAddrs()
{
  if (XRE_IsContentProcess()) {
    CSFLogDebug(LOGTAG, "%s: Get stun addresses via IPC",
                mParentHandle.c_str());

    nsCOMPtr<nsIEventTarget> target = mParent->GetWindow()
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

nsresult
PeerConnectionMedia::InitProxy()
{
  // Allow mochitests to disable this, since mochitest configures a fake proxy
  // that serves up content.
  bool disable = Preferences::GetBool("media.peerconnection.disable_http_proxy",
                                      false);
  if (disable) {
    mProxyResolveCompleted = true;
    return NS_OK;
  }

  nsresult rv;
  nsCOMPtr<nsIProtocolProxyService> pps =
    do_GetService(NS_PROTOCOLPROXYSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    CSFLogError(LOGTAG, "%s: Failed to get proxy service: %d", __FUNCTION__, (int)rv);
    return NS_ERROR_FAILURE;
  }

  // We use the following URL to find the "default" proxy address for all HTTPS
  // connections.  We will only attempt one HTTP(S) CONNECT per peer connection.
  // "example.com" is guaranteed to be unallocated and should return the best default.
  nsCOMPtr<nsIURI> fakeHttpsLocation;
  rv = NS_NewURI(getter_AddRefs(fakeHttpsLocation), "https://example.com");
  if (NS_FAILED(rv)) {
    CSFLogError(LOGTAG, "%s: Failed to set URI: %d", __FUNCTION__, (int)rv);
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel),
                     fakeHttpsLocation,
                     nsContentUtils::GetSystemPrincipal(),
                     nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
                     nsIContentPolicy::TYPE_OTHER);

  if (NS_FAILED(rv)) {
    CSFLogError(LOGTAG, "%s: Failed to get channel from URI: %d",
                __FUNCTION__, (int)rv);
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIEventTarget> target = mParent->GetWindow()
      ? mParent->GetWindow()->EventTargetFor(TaskCategory::Network)
      : nullptr;
  RefPtr<ProtocolProxyQueryHandler> handler = new ProtocolProxyQueryHandler(this);
  rv = pps->AsyncResolve(channel,
                         nsIProtocolProxyService::RESOLVE_PREFER_HTTPS_PROXY |
                         nsIProtocolProxyService::RESOLVE_ALWAYS_TUNNEL,
                         handler, target, getter_AddRefs(mProxyRequest));
  if (NS_FAILED(rv)) {
    CSFLogError(LOGTAG, "%s: Failed to resolve protocol proxy: %d", __FUNCTION__, (int)rv);
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

  bool ice_tcp = Preferences::GetBool("media.peerconnection.ice.tcp", false);

  // setup the stun local addresses IPC async call
  InitLocalAddrs();

  // TODO(ekr@rtfm.com): need some way to set not offerer later
  // Looks like a bug in the NrIceCtx API.
  mIceCtxHdlr = NrIceCtxHandler::Create("PC:" + mParentName,
                                        mParent->GetAllowIceLoopback(),
                                        ice_tcp,
                                        mParent->GetAllowIceLinkLocal(),
                                        policy);
  if(!mIceCtxHdlr) {
    CSFLogError(LOGTAG, "%s: Failed to create Ice Context", __FUNCTION__);
    return NS_ERROR_FAILURE;
  }

  if (NS_FAILED(rv = mIceCtxHdlr->ctx()->SetStunServers(stun_servers))) {
    CSFLogError(LOGTAG, "%s: Failed to set stun servers", __FUNCTION__);
    return rv;
  }
  // Give us a way to globally turn off TURN support
  bool disabled = Preferences::GetBool("media.peerconnection.turn.disable", false);
  if (!disabled) {
    if (NS_FAILED(rv = mIceCtxHdlr->ctx()->SetTurnServers(turn_servers))) {
      CSFLogError(LOGTAG, "%s: Failed to set turn servers", __FUNCTION__);
      return rv;
    }
  } else if (!turn_servers.empty()) {
    CSFLogError(LOGTAG, "%s: Setting turn servers disabled", __FUNCTION__);
  }
  if (NS_FAILED(rv = mDNSResolver->Init())) {
    CSFLogError(LOGTAG, "%s: Failed to initialize dns resolver", __FUNCTION__);
    return rv;
  }
  if (NS_FAILED(rv =
      mIceCtxHdlr->ctx()->SetResolver(mDNSResolver->AllocateResolver()))) {
    CSFLogError(LOGTAG, "%s: Failed to get dns resolver", __FUNCTION__);
    return rv;
  }
  ConnectSignals(mIceCtxHdlr->ctx().get());

  return NS_OK;
}

void
PeerConnectionMedia::EnsureTransports(const JsepSession& aSession)
{
  for (const auto& transceiver : aSession.GetTransceivers()) {
    if (!transceiver->HasLevel()) {
      continue;
    }

    RefPtr<JsepTransport> transport = transceiver->mTransport;
    RUN_ON_THREAD(
        GetSTSThread(),
        WrapRunnable(RefPtr<PeerConnectionMedia>(this),
                     &PeerConnectionMedia::EnsureTransport_s,
                     transceiver->GetLevel(),
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
    CSFLogDebug(LOGTAG, "%s: Creating ICE media stream=%u components=%u",
                mParentHandle.c_str(),
                static_cast<unsigned>(aLevel),
                static_cast<unsigned>(aComponentCount));

    std::ostringstream os;
    os << mParentName << " aLevel=" << aLevel;
    RefPtr<NrIceMediaStream> stream =
      mIceCtxHdlr->CreateStream(os.str(),
                                aComponentCount);

    if (!stream) {
      CSFLogError(LOGTAG, "Failed to create ICE stream.");
      return;
    }

    stream->SetLevel(aLevel);
    stream->SignalReady.connect(this, &PeerConnectionMedia::IceStreamReady_s);
    stream->SignalCandidate.connect(this,
                                    &PeerConnectionMedia::OnCandidateFound_s);
    mIceCtxHdlr->ctx()->SetStream(aLevel, stream);
  }
}

nsresult
PeerConnectionMedia::ActivateOrRemoveTransports(const JsepSession& aSession,
                                                const bool forceIceTcp)
{
  for (const auto& transceiver : aSession.GetTransceivers()) {
    if (!transceiver->HasLevel()) {
      continue;
    }

    std::string ufrag;
    std::string pwd;
    std::vector<std::string> candidates;
    size_t components = 0;

    RefPtr<JsepTransport> transport = transceiver->mTransport;
    unsigned level = transceiver->GetLevel();

    if (transport->mComponents &&
        (!transceiver->HasBundleLevel() ||
         (transceiver->BundleLevel() == level))) {
      CSFLogDebug(LOGTAG, "ACTIVATING TRANSPORT! - PC %s: level=%u components=%u",
                  mParentHandle.c_str(), (unsigned)level,
                  (unsigned)transport->mComponents);

      ufrag = transport->mIce->GetUfrag();
      pwd = transport->mIce->GetPassword();
      candidates = transport->mIce->GetCandidates();
      components = transport->mComponents;
      if (forceIceTcp) {
        candidates.erase(std::remove_if(candidates.begin(),
                                        candidates.end(),
                                        [](const std::string & s) {
                                          return s.find(" UDP ") != std::string::npos ||
                                                 s.find(" udp ") != std::string::npos; }),
                         candidates.end());
      }
    }

    RUN_ON_THREAD(
        GetSTSThread(),
        WrapRunnable(RefPtr<PeerConnectionMedia>(this),
                     &PeerConnectionMedia::ActivateOrRemoveTransport_s,
                     transceiver->GetLevel(),
                     components,
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
                   aSession.GetTransceivers().size()),
      NS_DISPATCH_NORMAL);

  return NS_OK;
}

nsresult
PeerConnectionMedia::UpdateTransceiverTransports(const JsepSession& aSession)
{
  for (const auto& transceiver : aSession.GetTransceivers()) {
    nsresult rv = UpdateTransportFlows(*transceiver);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  for (const auto& transceiverImpl : mTransceivers) {
    transceiverImpl->UpdateTransport(*this);
  }

  return NS_OK;
}

void
PeerConnectionMedia::ActivateOrRemoveTransport_s(
    size_t aMLine,
    size_t aComponentCount,
    const std::string& aUfrag,
    const std::string& aPassword,
    const std::vector<std::string>& aCandidateList) {

  if (!aComponentCount) {
    CSFLogDebug(LOGTAG, "%s: Removing ICE media stream=%u",
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
    CSFLogDebug(LOGTAG, "%s: Activating ICE media stream=%u components=%u",
                mParentHandle.c_str(),
                static_cast<unsigned>(aMLine),
                static_cast<unsigned>(aComponentCount));

    std::vector<std::string> attrs;
    attrs.reserve(aCandidateList.size() + 2 /* ufrag + pwd */);
    for (const auto& candidate : aCandidateList) {
      attrs.push_back("candidate:" + candidate);
    }
    attrs.push_back("ice-ufrag:" + aUfrag);
    attrs.push_back("ice-pwd:" + aPassword);

    nsresult rv = stream->ParseAttributes(attrs);
    if (NS_FAILED(rv)) {
      CSFLogError(LOGTAG, "Couldn't parse ICE attributes, rv=%u",
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

nsresult
PeerConnectionMedia::UpdateMediaPipelines()
{
  // The GMP code is all the way on the other side of webrtc.org, and it is not
  // feasible to plumb error information all the way back. So, we set up a
  // handle to the PC (for the duration of this call) in a global variable.
  // This allows the GMP code to report errors to the PC.
  WebrtcGmpPCHandleSetter setter(mParentHandle);

  for (RefPtr<TransceiverImpl>& transceiver : mTransceivers) {
    nsresult rv = transceiver->UpdateConduit();
    if (NS_FAILED(rv)) {
      return rv;
    }

    if (!transceiver->IsVideo()) {
      rv = transceiver->SyncWithMatchingVideoConduits(mTransceivers);
      if (NS_FAILED(rv)) {
        return rv;
      }
      // TODO: If there is no audio, we should probably de-sync. However, this
      // has never been done before, and it is unclear whether it is safe...
    }
  }

  return NS_OK;
}

nsresult
PeerConnectionMedia::UpdateTransportFlows(const JsepTransceiver& aTransceiver)
{
  if (!aTransceiver.HasLevel()) {
    // Nothing to do
    return NS_OK;
  }

  size_t transportLevel = aTransceiver.GetTransportLevel();

  nsresult rv =
    UpdateTransportFlow(transportLevel, false, *aTransceiver.mTransport);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return UpdateTransportFlow(transportLevel, true, *aTransceiver.mTransport);
}

// Accessing the PCMedia should be safe here because we shouldn't
// have enqueued this function unless it was still active and
// the ICE data is destroyed on the STS.
static void
FinalizeTransportFlow_s(RefPtr<PeerConnectionMedia> aPCMedia,
                        nsAutoPtr<PacketDumper> aPacketDumper,
                        RefPtr<TransportFlow> aFlow, size_t aLevel,
                        bool aIsRtcp,
                        TransportLayerIce* aIceLayer,
                        TransportLayerDtls* aDtlsLayer,
                        TransportLayerSrtp* aSrtpLayer)
{
  TransportLayerPacketDumper* srtpDumper(new TransportLayerPacketDumper(
        std::move(aPacketDumper), dom::mozPacketDumpType::Srtp));

  aIceLayer->SetParameters(aPCMedia->ice_media_stream(aLevel),
                           aIsRtcp ? 2 : 1);
  // TODO(bug 854518): Process errors.
  (void)aIceLayer->Init();
  (void)aDtlsLayer->Init();
  (void)srtpDumper->Init();
  (void)aSrtpLayer->Init();
  aDtlsLayer->Chain(aIceLayer);
  srtpDumper->Chain(aIceLayer);
  aSrtpLayer->Chain(srtpDumper);
  aFlow->PushLayer(aIceLayer);
  aFlow->PushLayer(aDtlsLayer);
  aFlow->PushLayer(srtpDumper);
  aFlow->PushLayer(aSrtpLayer);
}

static void
AddNewIceStreamForRestart_s(RefPtr<PeerConnectionMedia> aPCMedia,
                            RefPtr<TransportFlow> aFlow,
                            size_t aLevel,
                            bool aIsRtcp)
{
  TransportLayerIce* ice =
      static_cast<TransportLayerIce*>(aFlow->GetLayer("ice"));
  ice->SetParameters(aPCMedia->ice_media_stream(aLevel),
                     aIsRtcp ? 2 : 1);
}

nsresult
PeerConnectionMedia::UpdateTransportFlow(
    size_t aLevel,
    bool aIsRtcp,
    const JsepTransport& aTransport)
{
  if (aIsRtcp && aTransport.mComponents < 2) {
    RemoveTransportFlow(aLevel, aIsRtcp);
    return NS_OK;
  }

  if (!aIsRtcp && !aTransport.mComponents) {
    RemoveTransportFlow(aLevel, aIsRtcp);
    return NS_OK;
  }

  nsresult rv;

  RefPtr<TransportFlow> flow = GetTransportFlow(aLevel, aIsRtcp);
  if (flow) {
    if (IsIceRestarting()) {
      CSFLogInfo(LOGTAG, "Flow[%s]: detected ICE restart - level: %u rtcp: %d",
                 flow->id().c_str(), (unsigned)aLevel, aIsRtcp);

      RefPtr<PeerConnectionMedia> pcMedia(this);
      rv = GetSTSThread()->Dispatch(
          WrapRunnableNM(AddNewIceStreamForRestart_s,
                         pcMedia, flow, aLevel, aIsRtcp),
          NS_DISPATCH_NORMAL);
      if (NS_FAILED(rv)) {
        CSFLogError(LOGTAG, "Failed to dispatch AddNewIceStreamForRestart_s");
        return rv;
      }
    }

    return NS_OK;
  }

  std::ostringstream osId;
  osId << mParentHandle << ":" << aLevel << "," << (aIsRtcp ? "rtcp" : "rtp");
  flow = new TransportFlow(osId.str());

  // The media streams are made on STS so we need to defer setup.
  auto ice = MakeUnique<TransportLayerIce>();
  auto dtls = MakeUnique<TransportLayerDtls>();
  auto srtp = MakeUnique<TransportLayerSrtp>(*dtls);
  dtls->SetRole(aTransport.mDtls->GetRole() ==
                        JsepDtlsTransport::kJsepDtlsClient
                    ? TransportLayerDtls::CLIENT
                    : TransportLayerDtls::SERVER);

  RefPtr<DtlsIdentity> pcid = mParent->Identity();
  if (!pcid) {
    CSFLogError(LOGTAG, "Failed to get DTLS identity.");
    return NS_ERROR_FAILURE;
  }
  dtls->SetIdentity(pcid);

  const SdpFingerprintAttributeList& fingerprints =
      aTransport.mDtls->GetFingerprints();
  for (const auto& fingerprint : fingerprints.mFingerprints) {
    std::ostringstream ss;
    ss << fingerprint.hashFunc;
    rv = dtls->SetVerificationDigest(ss.str(), &fingerprint.fingerprint[0],
                                     fingerprint.fingerprint.size());
    if (NS_FAILED(rv)) {
      CSFLogError(LOGTAG, "Could not set fingerprint");
      return rv;
    }
  }

  std::vector<uint16_t> srtpCiphers;
  srtpCiphers.push_back(SRTP_AES128_CM_HMAC_SHA1_80);
  srtpCiphers.push_back(SRTP_AES128_CM_HMAC_SHA1_32);

  rv = dtls->SetSrtpCiphers(srtpCiphers);
  if (NS_FAILED(rv)) {
    CSFLogError(LOGTAG, "Couldn't set SRTP ciphers");
    return rv;
  }

  // Always permits negotiation of the confidential mode.
  // Only allow non-confidential (which is an allowed default),
  // if we aren't confidential.
  std::set<std::string> alpn;
  std::string alpnDefault = "";
  alpn.insert("c-webrtc");
  if (!mParent->PrivacyRequested()) {
    alpnDefault = "webrtc";
    alpn.insert(alpnDefault);
  }
  rv = dtls->SetAlpn(alpn, alpnDefault);
  if (NS_FAILED(rv)) {
    CSFLogError(LOGTAG, "Couldn't set ALPN");
    return rv;
  }

  nsAutoPtr<PacketDumper> packetDumper(new PacketDumper(mParent));

  RefPtr<PeerConnectionMedia> pcMedia(this);
  rv = GetSTSThread()->Dispatch(
      WrapRunnableNM(FinalizeTransportFlow_s, pcMedia, packetDumper, flow,
                     aLevel, aIsRtcp,
                     ice.release(), dtls.release(), srtp.release()),
      NS_DISPATCH_NORMAL);
  if (NS_FAILED(rv)) {
    CSFLogError(LOGTAG, "Failed to dispatch FinalizeTransportFlow_s");
    return rv;
  }

  AddTransportFlow(aLevel, aIsRtcp, flow);

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
        aSession.IsOfferer(),
        aSession.RemoteIsIceLite(),
        // Copy, just in case API changes to return a ref
        std::vector<std::string>(aSession.GetIceOptions())));

  PerformOrEnqueueIceCtxOperation(runnable);
}

void
PeerConnectionMedia::StartIceChecks_s(
    bool aIsControlling,
    bool aIsOfferer,
    bool aIsIceLite,
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

  nsresult rv = mIceCtxHdlr->ctx()->ParseGlobalAttributes(attributes);
  if (NS_FAILED(rv)) {
    CSFLogError(LOGTAG, "%s: couldn't parse global parameters", __FUNCTION__ );
  }

  mIceCtxHdlr->ctx()->SetControlling(aIsControlling ?
                                     NrIceCtx::ICE_CONTROLLING :
                                     NrIceCtx::ICE_CONTROLLED);

  mIceCtxHdlr->ctx()->StartChecks(aIsOfferer);
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
  for (auto& transportFlow : mTransportFlows) {
    RefPtr<TransportFlow> aFlow = transportFlow.second;
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
  for (auto& transportFlow : mTransportFlows) {
    RefPtr<TransportFlow> aFlow = transportFlow.second;
    if (!aFlow) continue;
    TransportLayerIce* ice =
      static_cast<TransportLayerIce*>(aFlow->GetLayer(TransportLayerIce::ID()));
    ice->RestoreOldStream();
  }

  mIceCtxHdlr->RollbackIceRestart();
  ConnectSignals(mIceCtxHdlr->ctx().get(), restartCtx.get());

  // Fixup the telemetry by transferring abandoned ctx stats to current ctx.
  NrIceStats stats = restartCtx->Destroy();
  restartCtx = nullptr;
  mIceCtxHdlr->ctx()->AccumulateStats(stats);
}

bool
PeerConnectionMedia::GetPrefDefaultAddressOnly() const
{
  ASSERT_ON_THREAD(mMainThread); // will crash on STS thread

  uint64_t winId = mParent->GetWindow()->WindowID();

  bool default_address_only = Preferences::GetBool(
    "media.peerconnection.ice.default_address_only", false);
  default_address_only |=
    !MediaManager::Get()->IsActivelyCapturingOrHasAPermission(winId);
  return default_address_only;
}

bool
PeerConnectionMedia::GetPrefProxyOnly() const
{
  ASSERT_ON_THREAD(mMainThread); // will crash on STS thread

  return Preferences::GetBool("media.peerconnection.ice.proxy_only", false);
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
    CSFLogError(LOGTAG, "No ICE stream for candidate at level %u: %s",
                        static_cast<unsigned>(aMLine), aCandidate.c_str());
    return;
  }

  nsresult rv = stream->ParseTrickleCandidate(aCandidate);
  if (NS_FAILED(rv)) {
    CSFLogError(LOGTAG, "Couldn't process ICE candidate at level %u",
                static_cast<unsigned>(aMLine));
    return;
  }
}

void
PeerConnectionMedia::UpdateNetworkState(bool online) {
  RUN_ON_THREAD(GetSTSThread(),
                WrapRunnable(
                    RefPtr<PeerConnectionMedia>(this),
                    &PeerConnectionMedia::UpdateNetworkState_s,
                    online),
                NS_DISPATCH_NORMAL);
}

void
PeerConnectionMedia::UpdateNetworkState_s(bool online) {
  mIceCtxHdlr->ctx()->UpdateNetworkState(online);
}

void
PeerConnectionMedia::FlushIceCtxOperationQueueIfReady()
{
  ASSERT_ON_THREAD(mMainThread);

  if (IsIceCtxReady()) {
    for (auto& mQueuedIceCtxOperation : mQueuedIceCtxOperations) {
      GetSTSThread()->Dispatch(mQueuedIceCtxOperation, NS_DISPATCH_NORMAL);
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

  // Make sure we don't call NrIceCtx::StartGathering if we're in e10s mode
  // and we received no STUN addresses from the parent process.  In the
  // absence of previously provided STUN addresses, StartGathering will
  // attempt to gather them (as in non-e10s mode), and this will cause a
  // sandboxing exception in e10s mode.
  if (!mStunAddrs.Length() && XRE_IsContentProcess()) {
    CSFLogInfo(LOGTAG,
               "%s: No STUN addresses returned from parent process",
               __FUNCTION__);
    return;
  }

  // Belt and suspenders - in e10s mode, the call below to SetStunAddrs
  // needs to have the proper flags set on ice ctx.  For non-e10s,
  // setting those flags happens in StartGathering.  We could probably
  // just set them here, and only do it here.
  mIceCtxHdlr->ctx()->SetCtxFlags(aDefaultRouteOnly, aProxyOnly);

  if (mStunAddrs.Length()) {
    mIceCtxHdlr->ctx()->SetStunAddrs(mStunAddrs);
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

void
PeerConnectionMedia::SelfDestruct()
{
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
  RUN_ON_THREAD(mSTSThread, WrapRunnable(
      this, &PeerConnectionMedia::ShutdownMediaTransport_s),
                NS_DISPATCH_NORMAL);

  CSFLogDebug(LOGTAG, "%s: Media shut down", __FUNCTION__);
}

void
PeerConnectionMedia::SelfDestruct_m()
{
  CSFLogDebug(LOGTAG, "%s: ", __FUNCTION__);

  ASSERT_ON_THREAD(mMainThread);

  mMainThread = nullptr;

  // Final self-destruct.
  this->Release();
}

void
PeerConnectionMedia::ShutdownMediaTransport_s()
{
  ASSERT_ON_THREAD(mSTSThread);

  CSFLogDebug(LOGTAG, "%s: ", __FUNCTION__);

  disconnect_all();
  mTransportFlows.clear();

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  NrIceStats stats = mIceCtxHdlr->Destroy();

  CSFLogDebug(LOGTAG, "Ice Telemetry: stun (retransmits: %d)"
                      "   turn (401s: %d   403s: %d   438s: %d)",
              stats.stun_retransmits, stats.turn_401s, stats.turn_403s,
              stats.turn_438s);

  Telemetry::ScalarAdd(Telemetry::ScalarID::WEBRTC_NICER_STUN_RETRANSMITS,
                       stats.stun_retransmits);
  Telemetry::ScalarAdd(Telemetry::ScalarID::WEBRTC_NICER_TURN_401S,
                       stats.turn_401s);
  Telemetry::ScalarAdd(Telemetry::ScalarID::WEBRTC_NICER_TURN_403S,
                       stats.turn_403s);
  Telemetry::ScalarAdd(Telemetry::ScalarID::WEBRTC_NICER_TURN_438S,
                       stats.turn_438s);
#endif

  mIceCtxHdlr = nullptr;

  // we're holding a ref to 'this' that's released by SelfDestruct_m
  mMainThread->Dispatch(WrapRunnable(this, &PeerConnectionMedia::SelfDestruct_m),
                        NS_DISPATCH_NORMAL);
}

nsresult
PeerConnectionMedia::AddTransceiver(
    JsepTransceiver* aJsepTransceiver,
    dom::MediaStreamTrack& aReceiveTrack,
    dom::MediaStreamTrack* aSendTrack,
    RefPtr<TransceiverImpl>* aTransceiverImpl)
{
  if (!mCall) {
    mCall = WebRtcCallWrapper::Create();
  }

  RefPtr<TransceiverImpl> transceiver = new TransceiverImpl(
      mParent->GetHandle(),
      aJsepTransceiver,
      mMainThread.get(),
      mSTSThread.get(),
      &aReceiveTrack,
      aSendTrack,
      mCall.get());

  if (!transceiver->IsValid()) {
    return NS_ERROR_FAILURE;
  }

  if (aSendTrack) {
    // implement checking for peerIdentity (where failure == black/silence)
    nsIDocument* doc = mParent->GetWindow()->GetExtantDoc();
    if (doc) {
      transceiver->UpdateSinkIdentity(nullptr,
                                      doc->NodePrincipal(),
                                      mParent->GetPeerIdentity());
    } else {
      MOZ_CRASH();
      return NS_ERROR_FAILURE; // Don't remove this till we know it's safe.
    }
  }

  mTransceivers.push_back(transceiver);
  *aTransceiverImpl = transceiver;

  return NS_OK;
}

void
PeerConnectionMedia::GetTransmitPipelinesMatching(
    MediaStreamTrack* aTrack,
    nsTArray<RefPtr<MediaPipeline>>* aPipelines)
{
  for (RefPtr<TransceiverImpl>& transceiver : mTransceivers) {
    if (transceiver->HasSendTrack(aTrack)) {
      aPipelines->AppendElement(transceiver->GetSendPipeline());
    }
  }

  if (!aPipelines->Length()) {
    CSFLogWarn(LOGTAG, "%s: none found for %p", __FUNCTION__, aTrack);
  }
}

void
PeerConnectionMedia::GetReceivePipelinesMatching(
    MediaStreamTrack* aTrack,
    nsTArray<RefPtr<MediaPipeline>>* aPipelines)
{
  for (RefPtr<TransceiverImpl>& transceiver : mTransceivers) {
    if (transceiver->HasReceiveTrack(aTrack)) {
      aPipelines->AppendElement(transceiver->GetReceivePipeline());
    }
  }

  if (!aPipelines->Length()) {
    CSFLogWarn(LOGTAG, "%s: none found for %p", __FUNCTION__, aTrack);
  }
}

nsresult
PeerConnectionMedia::AddRIDExtension(MediaStreamTrack& aRecvTrack,
                                     unsigned short aExtensionId)
{
  for (RefPtr<TransceiverImpl>& transceiver : mTransceivers) {
    if (transceiver->HasReceiveTrack(&aRecvTrack)) {
      transceiver->AddRIDExtension(aExtensionId);
    }
  }
  return NS_OK;
}

nsresult
PeerConnectionMedia::AddRIDFilter(MediaStreamTrack& aRecvTrack,
                                  const nsAString& aRid)
{
  for (RefPtr<TransceiverImpl>& transceiver : mTransceivers) {
    if (transceiver->HasReceiveTrack(&aRecvTrack)) {
      transceiver->AddRIDFilter(aRid);
    }
  }
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

  CSFLogDebug(LOGTAG, "%s: %s", __FUNCTION__, aStream->name().c_str());

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
    CSFLogError(LOGTAG, "%s: GetDefaultCandidates failed for level %u, "
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

  CSFLogDebug(LOGTAG, "%s: %s", __FUNCTION__, aStream->name().c_str());
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
  NS_ProxyRelease(
    "PeerConnectionMedia::mTransportFlows",
    GetSTSThread(), mTransportFlows[index_inner].forget());
}

void
PeerConnectionMedia::ConnectDtlsListener_s(const RefPtr<TransportFlow>& aFlow)
{
  TransportLayer* dtls = aFlow->GetLayer(TransportLayerDtls::ID());
  if (dtls) {
    dtls->SignalStateChange.connect(this, &PeerConnectionMedia::DtlsConnected_s);
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
bool
PeerConnectionMedia::AnyLocalTrackHasPeerIdentity() const
{
  ASSERT_ON_THREAD(mMainThread);

  for (const RefPtr<TransceiverImpl>& transceiver : mTransceivers) {
    if (transceiver->GetSendTrack() &&
        transceiver->GetSendTrack()->GetPeerIdentity()) {
      return true;
    }
  }
  return false;
}

void
PeerConnectionMedia::UpdateRemoteStreamPrincipals_m(nsIPrincipal* aPrincipal)
{
  ASSERT_ON_THREAD(mMainThread);

  for (RefPtr<TransceiverImpl>& transceiver : mTransceivers) {
    transceiver->UpdatePrincipal(aPrincipal);
  }
}

void
PeerConnectionMedia::UpdateSinkIdentity_m(const MediaStreamTrack* aTrack,
                                          nsIPrincipal* aPrincipal,
                                          const PeerIdentity* aSinkIdentity)
{
  ASSERT_ON_THREAD(mMainThread);

  for (RefPtr<TransceiverImpl>& transceiver : mTransceivers) {
    transceiver->UpdateSinkIdentity(aTrack, aPrincipal, aSinkIdentity);
  }
}

bool
PeerConnectionMedia::AnyCodecHasPluginID(uint64_t aPluginID)
{
  for (RefPtr<TransceiverImpl>& transceiver : mTransceivers) {
    if (transceiver->ConduitHasPluginID(aPluginID)) {
      return true;
    }
  }
  return false;
}

} // namespace mozilla
