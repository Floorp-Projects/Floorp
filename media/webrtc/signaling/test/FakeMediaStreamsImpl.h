/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef FAKE_MEDIA_STREAMIMPL_H_
#define FAKE_MEDIA_STREAMIMPL_H_

#include "FakeMediaStreams.h"

#include "nspr.h"
#include "nsError.h"

NS_IMPL_THREADSAFE_ISUPPORTS1(Fake_nsDOMMediaStream, nsIDOMMediaStream)

// DOM Media stream
NS_IMETHODIMP
Fake_nsDOMMediaStream::GetCurrentTime(double *aCurrentTime)
{
  PR_ASSERT(PR_FALSE);

  *aCurrentTime = 0;
  return NS_OK;
}

// Fake_SourceMediaStream
nsresult Fake_SourceMediaStream::Start() {
  mTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
  if (!mTimer) {
    return NS_ERROR_FAILURE;
  }

  mTimer->InitWithCallback(mPeriodic, 100, nsITimer::TYPE_REPEATING_SLACK);

  return NS_OK;
}

nsresult Fake_SourceMediaStream::Stop() {
  if (mTimer)
    mTimer->Cancel();
  mPeriodic->Detach();
  return NS_OK;
}

void Fake_SourceMediaStream::Periodic() {
  if (mPullEnabled) {
    for (std::set<Fake_MediaStreamListener *>::iterator it =
             mListeners.begin(); it != mListeners.end(); ++it) {
      (*it)->NotifyPull(NULL, mozilla::MillisecondsToMediaTime(10));
    }
  }
}


// Fake_MediaStreamBase
nsresult Fake_MediaStreamBase::Start() {
  mTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
  if (!mTimer) {
    return NS_ERROR_FAILURE;
  }

  mTimer->InitWithCallback(mPeriodic, 100, nsITimer::TYPE_REPEATING_SLACK);

  return NS_OK;
}

nsresult Fake_MediaStreamBase::Stop() {
  mTimer->Cancel();

  return NS_OK;
}

// Fake_AudioStreamSource
void Fake_AudioStreamSource::Periodic() {
  mozilla::AudioSegment segment;
  segment.Init(1);
  segment.InsertNullDataAtStart(160);

  for (std::set<Fake_MediaStreamListener *>::iterator it = mListeners.begin();
       it != mListeners.end(); ++it) {
    (*it)->NotifyQueuedTrackChanges(NULL, // Graph
                                    0, // TrackID
                                    16000, // Rate (hz)
                                    0, // Offset TODO(ekr@rtfm.com) fix
                                    0, // ???
                                    segment);
  }
}


// Fake_MediaPeriodic
NS_IMPL_THREADSAFE_ISUPPORTS1(Fake_MediaPeriodic, nsITimerCallback)

NS_IMETHODIMP
Fake_MediaPeriodic::Notify(nsITimer *timer) {
  if (mStream)
    mStream->Periodic();
  ++mCount;
  return NS_OK;
}


#if 0
#define WIDTH 320
#define HEIGHT 240
#define RATE USECS_PER_S
#define USECS_PER_S 1000000
#define FPS 10

NS_IMETHODIMP
Fake_VideoStreamSource::Notify(nsITimer* aTimer)
{
#if 0
  mozilla::layers::BufferRecycleBin bin;

  nsRefPtr<mozilla::layers::PlanarYCbCrImage> image = new
    mozilla::layers::PlanarYCbCrImage(&bin);

  const uint8_t lumaBpp = 8;
  const uint8_t chromaBpp = 4;

  int len = ((WIDTH * HEIGHT) * 3 / 2);
  uint8_t* frame = (uint8_t*) PR_Malloc(len);
  memset(frame, 0x80, len); // Gray

  mozilla::layers::PlanarYCbCrImage::Data data;
  data.mYChannel = frame;
  data.mYSize = gfxIntSize(WIDTH, HEIGHT);
  data.mYStride = WIDTH * lumaBpp / 8.0;
  data.mCbCrStride = WIDTH * chromaBpp / 8.0;
  data.mCbChannel = frame + HEIGHT * data.mYStride;
  data.mCrChannel = data.mCbChannel + HEIGHT * data.mCbCrStride / 2;
  data.mCbCrSize = gfxIntSize(WIDTH / 2, HEIGHT / 2);
  data.mPicX = 0;
  data.mPicY = 0;
  data.mPicSize = gfxIntSize(WIDTH, HEIGHT);
  data.mStereoMode = mozilla::layers::STEREO_MODE_MONO;

  mozilla::VideoSegment segment;
  segment.AppendFrame(image.forget(), USECS_PER_S / FPS, gfxIntSize(WIDTH, HEIGHT));

  // TODO(ekr@rtfm.com): are we leaking?
#endif

  return NS_OK;
}


#if 0
// Fake up buffer recycle bin
mozilla::layers::BufferRecycleBin::BufferRecycleBin() : 
 mLock("mozilla.layers.BufferRecycleBin.mLock") {
}

void mozilla::layers::BufferRecycleBin::RecycleBuffer(uint8_t* buffer, uint32_t size) {
  PR_Free(buffer);
}

uint8_t *mozilla::layers::BufferRecycleBin::GetBuffer(uint32_t size) {
  return (uint8_t *)PR_MALLOC(size);
}

// YCbCrImage constructor (from ImageLayers.cpp)
mozilla::layers::PlanarYCbCrImage::PlanarYCbCrImage(BufferRecycleBin *aRecycleBin)
  : Image(nsnull, PLANAR_YCBCR)
  , mBufferSize(0)
  , mRecycleBin(aRecycleBin)
{
}


#endif
#endif


#endif
