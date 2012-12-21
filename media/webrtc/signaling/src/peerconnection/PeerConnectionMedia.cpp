/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <string>

#include "nspr.h"
#include "cc_constants.h"
#include "CSFLog.h"
#include "CSFLogStream.h"

#include "nricectx.h"
#include "nricemediastream.h"
#include "PeerConnectionImpl.h"
#include "PeerConnectionMedia.h"
#include "runnable_utils.h"

#ifdef MOZILLA_INTERNAL_API
#include "MediaStreamList.h"
#include "nsIScriptGlobalObject.h"
#include "jsapi.h"
#endif

using namespace mozilla;

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

PeerConnectionImpl* PeerConnectionImpl::CreatePeerConnection()
{
  PeerConnectionImpl *pc = new PeerConnectionImpl();

  CSFLogDebugS(logTag, "Created PeerConnection: " << static_cast<void*>(pc));

  return pc;
}


nsresult PeerConnectionMedia::Init()
{
  // TODO(ekr@rtfm.com): need some way to set not offerer later
  // Looks like a bug in the NrIceCtx API.
  mIceCtx = NrIceCtx::Create("PC:" + mParent->GetHandle(), true);
  if(!mIceCtx) {
    CSFLogErrorS(logTag, __FUNCTION__ << ": Failed to create Ice Context");
    return NS_ERROR_FAILURE;
  }

  mIceCtx->SignalGatheringCompleted.connect(this,
                                            &PeerConnectionMedia::IceGatheringCompleted);
  mIceCtx->SignalCompleted.connect(this,
                                   &PeerConnectionMedia::IceCompleted);

  // Create three streams to start with.
  // One each for audio, video and DataChannel
  // TODO: this will be re-visited
  RefPtr<NrIceMediaStream> audioStream = mIceCtx->CreateStream("stream1", 2);
  RefPtr<NrIceMediaStream> videoStream = mIceCtx->CreateStream("stream2", 2);
  RefPtr<NrIceMediaStream> dcStream = mIceCtx->CreateStream("stream3", 2);

  if (!audioStream) {
    CSFLogErrorS(logTag, __FUNCTION__ << ": audio stream is NULL");
    return NS_ERROR_FAILURE;
  } else {
    mIceStreams.push_back(audioStream);
  }

  if (!videoStream) {
    CSFLogErrorS(logTag, __FUNCTION__ << ": video stream is NULL");
    return NS_ERROR_FAILURE;
  } else {
    mIceStreams.push_back(videoStream);
  }

  if (!dcStream) {
    CSFLogErrorS(logTag, __FUNCTION__ << ": datachannel stream is NULL");
    return NS_ERROR_FAILURE;
  } else {
    mIceStreams.push_back(dcStream);
  }

  // TODO(ekr@rtfm.com): This is not connected to the PCCimpl.
  // Will need to do that later.
  for (std::size_t i=0; i<mIceStreams.size(); i++) {
    mIceStreams[i]->SignalReady.connect(this, &PeerConnectionMedia::IceStreamReady);
  }

  // Start gathering
  nsresult res;
  mIceCtx->thread()->Dispatch(WrapRunnableRet(
    mIceCtx, &NrIceCtx::StartGathering, &res), NS_DISPATCH_SYNC
  );

  if (NS_FAILED(res)) {
    CSFLogErrorS(logTag, __FUNCTION__ << ": StartGathering failed: " <<
        static_cast<uint32_t>(res));
    return res;
  }

  return NS_OK;
}

nsresult
PeerConnectionMedia::AddStream(nsIDOMMediaStream* aMediaStream, uint32_t *stream_id)
{
  if (!aMediaStream) {
    CSFLogError(logTag, "%s - aMediaStream is NULL", __FUNCTION__);
    return NS_ERROR_FAILURE;
  }

  nsDOMMediaStream* stream = static_cast<nsDOMMediaStream*>(aMediaStream);

  CSFLogDebugS(logTag, __FUNCTION__ << ": MediaStream: " << static_cast<void*>(aMediaStream));

  // Adding tracks here based on nsDOMMediaStream expectation settings
  uint32_t hints = stream->GetHintContents();

  if (!(hints & (nsDOMMediaStream::HINT_CONTENTS_AUDIO |
        nsDOMMediaStream::HINT_CONTENTS_VIDEO))) {
    CSFLogDebug(logTag, "Empty Stream !!");
    return NS_OK;
  }

  // Now see if we already have a stream of this type, since we only
  // allow one of each.
  // TODO(ekr@rtfm.com): remove this when multiple of each stream
  // is allowed
  PR_Lock(mLocalSourceStreamsLock);
  for (uint32_t u = 0; u < mLocalSourceStreams.Length(); u++) {
    nsRefPtr<LocalSourceStreamInfo> localSourceStream = mLocalSourceStreams[u];

    if (localSourceStream->GetMediaStream()->GetHintContents() & hints) {
      CSFLogError(logTag, "Only one stream of any given type allowed");
      PR_Unlock(mLocalSourceStreamsLock);
      return NS_ERROR_FAILURE;
    }
  }

  // OK, we're good to add
  nsRefPtr<LocalSourceStreamInfo> localSourceStream =
    new LocalSourceStreamInfo(stream);
  *stream_id = mLocalSourceStreams.Length();

  if (hints & nsDOMMediaStream::HINT_CONTENTS_AUDIO) {
    localSourceStream->ExpectAudio(TRACK_AUDIO);
  }

  if (hints & nsDOMMediaStream::HINT_CONTENTS_VIDEO) {
    localSourceStream->ExpectVideo(TRACK_VIDEO);
  }

  mLocalSourceStreams.AppendElement(localSourceStream);

  PR_Unlock(mLocalSourceStreamsLock);
  return NS_OK;
}

nsresult
PeerConnectionMedia::RemoveStream(nsIDOMMediaStream* aMediaStream, uint32_t *stream_id)
{
  MOZ_ASSERT(aMediaStream);

  nsDOMMediaStream* stream = static_cast<nsDOMMediaStream*>(aMediaStream);

  CSFLogDebugS(logTag, __FUNCTION__ << ": MediaStream: " << static_cast<void*>(aMediaStream));

  PR_Lock(mLocalSourceStreamsLock);
  for (uint32_t u = 0; u < mLocalSourceStreams.Length(); u++) {
    nsRefPtr<LocalSourceStreamInfo> localSourceStream = mLocalSourceStreams[u];
    if (localSourceStream->GetMediaStream() == stream) {
      *stream_id = u;
      PR_Unlock(mLocalSourceStreamsLock);
      return NS_OK;
    }
  }

  return NS_ERROR_ILLEGAL_VALUE;
}

void
PeerConnectionMedia::SelfDestruct()
{
   CSFLogDebugS(logTag, __FUNCTION__ << " Disconnecting media streams from PC");

   DisconnectMediaStreams();

   CSFLogDebugS(logTag, __FUNCTION__ << " Disconnecting transport");
   // Shutdown the transport.
   RUN_ON_THREAD(mParent->GetSTSThread(), WrapRunnable(
       this, &PeerConnectionMedia::ShutdownMediaTransport), NS_DISPATCH_SYNC);

  CSFLogDebugS(logTag, __FUNCTION__ << " Media shut down");

  // This should destroy the object.
  this->Release();
}

void
PeerConnectionMedia::DisconnectMediaStreams()
{
  for (uint32_t i=0; i < mLocalSourceStreams.Length(); ++i) {
    mLocalSourceStreams[i]->Detach();
  }

  for (uint32_t i=0; i < mRemoteSourceStreams.Length(); ++i) {
    mRemoteSourceStreams[i]->Detach();
  }

  mLocalSourceStreams.Clear();
  mRemoteSourceStreams.Clear();
}

void
PeerConnectionMedia::ShutdownMediaTransport()
{
  mIceCtx->SignalCompleted.disconnect(this);
  mIceCtx->SignalGatheringCompleted.disconnect(this);
  mTransportFlows.clear();
  mIceStreams.clear();
  mIceCtx = NULL;
}

LocalSourceStreamInfo*
PeerConnectionMedia::GetLocalStream(int aIndex)
{
  if(aIndex < 0 || aIndex >= (int) mLocalSourceStreams.Length()) {
    return NULL;
  }

  MOZ_ASSERT(mLocalSourceStreams[aIndex]);
  return mLocalSourceStreams[aIndex];
}

RemoteSourceStreamInfo*
PeerConnectionMedia::GetRemoteStream(int aIndex)
{
  if(aIndex < 0 || aIndex >= (int) mRemoteSourceStreams.Length()) {
    return NULL;
  }

  MOZ_ASSERT(mRemoteSourceStreams[aIndex]);
  return mRemoteSourceStreams[aIndex];
}

nsresult
PeerConnectionMedia::AddRemoteStream(nsRefPtr<RemoteSourceStreamInfo> aInfo,
  int *aIndex)
{
  MOZ_ASSERT(aIndex);

  *aIndex = mRemoteSourceStreams.Length();

  mRemoteSourceStreams.AppendElement(aInfo);

  return NS_OK;
}


void
PeerConnectionMedia::IceGatheringCompleted(NrIceCtx *aCtx)
{
  MOZ_ASSERT(aCtx);
  SignalIceGatheringCompleted(aCtx);
}

void
PeerConnectionMedia::IceCompleted(NrIceCtx *aCtx)
{
  MOZ_ASSERT(aCtx);
  SignalIceCompleted(aCtx);
}

void
PeerConnectionMedia::IceStreamReady(NrIceMediaStream *aStream)
{
  MOZ_ASSERT(aStream);

  CSFLogDebugS(logTag, __FUNCTION__ << ": "  << aStream->name().c_str());
}


void
LocalSourceStreamInfo::StorePipeline(int aTrack,
  mozilla::RefPtr<mozilla::MediaPipeline> aPipeline)
{
  MOZ_ASSERT(mPipelines.find(aTrack) == mPipelines.end());
  if (mPipelines.find(aTrack) != mPipelines.end()) {
    CSFLogErrorS(logTag, __FUNCTION__ << ": Storing duplicate track");
    return;
  }
  //TODO: Revisit once we start supporting multiple streams or multiple tracks
  // of same type
  mPipelines[aTrack] = aPipeline;
}

void
RemoteSourceStreamInfo::StorePipeline(int aTrack,
  mozilla::RefPtr<mozilla::MediaPipeline> aPipeline)
{
  MOZ_ASSERT(mPipelines.find(aTrack) == mPipelines.end());
  if (mPipelines.find(aTrack) != mPipelines.end()) {
    CSFLogErrorS(logTag, __FUNCTION__ << ": Storing duplicate track");
    return;
  }
  //TODO: Revisit once we start supporting multiple streams or multiple tracks
  // of same type
  mPipelines[aTrack] = aPipeline;
}


}  // namespace sipcc
