/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/vie_frame_provider_base.h"

#include <algorithm>

#include "webrtc/base/checks.h"
#include "webrtc/common_video/interface/i420_video_frame.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/system_wrappers/interface/tick_util.h"
#include "webrtc/video_engine/vie_defines.h"

namespace webrtc {

ViEFrameProviderBase::ViEFrameProviderBase(int Id, int engine_id)
    : id_(Id),
      engine_id_(engine_id),
      provider_cs_(CriticalSectionWrapper::CreateCriticalSection()),
      frame_delay_(0) {
  frame_delivery_thread_checker_.DetachFromThread();
}

ViEFrameProviderBase::~ViEFrameProviderBase() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // XXX!!! FIX THIS - remove our callback
  // DCHECK(frame_callbacks_.empty());

  // TODO(tommi): Remove this when we're confident we've fixed the places where
  // cleanup wasn't being done.
  for (ViEFrameCallback* callback : frame_callbacks_) {
    LOG_F(LS_WARNING) << "FrameCallback still registered.";
    callback->ProviderDestroyed(id_);
  }
}

int ViEFrameProviderBase::Id() const {
  return id_;
}

void ViEFrameProviderBase::DeliverFrame(I420VideoFrame* video_frame,
                                        const std::vector<uint32_t>& csrcs) {
  DCHECK(frame_delivery_thread_checker_.CalledOnValidThread());
#ifdef DEBUG_
  const TickTime start_process_time = TickTime::Now();
#endif
  CriticalSectionScoped cs(provider_cs_.get());

  // Deliver the frame to all registered callbacks.
  if (frame_callbacks_.size() == 1) {
    // We don't have to copy the frame.
    frame_callbacks_.front()->DeliverFrame(id_, video_frame, csrcs);
  } else {
    for (ViEFrameCallback* callback : frame_callbacks_) {
      if (video_frame->native_handle() != NULL) {
        callback->DeliverFrame(id_, video_frame, csrcs);
      } else {
        // Make a copy of the frame for all callbacks.
        if (!extra_frame_.get()) {
          extra_frame_.reset(new I420VideoFrame());
        }
        // TODO(mflodman): We can get rid of this frame copy.
        extra_frame_->CopyFrame(*video_frame);
        callback->DeliverFrame(id_, extra_frame_.get(), csrcs);
      }
    }
  }
#ifdef DEBUG_
  const int process_time =
      static_cast<int>((TickTime::Now() - start_process_time).Milliseconds());
  if (process_time > 25) {
    // Warn if the delivery time is too long.
    LOG(LS_WARNING) << "Too long time delivering frame " << process_time;
  }
#endif
}

void ViEFrameProviderBase::SetFrameDelay(int frame_delay) {
  // Called on the capture thread (see OnIncomingCapturedFrame).
  // To test, run ViEStandardIntegrationTest.RunsBaseTestWithoutErrors
  // in vie_auto_tests.
  // In the same test, it appears that it's also called on a thread that's
  // neither the ctor thread nor the capture thread.
  CriticalSectionScoped cs(provider_cs_.get());
  frame_delay_ = frame_delay;

  for (ViEFrameCallback* callback : frame_callbacks_) {
    callback->DelayChanged(id_, frame_delay);
  }
}

int ViEFrameProviderBase::FrameDelay() {
  // Called on the default thread in WebRtcVideoMediaChannelTest.SetSend
  // (libjingle_media_unittest).

  // Called on neither the ctor thread nor the capture thread in
  // BitrateEstimatorTest.ImmediatelySwitchToAST (video_engine_tests).

  // Most of the time Called on the capture thread (see OnCaptureDelayChanged).
  // To test, run ViEStandardIntegrationTest.RunsBaseTestWithoutErrors
  // in vie_auto_tests.
  return frame_delay_;
}

int ViEFrameProviderBase::GetBestFormat(int* best_width,
                                        int* best_height,
                                        int* best_frame_rate) {
  DCHECK(thread_checker_.CalledOnValidThread());
  int largest_width = 0;
  int largest_height = 0;
  int highest_frame_rate = 0;

  // Here we don't need to grab the provider_cs_ lock to run through the list
  // of callbacks.  The reason is that we know that we're currently on the same
  // thread that is the only thread that will modify the callback list and
  // we can be sure that the thread won't race with itself.
  for (ViEFrameCallback* callback : frame_callbacks_) {
    int prefered_width = 0;
    int prefered_height = 0;
    int prefered_frame_rate = 0;
    if (callback->GetPreferedFrameSettings(&prefered_width, &prefered_height,
                                           &prefered_frame_rate) == 0) {
      if (prefered_width > largest_width) {
        largest_width = prefered_width;
      }
      if (prefered_height > largest_height) {
        largest_height = prefered_height;
      }
      if (prefered_frame_rate > highest_frame_rate) {
        highest_frame_rate = prefered_frame_rate;
      }
    }
  }
  *best_width = largest_width;
  *best_height = largest_height;
  *best_frame_rate = highest_frame_rate;
  return 0;
}

int ViEFrameProviderBase::RegisterFrameCallback(
    int observer_id, ViEFrameCallback* callback_object) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(callback_object);
  {
    CriticalSectionScoped cs(provider_cs_.get());
    if (std::find(frame_callbacks_.begin(), frame_callbacks_.end(),
                  callback_object) != frame_callbacks_.end()) {
      DCHECK(false && "frameObserver already registered");
      return -1;
    }
    frame_callbacks_.push_back(callback_object);
  }
  // Report current capture delay.
  callback_object->DelayChanged(id_, frame_delay_);

  // Notify implementer of this class that the callback list have changed.
  FrameCallbackChanged();
  return 0;
}

int ViEFrameProviderBase::DeregisterFrameCallback(
    const ViEFrameCallback* callback_object) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(callback_object);
  {
    CriticalSectionScoped cs(provider_cs_.get());
    FrameCallbacks::iterator it = std::find(frame_callbacks_.begin(),
                                            frame_callbacks_.end(),
                                            callback_object);
    if (it == frame_callbacks_.end()) {
      return -1;
    }
    frame_callbacks_.erase(it);
  }

  // Notify implementer of this class that the callback list have changed.
  FrameCallbackChanged();

  return 0;
}

bool ViEFrameProviderBase::IsFrameCallbackRegistered(
    const ViEFrameCallback* callback_object) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(callback_object);

  // Here we don't need to grab the lock to do this lookup.
  // The reason is that we know that we're currently on the same thread that
  // is the only thread that will modify the callback list and subsequently the
  // thread doesn't race with itself.
  return std::find(frame_callbacks_.begin(), frame_callbacks_.end(),
                   callback_object) != frame_callbacks_.end();
}
}  // namespac webrtc
