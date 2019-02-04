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
#include "mozilla/dom/PeerConnectionImplEnumsBinding.h"
#include "mozilla/dom/RTCConfigurationBinding.h"
#include "nricectx.h"               // Need some enums
#include "nsDOMNavigationTiming.h"  // DOMHighResTimeStamp
#include "signaling/src/common/CandidateInfo.h"

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
  // as appropriate.
  static already_AddRefed<MediaTransportHandler> Create();

  typedef MozPromise<dom::Sequence<nsString>, nsresult, true> IceLogPromise;

  // There's a wrinkle here; the ICE logging is not separated out by
  // MediaTransportHandler. These are a little more like static methods, but
  // to avoid needing yet another IPC interface, we bolt them on here.
  virtual RefPtr<IceLogPromise> GetIceLog(const nsCString& aPattern) = 0;
  virtual void ClearIceLog() = 0;
  virtual void EnterPrivateMode() = 0;
  virtual void ExitPrivateMode() = 0;

  virtual nsresult Init(const std::string& aName,
                        const nsTArray<dom::RTCIceServer>& aIceServers,
                        dom::RTCIceTransportPolicy aIcePolicy) = 0;
  virtual void Destroy() = 0;

  // We will probably be able to move the proxy lookup stuff into
  // this class once we move mtransport to its own process.
  virtual void SetProxyServer(NrSocketProxyConfig&& aProxyConfig) = 0;

  virtual void EnsureProvisionalTransport(const std::string& aTransportId,
                                          const std::string& aLocalUfrag,
                                          const std::string& aLocalPwd,
                                          size_t aComponentCount) = 0;

  // We set default-route-only as late as possible because it depends on what
  // capture permissions have been granted on the window, which could easily
  // change between Init (ie; when the PC is created) and StartIceGathering
  // (ie; when we set the local description).
  virtual void StartIceGathering(bool aDefaultRouteOnly,
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

  virtual void StartIceChecks(bool aIsControlling, bool aIsOfferer,
                              const std::vector<std::string>& aIceOptions) = 0;

  virtual void SendPacket(const std::string& aTransportId,
                          MediaPacket&& aPacket) = 0;

  virtual void AddIceCandidate(const std::string& aTransportId,
                               const std::string& aCandidate) = 0;

  virtual void UpdateNetworkState(bool aOnline) = 0;

  virtual TransportLayer::State GetState(const std::string& aTransportId,
                                         bool aRtcp) const = 0;

  // dom::RTCStatsReportInternal doesn't have move semantics.
  typedef MozPromise<std::unique_ptr<dom::RTCStatsReportInternal>, nsresult,
                     true>
      StatsPromise;
  virtual RefPtr<StatsPromise> GetIceStats(
      const std::string& aTransportId, DOMHighResTimeStamp aNow,
      std::unique_ptr<dom::RTCStatsReportInternal>&& aReport) = 0;

  sigslot::signal2<const std::string&, const CandidateInfo&> SignalCandidate;
  sigslot::signal1<const std::string&> SignalAlpnNegotiated;
  sigslot::signal1<dom::PCImplIceGatheringState> SignalGatheringStateChange;
  sigslot::signal1<dom::PCImplIceConnectionState> SignalConnectionStateChange;

  sigslot::signal2<const std::string&, MediaPacket&> SignalPacketReceived;
  sigslot::signal2<const std::string&, MediaPacket&> SignalEncryptedSending;
  sigslot::signal2<const std::string&, TransportLayer::State> SignalStateChange;
  sigslot::signal2<const std::string&, TransportLayer::State>
      SignalRtcpStateChange;
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaTransportHandler)

 protected:
  virtual ~MediaTransportHandler() = default;
};

}  // namespace mozilla

#endif  //_MTRANSPORTHANDLER_H__
