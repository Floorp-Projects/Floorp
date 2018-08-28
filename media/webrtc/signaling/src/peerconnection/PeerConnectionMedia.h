/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _PEER_CONNECTION_MEDIA_H_
#define _PEER_CONNECTION_MEDIA_H_

#include <string>
#include <vector>
#include <map>

#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/net/StunAddrsRequestChild.h"
#include "nsIProtocolProxyCallback.h"

#include "TransceiverImpl.h"

class nsIPrincipal;

namespace mozilla {
class DataChannel;
class PeerIdentity;
namespace dom {
struct RTCInboundRTPStreamStats;
struct RTCOutboundRTPStreamStats;
class MediaStreamTrack;
}
}

#include "nricectxhandler.h"
#include "nriceresolver.h"
#include "nricemediastream.h"

namespace mozilla {

class PeerConnectionImpl;
class PeerConnectionMedia;
class PCUuidGenerator;
class MediaPipeline;
class MediaPipelineFilter;
class JsepSession;

// TODO(bug 1402997): If we move the TransceiverImpl stuff out of here, this
// will be a class that handles just the transport stuff, and we can rename it
// to something more explanatory (say, PeerConnectionTransportManager).
class PeerConnectionMedia : public sigslot::has_slots<> {
  ~PeerConnectionMedia();

 public:
  explicit PeerConnectionMedia(PeerConnectionImpl *parent);

  enum IceRestartState { ICE_RESTART_NONE,
                         ICE_RESTART_PROVISIONAL,
                         ICE_RESTART_COMMITTED
  };

  PeerConnectionImpl* GetPC() { return mParent; }
  nsresult Init(const std::vector<NrIceStunServer>& stun_servers,
                const std::vector<NrIceTurnServer>& turn_servers,
                NrIceCtx::Policy policy);
  // WARNING: This destroys the object!
  void SelfDestruct();

  RefPtr<NrIceCtxHandler> ice_ctx_hdlr() const { return mIceCtxHdlr; }
  RefPtr<NrIceCtx> ice_ctx() const { return mIceCtxHdlr->ctx(); }

  RefPtr<NrIceMediaStream> ice_media_stream(
      const std::string& aTransportId) const {
    return mIceCtxHdlr->ctx()->GetStream(aTransportId);
  }

  // Ensure ICE transports exist that we might need when offer/answer concludes
  void EnsureTransports(const JsepSession& aSession);

  // Activate ICE transports at the conclusion of offer/answer,
  // or when rollback occurs.
  nsresult UpdateTransports(const JsepSession& aSession,
                            const bool forceIceTcp);

  // Start ICE checks.
  void StartIceChecks(const JsepSession& session);

  bool IsIceRestarting() const;
  IceRestartState GetIceRestartState() const;

  // Begin ICE restart
  void BeginIceRestart(const std::string& ufrag,
                       const std::string& pwd);
  // Commit ICE Restart - offer/answer complete, no rollback possible
  void CommitIceRestart();
  // Finalize ICE restart
  void FinalizeIceRestart();
  // Abort ICE restart
  void RollbackIceRestart();

  // Process a trickle ICE candidate.
  void AddIceCandidate(const std::string& candidate,
                       const std::string& aTransportId);

  // Handle notifications of network online/offline events.
  void UpdateNetworkState(bool online);

  // Handle complete media pipelines.
  // This updates codec parameters, starts/stops send/receive, and other
  // stuff that doesn't necessarily require negotiation. This can be called at
  // any time, not just when an offer/answer exchange completes.
  // TODO: Let's move this to PeerConnectionImpl
  nsresult UpdateMediaPipelines();

  // TODO: Let's move the TransceiverImpl stuff to PeerConnectionImpl.
  nsresult AddTransceiver(
      JsepTransceiver* aJsepTransceiver,
      dom::MediaStreamTrack& aReceiveTrack,
      dom::MediaStreamTrack* aSendTrack,
      RefPtr<TransceiverImpl>* aTransceiverImpl);

  void GetTransmitPipelinesMatching(
      const dom::MediaStreamTrack* aTrack,
      nsTArray<RefPtr<MediaPipeline>>* aPipelines);

  void GetReceivePipelinesMatching(
      const dom::MediaStreamTrack* aTrack,
      nsTArray<RefPtr<MediaPipeline>>* aPipelines);

  std::string GetTransportIdMatching(const dom::MediaStreamTrack& aTrack) const;

  nsresult AddRIDExtension(dom::MediaStreamTrack& aRecvTrack,
                           unsigned short aExtensionId);

  nsresult AddRIDFilter(dom::MediaStreamTrack& aRecvTrack,
                        const nsAString& aRid);

  // In cases where the peer isn't yet identified, we disable the pipeline (not
  // the stream, that would potentially affect others), so that it sends
  // black/silence.  Once the peer is identified, re-enable those streams.
  // aTrack will be set if this update came from a principal change on aTrack.
  // TODO: Move to PeerConnectionImpl
  void UpdateSinkIdentity_m(const dom::MediaStreamTrack* aTrack,
                            nsIPrincipal* aPrincipal,
                            const PeerIdentity* aSinkIdentity);
  // this determines if any track is peerIdentity constrained
  bool AnyLocalTrackHasPeerIdentity() const;
  // When we finally learn who is on the other end, we need to change the ownership
  // on streams
  void UpdateRemoteStreamPrincipals_m(nsIPrincipal* aPrincipal);

  bool AnyCodecHasPluginID(uint64_t aPluginID);

  const nsCOMPtr<nsIThread>& GetMainThread() const { return mMainThread; }
  const nsCOMPtr<nsIEventTarget>& GetSTSThread() const { return mSTSThread; }

  // Get a transport flow either RTP/RTCP for a particular stream
  // A stream can be of audio/video/datachannel/budled(?) types
  RefPtr<TransportFlow> GetTransportFlow(const std::string& aId,
                                         bool aIsRtcp) {
    auto& flows = aIsRtcp ? mRtcpTransportFlows : mTransportFlows;
    auto it = flows.find(aId);
    if (it == flows.end()) {
      return nullptr;
    }

    return it->second;
  }

  // Used by PCImpl in a couple of places. Might be good to move that code in
  // here.
  std::vector<RefPtr<TransceiverImpl>>& GetTransceivers()
  {
    return mTransceivers;
  }

  // Add a transport flow
  void AddTransportFlow(const std::string& aId, bool aRtcp,
                        const RefPtr<TransportFlow> &aFlow);
  void RemoveTransportFlow(const std::string& aId, bool aRtcp);
  void ConnectDtlsListener_s(const RefPtr<TransportFlow>& aFlow);
  void DtlsConnected_s(TransportLayer* aFlow,
                       TransportLayer::State state);
  static void DtlsConnected_m(const std::string& aParentHandle,
                              bool aPrivacyRequested);

  // ICE state signals
  sigslot::signal2<NrIceCtx*, NrIceCtx::GatheringState>
      SignalIceGatheringStateChange;
  sigslot::signal2<NrIceCtx*, NrIceCtx::ConnectionState>
      SignalIceConnectionStateChange;
  // This passes a candidate:... attribute and transport id
  sigslot::signal2<const std::string&, const std::string&> SignalCandidate;
  // This passes address, port, transport id of the default candidate.
  sigslot::signal5<const std::string&, uint16_t,
                   const std::string&, uint16_t, const std::string&>
      SignalUpdateDefaultCandidate;
  sigslot::signal1<const std::string&>
      SignalEndOfLocalCandidates;

  // TODO: Move to PeerConnectionImpl
  RefPtr<WebRtcCallWrapper> mCall;

 private:
  void InitLocalAddrs(); // for stun local address IPC request
  nsresult InitProxy();
  class ProtocolProxyQueryHandler : public nsIProtocolProxyCallback {
   public:
    explicit ProtocolProxyQueryHandler(PeerConnectionMedia *pcm) :
      pcm_(pcm) {}

    NS_IMETHOD OnProxyAvailable(nsICancelable *request,
                                nsIChannel *aChannel,
                                nsIProxyInfo *proxyinfo,
                                nsresult result) override;
    NS_DECL_ISUPPORTS

   private:
    void SetProxyOnPcm(nsIProxyInfo& proxyinfo);
    RefPtr<PeerConnectionMedia> pcm_;
    virtual ~ProtocolProxyQueryHandler() {}
  };

  class StunAddrsHandler : public net::StunAddrsListener {
   public:
    explicit StunAddrsHandler(PeerConnectionMedia *pcm) :
      pcm_(pcm) {}
    void OnStunAddrsAvailable(
        const mozilla::net::NrIceStunAddrArray& addrs) override;
   private:
    RefPtr<PeerConnectionMedia> pcm_;
    virtual ~StunAddrsHandler() {}
  };

  // Shutdown media transport. Must be called on STS thread.
  void ShutdownMediaTransport_s();

  // Final destruction of the media stream. Must be called on the main
  // thread.
  void SelfDestruct_m();

  // Manage ICE transports.
  nsresult UpdateTransport(const JsepTransceiver& aTransceiver,
                           bool aForceIceTcp);

  void EnsureTransport_s(const std::string& aTransportId,
                         size_t aComponentCount);
  void ActivateTransport_s(const std::string& aTransportId,
                           size_t aComponentCount,
                           const std::string& aUfrag,
                           const std::string& aPassword,
                           const std::vector<std::string>& aCandidateList);
  void RemoveTransportsExcept_s(const std::set<std::string>& aTransportIds);
  nsresult UpdateTransportFlows(const JsepTransceiver& transceiver);
  nsresult UpdateTransportFlow(bool aIsRtcp,
                               const JsepTransport& aTransport);

  void GatherIfReady();
  void FlushIceCtxOperationQueueIfReady();
  void PerformOrEnqueueIceCtxOperation(nsIRunnable* runnable);
  void EnsureIceGathering_s(bool aDefaultRouteOnly, bool aProxyOnly);
  void StartIceChecks_s(bool aIsControlling,
                        bool aIsOfferer,
                        bool aIsIceLite,
                        const std::vector<std::string>& aIceOptionsList);

  void BeginIceRestart_s(RefPtr<NrIceCtx> new_ctx);
  void FinalizeIceRestart_s();
  void RollbackIceRestart_s();
  bool GetPrefDefaultAddressOnly() const;
  bool GetPrefProxyOnly() const;

  void ConnectSignals(NrIceCtx *aCtx, NrIceCtx *aOldCtx=nullptr);

  // Process a trickle ICE candidate.
  void AddIceCandidate_s(const std::string& aCandidate,
                         const std::string& aTransportId);

  void UpdateNetworkState_s(bool online);

  // ICE events
  void IceGatheringStateChange_s(NrIceCtx* ctx,
                               NrIceCtx::GatheringState state);
  void IceConnectionStateChange_s(NrIceCtx* ctx,
                                NrIceCtx::ConnectionState state);
  void IceStreamReady_s(NrIceMediaStream *aStream);
  void OnCandidateFound_s(NrIceMediaStream *aStream,
                          const std::string& aCandidate);
  void EndOfLocalCandidates(const std::string& aDefaultAddr,
                            uint16_t aDefaultPort,
                            const std::string& aDefaultRtcpAddr,
                            uint16_t aDefaultRtcpPort,
                            const std::string& aTransportId);
  void GetDefaultCandidates(const NrIceMediaStream& aStream,
                            NrIceCandidate* aCandidate,
                            NrIceCandidate* aRtcpCandidate);

  void IceGatheringStateChange_m(NrIceCtx* ctx,
                                 NrIceCtx::GatheringState state);
  void IceConnectionStateChange_m(NrIceCtx* ctx,
                                  NrIceCtx::ConnectionState state);
  void OnCandidateFound_m(const std::string& aCandidateLine,
                          const std::string& aDefaultAddr,
                          uint16_t aDefaultPort,
                          const std::string& aDefaultRtcpAddr,
                          uint16_t aDefaultRtcpPort,
                          const std::string& aTransportId);
  void EndOfLocalCandidates_m(const std::string& aDefaultAddr,
                              uint16_t aDefaultPort,
                              const std::string& aDefaultRtcpAddr,
                              uint16_t aDefaultRtcpPort,
                              const std::string& aTransportId);
  bool IsIceCtxReady() const {
    return mProxyResolveCompleted && mLocalAddrsCompleted;
  }

  // The parent PC
  PeerConnectionImpl *mParent;
  // and a loose handle on it for event driven stuff
  std::string mParentHandle;
  std::string mParentName;

  std::vector<RefPtr<TransceiverImpl>> mTransceivers;

  // ICE objects
  RefPtr<NrIceCtxHandler> mIceCtxHdlr;

  // DNS
  RefPtr<NrIceResolver> mDNSResolver;

  // Transport flows for RTP and RTP/RTCP mux
  std::map<std::string, RefPtr<TransportFlow> > mTransportFlows;

  // Transport flows for standalone RTCP (rarely used)
  std::map<std::string, RefPtr<TransportFlow> > mRtcpTransportFlows;

  // UUID Generator
  UniquePtr<PCUuidGenerator> mUuidGen;

  // The main thread.
  nsCOMPtr<nsIThread> mMainThread;

  // The STS thread.
  nsCOMPtr<nsIEventTarget> mSTSThread;

  // Used whenever we need to dispatch a runnable to STS to tweak something
  // on our ICE ctx, but are not ready to do so at the moment (eg; we are
  // waiting to get a callback with our http proxy config before we start
  // gathering or start checking)
  std::vector<nsCOMPtr<nsIRunnable>> mQueuedIceCtxOperations;

  // Used to cancel any ongoing proxy request.
  nsCOMPtr<nsICancelable> mProxyRequest;

  // Used to track the state of the request.
  bool mProxyResolveCompleted;

  // Used to store the result of the request.
  UniquePtr<NrIceProxyServer> mProxyServer;

  // Used to track the state of ice restart
  IceRestartState mIceRestartState;

  // Used to cancel incoming stun addrs response
  RefPtr<net::StunAddrsRequestChild> mStunAddrsRequest;

  // Used to track the state of the stun addr IPC request
  bool mLocalAddrsCompleted;

  // Used to store the result of the stun addr IPC request
  nsTArray<NrIceStunAddr> mStunAddrs;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(PeerConnectionMedia)
};

} // namespace mozilla

#endif
