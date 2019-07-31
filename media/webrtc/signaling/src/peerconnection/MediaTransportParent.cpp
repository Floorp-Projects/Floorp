/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MediaTransportParent.h"
#include "signaling/src/peerconnection/MediaTransportHandler.h"

#include "sigslot.h"
#include "CSFLog.h"

namespace mozilla {

// Deals with the MediaTransportHandler interface, so MediaTransportParent
// doesn't have to..
class MediaTransportParent::Impl : public sigslot::has_slots<> {
 public:
  explicit Impl(MediaTransportParent* aParent)
      : mHandler(
            MediaTransportHandler::Create(GetCurrentThreadSerialEventTarget())),
        mParent(aParent) {
    mHandler->SignalCandidate.connect(this,
                                      &MediaTransportParent::Impl::OnCandidate);
    mHandler->SignalAlpnNegotiated.connect(
        this, &MediaTransportParent::Impl::OnAlpnNegotiated);
    mHandler->SignalGatheringStateChange.connect(
        this, &MediaTransportParent::Impl::OnGatheringStateChange);
    mHandler->SignalConnectionStateChange.connect(
        this, &MediaTransportParent::Impl::OnConnectionStateChange);
    mHandler->SignalPacketReceived.connect(
        this, &MediaTransportParent::Impl::OnPacketReceived);
    mHandler->SignalEncryptedSending.connect(
        this, &MediaTransportParent::Impl::OnEncryptedSending);
    mHandler->SignalStateChange.connect(
        this, &MediaTransportParent::Impl::OnStateChange);
    mHandler->SignalRtcpStateChange.connect(
        this, &MediaTransportParent::Impl::OnRtcpStateChange);
  }

  virtual ~Impl() {
    disconnect_all();
    mHandler->Destroy();
    mHandler = nullptr;
  }

  void OnCandidate(const std::string& aTransportId,
                   const CandidateInfo& aCandidateInfo) {
    NS_ENSURE_TRUE_VOID(mParent->SendOnCandidate(aTransportId, aCandidateInfo));
  }

  void OnAlpnNegotiated(const std::string& aAlpn) {
    NS_ENSURE_TRUE_VOID(mParent->SendOnAlpnNegotiated(aAlpn));
  }

  void OnGatheringStateChange(dom::RTCIceGatheringState aState) {
    NS_ENSURE_TRUE_VOID(
        mParent->SendOnGatheringStateChange(static_cast<int>(aState)));
  }

  void OnConnectionStateChange(dom::RTCIceConnectionState aState) {
    NS_ENSURE_TRUE_VOID(
        mParent->SendOnConnectionStateChange(static_cast<int>(aState)));
  }

  void OnPacketReceived(const std::string& aTransportId, MediaPacket& aPacket) {
    NS_ENSURE_TRUE_VOID(mParent->SendOnPacketReceived(aTransportId, aPacket));
  }

  void OnEncryptedSending(const std::string& aTransportId,
                          MediaPacket& aPacket) {
    NS_ENSURE_TRUE_VOID(mParent->SendOnEncryptedSending(aTransportId, aPacket));
  }

  void OnStateChange(const std::string& aTransportId,
                     TransportLayer::State aState) {
    NS_ENSURE_TRUE_VOID(mParent->SendOnStateChange(aTransportId, aState));
  }

  void OnRtcpStateChange(const std::string& aTransportId,
                         TransportLayer::State aState) {
    NS_ENSURE_TRUE_VOID(mParent->SendOnRtcpStateChange(aTransportId, aState));
  }

  RefPtr<MediaTransportHandler> mHandler;

 private:
  MediaTransportParent* mParent;
};

MediaTransportParent::MediaTransportParent() : mImpl(new Impl(this)) {}

MediaTransportParent::~MediaTransportParent() {}

mozilla::ipc::IPCResult MediaTransportParent::RecvGetIceLog(
    const nsCString& pattern, GetIceLogResolver&& aResolve) {
  mImpl->mHandler->GetIceLog(pattern)->Then(
      GetCurrentThreadSerialEventTarget(), __func__,
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

mozilla::ipc::IPCResult MediaTransportParent::RecvClearIceLog() {
  mImpl->mHandler->ClearIceLog();
  return ipc::IPCResult::Ok();
}

mozilla::ipc::IPCResult MediaTransportParent::RecvEnterPrivateMode() {
  mImpl->mHandler->EnterPrivateMode();
  return ipc::IPCResult::Ok();
}

mozilla::ipc::IPCResult MediaTransportParent::RecvExitPrivateMode() {
  mImpl->mHandler->ExitPrivateMode();
  return ipc::IPCResult::Ok();
}

mozilla::ipc::IPCResult MediaTransportParent::RecvCreateIceCtx(
    const string& name, nsTArray<RTCIceServer>&& iceServers,
    const RTCIceTransportPolicy& icePolicy) {
  nsresult rv = mImpl->mHandler->CreateIceCtx(name, iceServers, icePolicy);
  if (NS_FAILED(rv)) {
    return ipc::IPCResult::Fail(WrapNotNull(this), __func__,
                                "MediaTransportHandler::Init failed");
  }
  return ipc::IPCResult::Ok();
}

mozilla::ipc::IPCResult MediaTransportParent::RecvSetProxyServer(
    const dom::TabId& tabId, const net::LoadInfoArgs& args,
    const nsCString& alpn) {
  mImpl->mHandler->SetProxyServer(NrSocketProxyConfig(tabId, alpn, args));
  return ipc::IPCResult::Ok();
}

mozilla::ipc::IPCResult MediaTransportParent::RecvEnsureProvisionalTransport(
    const string& transportId, const string& localUfrag, const string& localPwd,
    const int& componentCount) {
  mImpl->mHandler->EnsureProvisionalTransport(transportId, localUfrag, localPwd,
                                              componentCount);
  return ipc::IPCResult::Ok();
}

mozilla::ipc::IPCResult
MediaTransportParent::RecvSetTargetForDefaultLocalAddressLookup(
    const std::string& targetIp, uint16_t targetPort) {
  mImpl->mHandler->SetTargetForDefaultLocalAddressLookup(targetIp, targetPort);
  return ipc::IPCResult::Ok();
}

mozilla::ipc::IPCResult MediaTransportParent::RecvStartIceGathering(
    const bool& defaultRouteOnly, const net::NrIceStunAddrArray& stunAddrs) {
  mImpl->mHandler->StartIceGathering(defaultRouteOnly, stunAddrs);
  return ipc::IPCResult::Ok();
}

mozilla::ipc::IPCResult MediaTransportParent::RecvActivateTransport(
    const string& transportId, const string& localUfrag, const string& localPwd,
    const int& componentCount, const string& remoteUfrag,
    const string& remotePwd, nsTArray<uint8_t>&& keyDer,
    nsTArray<uint8_t>&& certDer, const int& authType, const bool& dtlsClient,
    const DtlsDigestList& digests, const bool& privacyRequested) {
  mImpl->mHandler->ActivateTransport(
      transportId, localUfrag, localPwd, componentCount, remoteUfrag, remotePwd,
      keyDer, certDer, static_cast<SSLKEAType>(authType), dtlsClient, digests,
      privacyRequested);
  return ipc::IPCResult::Ok();
}

mozilla::ipc::IPCResult MediaTransportParent::RecvRemoveTransportsExcept(
    const StringVector& transportIds) {
  std::set<std::string> ids(transportIds.begin(), transportIds.end());
  mImpl->mHandler->RemoveTransportsExcept(ids);
  return ipc::IPCResult::Ok();
}

mozilla::ipc::IPCResult MediaTransportParent::RecvStartIceChecks(
    const bool& isControlling, const StringVector& iceOptions) {
  mImpl->mHandler->StartIceChecks(isControlling, iceOptions);
  return ipc::IPCResult::Ok();
}

mozilla::ipc::IPCResult MediaTransportParent::RecvSendPacket(
    const string& transportId, const MediaPacket& packet) {
  MediaPacket copy(packet);  // Laaaaaaame.
  mImpl->mHandler->SendPacket(transportId, std::move(copy));
  return ipc::IPCResult::Ok();
}

mozilla::ipc::IPCResult MediaTransportParent::RecvAddIceCandidate(
    const string& transportId, const string& candidate, const string& ufrag) {
  mImpl->mHandler->AddIceCandidate(transportId, candidate, ufrag);
  return ipc::IPCResult::Ok();
}

mozilla::ipc::IPCResult MediaTransportParent::RecvUpdateNetworkState(
    const bool& online) {
  mImpl->mHandler->UpdateNetworkState(online);
  return ipc::IPCResult::Ok();
}

mozilla::ipc::IPCResult MediaTransportParent::RecvGetIceStats(
    const string& transportId, const double& now,
    const RTCStatsReportInternal& reportIn, GetIceStatsResolver&& aResolve) {
  // Copy, because we are handed a const reference (lame), and put in a
  // unique_ptr because RTCStatsReportInternal doesn't have move semantics
  // (also lame).
  std::unique_ptr<dom::RTCStatsReportInternal> report(
      new dom::RTCStatsReportInternal(reportIn));

  mImpl->mHandler->GetIceStats(transportId, now, std::move(report))
      ->Then(
          GetCurrentThreadSerialEventTarget(), __func__,
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

void MediaTransportParent::ActorDestroy(ActorDestroyReason aWhy) {}

}  // namespace mozilla
