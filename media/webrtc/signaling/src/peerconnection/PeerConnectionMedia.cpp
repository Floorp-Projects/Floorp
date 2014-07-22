/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <string>

#include "CSFLog.h"

#include "nspr.h"
#include "cc_constants.h"

#include "nricectx.h"
#include "nricemediastream.h"
#include "PeerConnectionImpl.h"
#include "PeerConnectionMedia.h"
#include "AudioConduit.h"
#include "VideoConduit.h"
#include "runnable_utils.h"
#include "transportlayerdtls.h"

#ifdef MOZILLA_INTERNAL_API
#include "MediaStreamList.h"
#include "nsIScriptGlobalObject.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/RTCStatsReportBinding.h"
#endif

using namespace mozilla;
using namespace mozilla::dom;

namespace sipcc {

static const char* logTag = "PeerConnectionMedia";
static const mozilla::TrackID TRACK_AUDIO = 0;
static const mozilla::TrackID TRACK_VIDEO = 1;

/* If the ExpectAudio hint is on we will add a track at the default first
 * audio track ID (0)
 * FIX - Do we need to iterate over the tracks instead of taking these hints?
 */
void
LocalSourceStreamInfo::ExpectAudio(const mozilla::TrackID aID)
{
  mAudioTracks.AppendElement(aID);
}

// If the ExpectVideo hint is on we will add a track at the default first
// video track ID (1).
void
LocalSourceStreamInfo::ExpectVideo(const mozilla::TrackID aID)
{
  mVideoTracks.AppendElement(aID);
}

unsigned
LocalSourceStreamInfo::AudioTrackCount()
{
  return mAudioTracks.Length();
}

unsigned
LocalSourceStreamInfo::VideoTrackCount()
{
  return mVideoTracks.Length();
}

void LocalSourceStreamInfo::DetachTransport_s()
{
  ASSERT_ON_THREAD(mParent->GetSTSThread());
  // walk through all the MediaPipelines and call the shutdown
  // functions for transport. Must be on the STS thread.
  for (std::map<int, mozilla::RefPtr<mozilla::MediaPipeline> >::iterator it =
           mPipelines.begin(); it != mPipelines.end();
       ++it) {
    it->second->ShutdownTransport_s();
  }
}

void LocalSourceStreamInfo::DetachMedia_m()
{
  ASSERT_ON_THREAD(mParent->GetMainThread());
  // walk through all the MediaPipelines and call the shutdown
  // functions. Must be on the main thread.
  for (std::map<int, mozilla::RefPtr<mozilla::MediaPipeline> >::iterator it =
           mPipelines.begin(); it != mPipelines.end();
       ++it) {
    it->second->ShutdownMedia_m();
  }
  mAudioTracks.Clear();
  mVideoTracks.Clear();
  mMediaStream = nullptr;
}

void RemoteSourceStreamInfo::DetachTransport_s()
{
  ASSERT_ON_THREAD(mParent->GetSTSThread());
  // walk through all the MediaPipelines and call the shutdown
  // transport functions. Must be on the STS thread.
  for (std::map<int, mozilla::RefPtr<mozilla::MediaPipeline> >::iterator it =
           mPipelines.begin(); it != mPipelines.end();
       ++it) {
    it->second->ShutdownTransport_s();
  }
}

void RemoteSourceStreamInfo::DetachMedia_m()
{
  ASSERT_ON_THREAD(mParent->GetMainThread());

  // walk through all the MediaPipelines and call the shutdown
  // media functions. Must be on the main thread.
  for (std::map<int, mozilla::RefPtr<mozilla::MediaPipeline> >::iterator it =
           mPipelines.begin(); it != mPipelines.end();
       ++it) {
    it->second->ShutdownMedia_m();
  }
  mMediaStream = nullptr;
}

already_AddRefed<PeerConnectionImpl>
PeerConnectionImpl::Constructor(const dom::GlobalObject& aGlobal, ErrorResult& rv)
{
  nsRefPtr<PeerConnectionImpl> pc = new PeerConnectionImpl(&aGlobal);

  CSFLogDebug(logTag, "Created PeerConnection: %p", pc.get());

  return pc.forget();
}

PeerConnectionImpl* PeerConnectionImpl::CreatePeerConnection()
{
  PeerConnectionImpl *pc = new PeerConnectionImpl();

  CSFLogDebug(logTag, "Created PeerConnection: %p", pc);

  return pc;
}


PeerConnectionMedia::PeerConnectionMedia(PeerConnectionImpl *parent)
    : mParent(parent),
      mIceCtx(nullptr),
      mDNSResolver(new mozilla::NrIceResolver()),
      mMainThread(mParent->GetMainThread()),
      mSTSThread(mParent->GetSTSThread()) {}

nsresult PeerConnectionMedia::Init(const std::vector<NrIceStunServer>& stun_servers,
                                   const std::vector<NrIceTurnServer>& turn_servers)
{
  // TODO(ekr@rtfm.com): need some way to set not offerer later
  // Looks like a bug in the NrIceCtx API.
  mIceCtx = NrIceCtx::Create("PC:" + mParent->GetName(), true);
  if(!mIceCtx) {
    CSFLogError(logTag, "%s: Failed to create Ice Context", __FUNCTION__);
    return NS_ERROR_FAILURE;
  }
  nsresult rv;
  if (NS_FAILED(rv = mIceCtx->SetStunServers(stun_servers))) {
    CSFLogError(logTag, "%s: Failed to set stun servers", __FUNCTION__);
    return rv;
  }
  // Give us a way to globally turn off TURN support
#ifdef MOZILLA_INTERNAL_API
  bool disabled = Preferences::GetBool("media.peerconnection.turn.disable", false);
#else
  bool disabled = false;
#endif
  if (!disabled) {
    if (NS_FAILED(rv = mIceCtx->SetTurnServers(turn_servers))) {
      CSFLogError(logTag, "%s: Failed to set turn servers", __FUNCTION__);
      return rv;
    }
  } else if (turn_servers.size() != 0) {
    CSFLogError(logTag, "%s: Setting turn servers disabled", __FUNCTION__);
  }
  if (NS_FAILED(rv = mDNSResolver->Init())) {
    CSFLogError(logTag, "%s: Failed to initialize dns resolver", __FUNCTION__);
    return rv;
  }
  if (NS_FAILED(rv = mIceCtx->SetResolver(mDNSResolver->AllocateResolver()))) {
    CSFLogError(logTag, "%s: Failed to get dns resolver", __FUNCTION__);
    return rv;
  }
  mIceCtx->SignalGatheringStateChange.connect(
      this,
      &PeerConnectionMedia::IceGatheringStateChange);
  mIceCtx->SignalConnectionStateChange.connect(
      this,
      &PeerConnectionMedia::IceConnectionStateChange);

  // Create three streams to start with.
  // One each for audio, video and DataChannel
  // TODO: this will be re-visited
  RefPtr<NrIceMediaStream> audioStream =
    mIceCtx->CreateStream((mParent->GetName()+": stream1/audio").c_str(), 2);
  RefPtr<NrIceMediaStream> videoStream =
    mIceCtx->CreateStream((mParent->GetName()+": stream2/video").c_str(), 2);
  RefPtr<NrIceMediaStream> dcStream =
    mIceCtx->CreateStream((mParent->GetName()+": stream3/data").c_str(), 2);

  if (!audioStream) {
    CSFLogError(logTag, "%s: audio stream is NULL", __FUNCTION__);
    return NS_ERROR_FAILURE;
  } else {
    mIceStreams.push_back(audioStream);
  }

  if (!videoStream) {
    CSFLogError(logTag, "%s: video stream is NULL", __FUNCTION__);
    return NS_ERROR_FAILURE;
  } else {
    mIceStreams.push_back(videoStream);
  }

  if (!dcStream) {
    CSFLogError(logTag, "%s: datachannel stream is NULL", __FUNCTION__);
    return NS_ERROR_FAILURE;
  } else {
    mIceStreams.push_back(dcStream);
  }

  // TODO(ekr@rtfm.com): This is not connected to the PCCimpl.
  // Will need to do that later.
  for (std::size_t i=0; i<mIceStreams.size(); i++) {
    mIceStreams[i]->SignalReady.connect(this, &PeerConnectionMedia::IceStreamReady);
  }

  // TODO(ekr@rtfm.com): When we have a generic error reporting mechanism,
  // figure out how to report that StartGathering failed. Bug 827982.
  RUN_ON_THREAD(mIceCtx->thread(),
                WrapRunnable(mIceCtx, &NrIceCtx::StartGathering), NS_DISPATCH_NORMAL);

  return NS_OK;
}

nsresult
PeerConnectionMedia::AddStream(nsIDOMMediaStream* aMediaStream, uint32_t *stream_id)
{
  ASSERT_ON_THREAD(mMainThread);

  if (!aMediaStream) {
    CSFLogError(logTag, "%s - aMediaStream is NULL", __FUNCTION__);
    return NS_ERROR_FAILURE;
  }

  DOMMediaStream* stream = static_cast<DOMMediaStream*>(aMediaStream);

  CSFLogDebug(logTag, "%s: MediaStream: %p",
    __FUNCTION__, aMediaStream);

  // Adding tracks here based on nsDOMMediaStream expectation settings
  uint32_t hints = stream->GetHintContents();
#ifdef MOZILLA_INTERNAL_API
  if (!Preferences::GetBool("media.peerconnection.video.enabled", true)) {
    hints &= ~(DOMMediaStream::HINT_CONTENTS_VIDEO);
  }
#endif

  if (!(hints & (DOMMediaStream::HINT_CONTENTS_AUDIO |
        DOMMediaStream::HINT_CONTENTS_VIDEO))) {
    CSFLogDebug(logTag, "Empty Stream !!");
    return NS_OK;
  }

  // Now see if we already have a stream of this type, since we only
  // allow one of each.
  // TODO(ekr@rtfm.com): remove this when multiple of each stream
  // is allowed
  for (uint32_t u = 0; u < mLocalSourceStreams.Length(); u++) {
    nsRefPtr<LocalSourceStreamInfo> localSourceStream = mLocalSourceStreams[u];

    if (localSourceStream->GetMediaStream()->GetHintContents() & hints) {
      CSFLogError(logTag, "Only one stream of any given type allowed");
      return NS_ERROR_FAILURE;
    }
  }

  // OK, we're good to add
  nsRefPtr<LocalSourceStreamInfo> localSourceStream =
      new LocalSourceStreamInfo(stream, this);
  *stream_id = mLocalSourceStreams.Length();

  if (hints & DOMMediaStream::HINT_CONTENTS_AUDIO) {
    localSourceStream->ExpectAudio(TRACK_AUDIO);
  }

  if (hints & DOMMediaStream::HINT_CONTENTS_VIDEO) {
    localSourceStream->ExpectVideo(TRACK_VIDEO);
  }

  mLocalSourceStreams.AppendElement(localSourceStream);

  return NS_OK;
}

nsresult
PeerConnectionMedia::RemoveStream(nsIDOMMediaStream* aMediaStream, uint32_t *stream_id)
{
  MOZ_ASSERT(aMediaStream);
  ASSERT_ON_THREAD(mMainThread);

  DOMMediaStream* stream = static_cast<DOMMediaStream*>(aMediaStream);

  CSFLogDebug(logTag, "%s: MediaStream: %p",
    __FUNCTION__, aMediaStream);

  for (uint32_t u = 0; u < mLocalSourceStreams.Length(); u++) {
    nsRefPtr<LocalSourceStreamInfo> localSourceStream = mLocalSourceStreams[u];
    if (localSourceStream->GetMediaStream() == stream) {
      *stream_id = u;
      return NS_OK;
    }
  }

  return NS_ERROR_ILLEGAL_VALUE;
}

void
PeerConnectionMedia::SelfDestruct()
{
  ASSERT_ON_THREAD(mMainThread);

  CSFLogDebug(logTag, "%s: ", __FUNCTION__);

  // Shut down the media
  for (uint32_t i=0; i < mLocalSourceStreams.Length(); ++i) {
    mLocalSourceStreams[i]->DetachMedia_m();
  }

  for (uint32_t i=0; i < mRemoteSourceStreams.Length(); ++i) {
    mRemoteSourceStreams[i]->DetachMedia_m();
  }

  // Shutdown the transport (async)
  RUN_ON_THREAD(mSTSThread, WrapRunnable(
      this, &PeerConnectionMedia::ShutdownMediaTransport_s),
                NS_DISPATCH_NORMAL);

  CSFLogDebug(logTag, "%s: Media shut down", __FUNCTION__);
}

void
PeerConnectionMedia::SelfDestruct_m()
{
  CSFLogDebug(logTag, "%s: ", __FUNCTION__);

  ASSERT_ON_THREAD(mMainThread);
  mLocalSourceStreams.Clear();
  mRemoteSourceStreams.Clear();

  // Final self-destruct.
  this->Release();
}

void
PeerConnectionMedia::ShutdownMediaTransport_s()
{
  ASSERT_ON_THREAD(mSTSThread);

  CSFLogDebug(logTag, "%s: ", __FUNCTION__);

  // Here we access m{Local|Remote}SourceStreams off the main thread.
  // That's OK because by here PeerConnectionImpl has forgotten about us,
  // so there is no chance of getting a call in here from outside.
  // The dispatches from SelfDestruct() and to SelfDestruct_m() provide
  // memory barriers that protect us from badness.
  for (uint32_t i=0; i < mLocalSourceStreams.Length(); ++i) {
    mLocalSourceStreams[i]->DetachTransport_s();
  }

  for (uint32_t i=0; i < mRemoteSourceStreams.Length(); ++i) {
    mRemoteSourceStreams[i]->DetachTransport_s();
  }

  disconnect_all();
  mTransportFlows.clear();
  mIceStreams.clear();
  mIceCtx = nullptr;

  mMainThread->Dispatch(WrapRunnable(this, &PeerConnectionMedia::SelfDestruct_m),
                        NS_DISPATCH_NORMAL);
}

LocalSourceStreamInfo*
PeerConnectionMedia::GetLocalStream(int aIndex)
{
  ASSERT_ON_THREAD(mMainThread);
  if(aIndex < 0 || aIndex >= (int) mLocalSourceStreams.Length()) {
    return nullptr;
  }

  MOZ_ASSERT(mLocalSourceStreams[aIndex]);
  return mLocalSourceStreams[aIndex];
}

RemoteSourceStreamInfo*
PeerConnectionMedia::GetRemoteStream(int aIndex)
{
  ASSERT_ON_THREAD(mMainThread);
  if(aIndex < 0 || aIndex >= (int) mRemoteSourceStreams.Length()) {
    return nullptr;
  }

  MOZ_ASSERT(mRemoteSourceStreams[aIndex]);
  return mRemoteSourceStreams[aIndex];
}

bool
PeerConnectionMedia::SetUsingBundle_m(int level, bool decision)
{
  ASSERT_ON_THREAD(mMainThread);
  for (size_t i = 0; i < mRemoteSourceStreams.Length(); ++i) {
    if (mRemoteSourceStreams[i]->SetUsingBundle_m(level, decision)) {
      // Found the MediaPipeline for |level|
      return true;
    }
  }
  CSFLogWarn(logTag, "Could not locate level %d to set bundle flag to %s",
                     static_cast<int>(level),
                     decision ? "true" : "false");
  return false;
}

static void
UpdateFilterFromRemoteDescription_s(
  RefPtr<mozilla::MediaPipeline> receive,
  RefPtr<mozilla::MediaPipeline> transmit,
  nsAutoPtr<mozilla::MediaPipelineFilter> filter) {

  // Update filter, and make a copy of the final version.
  mozilla::MediaPipelineFilter *finalFilter(
    receive->UpdateFilterFromRemoteDescription_s(filter));

  if (finalFilter) {
    filter = new mozilla::MediaPipelineFilter(*finalFilter);
  }

  // Set same filter on transmit pipeline too.
  transmit->UpdateFilterFromRemoteDescription_s(filter);
}

bool
PeerConnectionMedia::UpdateFilterFromRemoteDescription_m(
    int level,
    nsAutoPtr<mozilla::MediaPipelineFilter> filter)
{
  ASSERT_ON_THREAD(mMainThread);

  RefPtr<mozilla::MediaPipeline> receive;
  for (size_t i = 0; !receive && i < mRemoteSourceStreams.Length(); ++i) {
    receive = mRemoteSourceStreams[i]->GetPipelineByLevel_m(level);
  }

  RefPtr<mozilla::MediaPipeline> transmit;
  for (size_t i = 0; !transmit && i < mLocalSourceStreams.Length(); ++i) {
    transmit = mLocalSourceStreams[i]->GetPipelineByLevel_m(level);
  }

  if (receive && transmit) {
    // GetPipelineByLevel_m will return nullptr if shutdown is in progress;
    // since shutdown is initiated in main, and involves a dispatch to STS
    // before the pipelines are released, our dispatch to STS will complete
    // before any release can happen due to a shutdown that hasn't started yet.
    RUN_ON_THREAD(GetSTSThread(),
                  WrapRunnableNM(
                      &UpdateFilterFromRemoteDescription_s,
                      receive,
                      transmit,
                      filter
                  ),
                  NS_DISPATCH_NORMAL);
    return true;
  } else {
    CSFLogWarn(logTag, "Could not locate level %d to update filter",
        static_cast<int>(level));
  }
  return false;
}

nsresult
PeerConnectionMedia::AddRemoteStream(nsRefPtr<RemoteSourceStreamInfo> aInfo,
  int *aIndex)
{
  ASSERT_ON_THREAD(mMainThread);
  MOZ_ASSERT(aIndex);

  *aIndex = mRemoteSourceStreams.Length();

  mRemoteSourceStreams.AppendElement(aInfo);

  return NS_OK;
}

nsresult
PeerConnectionMedia::AddRemoteStreamHint(int aIndex, bool aIsVideo)
{
  if (aIndex < 0 ||
      static_cast<unsigned int>(aIndex) >= mRemoteSourceStreams.Length()) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  RemoteSourceStreamInfo *pInfo = mRemoteSourceStreams.ElementAt(aIndex);
  MOZ_ASSERT(pInfo);

  if (aIsVideo) {
    pInfo->mTrackTypeHints |= DOMMediaStream::HINT_CONTENTS_VIDEO;
  } else {
    pInfo->mTrackTypeHints |= DOMMediaStream::HINT_CONTENTS_AUDIO;
  }

  return NS_OK;
}


void
PeerConnectionMedia::IceGatheringStateChange(NrIceCtx* ctx,
                                             NrIceCtx::GatheringState state)
{
  SignalIceGatheringStateChange(ctx, state);
}

void
PeerConnectionMedia::IceConnectionStateChange(NrIceCtx* ctx,
                                              NrIceCtx::ConnectionState state)
{
  SignalIceConnectionStateChange(ctx, state);
}

void
PeerConnectionMedia::IceStreamReady(NrIceMediaStream *aStream)
{
  MOZ_ASSERT(aStream);

  CSFLogDebug(logTag, "%s: %s", __FUNCTION__, aStream->name().c_str());
}


void
PeerConnectionMedia::DtlsConnected(TransportLayer *dtlsLayer,
                                   TransportLayer::State state)
{
  dtlsLayer->SignalStateChange.disconnect(this);

  bool privacyRequested = false;
  // TODO (Bug 952678) set privacy mode, ask the DTLS layer about that
  GetMainThread()->Dispatch(
    WrapRunnable(mParent, &PeerConnectionImpl::SetDtlsConnected, privacyRequested),
    NS_DISPATCH_NORMAL);
}

void
PeerConnectionMedia::AddTransportFlow(int aIndex, bool aRtcp,
                                      const RefPtr<TransportFlow> &aFlow)
{
  int index_inner = aIndex * 2 + (aRtcp ? 1 : 0);

  MOZ_ASSERT(!mTransportFlows[index_inner]);
  mTransportFlows[index_inner] = aFlow;

  GetSTSThread()->Dispatch(
    WrapRunnable(this, &PeerConnectionMedia::ConnectDtlsListener_s, aFlow),
    NS_DISPATCH_NORMAL);
}

void
PeerConnectionMedia::ConnectDtlsListener_s(const RefPtr<TransportFlow>& aFlow)
{
  TransportLayer* dtls = aFlow->GetLayer(TransportLayerDtls::ID());
  if (dtls) {
    dtls->SignalStateChange.connect(this, &PeerConnectionMedia::DtlsConnected);
  }
}

#ifdef MOZILLA_INTERNAL_API
/**
 * Tells you if any local streams is isolated to a specific peer identity.
 * Obviously, we want all the streams to be isolated equally so that they can
 * all be sent or not.  We check once when we are setting a local description
 * and that determines if we flip the "privacy requested" bit on.  Once the bit
 * is on, all media originating from this peer connection is isolated.
 *
 * @returns true if any stream has a peerIdentity set on it
 */
bool
PeerConnectionMedia::AnyLocalStreamHasPeerIdentity() const
{
  ASSERT_ON_THREAD(mMainThread);

  for (uint32_t u = 0; u < mLocalSourceStreams.Length(); u++) {
    // check if we should be asking for a private call for this stream
    DOMMediaStream* stream = mLocalSourceStreams[u]->GetMediaStream();
    if (stream->GetPeerIdentity()) {
      return true;
    }
  }
  return false;
}

void
PeerConnectionMedia::UpdateRemoteStreamPrincipals_m(nsIPrincipal* aPrincipal)
{
  ASSERT_ON_THREAD(mMainThread);

  for (uint32_t u = 0; u < mRemoteSourceStreams.Length(); u++) {
    mRemoteSourceStreams[u]->UpdatePrincipal_m(aPrincipal);
  }
}

void
PeerConnectionMedia::UpdateSinkIdentity_m(nsIPrincipal* aPrincipal,
                                          const PeerIdentity* aSinkIdentity)
{
  ASSERT_ON_THREAD(mMainThread);

  for (uint32_t u = 0; u < mLocalSourceStreams.Length(); u++) {
    mLocalSourceStreams[u]->UpdateSinkIdentity_m(aPrincipal, aSinkIdentity);
  }
}

void
LocalSourceStreamInfo::UpdateSinkIdentity_m(nsIPrincipal* aPrincipal,
                                            const PeerIdentity* aSinkIdentity)
{
  for (auto it = mPipelines.begin(); it != mPipelines.end(); ++it) {
    MediaPipelineTransmit* pipeline =
      static_cast<MediaPipelineTransmit*>((*it).second.get());
    pipeline->UpdateSinkIdentity_m(aPrincipal, aSinkIdentity);
  }
}

void RemoteSourceStreamInfo::UpdatePrincipal_m(nsIPrincipal* aPrincipal)
{
  // this blasts away the existing principal
  // we only do this when we become certain that the stream is safe to make
  // accessible to the script principal
  mMediaStream->SetPrincipal(aPrincipal);
}
#endif // MOZILLA_INTERNAL_API

bool
PeerConnectionMedia::AnyCodecHasPluginID(uint64_t aPluginID)
{
  for (uint32_t i=0; i < mLocalSourceStreams.Length(); ++i) {
    if (mLocalSourceStreams[i]->AnyCodecHasPluginID(aPluginID)) {
      return true;
    }
  }
  for (uint32_t i=0; i < mRemoteSourceStreams.Length(); ++i) {
    if (mRemoteSourceStreams[i]->AnyCodecHasPluginID(aPluginID)) {
      return true;
    }
  }
  return false;
}

bool
LocalSourceStreamInfo::AnyCodecHasPluginID(uint64_t aPluginID)
{
  // Scan the videoConduits for this plugin ID
  for (auto it = mPipelines.begin(); it != mPipelines.end(); ++it) {
    if (it->second->Conduit()->CodecPluginID() == aPluginID) {
      return true;
    }
  }
  return false;
}

bool
RemoteSourceStreamInfo::AnyCodecHasPluginID(uint64_t aPluginID)
{
  // Scan the videoConduits for this plugin ID
  for (auto it = mPipelines.begin(); it != mPipelines.end(); ++it) {
    if (it->second->Conduit()->CodecPluginID() == aPluginID) {
      return true;
    }
  }
  return false;
}

void
LocalSourceStreamInfo::StorePipeline(
  int aTrack, mozilla::RefPtr<mozilla::MediaPipelineTransmit> aPipeline)
{
  MOZ_ASSERT(mPipelines.find(aTrack) == mPipelines.end());
  if (mPipelines.find(aTrack) != mPipelines.end()) {
    CSFLogError(logTag, "%s: Storing duplicate track", __FUNCTION__);
    return;
  }
  //TODO: Revisit once we start supporting multiple streams or multiple tracks
  // of same type
  mPipelines[aTrack] = aPipeline;
}

void
RemoteSourceStreamInfo::StorePipeline(
  int aTrack, bool aIsVideo,
  mozilla::RefPtr<mozilla::MediaPipelineReceive> aPipeline)
{
  MOZ_ASSERT(mPipelines.find(aTrack) == mPipelines.end());
  if (mPipelines.find(aTrack) != mPipelines.end()) {
    CSFLogError(logTag, "%s: Request to store duplicate track %d", __FUNCTION__, aTrack);
    return;
  }
  CSFLogDebug(logTag, "%s track %d %s = %p", __FUNCTION__, aTrack, aIsVideo ? "video" : "audio",
              aPipeline.get());
  // See if we have both audio and video here, and if so cross the streams and sync them
  // XXX Needs to be adjusted when we support multiple streams of the same type
  for (std::map<int, bool>::iterator it = mTypes.begin(); it != mTypes.end(); ++it) {
    if (it->second != aIsVideo) {
      // Ok, we have one video, one non-video - cross the streams!
      mozilla::WebrtcAudioConduit *audio_conduit = static_cast<mozilla::WebrtcAudioConduit*>
                                                   (aIsVideo ?
                                                    mPipelines[it->first]->Conduit() :
                                                    aPipeline->Conduit());
      mozilla::WebrtcVideoConduit *video_conduit = static_cast<mozilla::WebrtcVideoConduit*>
                                                   (aIsVideo ?
                                                    aPipeline->Conduit() :
                                                    mPipelines[it->first]->Conduit());
      video_conduit->SyncTo(audio_conduit);
      CSFLogDebug(logTag, "Syncing %p to %p, %d to %d", video_conduit, audio_conduit,
                  aTrack, it->first);
    }
  }
  //TODO: Revisit once we start supporting multiple streams or multiple tracks
  // of same type
  mPipelines[aTrack] = aPipeline;
  //TODO: move to attribute on Pipeline
  mTypes[aTrack] = aIsVideo;
}

RefPtr<MediaPipeline> SourceStreamInfo::GetPipelineByLevel_m(int level) {
  ASSERT_ON_THREAD(mParent->GetMainThread());

  // Refuse to hand out references if we're tearing down.
  // (Since teardown involves a dispatch to and from STS before MediaPipelines
  // are released, it is safe to start other dispatches to and from STS with a
  // RefPtr<MediaPipeline>, since that reference won't be the last one
  // standing)
  if (mMediaStream) {
    for (auto p = mPipelines.begin(); p != mPipelines.end(); ++p) {
      if (p->second->level() == level) {
        return p->second;
      }
    }
  }

  return nullptr;
}

bool RemoteSourceStreamInfo::SetUsingBundle_m(int aLevel, bool decision) {
  ASSERT_ON_THREAD(mParent->GetMainThread());

  RefPtr<MediaPipeline> pipeline(GetPipelineByLevel_m(aLevel));

  if (pipeline) {
    RUN_ON_THREAD(mParent->GetSTSThread(),
                  WrapRunnable(
                      pipeline,
                      &MediaPipeline::SetUsingBundle_s,
                      decision
                  ),
                  NS_DISPATCH_NORMAL);
    return true;
  }
  return false;
}

}  // namespace sipcc
