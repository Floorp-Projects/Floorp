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

#ifdef USE_FAKE_MEDIA_STREAMS
#include "FakeMediaStreams.h"
#else
#include "DOMMediaStream.h"
#include "MediaSegment.h"
#endif

#include "signaling/src/jsep/JsepSession.h"
#include "AudioSegment.h"

#ifdef MOZILLA_INTERNAL_API
#include "Layers.h"
#include "VideoUtils.h"
#include "ImageLayers.h"
#include "VideoSegment.h"
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

  DOMMediaStream* GetMediaStream() const {
    return mMediaStream;
  }

  nsresult StorePipeline(
      const std::string& trackId,
      const mozilla::RefPtr<mozilla::MediaPipeline>& aPipeline);

  void AddTrack(const std::string& trackId) { mTracks.insert(trackId); }
  void RemoveTrack(const std::string& trackId) { mTracks.erase(trackId); }
  bool HasTrack(const std::string& trackId) const
  {
    return !!mTracks.count(trackId);
  }
  size_t GetTrackCount() const { return mTracks.size(); }

  // This method exists for stats and the unittests.
  // It allows visibility into the pipelines and flows.
  const std::map<std::string, mozilla::RefPtr<mozilla::MediaPipeline>>&
  GetPipelines() const { return mPipelines; }
  mozilla::RefPtr<mozilla::MediaPipeline> GetPipelineByTrackId_m(
      const std::string& trackId);
  const std::string& GetId() const { return mId; }

  void DetachTransport_s();
  void DetachMedia_m();
  bool AnyCodecHasPluginID(uint64_t aPluginID);
protected:
  nsRefPtr<DOMMediaStream> mMediaStream;
  PeerConnectionMedia *mParent;
  const std::string mId;
  // These get set up before we generate our local description, the pipelines
  // are set up once offer/answer completes.
  std::set<std::string> mTracks;
  // Indexed by track id, might contain pipelines for removed tracks
  std::map<std::string, mozilla::RefPtr<mozilla::MediaPipeline>> mPipelines;
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

  // XXX NOTE: does not change mMediaStream, even if it replaces the last track
  // in a LocalSourceStreamInfo.  Revise when we have support for multiple tracks
  // of a type.
  nsresult ReplaceTrack(const std::string& oldTrackId,
                        DOMMediaStream* aNewStream,
                        const std::string& aNewTrack);

#ifdef MOZILLA_INTERNAL_API
  void UpdateSinkIdentity_m(nsIPrincipal* aPrincipal,
                            const mozilla::PeerIdentity* aSinkIdentity);
#endif

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(LocalSourceStreamInfo)
};

class RemoteSourceStreamInfo : public SourceStreamInfo {
  ~RemoteSourceStreamInfo() {}
 public:
  RemoteSourceStreamInfo(already_AddRefed<DOMMediaStream> aMediaStream,
                         PeerConnectionMedia *aParent,
                         const std::string& aId)
    : SourceStreamInfo(aMediaStream, aParent, aId)
  {
  }

  void SyncPipeline(RefPtr<MediaPipelineReceive> aPipeline);

#ifdef MOZILLA_INTERNAL_API
  void UpdatePrincipal_m(nsIPrincipal* aPrincipal);
#endif

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RemoteSourceStreamInfo)
};

class PeerConnectionMedia : public sigslot::has_slots<> {
  ~PeerConnectionMedia() {}

 public:
  explicit PeerConnectionMedia(PeerConnectionImpl *parent);

  PeerConnectionImpl* GetPC() { return mParent; }
  nsresult Init(const std::vector<mozilla::NrIceStunServer>& stun_servers,
                const std::vector<mozilla::NrIceTurnServer>& turn_servers);
  // WARNING: This destroys the object!
  void SelfDestruct();

  // Configure the ability to use localhost.
  void SetAllowIceLoopback(bool val) { mAllowIceLoopback = val; }

  mozilla::RefPtr<mozilla::NrIceCtx> ice_ctx() const { return mIceCtx; }

  mozilla::RefPtr<mozilla::NrIceMediaStream> ice_media_stream(size_t i) const {
    // TODO(ekr@rtfm.com): If someone asks for a value that doesn't exist,
    // make one.
    if (i >= mIceStreams.size()) {
      return nullptr;
    }
    return mIceStreams[i];
  }

  size_t num_ice_media_streams() const {
    return mIceStreams.size();
  }

  // Create and modify transports in response to negotiation events.
  void UpdateTransports(const mozilla::JsepSession& session);

  // Start ICE checks.
  void StartIceChecks(const mozilla::JsepSession& session);

  // Process a trickle ICE candidate.
  void AddIceCandidate(const std::string& candidate, const std::string& mid,
                       uint32_t aMLine);

  // Handle complete media pipelines.
  nsresult UpdateMediaPipelines(const mozilla::JsepSession& session);

  // Add a track (main thread only)
  // TODO(bug 1089798): Once DOMMediaStream has an id field, use it instead of
  // letting PCMedia choose a streamId
  nsresult AddTrack(DOMMediaStream* aMediaStream,
                    std::string* streamId,
                    const std::string& trackId);

  // Remove a track (main thread only)
  // TODO(bug 1089798): Once DOMMediaStream has an id field, use it instead of
  // passing |aMediaStream|
  nsresult RemoveTrack(DOMMediaStream* aMediaStream,
                       const std::string& trackId);

  // Get a specific local stream
  uint32_t LocalStreamsLength()
  {
    return mLocalSourceStreams.Length();
  }
  LocalSourceStreamInfo* GetLocalStreamByIndex(int index);
  LocalSourceStreamInfo* GetLocalStreamById(const std::string& id);
  LocalSourceStreamInfo* GetLocalStreamByDomStream(
      const DOMMediaStream& stream);

  // Get a specific remote stream
  uint32_t RemoteStreamsLength()
  {
    return mRemoteSourceStreams.Length();
  }

  RemoteSourceStreamInfo* GetRemoteStreamByIndex(size_t index);
  RemoteSourceStreamInfo* GetRemoteStreamById(const std::string& id);
  RemoteSourceStreamInfo* GetRemoteStreamByDomStream(
      const DOMMediaStream& stream);


  bool UpdateFilterFromRemoteDescription_m(
      const std::string& trackId,
      nsAutoPtr<mozilla::MediaPipelineFilter> filter);

  // Add a remote stream.
  nsresult AddRemoteStream(nsRefPtr<RemoteSourceStreamInfo> aInfo);

#ifdef MOZILLA_INTERNAL_API
  // In cases where the peer isn't yet identified, we disable the pipeline (not
  // the stream, that would potentially affect others), so that it sends
  // black/silence.  Once the peer is identified, re-enable those streams.
  void UpdateSinkIdentity_m(nsIPrincipal* aPrincipal,
                            const mozilla::PeerIdentity* aSinkIdentity);
  // this determines if any stream is peerIdentity constrained
  bool AnyLocalStreamHasPeerIdentity() const;
  // When we finally learn who is on the other end, we need to change the ownership
  // on streams
  void UpdateRemoteStreamPrincipals_m(nsIPrincipal* aPrincipal);
#endif

  bool AnyCodecHasPluginID(uint64_t aPluginID);

  const nsCOMPtr<nsIThread>& GetMainThread() const { return mMainThread; }
  const nsCOMPtr<nsIEventTarget>& GetSTSThread() const { return mSTSThread; }

  // Get a transport flow either RTP/RTCP for a particular stream
  // A stream can be of audio/video/datachannel/budled(?) types
  mozilla::RefPtr<mozilla::TransportFlow> GetTransportFlow(int aStreamIndex,
                                                           bool aIsRtcp) {
    int index_inner = aStreamIndex * 2 + (aIsRtcp ? 1 : 0);

    if (mTransportFlows.find(index_inner) == mTransportFlows.end())
      return nullptr;

    return mTransportFlows[index_inner];
  }

  // Add a transport flow
  void AddTransportFlow(int aIndex, bool aRtcp,
                        const mozilla::RefPtr<mozilla::TransportFlow> &aFlow);
  void ConnectDtlsListener_s(const mozilla::RefPtr<mozilla::TransportFlow>& aFlow);
  void DtlsConnected_s(mozilla::TransportLayer* aFlow,
                       mozilla::TransportLayer::State state);
  static void DtlsConnected_m(const std::string& aParentHandle,
                              bool aPrivacyRequested);

  mozilla::RefPtr<mozilla::MediaSessionConduit> GetConduit(int aStreamIndex, bool aReceive) {
    int index_inner = aStreamIndex * 2 + (aReceive ? 0 : 1);

    if (mConduits.find(index_inner) == mConduits.end())
      return nullptr;

    return mConduits[index_inner];
  }

  // Add a conduit
  void AddConduit(int aIndex, bool aReceive,
                  const mozilla::RefPtr<mozilla::MediaSessionConduit> &aConduit) {
    int index_inner = aIndex * 2 + (aReceive ? 0 : 1);

    MOZ_ASSERT(!mConduits[index_inner]);
    mConduits[index_inner] = aConduit;
  }

  // ICE state signals
  sigslot::signal2<mozilla::NrIceCtx*, mozilla::NrIceCtx::GatheringState>
      SignalIceGatheringStateChange;
  sigslot::signal2<mozilla::NrIceCtx*, mozilla::NrIceCtx::ConnectionState>
      SignalIceConnectionStateChange;
  // This passes a candidate:... attribute  and level
  sigslot::signal2<const std::string&, uint16_t> SignalCandidate;
  // This passes address, port, level of the default candidate.
  sigslot::signal3<const std::string&, uint16_t, uint16_t>
      SignalEndOfLocalCandidates;

 private:
  class ProtocolProxyQueryHandler : public nsIProtocolProxyCallback {
   public:
    explicit ProtocolProxyQueryHandler(PeerConnectionMedia *pcm) :
      pcm_(pcm) {}

    NS_IMETHODIMP OnProxyAvailable(nsICancelable *request,
                                   nsIChannel *aChannel,
                                   nsIProxyInfo *proxyinfo,
                                   nsresult result) MOZ_OVERRIDE;
    NS_DECL_ISUPPORTS

   private:
      RefPtr<PeerConnectionMedia> pcm_;
      virtual ~ProtocolProxyQueryHandler() {}
  };

  // Shutdown media transport. Must be called on STS thread.
  void ShutdownMediaTransport_s();

  // Final destruction of the media stream. Must be called on the main
  // thread.
  void SelfDestruct_m();

  // Manage ICE transports.
  void UpdateIceMediaStream_s(size_t aMLine, size_t aComponentCount,
                              bool aHasAttrs,
                              const std::string& aUfrag,
                              const std::string& aPassword,
                              const std::vector<std::string>& aCandidateList);
  void GatherIfReady();
  void FlushIceCtxOperationQueueIfReady();
  void PerformOrEnqueueIceCtxOperation(const nsRefPtr<nsIRunnable>& runnable);
  void EnsureIceGathering_s();
  void StartIceChecks_s(bool aIsControlling,
                        bool aIsIceLite,
                        const std::vector<std::string>& aIceOptionsList,
                        const std::vector<size_t>& aComponentCountByLevel);

  // Process a trickle ICE candidate.
  void AddIceCandidate_s(const std::string& aCandidate, const std::string& aMid,
                         uint32_t aMLine);


  // ICE events
  void IceGatheringStateChange_s(mozilla::NrIceCtx* ctx,
                               mozilla::NrIceCtx::GatheringState state);
  void IceConnectionStateChange_s(mozilla::NrIceCtx* ctx,
                                mozilla::NrIceCtx::ConnectionState state);
  void IceStreamReady_s(mozilla::NrIceMediaStream *aStream);
  void OnCandidateFound_s(mozilla::NrIceMediaStream *aStream,
                        const std::string &candidate);
  void EndOfLocalCandidates(const std::string& aDefaultAddr,
                            uint16_t aDefaultPort,
                            uint16_t aMLine);

  void IceGatheringStateChange_m(mozilla::NrIceCtx* ctx,
                                 mozilla::NrIceCtx::GatheringState state);
  void IceConnectionStateChange_m(mozilla::NrIceCtx* ctx,
                                  mozilla::NrIceCtx::ConnectionState state);
  void OnCandidateFound_m(const std::string &candidate, uint16_t aMLine);
  void EndOfLocalCandidates_m(const std::string& aDefaultAddr,
                              uint16_t aDefaultPort,
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

  // Allow loopback for ICE.
  bool mAllowIceLoopback;

  // ICE objects
  mozilla::RefPtr<mozilla::NrIceCtx> mIceCtx;
  std::vector<mozilla::RefPtr<mozilla::NrIceMediaStream> > mIceStreams;

  // DNS
  nsRefPtr<mozilla::NrIceResolver> mDNSResolver;

  // Transport flows: even is RTP, odd is RTCP
  std::map<int, mozilla::RefPtr<mozilla::TransportFlow> > mTransportFlows;

  // Conduits: even is receive, odd is transmit (for easier correlation with
  // flows)
  std::map<int, mozilla::RefPtr<mozilla::MediaSessionConduit> > mConduits;

  // UUID Generator
  mozilla::UniquePtr<PCUuidGenerator> mUuidGen;

  // The main thread.
  nsCOMPtr<nsIThread> mMainThread;

  // The STS thread.
  nsCOMPtr<nsIEventTarget> mSTSThread;

  // Used whenever we need to dispatch a runnable to STS to tweak something
  // on our ICE ctx, but are not ready to do so at the moment (eg; we are
  // waiting to get a callback with our http proxy config before we start
  // gathering or start checking)
  std::vector<nsRefPtr<nsIRunnable>> mQueuedIceCtxOperations;

  // Used to cancel any ongoing proxy request.
  nsCOMPtr<nsICancelable> mProxyRequest;

  // Used to track the state of the request.
  bool mProxyResolveCompleted;

  // Used to store the result of the request.
  UniquePtr<NrIceProxyServer> mProxyServer;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(PeerConnectionMedia)
};

}  // namespace mozilla
#endif
