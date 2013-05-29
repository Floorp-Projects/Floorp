/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _PEER_CONNECTION_MEDIA_H_
#define _PEER_CONNECTION_MEDIA_H_

#include <string>
#include <iostream>
#include <vector>
#include <map>

#include "nspr.h"
#include "prlock.h"

#include "mozilla/RefPtr.h"
#include "nsComponentManagerUtils.h"

#ifdef USE_FAKE_MEDIA_STREAMS
#include "FakeMediaStreams.h"
#else
#include "DOMMediaStream.h"
#include "MediaSegment.h"
#endif

#include "AudioSegment.h"

#ifdef MOZILLA_INTERNAL_API
#include "Layers.h"
#include "VideoUtils.h"
#include "ImageLayers.h"
#include "VideoSegment.h"
#else
namespace mozilla {
  class DataChannel;
}
#endif

#include "nricectx.h"
#include "nriceresolver.h"
#include "nricemediastream.h"
#include "MediaPipeline.h"

namespace sipcc {

class PeerConnectionImpl;
class PeerConnectionMedia;

/* Temporary for providing audio data */
class Fake_AudioGenerator {
 public:
  typedef mozilla::DOMMediaStream DOMMediaStream;

Fake_AudioGenerator(DOMMediaStream* aStream) : mStream(aStream), mCount(0) {
    mTimer = do_CreateInstance("@mozilla.org/timer;1");
    MOZ_ASSERT(mTimer);

    // Make a track
    mozilla::AudioSegment *segment = new mozilla::AudioSegment();
    mStream->GetStream()->AsSourceStream()->AddTrack(1, 16000, 0, segment);

    // Set the timer
    mTimer->InitWithFuncCallback(Callback, this, 100, nsITimer::TYPE_REPEATING_PRECISE);
  }

  static void Callback(nsITimer* timer, void *arg) {
    Fake_AudioGenerator* gen = static_cast<Fake_AudioGenerator*>(arg);

    nsRefPtr<mozilla::SharedBuffer> samples = mozilla::SharedBuffer::Create(1600 * sizeof(int16_t));
    int16_t* data = static_cast<int16_t*>(samples->Data());
    for (int i=0; i<1600; i++) {
      data[i] = ((gen->mCount % 8) * 4000) - (7*4000)/2;
      ++gen->mCount;
    }

    mozilla::AudioSegment segment;
    nsAutoTArray<const int16_t*,1> channelData;
    channelData.AppendElement(data);
    segment.AppendFrames(samples.forget(), channelData, 1600);
    gen->mStream->GetStream()->AsSourceStream()->AppendToTrack(1, &segment);
  }

 private:
  nsCOMPtr<nsITimer> mTimer;
  nsRefPtr<DOMMediaStream> mStream;
  int mCount;
};

/* Temporary for providing video data */
#ifdef MOZILLA_INTERNAL_API
class Fake_VideoGenerator {
 public:
  typedef mozilla::DOMMediaStream DOMMediaStream;

  Fake_VideoGenerator(DOMMediaStream* aStream) {
    mStream = aStream;
    mCount = 0;
    mTimer = do_CreateInstance("@mozilla.org/timer;1");
    MOZ_ASSERT(mTimer);

    // Make a track
    mozilla::VideoSegment *segment = new mozilla::VideoSegment();
    mStream->GetStream()->AsSourceStream()->AddTrack(1, USECS_PER_S, 0, segment);
    mStream->GetStream()->AsSourceStream()->AdvanceKnownTracksTime(mozilla::STREAM_TIME_MAX);

    // Set the timer. Set to 10 fps.
    mTimer->InitWithFuncCallback(Callback, this, 100, nsITimer::TYPE_REPEATING_SLACK);
  }

  static void Callback(nsITimer* timer, void *arg) {
    Fake_VideoGenerator* gen = static_cast<Fake_VideoGenerator*>(arg);

    const uint32_t WIDTH = 640;
    const uint32_t HEIGHT = 480;

    // Allocate a single blank Image
    mozilla::ImageFormat format = mozilla::PLANAR_YCBCR;
    nsRefPtr<mozilla::layers::ImageContainer> container =
      mozilla::layers::LayerManager::CreateImageContainer();

    nsRefPtr<mozilla::layers::Image> image = container->CreateImage(&format, 1);

    int len = ((WIDTH * HEIGHT) * 3 / 2);
    mozilla::layers::PlanarYCbCrImage* planar =
      static_cast<mozilla::layers::PlanarYCbCrImage*>(image.get());
    uint8_t* frame = (uint8_t*) PR_Malloc(len);
    ++gen->mCount;
    memset(frame, (gen->mCount / 8) & 0xff, len); // Rotating colors

    const uint8_t lumaBpp = 8;
    const uint8_t chromaBpp = 4;

    mozilla::layers::PlanarYCbCrImage::Data data;
    data.mYChannel = frame;
    data.mYSize = gfxIntSize(WIDTH, HEIGHT);
    data.mYStride = (int32_t) (WIDTH * lumaBpp / 8.0);
    data.mCbCrStride = (int32_t) (WIDTH * chromaBpp / 8.0);
    data.mCbChannel = frame + HEIGHT * data.mYStride;
    data.mCrChannel = data.mCbChannel + HEIGHT * data.mCbCrStride / 2;
    data.mCbCrSize = gfxIntSize(WIDTH / 2, HEIGHT / 2);
    data.mPicX = 0;
    data.mPicY = 0;
    data.mPicSize = gfxIntSize(WIDTH, HEIGHT);
    data.mStereoMode = mozilla::STEREO_MODE_MONO;

    // SetData copies data, so we can free the frame
    planar->SetData(data);
    PR_Free(frame);

    // AddTrack takes ownership of segment
    mozilla::VideoSegment *segment = new mozilla::VideoSegment();
    // 10 fps.
    segment->AppendFrame(image.forget(), USECS_PER_S / 10, gfxIntSize(WIDTH, HEIGHT));

    gen->mStream->GetStream()->AsSourceStream()->AppendToTrack(1, segment);
  }

 private:
  nsCOMPtr<nsITimer> mTimer;
  nsRefPtr<DOMMediaStream> mStream;
  int mCount;
};
#endif


// TODO(ekr@rtfm.com): Refactor {Local,Remote}SourceStreamInfo
// bug 837539.
class LocalSourceStreamInfo {
public:
  typedef mozilla::DOMMediaStream DOMMediaStream;

  LocalSourceStreamInfo(DOMMediaStream* aMediaStream, PeerConnectionMedia *aParent)
      : mMediaStream(aMediaStream), mParent(aParent) {
    MOZ_ASSERT(aMediaStream);
  }

  ~LocalSourceStreamInfo() {
    mMediaStream = NULL;
  }

  DOMMediaStream* GetMediaStream() {
    return mMediaStream;
  }
  void StorePipeline(int aTrack, mozilla::RefPtr<mozilla::MediaPipeline> aPipeline);

  void ExpectAudio(const mozilla::TrackID);
  void ExpectVideo(const mozilla::TrackID);
  unsigned AudioTrackCount();
  unsigned VideoTrackCount();
  void DetachTransport_s();
  void DetachMedia_m();

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(LocalSourceStreamInfo)
private:
  std::map<int, mozilla::RefPtr<mozilla::MediaPipeline> > mPipelines;
  nsRefPtr<DOMMediaStream> mMediaStream;
  nsTArray<mozilla::TrackID> mAudioTracks;
  nsTArray<mozilla::TrackID> mVideoTracks;
  PeerConnectionMedia *mParent;
};

class RemoteSourceStreamInfo {
 public:
  typedef mozilla::DOMMediaStream DOMMediaStream;

RemoteSourceStreamInfo(already_AddRefed<DOMMediaStream> aMediaStream,
                       PeerConnectionMedia *aParent)
    : mTrackTypeHints(0),
      mMediaStream(aMediaStream),
      mPipelines(),
      mParent(aParent) {
      MOZ_ASSERT(mMediaStream);
    }

  DOMMediaStream* GetMediaStream() {
    return mMediaStream;
  }
  void StorePipeline(int aTrack, bool aIsVideo,
                     mozilla::RefPtr<mozilla::MediaPipeline> aPipeline);

  void DetachTransport_s();
  void DetachMedia_m();

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RemoteSourceStreamInfo)

public:
  DOMMediaStream::TrackTypeHints mTrackTypeHints;
 private:
  nsRefPtr<DOMMediaStream> mMediaStream;
  std::map<int, mozilla::RefPtr<mozilla::MediaPipeline> > mPipelines;
  std::map<int, bool> mTypes;
  PeerConnectionMedia *mParent;
};

class PeerConnectionMedia : public sigslot::has_slots<> {
 public:
  PeerConnectionMedia(PeerConnectionImpl *parent);
  ~PeerConnectionMedia() {}

  nsresult Init(const std::vector<mozilla::NrIceStunServer>& stun_servers,
                const std::vector<mozilla::NrIceTurnServer>& turn_servers);
  // WARNING: This destroys the object!
  void SelfDestruct();

  mozilla::RefPtr<mozilla::NrIceCtx> ice_ctx() const { return mIceCtx; }

  mozilla::RefPtr<mozilla::NrIceMediaStream> ice_media_stream(size_t i) const {
    // TODO(ekr@rtfm.com): If someone asks for a value that doesn't exist,
    // make one.
    if (i >= mIceStreams.size()) {
      return NULL;
    }
    return mIceStreams[i];
  }

  // Add a stream
  nsresult AddStream(nsIDOMMediaStream* aMediaStream, uint32_t *stream_id);

  // Remove a stream
  nsresult RemoveStream(nsIDOMMediaStream* aMediaStream, uint32_t *stream_id);

  // Get a specific local stream
  uint32_t LocalStreamsLength()
  {
    return mLocalSourceStreams.Length();
  }
  LocalSourceStreamInfo* GetLocalStream(int index);

  // Get a specific remote stream
  uint32_t RemoteStreamsLength()
  {
    return mRemoteSourceStreams.Length();
  }
  RemoteSourceStreamInfo* GetRemoteStream(int index);

  // Add a remote stream. Returns the index in index
  nsresult AddRemoteStream(nsRefPtr<RemoteSourceStreamInfo> aInfo, int *aIndex);
  nsresult AddRemoteStreamHint(int aIndex, bool aIsVideo);

  const nsCOMPtr<nsIThread>& GetMainThread() const { return mMainThread; }
  const nsCOMPtr<nsIEventTarget>& GetSTSThread() const { return mSTSThread; }

  // Get a transport flow either RTP/RTCP for a particular stream
  // A stream can be of audio/video/datachannel/budled(?) types
  mozilla::RefPtr<mozilla::TransportFlow> GetTransportFlow(int aStreamIndex,
                                                           bool aIsRtcp) {
    int index_inner = aStreamIndex * 2 + (aIsRtcp ? 1 : 0);

    if (mTransportFlows.find(index_inner) == mTransportFlows.end())
      return NULL;

    return mTransportFlows[index_inner];
  }

  // Add a transport flow
  void AddTransportFlow(int aIndex, bool aRtcp,
                        mozilla::RefPtr<mozilla::TransportFlow> aFlow) {
    int index_inner = aIndex * 2 + (aRtcp ? 1 : 0);

    MOZ_ASSERT(!mTransportFlows[index_inner]);
    mTransportFlows[index_inner] = aFlow;
  }

  mozilla::RefPtr<mozilla::AudioSessionConduit> GetConduit(int aStreamIndex, bool aReceive) {
    int index_inner = aStreamIndex * 2 + (aReceive ? 0 : 1);

    if (mAudioConduits.find(index_inner) == mAudioConduits.end())
      return NULL;

    return mAudioConduits[index_inner];
  }

  // Add a conduit
  void AddConduit(int aIndex, bool aReceive,
                  const mozilla::RefPtr<mozilla::AudioSessionConduit> &aConduit) {
    int index_inner = aIndex * 2 + (aReceive ? 0 : 1);

    MOZ_ASSERT(!mAudioConduits[index_inner]);
    mAudioConduits[index_inner] = aConduit;
  }

  // ICE state signals
  sigslot::signal1<mozilla::NrIceCtx *> SignalIceGatheringCompleted;  // Done gathering
  sigslot::signal1<mozilla::NrIceCtx *> SignalIceCompleted;  // Done handshaking
  sigslot::signal1<mozilla::NrIceCtx *> SignalIceFailed;  // Self explanatory

 private:
  // Shutdown media transport. Must be called on STS thread.
  void ShutdownMediaTransport_s();

  // Final destruction of the media stream. Must be called on the main
  // thread.
  void SelfDestruct_m();

  // ICE events
  void IceGatheringCompleted(mozilla::NrIceCtx *aCtx);
  void IceCompleted(mozilla::NrIceCtx *aCtx);
  void IceFailed(mozilla::NrIceCtx *aCtx);
  void IceStreamReady(mozilla::NrIceMediaStream *aStream);

  // The parent PC
  PeerConnectionImpl *mParent;

  // A list of streams returned from GetUserMedia
  mozilla::Mutex mLocalSourceStreamsLock;
  nsTArray<nsRefPtr<LocalSourceStreamInfo> > mLocalSourceStreams;

  // A list of streams provided by the other side
  nsTArray<nsRefPtr<RemoteSourceStreamInfo> > mRemoteSourceStreams;

  // ICE objects
  mozilla::RefPtr<mozilla::NrIceCtx> mIceCtx;
  std::vector<mozilla::RefPtr<mozilla::NrIceMediaStream> > mIceStreams;

  // DNS
  nsRefPtr<mozilla::NrIceResolver> mDNSResolver;

  // Transport flows: even is RTP, odd is RTCP
  std::map<int, mozilla::RefPtr<mozilla::TransportFlow> > mTransportFlows;

  // Conduits: even is receive, odd is transmit (for easier correlation with
  // flows)
  std::map<int, mozilla::RefPtr<mozilla::AudioSessionConduit> > mAudioConduits;

  // The main thread.
  nsCOMPtr<nsIThread> mMainThread;

  // The STS thread.
  nsCOMPtr<nsIEventTarget> mSTSThread;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(PeerConnectionMedia)
};

}  // namespace sipcc
#endif
