/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_capture_impl.h"

#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "critical_section_wrapper.h"
#include "module_common_types.h"
#include "ref_count.h"
#include "tick_util.h"
#include "trace.h"
#include "video_capture_config.h"

#include <stdlib.h>

namespace webrtc
{
namespace videocapturemodule
{
VideoCaptureModule* VideoCaptureImpl::Create(
    const WebRtc_Word32 id,
    VideoCaptureExternal*& externalCapture)
{
    RefCountImpl<VideoCaptureImpl>* implementation =
        new RefCountImpl<VideoCaptureImpl>(id);
    externalCapture = implementation;
    return implementation;
}

const char* VideoCaptureImpl::CurrentDeviceName() const
{
    return _deviceUniqueId;
}

WebRtc_Word32 VideoCaptureImpl::ChangeUniqueId(const WebRtc_Word32 id)
{
    _id = id;
    return 0;
}

// returns the number of milliseconds until the module want a worker thread to call Process
WebRtc_Word32 VideoCaptureImpl::TimeUntilNextProcess()
{
    CriticalSectionScoped cs(&_callBackCs);
    TickTime timeNow = TickTime::Now();

    WebRtc_Word32 timeToNormalProcess = kProcessInterval
        - (WebRtc_Word32)((TickTime::Now() - _lastProcessTime).Milliseconds());
    WebRtc_Word32 timeToStartImage = timeToNormalProcess;
    if (_startImageFrameIntervall)
    {
        timeToStartImage = _startImageFrameIntervall
            - (WebRtc_Word32)((timeNow - _lastSentStartImageTime).Milliseconds());
        if (timeToStartImage < 0)
        {
            timeToStartImage = 0;
        }
    }
    return (timeToStartImage < timeToNormalProcess)
            ? timeToStartImage : timeToNormalProcess;
}

// Process any pending tasks such as timeouts
WebRtc_Word32 VideoCaptureImpl::Process()
{
    CriticalSectionScoped cs(&_callBackCs);

    const TickTime now = TickTime::Now();
    _lastProcessTime = TickTime::Now();

    // Handle No picture alarm

    if (_lastProcessFrameCount.Ticks() == _incomingFrameTimes[0].Ticks() &&
        _captureAlarm != Raised)
    {
        if (_noPictureAlarmCallBack && _captureCallBack)
        {
            _captureAlarm = Raised;
            _captureCallBack->OnNoPictureAlarm(_id, _captureAlarm);
        }
    }
    else if (_lastProcessFrameCount.Ticks() != _incomingFrameTimes[0].Ticks() &&
             _captureAlarm != Cleared)
    {
        if (_noPictureAlarmCallBack && _captureCallBack)
        {
            _captureAlarm = Cleared;
            _captureCallBack->OnNoPictureAlarm(_id, _captureAlarm);

        }
    }

    // Handle frame rate callback
    if ((now - _lastFrameRateCallbackTime).Milliseconds()
        > kFrameRateCallbackInterval)
    {
        if (_frameRateCallBack && _captureCallBack)
        {
            const WebRtc_UWord32 frameRate = CalculateFrameRate(now);
            _captureCallBack->OnCaptureFrameRate(_id, frameRate);
        }
        _lastFrameRateCallbackTime = now; // Can be set by EnableFrameRateCallback

    }

    _lastProcessFrameCount = _incomingFrameTimes[0];

    // Handle start image frame rates.
    if (_startImageFrameIntervall
        && (now - _lastSentStartImageTime).Milliseconds() >= _startImageFrameIntervall)
    {
        _lastSentStartImageTime = now;
        if (_dataCallBack)
        {
            _captureFrame.CopyFrame(_startImage);
            _captureFrame.SetRenderTime(TickTime::MillisecondTimestamp());
            _dataCallBack->OnIncomingCapturedFrame(_id, _captureFrame,
                                                   kVideoCodecUnknown);
        }
    }
    return 0;
}

VideoCaptureImpl::VideoCaptureImpl(const WebRtc_Word32 id)
    : _id(id), _deviceUniqueId(NULL), _apiCs(*CriticalSectionWrapper::CreateCriticalSection()),
      _captureDelay(0), _requestedCapability(),
      _callBackCs(*CriticalSectionWrapper::CreateCriticalSection()),
      _lastProcessTime(TickTime::Now()),
      _lastFrameRateCallbackTime(TickTime::Now()), _frameRateCallBack(false),
      _noPictureAlarmCallBack(false), _captureAlarm(Cleared), _setCaptureDelay(0),
      _dataCallBack(NULL), _captureCallBack(NULL),
      _startImage(), _startImageFrameIntervall(0),
      _lastSentStartImageTime(TickTime::Now()),
      _lastProcessFrameCount(TickTime::Now()), _rotateFrame(kRotateNone),
      last_capture_time_(TickTime::MillisecondTimestamp())

{
    _requestedCapability.width = kDefaultWidth;
    _requestedCapability.height = kDefaultHeight;
    _requestedCapability.maxFPS = 30;
    _requestedCapability.rawType = kVideoI420;
    _requestedCapability.codecType = kVideoCodecUnknown;
    memset(_incomingFrameTimes, 0, sizeof(_incomingFrameTimes));
}

VideoCaptureImpl::~VideoCaptureImpl()
{
    DeRegisterCaptureDataCallback();
    DeRegisterCaptureCallback();
    delete &_callBackCs;
    delete &_apiCs;

    if (_deviceUniqueId)
        delete[] _deviceUniqueId;
}

WebRtc_Word32 VideoCaptureImpl::RegisterCaptureDataCallback(
                                        VideoCaptureDataCallback& dataCallBack)
{
    CriticalSectionScoped cs(&_apiCs);
    CriticalSectionScoped cs2(&_callBackCs);
    _dataCallBack = &dataCallBack;

    return 0;
}

WebRtc_Word32 VideoCaptureImpl::DeRegisterCaptureDataCallback()
{
    CriticalSectionScoped cs(&_apiCs);
    CriticalSectionScoped cs2(&_callBackCs);
    _dataCallBack = NULL;
    return 0;
}
WebRtc_Word32 VideoCaptureImpl::RegisterCaptureCallback(VideoCaptureFeedBack& callBack)
{

    CriticalSectionScoped cs(&_apiCs);
    CriticalSectionScoped cs2(&_callBackCs);
    _captureCallBack = &callBack;
    return 0;
}
WebRtc_Word32 VideoCaptureImpl::DeRegisterCaptureCallback()
{

    CriticalSectionScoped cs(&_apiCs);
    CriticalSectionScoped cs2(&_callBackCs);
    _captureCallBack = NULL;
    return 0;

}
WebRtc_Word32 VideoCaptureImpl::SetCaptureDelay(WebRtc_Word32 delayMS)
{
    CriticalSectionScoped cs(&_apiCs);
    _captureDelay = delayMS;
    return 0;
}
WebRtc_Word32 VideoCaptureImpl::CaptureDelay()
{
    CriticalSectionScoped cs(&_apiCs);
    return _setCaptureDelay;
}

WebRtc_Word32 VideoCaptureImpl::DeliverCapturedFrame(VideoFrame& captureFrame,
    WebRtc_Word32 width, WebRtc_Word32 height, WebRtc_Word64 capture_time,
    VideoCodecType codec_type) {
  UpdateFrameCount();// frame count used for local frame rate callback.
  _startImageFrameIntervall = 0; // prevent the start image to be displayed.

  const bool callOnCaptureDelayChanged = _setCaptureDelay != _captureDelay;
  // Capture delay changed
  if (_setCaptureDelay != _captureDelay) {
      _setCaptureDelay = _captureDelay;
  }

  // Set the capture time
  if (capture_time != 0) {
      captureFrame.SetRenderTime(capture_time);
  }
  else {
      captureFrame.SetRenderTime(TickTime::MillisecondTimestamp());
  }

  if (captureFrame.RenderTimeMs() == last_capture_time_) {
    // We don't allow the same capture time for two frames, drop this one.
    return -1;
  }
  last_capture_time_ = captureFrame.RenderTimeMs();

  captureFrame.SetHeight(height);
  captureFrame.SetWidth(width);

  if (_dataCallBack) {
    if (callOnCaptureDelayChanged) {
      _dataCallBack->OnCaptureDelayChanged(_id, _captureDelay);
    }
    _dataCallBack->OnIncomingCapturedFrame(_id, captureFrame, codec_type);
  }

  return 0;
}

WebRtc_Word32 VideoCaptureImpl::IncomingFrame(
    WebRtc_UWord8* videoFrame,
    WebRtc_Word32 videoFrameLength,
    const VideoCaptureCapability& frameInfo,
    WebRtc_Word64 captureTime/*=0*/)
{
    WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceVideoCapture, _id,
               "IncomingFrame width %d, height %d", (int) frameInfo.width,
               (int) frameInfo.height);

    TickTime startProcessTime = TickTime::Now();

    CriticalSectionScoped cs(&_callBackCs);

    const WebRtc_Word32 width = frameInfo.width;
    const WebRtc_Word32 height = frameInfo.height;

    if (frameInfo.codecType == kVideoCodecUnknown)
    {
        // Not encoded, convert to I420.
        const VideoType commonVideoType =
                  RawVideoTypeToCommonVideoVideoType(frameInfo.rawType);

        if (frameInfo.rawType != kVideoMJPEG &&
            CalcBufferSize(commonVideoType, width,
                           abs(height)) != videoFrameLength)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                         "Wrong incoming frame length.");
            return -1;
        }

        // Allocate I420 buffer.
        int requiredLength = CalcBufferSize(kI420, width, abs(height));
        _captureFrame.VerifyAndAllocate(requiredLength);
        if (!_captureFrame.Buffer())
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                       "Failed to allocate frame buffer.");
            return -1;
        }

        memset(_captureFrame.Buffer(), 0, _captureFrame.Size());
        // Keeping stride = width for I420 destination.
        int dstStride  = width;
        const int conversionResult = ConvertToI420(commonVideoType,
                                                   videoFrame,
                                                   0, 0,  // No cropping
                                                   width, height,
                                                   videoFrameLength,
                                                   width, height, dstStride,
                                                   _rotateFrame,
                                                   _captureFrame.Buffer());
        if (conversionResult < 0)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                       "Failed to convert capture frame from type %d to I420",
                       frameInfo.rawType);
            return -1;
        }
        _captureFrame.SetLength(requiredLength);
    }
    else // Encoded format
    {
        if (_captureFrame.CopyFrame(videoFrameLength, videoFrame) != 0)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                       "Failed to copy captured frame of length %d", (int) videoFrameLength);
        }
    }

    DeliverCapturedFrame(_captureFrame, width, abs(height), captureTime,
                         frameInfo.codecType);


    const WebRtc_UWord32 processTime =
        (WebRtc_UWord32)(TickTime::Now() - startProcessTime).Milliseconds();
    if (processTime > 10) // If the process time is too long MJPG will not work well.
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideoCapture, _id,
                   "Too long processing time of Incoming frame: %ums",
                   (unsigned int) processTime);
    }

    return 0;
}

WebRtc_Word32 VideoCaptureImpl::IncomingFrameI420(
    const VideoFrameI420& video_frame, WebRtc_Word64 captureTime) {

  CriticalSectionScoped cs(&_callBackCs);

  // Allocate I420 buffer
  int frame_size = CalcBufferSize(kI420,
                                  video_frame.width,
                                  video_frame.height);
  _captureFrame.VerifyAndAllocate(frame_size);
  if (!_captureFrame.Buffer()) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
               "Failed to allocate frame buffer.");
    return -1;
  }

  // Copy planes to the _captureFrame
  int y_width = video_frame.width;
  int uv_width = video_frame.width / 2;
  int y_rows = video_frame.height;
  int uv_rows = video_frame.height / 2;  // I420
  unsigned char* current_pointer = _captureFrame.Buffer();
  unsigned char* y_plane = video_frame.y_plane;
  unsigned char* u_plane = video_frame.u_plane;
  unsigned char* v_plane = video_frame.v_plane;
  // Copy Y
  for (int i = 0; i < y_rows; ++i) {
    memcpy(current_pointer, y_plane, y_width);
    // Remove the alignment which ViE doesn't support.
    current_pointer += y_width;
    y_plane += video_frame.y_pitch;
  }
  // Copy U
  for (int i = 0; i < uv_rows; ++i) {
    memcpy(current_pointer, u_plane, uv_width);
    // Remove the alignment which ViE doesn't support.
    current_pointer += uv_width;
    u_plane += video_frame.u_pitch;
  }
  // Copy V
  for (int i = 0; i < uv_rows; ++i) {
    memcpy(current_pointer, v_plane, uv_width);
    // Remove the alignment which ViE doesn't support.
    current_pointer += uv_width;
    v_plane += video_frame.v_pitch;
  }
  _captureFrame.SetLength(frame_size);

  DeliverCapturedFrame(_captureFrame,
                       video_frame.width,
                       video_frame.height,
                       captureTime,
                       kVideoCodecUnknown);

  return 0;
}

WebRtc_Word32 VideoCaptureImpl::SetCaptureRotation(VideoCaptureRotation rotation)
{
    CriticalSectionScoped cs(&_apiCs);
    CriticalSectionScoped cs2(&_callBackCs);
    switch (rotation)
    {
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

WebRtc_Word32 VideoCaptureImpl::StartSendImage(const VideoFrame& videoFrame,
                                                     WebRtc_Word32 frameRate)
{
    CriticalSectionScoped cs(&_apiCs);
    CriticalSectionScoped cs2(&_callBackCs);
    if (frameRate < 1 || frameRate > kMaxFrameRate)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                   "StartSendImage Invalid parameter. frameRate %d", (int) frameRate);
        return -1;;
    }
    _startImage.CopyFrame(videoFrame);
    _startImageFrameIntervall = 1000 / frameRate;
    _lastSentStartImageTime = TickTime::Now();
    return 0;

}
WebRtc_Word32 VideoCaptureImpl::StopSendImage()
{
    CriticalSectionScoped cs(&_apiCs);
    CriticalSectionScoped cs2(&_callBackCs);
    _startImageFrameIntervall = 0;
    return 0;
}

WebRtc_Word32 VideoCaptureImpl::EnableFrameRateCallback(const bool enable)
{
    CriticalSectionScoped cs(&_apiCs);
    CriticalSectionScoped cs2(&_callBackCs);
    _frameRateCallBack = enable;
    if (enable)
    {
        _lastFrameRateCallbackTime = TickTime::Now();
    }
    return 0;
}

WebRtc_Word32 VideoCaptureImpl::EnableNoPictureAlarm(const bool enable)
{
    CriticalSectionScoped cs(&_apiCs);
    CriticalSectionScoped cs2(&_callBackCs);
    _noPictureAlarmCallBack = enable;
    return 0;
}

void VideoCaptureImpl::UpdateFrameCount()
{
    if (_incomingFrameTimes[0].MicrosecondTimestamp() == 0)
    {
        // first no shift
    }
    else
    {
        // shift
        for (int i = (kFrameRateCountHistorySize - 2); i >= 0; i--)
        {
            _incomingFrameTimes[i + 1] = _incomingFrameTimes[i];
        }
    }
    _incomingFrameTimes[0] = TickTime::Now();
}

WebRtc_UWord32 VideoCaptureImpl::CalculateFrameRate(const TickTime& now)
{
    WebRtc_Word32 num = 0;
    WebRtc_Word32 nrOfFrames = 0;
    for (num = 1; num < (kFrameRateCountHistorySize - 1); num++)
    {
        if (_incomingFrameTimes[num].Ticks() <= 0
            || (now - _incomingFrameTimes[num]).Milliseconds() > kFrameRateHistoryWindowMs) // don't use data older than 2sec
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
        WebRtc_Word64 diff = (now - _incomingFrameTimes[num - 1]).Milliseconds();
        if (diff > 0)
        {
            return WebRtc_UWord32((nrOfFrames * 1000.0f / diff) + 0.5f);
        }
    }

    return nrOfFrames;
}
} // namespace videocapturemodule
} // namespace webrtc
