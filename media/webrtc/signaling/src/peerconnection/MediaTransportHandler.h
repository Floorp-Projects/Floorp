/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MTRANSPORTHANDLER_H__
#define _MTRANSPORTHANDLER_H__

#include "mozilla/RefPtr.h"
#include "nsISupportsImpl.h"
#include "sigslot.h"
#include "transportlayer.h"  // Need the State enum
#include "dtlsidentity.h"    // For DtlsDigest
#include "mozilla/dom/RTCPeerConnectionBinding.h"
#include "mozilla/dom/RTCConfigurationBinding.h"
#include "nricectx.h"               // Need some enums
#include "nsDOMNavigationTiming.h"  // DOMHighResTimeStamp
#include "signaling/src/common/CandidateInfo.h"
#include "nr_socket_proxy_config.h"

#include "nsString.h"

#include <string>
#include <set>
#include <vector>

namespace mozilla {
class DtlsIdentity;
class NrIceCtx;
class NrIceMediaStream;
class NrIceResolver;
class TransportFlow;
class RTCStatsQuery;

namespace dom {
struct RTCStatsReportInternal;
}

class MediaTransportHandler {
 public:
  // Creates either a MediaTransportHandlerSTS or a MediaTransportHandlerIPC,
  // as appropriate. If you want signals to fire on a specific thread, pass
  // the event target here, otherwise they will fire on whatever is convenient.
  // Note: This also determines what thread the state cache is updated on!
  // Don't call GetState on any other thread!
  static already_AddRefed<MediaTransportHandler> Create(
      nsISerialEventTarget* aCallbackThread);

  explicit MediaTransportHandler(nsISerialEventTarget* aCallbackThread)
      : mCallbackThread(aCallbackThread) {}

  static nsresult ConvertIceServers(
      const nsTArray<dom::RTCIceServer>& aIceServers,
      std::vector<NrIceStunServer>* aStunServers,
      std::vector<NrIceTurnServer>* aTurnServers);

  typedef MozPromise<dom::Sequence<nsString>, nsresult, true> IceLogPromise;

  // There's a wrinkle here; the ICE logging is not separated out by
  // MediaTransportHandler. These are a little more like static methods, but
  // to avoid needing yet another IPC interface, we bolt them on here.
  virtual RefPtr<IceLogPromise> GetIceLog(const nsCString& aPattern) = 0;
  virtual void ClearIceLog() = 0;
  virtual void EnterPrivateMode() = 0;
  virtual void ExitPrivateMode() = 0;

  virtual void Destroy() = 0;

  virtual nsresult CreateIceCtx(const std::string& aName,
                                const nsTArray<dom::RTCIceServer>& aIceServers,
                                dom::RTCIceTransportPolicy aIcePolicy) = 0;

  // We will probably be able to move the proxy lookup stuff into
  // this class once we move mtransport to its own process.
  virtual void SetProxyServer(NrSocketProxyConfig&& aProxyConfig) = 0;

  virtual void EnsureProvisionalTransport(const std::string& aTransportId,
                                          const std::string& aLocalUfrag,
                                          const std::string& aLocalPwd,
                                          size_t aComponentCount) = 0;

  virtual void SetTargetForDefaultLocalAddressLookup(
      const std::string& aTargetIp, uint16_t aTargetPort) = 0;

  // We set default-route-only as late as possible because it depends on what
  // capture permissions have been granted on the window, which could easily
  // change between Init (ie; when the PC is created) and StartIceGathering
  // (ie; when we set the local description).
  virtual void StartIceGathering(bool aDefaultRouteOnly,
                                 bool aObfuscateHostAddresses,
                                 // TODO: It probably makes sense to look
                                 // this up internally
                                 const nsTArray<NrIceStunAddr>& aStunAddrs) = 0;

  virtual void ActivateTransport(
      const std::string& aTransportId, const std::string& aLocalUfrag,
      const std::string& aLocalPwd, size_t aComponentCount,
      const std::string& aUfrag, const std::string& aPassword,
      const nsTArray<uint8_t>& aKeyDer, const nsTArray<uint8_t>& aCertDer,
      SSLKEAType aAuthType, bool aDtlsClient, const DtlsDigestList& aDigests,
      bool aPrivacyRequested) = 0;

  virtual void RemoveTransportsExcept(
      const std::set<std::string>& aTransportIds) = 0;

  virtual void StartIceChecks(bool aIsControlling,
                              const std::vector<std::string>& aIceOptions) = 0;

  virtual void SendPacket(const std::string& aTransportId,
                          MediaPacket&& aPacket) = 0;

  virtual void AddIceCandidate(const std::string& aTransportId,
                               const std::string& aCandidate,
                               const std::string& aUFrag) = 0;

  virtual void UpdateNetworkState(bool aOnline) = 0;

  // dom::RTCStatsReportInternal doesn't have move semantics.
  typedef MozPromise<std::unique_ptr<dom::RTCStatsReportInternal>, nsresult,
                     true>
      StatsPromise;
  virtual RefPtr<StatsPromise> GetIceStats(
      const std::string& aTransportId, DOMHighResTimeStamp aNow,
      std::unique_ptr<dom::RTCStatsReportInternal>&& aReport) = 0;

  sigslot::signal2<const std::string&, const CandidateInfo&> SignalCandidate;
  sigslot::signal1<const std::string&> SignalAlpnNegotiated;
  sigslot::signal1<dom::RTCIceGatheringState> SignalGatheringStateChange;
  sigslot::signal1<dom::RTCIceConnectionState> SignalConnectionStateChange;

  sigslot::signal2<const std::string&, MediaPacket&> SignalPacketReceived;
  sigslot::signal2<const std::string&, MediaPacket&> SignalEncryptedSending;
  sigslot::signal2<const std::string&, TransportLayer::State> SignalStateChange;
  sigslot::signal2<const std::string&, TransportLayer::State>
      SignalRtcpStateChange;
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaTransportHandler)

  TransportLayer::State GetState(const std::string& aTransportId,
                                 bool aRtcp) const;

 protected:
  void OnCandidate(const std::string& aTransportId,
                   const CandidateInfo& aCandidateInfo);
  void OnAlpnNegotiated(const std::string& aAlpn);
  void OnGatheringStateChange(dom::RTCIceGatheringState aState);
  void OnConnectionStateChange(dom::RTCIceConnectionState aState);
  void OnPacketReceived(const std::string& aTransportId, MediaPacket& aPacket);
  void OnEncryptedSending(const std::string& aTransportId,
                          MediaPacket& aPacket);
  void OnStateChange(const std::string& aTransportId,
                     TransportLayer::State aState);
  void OnRtcpStateChange(const std::string& aTransportId,
                         TransportLayer::State aState);
  virtual ~MediaTransportHandler() = default;
  std::map<std::string, TransportLayer::State> mStateCache;
  std::map<std::string, TransportLayer::State> mRtcpStateCache;
  RefPtr<nsISerialEventTarget> mCallbackThread;
};

}  // namespace mozilla

#endif  //_MTRANSPORTHANDLER_H__
