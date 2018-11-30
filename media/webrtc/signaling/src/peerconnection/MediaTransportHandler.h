/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MTRANSPORTHANDLER_H__
#define _MTRANSPORTHANDLER_H__

#include "mozilla/RefPtr.h"
#include "nsISupportsImpl.h"
#include "sigslot.h"
#include "transportlayer.h"  // Need the State enum
#include "mozilla/dom/PeerConnectionImplEnumsBinding.h"
#include "nricectx.h"               // Need some enums
#include "nsDOMNavigationTiming.h"  // DOMHighResTimeStamp

// For RTCStatsQueryPromise typedef
#include "signaling/src/peerconnection/PeerConnectionImpl.h"

#include "nsString.h"

#include <map>
#include <string>
#include <set>
#include <vector>

namespace mozilla {
class DtlsIdentity;  // TODO(bug 1494311) Use IPC type
class NrIceCtx;
class NrIceMediaStream;
class NrIceResolver;
class SdpFingerprintAttributeList;  // TODO(bug 1494311) Use IPC type
class TransportFlow;
class RTCStatsQuery;

namespace dom {
struct RTCConfiguration;
struct RTCStatsReportInternal;
}  // namespace dom

// Base-class, makes some testing easier
class MediaTransportBase {
 public:
  virtual void SendPacket(const std::string& aTransportId,
                          MediaPacket& aPacket) = 0;

  virtual TransportLayer::State GetState(const std::string& aTransportId,
                                         bool aRtcp) const = 0;
  sigslot::signal2<const std::string&, MediaPacket&> SignalPacketReceived;
  sigslot::signal2<const std::string&, MediaPacket&> SignalEncryptedSending;
  sigslot::signal2<const std::string&, TransportLayer::State> SignalStateChange;
  sigslot::signal2<const std::string&, TransportLayer::State>
      SignalRtcpStateChange;
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaTransportBase)

 protected:
  virtual ~MediaTransportBase() {}
};

class MediaTransportHandler : public MediaTransportBase,
                              public sigslot::has_slots<> {
 public:
  MediaTransportHandler();
  nsresult Init(const std::string& aName,
                const dom::RTCConfiguration& aConfiguration);
  void Destroy();

  // We will probably be able to move the proxy lookup stuff into
  // this class once we move mtransport to its own process.
  void SetProxyServer(NrSocketProxyConfig&& aProxyConfig);

  void EnsureProvisionalTransport(const std::string& aTransportId,
                                  const std::string& aLocalUfrag,
                                  const std::string& aLocalPwd,
                                  size_t aComponentCount);

  // We set default-route-only as late as possible because it depends on what
  // capture permissions have been granted on the window, which could easily
  // change between Init (ie; when the PC is created) and StartIceGathering
  // (ie; when we set the local description).
  void StartIceGathering(bool aDefaultRouteOnly,
                         // This will go away once mtransport moves to its
                         // own process, because we won't need to get this
                         // via IPC anymore
                         const nsTArray<NrIceStunAddr>& aStunAddrs);

  void ActivateTransport(const std::string& aTransportId,
                         const std::string& aLocalUfrag,
                         const std::string& aLocalPwd, size_t aComponentCount,
                         const std::string& aUfrag,
                         const std::string& aPassword,
                         const std::vector<std::string>& aCandidateList,
                         // TODO(bug 1494311): Use an IPC type.
                         RefPtr<DtlsIdentity> aDtlsIdentity, bool aDtlsClient,
                         // TODO(bug 1494311): Use IPC type
                         const SdpFingerprintAttributeList& aFingerprints,
                         bool aPrivacyRequested);

  void RemoveTransportsExcept(const std::set<std::string>& aTransportIds);

  void StartIceChecks(bool aIsControlling, bool aIsOfferer,
                      const std::vector<std::string>& aIceOptions);

  void AddIceCandidate(const std::string& aTransportId,
                       const std::string& aCandidate);

  void UpdateNetworkState(bool aOnline);

  void SendPacket(const std::string& aTransportId,
                  MediaPacket& aPacket) override;

  // TODO(bug 1494312): Figure out how this fits with an async API. Maybe we
  // cache on the content process.
  TransportLayer::State GetState(const std::string& aTransportId,
                                 bool aRtcp) const override;

  RefPtr<RTCStatsQueryPromise> GetIceStats(UniquePtr<RTCStatsQuery>&& aQuery);

  typedef MozPromise<dom::Sequence<nsString>, nsresult, true> IceLogPromise;

  static RefPtr<IceLogPromise> GetIceLog(const nsCString& aPattern);

  static void ClearIceLog();

  static void EnterPrivateMode();
  static void ExitPrivateMode();

  // TODO(bug 1494311) Use IPC type
  struct CandidateInfo {
    std::string mCandidate;
    std::string mDefaultHostRtp;
    uint16_t mDefaultPortRtp = 0;
    std::string mDefaultHostRtcp;
    uint16_t mDefaultPortRtcp = 0;
  };

  sigslot::signal2<const std::string&, const CandidateInfo&> SignalCandidate;
  sigslot::signal1<const std::string&> SignalAlpnNegotiated;
  sigslot::signal1<dom::PCImplIceGatheringState> SignalGatheringStateChange;
  sigslot::signal1<dom::PCImplIceConnectionState> SignalConnectionStateChange;

 private:
  RefPtr<TransportFlow> CreateTransportFlow(
      const std::string& aTransportId, bool aIsRtcp,
      RefPtr<DtlsIdentity> aDtlsIdentity, bool aDtlsClient,
      // TODO(bug 1494312) Use IPC type
      const SdpFingerprintAttributeList& aFingerprints, bool aPrivacyRequested);

  struct Transport {
    RefPtr<TransportFlow> mFlow;
    RefPtr<TransportFlow> mRtcpFlow;
  };

  void OnGatheringStateChange(NrIceCtx* aIceCtx,
                              NrIceCtx::GatheringState aState);
  void OnConnectionStateChange(NrIceCtx* aIceCtx,
                               NrIceCtx::ConnectionState aState);
  void OnCandidateFound(NrIceMediaStream* aStream,
                        const std::string& aCandidate);
  void OnStateChange(TransportLayer* aLayer, TransportLayer::State);
  void OnRtcpStateChange(TransportLayer* aLayer, TransportLayer::State);
  void PacketReceived(TransportLayer* aLayer, MediaPacket& aPacket);
  void EncryptedPacketSending(TransportLayer* aLayer, MediaPacket& aPacket);
  RefPtr<TransportFlow> GetTransportFlow(const std::string& aId,
                                         bool aIsRtcp) const;
  void GetIceStats(const NrIceMediaStream& aStream, DOMHighResTimeStamp aNow,
                   dom::RTCStatsReportInternal* aReport) const;

  ~MediaTransportHandler() override;
  RefPtr<NrIceCtx> mIceCtx;
  RefPtr<NrIceResolver> mDNSResolver;
  std::map<std::string, Transport> mTransports;
  bool mProxyOnly = false;
};
}  // namespace mozilla

#endif  //_MTRANSPORTHANDLER_H__
