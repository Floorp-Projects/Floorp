/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaTransportHandlerParent.h"
#include "signaling/src/peerconnection/MediaTransportHandler.h"

#include "nss.h"                // For NSS_NoDB_Init
#include "mozilla/PublicSSL.h"  // For psm::InitializeCipherSuite

namespace mozilla {

MediaTransportHandlerParent::MediaTransportHandlerParent()
    : mImpl(MediaTransportHandler::Create(GetMainThreadSerialEventTarget())) {
  // We cannot register PMediaTransportParent's Send* API to these signals,
  // because this API returns must-use bools, and the params are in some cases
  // slightly different.
  mImpl->SignalCandidate.connect(this,
                                 &MediaTransportHandlerParent::OnCandidate);
  mImpl->SignalAlpnNegotiated.connect(
      this, &MediaTransportHandlerParent::OnAlpnNegotiated);
  mImpl->SignalGatheringStateChange.connect(
      this, &MediaTransportHandlerParent::OnGatheringStateChange);
  mImpl->SignalConnectionStateChange.connect(
      this, &MediaTransportHandlerParent::OnConnectionStateChange);
  mImpl->SignalPacketReceived.connect(
      this, &MediaTransportHandlerParent::OnPacketReceived);
  mImpl->SignalEncryptedSending.connect(
      this, &MediaTransportHandlerParent::OnEncryptedSending);
  mImpl->SignalStateChange.connect(this,
                                   &MediaTransportHandlerParent::OnStateChange);
  mImpl->SignalRtcpStateChange.connect(
      this, &MediaTransportHandlerParent::OnRtcpStateChange);
}

MediaTransportHandlerParent::~MediaTransportHandlerParent() {
  MOZ_RELEASE_ASSERT(!mImpl);
}

void MediaTransportHandlerParent::OnCandidate(
    const std::string& aTransportId, const CandidateInfo& aCandidateInfo) {
  NS_ENSURE_TRUE_VOID(SendOnCandidate(aTransportId, aCandidateInfo));
}

void MediaTransportHandlerParent::OnAlpnNegotiated(const std::string& aAlpn) {
  NS_ENSURE_TRUE_VOID(SendOnAlpnNegotiated(aAlpn));
}

void MediaTransportHandlerParent::OnGatheringStateChange(
    dom::PCImplIceGatheringState aState) {
  NS_ENSURE_TRUE_VOID(SendOnGatheringStateChange(static_cast<int>(aState)));
}

void MediaTransportHandlerParent::OnConnectionStateChange(
    dom::PCImplIceConnectionState aState) {
  NS_ENSURE_TRUE_VOID(SendOnConnectionStateChange(static_cast<int>(aState)));
}

void MediaTransportHandlerParent::OnPacketReceived(
    const std::string& aTransportId, MediaPacket& aPacket) {
  NS_ENSURE_TRUE_VOID(SendOnPacketReceived(aTransportId, aPacket));
}

void MediaTransportHandlerParent::OnEncryptedSending(
    const std::string& aTransportId, MediaPacket& aPacket) {
  NS_ENSURE_TRUE_VOID(SendOnEncryptedSending(aTransportId, aPacket));
}

void MediaTransportHandlerParent::OnStateChange(const std::string& aTransportId,
                                                TransportLayer::State aState) {
  NS_ENSURE_TRUE_VOID(SendOnStateChange(aTransportId, aState));
}

void MediaTransportHandlerParent::OnRtcpStateChange(
    const std::string& aTransportId, TransportLayer::State aState) {
  NS_ENSURE_TRUE_VOID(SendOnRtcpStateChange(aTransportId, aState));
}

mozilla::ipc::IPCResult MediaTransportHandlerParent::RecvGetIceLog(
    const nsCString& pattern, GetIceLogResolver&& aResolve) {
  MOZ_ASSERT(GetMainThreadEventTarget()->IsOnCurrentThread());
  mImpl->GetIceLog(pattern)->Then(
      GetMainThreadSerialEventTarget(), __func__,
      // IPDL doesn't give us a reject function, so we cannot reject async, so
      // we are forced to resolve with an empty result. Laaaaaaame.
      [aResolve = std::move(aResolve)](
          MediaTransportHandler::IceLogPromise::ResolveOrRejectValue&&
              aResult) mutable {
        WebrtcGlobalLog logLines;
        if (aResult.IsResolve()) {
          logLines = std::move(aResult.ResolveValue());
        }
        aResolve(logLines);
      });

  return ipc::IPCResult::Ok();
}

mozilla::ipc::IPCResult MediaTransportHandlerParent::RecvClearIceLog() {
  MOZ_ASSERT(GetMainThreadEventTarget()->IsOnCurrentThread());
  mImpl->ClearIceLog();
  return ipc::IPCResult::Ok();
}

mozilla::ipc::IPCResult MediaTransportHandlerParent::RecvEnterPrivateMode() {
  MOZ_ASSERT(GetMainThreadEventTarget()->IsOnCurrentThread());
  mImpl->EnterPrivateMode();
  return ipc::IPCResult::Ok();
}

mozilla::ipc::IPCResult MediaTransportHandlerParent::RecvExitPrivateMode() {
  MOZ_ASSERT(GetMainThreadEventTarget()->IsOnCurrentThread());
  mImpl->ExitPrivateMode();
  return ipc::IPCResult::Ok();
}

mozilla::ipc::IPCResult MediaTransportHandlerParent::RecvCreateIceCtx(
    const string& name, nsTArray<RTCIceServer>&& iceServers,
    const RTCIceTransportPolicy& icePolicy) {
  MOZ_ASSERT(GetMainThreadEventTarget()->IsOnCurrentThread());
  static bool nssStarted = false;
  if (!nssStarted) {
    if (NSS_NoDB_Init(nullptr) != SECSuccess) {
      MOZ_CRASH();
      return ipc::IPCResult::Fail(WrapNotNull(this), __func__,
                                  "NSS_NoDB_Init failed");
    }

    if (NS_FAILED(mozilla::psm::InitializeCipherSuite())) {
      MOZ_CRASH();
      return ipc::IPCResult::Fail(WrapNotNull(this), __func__,
                                  "InitializeCipherSuite failed");
    }

    mozilla::psm::DisableMD5();
  }

  nssStarted = true;

  nsresult rv = mImpl->CreateIceCtx(name, iceServers, icePolicy);
  if (NS_FAILED(rv)) {
    return ipc::IPCResult::Fail(WrapNotNull(this), __func__,
                                "MediaTransportHandler::Init failed");
  }
  return ipc::IPCResult::Ok();
}

mozilla::ipc::IPCResult MediaTransportHandlerParent::RecvSetProxyServer(
    const PBrowserOrId& browserOrId, const nsCString& alpn) {
  MOZ_ASSERT(GetMainThreadEventTarget()->IsOnCurrentThread());
  mImpl->SetProxyServer(NrSocketProxyConfig(browserOrId, alpn));
  return ipc::IPCResult::Ok();
}

mozilla::ipc::IPCResult
MediaTransportHandlerParent::RecvEnsureProvisionalTransport(
    const string& transportId, const string& localUfrag, const string& localPwd,
    const int& componentCount) {
  MOZ_ASSERT(GetMainThreadEventTarget()->IsOnCurrentThread());
  mImpl->EnsureProvisionalTransport(transportId, localUfrag, localPwd,
                                    componentCount);
  return ipc::IPCResult::Ok();
}

mozilla::ipc::IPCResult MediaTransportHandlerParent::RecvStartIceGathering(
    const bool& defaultRouteOnly, const net::NrIceStunAddrArray& stunAddrs) {
  MOZ_ASSERT(GetMainThreadEventTarget()->IsOnCurrentThread());
  mImpl->StartIceGathering(defaultRouteOnly, stunAddrs);
  return ipc::IPCResult::Ok();
}

mozilla::ipc::IPCResult MediaTransportHandlerParent::RecvActivateTransport(
    const string& transportId, const string& localUfrag, const string& localPwd,
    const int& componentCount, const string& remoteUfrag,
    const string& remotePwd, nsTArray<uint8_t>&& keyDer,
    nsTArray<uint8_t>&& certDer, const int& authType, const bool& dtlsClient,
    const DtlsDigestList& digests, const bool& privacyRequested) {
  MOZ_ASSERT(GetMainThreadEventTarget()->IsOnCurrentThread());
  mImpl->ActivateTransport(transportId, localUfrag, localPwd, componentCount,
                           remoteUfrag, remotePwd, keyDer, certDer,
                           static_cast<SSLKEAType>(authType), dtlsClient,
                           digests, privacyRequested);
  return ipc::IPCResult::Ok();
}

mozilla::ipc::IPCResult MediaTransportHandlerParent::RecvRemoveTransportsExcept(
    const StringVector& transportIds) {
  MOZ_ASSERT(GetMainThreadEventTarget()->IsOnCurrentThread());
  std::set<std::string> ids(transportIds.begin(), transportIds.end());
  mImpl->RemoveTransportsExcept(ids);
  return ipc::IPCResult::Ok();
}

mozilla::ipc::IPCResult MediaTransportHandlerParent::RecvStartIceChecks(
    const bool& isControlling, const bool& isOfferer,
    const StringVector& iceOptions) {
  MOZ_ASSERT(GetMainThreadEventTarget()->IsOnCurrentThread());
  mImpl->StartIceChecks(isControlling, isOfferer, iceOptions);
  return ipc::IPCResult::Ok();
}

mozilla::ipc::IPCResult MediaTransportHandlerParent::RecvSendPacket(
    const string& transportId, const MediaPacket& packet) {
  MOZ_ASSERT(GetMainThreadEventTarget()->IsOnCurrentThread());
  MediaPacket copy(packet);  // Laaaaaaame.
  mImpl->SendPacket(transportId, std::move(copy));
  return ipc::IPCResult::Ok();
}

mozilla::ipc::IPCResult MediaTransportHandlerParent::RecvAddIceCandidate(
    const string& transportId, const string& candidate) {
  MOZ_ASSERT(GetMainThreadEventTarget()->IsOnCurrentThread());
  mImpl->AddIceCandidate(transportId, candidate);
  return ipc::IPCResult::Ok();
}

mozilla::ipc::IPCResult MediaTransportHandlerParent::RecvUpdateNetworkState(
    const bool& online) {
  MOZ_ASSERT(GetMainThreadEventTarget()->IsOnCurrentThread());
  mImpl->UpdateNetworkState(online);
  return ipc::IPCResult::Ok();
}

mozilla::ipc::IPCResult MediaTransportHandlerParent::RecvGetIceStats(
    const string& transportId, const double& now,
    const RTCStatsReportInternal& reportIn, GetIceStatsResolver&& aResolve) {
  MOZ_ASSERT(GetMainThreadEventTarget()->IsOnCurrentThread());
  // Copy, because we are handed a const reference (lame), and put in a
  // unique_ptr because RTCStatsReportInternal doesn't have move semantics
  // (also lame).
  std::unique_ptr<dom::RTCStatsReportInternal> report(
      new dom::RTCStatsReportInternal(reportIn));

  mImpl->GetIceStats(transportId, now, std::move(report))
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          // IPDL doesn't give us a reject function, so we cannot reject async,
          // so we are forced to resolve with an unmodified result. Laaaaaaame.
          [aResolve = std::move(aResolve),
           reportIn](MediaTransportHandler::StatsPromise::ResolveOrRejectValue&&
                         aResult) {
            if (aResult.IsResolve()) {
              MovableRTCStatsReportInternal copy(*aResult.ResolveValue());
              aResolve(copy);
            } else {
              aResolve(MovableRTCStatsReportInternal(reportIn));
            }
          });

  return ipc::IPCResult::Ok();
}

void MediaTransportHandlerParent::ActorDestroy(ActorDestroyReason aWhy) {
  MOZ_ASSERT(GetMainThreadEventTarget()->IsOnCurrentThread());
  disconnect_all();
  mImpl->Destroy();
  mImpl = nullptr;
}

}  // namespace mozilla
