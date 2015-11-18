/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_VIE_CAPTURER_H_
#define WEBRTC_VIDEO_ENGINE_VIE_CAPTURER_H_

#include <vector>

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/common_types.h"
#include "webrtc/engine_configurations.h"
#include "webrtc/modules/video_capture/include/video_capture.h"
#include "webrtc/modules/video_coding/codecs/interface/video_codec_interface.h"
#include "webrtc/modules/video_coding/main/interface/video_coding.h"
#include "webrtc/modules/video_processing/main/interface/video_processing.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/thread_wrapper.h"
#include "webrtc/typedefs.h"
#include "webrtc/video_engine/include/vie_base.h"
#include "webrtc/video_engine/include/vie_capture.h"
#include "webrtc/video_engine/vie_defines.h"
#include "webrtc/video_engine/vie_frame_provider_base.h"

namespace webrtc {

class Config;
class CriticalSectionWrapper;
class EventWrapper;
class CpuOveruseObserver;
class OveruseFrameDetector;
class ProcessThread;
class ViEEffectFilter;
class ViEEncoder;
struct ViEPicture;
class RegistrableCpuOveruseMetricsObserver;

class ViECapturer
    : public ViEFrameProviderBase,
      public ViEExternalCapture,
      protected VideoCaptureDataCallback,
      protected VideoCaptureFeedBack {
 public:
  static ViECapturer* CreateViECapture(int capture_id,
                                       int engine_id,
                                       const Config& config,
                                       VideoCaptureModule* capture_module,
                                       ProcessThread& module_process_thread);

  static ViECapturer* CreateViECapture(
      int capture_id,
      int engine_id,
      const Config& config,
      const char* device_unique_idUTF8,
      uint32_t device_unique_idUTF8Length,
      ProcessThread& module_process_thread);

  ~ViECapturer();

  // Implements ViEFrameProviderBase.
  int FrameCallbackChanged();

  // Implements ExternalCapture.
  void IncomingFrame(const I420VideoFrame& frame) override;

  // Start/Stop.
  int32_t Start(
      const CaptureCapability& capture_capability = CaptureCapability());
  int32_t Stop();
  bool Started();

  // Overrides the capture delay.
  int32_t SetCaptureDelay(int32_t delay_ms);

  // Sets rotation of the incoming captured frame.
  int32_t SetVideoRotation(const VideoRotation rotation);

  // Effect filter.
  int32_t RegisterEffectFilter(ViEEffectFilter* effect_filter);
  int32_t EnableDeflickering(bool enable);
  int32_t EnableBrightnessAlarm(bool enable);

  // Statistics observer.
  int32_t RegisterObserver(ViECaptureObserver* observer);
  int32_t DeRegisterObserver();
  bool IsObserverRegistered();

  // Information.
  const char* CurrentDeviceName() const;

  void RegisterCpuOveruseObserver(CpuOveruseObserver* observer);
  void SetCpuOveruseOptions(const CpuOveruseOptions& options);
  void RegisterCpuOveruseMetricsObserver(CpuOveruseMetricsObserver* observer);
  void GetCpuOveruseMetrics(CpuOveruseMetrics* metrics) const;

 protected:
  ViECapturer(int capture_id,
              int engine_id,
              const Config& config,
              ProcessThread& module_process_thread);

  int32_t Init(VideoCaptureModule* capture_module);
  int32_t Init(const char* device_unique_idUTF8,
               uint32_t device_unique_idUTF8Length);

  // Implements VideoCaptureDataCallback.
  virtual void OnIncomingCapturedFrame(const int32_t id,
                                       const I420VideoFrame& video_frame);
  virtual void OnCaptureDelayChanged(const int32_t id,
                                     const int32_t delay);

  // Returns true if the capture capability has been set in |StartCapture|
  // function and may not be changed.
  bool CaptureCapabilityFixed();

  // Help function used for keeping track of VideoImageProcesingModule.
  // Creates the module if it is needed, returns 0 on success and guarantees
  // that the image proc module exist.
  int32_t IncImageProcRefCount();
  int32_t DecImageProcRefCount();

  // Implements VideoCaptureFeedBack
  virtual void OnCaptureFrameRate(const int32_t id,
                                  const uint32_t frame_rate);
  virtual void OnNoPictureAlarm(const int32_t id,
                                const VideoCaptureAlarm alarm);

  // Thread functions for deliver captured frames to receivers.
  static bool ViECaptureThreadFunction(void* obj);
  bool ViECaptureProcess();

 private:
  void DeliverI420Frame(I420VideoFrame* video_frame);

  // Never take capture_cs_ before effects_and_stats_cs_!
  rtc::scoped_ptr<CriticalSectionWrapper> capture_cs_;
  rtc::scoped_ptr<CriticalSectionWrapper> effects_and_stats_cs_;
  VideoCaptureModule* capture_module_;
  bool use_external_capture_;
  ProcessThread& module_process_thread_;
  const int capture_id_;

  // Frame used in IncomingFrameI420.
  rtc::scoped_ptr<CriticalSectionWrapper> incoming_frame_cs_;
  I420VideoFrame incoming_frame_;

  // Capture thread.
  rtc::scoped_ptr<ThreadWrapper> capture_thread_;
  EventWrapper& capture_event_;
  EventWrapper& deliver_event_;

  volatile int stop_;

  I420VideoFrame captured_frame_ GUARDED_BY(capture_cs_.get());
  // Used to make sure incoming time stamp is increasing for every frame.
  int64_t last_captured_timestamp_;
  // Delta used for translating between NTP and internal timestamps.
  const int64_t delta_ntp_internal_ms_;

  // Image processing.
  ViEEffectFilter* effect_filter_ GUARDED_BY(effects_and_stats_cs_.get());
  VideoProcessingModule* image_proc_module_;
  int image_proc_module_ref_counter_;
  VideoProcessingModule::FrameStats* deflicker_frame_stats_
      GUARDED_BY(effects_and_stats_cs_.get());
  VideoProcessingModule::FrameStats* brightness_frame_stats_
      GUARDED_BY(effects_and_stats_cs_.get());
  Brightness current_brightness_level_;
  Brightness reported_brightness_level_;

  // Statistics observer.
  rtc::scoped_ptr<CriticalSectionWrapper> observer_cs_;
  ViECaptureObserver* observer_ GUARDED_BY(observer_cs_.get());

  CaptureCapability requested_capability_;

  // Must be declared before overuse_detector_ where it's registered.
  const rtc::scoped_ptr<RegistrableCpuOveruseMetricsObserver>
      cpu_overuse_metrics_observer_;
  rtc::scoped_ptr<OveruseFrameDetector> overuse_detector_;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_VIE_CAPTURER_H_
