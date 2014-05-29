/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/utility/source/video_frames_queue.h"

#ifdef WEBRTC_MODULE_UTILITY_VIDEO

#include <assert.h>

#include "webrtc/common_video/interface/texture_video_frame.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/system_wrappers/interface/tick_util.h"
#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc {
VideoFramesQueue::VideoFramesQueue()
    : _renderDelayMs(10)
{
}

VideoFramesQueue::~VideoFramesQueue() {
  for (FrameList::iterator iter = _incomingFrames.begin();
       iter != _incomingFrames.end(); ++iter) {
      delete *iter;
  }
  for (FrameList::iterator iter = _emptyFrames.begin();
       iter != _emptyFrames.end(); ++iter) {
      delete *iter;
  }
}

int32_t VideoFramesQueue::AddFrame(const I420VideoFrame& newFrame) {
  if (newFrame.native_handle() != NULL) {
    _incomingFrames.push_back(new TextureVideoFrame(
        static_cast<NativeHandle*>(newFrame.native_handle()),
        newFrame.width(),
        newFrame.height(),
        newFrame.timestamp(),
        newFrame.render_time_ms()));
    return 0;
  }

  I420VideoFrame* ptrFrameToAdd = NULL;
  // Try to re-use a VideoFrame. Only allocate new memory if it is necessary.
  if (!_emptyFrames.empty()) {
    ptrFrameToAdd = _emptyFrames.front();
    _emptyFrames.pop_front();
  }
  if (!ptrFrameToAdd) {
    if (_emptyFrames.size() + _incomingFrames.size() >
        KMaxNumberOfFrames) {
      WEBRTC_TRACE(kTraceWarning, kTraceVideoRenderer, -1,
                   "%s: too many frames, limit: %d", __FUNCTION__,
                   KMaxNumberOfFrames);
      return -1;
    }

    WEBRTC_TRACE(kTraceMemory, kTraceVideoRenderer, -1,
                 "%s: allocating buffer %d", __FUNCTION__,
                 _emptyFrames.size() + _incomingFrames.size());

    ptrFrameToAdd = new I420VideoFrame();
  }
  ptrFrameToAdd->CopyFrame(newFrame);
  _incomingFrames.push_back(ptrFrameToAdd);
  return 0;
}

// Find the most recent frame that has a VideoFrame::RenderTimeMs() that is
// lower than current time in ms (TickTime::MillisecondTimestamp()).
// Note _incomingFrames is sorted so that the oldest frame is first.
// Recycle all frames that are older than the most recent frame.
I420VideoFrame* VideoFramesQueue::FrameToRecord() {
  I420VideoFrame* ptrRenderFrame = NULL;
  for (FrameList::iterator iter = _incomingFrames.begin();
       iter != _incomingFrames.end(); ++iter) {
    I420VideoFrame* ptrOldestFrameInList = *iter;
    if (ptrOldestFrameInList->render_time_ms() <=
        TickTime::MillisecondTimestamp() + _renderDelayMs) {
      // List is traversed beginning to end. If ptrRenderFrame is not
      // NULL it must be the first, and thus oldest, VideoFrame in the
      // queue. It can be recycled.
      ReturnFrame(ptrRenderFrame);
      iter = _incomingFrames.erase(iter);
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
  // No need to reuse texture frames because they do not allocate memory.
  if (ptrOldFrame->native_handle() == NULL) {
    ptrOldFrame->set_timestamp(0);
    ptrOldFrame->set_width(0);
    ptrOldFrame->set_height(0);
    ptrOldFrame->set_render_time_ms(0);
    ptrOldFrame->ResetSize();
    _emptyFrames.push_back(ptrOldFrame);
  } else {
    delete ptrOldFrame;
  }
  return 0;
}

int32_t VideoFramesQueue::SetRenderDelay(uint32_t renderDelay) {
  _renderDelayMs = renderDelay;
  return 0;
}
}  // namespace webrtc
#endif // WEBRTC_MODULE_UTILITY_VIDEO
