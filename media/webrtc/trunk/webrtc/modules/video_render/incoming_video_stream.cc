/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_render//incoming_video_stream.h"

#include <cassert>

#if defined(_WIN32)
#include <windows.h>
#elif defined(WEBRTC_LINUX)
#include <ctime>
#include <sys/time.h>
#else
#include <sys/time.h>
#endif

#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/video_render//video_render_frames.h"
#include "system_wrappers/interface/critical_section_wrapper.h"
#include "system_wrappers/interface/event_wrapper.h"
#include "system_wrappers/interface/map_wrapper.h"
#include "system_wrappers/interface/thread_wrapper.h"
#include "system_wrappers/interface/tick_util.h"
#include "system_wrappers/interface/trace.h"

namespace webrtc {

IncomingVideoStream::IncomingVideoStream(const WebRtc_Word32 module_id,
                                         const WebRtc_UWord32 stream_id)
    : module_id_(module_id),
      stream_id_(stream_id),
      stream_critsect_(*CriticalSectionWrapper::CreateCriticalSection()),
      thread_critsect_(*CriticalSectionWrapper::CreateCriticalSection()),
      buffer_critsect_(*CriticalSectionWrapper::CreateCriticalSection()),
      incoming_render_thread_(),
      deliver_buffer_event_(*EventWrapper::Create()),
      running_(false),
      external_callback_(NULL),
      render_callback_(NULL),
      render_buffers_(*(new VideoRenderFrames)),
      callbackVideoType_(kVideoI420),
      callbackWidth_(0),
      callbackHeight_(0),
      incoming_rate_(0),
      last_rate_calculation_time_ms_(0),
      num_frames_since_last_calculation_(0),
      last_rendered_frame_(),
      temp_frame_(),
      start_image_(),
      timeout_image_(),
      timeout_time_(),
      mirror_frames_enabled_(false),
      mirroring_(),
      transformed_video_frame_() {
  WEBRTC_TRACE(kTraceMemory, kTraceVideoRenderer, module_id_,
               "%s created for stream %d", __FUNCTION__, stream_id);
}

IncomingVideoStream::~IncomingVideoStream() {
  WEBRTC_TRACE(kTraceMemory, kTraceVideoRenderer, module_id_,
               "%s deleted for stream %d", __FUNCTION__, stream_id_);

  Stop();

  // incoming_render_thread_ - Delete in stop
  delete &render_buffers_;
  delete &stream_critsect_;
  delete &buffer_critsect_;
  delete &thread_critsect_;
  delete &deliver_buffer_event_;
}

WebRtc_Word32 IncomingVideoStream::ChangeModuleId(const WebRtc_Word32 id) {
  CriticalSectionScoped cs(&stream_critsect_);
  module_id_ = id;
  return 0;
}

VideoRenderCallback* IncomingVideoStream::ModuleCallback() {
  CriticalSectionScoped cs(&stream_critsect_);
  return this;
}

WebRtc_Word32 IncomingVideoStream::RenderFrame(const WebRtc_UWord32 stream_id,
                                               I420VideoFrame& video_frame) {
  CriticalSectionScoped csS(&stream_critsect_);
  WEBRTC_TRACE(kTraceStream, kTraceVideoRenderer, module_id_,
               "%s for stream %d, render time: %u", __FUNCTION__, stream_id_,
               video_frame.render_time_ms());

  if (!running_) {
    WEBRTC_TRACE(kTraceStream, kTraceVideoRenderer, module_id_,
                 "%s: Not running", __FUNCTION__);
    return -1;
  }

  if (true == mirror_frames_enabled_) {
    transformed_video_frame_.CreateEmptyFrame(video_frame.width(),
                                              video_frame.height(),
                                              video_frame.stride(kYPlane),
                                              video_frame.stride(kUPlane),
                                              video_frame.stride(kVPlane));
    if (mirroring_.mirror_x_axis) {
      MirrorI420UpDown(&video_frame,
                       &transformed_video_frame_);
      video_frame.SwapFrame(&transformed_video_frame_);
    }
    if (mirroring_.mirror_y_axis) {
      MirrorI420LeftRight(&video_frame,
                          &transformed_video_frame_);
      video_frame.SwapFrame(&transformed_video_frame_);
    }
  }

  // Rate statistics.
  num_frames_since_last_calculation_++;
  WebRtc_Word64 now_ms = TickTime::MillisecondTimestamp();
  if (now_ms >= last_rate_calculation_time_ms_ + KFrameRatePeriodMs) {
    incoming_rate_ =
        static_cast<WebRtc_UWord32>(1000 * num_frames_since_last_calculation_ /
                                    (now_ms - last_rate_calculation_time_ms_));
    num_frames_since_last_calculation_ = 0;
    last_rate_calculation_time_ms_ = now_ms;
  }

  // Insert frame.
  CriticalSectionScoped csB(&buffer_critsect_);
  if (render_buffers_.AddFrame(&video_frame) == 1)
    deliver_buffer_event_.Set();

  return 0;
}

WebRtc_Word32 IncomingVideoStream::SetStartImage(
    const I420VideoFrame& video_frame) {
  CriticalSectionScoped csS(&thread_critsect_);
  return start_image_.CopyFrame(video_frame);
}

WebRtc_Word32 IncomingVideoStream::SetTimeoutImage(
    const I420VideoFrame& video_frame, const WebRtc_UWord32 timeout) {
  CriticalSectionScoped csS(&thread_critsect_);
  timeout_time_ = timeout;
  return timeout_image_.CopyFrame(video_frame);
}

WebRtc_Word32 IncomingVideoStream::SetRenderCallback(
    VideoRenderCallback* render_callback) {
  CriticalSectionScoped cs(&stream_critsect_);

  WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, module_id_,
               "%s(%x) for stream %d", __FUNCTION__, render_callback,
               stream_id_);
  render_callback_ = render_callback;
  return 0;
}

WebRtc_Word32 IncomingVideoStream::EnableMirroring(const bool enable,
                                                   const bool mirror_x_axis,
                                                   const bool mirror_y_axis) {
  CriticalSectionScoped cs(&stream_critsect_);
  mirror_frames_enabled_ = enable;
  mirroring_.mirror_x_axis = mirror_x_axis;
  mirroring_.mirror_y_axis = mirror_y_axis;

  return 0;
}

WebRtc_Word32 IncomingVideoStream::SetExpectedRenderDelay(
    WebRtc_Word32 delay_ms) {
  CriticalSectionScoped csS(&stream_critsect_);
  if (running_) {
    WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, module_id_,
                 "%s(%d) for stream %d", __FUNCTION__, delay_ms, stream_id_);
    return -1;
  }
  CriticalSectionScoped cs(&buffer_critsect_);
  return render_buffers_.SetRenderDelay(delay_ms);
}

WebRtc_Word32 IncomingVideoStream::SetExternalCallback(
    VideoRenderCallback* external_callback) {
  CriticalSectionScoped cs(&stream_critsect_);
  WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, module_id_,
               "%s(%x) for stream %d", __FUNCTION__, external_callback,
               stream_id_);
  external_callback_ = external_callback;
  callbackVideoType_ = kVideoI420;
  callbackWidth_ = 0;
  callbackHeight_ = 0;
  return 0;
}

WebRtc_Word32 IncomingVideoStream::Start() {
  CriticalSectionScoped csS(&stream_critsect_);
  WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, module_id_,
               "%s for stream %d", __FUNCTION__, stream_id_);
  if (running_) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideoRenderer, module_id_,
                 "%s: Already running", __FUNCTION__);
    return 0;
  }

  CriticalSectionScoped csT(&thread_critsect_);
  assert(incoming_render_thread_ == NULL);

  incoming_render_thread_ = ThreadWrapper::CreateThread(
      IncomingVideoStreamThreadFun, this, kRealtimePriority,
      "IncomingVideoStreamThread");
  if (!incoming_render_thread_) {
    WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, module_id_,
                 "%s: No thread", __FUNCTION__);
    return -1;
  }

  unsigned int t_id = 0;
  if (incoming_render_thread_->Start(t_id)) {
    WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, module_id_,
                 "%s: thread started: %u", __FUNCTION__, t_id);
  } else {
    WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, module_id_,
                 "%s: Could not start send thread", __FUNCTION__);
    return -1;
  }
  deliver_buffer_event_.StartTimer(false, KEventStartupTimeMS);

  running_ = true;
  return 0;
}

WebRtc_Word32 IncomingVideoStream::Stop() {
  CriticalSectionScoped cs_stream(&stream_critsect_);
  WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, module_id_,
               "%s for stream %d", __FUNCTION__, stream_id_);

  if (!running_) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideoRenderer, module_id_,
                 "%s: Not running", __FUNCTION__);
    return 0;
  }

  thread_critsect_.Enter();
  if (incoming_render_thread_) {
    ThreadWrapper* thread = incoming_render_thread_;
    incoming_render_thread_ = NULL;
    thread->SetNotAlive();
#ifndef WIN32_
    deliver_buffer_event_.StopTimer();
#endif
    thread_critsect_.Leave();
    if (thread->Stop()) {
      delete thread;
    } else {
      assert(false);
      WEBRTC_TRACE(kTraceWarning, kTraceVideoRenderer, module_id_,
                   "%s: Not able to stop thread, leaking", __FUNCTION__);
    }
  } else {
    thread_critsect_.Leave();
  }
  running_ = false;
  return 0;
}

WebRtc_Word32 IncomingVideoStream::Reset() {
  CriticalSectionScoped cs_stream(&stream_critsect_);
  CriticalSectionScoped cs_buffer(&buffer_critsect_);
  render_buffers_.ReleaseAllFrames();
  return 0;
}

WebRtc_UWord32 IncomingVideoStream::StreamId() const {
  CriticalSectionScoped cs_stream(&stream_critsect_);
  return stream_id_;
}

WebRtc_UWord32 IncomingVideoStream::IncomingRate() const {
  CriticalSectionScoped cs(&stream_critsect_);
  return incoming_rate_;
}

bool IncomingVideoStream::IncomingVideoStreamThreadFun(void* obj) {
  return static_cast<IncomingVideoStream*>(obj)->IncomingVideoStreamProcess();
}

bool IncomingVideoStream::IncomingVideoStreamProcess() {
  if (kEventError != deliver_buffer_event_.Wait(KEventMaxWaitTimeMs)) {
    if (incoming_render_thread_ == NULL) {
      // Terminating
      return false;
    }

    thread_critsect_.Enter();
    I420VideoFrame* frame_to_render = NULL;

    // Get a new frame to render and the time for the frame after this one.
    buffer_critsect_.Enter();
    frame_to_render = render_buffers_.FrameToRender();
    WebRtc_UWord32 wait_time = render_buffers_.TimeToNextFrameRelease();
    buffer_critsect_.Leave();

    // Set timer for next frame to render.
    if (wait_time > KEventMaxWaitTimeMs) {
      wait_time = KEventMaxWaitTimeMs;
    }
    deliver_buffer_event_.StartTimer(false, wait_time);

    if (!frame_to_render) {
      if (render_callback_) {
        if (last_rendered_frame_.render_time_ms() == 0 &&
            !start_image_.IsZeroSize()) {
          // We have not rendered anything and have a start image.
          temp_frame_.CopyFrame(start_image_);
          render_callback_->RenderFrame(stream_id_, temp_frame_);
        } else if (!timeout_image_.IsZeroSize() &&
                   last_rendered_frame_.render_time_ms() + timeout_time_ <
                       TickTime::MillisecondTimestamp()) {
          // Render a timeout image.
          temp_frame_.CopyFrame(timeout_image_);
          render_callback_->RenderFrame(stream_id_, temp_frame_);
        }
      }

      // No frame.
      thread_critsect_.Leave();
      return true;
    }

    // Send frame for rendering.
    if (external_callback_) {
      WEBRTC_TRACE(kTraceStream, kTraceVideoRenderer, module_id_,
                   "%s: executing external renderer callback to deliver frame",
                   __FUNCTION__, frame_to_render->render_time_ms());
      external_callback_->RenderFrame(stream_id_, *frame_to_render);
    } else {
      if (render_callback_) {
        WEBRTC_TRACE(kTraceStream, kTraceVideoRenderer, module_id_,
                     "%s: Render frame, time: ", __FUNCTION__,
                     frame_to_render->render_time_ms());
        render_callback_->RenderFrame(stream_id_, *frame_to_render);
      }
    }

    // Release critsect before calling the module user.
    thread_critsect_.Leave();

    // We're done with this frame, delete it.
    if (frame_to_render) {
      CriticalSectionScoped cs(&buffer_critsect_);
      last_rendered_frame_.SwapFrame(frame_to_render);
      render_buffers_.ReturnFrame(frame_to_render);
    }
  }
  return true;
}

WebRtc_Word32 IncomingVideoStream::GetLastRenderedFrame(
    I420VideoFrame& video_frame) const {
  CriticalSectionScoped cs(&buffer_critsect_);
  return video_frame.CopyFrame(last_rendered_frame_);
}

}  // namespace webrtc
