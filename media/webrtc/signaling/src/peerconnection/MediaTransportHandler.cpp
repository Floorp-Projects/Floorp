/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaTransportHandler.h"
#include "nricemediastream.h"
#include "nriceresolver.h"
#include "transportflow.h"
#include "transportlayerice.h"
#include "transportlayerdtls.h"
#include "transportlayersrtp.h"

// Config stuff
#include "nsIPrefService.h"
#include "mozilla/dom/RTCConfigurationBinding.h"

// Parsing STUN/TURN URIs
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsURLHelper.h"
#include "nsIURLParser.h"

// Logging stuff
#include "CSFLog.h"

// For fetching ICE logging
#include "rlogconnector.h"

// DTLS
#include "signaling/src/sdp/SdpAttribute.h"

#include "mozilla/Telemetry.h"

#include "mozilla/dom/RTCStatsReportBinding.h"

#include <string>
#include <vector>

namespace mozilla {

static const char* mthLogTag = "MediaTransportHandler";
#ifdef LOGTAG
#undef LOGTAG
#endif
#define LOGTAG mthLogTag

MediaTransportHandler::MediaTransportHandler() {}

MediaTransportHandler::~MediaTransportHandler() {}

static NrIceCtx::Policy toNrIcePolicy(dom::RTCIceTransportPolicy aPolicy) {
  switch (aPolicy) {
    case dom::RTCIceTransportPolicy::Relay:
      return NrIceCtx::ICE_POLICY_RELAY;
    case dom::RTCIceTransportPolicy::All:
      if (Preferences::GetBool("media.peerconnection.ice.no_host", false)) {
        return NrIceCtx::ICE_POLICY_NO_HOST;
      } else {
        return NrIceCtx::ICE_POLICY_ALL;
      }
    default:
      MOZ_CRASH();
  }
  return NrIceCtx::ICE_POLICY_ALL;
}

static nsresult addNrIceServer(const nsString& aIceUrl,
                               const dom::RTCIceServer& aIceServer,
                               std::vector<NrIceStunServer>* aStunServersOut,
                               std::vector<NrIceTurnServer>* aTurnServersOut) {
  // Without STUN/TURN handlers, NS_NewURI returns nsSimpleURI rather than
  // nsStandardURL. To parse STUN/TURN URI's to spec
  // http://tools.ietf.org/html/draft-nandakumar-rtcweb-stun-uri-02#section-3
  // http://tools.ietf.org/html/draft-petithuguenin-behave-turn-uri-03#section-3
  // we parse out the query-string, and use ParseAuthority() on the rest
  RefPtr<nsIURI> url;
  nsresult rv = NS_NewURI(getter_AddRefs(url), aIceUrl);
  NS_ENSURE_SUCCESS(rv, rv);
  bool isStun = false, isStuns = false, isTurn = false, isTurns = false;
  url->SchemeIs("stun", &isStun);
  url->SchemeIs("stuns", &isStuns);
  url->SchemeIs("turn", &isTurn);
  url->SchemeIs("turns", &isTurns);
  if (!(isStun || isStuns || isTurn || isTurns)) {
    return NS_ERROR_FAILURE;
  }
  if (isStuns) {
    return NS_OK;  // TODO: Support STUNS (Bug 1056934)
  }

  nsAutoCString spec;
  rv = url->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  // TODO(jib@mozilla.com): Revisit once nsURI supports STUN/TURN (Bug 833509)
  int32_t port;
  nsAutoCString host;
  nsAutoCString transport;
  {
    uint32_t hostPos;
    int32_t hostLen;
    nsAutoCString path;
    rv = url->GetPathQueryRef(path);
    NS_ENSURE_SUCCESS(rv, rv);

    // Tolerate query-string + parse 'transport=[udp|tcp]' by hand.
    int32_t questionmark = path.FindChar('?');
    if (questionmark >= 0) {
      const nsCString match = NS_LITERAL_CSTRING("transport=");

      for (int32_t i = questionmark, endPos; i >= 0; i = endPos) {
        endPos = path.FindCharInSet("&", i + 1);
        const nsDependentCSubstring fieldvaluepair =
            Substring(path, i + 1, endPos);
        if (StringBeginsWith(fieldvaluepair, match)) {
          transport = Substring(fieldvaluepair, match.Length());
          ToLowerCase(transport);
        }
      }
      path.SetLength(questionmark);
    }

    rv = net_GetAuthURLParser()->ParseAuthority(
        path.get(), path.Length(), nullptr, nullptr, nullptr, nullptr, &hostPos,
        &hostLen, &port);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!hostLen) {
      return NS_ERROR_FAILURE;
    }
    if (hostPos > 1) /* The username was removed */
      return NS_ERROR_FAILURE;
    path.Mid(host, hostPos, hostLen);
  }
  if (port == -1) port = (isStuns || isTurns) ? 5349 : 3478;

  if (isStuns || isTurns) {
    // Should we barf if transport is set to udp or something?
    transport = kNrIceTransportTls;
  }

  if (transport.IsEmpty()) {
    transport = kNrIceTransportUdp;
  }

  if (isTurn || isTurns) {
    std::string pwd(
        NS_ConvertUTF16toUTF8(aIceServer.mCredential.Value()).get());
    std::string username(
        NS_ConvertUTF16toUTF8(aIceServer.mUsername.Value()).get());

    std::vector<unsigned char> password(pwd.begin(), pwd.end());

    UniquePtr<NrIceTurnServer> server(NrIceTurnServer::Create(
        host.get(), port, username, password, transport.get()));
    if (!server) {
      return NS_ERROR_FAILURE;
    }
    aTurnServersOut->emplace_back(std::move(*server));
  } else {
    UniquePtr<NrIceStunServer> server(
        NrIceStunServer::Create(host.get(), port, transport.get()));
    if (!server) {
      return NS_ERROR_FAILURE;
    }
    aStunServersOut->emplace_back(std::move(*server));
  }
  return NS_OK;
}

nsresult MediaTransportHandler::Init(
    const std::string& aName, const dom::RTCConfiguration& aConfiguration) {
  std::vector<NrIceStunServer> stunServers;
  std::vector<NrIceTurnServer> turnServers;

  nsresult rv;
  if (aConfiguration.mIceServers.WasPassed()) {
    for (const auto& iceServer : aConfiguration.mIceServers.Value()) {
      NS_ENSURE_STATE(iceServer.mUrls.WasPassed());
      NS_ENSURE_STATE(iceServer.mUrls.Value().IsStringSequence());
      for (const auto& iceUrl : iceServer.mUrls.Value().GetAsStringSequence()) {
        rv = addNrIceServer(iceUrl, iceServer, &stunServers, &turnServers);
        if (NS_FAILED(rv)) {
          CSFLogError(LOGTAG, "%s: invalid STUN/TURN server: %s", __FUNCTION__,
                      NS_ConvertUTF16toUTF8(iceUrl).get());
          return rv;
        }
      }
    }
  }

  // This stuff will probably live on the other side of IPC; errors down here
  // will either need to be ignored, or plumbed back in some way other than
  // the return.
  bool allowLoopback =
      Preferences::GetBool("media.peerconnection.ice.loopback", false);
  bool tcpEnabled = Preferences::GetBool("media.peerconnection.ice.tcp", false);
  bool allowLinkLocal =
      Preferences::GetBool("media.peerconnection.ice.link_local", false);

  mIceCtx = NrIceCtx::Create(aName, allowLoopback, tcpEnabled, allowLinkLocal,
                             toNrIcePolicy(aConfiguration.mIceTransportPolicy));
  if (!mIceCtx) {
    return NS_ERROR_FAILURE;
  }

  mProxyOnly =
      Preferences::GetBool("media.peerconnection.ice.proxy_only", false);

  mIceCtx->SignalGatheringStateChange.connect(
      this, &MediaTransportHandler::OnGatheringStateChange);
  mIceCtx->SignalConnectionStateChange.connect(
      this, &MediaTransportHandler::OnConnectionStateChange);

  if (NS_FAILED(rv = mIceCtx->SetStunServers(stunServers))) {
    CSFLogError(LOGTAG, "%s: Failed to set stun servers", __FUNCTION__);
    return rv;
  }
  // Give us a way to globally turn off TURN support
  bool disabled =
      Preferences::GetBool("media.peerconnection.turn.disable", false);
  if (!disabled) {
    if (NS_FAILED(rv = mIceCtx->SetTurnServers(turnServers))) {
      CSFLogError(LOGTAG, "%s: Failed to set turn servers", __FUNCTION__);
      return rv;
    }
  } else if (!turnServers.empty()) {
    CSFLogError(LOGTAG, "%s: Setting turn servers disabled", __FUNCTION__);
  }

  mDNSResolver = new NrIceResolver;
  if (NS_FAILED(rv = mDNSResolver->Init())) {
    CSFLogError(LOGTAG, "%s: Failed to initialize dns resolver", __FUNCTION__);
    return rv;
  }
  if (NS_FAILED(rv = mIceCtx->SetResolver(mDNSResolver->AllocateResolver()))) {
    CSFLogError(LOGTAG, "%s: Failed to get dns resolver", __FUNCTION__);
    return rv;
  }

  return NS_OK;
}

void MediaTransportHandler::Destroy() {
  disconnect_all();
  if (mIceCtx) {
    NrIceStats stats = mIceCtx->Destroy();
    CSFLogDebug(LOGTAG,
                "Ice Telemetry: stun (retransmits: %d)"
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
  }
}

void MediaTransportHandler::SetProxyServer(NrSocketProxyConfig&& aProxyConfig) {
  mIceCtx->SetProxyServer(std::move(aProxyConfig));
}

void MediaTransportHandler::EnsureProvisionalTransport(
    const std::string& aTransportId, const std::string& aUfrag,
    const std::string& aPwd, size_t aComponentCount) {
  RefPtr<NrIceMediaStream> stream(mIceCtx->GetStream(aTransportId));
  if (!stream) {
    CSFLogDebug(LOGTAG, "%s: Creating ICE media stream=%s components=%u",
                mIceCtx->name().c_str(), aTransportId.c_str(),
                static_cast<unsigned>(aComponentCount));

    std::ostringstream os;
    os << mIceCtx->name() << " transport-id=" << aTransportId;
    stream = mIceCtx->CreateStream(aTransportId, os.str(), aComponentCount);

    if (!stream) {
      CSFLogError(LOGTAG, "Failed to create ICE stream.");
      return;
    }

    stream->SignalCandidate.connect(this,
                                    &MediaTransportHandler::OnCandidateFound);
  }

  // Begins an ICE restart if this stream has a different ufrag/pwd
  stream->SetIceCredentials(aUfrag, aPwd);

  // Make sure there's an entry in mTransports
  mTransports[aTransportId];
}

void MediaTransportHandler::ActivateTransport(
    const std::string& aTransportId, const std::string& aLocalUfrag,
    const std::string& aLocalPwd, size_t aComponentCount,
    const std::string& aUfrag, const std::string& aPassword,
    const std::vector<std::string>& aCandidateList,
    RefPtr<DtlsIdentity> aDtlsIdentity, bool aDtlsClient,
    const SdpFingerprintAttributeList& aFingerprints, bool aPrivacyRequested) {
  MOZ_ASSERT(aComponentCount);
  MOZ_ASSERT(aDtlsIdentity);

  RefPtr<NrIceMediaStream> stream(mIceCtx->GetStream(aTransportId));
  if (!stream) {
    MOZ_ASSERT(false);
    return;
  }

  CSFLogDebug(LOGTAG, "%s: Activating ICE media stream=%s components=%u",
              mIceCtx->name().c_str(), aTransportId.c_str(),
              static_cast<unsigned>(aComponentCount));

  std::vector<std::string> attrs;
  attrs.reserve(aCandidateList.size() + 2 /* ufrag + pwd */);
  for (const auto& candidate : aCandidateList) {
    attrs.push_back("candidate:" + candidate);
  }
  attrs.push_back("ice-ufrag:" + aUfrag);
  attrs.push_back("ice-pwd:" + aPassword);

  // If we started an ICE restart in EnsureProvisionalTransport, this is where
  // we decide whether to commit or rollback.
  nsresult rv = stream->ConnectToPeer(aLocalUfrag, aLocalPwd, attrs);
  if (NS_FAILED(rv)) {
    CSFLogError(LOGTAG, "Couldn't parse ICE attributes, rv=%u",
                static_cast<unsigned>(rv));
    MOZ_ASSERT(false);
    return;
  }

  Transport transport = mTransports[aTransportId];
  if (!transport.mFlow) {
    transport.mFlow =
        CreateTransportFlow(aTransportId, false, aDtlsIdentity, aDtlsClient,
                            aFingerprints, aPrivacyRequested);
    if (!transport.mFlow) {
      return;
    }
    TransportLayer* dtls = transport.mFlow->GetLayer(TransportLayerDtls::ID());
    dtls->SignalStateChange.connect(this,
                                    &MediaTransportHandler::OnStateChange);
    if (aComponentCount < 2) {
      dtls->SignalStateChange.connect(
          this, &MediaTransportHandler::OnRtcpStateChange);
    }
  }

  if (aComponentCount == 2) {
    if (!transport.mRtcpFlow) {
      transport.mRtcpFlow =
          CreateTransportFlow(aTransportId, true, aDtlsIdentity, aDtlsClient,
                              aFingerprints, aPrivacyRequested);
      if (!transport.mRtcpFlow) {
        return;
      }
      TransportLayer* dtls =
          transport.mRtcpFlow->GetLayer(TransportLayerDtls::ID());
      dtls->SignalStateChange.connect(
          this, &MediaTransportHandler::OnRtcpStateChange);
    }
  } else {
    transport.mRtcpFlow = nullptr;
    // components are 1-indexed
    stream->DisableComponent(2);
  }

  mTransports[aTransportId] = transport;
}

void MediaTransportHandler::StartIceGathering(
    bool aDefaultRouteOnly, const nsTArray<NrIceStunAddr>& aStunAddrs) {
  // Belt and suspenders - in e10s mode, the call below to SetStunAddrs
  // needs to have the proper flags set on ice ctx.  For non-e10s,
  // setting those flags happens in StartGathering.  We could probably
  // just set them here, and only do it here.
  mIceCtx->SetCtxFlags(aDefaultRouteOnly, mProxyOnly);

  if (aStunAddrs.Length()) {
    mIceCtx->SetStunAddrs(aStunAddrs);
  }

  // Start gathering, but only if there are streams
  if (!mIceCtx->GetStreams().empty()) {
    mIceCtx->StartGathering(aDefaultRouteOnly, mProxyOnly);
    return;
  }

  CSFLogWarn(LOGTAG,
             "%s: No streams to start gathering on. Can happen with rollback",
             __FUNCTION__);

  // If there are no streams, we're probably in a situation where we've rolled
  // back while still waiting for our proxy configuration to come back. Make
  // sure content knows that the rollback has stuck wrt gathering.
  SignalGatheringStateChange(dom::PCImplIceGatheringState::Complete);
}

void MediaTransportHandler::StartIceChecks(
    bool aIsControlling, bool aIsOfferer,
    const std::vector<std::string>& aIceOptions) {
  nsresult rv = mIceCtx->ParseGlobalAttributes(aIceOptions);
  if (NS_FAILED(rv)) {
    CSFLogError(LOGTAG, "%s: couldn't parse global parameters", __FUNCTION__);
    return;
  }

  rv = mIceCtx->SetControlling(aIsControlling ? NrIceCtx::ICE_CONTROLLING
                                              : NrIceCtx::ICE_CONTROLLED);
  if (NS_FAILED(rv)) {
    CSFLogError(LOGTAG, "%s: couldn't set controlling to %d", __FUNCTION__,
                aIsControlling);
    return;
  }

  rv = mIceCtx->StartChecks(aIsOfferer);
  if (NS_FAILED(rv)) {
    CSFLogError(LOGTAG, "%s: couldn't start checks", __FUNCTION__);
    return;
  }
}

void MediaTransportHandler::AddIceCandidate(const std::string& aTransportId,
                                            const std::string& aCandidate) {
  RefPtr<NrIceMediaStream> stream(mIceCtx->GetStream(aTransportId));
  if (!stream) {
    CSFLogError(LOGTAG, "No ICE stream for candidate with transport id %s: %s",
                aTransportId.c_str(), aCandidate.c_str());
    return;
  }

  nsresult rv = stream->ParseTrickleCandidate(aCandidate);
  if (NS_FAILED(rv)) {
    CSFLogError(LOGTAG,
                "Couldn't process ICE candidate with transport id %s: "
                "%s",
                aTransportId.c_str(), aCandidate.c_str());
  }
}

void MediaTransportHandler::UpdateNetworkState(bool aOnline) {
  mIceCtx->UpdateNetworkState(aOnline);
}

void MediaTransportHandler::RemoveTransportsExcept(
    const std::set<std::string>& aTransportIds) {
  for (auto it = mTransports.begin(); it != mTransports.end();) {
    if (!aTransportIds.count(it->first)) {
      if (it->second.mFlow) {
        SignalStateChange(it->first, TransportLayer::TS_NONE);
        SignalRtcpStateChange(it->first, TransportLayer::TS_NONE);
      }
      mIceCtx->DestroyStream(it->first);
      it = mTransports.erase(it);
    } else {
      MOZ_ASSERT(it->second.mFlow);
      ++it;
    }
  }
}

void MediaTransportHandler::SendPacket(const std::string& aTransportId,
                                       MediaPacket& aPacket) {
  MOZ_ASSERT(aPacket.type() != MediaPacket::UNCLASSIFIED);
  RefPtr<TransportFlow> flow =
      GetTransportFlow(aTransportId, aPacket.type() == MediaPacket::RTCP);

  if (!flow) {
    CSFLogError(LOGTAG, "%s: No such transport flow (%s) for outgoing packet",
                mIceCtx->name().c_str(), aTransportId.c_str());
    MOZ_ASSERT(false);
    return;
  }

  TransportLayer* layer = nullptr;
  switch (aPacket.type()) {
    case MediaPacket::SCTP:
      layer = flow->GetLayer(TransportLayerDtls::ID());
      break;
    case MediaPacket::RTP:
    case MediaPacket::RTCP:
      layer = flow->GetLayer(TransportLayerSrtp::ID());
      break;
    default:
      // Maybe it would be useful to allow the injection of other packet types
      // for testing?
      MOZ_ASSERT(false);
      return;
  }

  MOZ_ASSERT(layer);

  if (layer->SendPacket(aPacket) < 0) {
    CSFLogError(LOGTAG, "%s: Transport flow (%s) failed to send packet",
                mIceCtx->name().c_str(), aTransportId.c_str());
  }
}

TransportLayer::State MediaTransportHandler::GetState(
    const std::string& aTransportId, bool aRtcp) const {
  // TODO Bug 1520692: we should allow Datachannel to connect without
  // DTLS SRTP keys
  RefPtr<TransportFlow> flow = GetTransportFlow(aTransportId, aRtcp);
  if (flow) {
    return flow->GetLayer(TransportLayerDtls::ID())->state();
  }
  return TransportLayer::TS_NONE;
}

RefPtr<RTCStatsQueryPromise> MediaTransportHandler::GetIceStats(
    UniquePtr<RTCStatsQuery>&& aQuery) {
  for (const auto& stream : mIceCtx->GetStreams()) {
    if (aQuery->grabAllLevels || aQuery->transportId == stream->GetId()) {
      GetIceStats(*stream, aQuery->now, aQuery->report);
    }
  }
  return RTCStatsQueryPromise::CreateAndResolve(std::move(aQuery), __func__);
}

/* static */
RefPtr<MediaTransportHandler::IceLogPromise> MediaTransportHandler::GetIceLog(
    const nsCString& aPattern) {
  RLogConnector* logs = RLogConnector::GetInstance();
  nsAutoPtr<std::deque<std::string>> result(new std::deque<std::string>);
  // Might not exist yet.
  if (logs) {
    logs->Filter(aPattern.get(), 0, result);
  }
  dom::Sequence<nsString> converted;
  for (auto& line : *result) {
    converted.AppendElement(NS_ConvertUTF8toUTF16(line.c_str()), fallible);
  }
  return IceLogPromise::CreateAndResolve(std::move(converted), __func__);
}

/* static */
void MediaTransportHandler::ClearIceLog() {
  RLogConnector* logs = RLogConnector::GetInstance();
  if (logs) {
    logs->Clear();
  }
}

/* static */
void MediaTransportHandler::EnterPrivateMode() {
  RLogConnector::CreateInstance()->EnterPrivateMode();
}

/* static */
void MediaTransportHandler::ExitPrivateMode() {
  auto* log = RLogConnector::GetInstance();
  MOZ_ASSERT(log);
  if (log) {
    log->ExitPrivateMode();
  }
}

static void ToRTCIceCandidateStats(
    const std::vector<NrIceCandidate>& candidates,
    dom::RTCStatsType candidateType, const nsString& transportId,
    DOMHighResTimeStamp now, dom::RTCStatsReportInternal* report) {
  MOZ_ASSERT(report);
  for (const auto& candidate : candidates) {
    dom::RTCIceCandidateStats cand;
    cand.mType.Construct(candidateType);
    NS_ConvertASCIItoUTF16 codeword(candidate.codeword.c_str());
    cand.mTransportId.Construct(transportId);
    cand.mId.Construct(codeword);
    cand.mTimestamp.Construct(now);
    cand.mCandidateType.Construct(dom::RTCIceCandidateType(candidate.type));
    cand.mPriority.Construct(candidate.priority);
    cand.mAddress.Construct(
        NS_ConvertASCIItoUTF16(candidate.cand_addr.host.c_str()));
    cand.mPort.Construct(candidate.cand_addr.port);
    cand.mProtocol.Construct(
        NS_ConvertASCIItoUTF16(candidate.cand_addr.transport.c_str()));
    if (candidateType == dom::RTCStatsType::Local_candidate &&
        dom::RTCIceCandidateType(candidate.type) ==
            dom::RTCIceCandidateType::Relay) {
      cand.mRelayProtocol.Construct(
          NS_ConvertASCIItoUTF16(candidate.local_addr.transport.c_str()));
    }
    report->mIceCandidateStats.Value().AppendElement(cand, fallible);
    if (candidate.trickled) {
      report->mTrickledIceCandidateStats.Value().AppendElement(cand, fallible);
    }
  }
}

void MediaTransportHandler::GetIceStats(
    const NrIceMediaStream& aStream, DOMHighResTimeStamp aNow,
    dom::RTCStatsReportInternal* aReport) const {
  NS_ConvertASCIItoUTF16 transportId(aStream.GetId().c_str());

  std::vector<NrIceCandidatePair> candPairs;
  nsresult res = aStream.GetCandidatePairs(&candPairs);
  if (NS_FAILED(res)) {
    CSFLogError(LOGTAG,
                "%s: Error getting candidate pairs for transport id \"%s\"",
                __FUNCTION__, aStream.GetId().c_str());
    return;
  }

  for (auto& candPair : candPairs) {
    NS_ConvertASCIItoUTF16 codeword(candPair.codeword.c_str());
    NS_ConvertASCIItoUTF16 localCodeword(candPair.local.codeword.c_str());
    NS_ConvertASCIItoUTF16 remoteCodeword(candPair.remote.codeword.c_str());
    // Only expose candidate-pair statistics to chrome, until we've thought
    // through the implications of exposing it to content.

    dom::RTCIceCandidatePairStats s;
    s.mId.Construct(codeword);
    s.mTransportId.Construct(transportId);
    s.mTimestamp.Construct(aNow);
    s.mType.Construct(dom::RTCStatsType::Candidate_pair);
    s.mLocalCandidateId.Construct(localCodeword);
    s.mRemoteCandidateId.Construct(remoteCodeword);
    s.mNominated.Construct(candPair.nominated);
    s.mWritable.Construct(candPair.writable);
    s.mReadable.Construct(candPair.readable);
    s.mPriority.Construct(candPair.priority);
    s.mSelected.Construct(candPair.selected);
    s.mBytesSent.Construct(candPair.bytes_sent);
    s.mBytesReceived.Construct(candPair.bytes_recvd);
    s.mLastPacketSentTimestamp.Construct(candPair.ms_since_last_send);
    s.mLastPacketReceivedTimestamp.Construct(candPair.ms_since_last_recv);
    s.mState.Construct(dom::RTCStatsIceCandidatePairState(candPair.state));
    s.mComponentId.Construct(candPair.component_id);
    aReport->mIceCandidatePairStats.Value().AppendElement(s, fallible);
  }

  std::vector<NrIceCandidate> candidates;
  if (NS_SUCCEEDED(aStream.GetLocalCandidates(&candidates))) {
    ToRTCIceCandidateStats(candidates, dom::RTCStatsType::Local_candidate,
                           transportId, aNow, aReport);
    // add the local candidates unparsed string to a sequence
    for (const auto& candidate : candidates) {
      aReport->mRawLocalCandidates.Value().AppendElement(
          NS_ConvertASCIItoUTF16(candidate.label.c_str()), fallible);
    }
  }
  candidates.clear();

  if (NS_SUCCEEDED(aStream.GetRemoteCandidates(&candidates))) {
    ToRTCIceCandidateStats(candidates, dom::RTCStatsType::Remote_candidate,
                           transportId, aNow, aReport);
    // add the remote candidates unparsed string to a sequence
    for (const auto& candidate : candidates) {
      aReport->mRawRemoteCandidates.Value().AppendElement(
          NS_ConvertASCIItoUTF16(candidate.label.c_str()), fallible);
    }
  }
}

RefPtr<TransportFlow> MediaTransportHandler::GetTransportFlow(
    const std::string& aTransportId, bool aIsRtcp) const {
  auto it = mTransports.find(aTransportId);
  if (it == mTransports.end()) {
    return nullptr;
  }

  if (aIsRtcp) {
    return it->second.mRtcpFlow ? it->second.mRtcpFlow : it->second.mFlow;
    ;
  }

  return it->second.mFlow;
}

RefPtr<TransportFlow> MediaTransportHandler::CreateTransportFlow(
    const std::string& aTransportId, bool aIsRtcp,
    RefPtr<DtlsIdentity> aDtlsIdentity, bool aDtlsClient,
    const SdpFingerprintAttributeList& aFingerprints, bool aPrivacyRequested) {
  nsresult rv;
  RefPtr<TransportFlow> flow = new TransportFlow(aTransportId);

  // The media streams are made on STS so we need to defer setup.
  auto ice = MakeUnique<TransportLayerIce>();
  auto dtls = MakeUnique<TransportLayerDtls>();
  auto srtp = MakeUnique<TransportLayerSrtp>(*dtls);
  dtls->SetRole(aDtlsClient ? TransportLayerDtls::CLIENT
                            : TransportLayerDtls::SERVER);

  dtls->SetIdentity(aDtlsIdentity);

  for (const auto& fingerprint : aFingerprints.mFingerprints) {
    std::ostringstream ss;
    ss << fingerprint.hashFunc;
    rv = dtls->SetVerificationDigest(ss.str(), &fingerprint.fingerprint[0],
                                     fingerprint.fingerprint.size());
    if (NS_FAILED(rv)) {
      CSFLogError(LOGTAG, "Could not set fingerprint");
      return nullptr;
    }
  }

  std::vector<uint16_t> srtpCiphers =
      TransportLayerDtls::GetDefaultSrtpCiphers();

  rv = dtls->SetSrtpCiphers(srtpCiphers);
  if (NS_FAILED(rv)) {
    CSFLogError(LOGTAG, "Couldn't set SRTP ciphers");
    return nullptr;
  }

  // Always permits negotiation of the confidential mode.
  // Only allow non-confidential (which is an allowed default),
  // if we aren't confidential.
  std::set<std::string> alpn;
  std::string alpnDefault = "";
  alpn.insert("c-webrtc");
  if (!aPrivacyRequested) {
    alpnDefault = "webrtc";
    alpn.insert(alpnDefault);
  }
  rv = dtls->SetAlpn(alpn, alpnDefault);
  if (NS_FAILED(rv)) {
    CSFLogError(LOGTAG, "Couldn't set ALPN");
    return nullptr;
  }

  ice->SetParameters(mIceCtx->GetStream(aTransportId), aIsRtcp ? 2 : 1);
  NS_ENSURE_SUCCESS(ice->Init(), nullptr);
  NS_ENSURE_SUCCESS(dtls->Init(), nullptr);
  NS_ENSURE_SUCCESS(srtp->Init(), nullptr);
  dtls->Chain(ice.get());
  srtp->Chain(ice.get());

  dtls->SignalPacketReceived.connect(this,
                                     &MediaTransportHandler::PacketReceived);
  srtp->SignalPacketReceived.connect(this,
                                     &MediaTransportHandler::PacketReceived);
  ice->SignalPacketSending.connect(
      this, &MediaTransportHandler::EncryptedPacketSending);
  flow->PushLayer(ice.release());
  flow->PushLayer(dtls.release());
  flow->PushLayer(srtp.release());
  return flow;
}

static mozilla::dom::PCImplIceGatheringState toDomIceGatheringState(
    NrIceCtx::GatheringState aState) {
  switch (aState) {
    case NrIceCtx::ICE_CTX_GATHER_INIT:
      return dom::PCImplIceGatheringState::New;
    case NrIceCtx::ICE_CTX_GATHER_STARTED:
      return dom::PCImplIceGatheringState::Gathering;
    case NrIceCtx::ICE_CTX_GATHER_COMPLETE:
      return dom::PCImplIceGatheringState::Complete;
  }
  MOZ_CRASH();
}

void MediaTransportHandler::OnGatheringStateChange(
    NrIceCtx* aIceCtx, NrIceCtx::GatheringState aState) {
  if (aState == NrIceCtx::ICE_CTX_GATHER_COMPLETE) {
    for (const auto& stream : mIceCtx->GetStreams()) {
      OnCandidateFound(stream, "");
    }
  }
  SignalGatheringStateChange(toDomIceGatheringState(aState));
}

static mozilla::dom::PCImplIceConnectionState toDomIceConnectionState(
    NrIceCtx::ConnectionState aState) {
  switch (aState) {
    case NrIceCtx::ICE_CTX_INIT:
      return dom::PCImplIceConnectionState::New;
    case NrIceCtx::ICE_CTX_CHECKING:
      return dom::PCImplIceConnectionState::Checking;
    case NrIceCtx::ICE_CTX_CONNECTED:
      return dom::PCImplIceConnectionState::Connected;
    case NrIceCtx::ICE_CTX_COMPLETED:
      return dom::PCImplIceConnectionState::Completed;
    case NrIceCtx::ICE_CTX_FAILED:
      return dom::PCImplIceConnectionState::Failed;
    case NrIceCtx::ICE_CTX_DISCONNECTED:
      return dom::PCImplIceConnectionState::Disconnected;
    case NrIceCtx::ICE_CTX_CLOSED:
      return dom::PCImplIceConnectionState::Closed;
  }
  MOZ_CRASH();
}

void MediaTransportHandler::OnConnectionStateChange(
    NrIceCtx* aIceCtx, NrIceCtx::ConnectionState aState) {
  SignalConnectionStateChange(toDomIceConnectionState(aState));
}

// The stuff below here will eventually go into the MediaTransportChild class
void MediaTransportHandler::OnCandidateFound(NrIceMediaStream* aStream,
                                             const std::string& aCandidate) {
  CandidateInfo info;
  info.mCandidate = aCandidate;
  NrIceCandidate defaultRtpCandidate;
  NrIceCandidate defaultRtcpCandidate;
  nsresult rv = aStream->GetDefaultCandidate(1, &defaultRtpCandidate);
  if (NS_SUCCEEDED(rv)) {
    info.mDefaultHostRtp = defaultRtpCandidate.cand_addr.host;
    info.mDefaultPortRtp = defaultRtpCandidate.cand_addr.port;
  } else {
    CSFLogError(LOGTAG,
                "%s: GetDefaultCandidates failed for transport id %s, "
                "res=%u",
                __FUNCTION__, aStream->GetId().c_str(),
                static_cast<unsigned>(rv));
  }

  // Optional; component won't exist if doing rtcp-mux
  if (NS_SUCCEEDED(aStream->GetDefaultCandidate(2, &defaultRtcpCandidate))) {
    info.mDefaultHostRtcp = defaultRtcpCandidate.cand_addr.host;
    info.mDefaultPortRtcp = defaultRtcpCandidate.cand_addr.port;
  }

  SignalCandidate(aStream->GetId(), info);
}

void MediaTransportHandler::OnStateChange(TransportLayer* aLayer,
                                          TransportLayer::State aState) {
  if (aState == TransportLayer::TS_OPEN) {
    MOZ_ASSERT(aLayer->id() == TransportLayerDtls::ID());
    TransportLayerDtls* dtlsLayer = static_cast<TransportLayerDtls*>(aLayer);
    SignalAlpnNegotiated(dtlsLayer->GetNegotiatedAlpn());
  }

  // DTLS state indicates the readiness of the transport as a whole, because
  // SRTP uses the keys from the DTLS handshake.
  SignalStateChange(aLayer->flow_id(), aState);
}

void MediaTransportHandler::OnRtcpStateChange(TransportLayer* aLayer,
                                              TransportLayer::State aState) {
  SignalRtcpStateChange(aLayer->flow_id(), aState);
}

void MediaTransportHandler::PacketReceived(TransportLayer* aLayer,
                                           MediaPacket& aPacket) {
  SignalPacketReceived(aLayer->flow_id(), aPacket);
}

void MediaTransportHandler::EncryptedPacketSending(TransportLayer* aLayer,
                                                   MediaPacket& aPacket) {
  SignalEncryptedSending(aLayer->flow_id(), aPacket);
}
}  // namespace mozilla
