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
#if !defined(MOZILLA_XPCOMRT_API)
#include "nsIProtocolProxyCallback.h"
#endif

#ifdef USE_FAKE_MEDIA_STREAMS
#include "FakeMediaStreams.h"
#else
#include "DOMMediaStream.h"
#include "MediaSegment.h"
#endif

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

#include "nricectx.h"
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

  virtual void AddTrack(const std::string& trackId) { mTracks.insert(trackId); }
  void RemoveTrack(const std::string& trackId);
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
  const std::string& GetId() const { return mId; }

  void DetachTransport_s();
  void DetachMedia_m();
  bool AnyCodecHasPluginID(uint64_t aPluginID);
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  nsRefPtr<mozilla::dom::VideoStreamTrack> GetVideoTrackByTrackId(const std::string& trackId);
#endif
protected:
  nsRefPtr<DOMMediaStream> mMediaStream;
  PeerConnectionMedia *mParent;
  const std::string mId;
  // These get set up before we generate our local description, the pipelines
  // and conduits are set up once offer/answer completes.
  std::set<std::string> mTracks;
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
                            const std::string& newTrackId);

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  void UpdateSinkIdentity_m(nsIPrincipal* aPrincipal,
                            const PeerIdentity* aSinkIdentity);
#endif

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(LocalSourceStreamInfo)

private:
  already_AddRefed<MediaPipeline> ForgetPipelineByTrackId_m(
      const std::string& trackId);
};

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

  void SyncPipeline(RefPtr<MediaPipelineReceive> aPipeline);

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  void UpdatePrincipal_m(nsIPrincipal* aPrincipal);
#endif

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RemoteSourceStreamInfo)

  virtual void AddTrack(const std::string& track) override
  {
    mTrackIdMap.push_back(track);
    SourceStreamInfo::AddTrack(track);
  }

  TrackID GetNumericTrackId(const std::string& trackId) const
  {
    for (size_t i = 0; i < mTrackIdMap.size(); ++i) {
      if (mTrackIdMap[i] == trackId) {
        return static_cast<TrackID>(i + 1);
      }
    }
    return TRACK_INVALID;
  }

  nsresult GetTrackId(TrackID numericTrackId, std::string* trackId) const
  {
    if (numericTrackId <= 0 ||
        static_cast<size_t>(numericTrackId) > mTrackIdMap.size()) {
      return NS_ERROR_INVALID_ARG;;
    }

    *trackId = mTrackIdMap[numericTrackId - 1];
    return NS_OK;
  }

  void StartReceiving();

  /**
   * Returns true if a |MediaPipeline| should be queueing its track instead of
   * adding it to the |SourceMediaStream| directly.
   */
  bool ShouldQueueTracks() const
  {
    return !mReceiving;
  }

 private:
  // For remote streams, the MediaStreamGraph API forces us to select a
  // numeric track id before creation of the MediaStreamTrack, and does not
  // allow us to specify a string-based id until later. We cannot simply use
  // something based on mline index, since renegotiation can move tracks
  // around. Hopefully someday we'll be able to specify the string id up-front,
  // and have the numeric track id selected for us, in which case this variable
  // and its dependencies can go away.
  std::vector<std::string> mTrackIdMap;

  // True iff SetPullEnabled(true) has been called on the DOMMediaStream. This
  // happens when offer/answer concludes.
  bool mReceiving;
};

class PeerConnectionMedia : public sigslot::has_slots<> {
  ~PeerConnectionMedia() {}

 public:
  explicit PeerConnectionMedia(PeerConnectionImpl *parent);

  PeerConnectionImpl* GetPC() { return mParent; }
  nsresult Init(const std::vector<NrIceStunServer>& stun_servers,
                const std::vector<NrIceTurnServer>& turn_servers);
  // WARNING: This destroys the object!
  void SelfDestruct();

  RefPtr<NrIceCtx> ice_ctx() const { return mIceCtx; }

  RefPtr<NrIceMediaStream> ice_media_stream(size_t i) const {
    return mIceCtx->GetStream(i);
  }

  size_t num_ice_media_streams() const {
    return mIceCtx->GetStreamCount();
  }

  // Ensure ICE transports exist that we might need when offer/answer concludes
  void EnsureTransports(const JsepSession& aSession);

  // Activate or remove ICE transports at the conclusion of offer/answer,
  // or when rollback occurs.
  void ActivateOrRemoveTransports(const JsepSession& aSession);

  // Start ICE checks.
  void StartIceChecks(const JsepSession& session);

  // Process a trickle ICE candidate.
  void AddIceCandidate(const std::string& candidate, const std::string& mid,
                       uint32_t aMLine);

  // Handle complete media pipelines.
  nsresult UpdateMediaPipelines(const JsepSession& session);

  // Add a track (main thread only)
  nsresult AddTrack(DOMMediaStream* aMediaStream,
                    const std::string& streamId,
                    const std::string& trackId);

  nsresult RemoveLocalTrack(const std::string& streamId,
                            const std::string& trackId);
  nsresult RemoveRemoteTrack(const std::string& streamId,
                            const std::string& trackId);

  nsresult GetRemoteTrackId(const std::string streamId,
                            TrackID numericTrackId,
                            std::string* trackId) const;

  // Get a specific local stream
  uint32_t LocalStreamsLength()
  {
    return mLocalSourceStreams.Length();
  }
  LocalSourceStreamInfo* GetLocalStreamByIndex(int index);
  LocalSourceStreamInfo* GetLocalStreamById(const std::string& id);

  // Get a specific remote stream
  uint32_t RemoteStreamsLength()
  {
    return mRemoteSourceStreams.Length();
  }

  RemoteSourceStreamInfo* GetRemoteStreamByIndex(size_t index);
  RemoteSourceStreamInfo* GetRemoteStreamById(const std::string& id);

  // Add a remote stream.
  nsresult AddRemoteStream(nsRefPtr<RemoteSourceStreamInfo> aInfo);

  nsresult ReplaceTrack(const std::string& oldStreamId,
                        const std::string& oldTrackId,
                        DOMMediaStream* aNewStream,
                        const std::string& newStreamId,
                        const std::string& aNewTrack);

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  // In cases where the peer isn't yet identified, we disable the pipeline (not
  // the stream, that would potentially affect others), so that it sends
  // black/silence.  Once the peer is identified, re-enable those streams.
  void UpdateSinkIdentity_m(nsIPrincipal* aPrincipal,
                            const PeerIdentity* aSinkIdentity);
  // this determines if any stream is peerIdentity constrained
  bool AnyLocalStreamHasPeerIdentity() const;
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
      SignalEndOfLocalCandidates;

 private:
#if !defined(MOZILLA_XPCOMRT_API)
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
#endif // !defined(MOZILLA_XPCOMRT_API)

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
                        const std::string &candidate);
  void EndOfLocalCandidates(const std::string& aDefaultAddr,
                            uint16_t aDefaultPort,
                            const std::string& aDefaultRtcpAddr,
                            uint16_t aDefaultRtcpPort,
                            uint16_t aMLine);

  void IceGatheringStateChange_m(NrIceCtx* ctx,
                                 NrIceCtx::GatheringState state);
  void IceConnectionStateChange_m(NrIceCtx* ctx,
                                  NrIceCtx::ConnectionState state);
  void OnCandidateFound_m(const std::string &candidate, uint16_t aMLine);
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
  nsTArray<nsRefPtr<LocalSourceStreamInfo> > mLocalSourceStreams;

  // A list of streams provided by the other side
  // This is only accessed on the main thread (with one special exception)
  nsTArray<nsRefPtr<RemoteSourceStreamInfo> > mRemoteSourceStreams;

  std::map<size_t, std::pair<bool, RefPtr<MediaSessionConduit>>> mConduits;

  // ICE objects
  RefPtr<NrIceCtx> mIceCtx;

  // DNS
  nsRefPtr<NrIceResolver> mDNSResolver;

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

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(PeerConnectionMedia)
};

} // namespace mozilla

#endif
