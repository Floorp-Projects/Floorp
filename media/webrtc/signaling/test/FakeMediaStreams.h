/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FAKE_MEDIA_STREAM_H_
#define FAKE_MEDIA_STREAM_H_

#include <set>

#include "nsNetCID.h"
#include "nsITimer.h"
#include "nsComponentManagerUtils.h"
#include "nsIComponentManager.h"
#include "nsIComponentRegistrar.h"

// #includes from MediaStream.h
#include "mozilla/Mutex.h"
#include "AudioSegment.h"
#include "MediaSegment.h"
#include "StreamBuffer.h"
#include "nsAudioStream.h"
#include "nsTArray.h"
#include "nsIRunnable.h"
#include "nsISupportsImpl.h"
#include "nsIDOMMediaStream.h"

namespace mozilla {
   class MediaStreamGraph;
   class MediaSegment;
};

class Fake_SourceMediaStream;

class Fake_MediaStreamListener
{
public:
  virtual ~Fake_MediaStreamListener() {}

  virtual void NotifyQueuedTrackChanges(mozilla::MediaStreamGraph* aGraph, mozilla::TrackID aID,
                                        mozilla::TrackRate aTrackRate,
                                        mozilla::TrackTicks aTrackOffset,
                                        PRUint32 aTrackEvents,
                                        const mozilla::MediaSegment& aQueuedMedia)  = 0;
  virtual void NotifyPull(mozilla::MediaStreamGraph* aGraph, mozilla::StreamTime aDesiredTime) = 0;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Fake_MediaStreamListener)
};


// Note: only one listener supported
class Fake_MediaStream {
public:
  Fake_MediaStream () : mListeners() {}
  virtual ~Fake_MediaStream() { Stop(); }

  void AddListener(Fake_MediaStreamListener *aListener) {
    mListeners.insert(aListener);
  }

  void RemoveListener(Fake_MediaStreamListener *aListener) {
    mListeners.erase(aListener);
  }

  virtual Fake_SourceMediaStream *AsSourceStream() { return NULL; }

  virtual nsresult Start() { return NS_OK; }
  virtual nsresult Stop() { return NS_OK; }

  virtual void Periodic() {}

 protected:
  std::set<Fake_MediaStreamListener *> mListeners;
};

class Fake_MediaPeriodic : public nsITimerCallback {
public:
Fake_MediaPeriodic(Fake_MediaStream *aStream) : mStream(aStream),
                                                mCount(0) {}
  virtual ~Fake_MediaPeriodic() {}
  void Detach() {
    mStream = NULL;
  }

  int GetTimesCalled() { return mCount; }

  NS_DECL_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK

protected:
  Fake_MediaStream *mStream;
  int mCount;
};


class Fake_SourceMediaStream : public Fake_MediaStream {
 public:
  Fake_SourceMediaStream() : mSegmentsAdded(0),
                             mPullEnabled(false),
                             mPeriodic(new Fake_MediaPeriodic(this)) {}

  void AddTrack(mozilla::TrackID aID, mozilla::TrackRate aRate, mozilla::TrackTicks aStart,
                mozilla::MediaSegment* aSegment) {}

  void AppendToTrack(mozilla::TrackID aID, mozilla::MediaSegment* aSegment) {
    ++mSegmentsAdded;
  }

  void AdvanceKnownTracksTime(mozilla::StreamTime aKnownTime) {}

  void SetPullEnabled(bool aEnabled) {
    mPullEnabled = aEnabled;
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
  bool mPullEnabled;
  nsRefPtr<Fake_MediaPeriodic> mPeriodic;
  nsCOMPtr<nsITimer> mTimer;
};


class Fake_nsDOMMediaStream : public nsIDOMMediaStream
{
public:
  Fake_nsDOMMediaStream() : mMediaStream(new Fake_MediaStream()) {}
  Fake_nsDOMMediaStream(Fake_MediaStream *stream) :
      mMediaStream(stream) {}

  virtual ~Fake_nsDOMMediaStream() {
    // Note: memory leak
    mMediaStream->Stop();
  }

  
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMMEDIASTREAM
    
  static already_AddRefed<Fake_nsDOMMediaStream> CreateInputStream(PRUint32 aHintContents) {
    Fake_SourceMediaStream *source = new Fake_SourceMediaStream();
    
    Fake_nsDOMMediaStream *ds = new Fake_nsDOMMediaStream(source);
    ds->SetHintContents(aHintContents);
    ds->AddRef();
    
    return ds;
  }

  Fake_MediaStream *GetStream() { return mMediaStream; }

  // Hints to tell the SDP generator about whether this
  // MediaStream probably has audio and/or video
  enum {
    HINT_CONTENTS_AUDIO = 0x00000001U,
    HINT_CONTENTS_VIDEO = 0x00000002U
  };
  PRUint32 GetHintContents() const { return mHintContents; }
  void SetHintContents(PRUint32 aHintContents) { mHintContents = aHintContents; }

private:
  Fake_MediaStream *mMediaStream;

  // tells the SDP generator about whether this
  // MediaStream probably has audio and/or video
  PRUint32 mHintContents;
};

class Fake_MediaStreamGraph
{
public:
  virtual ~Fake_MediaStreamGraph() {}
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
  nsRefPtr<Fake_MediaPeriodic> mPeriodic;
};


class Fake_AudioStreamSource : public Fake_MediaStreamBase {
 public:
  Fake_AudioStreamSource() : Fake_MediaStreamBase() {}

  virtual void Periodic();
};

class Fake_VideoStreamSource : public Fake_MediaStreamBase {
 public:
  Fake_VideoStreamSource() : Fake_MediaStreamBase() {}
};


typedef Fake_nsDOMMediaStream nsDOMMediaStream;

namespace mozilla {
typedef Fake_MediaStream MediaStream;
typedef Fake_SourceMediaStream SourceMediaStream;
typedef Fake_MediaStreamListener MediaStreamListener;
}

#endif
