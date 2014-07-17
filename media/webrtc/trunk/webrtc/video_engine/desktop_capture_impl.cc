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

#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/modules/video_capture/video_capture_config.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/ref_count.h"
#include "webrtc/system_wrappers/interface/tick_util.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/system_wrappers/interface/trace_event.h"
#include "webrtc/video_engine/desktop_capture_impl.h"
#include "webrtc/modules/desktop_capture/desktop_frame.h"
#include "webrtc/modules/desktop_capture/desktop_device_info.h"
#include "webrtc/modules/desktop_capture/app_capturer.h"
#include "webrtc/modules/desktop_capture/desktop_capture_options.h"

namespace webrtc {

ScreenDeviceInfoImpl::ScreenDeviceInfoImpl(const int32_t id) : _id(id) {
}

ScreenDeviceInfoImpl::~ScreenDeviceInfoImpl(void) {
}

int32_t ScreenDeviceInfoImpl::Init() {
  desktop_device_info_.reset(DesktopDeviceInfoImpl::Create());
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
                                            uint32_t productUniqueIdUTF8Length) {

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
                                             VideoCaptureRotation& orientation) {
  return 0;
}

AppDeviceInfoImpl::AppDeviceInfoImpl(const int32_t id) {
}

AppDeviceInfoImpl::~AppDeviceInfoImpl(void) {
}

int32_t AppDeviceInfoImpl::Init() {
  desktop_device_info_.reset(DesktopDeviceInfoImpl::Create());
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
                                         uint32_t productUniqueIdUTF8Length) {

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
                                          VideoCaptureRotation& orientation) {
  return 0;
}

VideoCaptureModule* DesktopCaptureImpl::Create(const int32_t id,
                                               const char* uniqueId,
                                               const bool bIsApp) {
  RefCountImpl<DesktopCaptureImpl>* capture = new RefCountImpl<DesktopCaptureImpl>(id);

  //create real screen capturer.
  if (capture->Init(uniqueId, bIsApp) != 0) {
    delete capture;
    return NULL;
  }

  return capture;
}

VideoCaptureModule::DeviceInfo* DesktopCaptureImpl::CreateDeviceInfo(const int32_t id,
                                                                     const bool bIsApp) {
  if (bIsApp) {
    AppDeviceInfoImpl * pAppDeviceInfoImpl = new AppDeviceInfoImpl(id);
    if (!pAppDeviceInfoImpl || pAppDeviceInfoImpl->Init()) {
      delete pAppDeviceInfoImpl;
      pAppDeviceInfoImpl = NULL;
    }
    return pAppDeviceInfoImpl;
  } else {
    ScreenDeviceInfoImpl * pScreenDeviceInfoImpl = new ScreenDeviceInfoImpl(id);
    if (!pScreenDeviceInfoImpl || pScreenDeviceInfoImpl->Init()) {
      delete pScreenDeviceInfoImpl;
      pScreenDeviceInfoImpl = NULL;
    }
    return pScreenDeviceInfoImpl;
  }
}

const char* DesktopCaptureImpl::CurrentDeviceName() const {
  return _deviceUniqueId;
}

int32_t DesktopCaptureImpl::ChangeUniqueId(const int32_t id) {
  _id = id;
  return 0;
}

int32_t DesktopCaptureImpl::Init(const char* uniqueId,
                                 const bool bIsApp) {
  if (bIsApp) {
    AppCapturer *pAppCapturer = AppCapturer::Create();
    if (!pAppCapturer) {
      return -1;
    }

    // processid hard-coded until implemented.  See Bug 1036653
    ProcessId processid = 0;
    pAppCapturer->SelectApp(processid);

    MouseCursorMonitor * pMouseCursorMonitor = MouseCursorMonitor::CreateForScreen(webrtc::DesktopCaptureOptions::CreateDefault(), webrtc::kFullDesktopScreenId);
    desktop_capturer_cursor_composer_.reset(new DesktopAndCursorComposer(pAppCapturer, pMouseCursorMonitor));
  } else {
    ScreenCapturer *pScreenCapturer = ScreenCapturer::Create();
    if (!pScreenCapturer) {
      return -1;
    }

    ScreenId screenid = webrtc::kFullDesktopScreenId;
    pScreenCapturer->SelectScreen(screenid);
    pScreenCapturer->SetMouseShapeObserver(this);

    MouseCursorMonitor * pMouseCursorMonitor = MouseCursorMonitor::CreateForScreen(webrtc::DesktopCaptureOptions::CreateDefault(), screenid);
    desktop_capturer_cursor_composer_.reset(new DesktopAndCursorComposer(pScreenCapturer, pMouseCursorMonitor));
  }
  return 0;
}

// returns the number of milliseconds until the module want a worker thread to call Process
int32_t DesktopCaptureImpl::TimeUntilNextProcess() {
  CriticalSectionScoped cs(&_callBackCs);

  int32_t timeToNormalProcess = kProcessInterval
    - (int32_t)((TickTime::Now() - _lastProcessTime).Milliseconds());

  return timeToNormalProcess;
}

// Process any pending tasks such as timeouts
int32_t DesktopCaptureImpl::Process() {
  CriticalSectionScoped cs(&_callBackCs);

  const TickTime now = TickTime::Now();
  _lastProcessTime = TickTime::Now();

  // Handle No picture alarm
  if (_lastProcessFrameCount.Ticks() == _incomingFrameTimes[0].Ticks() &&
      _captureAlarm != Raised) {
    if (_noPictureAlarmCallBack && _captureCallBack) {
      _captureAlarm = Raised;
      _captureCallBack->OnNoPictureAlarm(_id, _captureAlarm);
    }
  } else if (_lastProcessFrameCount.Ticks() != _incomingFrameTimes[0].Ticks() &&
             _captureAlarm != Cleared) {
    if (_noPictureAlarmCallBack && _captureCallBack) {
      _captureAlarm = Cleared;
      _captureCallBack->OnNoPictureAlarm(_id, _captureAlarm);
    }
  }

  // Handle frame rate callback
  if ((now - _lastFrameRateCallbackTime).Milliseconds()
      > kFrameRateCallbackInterval) {
    if (_frameRateCallBack && _captureCallBack) {
      const uint32_t frameRate = CalculateFrameRate(now);
      _captureCallBack->OnCaptureFrameRate(_id, frameRate);
    }
    _lastFrameRateCallbackTime = now; // Can be set by EnableFrameRateCallback

  }

  _lastProcessFrameCount = _incomingFrameTimes[0];

  return 0;
}

DesktopCaptureImpl::DesktopCaptureImpl(const int32_t id)
  : _id(id),
    _deviceUniqueId(NULL),
    _apiCs(*CriticalSectionWrapper::CreateCriticalSection()),
    _captureDelay(0),
    _requestedCapability(),
    _callBackCs(*CriticalSectionWrapper::CreateCriticalSection()),
    _lastProcessTime(TickTime::Now()),
    _lastFrameRateCallbackTime(TickTime::Now()),
    _frameRateCallBack(false),
    _noPictureAlarmCallBack(false),
    _captureAlarm(Cleared),
    _setCaptureDelay(0),
    _dataCallBack(NULL),
    _captureCallBack(NULL),
  _lastProcessFrameCount(TickTime::Now()),
  _rotateFrame(kRotateNone),
  last_capture_time_(TickTime::MillisecondTimestamp()),
  delta_ntp_internal_ms_(
                         Clock::GetRealTimeClock()->CurrentNtpInMilliseconds() -
                         TickTime::MillisecondTimestamp()),
  capturer_thread_(*ThreadWrapper::CreateThread(Run, this, kHighPriority, "ScreenCaptureThread")) {
  _requestedCapability.width = kDefaultWidth;
  _requestedCapability.height = kDefaultHeight;
  _requestedCapability.maxFPS = 30;
  _requestedCapability.rawType = kVideoI420;
  _requestedCapability.codecType = kVideoCodecUnknown;
  memset(_incomingFrameTimes, 0, sizeof(_incomingFrameTimes));
}

DesktopCaptureImpl::~DesktopCaptureImpl() {
  capturer_thread_.Stop();
  delete &capturer_thread_;

  DeRegisterCaptureDataCallback();
  DeRegisterCaptureCallback();
  delete &_callBackCs;
  delete &_apiCs;

  delete[] _deviceUniqueId;
}

void DesktopCaptureImpl::RegisterCaptureDataCallback(
                                                     VideoCaptureDataCallback& dataCallBack)
{
  CriticalSectionScoped cs(&_apiCs);
  CriticalSectionScoped cs2(&_callBackCs);
  _dataCallBack = &dataCallBack;
}

void DesktopCaptureImpl::DeRegisterCaptureDataCallback()
{
  CriticalSectionScoped cs(&_apiCs);
  CriticalSectionScoped cs2(&_callBackCs);
  _dataCallBack = NULL;
}

void DesktopCaptureImpl::RegisterCaptureCallback(VideoCaptureFeedBack& callBack)
{

  CriticalSectionScoped cs(&_apiCs);
  CriticalSectionScoped cs2(&_callBackCs);
  _captureCallBack = &callBack;
}

void DesktopCaptureImpl::DeRegisterCaptureCallback()
{

  CriticalSectionScoped cs(&_apiCs);
  CriticalSectionScoped cs2(&_callBackCs);
  _captureCallBack = NULL;
}

void DesktopCaptureImpl::SetCaptureDelay(int32_t delayMS)
{
  CriticalSectionScoped cs(&_apiCs);
  _captureDelay = delayMS;
}

int32_t DesktopCaptureImpl::CaptureDelay()
{
  CriticalSectionScoped cs(&_apiCs);
  return _setCaptureDelay;
}

int32_t DesktopCaptureImpl::DeliverCapturedFrame(I420VideoFrame& captureFrame,
                                                 int64_t capture_time) {
  UpdateFrameCount();  // frame count used for local frame rate callback.

  const bool callOnCaptureDelayChanged = _setCaptureDelay != _captureDelay;
  // Capture delay changed
  if (_setCaptureDelay != _captureDelay) {
    _setCaptureDelay = _captureDelay;
  }

  // Set the capture time
  if (capture_time != 0) {
    captureFrame.set_render_time_ms(capture_time - delta_ntp_internal_ms_);
  } else {
    captureFrame.set_render_time_ms(TickTime::MillisecondTimestamp());
  }

  if (captureFrame.render_time_ms() == last_capture_time_) {
    // We don't allow the same capture time for two frames, drop this one.
    return -1;
  }
  last_capture_time_ = captureFrame.render_time_ms();

  if (_dataCallBack) {
    if (callOnCaptureDelayChanged) {
      _dataCallBack->OnCaptureDelayChanged(_id, _captureDelay);
    }
    _dataCallBack->OnIncomingCapturedFrame(_id, captureFrame);
  }

  return 0;
}

// Copied from VideoCaptureImpl::IncomingFrame. See Bug 1038324
int32_t DesktopCaptureImpl::IncomingFrame(uint8_t* videoFrame,
                                          int32_t videoFrameLength,
                                          const VideoCaptureCapability& frameInfo,
                                          int64_t captureTime/*=0*/)
{
  WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceVideoCapture, _id,
               "IncomingFrame width %d, height %d", (int) frameInfo.width,
               (int) frameInfo.height);

  TickTime startProcessTime = TickTime::Now();

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
    int target_height = height;
    // Rotating resolution when for 90/270 degree rotations.
    if (_rotateFrame == kRotate90 || _rotateFrame == kRotate270)  {
      target_width = abs(height);
      target_height = width;
    }

    // Setting absolute height (in case it was negative).
    // In Windows, the image starts bottom left, instead of top left.
    // Setting a negative source height, inverts the image (within LibYuv).
    int ret = _captureFrame.CreateEmptyFrame(target_width,
                                             abs(target_height),
                                             stride_y,
                                             stride_uv, stride_uv);
    if (ret < 0) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                   "Failed to allocate I420 frame.");
      return -1;
    }
    const int conversionResult = ConvertToI420(commonVideoType,
                                               videoFrame,
                                               0, 0,  // No cropping
                                               width, height,
                                               videoFrameLength,
                                               _rotateFrame,
                                               &_captureFrame);
    if (conversionResult < 0) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                   "Failed to convert capture frame from type %d to I420",
                   frameInfo.rawType);
      return -1;
    }
    DeliverCapturedFrame(_captureFrame, captureTime);
  } else {
    assert(false);
    return -1;
  }

  const uint32_t processTime =
    (uint32_t)(TickTime::Now() - startProcessTime).Milliseconds();

  if (processTime > 10) {
    WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideoCapture, _id,
                 "Too long processing time of Incoming frame: %ums",
                 (unsigned int) processTime);
  }

  return 0;
}

int32_t DesktopCaptureImpl::IncomingI420VideoFrame(I420VideoFrame* video_frame, int64_t captureTime) {

  CriticalSectionScoped cs(&_callBackCs);
  int stride_y = video_frame->stride(kYPlane);
  int stride_u = video_frame->stride(kUPlane);
  int stride_v = video_frame->stride(kVPlane);
  int size_y = video_frame->height() * stride_y;
  int size_u = stride_u * ((video_frame->height() + 1) / 2);
  int size_v =  stride_v * ((video_frame->height() + 1) / 2);
  int ret = _captureFrame.CreateFrame(size_y, video_frame->buffer(kYPlane),
                                      size_u, video_frame->buffer(kUPlane),
                                      size_v, video_frame->buffer(kVPlane),
                                      video_frame->width(), video_frame->height(),
                                      stride_y, stride_u, stride_v);

  if (ret < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                 "Failed to create I420VideoFrame");
    return -1;
  }

  DeliverCapturedFrame(_captureFrame, captureTime);

  return 0;
}

int32_t DesktopCaptureImpl::SetCaptureRotation(VideoCaptureRotation rotation) {
  CriticalSectionScoped cs(&_apiCs);
  CriticalSectionScoped cs2(&_callBackCs);
  switch (rotation) {
  case kCameraRotate0:
    _rotateFrame = kRotateNone;
    break;
  case kCameraRotate90:
    _rotateFrame = kRotate90;
    break;
  case kCameraRotate180:
    _rotateFrame = kRotate180;
    break;
  case kCameraRotate270:
    _rotateFrame = kRotate270;
    break;
  }
  return 0;
}

void DesktopCaptureImpl::EnableFrameRateCallback(const bool enable) {
  CriticalSectionScoped cs(&_apiCs);
  CriticalSectionScoped cs2(&_callBackCs);
  _frameRateCallBack = enable;
  if (enable) {
    _lastFrameRateCallbackTime = TickTime::Now();
  }
}

void DesktopCaptureImpl::EnableNoPictureAlarm(const bool enable) {
  CriticalSectionScoped cs(&_apiCs);
  CriticalSectionScoped cs2(&_callBackCs);
  _noPictureAlarmCallBack = enable;
}

void DesktopCaptureImpl::UpdateFrameCount() {
  if (_incomingFrameTimes[0].MicrosecondTimestamp() == 0) {
    // first no shift
  } else {
    // shift
    for (int i = (kFrameRateCountHistorySize - 2); i >= 0; i--) {
      _incomingFrameTimes[i + 1] = _incomingFrameTimes[i];
    }
  }
  _incomingFrameTimes[0] = TickTime::Now();
}

uint32_t DesktopCaptureImpl::CalculateFrameRate(const TickTime& now) {
  int32_t num = 0;
  int32_t nrOfFrames = 0;
  for (num = 1; num < (kFrameRateCountHistorySize - 1); num++) {
    // don't use data older than 2sec
    if (_incomingFrameTimes[num].Ticks() <= 0
        || (now - _incomingFrameTimes[num]).Milliseconds() > kFrameRateHistoryWindowMs) {
      break;
    } else {
      nrOfFrames++;
    }
  }
  if (num > 1) {
    int64_t diff = (now - _incomingFrameTimes[num - 1]).Milliseconds();
    if (diff > 0) {
      // round to nearest value rather than return minimumid_
      return uint32_t((nrOfFrames * 1000.0f / diff) + 0.5f);
    }
  }

  return nrOfFrames;
}

int32_t DesktopCaptureImpl::StartCapture(const VideoCaptureCapability& capability) {
  _requestedCapability = capability;
  desktop_capturer_cursor_composer_->Start(this);
  unsigned int t_id =0;
  capturer_thread_.Start(t_id);
  return 0;
}

int32_t DesktopCaptureImpl::StopCapture() {
  return -1;
}

bool DesktopCaptureImpl::CaptureStarted() {
  return false;
}

int32_t DesktopCaptureImpl::CaptureSettings(VideoCaptureCapability& settings) {
  return -1;
}

void DesktopCaptureImpl::OnCaptureCompleted(DesktopFrame* frame) {
  if (frame == NULL) return;
  uint8_t * videoFrame = frame->data();
  VideoCaptureCapability frameInfo;
  frameInfo.width = frame->size().width();
  frameInfo.height = frame->size().height();
  frameInfo.rawType = kVideoARGB;

  // combine cursor in frame
  // Latest WebRTC already support it by DesktopFrameWithCursor/DesktopAndCursorComposer.

  int32_t videoFrameLength = frameInfo.width * frameInfo.height * DesktopFrame::kBytesPerPixel;
  IncomingFrame(videoFrame, videoFrameLength, frameInfo);
  delete frame; //handled it, so we need delete it
}

SharedMemory* DesktopCaptureImpl::CreateSharedMemory(size_t size) {
  return NULL;
}

void DesktopCaptureImpl::process() {
  DesktopRect desktop_rect;
  DesktopRegion desktop_region;

  desktop_capturer_cursor_composer_->Capture(DesktopRegion());
}

void DesktopCaptureImpl::OnCursorShapeChanged(MouseCursorShape* cursor_shape) {
  // do nothing, DesktopAndCursorComposer does it all
}

}  // namespace webrtc

