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
#include "MediaTransportHandler.h"

#include "TransceiverImpl.h"

class nsIPrincipal;

namespace mozilla {
class DataChannel;
class PeerIdentity;
namespace dom {
class MediaStreamTrack;
}
}  // namespace mozilla

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
  explicit PeerConnectionMedia(PeerConnectionImpl* parent);

  nsresult Init();
  // WARNING: This destroys the object!
  void SelfDestruct();

  // Ensure ICE transports exist that we might need when offer/answer concludes
  void EnsureTransports(const JsepSession& aSession);

  // Activate ICE transports at the conclusion of offer/answer,
  // or when rollback occurs.
  nsresult UpdateTransports(const JsepSession& aSession,
                            const bool forceIceTcp);

  // Start ICE checks.
  void StartIceChecks(const JsepSession& session);

  // Process a trickle ICE candidate.
  void AddIceCandidate(const std::string& candidate,
                       const std::string& aTransportId,
                       const std::string& aUFrag);

  // Handle notifications of network online/offline events.
  void UpdateNetworkState(bool online);

  // Handle complete media pipelines.
  // This updates codec parameters, starts/stops send/receive, and other
  // stuff that doesn't necessarily require negotiation. This can be called at
  // any time, not just when an offer/answer exchange completes.
  // TODO: Let's move this to PeerConnectionImpl
  nsresult UpdateMediaPipelines();

  // TODO: Let's move the TransceiverImpl stuff to PeerConnectionImpl.
  nsresult AddTransceiver(JsepTransceiver* aJsepTransceiver,
                          dom::MediaStreamTrack& aReceiveTrack,
                          dom::MediaStreamTrack* aSendTrack,
                          RefPtr<TransceiverImpl>* aTransceiverImpl);

  void GetTransmitPipelinesMatching(
      const dom::MediaStreamTrack* aTrack,
      nsTArray<RefPtr<MediaPipeline>>* aPipelines);

  void GetReceivePipelinesMatching(const dom::MediaStreamTrack* aTrack,
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
  // When we finally learn who is on the other end, we need to change the
  // ownership on streams
  void UpdateRemoteStreamPrincipals_m(nsIPrincipal* aPrincipal);

  bool AnyCodecHasPluginID(uint64_t aPluginID);

  const nsCOMPtr<nsIThread>& GetMainThread() const { return mMainThread; }
  const nsCOMPtr<nsIEventTarget>& GetSTSThread() const { return mSTSThread; }

  // Used by PCImpl in a couple of places. Might be good to move that code in
  // here.
  std::vector<RefPtr<TransceiverImpl>>& GetTransceivers() {
    return mTransceivers;
  }

  nsPIDOMWindowInner* GetWindow() const;

  void AlpnNegotiated_s(const std::string& aAlpn);
  void AlpnNegotiated_m(const std::string& aAlpn);

  void ProxySettingReceived(bool aProxied);

  // TODO: Move to PeerConnectionImpl
  RefPtr<WebRtcCallWrapper> mCall;

  // mtransport objects
  RefPtr<MediaTransportHandler> mTransportHandler;

 private:
  void InitLocalAddrs();  // for stun local address IPC request
  nsresult InitProxy();
  void SetProxy();

  class StunAddrsHandler : public net::StunAddrsListener {
   public:
    explicit StunAddrsHandler(PeerConnectionMedia* pcm) : pcm_(pcm) {}
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
  void UpdateTransport(const JsepTransceiver& aTransceiver, bool aForceIceTcp);

  void GatherIfReady();
  void FlushIceCtxOperationQueueIfReady();
  void PerformOrEnqueueIceCtxOperation(nsIRunnable* runnable);
  nsresult SetTargetForDefaultLocalAddressLookup();
  void EnsureIceGathering(bool aDefaultRouteOnly);

  bool GetPrefDefaultAddressOnly() const;

  void ConnectSignals();

  // ICE events
  void IceGatheringStateChange_s(dom::RTCIceGatheringState aState);
  void IceConnectionStateChange_s(dom::RTCIceConnectionState aState);
  void OnCandidateFound_s(const std::string& aTransportId,
                          const CandidateInfo& aCandidateInfo);

  void IceGatheringStateChange_m(dom::RTCIceGatheringState aState);
  void IceConnectionStateChange_m(dom::RTCIceConnectionState aState);
  void OnCandidateFound_m(const std::string& aTransportId,
                          const CandidateInfo& aCandidateInfo);

  bool IsIceCtxReady() const {
    return mProxyResolveCompleted && mLocalAddrsCompleted;
  }

  // The parent PC
  PeerConnectionImpl* mParent;
  // and a loose handle on it for event driven stuff
  std::string mParentHandle;
  std::string mParentName;

  std::vector<RefPtr<TransceiverImpl>> mTransceivers;

  // The main thread.
  nsCOMPtr<nsIThread> mMainThread;

  // The STS thread.
  nsCOMPtr<nsIEventTarget> mSTSThread;

  // Used whenever we need to dispatch a runnable to STS to tweak something
  // on our ICE ctx, but are not ready to do so at the moment (eg; we are
  // waiting to get a callback with our http proxy config before we start
  // gathering or start checking)
  std::vector<nsCOMPtr<nsIRunnable>> mQueuedIceCtxOperations;

  // Used to track the state of the request.
  bool mProxyResolveCompleted;

  // Used to track proxy existence and socket proxy configuration.
  std::unique_ptr<NrSocketProxyConfig> mProxyConfig;

  // Used to cancel incoming stun addrs response
  RefPtr<net::StunAddrsRequestChild> mStunAddrsRequest;

  // Used to track the state of the stun addr IPC request
  bool mLocalAddrsCompleted;

  // Used to store the result of the stun addr IPC request
  nsTArray<NrIceStunAddr> mStunAddrs;

  // Used to ensure the target for default local address lookup is only set
  // once.
  bool mTargetForDefaultLocalAddressLookupIsSet;

  // Set to true when the object is going to be released.
  bool mDestroyed;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(PeerConnectionMedia)
};

}  // namespace mozilla

#endif
