/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FAKE_MEDIA_STREAM_H_
#define FAKE_MEDIA_STREAM_H_

#include <set>
#include <string>
#include <sstream>
#include <vector>

#include "nsNetCID.h"
#include "nsITimer.h"
#include "nsComponentManagerUtils.h"
#include "nsIComponentManager.h"
#include "nsIComponentRegistrar.h"
#include "nsISupportsImpl.h"
#include "nsServiceManagerUtils.h"

// #includes from MediaStream.h
#include "mozilla/Mutex.h"
#include "AudioSegment.h"
#include "MediaSegment.h"
#include "StreamTracks.h"
#include "VideoSegment.h"
#include "nsTArray.h"
#include "nsIRunnable.h"
#include "nsISupportsImpl.h"

class nsPIDOMWindowInner;

namespace mozilla {
   class MediaStreamGraphImpl;
   class MediaSegment;
   class PeerConnectionImpl;
   class PeerConnectionMedia;
   class RemoteSourceStreamInfo;
};


namespace mozilla {

class MediaStreamGraph;

static MediaStreamGraph* gGraph;

struct AudioChannel {
  enum {
    Normal
  };
};

enum MediaStreamGraphEvent : uint32_t;
enum TrackEventCommand : uint32_t;

class MediaStreamGraph {
public:
  // Keep this in sync with the enum in MediaStreamGraph.h
  enum GraphDriverType {
    AUDIO_THREAD_DRIVER,
    SYSTEM_THREAD_DRIVER,
    OFFLINE_THREAD_DRIVER
  };
  static MediaStreamGraph* GetInstance(GraphDriverType aDriverType,
                                       uint32_t aType,
                                       nsPIDOMWindowInner* aWindow) {
    // We don't care about the AudioChannel type nor the window here.
    if (gGraph) {
      return gGraph;
    }
    gGraph = new MediaStreamGraph();
    return gGraph;
  }
  uint32_t GraphRate() { return 16000; }
};
}

class Fake_VideoSink {
public:
  Fake_VideoSink() {}
  virtual void SegmentReady(mozilla::MediaSegment* aSegment) = 0;
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Fake_VideoSink)
protected:
  virtual ~Fake_VideoSink() {}
};

class Fake_MediaStream;
class Fake_SourceMediaStream;

class Fake_MediaStreamListener
{
protected:
  virtual ~Fake_MediaStreamListener() {}

public:
  virtual void NotifyQueuedTrackChanges(mozilla::MediaStreamGraph* aGraph, mozilla::TrackID aID,
                                        mozilla::StreamTime aTrackOffset,
                                        mozilla::TrackEventCommand  aTrackEvents,
                                        const mozilla::MediaSegment& aQueuedMedia,
                                        Fake_MediaStream* aInputStream,
                                        mozilla::TrackID aInputTrackID) {}
  virtual void NotifyPull(mozilla::MediaStreamGraph* aGraph, mozilla::StreamTime aDesiredTime) = 0;
  virtual void NotifyQueuedAudioData(mozilla::MediaStreamGraph* aGraph, mozilla::TrackID aID,
                                       mozilla::StreamTime aTrackOffset,
                                       const mozilla::AudioSegment& aQueuedMedia,
                                       Fake_MediaStream* aInputStream,
                                       mozilla::TrackID aInputTrackID) {}

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Fake_MediaStreamListener)
};

class Fake_DirectMediaStreamListener : public Fake_MediaStreamListener
{
protected:
  virtual ~Fake_DirectMediaStreamListener() {}

public:
  virtual void NotifyRealtimeData(mozilla::MediaStreamGraph* graph, mozilla::TrackID tid,
                                  mozilla::StreamTime offset,
                                  const mozilla::MediaSegment& media) = 0;
};

class Fake_MediaStreamTrackListener
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Fake_MediaStreamTrackListener)

protected:
  virtual ~Fake_MediaStreamTrackListener() {}

public:
  virtual void NotifyQueuedChanges(mozilla::MediaStreamGraph* aGraph,
                                   mozilla::StreamTime aTrackOffset,
                                   const mozilla::MediaSegment& aQueuedMedia) = 0;
};

class Fake_DirectMediaStreamTrackListener : public Fake_MediaStreamTrackListener
{
protected:
  virtual ~Fake_DirectMediaStreamTrackListener() {}

public:
  virtual void NotifyRealtimeTrackData(mozilla::MediaStreamGraph* aGraph,
                                       mozilla::StreamTime aTrackOffset,
                                       const mozilla::MediaSegment& aMedia) = 0;
  enum class InstallationResult {
    STREAM_NOT_SUPPORTED,
    SUCCESS
  };
  virtual void NotifyDirectListenerInstalled(InstallationResult aResult) = 0;
  virtual void NotifyDirectListenerUninstalled() = 0;
};

class Fake_MediaStreamVideoSink : public Fake_DirectMediaStreamTrackListener{
public:
  Fake_MediaStreamVideoSink() {}

  void NotifyQueuedChanges(mozilla::MediaStreamGraph* aGraph,
                           mozilla::StreamTime aTrackOffset,
                           const mozilla::MediaSegment& aQueuedMedia) override {}

  void NotifyRealtimeTrackData(mozilla::MediaStreamGraph* aGraph,
                               mozilla::StreamTime aTrackOffset,
                               const mozilla::MediaSegment& aMedia) override {}
  void NotifyDirectListenerInstalled(InstallationResult aResult) override {}
  void NotifyDirectListenerUninstalled() override {}

  virtual void SetCurrentFrames(const mozilla::VideoSegment& aSegment) {};
  virtual void ClearFrames() {};

protected:
  virtual ~Fake_MediaStreamVideoSink() {}
};

// Note: only one listener supported
class Fake_MediaStream {
 protected:
  virtual ~Fake_MediaStream() { Stop(); }

  struct BoundTrackListener
  {
    BoundTrackListener(Fake_MediaStreamTrackListener* aListener,
                       mozilla::TrackID aTrackID)
      : mListener(aListener), mTrackID(aTrackID) {}
    RefPtr<Fake_MediaStreamTrackListener> mListener;
    mozilla::TrackID mTrackID;
  };

 public:
  Fake_MediaStream () : mListeners(), mTrackListeners(), mMutex("Fake MediaStream") {}

  void AddListener(Fake_MediaStreamListener *aListener) {
    mozilla::MutexAutoLock lock(mMutex);
    mListeners.insert(aListener);
  }

  void RemoveListener(Fake_MediaStreamListener *aListener) {
    mozilla::MutexAutoLock lock(mMutex);
    mListeners.erase(aListener);
  }

  void AddTrackListener(Fake_MediaStreamTrackListener *aListener,
                        mozilla::TrackID aTrackID) {
    mozilla::MutexAutoLock lock(mMutex);
    mTrackListeners.push_back(BoundTrackListener(aListener, aTrackID));
  }

  void RemoveTrackListener(Fake_MediaStreamTrackListener *aListener,
                           mozilla::TrackID aTrackID) {
    mozilla::MutexAutoLock lock(mMutex);
    for (std::vector<BoundTrackListener>::iterator it = mTrackListeners.begin();
         it != mTrackListeners.end(); ++it) {
      if (it->mListener == aListener && it->mTrackID == aTrackID) {
        mTrackListeners.erase(it);
        return;
      }
    }
  }

  void NotifyPull(mozilla::MediaStreamGraph* graph,
                  mozilla::StreamTime aDesiredTime) {

    mozilla::MutexAutoLock lock(mMutex);
    std::set<RefPtr<Fake_MediaStreamListener>>::iterator it;
    for (it = mListeners.begin(); it != mListeners.end(); ++it) {
      (*it)->NotifyPull(graph, aDesiredTime);
    }
  }

  virtual Fake_SourceMediaStream *AsSourceStream() { return nullptr; }

  virtual mozilla::MediaStreamGraphImpl *GraphImpl() { return nullptr; }
  virtual nsresult Start() { return NS_OK; }
  virtual nsresult Stop() { return NS_OK; }
  virtual void StopStream() {}

  virtual void Periodic() {}

  double StreamTimeToSeconds(mozilla::StreamTime aTime);
  mozilla::StreamTime
  TicksToTimeRoundDown(mozilla::TrackRate aRate, mozilla::TrackTicks aTicks);
  mozilla::TrackTicks TimeToTicksRoundUp(mozilla::TrackRate aRate,
                                         mozilla::StreamTime aTime);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Fake_MediaStream);

 protected:
  std::set<RefPtr<Fake_MediaStreamListener>> mListeners;
  std::vector<BoundTrackListener> mTrackListeners;
  mozilla::Mutex mMutex;  // Lock to prevent the listener list from being modified while
                          // executing Periodic().
};

class Fake_MediaPeriodic : public nsITimerCallback {
public:
  explicit Fake_MediaPeriodic(Fake_MediaStream *aStream) : mStream(aStream),
                                                           mCount(0) {}
  void Detach() {
    mStream = nullptr;
  }

  int GetTimesCalled() { return mCount; }

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK

protected:
  virtual ~Fake_MediaPeriodic() {}

  Fake_MediaStream *mStream;
  int mCount;
};


class Fake_SourceMediaStream : public Fake_MediaStream {
 public:
  Fake_SourceMediaStream() : mSegmentsAdded(0),
                             mDesiredTime(0),
                             mPullEnabled(false),
                             mStop(false),
                             mPeriodic(new Fake_MediaPeriodic(this)) {}

  enum {
    ADDTRACK_QUEUED    = 0x01 // Queue track add until FinishAddTracks()
  };

  void AddVideoSink(const RefPtr<Fake_VideoSink>& aSink) {
    mSink  = aSink;
  }

  void AddTrack(mozilla::TrackID aID, mozilla::StreamTime aStart,
                mozilla::MediaSegment* aSegment, uint32_t aFlags = 0) {
    delete aSegment;
  }
  void AddAudioTrack(mozilla::TrackID aID, mozilla::TrackRate aRate, mozilla::StreamTime aStart,
                     mozilla::AudioSegment* aSegment, uint32_t aFlags = 0) {
    delete aSegment;
  }
  void FinishAddTracks() {}
  void EndTrack(mozilla::TrackID aID) {}

  bool AppendToTrack(mozilla::TrackID aID, mozilla::MediaSegment* aSegment,
                     mozilla::MediaSegment *aRawSegment) {
    return AppendToTrack(aID, aSegment);
  }

  bool AppendToTrack(mozilla::TrackID aID, mozilla::MediaSegment* aSegment) {
    bool nonZeroSample = false;
    MOZ_ASSERT(aSegment);
    if(aSegment->GetType() == mozilla::MediaSegment::AUDIO) {
      //On audio segment append, we verify for validity
      //of the audio samples.
      mozilla::AudioSegment* audio =
              static_cast<mozilla::AudioSegment*>(aSegment);
      mozilla::AudioSegment::ChunkIterator iter(*audio);
      while(!iter.IsEnded()) {
        mozilla::AudioChunk& chunk = *(iter);
        MOZ_ASSERT(chunk.mBuffer);
        const int16_t* buf =
          static_cast<const int16_t*>(chunk.mChannelData[0]);
        for(int i=0; i<chunk.mDuration; i++) {
          if(buf[i]) {
            //atleast one non-zero sample found.
            nonZeroSample = true;
            break;
          }
        }
        //process next chunk
        iter.Next();
      }
      if(nonZeroSample) {
          //we increment segments count if
          //atleast one non-zero samples was found.
          ++mSegmentsAdded;
      }
    } else {
      //in the case of video segment appended, we just increase the
      //segment count.
      if (mSink.get()) {
        mSink->SegmentReady(aSegment);
      }
      ++mSegmentsAdded;
    }
    return true;
  }

  void AdvanceKnownTracksTime(mozilla::StreamTime aKnownTime) {}

  void SetPullEnabled(bool aEnabled) {
    mPullEnabled = aEnabled;
  }
  void AddDirectListener(Fake_MediaStreamListener* aListener) {}
  void RemoveDirectListener(Fake_MediaStreamListener* aListener) {}

  //Don't pull anymore data,if mStop is true.
  virtual void StopStream() {
   mStop = true;
  }

  virtual Fake_SourceMediaStream *AsSourceStream() { return this; }

  virtual nsresult Start();
  virtual nsresult Stop();

  virtual void Periodic();

  virtual int GetSegmentsAdded() {
    return mSegmentsAdded;
  }

 protected:
  int mSegmentsAdded;
  uint64_t mDesiredTime;
  bool mPullEnabled;
  bool mStop;
  RefPtr<Fake_MediaPeriodic> mPeriodic;
  RefPtr<Fake_VideoSink> mSink;
  nsCOMPtr<nsITimer> mTimer;
};

class Fake_DOMMediaStream;

class Fake_MediaStreamTrackSource
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Fake_MediaStreamTrackSource)

protected:
  virtual ~Fake_MediaStreamTrackSource() {}
};

class Fake_MediaStreamTrack
{
  friend class mozilla::PeerConnectionImpl;
  friend class mozilla::PeerConnectionMedia;
  friend class mozilla::RemoteSourceStreamInfo;
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Fake_MediaStreamTrack)

  Fake_MediaStreamTrack(bool aIsVideo, Fake_DOMMediaStream* aOwningStream) :
    mIsVideo (aIsVideo),
    mOwningStream (aOwningStream),
    mTrackID(mIsVideo ? 1 : 0)
  {
    static size_t counter = 0;
    std::ostringstream os;
    os << counter++;
    mID = os.str();
  }

  std::string GetId() const { return mID; }
  void AssignId(const std::string& id) { mID = id; }
  mozilla::MediaStreamGraphImpl* GraphImpl() { return nullptr; }
  Fake_MediaStreamTrack* AsVideoStreamTrack()
  {
    return mIsVideo ? this : nullptr;
  }
  Fake_MediaStreamTrack* AsAudioStreamTrack()
  {
    return mIsVideo ? nullptr : this;
  }
  const Fake_MediaStreamTrack* AsVideoStreamTrack() const
  {
    return mIsVideo ? this : nullptr;
  }
  const Fake_MediaStreamTrack* AsAudioStreamTrack() const
  {
    return mIsVideo ? nullptr : this;
  }
  uint32_t typeSize () const
  {
    return sizeof(Fake_MediaStreamTrack);
  }
  const char* typeName () const
  {
    return "Fake_MediaStreamTrack";
  }
  void AddListener(Fake_MediaStreamTrackListener *aListener);
  void RemoveListener(Fake_MediaStreamTrackListener *aListener);
  void AddDirectListener(Fake_DirectMediaStreamTrackListener *aListener)
  {
    aListener->NotifyDirectListenerInstalled(
      Fake_DirectMediaStreamTrackListener::InstallationResult::STREAM_NOT_SUPPORTED);
  }
  void RemoveDirectListener(Fake_DirectMediaStreamTrackListener *aListener) {}
  void AddVideoOutput(Fake_MediaStreamVideoSink *aOutput)
  {
    AddDirectListener(aOutput);
  }
  void RemoveVideoOutput(Fake_MediaStreamVideoSink *aOutput)
  {
    RemoveDirectListener(aOutput);
  }

  class PrincipalChangeObserver
  {
  public:
    virtual void PrincipalChanged(Fake_MediaStreamTrack* aMediaStreamTrack) = 0;
  };
  void AddPrincipalChangeObserver(void* ignoredObserver) {}
  void RemovePrincipalChangeObserver(void* ignoredObserver) {}

private:
  ~Fake_MediaStreamTrack() {}

  const bool mIsVideo;
  Fake_DOMMediaStream* mOwningStream;
  mozilla::TrackID mTrackID;
  std::string mID;
};

class Fake_DOMMediaStream : public nsISupports
{
  friend class mozilla::PeerConnectionMedia;
protected:
  virtual ~Fake_DOMMediaStream() {
    // Note: memory leak
    mMediaStream->Stop();
  }

public:
  explicit Fake_DOMMediaStream(Fake_MediaStream *stream = nullptr)
    : mMediaStream(stream ? stream : new Fake_MediaStream())
    , mVideoTrack(new Fake_MediaStreamTrack(true, this))
    , mAudioTrack(new Fake_MediaStreamTrack(false, this))
    {
      static size_t counter = 0;
      std::ostringstream os;
      os << counter++;
      mID = os.str();
    }

  NS_DECL_THREADSAFE_ISUPPORTS

  static already_AddRefed<Fake_DOMMediaStream>
  CreateSourceStreamAsInput(nsPIDOMWindowInner* aWindow,
                            mozilla::MediaStreamGraph* aGraph,
                            uint32_t aHintContents = 0) {
    Fake_SourceMediaStream *source = new Fake_SourceMediaStream();

    RefPtr<Fake_DOMMediaStream> ds = new Fake_DOMMediaStream(source);
    ds->SetHintContents(aHintContents);

    return ds.forget();
  }

  virtual void Stop() {} // Really DOMLocalMediaStream

  virtual bool AddDirectListener(Fake_MediaStreamListener *aListener) { return false; }
  virtual void RemoveDirectListener(Fake_MediaStreamListener *aListener) {}

  Fake_MediaStream *GetInputStream() { return mMediaStream; }
  Fake_MediaStream *GetOwnedStream() { return mMediaStream; }
  Fake_MediaStream *GetPlaybackStream() { return mMediaStream; }
  Fake_MediaStream *GetStream() { return mMediaStream; }
  std::string GetId() const { return mID; }
  void AssignId(const std::string& id) { mID = id; }
  Fake_MediaStreamTrack* GetTrackById(const std::string& aId)
  {
    if (mHintContents & HINT_CONTENTS_AUDIO) {
      if (mAudioTrack && mAudioTrack->GetId() == aId) {
        return mAudioTrack;
      }
    }
    if (mHintContents & HINT_CONTENTS_VIDEO) {
      if (mVideoTrack && mVideoTrack->GetId() == aId) {
        return mVideoTrack;
      }
    }
    return nullptr;
  }
  Fake_MediaStreamTrack* GetOwnedTrackById(const std::string& aId)
  {
    return GetTrackById(aId);
  }

  // Hints to tell the SDP generator about whether this
  // MediaStream probably has audio and/or video
  typedef uint8_t TrackTypeHints;
  enum {
    HINT_CONTENTS_AUDIO = 0x01,
    HINT_CONTENTS_VIDEO = 0x02
  };
  uint32_t GetHintContents() const { return mHintContents; }
  void SetHintContents(uint32_t aHintContents) { mHintContents = aHintContents; }

  void
  GetTracks(nsTArray<RefPtr<Fake_MediaStreamTrack> >& aTracks)
  {
    GetAudioTracks(aTracks);
    GetVideoTracks(aTracks);
  }

  void GetAudioTracks(nsTArray<RefPtr<Fake_MediaStreamTrack> >& aTracks)
  {
    if (mHintContents & HINT_CONTENTS_AUDIO) {
      aTracks.AppendElement(mAudioTrack);
    }
  }

  void
  GetVideoTracks(nsTArray<RefPtr<Fake_MediaStreamTrack> >& aTracks)
  {
    if (mHintContents & HINT_CONTENTS_VIDEO) {
      aTracks.AppendElement(mVideoTrack);
    }
  }

  bool
  HasTrack(const Fake_MediaStreamTrack& aTrack) const
  {
    return ((mHintContents & HINT_CONTENTS_AUDIO) && aTrack.AsAudioStreamTrack()) ||
           ((mHintContents & HINT_CONTENTS_VIDEO) && aTrack.AsVideoStreamTrack());
  }

  bool
  OwnsTrack(const Fake_MediaStreamTrack& aTrack) const
  {
    return HasTrack(aTrack);
  }

  void SetTrackEnabled(mozilla::TrackID aTrackID, bool aEnabled) {}

  void AddTrackInternal(Fake_MediaStreamTrack* aTrack) {}

  already_AddRefed<Fake_MediaStreamTrack>
  CreateDOMTrack(mozilla::TrackID aTrackID, mozilla::MediaSegment::Type aType,
                 Fake_MediaStreamTrackSource* aSource)
  {
    switch(aType) {
      case mozilla::MediaSegment::AUDIO: {
        return do_AddRef(mAudioTrack);
      }
      case mozilla::MediaSegment::VIDEO: {
        return do_AddRef(mVideoTrack);
      }
      default: {
        MOZ_CRASH("Unkown media type");
      }
    }
  }

private:
  RefPtr<Fake_MediaStream> mMediaStream;

  // tells the SDP generator about whether this
  // MediaStream probably has audio and/or video
  uint32_t mHintContents;
  RefPtr<Fake_MediaStreamTrack> mVideoTrack;
  RefPtr<Fake_MediaStreamTrack> mAudioTrack;

  std::string mID;
};

class Fake_MediaStreamBase : public Fake_MediaStream {
 public:
  Fake_MediaStreamBase() : mPeriodic(new Fake_MediaPeriodic(this)) {}

  virtual nsresult Start();
  virtual nsresult Stop();

  virtual int GetSegmentsAdded() {
    return mPeriodic->GetTimesCalled();
  }

 private:
  nsCOMPtr<nsITimer> mTimer;
  RefPtr<Fake_MediaPeriodic> mPeriodic;
};


class Fake_AudioStreamSource : public Fake_MediaStreamBase {
 public:
  Fake_AudioStreamSource() : Fake_MediaStreamBase(),
                             mCount(0),
                             mStop(false) {}
  //Signaling Agent indicates us to stop generating
  //further audio.
  void StopStream() {
    mStop = true;
  }
  virtual void Periodic();
  int mCount;
  bool mStop;
};

class Fake_VideoStreamSource : public Fake_MediaStreamBase {
 public:
  Fake_VideoStreamSource() : Fake_MediaStreamBase() {}
};


namespace mozilla {
typedef Fake_MediaStream MediaStream;
typedef Fake_SourceMediaStream SourceMediaStream;
typedef Fake_MediaStreamListener MediaStreamListener;
typedef Fake_DirectMediaStreamListener DirectMediaStreamListener;
typedef Fake_MediaStreamTrackListener MediaStreamTrackListener;
typedef Fake_DirectMediaStreamTrackListener DirectMediaStreamTrackListener;
typedef Fake_DOMMediaStream DOMMediaStream;
typedef Fake_DOMMediaStream DOMLocalMediaStream;
typedef Fake_MediaStreamVideoSink MediaStreamVideoSink;

namespace dom {
typedef Fake_MediaStreamTrack MediaStreamTrack;
typedef Fake_MediaStreamTrackSource MediaStreamTrackSource;
typedef Fake_MediaStreamTrack VideoStreamTrack;
}
}

#endif
