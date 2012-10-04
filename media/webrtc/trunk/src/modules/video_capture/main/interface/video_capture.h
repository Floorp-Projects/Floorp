/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_INTERFACE_VIDEO_CAPTURE_H_
#define WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_INTERFACE_VIDEO_CAPTURE_H_

#include "modules/interface/module.h"
#include "modules/video_capture/main/interface/video_capture_defines.h"

namespace webrtc {

#if defined(WEBRTC_ANDROID) && !defined(WEBRTC_CHROMIUM_BUILD)
WebRtc_Word32 SetCaptureAndroidVM(void* javaVM, void* javaContext);
#endif

class VideoCaptureModule: public RefCountedModule {
 public:
  // Interface for receiving information about available camera devices.
  class DeviceInfo {
   public:
    virtual WebRtc_UWord32 NumberOfDevices() = 0;

    // Returns the available capture devices.
    // deviceNumber   - Index of capture device.
    // deviceNameUTF8 - Friendly name of the capture device.
    // deviceUniqueIdUTF8 - Unique name of the capture device if it exist.
    //                      Otherwise same as deviceNameUTF8.
    // productUniqueIdUTF8 - Unique product id if it exist.
    //                       Null terminated otherwise.
    virtual WebRtc_Word32 GetDeviceName(
        WebRtc_UWord32 deviceNumber,
        char* deviceNameUTF8,
        WebRtc_UWord32 deviceNameLength,
        char* deviceUniqueIdUTF8,
        WebRtc_UWord32 deviceUniqueIdUTF8Length,
        char* productUniqueIdUTF8 = 0,
        WebRtc_UWord32 productUniqueIdUTF8Length = 0) = 0;


    // Returns the number of capabilities this device.
    virtual WebRtc_Word32 NumberOfCapabilities(
        const char* deviceUniqueIdUTF8) = 0;

    // Gets the capabilities of the named device.
    virtual WebRtc_Word32 GetCapability(
        const char* deviceUniqueIdUTF8,
        const WebRtc_UWord32 deviceCapabilityNumber,
        VideoCaptureCapability& capability) = 0;

    // Gets clockwise angle the captured frames should be rotated in order
    // to be displayed correctly on a normally rotated display.
    virtual WebRtc_Word32 GetOrientation(
        const char* deviceUniqueIdUTF8,
        VideoCaptureRotation& orientation) = 0;

    // Gets the capability that best matches the requested width, height and
    // frame rate.
    // Returns the deviceCapabilityNumber on success.
    virtual WebRtc_Word32 GetBestMatchedCapability(
        const char* deviceUniqueIdUTF8,
        const VideoCaptureCapability& requested,
        VideoCaptureCapability& resulting) = 0;

     // Display OS /capture device specific settings dialog
    virtual WebRtc_Word32 DisplayCaptureSettingsDialogBox(
        const char* deviceUniqueIdUTF8,
        const char* dialogTitleUTF8,
        void* parentWindow,
        WebRtc_UWord32 positionX,
        WebRtc_UWord32 positionY) = 0;

    virtual ~DeviceInfo() {}
  };

  class VideoCaptureEncodeInterface {
   public:
    virtual WebRtc_Word32 ConfigureEncoder(const VideoCodec& codec,
                                           WebRtc_UWord32 maxPayloadSize) = 0;
    // Inform the encoder about the new target bit rate.
    //  - newBitRate       : New target bit rate in Kbit/s.
    //  - frameRate        : The target frame rate.
    virtual WebRtc_Word32 SetRates(WebRtc_Word32 newBitRate,
                                   WebRtc_Word32 frameRate) = 0;
    // Inform the encoder about the packet loss and the round-trip time.
    //   - packetLoss   : Fraction lost
    //                    (loss rate in percent = 100 * packetLoss / 255).
    //   - rtt          : Round-trip time in milliseconds.
    virtual WebRtc_Word32 SetChannelParameters(WebRtc_UWord32 packetLoss,
                                               int rtt) = 0;

    // Encode the next frame as key frame.
    virtual WebRtc_Word32 EncodeFrameType(const FrameType type) = 0;
  protected:
    virtual ~VideoCaptureEncodeInterface() {
    }
  };

  //   Register capture data callback
  virtual WebRtc_Word32 RegisterCaptureDataCallback(
      VideoCaptureDataCallback& dataCallback) = 0;

  //  Remove capture data callback
  virtual WebRtc_Word32 DeRegisterCaptureDataCallback() = 0;

  // Register capture callback.
  virtual WebRtc_Word32 RegisterCaptureCallback(
      VideoCaptureFeedBack& callBack) = 0;

  //  Remove capture callback.
  virtual WebRtc_Word32 DeRegisterCaptureCallback() = 0;

  // Start capture device
  virtual WebRtc_Word32 StartCapture(
      const VideoCaptureCapability& capability) = 0;

  virtual WebRtc_Word32 StopCapture() = 0;

  // Send an image when the capture device is not running.
  virtual WebRtc_Word32 StartSendImage(const VideoFrame& videoFrame,
                                       WebRtc_Word32 frameRate = 1) = 0;

  virtual WebRtc_Word32 StopSendImage() = 0;

  // Returns the name of the device used by this module.
  virtual const char* CurrentDeviceName() const = 0;

  // Returns true if the capture device is running
  virtual bool CaptureStarted() = 0;

  // Gets the current configuration.
  virtual WebRtc_Word32 CaptureSettings(VideoCaptureCapability& settings) = 0;

  virtual WebRtc_Word32 SetCaptureDelay(WebRtc_Word32 delayMS) = 0;

  // Returns the current CaptureDelay. Only valid when the camera is running.
  virtual WebRtc_Word32 CaptureDelay() = 0;

  // Set the rotation of the captured frames.
  // If the rotation is set to the same as returned by
  // DeviceInfo::GetOrientation the captured frames are
  // displayed correctly if rendered.
  virtual WebRtc_Word32 SetCaptureRotation(VideoCaptureRotation rotation) = 0;

  // Gets a pointer to an encode interface if the capture device supports the
  // requested type and size.  NULL otherwise.
  virtual VideoCaptureEncodeInterface* GetEncodeInterface(
      const VideoCodec& codec) = 0;

  virtual WebRtc_Word32 EnableFrameRateCallback(const bool enable) = 0;
  virtual WebRtc_Word32 EnableNoPictureAlarm(const bool enable) = 0;

protected:
  virtual ~VideoCaptureModule() {};
};

} // namespace webrtc
#endif  // WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_INTERFACE_VIDEO_CAPTURE_H_
