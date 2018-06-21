/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_capture/video_capture_impl.h"

#include <stdlib.h>
#include <string>

#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/video_capture/video_capture_config.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"
#include "webrtc/system_wrappers/include/trace.h"
#include "webrtc/base/trace_event.h"
#include "webrtc/video_engine/desktop_capture_impl.h"
#include "webrtc/modules/desktop_capture/desktop_frame.h"
#include "webrtc/modules/desktop_capture/desktop_device_info.h"
#include "webrtc/modules/desktop_capture/app_capturer.h"
#include "webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "webrtc/modules/video_capture/video_capture.h"

namespace webrtc {

ScreenDeviceInfoImpl::ScreenDeviceInfoImpl(const int32_t id) : _id(id) {
}

ScreenDeviceInfoImpl::~ScreenDeviceInfoImpl(void) {
}

int32_t ScreenDeviceInfoImpl::Init() {
  desktop_device_info_ = std::unique_ptr<DesktopDeviceInfo>(DesktopDeviceInfoImpl::Create());
  return 0;
}

int32_t ScreenDeviceInfoImpl::Refresh() {
  desktop_device_info_->Refresh();
  return 0;
}

uint32_t ScreenDeviceInfoImpl::NumberOfDevices() {
  return desktop_device_info_->getDisplayDeviceCount();
}

int32_t ScreenDeviceInfoImpl::GetDeviceName(uint32_t deviceNumber,
                                            char* deviceNameUTF8,
                                            uint32_t deviceNameUTF8Length,
                                            char* deviceUniqueIdUTF8,
                                            uint32_t deviceUniqueIdUTF8Length,
                                            char* productUniqueIdUTF8,
                                            uint32_t productUniqueIdUTF8Length,
                                            pid_t* pid) {

  DesktopDisplayDevice desktopDisplayDevice;

  // always initialize output
  if (deviceNameUTF8 && deviceNameUTF8Length > 0) {
    memset(deviceNameUTF8, 0, deviceNameUTF8Length);
  }

  if (deviceUniqueIdUTF8 && deviceUniqueIdUTF8Length > 0) {
    memset(deviceUniqueIdUTF8, 0, deviceUniqueIdUTF8Length);
  }
  if (productUniqueIdUTF8 && productUniqueIdUTF8Length > 0) {
    memset(productUniqueIdUTF8, 0, productUniqueIdUTF8Length);
  }

  if (desktop_device_info_->getDesktopDisplayDeviceInfo(deviceNumber,
                                                       desktopDisplayDevice) == 0) {
    size_t len;

    const char *deviceName = desktopDisplayDevice.getDeviceName();
    len = deviceName ? strlen(deviceName) : 0;
    if (len && deviceNameUTF8 && len <= deviceNameUTF8Length) {
      memcpy(deviceNameUTF8,
             deviceName,
             len);
    }

    const char *deviceUniqueId = desktopDisplayDevice.getUniqueIdName();
    len = deviceUniqueId ? strlen(deviceUniqueId) : 0;
    if (len && deviceUniqueIdUTF8 && len <= deviceUniqueIdUTF8Length) {
      memcpy(deviceUniqueIdUTF8,
             deviceUniqueId,
             len);
    }
  }

  return 0;
}

int32_t ScreenDeviceInfoImpl::DisplayCaptureSettingsDialogBox(const char* deviceUniqueIdUTF8,
                                                              const char* dialogTitleUTF8,
                                                              void* parentWindow,
                                                              uint32_t positionX,
                                                              uint32_t positionY) {
  // no device properties to change
  return 0;
}

int32_t ScreenDeviceInfoImpl::NumberOfCapabilities(const char* deviceUniqueIdUTF8) {
  return 0;
}

int32_t ScreenDeviceInfoImpl::GetCapability(const char* deviceUniqueIdUTF8,
                                            const uint32_t deviceCapabilityNumber,
                                            VideoCaptureCapability& capability) {
  return 0;
}

int32_t ScreenDeviceInfoImpl::GetBestMatchedCapability(const char* deviceUniqueIdUTF8,
                                                       const VideoCaptureCapability& requested,
                                                       VideoCaptureCapability& resulting) {
  return 0;
}

int32_t ScreenDeviceInfoImpl::GetOrientation(const char* deviceUniqueIdUTF8,
                                             VideoRotation& orientation) {
  return 0;
}

AppDeviceInfoImpl::AppDeviceInfoImpl(const int32_t id) {
}

AppDeviceInfoImpl::~AppDeviceInfoImpl(void) {
}

int32_t AppDeviceInfoImpl::Init() {
  desktop_device_info_ = std::unique_ptr<DesktopDeviceInfo>(DesktopDeviceInfoImpl::Create());
  return 0;
}

int32_t AppDeviceInfoImpl::Refresh() {
  desktop_device_info_->Refresh();
  return 0;
}

uint32_t AppDeviceInfoImpl::NumberOfDevices() {
  return desktop_device_info_->getApplicationCount();
}

int32_t AppDeviceInfoImpl::GetDeviceName(uint32_t deviceNumber,
                                         char* deviceNameUTF8,
                                         uint32_t deviceNameUTF8Length,
                                         char* deviceUniqueIdUTF8,
                                         uint32_t deviceUniqueIdUTF8Length,
                                         char* productUniqueIdUTF8,
                                         uint32_t productUniqueIdUTF8Length,
                                         pid_t* pid) {

  DesktopApplication desktopApplication;

  // always initialize output
  if (deviceNameUTF8Length && deviceNameUTF8Length > 0) {
    memset(deviceNameUTF8, 0, deviceNameUTF8Length);
  }
  if (deviceUniqueIdUTF8 && deviceUniqueIdUTF8Length > 0) {
    memset(deviceUniqueIdUTF8, 0, deviceUniqueIdUTF8Length);
  }
  if (productUniqueIdUTF8 && productUniqueIdUTF8Length > 0) {
    memset(productUniqueIdUTF8, 0, productUniqueIdUTF8Length);
  }

  if (desktop_device_info_->getApplicationInfo(deviceNumber,desktopApplication) == 0) {
    size_t len;

    const char *deviceName = desktopApplication.getProcessAppName();
    len = deviceName ? strlen(deviceName) : 0;
    if (len && len <= deviceNameUTF8Length) {
      memcpy(deviceNameUTF8, deviceName, len);
    }

    const char *deviceUniqueId = desktopApplication.getUniqueIdName();
    len = deviceUniqueId ? strlen(deviceUniqueId) : 0;
    if (len && deviceUniqueIdUTF8 && len <= deviceUniqueIdUTF8Length) {
      memcpy(deviceUniqueIdUTF8,
             deviceUniqueId,
             len);
    }
    if (pid) {
      *pid = desktopApplication.getProcessId();
    }
  }
  return 0;
}

int32_t AppDeviceInfoImpl::DisplayCaptureSettingsDialogBox(const char* deviceUniqueIdUTF8,
                                                           const char* dialogTitleUTF8,
                                                           void* parentWindow,
                                                           uint32_t positionX,
                                                           uint32_t positionY) {
  return 0;
}

int32_t AppDeviceInfoImpl::NumberOfCapabilities(const char* deviceUniqueIdUTF8) {
  return 0;
}

int32_t AppDeviceInfoImpl::GetCapability(
                                         const char* deviceUniqueIdUTF8,
                                         const uint32_t deviceCapabilityNumber,
                                         VideoCaptureCapability& capability) {
  return 0;
}

int32_t AppDeviceInfoImpl::GetBestMatchedCapability(
                                                    const char* deviceUniqueIdUTF8,
                                                    const VideoCaptureCapability& requested,
                                                    VideoCaptureCapability& resulting) {
  return 0;
}

int32_t AppDeviceInfoImpl::GetOrientation(const char* deviceUniqueIdUTF8,
                                          VideoRotation& orientation) {
  return 0;
}

VideoCaptureModule* DesktopCaptureImpl::Create(const int32_t id,
                                               const char* uniqueId,
                                               const CaptureDeviceType type) {

  rtc::RefCountedObject<DesktopCaptureImpl>* capture = new rtc::RefCountedObject<DesktopCaptureImpl>(id);

  //create real screen capturer.
  if (capture->Init(uniqueId, type)) {
    capture->Release();
    return nullptr;
  }

  return capture;
}

int32_t WindowDeviceInfoImpl::Init() {
  desktop_device_info_ = std::unique_ptr<DesktopDeviceInfo>(DesktopDeviceInfoImpl::Create());
  return 0;
}

int32_t DesktopCaptureImpl::AddRef() const {
  return ++mRefCount;
}
int32_t DesktopCaptureImpl::Release() const {
  assert(mRefCount > 0);
  auto count = --mRefCount;
  if (!count) {
    WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceVideoCapture, -1,
                 "DesktopCapture self deleting (desktopCapture=0x%p)", this);

    // Clear any pointers before starting destruction. Otherwise worker-
    // threads will still have pointers to a partially destructed object.
    // Example: AudioDeviceBuffer::RequestPlayoutData() can access a
    // partially deconstructed |_ptrCbAudioTransport| during destruction
    // if we don't call Terminate here.
    //-> NG TODO Terminate();
    delete this;
    return count;
  }
  return mRefCount;
}

int32_t WindowDeviceInfoImpl::Refresh() {
  desktop_device_info_->Refresh();
  return 0;
}

uint32_t WindowDeviceInfoImpl::NumberOfDevices() {
  return desktop_device_info_->getWindowCount();
}

int32_t WindowDeviceInfoImpl::GetDeviceName(uint32_t deviceNumber,
                                            char* deviceNameUTF8,
                                            uint32_t deviceNameUTF8Length,
                                            char* deviceUniqueIdUTF8,
                                            uint32_t deviceUniqueIdUTF8Length,
                                            char* productUniqueIdUTF8,
                                            uint32_t productUniqueIdUTF8Length,
                                            pid_t* pid) {

  DesktopDisplayDevice desktopDisplayDevice;

  // always initialize output
  if (deviceNameUTF8 && deviceNameUTF8Length > 0) {
    memset(deviceNameUTF8, 0, deviceNameUTF8Length);
  }
  if (deviceUniqueIdUTF8 && deviceUniqueIdUTF8Length > 0) {
    memset(deviceUniqueIdUTF8, 0, deviceUniqueIdUTF8Length);
  }
  if (productUniqueIdUTF8 && productUniqueIdUTF8Length > 0) {
    memset(productUniqueIdUTF8, 0, productUniqueIdUTF8Length);
  }

  if (desktop_device_info_->getWindowInfo(deviceNumber,
                                          desktopDisplayDevice) == 0) {

    size_t len;

    const char *deviceName = desktopDisplayDevice.getDeviceName();
    len = deviceName ? strlen(deviceName) : 0;
    if (len && deviceNameUTF8 && len <= deviceNameUTF8Length) {
      memcpy(deviceNameUTF8,
             deviceName,
             len);
    }

    const char *deviceUniqueId = desktopDisplayDevice.getUniqueIdName();
    len = deviceUniqueId ? strlen(deviceUniqueId) : 0;
    if (len && deviceUniqueIdUTF8 && len <= deviceUniqueIdUTF8Length) {
      memcpy(deviceUniqueIdUTF8,
             deviceUniqueId,
             len);
    }
    if (pid) {
      *pid = desktopDisplayDevice.getPid();
    }
  }

  return 0;
}

int32_t WindowDeviceInfoImpl::DisplayCaptureSettingsDialogBox(const char* deviceUniqueIdUTF8,
                                                              const char* dialogTitleUTF8,
                                                              void* parentWindow,
                                                              uint32_t positionX,
                                                              uint32_t positionY) {
  // no device properties to change
  return 0;
}

int32_t WindowDeviceInfoImpl::NumberOfCapabilities(const char* deviceUniqueIdUTF8) {
  return 0;
}

int32_t WindowDeviceInfoImpl::GetCapability(const char* deviceUniqueIdUTF8,
                                            const uint32_t deviceCapabilityNumber,
                                            VideoCaptureCapability& capability) {
  return 0;
}

int32_t WindowDeviceInfoImpl::GetBestMatchedCapability(const char* deviceUniqueIdUTF8,
                                                       const VideoCaptureCapability& requested,
                                                       VideoCaptureCapability& resulting) {
  return 0;
}

int32_t WindowDeviceInfoImpl::GetOrientation(const char* deviceUniqueIdUTF8,
                                             VideoRotation& orientation) {
  return 0;
}

VideoCaptureModule::DeviceInfo* DesktopCaptureImpl::CreateDeviceInfo(const int32_t id,
                                                                     const CaptureDeviceType type) {
  if (type == CaptureDeviceType::Application) {
    AppDeviceInfoImpl * pAppDeviceInfoImpl = new AppDeviceInfoImpl(id);
    if (!pAppDeviceInfoImpl || pAppDeviceInfoImpl->Init()) {
      delete pAppDeviceInfoImpl;
      pAppDeviceInfoImpl = NULL;
    }
    return pAppDeviceInfoImpl;
  } else if (type == CaptureDeviceType::Screen) {
    ScreenDeviceInfoImpl * pScreenDeviceInfoImpl = new ScreenDeviceInfoImpl(id);
    if (!pScreenDeviceInfoImpl || pScreenDeviceInfoImpl->Init()) {
      delete pScreenDeviceInfoImpl;
      pScreenDeviceInfoImpl = NULL;
    }
    return pScreenDeviceInfoImpl;
  } else if (type == CaptureDeviceType::Window) {
    WindowDeviceInfoImpl * pWindowDeviceInfoImpl = new WindowDeviceInfoImpl(id);
    if (!pWindowDeviceInfoImpl || pWindowDeviceInfoImpl->Init()) {
      delete pWindowDeviceInfoImpl;
      pWindowDeviceInfoImpl = NULL;
    }
    return pWindowDeviceInfoImpl;
  }
  return NULL;
}

const char* DesktopCaptureImpl::CurrentDeviceName() const {
  return _deviceUniqueId.c_str();
}

int32_t DesktopCaptureImpl::Init(const char* uniqueId,
                                 const CaptureDeviceType type) {
  DesktopCaptureOptions options = DesktopCaptureOptions::CreateDefault();
  // Leave desktop effects enabled during WebRTC captures.
  options.set_disable_effects(false);

  if (type == CaptureDeviceType::Application) {
    std::unique_ptr<DesktopCapturer> pAppCapturer = DesktopCapturer::CreateAppCapturer(options);
    if (!pAppCapturer) {
      return -1;
    }

    DesktopCapturer::SourceId sourceId = atoi(uniqueId);
    pAppCapturer->SelectSource(sourceId);

    MouseCursorMonitor *pMouseCursorMonitor = MouseCursorMonitor::CreateForScreen(options, webrtc::kFullDesktopScreenId);
    desktop_capturer_cursor_composer_ = std::unique_ptr<DesktopAndCursorComposer>(new DesktopAndCursorComposer(pAppCapturer.release(), pMouseCursorMonitor));
  } else if (type == CaptureDeviceType::Screen) {
    std::unique_ptr<DesktopCapturer> pScreenCapturer = DesktopCapturer::CreateScreenCapturer(options);
    if (!pScreenCapturer.get()) {
      return -1;
    }

    DesktopCapturer::SourceId sourceId = atoi(uniqueId);
    pScreenCapturer->SelectSource(sourceId);

    // Upstream removed the ShapeObserver
    //pScreenCapturer->SetMouseShapeObserver(this);

    MouseCursorMonitor *pMouseCursorMonitor = MouseCursorMonitor::CreateForScreen(options, sourceId);
    desktop_capturer_cursor_composer_ = std::unique_ptr<DesktopAndCursorComposer>(new DesktopAndCursorComposer(pScreenCapturer.release(), pMouseCursorMonitor));
  } else if (type == CaptureDeviceType::Window) {
    std::unique_ptr<DesktopCapturer> pWindowCapturer = DesktopCapturer::CreateWindowCapturer(options);
    if (!pWindowCapturer.get()) {
      return -1;
    }

    DesktopCapturer::SourceId sourceId = atoi(uniqueId);
    pWindowCapturer->SelectSource(sourceId);

    MouseCursorMonitor *pMouseCursorMonitor = MouseCursorMonitor::CreateForWindow(webrtc::DesktopCaptureOptions::CreateDefault(), sourceId);
    desktop_capturer_cursor_composer_ = std::unique_ptr<DesktopAndCursorComposer>(new DesktopAndCursorComposer(pWindowCapturer.release(), pMouseCursorMonitor));
  }
  _deviceUniqueId = uniqueId;

  return 0;
}

DesktopCaptureImpl::DesktopCaptureImpl(const int32_t id)
  : _id(id),
    _deviceUniqueId(""),
    _apiCs(*CriticalSectionWrapper::CreateCriticalSection()),
    _requestedCapability(),
    _callBackCs(*CriticalSectionWrapper::CreateCriticalSection()),
    _rotateFrame(kVideoRotation_0),
    last_capture_time_(rtc::TimeNanos()/rtc::kNumNanosecsPerMillisec),
    // XXX Note that this won't capture drift!
    delta_ntp_internal_ms_(Clock::GetRealTimeClock()->CurrentNtpInMilliseconds() -
                           last_capture_time_),
    time_event_(EventWrapper::Create()),
    mRefCount(0),
#if defined(_WIN32)
    capturer_thread_(new rtc::PlatformUIThread(Run, this, "ScreenCaptureThread")),
#else
#if defined(WEBRTC_LINUX)
    capturer_thread_(nullptr),
#else
    capturer_thread_(new rtc::PlatformThread(Run, this, "ScreenCaptureThread")),
#endif
#endif
    started_(false) {
  //-> TODO @@NG why is this crashing (seen on Linux)
  //-> capturer_thread_->SetPriority(rtc::kHighPriority);
  _requestedCapability.width = kDefaultWidth;
  _requestedCapability.height = kDefaultHeight;
  _requestedCapability.maxFPS = 30;
  _requestedCapability.rawType = kVideoI420;
  _requestedCapability.codecType = kVideoCodecUnknown;
  memset(_incomingFrameTimesNanos, 0, sizeof(_incomingFrameTimesNanos));
}

DesktopCaptureImpl::~DesktopCaptureImpl() {
  time_event_->Set();
  if (capturer_thread_) {
    capturer_thread_->Stop();
  }
  delete &_callBackCs;
  delete &_apiCs;
}

void DesktopCaptureImpl::RegisterCaptureDataCallback(rtc::VideoSinkInterface<VideoFrame> *dataCallback)
{
  CriticalSectionScoped cs(&_apiCs);
  CriticalSectionScoped cs2(&_callBackCs);
  _dataCallBacks.insert(dataCallback);
}

void DesktopCaptureImpl::DeRegisterCaptureDataCallback(
  rtc::VideoSinkInterface<VideoFrame> *dataCallback)
{
  CriticalSectionScoped cs(&_apiCs);
  CriticalSectionScoped cs2(&_callBackCs);
  auto it = _dataCallBacks.find(dataCallback);
  if (it != _dataCallBacks.end()) {
    _dataCallBacks.erase(it);
  }
}

int32_t DesktopCaptureImpl::StopCaptureIfAllClientsClose() {
  if (_dataCallBacks.empty()) {
    return StopCapture();
  } else {
    return 0;
  }
}

int32_t DesktopCaptureImpl::DeliverCapturedFrame(webrtc::VideoFrame& captureFrame,
                                                 int64_t capture_time) {
  UpdateFrameCount();  // frame count used for local frame rate callback.

  // Set the capture time
  if (capture_time != 0) {
    captureFrame.set_render_time_ms(capture_time - delta_ntp_internal_ms_);
  } else {
    captureFrame.set_render_time_ms(rtc::TimeNanos()/rtc::kNumNanosecsPerMillisec);
  }

  if (captureFrame.render_time_ms() == last_capture_time_) {
    // We don't allow the same capture time for two frames, drop this one.
    return -1;
  }
  last_capture_time_ = captureFrame.render_time_ms();

  for (auto dataCallBack : _dataCallBacks) {
    dataCallBack->OnFrame(captureFrame);
  }

  return 0;
}

// Copied from VideoCaptureImpl::IncomingFrame. See Bug 1038324
int32_t DesktopCaptureImpl::IncomingFrame(uint8_t* videoFrame,
                                          size_t videoFrameLength,
                                          const VideoCaptureCapability& frameInfo,
                                          int64_t captureTime/*=0*/)
{
  WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceVideoCapture, _id,
               "IncomingFrame width %d, height %d", (int) frameInfo.width,
               (int) frameInfo.height);

  int64_t startProcessTime = rtc::TimeNanos();

  CriticalSectionScoped cs(&_callBackCs);

  const int32_t width = frameInfo.width;
  const int32_t height = frameInfo.height;

  TRACE_EVENT1("webrtc", "VC::IncomingFrame", "capture_time", captureTime);

  if (frameInfo.codecType == kVideoCodecUnknown) {
    // Not encoded, convert to I420.
    const VideoType commonVideoType =
      RawVideoTypeToCommonVideoVideoType(frameInfo.rawType);

    if (frameInfo.rawType != kVideoMJPEG &&
        CalcBufferSize(commonVideoType, width,
                       abs(height)) != videoFrameLength) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                   "Wrong incoming frame length.");
      return -1;
    }

    int stride_y = width;
    int stride_uv = (width + 1) / 2;
    int target_width = width;
    int target_height = abs(height);
    // Rotating resolution when for 90/270 degree rotations.
    if (_rotateFrame == kVideoRotation_90 || _rotateFrame == kVideoRotation_270)  {
      target_height = width;
      target_width = abs(height);
    }

    // Setting absolute height (in case it was negative).
    // In Windows, the image starts bottom left, instead of top left.
    // Setting a negative source height, inverts the image (within LibYuv).
    rtc::scoped_refptr<webrtc::I420Buffer> buffer;
    buffer = I420Buffer::Create(target_width, target_height, stride_y,
                                stride_uv, stride_uv);
    const int conversionResult = ConvertToI420(commonVideoType,
                                               videoFrame,
                                               0, 0,  // No cropping
                                               width, height,
                                               videoFrameLength,
                                               _rotateFrame,
                                               buffer.get());
    webrtc::VideoFrame captureFrame(buffer, 0, 0, kVideoRotation_0);
    if (conversionResult < 0) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                   "Failed to convert capture frame from type %d to I420",
                   frameInfo.rawType);
      return -1;
    }

    DeliverCapturedFrame(captureFrame, captureTime);
  } else {
    assert(false);
    return -1;
  }

  const int64_t processTime =
    (rtc::TimeNanos() - startProcessTime)/rtc::kNumNanosecsPerMillisec;

  if (processTime > 10) {
    WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideoCapture, _id,
                 "Too long processing time of Incoming frame: %ums",
                 (unsigned int) processTime);
  }

  return 0;
}

int32_t DesktopCaptureImpl::SetCaptureRotation(VideoRotation rotation) {
  CriticalSectionScoped cs(&_apiCs);
  CriticalSectionScoped cs2(&_callBackCs);
  _rotateFrame = rotation;
  return 0;
}

bool DesktopCaptureImpl::SetApplyRotation(bool enable) {
  return true;
}

void DesktopCaptureImpl::UpdateFrameCount() {
  if (_incomingFrameTimesNanos[0] == 0) {
    // first no shift
  } else {
    // shift
    for (int i = (kFrameRateCountHistorySize - 2); i >= 0; i--) {
      _incomingFrameTimesNanos[i + 1] = _incomingFrameTimesNanos[i];
    }
  }
  _incomingFrameTimesNanos[0] = rtc::TimeNanos();
}

uint32_t DesktopCaptureImpl::CalculateFrameRate(int64_t now_ns)
{
    int32_t num = 0;
    int32_t nrOfFrames = 0;
    for (num = 1; num < (kFrameRateCountHistorySize - 1); num++)
    {
        if (_incomingFrameTimesNanos[num] <= 0 ||
            (now_ns - _incomingFrameTimesNanos[num]) /
            rtc::kNumNanosecsPerMillisec >
                kFrameRateHistoryWindowMs) // don't use data older than 2sec
        {
            break;
        }
        else
        {
            nrOfFrames++;
        }
    }
    if (num > 1)
    {
        int64_t diff = (now_ns - _incomingFrameTimesNanos[num - 1]) /
                       rtc::kNumNanosecsPerMillisec;
        if (diff > 0)
        {
            return uint32_t((nrOfFrames * 1000.0f / diff) + 0.5f);
        }
    }

    return nrOfFrames;
}

int32_t DesktopCaptureImpl::StartCapture(const VideoCaptureCapability& capability) {
  _requestedCapability = capability;
#if defined(_WIN32)
  uint32_t maxFPSNeeded = 1000/_requestedCapability.maxFPS;
  capturer_thread_->RequestCallbackTimer(maxFPSNeeded);
#endif

  if (started_) {
    return 0;
  }
#if defined(WEBRTC_LINUX)
  // Lazily init capturer_thread_
  if (!capturer_thread_) {
      capturer_thread_ = std::unique_ptr<rtc::PlatformThread>(
          new rtc::PlatformThread(Run, this, "ScreenCaptureThread"));
  }
#endif
  desktop_capturer_cursor_composer_->Start(this);
  capturer_thread_->Start();
  started_ = true;

  return 0;
}

bool DesktopCaptureImpl::FocusOnSelectedSource()
{
  return desktop_capturer_cursor_composer_->FocusOnSelectedSource();
}

int32_t DesktopCaptureImpl::StopCapture() {
  if (started_) {
    capturer_thread_->Stop(); // thread is guaranteed stopped before this returns
    desktop_capturer_cursor_composer_->Stop();
    started_ = false;
    return 0;
  }
  return -1;
}

bool DesktopCaptureImpl::CaptureStarted() {
  return started_;
}

int32_t DesktopCaptureImpl::CaptureSettings(VideoCaptureCapability& settings) {
  return -1;
}

void DesktopCaptureImpl::OnCaptureResult(DesktopCapturer::Result result,
                                         std::unique_ptr<DesktopFrame> frame) {
  if (frame.get() == nullptr) return;
  uint8_t * videoFrame = frame->data();
  VideoCaptureCapability frameInfo;
  frameInfo.width = frame->size().width();
  frameInfo.height = frame->size().height();
  frameInfo.rawType = kVideoARGB;

  // combine cursor in frame
  // Latest WebRTC already support it by DesktopFrameWithCursor/DesktopAndCursorComposer.

  size_t videoFrameLength = frameInfo.width * frameInfo.height * DesktopFrame::kBytesPerPixel;
  IncomingFrame(videoFrame, videoFrameLength, frameInfo);
}

void DesktopCaptureImpl::process() {
  DesktopRect desktop_rect;
  DesktopRegion desktop_region;

#if !defined(_WIN32)
  int64_t startProcessTime = rtc::TimeNanos();
#endif

  desktop_capturer_cursor_composer_->CaptureFrame();

#if !defined(_WIN32)
  const uint32_t processTime =
    ((uint32_t)(rtc::TimeNanos() - startProcessTime))/rtc::kNumNanosecsPerMillisec;
  // Use at most x% CPU or limit framerate
  const uint32_t maxFPSNeeded = 1000/_requestedCapability.maxFPS;
  const float sleepTimeFactor = (100.0f / kMaxDesktopCaptureCpuUsage) - 1.0f;
  const uint32_t sleepTime = sleepTimeFactor * processTime;
  time_event_->Wait(std::max<uint32_t>(maxFPSNeeded, sleepTime));
#endif
}

}  // namespace webrtc
