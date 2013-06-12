/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_frames_queue.h"

#ifdef WEBRTC_MODULE_UTILITY_VIDEO

#include <cassert>

#include "module_common_types.h"
#include "tick_util.h"
#include "trace.h"

namespace webrtc {
VideoFramesQueue::VideoFramesQueue()
    : _incomingFrames(),
      _renderDelayMs(10)
{
}

VideoFramesQueue::~VideoFramesQueue() {
  while (!_incomingFrames.Empty()) {
    ListItem* item = _incomingFrames.First();
    if (item) {
      I420VideoFrame* ptrFrame = static_cast<I420VideoFrame*>(item->GetItem());
      assert(ptrFrame != NULL);
      delete ptrFrame;
    }
    _incomingFrames.Erase(item);
  }
  while (!_emptyFrames.Empty()) {
    ListItem* item = _emptyFrames.First();
    if (item) {
      I420VideoFrame* ptrFrame =
        static_cast<I420VideoFrame*>(item->GetItem());
      assert(ptrFrame != NULL);
      delete ptrFrame;
    }
    _emptyFrames.Erase(item);
  }
}

int32_t VideoFramesQueue::AddFrame(const I420VideoFrame& newFrame) {
  I420VideoFrame* ptrFrameToAdd = NULL;
  // Try to re-use a VideoFrame. Only allocate new memory if it is necessary.
  if (!_emptyFrames.Empty()) {
    ListItem* item = _emptyFrames.First();
    if (item) {
      ptrFrameToAdd = static_cast<I420VideoFrame*>(item->GetItem());
      _emptyFrames.Erase(item);
    }
  }
  if (!ptrFrameToAdd) {
    if (_emptyFrames.GetSize() + _incomingFrames.GetSize() >
        KMaxNumberOfFrames) {
      WEBRTC_TRACE(kTraceWarning, kTraceVideoRenderer, -1,
                   "%s: too many frames, limit: %d", __FUNCTION__,
                   KMaxNumberOfFrames);
      return -1;
    }

    WEBRTC_TRACE(kTraceMemory, kTraceVideoRenderer, -1,
                 "%s: allocating buffer %d", __FUNCTION__,
                 _emptyFrames.GetSize() + _incomingFrames.GetSize());

    ptrFrameToAdd = new I420VideoFrame();
    if (!ptrFrameToAdd) {
      WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, -1,
                   "%s: could not create new frame for", __FUNCTION__);
      return -1;
    }
  }
  ptrFrameToAdd->CopyFrame(newFrame);
  _incomingFrames.PushBack(ptrFrameToAdd);
  return 0;
}

// Find the most recent frame that has a VideoFrame::RenderTimeMs() that is
// lower than current time in ms (TickTime::MillisecondTimestamp()).
// Note _incomingFrames is sorted so that the oldest frame is first.
// Recycle all frames that are older than the most recent frame.
I420VideoFrame* VideoFramesQueue::FrameToRecord() {
  I420VideoFrame* ptrRenderFrame = NULL;
  ListItem* item = _incomingFrames.First();
  while(item) {
    I420VideoFrame* ptrOldestFrameInList =
        static_cast<I420VideoFrame*>(item->GetItem());
    if (ptrOldestFrameInList->render_time_ms() <=
        TickTime::MillisecondTimestamp() + _renderDelayMs) {
      if (ptrRenderFrame) {
        // List is traversed beginning to end. If ptrRenderFrame is not
        // NULL it must be the first, and thus oldest, VideoFrame in the
        // queue. It can be recycled.
        ReturnFrame(ptrRenderFrame);
        _incomingFrames.PopFront();
      }
      item = _incomingFrames.Next(item);
      ptrRenderFrame = ptrOldestFrameInList;
    } else {
      // All VideoFrames following this one will be even newer. No match
      // will be found.
      break;
    }
  }
  return ptrRenderFrame;
}

int32_t VideoFramesQueue::ReturnFrame(I420VideoFrame* ptrOldFrame) {
  ptrOldFrame->set_timestamp(0);
  ptrOldFrame->set_width(0);
  ptrOldFrame->set_height(0);
  ptrOldFrame->set_render_time_ms(0);
  ptrOldFrame->ResetSize();
  _emptyFrames.PushBack(ptrOldFrame);
  return 0;
}

int32_t VideoFramesQueue::SetRenderDelay(uint32_t renderDelay) {
  _renderDelayMs = renderDelay;
  return 0;
}
} // namespace webrtc
#endif // WEBRTC_MODULE_UTILITY_VIDEO
