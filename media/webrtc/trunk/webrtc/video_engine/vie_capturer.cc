/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/vie_capturer.h"

#include "webrtc/common_video/interface/texture_video_frame.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/modules/utility/interface/process_thread.h"
#include "webrtc/modules/video_capture/include/video_capture_factory.h"
#include "webrtc/modules/video_processing/main/interface/video_processing.h"
#include "webrtc/modules/video_render/include/video_render_defines.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/system_wrappers/interface/thread_wrapper.h"
#include "webrtc/system_wrappers/interface/trace_event.h"
#include "webrtc/video_engine/include/vie_image_process.h"
#include "webrtc/video_engine/overuse_frame_detector.h"
#include "webrtc/video_engine/vie_defines.h"
#include "webrtc/video_engine/vie_encoder.h"
#include "webrtc/video_engine/desktop_capture_impl.h"

namespace webrtc {

const int kThreadWaitTimeMs = 100;

ViECapturer::ViECapturer(int capture_id,
                         int engine_id,
                         const Config& config,
                         ProcessThread& module_process_thread)
    : ViEFrameProviderBase(capture_id, engine_id),
      capture_cs_(CriticalSectionWrapper::CreateCriticalSection()),
      deliver_cs_(CriticalSectionWrapper::CreateCriticalSection()),
      capture_module_(NULL),
      external_capture_module_(NULL),
      module_process_thread_(module_process_thread),
      capture_id_(capture_id),
      incoming_frame_cs_(CriticalSectionWrapper::CreateCriticalSection()),
      capture_thread_(*ThreadWrapper::CreateThread(ViECaptureThreadFunction,
                                                   this, kHighPriority,
                                                   "ViECaptureThread")),
      capture_event_(*EventWrapper::Create()),
      deliver_event_(*EventWrapper::Create()),
      effect_filter_(NULL),
      image_proc_module_(NULL),
      image_proc_module_ref_counter_(0),
      deflicker_frame_stats_(NULL),
      brightness_frame_stats_(NULL),
      current_brightness_level_(Normal),
      reported_brightness_level_(Normal),
      observer_cs_(CriticalSectionWrapper::CreateCriticalSection()),
      observer_(NULL),
      overuse_detector_(new OveruseFrameDetector(Clock::GetRealTimeClock())),
      config_(config) {
  unsigned int t_id = 0;
  if (!capture_thread_.Start(t_id)) {
    assert(false);
  }
  module_process_thread_.RegisterModule(overuse_detector_.get());
}

ViECapturer::~ViECapturer() {
  module_process_thread_.DeRegisterModule(overuse_detector_.get());

  // Stop the thread.
  deliver_cs_->Enter();
  capture_cs_->Enter();
  capture_thread_.SetNotAlive();
  capture_event_.Set();
  capture_cs_->Leave();
  deliver_cs_->Leave();

  // Stop the camera input.
  if (capture_module_) {
    module_process_thread_.DeRegisterModule(capture_module_);
    capture_module_->DeRegisterCaptureDataCallback();
    capture_module_->Release();
    capture_module_ = NULL;
  }
  if (capture_thread_.Stop()) {
    // Thread stopped.
    delete &capture_thread_;
    delete &capture_event_;
    delete &deliver_event_;
  } else {
    assert(false);
  }

  if (image_proc_module_) {
    VideoProcessingModule::Destroy(image_proc_module_);
  }
  if (deflicker_frame_stats_) {
    delete deflicker_frame_stats_;
    deflicker_frame_stats_ = NULL;
  }
  delete brightness_frame_stats_;
}

ViECapturer* ViECapturer::CreateViECapture(
    int capture_id,
    int engine_id,
    const Config& config,
    VideoCaptureModule* capture_module,
    ProcessThread& module_process_thread) {
  ViECapturer* capture = new ViECapturer(capture_id, engine_id, config,
                                         module_process_thread);
  if (!capture || capture->Init(capture_module) != 0) {
    delete capture;
    capture = NULL;
  }
  return capture;
}

int32_t ViECapturer::Init(VideoCaptureModule* capture_module) {
  assert(capture_module_ == NULL);
  capture_module_ = capture_module;
  capture_module_->RegisterCaptureDataCallback(*this);
  capture_module_->AddRef();
  if (module_process_thread_.RegisterModule(capture_module_) != 0) {
    return -1;
  }

  return 0;
}

ViECapturer* ViECapturer::CreateViECapture(
    int capture_id,
    int engine_id,
    const Config& config,
    const char* device_unique_idUTF8,
    const uint32_t device_unique_idUTF8Length,
    ProcessThread& module_process_thread) {
  ViECapturer* capture = new ViECapturer(capture_id, engine_id, config,
                                         module_process_thread);
  if (!capture ||
      capture->Init(device_unique_idUTF8, device_unique_idUTF8Length) != 0) {
    delete capture;
    capture = NULL;
  }
  return capture;
}

int32_t ViECapturer::Init(const char* device_unique_idUTF8,
                          uint32_t device_unique_idUTF8Length) {
  assert(capture_module_ == NULL);
  CaptureDeviceType type = config_.Get<CaptureDeviceInfo>().type;

  if(type != CaptureDeviceType::Camera) {
#if !defined(ANDROID)
    capture_module_ = DesktopCaptureImpl::Create(
      ViEModuleId(engine_id_, capture_id_), device_unique_idUTF8, type);
#endif
  } else if (device_unique_idUTF8 == NULL) {
    capture_module_  = VideoCaptureFactory::Create(
      ViEModuleId(engine_id_, capture_id_), external_capture_module_);
  } else {
    capture_module_ = VideoCaptureFactory::Create(
      ViEModuleId(engine_id_, capture_id_), device_unique_idUTF8);
  }
  if (!capture_module_) {
    return -1;
  }
  capture_module_->AddRef();
  capture_module_->RegisterCaptureDataCallback(*this);
  if (module_process_thread_.RegisterModule(capture_module_) != 0) {
    return -1;
  }

  return 0;
}

int ViECapturer::FrameCallbackChanged() {
  if (Started() && !CaptureCapabilityFixed()) {
    // Reconfigure the camera if a new size is required and the capture device
    // does not provide encoded frames.
    int best_width;
    int best_height;
    int best_frame_rate;
    VideoCaptureCapability capture_settings;
    capture_module_->CaptureSettings(capture_settings);
    GetBestFormat(&best_width, &best_height, &best_frame_rate);
    if (best_width != 0 && best_height != 0 && best_frame_rate != 0) {
      if (best_width != capture_settings.width ||
          best_height != capture_settings.height ||
          best_frame_rate != capture_settings.maxFPS ||
          capture_settings.codecType != kVideoCodecUnknown) {
        Stop();
        Start(requested_capability_);
      }
    }
  }
  return 0;
}

int32_t ViECapturer::Start(const CaptureCapability& capture_capability) {
  int width;
  int height;
  int frame_rate;
  VideoCaptureCapability capability;
  requested_capability_ = capture_capability;

  if (!CaptureCapabilityFixed()) {
    // Ask the observers for best size.
    GetBestFormat(&width, &height, &frame_rate);
    if (width == 0) {
      width = kViECaptureDefaultWidth;
    }
    if (height == 0) {
      height = kViECaptureDefaultHeight;
    }
    if (frame_rate == 0) {
      frame_rate = kViECaptureDefaultFramerate;
    }
    capability.height = height;
    capability.width = width;
    capability.maxFPS = frame_rate;
    capability.rawType = kVideoI420;
    capability.codecType = kVideoCodecUnknown;
  } else {
    // Width, height and type specified with call to Start, not set by
    // observers.
    capability.width = requested_capability_.width;
    capability.height = requested_capability_.height;
    capability.maxFPS = requested_capability_.maxFPS;
    capability.rawType = requested_capability_.rawType;
    capability.interlaced = requested_capability_.interlaced;
  }
  return capture_module_->StartCapture(capability);
}

int32_t ViECapturer::Stop() {
  requested_capability_ = CaptureCapability();
  return capture_module_->StopCapture();
}

bool ViECapturer::Started() {
  return capture_module_->CaptureStarted();
}

const char* ViECapturer::CurrentDeviceName() const {
  return capture_module_->CurrentDeviceName();
}

void ViECapturer::RegisterCpuOveruseObserver(CpuOveruseObserver* observer) {
  overuse_detector_->SetObserver(observer);
}

void ViECapturer::SetCpuOveruseOptions(const CpuOveruseOptions& options) {
  overuse_detector_->SetOptions(options);
}

void ViECapturer::GetCpuOveruseMetrics(CpuOveruseMetrics* metrics) const {
  overuse_detector_->GetCpuOveruseMetrics(metrics);
}

int32_t ViECapturer::SetCaptureDelay(int32_t delay_ms) {
  capture_module_->SetCaptureDelay(delay_ms);
  return 0;
}

int32_t ViECapturer::SetRotateCapturedFrames(
  const RotateCapturedFrame rotation) {
  VideoCaptureRotation converted_rotation = kCameraRotate0;
  switch (rotation) {
    case RotateCapturedFrame_0:
      converted_rotation = kCameraRotate0;
      break;
    case RotateCapturedFrame_90:
      converted_rotation = kCameraRotate90;
      break;
    case RotateCapturedFrame_180:
      converted_rotation = kCameraRotate180;
      break;
    case RotateCapturedFrame_270:
      converted_rotation = kCameraRotate270;
      break;
  }
  return capture_module_->SetCaptureRotation(converted_rotation);
}

int ViECapturer::IncomingFrame(unsigned char* video_frame,
                               unsigned int video_frame_length,
                               uint16_t width,
                               uint16_t height,
                               RawVideoType video_type,
                               unsigned long long capture_time) {  // NOLINT
  if (!external_capture_module_) {
    return -1;
  }
  VideoCaptureCapability capability;
  capability.width = width;
  capability.height = height;
  capability.rawType = video_type;
  return external_capture_module_->IncomingFrame(video_frame,
                                                 video_frame_length,
                                                 capability, capture_time);
}

int ViECapturer::IncomingFrameI420(const ViEVideoFrameI420& video_frame,
                                   unsigned long long capture_time) {  // NOLINT
  if (!external_capture_module_) {
    return -1;
  }

  int size_y = video_frame.height * video_frame.y_pitch;
  int size_u = video_frame.u_pitch * ((video_frame.height + 1) / 2);
  int size_v = video_frame.v_pitch * ((video_frame.height + 1) / 2);
  CriticalSectionScoped cs(incoming_frame_cs_.get());
  int ret = incoming_frame_.CreateFrame(size_y,
                                       video_frame.y_plane,
                                       size_u,
                                       video_frame.u_plane,
                                       size_v,
                                       video_frame.v_plane,
                                       video_frame.width,
                                       video_frame.height,
                                       video_frame.y_pitch,
                                       video_frame.u_pitch,
                                       video_frame.v_pitch);

  if (ret < 0) {
    LOG_F(LS_ERROR) << "Could not create I420Frame.";
    return -1;
  }

  return external_capture_module_->IncomingI420VideoFrame(&incoming_frame_,
                                                          capture_time);
}

void ViECapturer::SwapFrame(I420VideoFrame* frame) {
  external_capture_module_->IncomingI420VideoFrame(frame,
                                                   frame->render_time_ms());
  frame->set_timestamp(0);
  frame->set_ntp_time_ms(0);
  frame->set_render_time_ms(0);
}

void ViECapturer::OnIncomingCapturedFrame(const int32_t capture_id,
                                          I420VideoFrame& video_frame) {
  CriticalSectionScoped cs(capture_cs_.get());
  // Make sure we render this frame earlier since we know the render time set
  // is slightly off since it's being set when the frame has been received from
  // the camera, and not when the camera actually captured the frame.
  video_frame.set_render_time_ms(video_frame.render_time_ms() - FrameDelay());

  overuse_detector_->FrameCaptured(video_frame.width(),
                                   video_frame.height(),
                                   video_frame.render_time_ms());

  TRACE_EVENT_ASYNC_BEGIN1("webrtc", "Video", video_frame.render_time_ms(),
                           "render_time", video_frame.render_time_ms());

  if (video_frame.native_handle() != NULL) {
    captured_frame_.reset(video_frame.CloneFrame());
  } else {
    if (captured_frame_ == NULL || captured_frame_->native_handle() != NULL)
      captured_frame_.reset(new I420VideoFrame());
    captured_frame_->SwapFrame(&video_frame);
  }
  capture_event_.Set();
}

void ViECapturer::OnCaptureDelayChanged(const int32_t id,
                                        const int32_t delay) {
  LOG(LS_INFO) << "Capture delayed change to " << delay
               << " for device " << id;

  // Deliver the network delay to all registered callbacks.
  ViEFrameProviderBase::SetFrameDelay(delay);
}

int32_t ViECapturer::RegisterEffectFilter(
    ViEEffectFilter* effect_filter) {
  CriticalSectionScoped cs(deliver_cs_.get());

  if (effect_filter != NULL && effect_filter_ != NULL) {
    LOG_F(LS_ERROR) << "Effect filter already registered.";
    return -1;
  }
  effect_filter_ = effect_filter;
  return 0;
}

int32_t ViECapturer::IncImageProcRefCount() {
  if (!image_proc_module_) {
    assert(image_proc_module_ref_counter_ == 0);
    image_proc_module_ = VideoProcessingModule::Create(
        ViEModuleId(engine_id_, capture_id_));
    if (!image_proc_module_) {
      LOG_F(LS_ERROR) << "Could not create video processing module.";
      return -1;
    }
  }
  image_proc_module_ref_counter_++;
  return 0;
}

int32_t ViECapturer::DecImageProcRefCount() {
  image_proc_module_ref_counter_--;
  if (image_proc_module_ref_counter_ == 0) {
    // Destroy module.
    VideoProcessingModule::Destroy(image_proc_module_);
    image_proc_module_ = NULL;
  }
  return 0;
}

int32_t ViECapturer::EnableDeflickering(bool enable) {
  CriticalSectionScoped cs(deliver_cs_.get());
  if (enable) {
    if (deflicker_frame_stats_) {
      return -1;
    }
    if (IncImageProcRefCount() != 0) {
      return -1;
    }
    deflicker_frame_stats_ = new VideoProcessingModule::FrameStats();
  } else {
    if (deflicker_frame_stats_ == NULL) {
      return -1;
    }
    DecImageProcRefCount();
    delete deflicker_frame_stats_;
    deflicker_frame_stats_ = NULL;
  }
  return 0;
}

int32_t ViECapturer::EnableBrightnessAlarm(bool enable) {
  CriticalSectionScoped cs(deliver_cs_.get());
  if (enable) {
    if (brightness_frame_stats_) {
      return -1;
    }
    if (IncImageProcRefCount() != 0) {
      return -1;
    }
    brightness_frame_stats_ = new VideoProcessingModule::FrameStats();
  } else {
    DecImageProcRefCount();
    if (brightness_frame_stats_ == NULL) {
      return -1;
    }
    delete brightness_frame_stats_;
    brightness_frame_stats_ = NULL;
  }
  return 0;
}

bool ViECapturer::ViECaptureThreadFunction(void* obj) {
  return static_cast<ViECapturer*>(obj)->ViECaptureProcess();
}

bool ViECapturer::ViECaptureProcess() {
  int64_t capture_time = -1;
  if (capture_event_.Wait(kThreadWaitTimeMs) == kEventSignaled) {
    overuse_detector_->FrameProcessingStarted();
    int64_t encode_start_time = -1;
    deliver_cs_->Enter();
    if (SwapCapturedAndDeliverFrameIfAvailable()) {
      capture_time = deliver_frame_->render_time_ms();
      encode_start_time = Clock::GetRealTimeClock()->TimeInMilliseconds();
      DeliverI420Frame(deliver_frame_.get());
      if (deliver_frame_->native_handle() != NULL)
        deliver_frame_.reset();  // Release the texture so it can be reused.
    }
    deliver_cs_->Leave();
    if (current_brightness_level_ != reported_brightness_level_) {
      CriticalSectionScoped cs(observer_cs_.get());
      if (observer_) {
        observer_->BrightnessAlarm(id_, current_brightness_level_);
        reported_brightness_level_ = current_brightness_level_;
      }
    }
    // Update the overuse detector with the duration.
    if (encode_start_time != -1) {
      overuse_detector_->FrameEncoded(
          Clock::GetRealTimeClock()->TimeInMilliseconds() - encode_start_time);
    }
  }
  // We're done!
  if (capture_time != -1) {
    overuse_detector_->FrameSent(capture_time);
  }
  return true;
}

void ViECapturer::DeliverI420Frame(I420VideoFrame* video_frame) {
  if (video_frame->native_handle() != NULL) {
    ViEFrameProviderBase::DeliverFrame(video_frame);
    return;
  }

  // Apply image enhancement and effect filter.
  if (deflicker_frame_stats_) {
    if (image_proc_module_->GetFrameStats(deflicker_frame_stats_,
                                          *video_frame) == 0) {
      image_proc_module_->Deflickering(video_frame, deflicker_frame_stats_);
    } else {
      LOG_F(LS_ERROR) << "Could not get frame stats.";
    }
  }
  if (brightness_frame_stats_) {
    if (image_proc_module_->GetFrameStats(brightness_frame_stats_,
                                          *video_frame) == 0) {
      int32_t brightness = image_proc_module_->BrightnessDetection(
          *video_frame, *brightness_frame_stats_);

      switch (brightness) {
      case VideoProcessingModule::kNoWarning:
        current_brightness_level_ = Normal;
        break;
      case VideoProcessingModule::kDarkWarning:
        current_brightness_level_ = Dark;
        break;
      case VideoProcessingModule::kBrightWarning:
        current_brightness_level_ = Bright;
        break;
      default:
        break;
      }
    }
  }
  if (effect_filter_) {
    unsigned int length = CalcBufferSize(kI420,
                                         video_frame->width(),
                                         video_frame->height());
    scoped_ptr<uint8_t[]> video_buffer(new uint8_t[length]);
    ExtractBuffer(*video_frame, length, video_buffer.get());
    effect_filter_->Transform(length,
                              video_buffer.get(),
                              video_frame->ntp_time_ms(),
                              video_frame->timestamp(),
                              video_frame->width(),
                              video_frame->height());
  }
  // Deliver the captured frame to all observers (channels, renderer or file).
  ViEFrameProviderBase::DeliverFrame(video_frame);
}

int ViECapturer::DeregisterFrameCallback(
    const ViEFrameCallback* callbackObject) {
  return ViEFrameProviderBase::DeregisterFrameCallback(callbackObject);
}

bool ViECapturer::IsFrameCallbackRegistered(
    const ViEFrameCallback* callbackObject) {
  CriticalSectionScoped cs(provider_cs_.get());
  return ViEFrameProviderBase::IsFrameCallbackRegistered(callbackObject);
}

bool ViECapturer::CaptureCapabilityFixed() {
  return requested_capability_.width != 0 &&
      requested_capability_.height != 0 &&
      requested_capability_.maxFPS != 0;
}

int32_t ViECapturer::RegisterObserver(ViECaptureObserver* observer) {
  {
    CriticalSectionScoped cs(observer_cs_.get());
    if (observer_) {
      LOG_F(LS_ERROR) << "Observer already registered.";
      return -1;
    }
    observer_ = observer;
  }
  capture_module_->RegisterCaptureCallback(*this);
  capture_module_->EnableFrameRateCallback(true);
  capture_module_->EnableNoPictureAlarm(true);
  return 0;
}

int32_t ViECapturer::DeRegisterObserver() {
  capture_module_->EnableFrameRateCallback(false);
  capture_module_->EnableNoPictureAlarm(false);
  capture_module_->DeRegisterCaptureCallback();

  CriticalSectionScoped cs(observer_cs_.get());
  observer_ = NULL;
  return 0;
}

bool ViECapturer::IsObserverRegistered() {
  CriticalSectionScoped cs(observer_cs_.get());
  return observer_ != NULL;
}

void ViECapturer::OnCaptureFrameRate(const int32_t id,
                                     const uint32_t frame_rate) {
  CriticalSectionScoped cs(observer_cs_.get());
  observer_->CapturedFrameRate(id_, static_cast<uint8_t>(frame_rate));
}

void ViECapturer::OnNoPictureAlarm(const int32_t id,
                                   const VideoCaptureAlarm alarm) {
  LOG(LS_WARNING) << "OnNoPictureAlarm " << id;

  CriticalSectionScoped cs(observer_cs_.get());
  CaptureAlarm vie_alarm = (alarm == Raised) ? AlarmRaised : AlarmCleared;
  observer_->NoPictureAlarm(id, vie_alarm);
}

bool ViECapturer::SwapCapturedAndDeliverFrameIfAvailable() {
  CriticalSectionScoped cs(capture_cs_.get());
  if (captured_frame_ == NULL)
    return false;

  if (captured_frame_->native_handle() != NULL) {
    deliver_frame_.reset(captured_frame_.release());
    return true;
  }

  if (captured_frame_->IsZeroSize())
    return false;

  if (deliver_frame_ == NULL)
    deliver_frame_.reset(new I420VideoFrame());
  deliver_frame_->SwapFrame(captured_frame_.get());
  captured_frame_->ResetSize();
  return true;
}

}  // namespace webrtc
