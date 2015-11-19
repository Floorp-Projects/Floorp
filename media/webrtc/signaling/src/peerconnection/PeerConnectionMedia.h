/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _PEER_CONNECTION_MEDIA_H_
#define _PEER_CONNECTION_MEDIA_H_

#include <string>
#include <vector>
#include <map>

#include "nspr.h"
#include "prlock.h"

#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "nsComponentManagerUtils.h"
#include "nsIProtocolProxyCallback.h"

#include "signaling/src/jsep/JsepSession.h"
#include "AudioSegment.h"

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
#include "Layers.h"
#include "VideoUtils.h"
#include "ImageLayers.h"
#include "VideoSegment.h"
#include "MediaStreamTrack.h"
#endif

class nsIPrincipal;

namespace mozilla {
class DataChannel;
class PeerIdentity;
class MediaPipelineFactory;
namespace dom {
struct RTCInboundRTPStreamStats;
struct RTCOutboundRTPStreamStats;
}
}

#include "nricectxhandler.h"
#include "nriceresolver.h"
#include "nricemediastream.h"
#include "MediaPipeline.h"

namespace mozilla {

class PeerConnectionImpl;
class PeerConnectionMedia;
class PCUuidGenerator;

class SourceStreamInfo {
public:
  SourceStreamInfo(DOMMediaStream* aMediaStream,
                   PeerConnectionMedia *aParent,
                   const std::string& aId)
      : mMediaStream(aMediaStream),
        mParent(aParent),
        mId(aId) {
    MOZ_ASSERT(mMediaStream);
  }

  SourceStreamInfo(already_AddRefed<DOMMediaStream>& aMediaStream,
                   PeerConnectionMedia *aParent,
                   const std::string& aId)
      : mMediaStream(aMediaStream),
        mParent(aParent),
        mId(aId) {
    MOZ_ASSERT(mMediaStream);
  }

  virtual ~SourceStreamInfo() {}

  DOMMediaStream* GetMediaStream() const {
    return mMediaStream;
  }

  nsresult StorePipeline(const std::string& trackId,
                         const RefPtr<MediaPipeline>& aPipeline);

  virtual void AddTrack(const std::string& trackId,
                        const RefPtr<dom::MediaStreamTrack>& aTrack)
  {
    mTracks.insert(std::make_pair(trackId, aTrack));
  }
  virtual void RemoveTrack(const std::string& trackId);
  bool HasTrack(const std::string& trackId) const
  {
    return !!mTracks.count(trackId);
  }
  size_t GetTrackCount() const { return mTracks.size(); }

  // This method exists for stats and the unittests.
  // It allows visibility into the pipelines and flows.
  const std::map<std::string, RefPtr<MediaPipeline>>&
  GetPipelines() const { return mPipelines; }
  RefPtr<MediaPipeline> GetPipelineByTrackId_m(const std::string& trackId);
  // This is needed so PeerConnectionImpl can unregister itself as
  // PrincipalChangeObserver from each track.
  const std::map<std::string, RefPtr<dom::MediaStreamTrack>>&
  GetMediaStreamTracks() const { return mTracks; }
  dom::MediaStreamTrack* GetTrackById(const std::string& trackId) const
  {
    auto it = mTracks.find(trackId);
    if (it == mTracks.end()) {
      return nullptr;
    }

    return it->second;
  }
  const std::string& GetId() const { return mId; }

  void DetachTransport_s();
  virtual void DetachMedia_m();
  bool AnyCodecHasPluginID(uint64_t aPluginID);
protected:
  void EndTrack(MediaStream* stream, dom::MediaStreamTrack* track);
  RefPtr<DOMMediaStream> mMediaStream;
  PeerConnectionMedia *mParent;
  const std::string mId;
  // These get set up before we generate our local description, the pipelines
  // and conduits are set up once offer/answer completes.
  std::map<std::string, RefPtr<dom::MediaStreamTrack>> mTracks;
  std::map<std::string, RefPtr<MediaPipeline>> mPipelines;
};

// TODO(ekr@rtfm.com): Refactor {Local,Remote}SourceStreamInfo
// bug 837539.
class LocalSourceStreamInfo : public SourceStreamInfo {
  ~LocalSourceStreamInfo() {
    mMediaStream = nullptr;
  }
public:
  LocalSourceStreamInfo(DOMMediaStream *aMediaStream,
                        PeerConnectionMedia *aParent,
                        const std::string& aId)
     : SourceStreamInfo(aMediaStream, aParent, aId) {}

  nsresult TakePipelineFrom(RefPtr<LocalSourceStreamInfo>& info,
                            const std::string& oldTrackId,
                            dom::MediaStreamTrack& aNewTrack,
                            const std::string& newTrackId);

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  void UpdateSinkIdentity_m(dom::MediaStreamTrack* aTrack,
                            nsIPrincipal* aPrincipal,
                            const PeerIdentity* aSinkIdentity);
#endif

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(LocalSourceStreamInfo)

private:
  already_AddRefed<MediaPipeline> ForgetPipelineByTrackId_m(
      const std::string& trackId);
};

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
class RemoteTrackSource : public dom::MediaStreamTrackSource
{
public:
  explicit RemoteTrackSource(nsIPrincipal* aPrincipal, const nsString& aLabel)
    : dom::MediaStreamTrackSource(aPrincipal, true, aLabel) {}

  dom::MediaSourceEnum GetMediaSource() const override
  {
    return dom::MediaSourceEnum::Other;
  }

  already_AddRefed<PledgeVoid>
  ApplyConstraints(nsPIDOMWindowInner* aWindow,
                   const dom::MediaTrackConstraints& aConstraints) override;

  void Stop() override { NS_ERROR("Can't stop a remote source!"); }

  void SetPrincipal(nsIPrincipal* aPrincipal)
  {
    mPrincipal = aPrincipal;
    PrincipalChanged();
  }

protected:
  virtual ~RemoteTrackSource() {}
};
#endif

class RemoteSourceStreamInfo : public SourceStreamInfo {
  ~RemoteSourceStreamInfo() {}
 public:
  RemoteSourceStreamInfo(already_AddRefed<DOMMediaStream> aMediaStream,
                         PeerConnectionMedia *aParent,
                         const std::string& aId)
    : SourceStreamInfo(aMediaStream, aParent, aId),
      mReceiving(false)
  {
  }

  void DetachMedia_m() override;
  void RemoveTrack(const std::string& trackId) override;
  void SyncPipeline(RefPtr<MediaPipelineReceive> aPipeline);

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  void UpdatePrincipal_m(nsIPrincipal* aPrincipal);
#endif

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RemoteSourceStreamInfo)

  void AddTrack(const std::string& trackId,
                const RefPtr<dom::MediaStreamTrack>& aTrack) override
  {
    SourceStreamInfo::AddTrack(trackId, aTrack);
  }

  TrackID GetNumericTrackId(const std::string& trackId) const
  {
    dom::MediaStreamTrack* track = GetTrackById(trackId);
    if (!track) {
      return TRACK_INVALID;
    }
    return track->mTrackID;
  }

  void StartReceiving();

 private:
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  // MediaStreamTrackSources associated with this remote stream.
  // We use them for updating their principal if that's needed.
  std::vector<RefPtr<RemoteTrackSource>> mTrackSources;
#endif

  // True iff SetPullEnabled(true) has been called on the DOMMediaStream. This
  // happens when offer/answer concludes.
  bool mReceiving;
};

class PeerConnectionMedia : public sigslot::has_slots<> {
  ~PeerConnectionMedia()
  {
    MOZ_RELEASE_ASSERT(!mMainThread);
  }

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

  RefPtr<NrIceMediaStream> ice_media_stream(size_t i) const {
    return mIceCtxHdlr->ctx()->GetStream(i);
  }

  size_t num_ice_media_streams() const {
    return mIceCtxHdlr->ctx()->GetStreamCount();
  }

  // Ensure ICE transports exist that we might need when offer/answer concludes
  void EnsureTransports(const JsepSession& aSession);

  // Activate or remove ICE transports at the conclusion of offer/answer,
  // or when rollback occurs.
  void ActivateOrRemoveTransports(const JsepSession& aSession);

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
  void AddIceCandidate(const std::string& candidate, const std::string& mid,
                       uint32_t aMLine);

  // Handle complete media pipelines.
  nsresult UpdateMediaPipelines(const JsepSession& session);

  // Add a track (main thread only)
  nsresult AddTrack(DOMMediaStream& aMediaStream,
                    const std::string& streamId,
                    dom::MediaStreamTrack& aTrack,
                    const std::string& trackId);

  nsresult RemoveLocalTrack(const std::string& streamId,
                            const std::string& trackId);
  nsresult RemoveRemoteTrack(const std::string& streamId,
                            const std::string& trackId);

  // Get a specific local stream
  uint32_t LocalStreamsLength()
  {
    return mLocalSourceStreams.Length();
  }
  LocalSourceStreamInfo* GetLocalStreamByIndex(int index);
  LocalSourceStreamInfo* GetLocalStreamById(const std::string& id);
  LocalSourceStreamInfo* GetLocalStreamByTrackId(const std::string& id);

  // Get a specific remote stream
  uint32_t RemoteStreamsLength()
  {
    return mRemoteSourceStreams.Length();
  }

  RemoteSourceStreamInfo* GetRemoteStreamByIndex(size_t index);
  RemoteSourceStreamInfo* GetRemoteStreamById(const std::string& id);
  RemoteSourceStreamInfo* GetRemoteStreamByTrackId(const std::string& id);

  // Add a remote stream.
  nsresult AddRemoteStream(RefPtr<RemoteSourceStreamInfo> aInfo);

  nsresult ReplaceTrack(const std::string& aOldStreamId,
                        const std::string& aOldTrackId,
                        dom::MediaStreamTrack& aNewTrack,
                        const std::string& aNewStreamId,
                        const std::string& aNewTrackId);

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  // In cases where the peer isn't yet identified, we disable the pipeline (not
  // the stream, that would potentially affect others), so that it sends
  // black/silence.  Once the peer is identified, re-enable those streams.
  // aTrack will be set if this update came from a principal change on aTrack.
  void UpdateSinkIdentity_m(dom::MediaStreamTrack* aTrack,
                            nsIPrincipal* aPrincipal,
                            const PeerIdentity* aSinkIdentity);
  // this determines if any track is peerIdentity constrained
  bool AnyLocalTrackHasPeerIdentity() const;
  // When we finally learn who is on the other end, we need to change the ownership
  // on streams
  void UpdateRemoteStreamPrincipals_m(nsIPrincipal* aPrincipal);
#endif

  bool AnyCodecHasPluginID(uint64_t aPluginID);

  const nsCOMPtr<nsIThread>& GetMainThread() const { return mMainThread; }
  const nsCOMPtr<nsIEventTarget>& GetSTSThread() const { return mSTSThread; }

  static size_t GetTransportFlowIndex(int aStreamIndex, bool aRtcp)
  {
    return aStreamIndex * 2 + (aRtcp ? 1 : 0);
  }

  // Get a transport flow either RTP/RTCP for a particular stream
  // A stream can be of audio/video/datachannel/budled(?) types
  RefPtr<TransportFlow> GetTransportFlow(int aStreamIndex, bool aIsRtcp) {
    int index_inner = GetTransportFlowIndex(aStreamIndex, aIsRtcp);

    if (mTransportFlows.find(index_inner) == mTransportFlows.end())
      return nullptr;

    return mTransportFlows[index_inner];
  }

  // Add a transport flow
  void AddTransportFlow(int aIndex, bool aRtcp,
                        const RefPtr<TransportFlow> &aFlow);
  void RemoveTransportFlow(int aIndex, bool aRtcp);
  void ConnectDtlsListener_s(const RefPtr<TransportFlow>& aFlow);
  void DtlsConnected_s(TransportLayer* aFlow,
                       TransportLayer::State state);
  static void DtlsConnected_m(const std::string& aParentHandle,
                              bool aPrivacyRequested);

  RefPtr<AudioSessionConduit> GetAudioConduit(size_t level) {
    auto it = mConduits.find(level);
    if (it == mConduits.end()) {
      return nullptr;
    }

    if (it->second.first) {
      MOZ_ASSERT(false, "In GetAudioConduit, we found a video conduit!");
      return nullptr;
    }

    return RefPtr<AudioSessionConduit>(
        static_cast<AudioSessionConduit*>(it->second.second.get()));
  }

  RefPtr<VideoSessionConduit> GetVideoConduit(size_t level) {
    auto it = mConduits.find(level);
    if (it == mConduits.end()) {
      return nullptr;
    }

    if (!it->second.first) {
      MOZ_ASSERT(false, "In GetVideoConduit, we found an audio conduit!");
      return nullptr;
    }

    return RefPtr<VideoSessionConduit>(
        static_cast<VideoSessionConduit*>(it->second.second.get()));
  }

  // Add a conduit
  void AddAudioConduit(size_t level, const RefPtr<AudioSessionConduit> &aConduit) {
    mConduits[level] = std::make_pair(false, aConduit);
  }

  void AddVideoConduit(size_t level, const RefPtr<VideoSessionConduit> &aConduit) {
    mConduits[level] = std::make_pair(true, aConduit);
  }

  // ICE state signals
  sigslot::signal2<NrIceCtx*, NrIceCtx::GatheringState>
      SignalIceGatheringStateChange;
  sigslot::signal2<NrIceCtx*, NrIceCtx::ConnectionState>
      SignalIceConnectionStateChange;
  // This passes a candidate:... attribute  and level
  sigslot::signal2<const std::string&, uint16_t> SignalCandidate;
  // This passes address, port, level of the default candidate.
  sigslot::signal5<const std::string&, uint16_t,
                   const std::string&, uint16_t, uint16_t>
      SignalUpdateDefaultCandidate;
  sigslot::signal1<uint16_t>
      SignalEndOfLocalCandidates;

 private:
  nsresult InitProxy();
  class ProtocolProxyQueryHandler : public nsIProtocolProxyCallback {
   public:
    explicit ProtocolProxyQueryHandler(PeerConnectionMedia *pcm) :
      pcm_(pcm) {}

    NS_IMETHODIMP OnProxyAvailable(nsICancelable *request,
                                   nsIChannel *aChannel,
                                   nsIProxyInfo *proxyinfo,
                                   nsresult result) override;
    NS_DECL_ISUPPORTS

   private:
    void SetProxyOnPcm(nsIProxyInfo& proxyinfo);
    RefPtr<PeerConnectionMedia> pcm_;
    virtual ~ProtocolProxyQueryHandler() {}
  };

  // Shutdown media transport. Must be called on STS thread.
  void ShutdownMediaTransport_s();

  // Final destruction of the media stream. Must be called on the main
  // thread.
  void SelfDestruct_m();

  // Manage ICE transports.
  void EnsureTransport_s(size_t aLevel, size_t aComponentCount);
  void ActivateOrRemoveTransport_s(
      size_t aMLine,
      size_t aComponentCount,
      const std::string& aUfrag,
      const std::string& aPassword,
      const std::vector<std::string>& aCandidateList);
  void RemoveTransportsAtOrAfter_s(size_t aMLine);

  void GatherIfReady();
  void FlushIceCtxOperationQueueIfReady();
  void PerformOrEnqueueIceCtxOperation(nsIRunnable* runnable);
  void EnsureIceGathering_s();
  void StartIceChecks_s(bool aIsControlling,
                        bool aIsIceLite,
                        const std::vector<std::string>& aIceOptionsList);

  void BeginIceRestart_s(RefPtr<NrIceCtx> new_ctx);
  void FinalizeIceRestart_s();
  void RollbackIceRestart_s();
  bool GetPrefDefaultAddressOnly() const;

  void ConnectSignals(NrIceCtx *aCtx, NrIceCtx *aOldCtx=nullptr);

  // Process a trickle ICE candidate.
  void AddIceCandidate_s(const std::string& aCandidate, const std::string& aMid,
                         uint32_t aMLine);


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
                            uint16_t aMLine);
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
                          uint16_t aMLine);
  void EndOfLocalCandidates_m(const std::string& aDefaultAddr,
                              uint16_t aDefaultPort,
                              const std::string& aDefaultRtcpAddr,
                              uint16_t aDefaultRtcpPort,
                              uint16_t aMLine);
  bool IsIceCtxReady() const {
    return mProxyResolveCompleted;
  }

  // The parent PC
  PeerConnectionImpl *mParent;
  // and a loose handle on it for event driven stuff
  std::string mParentHandle;
  std::string mParentName;

  // A list of streams returned from GetUserMedia
  // This is only accessed on the main thread (with one special exception)
  nsTArray<RefPtr<LocalSourceStreamInfo> > mLocalSourceStreams;

  // A list of streams provided by the other side
  // This is only accessed on the main thread (with one special exception)
  nsTArray<RefPtr<RemoteSourceStreamInfo> > mRemoteSourceStreams;

  std::map<size_t, std::pair<bool, RefPtr<MediaSessionConduit>>> mConduits;

  // ICE objects
  RefPtr<NrIceCtxHandler> mIceCtxHdlr;

  // DNS
  RefPtr<NrIceResolver> mDNSResolver;

  // Transport flows: even is RTP, odd is RTCP
  std::map<int, RefPtr<TransportFlow> > mTransportFlows;

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

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(PeerConnectionMedia)
};

} // namespace mozilla

#endif
