/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_engine/vie_frame_provider_base.h"

#include <algorithm>

#include "modules/interface/module_common_types.h"
#include "system_wrappers/interface/critical_section_wrapper.h"
#include "system_wrappers/interface/tick_util.h"
#include "system_wrappers/interface/trace.h"
#include "video_engine/vie_defines.h"

namespace webrtc {

ViEFrameProviderBase::ViEFrameProviderBase(int Id, int engine_id)
    : id_(Id),
      engine_id_(engine_id),
      provider_cs_(CriticalSectionWrapper::CreateCriticalSection()),
      frame_delay_(0) {
}

ViEFrameProviderBase::~ViEFrameProviderBase() {
  if (frame_callbacks_.size() > 0) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(engine_id_, id_),
                 "FrameCallbacks still exist when Provider deleted %d",
                 frame_callbacks_.size());
  }

  for (FrameCallbacks::iterator it = frame_callbacks_.begin();
       it != frame_callbacks_.end(); ++it) {
    (*it)->ProviderDestroyed(id_);
  }
  frame_callbacks_.clear();
}

int ViEFrameProviderBase::Id() {
  return id_;
}

void ViEFrameProviderBase::DeliverFrame(
    VideoFrame& video_frame,
    int num_csrcs,
    const WebRtc_UWord32 CSRC[kRtpCsrcSize]) {
#ifdef DEBUG_
  const TickTime start_process_time = TickTime::Now();
#endif
  CriticalSectionScoped cs(provider_cs_.get());

  // Deliver the frame to all registered callbacks.
  if (frame_callbacks_.size() > 0) {
    if (frame_callbacks_.size() == 1) {
      // We don't have to copy the frame.
      frame_callbacks_.front()->DeliverFrame(id_, video_frame, num_csrcs, CSRC);
    } else {
      // Make a copy of the frame for all callbacks.callback
      for (FrameCallbacks::iterator it = frame_callbacks_.begin();
           it != frame_callbacks_.end(); ++it) {
        if (!extra_frame_.get()) {
          extra_frame_.reset(new VideoFrame());
        }
        extra_frame_->CopyFrame(video_frame);
        (*it)->DeliverFrame(id_, *(extra_frame_.get()), num_csrcs, CSRC);
      }
    }
  }
#ifdef DEBUG_
  const int process_time =
      static_cast<int>((TickTime::Now() - start_process_time).Milliseconds());
  if (process_time > 25) {
    // Warn if the delivery time is too long.
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(engine_id_, id_),
                 "%s Too long time: %ums", __FUNCTION__, process_time);
  }
#endif
}

void ViEFrameProviderBase::SetFrameDelay(int frame_delay) {
  CriticalSectionScoped cs(provider_cs_.get());
  frame_delay_ = frame_delay;

  for (FrameCallbacks::iterator it = frame_callbacks_.begin();
       it != frame_callbacks_.end(); ++it) {
    (*it)->DelayChanged(id_, frame_delay);
  }
}

int ViEFrameProviderBase::FrameDelay() {
  return frame_delay_;
}

int ViEFrameProviderBase::GetBestFormat(int& best_width,
                                        int& best_height,
                                        int& best_frame_rate) {
  int largest_width = 0;
  int largest_height = 0;
  int highest_frame_rate = 0;

  CriticalSectionScoped cs(provider_cs_.get());
  for (FrameCallbacks::iterator it = frame_callbacks_.begin();
       it != frame_callbacks_.end(); ++it) {
    int prefered_width = 0;
    int prefered_height = 0;
    int prefered_frame_rate = 0;
    if ((*it)->GetPreferedFrameSettings(prefered_width, prefered_height,
                                        prefered_frame_rate) == 0) {
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
  best_width = largest_width;
  best_height = largest_height;
  best_frame_rate = highest_frame_rate;
  return 0;
}

int ViEFrameProviderBase::RegisterFrameCallback(
    int observer_id, ViEFrameCallback* callback_object) {
  assert(callback_object);
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, id_), "%s(0x%p)",
               __FUNCTION__, callback_object);
  {
    CriticalSectionScoped cs(provider_cs_.get());
    if (std::find(frame_callbacks_.begin(), frame_callbacks_.end(),
                  callback_object) != frame_callbacks_.end()) {
      // This object is already registered.
      WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(engine_id_, id_),
                   "%s 0x%p already registered", __FUNCTION__,
                   callback_object);
      assert(false && "frameObserver already registered");
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
  assert(callback_object);
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, id_), "%s(0x%p)",
               __FUNCTION__, callback_object);
  CriticalSectionScoped cs(provider_cs_.get());

  FrameCallbacks::iterator it = std::find(frame_callbacks_.begin(),
                                          frame_callbacks_.end(),
                                          callback_object);
  if (it == frame_callbacks_.end()) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(engine_id_, id_),
                 "%s 0x%p not found", __FUNCTION__, callback_object);
    return -1;
  }
  frame_callbacks_.erase(it);
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, id_),
               "%s 0x%p deregistered", __FUNCTION__, callback_object);

  // Notify implementer of this class that the callback list have changed.
  FrameCallbackChanged();
  return 0;
}

bool ViEFrameProviderBase::IsFrameCallbackRegistered(
    const ViEFrameCallback* callback_object) {
  assert(callback_object);
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, id_),
               "%s(0x%p)", __FUNCTION__, callback_object);

  CriticalSectionScoped cs(provider_cs_.get());
  return std::find(frame_callbacks_.begin(), frame_callbacks_.end(),
                   callback_object) != frame_callbacks_.end();
}

int ViEFrameProviderBase::NumberOfRegisteredFrameCallbacks() {
  CriticalSectionScoped cs(provider_cs_.get());
  return frame_callbacks_.size();
}

}  // namespac webrtc
