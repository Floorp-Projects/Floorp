/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MTRANSPORTHANDLER_PARENT_H__
#define _MTRANSPORTHANDLER_PARENT_H__

#include "mozilla/dom/PMediaTransportParent.h"
#include "signaling/src/peerconnection/MediaTransportHandler.h"
#include "sigslot.h"

namespace mozilla {

class MediaTransportHandlerParent : public dom::PMediaTransportParent,
                                    public sigslot::has_slots<> {
 public:
  MediaTransportHandlerParent();
  virtual ~MediaTransportHandlerParent();

  void OnCandidate(const std::string& aTransportId,
                   const CandidateInfo& aCandidateInfo);
  void OnAlpnNegotiated(const std::string& aAlpn);
  void OnGatheringStateChange(dom::PCImplIceGatheringState aState);
  void OnConnectionStateChange(dom::PCImplIceConnectionState aState);
  void OnPacketReceived(const std::string& aTransportId, MediaPacket& aPacket);
  void OnEncryptedSending(const std::string& aTransportId,
                          MediaPacket& aPacket);
  void OnStateChange(const std::string& aTransportId,
                     TransportLayer::State aState);
  void OnRtcpStateChange(const std::string& aTransportId,
                         TransportLayer::State aState);

  mozilla::ipc::IPCResult RecvGetIceLog(const nsCString& pattern,
                                        GetIceLogResolver&& aResolve) override;
  mozilla::ipc::IPCResult RecvClearIceLog() override;
  mozilla::ipc::IPCResult RecvEnterPrivateMode() override;
  mozilla::ipc::IPCResult RecvExitPrivateMode() override;
  mozilla::ipc::IPCResult RecvCreateIceCtx(
      const string& name, nsTArray<RTCIceServer>&& iceServers,
      const RTCIceTransportPolicy& icePolicy) override;
  mozilla::ipc::IPCResult RecvSetProxyServer(const PBrowserOrId& browserOrId,
                                             const nsCString& alpn) override;
  mozilla::ipc::IPCResult RecvEnsureProvisionalTransport(
      const string& transportId, const string& localUfrag,
      const string& localPwd, const int& componentCount) override;
  mozilla::ipc::IPCResult RecvStartIceGathering(
      const bool& defaultRouteOnly,
      const net::NrIceStunAddrArray& stunAddrs) override;
  mozilla::ipc::IPCResult RecvActivateTransport(
      const string& transportId, const string& localUfrag,
      const string& localPwd, const int& componentCount,
      const string& remoteUfrag, const string& remotePwd,
      nsTArray<uint8_t>&& keyDer, nsTArray<uint8_t>&& certDer,
      const int& authType, const bool& dtlsClient,
      const DtlsDigestList& digests, const bool& privacyRequested) override;
  mozilla::ipc::IPCResult RecvRemoveTransportsExcept(
      const StringVector& transportIds) override;
  mozilla::ipc::IPCResult RecvStartIceChecks(
      const bool& isControlling, const bool& isOfferer,
      const StringVector& iceOptions) override;
  mozilla::ipc::IPCResult RecvSendPacket(const string& transportId,
                                         const MediaPacket& packet) override;
  mozilla::ipc::IPCResult RecvAddIceCandidate(const string& transportId,
                                              const string& candidate) override;
  mozilla::ipc::IPCResult RecvUpdateNetworkState(const bool& online) override;
  mozilla::ipc::IPCResult RecvGetIceStats(
      const string& transportId, const double& now,
      const RTCStatsReportInternal& reportIn,
      GetIceStatsResolver&& aResolve) override;

  void ActorDestroy(ActorDestroyReason aWhy) override;

 private:
  RefPtr<MediaTransportHandler> mImpl;
  RefPtr<nsIEventTarget> mStsThread;
};
}  // namespace mozilla
#endif  //_MTRANSPORTHANDLER_PARENT_H__
