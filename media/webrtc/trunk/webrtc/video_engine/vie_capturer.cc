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
#include "webrtc/system_wrappers/interface/tick_util.h"
#include "webrtc/system_wrappers/interface/trace_event.h"
#include "webrtc/video_engine/include/vie_image_process.h"
#include "webrtc/video_engine/overuse_frame_detector.h"
#include "webrtc/video_engine/vie_defines.h"
#include "webrtc/video_engine/vie_encoder.h"
#include "webrtc/video_engine/desktop_capture_impl.h"

namespace webrtc {

const int kThreadWaitTimeMs = 100;

class RegistrableCpuOveruseMetricsObserver : public CpuOveruseMetricsObserver {
 public:
  void CpuOveruseMetricsUpdated(const CpuOveruseMetrics& metrics) override {
    rtc::CritScope lock(&crit_);
    if (observer_)
      observer_->CpuOveruseMetricsUpdated(metrics);
    metrics_ = metrics;
  }

  CpuOveruseMetrics GetCpuOveruseMetrics() const {
    rtc::CritScope lock(&crit_);
    return metrics_;
  }

  void Set(CpuOveruseMetricsObserver* observer) {
    rtc::CritScope lock(&crit_);
    observer_ = observer;
  }

 private:
  mutable rtc::CriticalSection crit_;
  CpuOveruseMetricsObserver* observer_ GUARDED_BY(crit_) = nullptr;
  CpuOveruseMetrics metrics_ GUARDED_BY(crit_);
};

ViECapturer::ViECapturer(int capture_id,
                         int engine_id,
                         const Config& config,
                         ProcessThread& module_process_thread)
    : ViEFrameProviderBase(capture_id, engine_id),
      capture_cs_(CriticalSectionWrapper::CreateCriticalSection()),
      effects_and_stats_cs_(CriticalSectionWrapper::CreateCriticalSection()),
      capture_module_(NULL),
      use_external_capture_(false),
      module_process_thread_(module_process_thread),
      capture_id_(capture_id),
      incoming_frame_cs_(CriticalSectionWrapper::CreateCriticalSection()),
      capture_thread_(ThreadWrapper::CreateThread(
          ViECaptureThreadFunction, this, "ViECaptureThread")),
      capture_event_(*EventWrapper::Create()),
      deliver_event_(*EventWrapper::Create()),
      stop_(0),
      last_captured_timestamp_(0),
      delta_ntp_internal_ms_(
          Clock::GetRealTimeClock()->CurrentNtpInMilliseconds() -
          TickTime::MillisecondTimestamp()),
      effect_filter_(NULL),
      image_proc_module_(NULL),
      image_proc_module_ref_counter_(0),
      deflicker_frame_stats_(NULL),
      brightness_frame_stats_(NULL),
      current_brightness_level_(Normal),
      reported_brightness_level_(Normal),
      observer_cs_(CriticalSectionWrapper::CreateCriticalSection()),
      observer_(NULL),
      cpu_overuse_metrics_observer_(new RegistrableCpuOveruseMetricsObserver()),
      overuse_detector_(
          new OveruseFrameDetector(Clock::GetRealTimeClock(),
                                   cpu_overuse_metrics_observer_.get())),
      config_(config) {
  capture_thread_->Start();
  capture_thread_->SetPriority(kHighPriority);
  module_process_thread_.RegisterModule(overuse_detector_.get());
}

ViECapturer::~ViECapturer() {
  module_process_thread_.DeRegisterModule(overuse_detector_.get());

  // Stop the thread.
  rtc::AtomicOps::Increment(&stop_);
  capture_event_.Set();

  // Stop the camera input.
  if (capture_module_) {
    module_process_thread_.DeRegisterModule(capture_module_);
    capture_module_->DeRegisterCaptureDataCallback();
    capture_module_->Release();
    capture_module_ = NULL;
  }

  capture_thread_->Stop();
  delete &capture_event_;
  delete &deliver_event_;

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
  module_process_thread_.RegisterModule(capture_module_);
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
#if !defined(ANDROID) && !defined(WEBRTC_IOS)
    capture_module_ = DesktopCaptureImpl::Create(
      ViEModuleId(engine_id_, capture_id_), device_unique_idUTF8, type);
#endif
  } else if (device_unique_idUTF8 == NULL) {
    use_external_capture_ = true;
    // YYY: was      ViEModuleId(engine_id_, capture_id_), external_capture_module_);
    return 0;
  } else {
    capture_module_ = VideoCaptureFactory::Create(
      ViEModuleId(engine_id_, capture_id_), device_unique_idUTF8);
  }
  if (!capture_module_) {
    return -1;
  }
  capture_module_->AddRef();
  capture_module_->RegisterCaptureDataCallback(*this);
  module_process_thread_.RegisterModule(capture_module_);

  return 0;
}

int ViECapturer::FrameCallbackChanged() {
  if (use_external_capture_)
    return -1;
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
  if (use_external_capture_)
    return -1;
  int width;
  int height;
  int frame_rate;
  VideoCaptureCapability capability;
  requested_capability_ = capture_capability;
  CaptureDeviceType type = config_.Get<CaptureDeviceInfo>().type;

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
      if (type == Screen || type == Window || type == Application) {
        frame_rate = kViEScreenCaptureDefaultFramerate;
      } else {
        frame_rate = kViECaptureDefaultFramerate;
      }
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
  if (use_external_capture_)
    return -1;
  requested_capability_ = CaptureCapability();
  return capture_module_->StopCapture();
}

bool ViECapturer::Started() {
  if (use_external_capture_)
    return false;
  return capture_module_->CaptureStarted();
}

const char* ViECapturer::CurrentDeviceName() const {
  if (use_external_capture_)
    return "";
  return capture_module_->CurrentDeviceName();
}

void ViECapturer::RegisterCpuOveruseObserver(CpuOveruseObserver* observer) {
  overuse_detector_->SetObserver(observer);
}

void ViECapturer::SetCpuOveruseOptions(const CpuOveruseOptions& options) {
  overuse_detector_->SetOptions(options);
}

void ViECapturer::RegisterCpuOveruseMetricsObserver(
    CpuOveruseMetricsObserver* observer) {
  cpu_overuse_metrics_observer_->Set(observer);
}

void ViECapturer::GetCpuOveruseMetrics(CpuOveruseMetrics* metrics) const {
  *metrics = cpu_overuse_metrics_observer_->GetCpuOveruseMetrics();
}

int32_t ViECapturer::SetCaptureDelay(int32_t delay_ms) {
  if (use_external_capture_)
    return -1;
  capture_module_->SetCaptureDelay(delay_ms);
  return 0;
}

int32_t ViECapturer::SetVideoRotation(const VideoRotation rotation) {
  if (use_external_capture_)
    return -1;
  return capture_module_->SetCaptureRotation(rotation);
}

void ViECapturer::IncomingFrame(const I420VideoFrame& frame) {
  OnIncomingCapturedFrame(-1, frame);
}

void ViECapturer::OnIncomingCapturedFrame(const int32_t capture_id,
                                          const I420VideoFrame& video_frame) {
  I420VideoFrame incoming_frame = video_frame;

  if (incoming_frame.ntp_time_ms() != 0) {
    // If a NTP time stamp is set, this is the time stamp we will use.
    incoming_frame.set_render_time_ms(
        incoming_frame.ntp_time_ms() - delta_ntp_internal_ms_);
  } else {  // NTP time stamp not set.
    int64_t render_time = incoming_frame.render_time_ms() != 0 ?
        incoming_frame.render_time_ms() : TickTime::MillisecondTimestamp();

    // Make sure we render this frame earlier since we know the render time set
    // is slightly off since it's being set when the frame was received
    // from the camera, and not when the camera actually captured the frame.
    render_time -= FrameDelay();
    incoming_frame.set_render_time_ms(render_time);
    incoming_frame.set_ntp_time_ms(
        render_time + delta_ntp_internal_ms_);
  }

  // Convert NTP time, in ms, to RTP timestamp.
  const int kMsToRtpTimestamp = 90;
  incoming_frame.set_timestamp(kMsToRtpTimestamp *
      static_cast<uint32_t>(incoming_frame.ntp_time_ms()));

  CriticalSectionScoped cs(capture_cs_.get());
  if (incoming_frame.ntp_time_ms() <= last_captured_timestamp_) {
    // We don't allow the same capture time for two frames, drop this one.
    LOG(LS_WARNING) << "Same/old NTP timestamp for incoming frame. Dropping.";
    return;
  }

  captured_frame_.ShallowCopy(incoming_frame);
  last_captured_timestamp_ = incoming_frame.ntp_time_ms();

  overuse_detector_->FrameCaptured(captured_frame_.width(),
                                   captured_frame_.height(),
                                   captured_frame_.render_time_ms());

  TRACE_EVENT_ASYNC_BEGIN1("webrtc", "Video", video_frame.render_time_ms(),
                           "render_time", video_frame.render_time_ms());

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
  CriticalSectionScoped cs(effects_and_stats_cs_.get());

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
  CriticalSectionScoped cs(effects_and_stats_cs_.get());
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
  CriticalSectionScoped cs(effects_and_stats_cs_.get());
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
    if (rtc::AtomicOps::Load(&stop_))
      return false;

    overuse_detector_->FrameProcessingStarted();
    int64_t encode_start_time = -1;
    I420VideoFrame deliver_frame;
    {
      CriticalSectionScoped cs(capture_cs_.get());
      if (!captured_frame_.IsZeroSize()) {
        deliver_frame = captured_frame_;
        captured_frame_.Reset();
      }
    }
    if (!deliver_frame.IsZeroSize()) {
      capture_time = deliver_frame.render_time_ms();
      encode_start_time = Clock::GetRealTimeClock()->TimeInMilliseconds();
      DeliverI420Frame(&deliver_frame);
    }
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
    ViEFrameProviderBase::DeliverFrame(video_frame, std::vector<uint32_t>());
    return;
  }

  // Apply image enhancement and effect filter.
  {
    CriticalSectionScoped cs(effects_and_stats_cs_.get());
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
      size_t length =
          CalcBufferSize(kI420, video_frame->width(), video_frame->height());
      rtc::scoped_ptr<uint8_t[]> video_buffer(new uint8_t[length]);
      ExtractBuffer(*video_frame, length, video_buffer.get());
      effect_filter_->Transform(length,
                                video_buffer.get(),
                                video_frame->ntp_time_ms(),
                                video_frame->timestamp(),
                                video_frame->width(),
                                video_frame->height());
    }
  }
  // Deliver the captured frame to all observers (channels, renderer or file).
  ViEFrameProviderBase::DeliverFrame(video_frame, std::vector<uint32_t>());
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

}  // namespace webrtc
