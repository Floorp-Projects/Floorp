/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _PEER_CONNECTION_IMPL_H_
#define _PEER_CONNECTION_IMPL_H_

#include <string>
#include <iostream>
#include <vector>
#include <map>
#include <cmath>

#include "prlock.h"
#include "mozilla/RefPtr.h"
#include "IPeerConnection.h"
#include "nsComponentManagerUtils.h"
#include "nsPIDOMWindow.h"

#ifdef USE_FAKE_MEDIA_STREAMS
#include "FakeMediaStreams.h"
#else
#include "nsDOMMediaStream.h"
#endif

#include "dtlsidentity.h"
#include "nricectx.h"
#include "nricemediastream.h"

#include "peer_connection_types.h"
#include "CallControlManager.h"
#include "CC_Device.h"
#include "CC_Call.h"
#include "CC_Observer.h"
#include "MediaPipeline.h"


#ifdef MOZILLA_INTERNAL_API
#include "mozilla/net/DataChannel.h"
#include "Layers.h"
#include "VideoUtils.h"
#include "ImageLayers.h"
#include "VideoSegment.h"
#else
namespace mozilla {
  class DataChannel;
}
#endif

using namespace mozilla;

namespace sipcc {

/* Temporary for providing audio data */
class Fake_AudioGenerator {
 public:
Fake_AudioGenerator(nsDOMMediaStream* aStream) : mStream(aStream), mCount(0) {
    mTimer = do_CreateInstance("@mozilla.org/timer;1");
    PR_ASSERT(mTimer);

    // Make a track
    mozilla::AudioSegment *segment = new mozilla::AudioSegment();
    segment->Init(1); // 1 Channel
    mStream->GetStream()->AsSourceStream()->AddTrack(1, 16000, 0, segment);

    // Set the timer
    mTimer->InitWithFuncCallback(Callback, this, 100, nsITimer::TYPE_REPEATING_PRECISE);
  }

  static void Callback(nsITimer* timer, void *arg) {
    Fake_AudioGenerator* gen = static_cast<Fake_AudioGenerator*>(arg);

    nsRefPtr<mozilla::SharedBuffer> samples = mozilla::SharedBuffer::Create(1600 * 2 * sizeof(int16_t));
    for (int i=0; i<1600*2; i++) {
      reinterpret_cast<int16_t *>(samples->Data())[i] = ((gen->mCount % 8) * 4000) - (7*4000)/2;
      ++gen->mCount;
    }

    mozilla::AudioSegment segment;
    segment.Init(1);
    segment.AppendFrames(samples.forget(), 1600,
      0, 1600, nsAudioStream::FORMAT_S16);

    gen->mStream->GetStream()->AsSourceStream()->AppendToTrack(1, &segment);
  }

 private:
  nsCOMPtr<nsITimer> mTimer;
  nsRefPtr<nsDOMMediaStream> mStream;
  int mCount;
};

/* Temporary for providing video data */
#ifdef MOZILLA_INTERNAL_API
class Fake_VideoGenerator {
 public:
  Fake_VideoGenerator(nsDOMMediaStream* aStream) {
    mStream = aStream;
    mCount = 0;
    mTimer = do_CreateInstance("@mozilla.org/timer;1");
    PR_ASSERT(mTimer);

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
  nsRefPtr<nsDOMMediaStream> mStream;
  int mCount;
};
#endif

class LocalSourceStreamInfo : public mozilla::MediaStreamListener {
public:
  LocalSourceStreamInfo(nsDOMMediaStream* aMediaStream)
    : mMediaStream(aMediaStream) {}
  ~LocalSourceStreamInfo() {
    mMediaStream = NULL;
  }

  /**
   * Notify that changes to one of the stream tracks have been queued.
   * aTrackEvents can be any combination of TRACK_EVENT_CREATED and
   * TRACK_EVENT_ENDED. aQueuedMedia is the data being added to the track
   * at aTrackOffset (relative to the start of the stream).
   */
  virtual void NotifyQueuedTrackChanges(
    mozilla::MediaStreamGraph* aGraph,
    mozilla::TrackID aID,
    mozilla::TrackRate aTrackRate,
    mozilla::TrackTicks aTrackOffset,
    uint32_t aTrackEvents,
    const mozilla::MediaSegment& aQueuedMedia
  );

  virtual void NotifyPull(mozilla::MediaStreamGraph* aGraph,
    mozilla::StreamTime aDesiredTime) {}

  nsDOMMediaStream* GetMediaStream() {
    return mMediaStream;
  }
  void StorePipeline(int aTrack, mozilla::RefPtr<mozilla::MediaPipeline> aPipeline);

  void ExpectAudio(const mozilla::TrackID);
  void ExpectVideo(const mozilla::TrackID);
  unsigned AudioTrackCount();
  unsigned VideoTrackCount();

  void Detach() {
    // Disconnect my own listener
    GetMediaStream()->GetStream()->RemoveListener(this);

    // walk through all the MediaPipelines and disconnect them.
    for (std::map<int, mozilla::RefPtr<mozilla::MediaPipeline> >::iterator it =
           mPipelines.begin(); it != mPipelines.end();
         ++it) {
      it->second->DetachMediaStream();
    }
    mMediaStream = NULL;
  }

private:
  std::map<int, mozilla::RefPtr<mozilla::MediaPipeline> > mPipelines;
  nsRefPtr<nsDOMMediaStream> mMediaStream;
  nsTArray<mozilla::TrackID> mAudioTracks;
  nsTArray<mozilla::TrackID> mVideoTracks;
};

class RemoteSourceStreamInfo {
 public:
  RemoteSourceStreamInfo(nsDOMMediaStream* aMediaStream) :
      mMediaStream(aMediaStream),
      mPipelines() {}

  nsDOMMediaStream* GetMediaStream() {
    return mMediaStream;
  }
  void StorePipeline(int aTrack, mozilla::RefPtr<mozilla::MediaPipeline> aPipeline);

  void Detach() {
    // walk through all the MediaPipelines and disconnect them.
    for (std::map<int, mozilla::RefPtr<mozilla::MediaPipeline> >::iterator it =
           mPipelines.begin(); it != mPipelines.end();
         ++it) {
      it->second->DetachMediaStream();
    }
    mMediaStream = NULL;
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RemoteSourceStreamInfo)
 private:
  nsRefPtr<nsDOMMediaStream> mMediaStream;
  std::map<int, mozilla::RefPtr<mozilla::MediaPipeline> > mPipelines;
};

class PeerConnectionWrapper;

class PeerConnectionImpl MOZ_FINAL : public IPeerConnection,
#ifdef MOZILLA_INTERNAL_API
                                     public mozilla::DataChannelConnection::DataConnectionListener,
#endif
                                     public sigslot::has_slots<> {
public:
  PeerConnectionImpl();
  ~PeerConnectionImpl();

  enum ReadyState {
    kNew,
    kNegotiating,
    kActive,
    kClosing,
    kClosed
  };

  enum SipccState {
    kIdle,
    kStarting,
    kStarted
  };

  // TODO(ekr@rtfm.com): make this conform to the specifications
  enum IceState {
    kIceGathering,
    kIceWaiting,
    kIceChecking,
    kIceConnected,
    kIceFailed
  };

  enum Role {
    kRoleUnknown,
    kRoleOfferer,
    kRoleAnswerer
  };

  NS_DECL_ISUPPORTS
  NS_DECL_IPEERCONNECTION

  static PeerConnectionImpl* CreatePeerConnection();
  static void Shutdown();

  Role GetRole() const { return mRole; }
  nsresult CreateRemoteSourceStreamInfo(uint32_t aHint, RemoteSourceStreamInfo** aInfo);

  // Implementation of the only observer we need
  virtual void onCallEvent(
    ccapi_call_event_e aCallEvent,
    CSF::CC_CallPtr aCall,
    CSF::CC_CallInfoPtr aInfo
  );

  // DataConnection observers
  void NotifyConnection();
  void NotifyClosedConnection();
  void NotifyDataChannel(mozilla::DataChannel *aChannel);

  // Handle system to allow weak references to be passed through C code
  static PeerConnectionWrapper *AcquireInstance(const std::string& aHandle);
  virtual void ReleaseInstance();
  virtual const std::string& GetHandle();

  // ICE events
  void IceGatheringCompleted(NrIceCtx *aCtx);
  void IceCompleted(NrIceCtx *aCtx);
  void IceStreamReady(NrIceMediaStream *aStream);

  mozilla::RefPtr<NrIceCtx> ice_ctx() const { return mIceCtx; }
  mozilla::RefPtr<NrIceMediaStream> ice_media_stream(size_t i) const {
    // TODO(ekr@rtfm.com): If someone asks for a value that doesn't exist,
    // make one.
    if (i >= mIceStreams.size()) {
      return NULL;
    }
    return mIceStreams[i];
  }

  // Get a specific local stream
  nsRefPtr<LocalSourceStreamInfo> GetLocalStream(int aIndex);

  // Get a specific remote stream
  nsRefPtr<RemoteSourceStreamInfo> GetRemoteStream(int aIndex);

  // Add a remote stream. Returns the index in index
  nsresult AddRemoteStream(nsRefPtr<RemoteSourceStreamInfo> aInfo, int *aIndex);

  // Get a transport flow either RTP/RTCP for a particular stream
  // A stream can be of audio/video/datachannel/budled(?) types
  mozilla::RefPtr<TransportFlow> GetTransportFlow(int aStreamIndex, bool aIsRtcp) {
    int index_inner = aStreamIndex * 2 + (aIsRtcp ? 1 : 0);

    if (mTransportFlows.find(index_inner) == mTransportFlows.end())
      return NULL;

    return mTransportFlows[index_inner];
  }

  // Add a transport flow
  void AddTransportFlow(int aIndex, bool aRtcp, mozilla::RefPtr<TransportFlow> aFlow) {
    int index_inner = aIndex * 2 + (aRtcp ? 1 : 0);

    mTransportFlows[index_inner] = aFlow;
  }

  static void ListenThread(void *aData);
  static void ConnectThread(void *aData);

  // Get the main thread
  nsCOMPtr<nsIThread> GetMainThread() { return mThread; }

  // Get the STS thread
  nsCOMPtr<nsIEventTarget> GetSTSThread() { return mSTSThread; }

  // Get the DTLS identity
  mozilla::RefPtr<DtlsIdentity> const GetIdentity() { return mIdentity; }

  // Create a fake media stream
  nsresult CreateFakeMediaStream(uint32_t hint, nsIDOMMediaStream** retval);

private:
  PeerConnectionImpl(const PeerConnectionImpl&rhs);
  PeerConnectionImpl& operator=(PeerConnectionImpl);

  void ChangeReadyState(ReadyState aReadyState);
  void CheckIceState() {
    PR_ASSERT(mIceState != kIceGathering);
  }

  // Shut down media. Called on any thread.
  void ShutdownMedia();

  // Disconnect the media streams. Must be called on the
  // main thread.
  void DisconnectMediaStreams();

  // Shutdown media transport. Must be called on STS thread.
  void ShutdownMediaTransport();

  nsresult MakeMediaStream(uint32_t aHint, nsIDOMMediaStream** aStream);
  nsresult MakeRemoteSource(nsDOMMediaStream* aStream, RemoteSourceStreamInfo** aInfo);

  // The role we are adopting
  Role mRole;

  // The call
  CSF::CC_CallPtr mCall;
  ReadyState mReadyState;


  nsCOMPtr<nsIThread> mThread;
  nsCOMPtr<IPeerConnectionObserver> mPCObserver;
  nsCOMPtr<nsPIDOMWindow> mWindow;

  // The SDP sent in from JS - here for debugging.
  std::string mLocalRequestedSDP;
  std::string mRemoteRequestedSDP;
  // The SDP we are using.
  std::string mLocalSDP;
  std::string mRemoteSDP;

  // DTLS fingerprint
  std::string mFingerprint;
  std::string mRemoteFingerprint;

  // A list of streams returned from GetUserMedia
  PRLock *mLocalSourceStreamsLock;
  nsTArray<nsRefPtr<LocalSourceStreamInfo> > mLocalSourceStreams;

  // A list of streams provided by the other side
  PRLock *mRemoteSourceStreamsLock;
  nsTArray<nsRefPtr<RemoteSourceStreamInfo> > mRemoteSourceStreams;

  // A handle to refer to this PC with
  std::string mHandle;

  // ICE objects
  mozilla::RefPtr<NrIceCtx> mIceCtx;
  std::vector<mozilla::RefPtr<NrIceMediaStream> > mIceStreams;
  IceState mIceState;

  // Transport flows: even is RTP, odd is RTCP
  std::map<int, mozilla::RefPtr<TransportFlow> > mTransportFlows;

  // The DTLS identity
  mozilla::RefPtr<DtlsIdentity> mIdentity;

  // The target to run stuff on
  nsCOMPtr<nsIEventTarget> mSTSThread;

#ifdef MOZILLA_INTERNAL_API
  // DataConnection that's used to get all the DataChannels
	nsRefPtr<mozilla::DataChannelConnection> mDataConnection;
#endif

  // Singleton list of all the PeerConnections
  static std::map<const std::string, PeerConnectionImpl *> peerconnections;

public:
  //these are temporary until the DataChannel Listen/Connect API is removed
  unsigned short listenPort;
  unsigned short connectPort;
  char *connectStr; // XXX ownership/free
};

// This is what is returned when you acquire on a handle
class PeerConnectionWrapper {
 public:
  PeerConnectionWrapper(PeerConnectionImpl *impl) : impl_(impl) {}

  ~PeerConnectionWrapper() {
    if (impl_)
      impl_->ReleaseInstance();
  }

  PeerConnectionImpl *impl() { return impl_; }

 private:
  PeerConnectionImpl *impl_;
};

}  // end sipcc namespace

#endif  // _PEER_CONNECTION_IMPL_H_
