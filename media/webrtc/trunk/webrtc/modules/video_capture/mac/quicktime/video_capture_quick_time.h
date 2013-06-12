/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 *  video_capture_quick_time.h
 *
 */

#ifndef WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_MAC_QUICKTIME_VIDEO_CAPTURE_QUICK_TIME_H_
#define WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_MAC_QUICKTIME_VIDEO_CAPTURE_QUICK_TIME_H_

#include <QuickTime/QuickTime.h>

#include "../../device_info_impl.h"
#include "../../video_capture_impl.h"
#include "list_wrapper.h"

#define START_CODEC_WIDTH 352
#define START_CODEC_HEIGHT 288
#define SLEEP(x) usleep(x * 1000);

namespace webrtc {
class CriticalSectionWrapper;
class EventWrapper;
class ThreadWrapper;

class VideoCaptureMacQuickTime : public VideoCaptureImpl {

 public:
  VideoCaptureMacQuickTime(const int32_t id);
  virtual ~VideoCaptureMacQuickTime();

  static void Destroy(VideoCaptureModule* module);

  int32_t Init(const int32_t id, const uint8_t* deviceUniqueIdUTF8);
  virtual int32_t StartCapture(const VideoCaptureCapability& capability);
  virtual int32_t StopCapture();
  virtual bool CaptureStarted();
  virtual int32_t CaptureSettings(VideoCaptureCapability& settings);

  // TODO: remove?
  int VideoCaptureInitThreadContext();
  int VideoCaptureTerminate();
  int VideoCaptureSetCaptureDevice(const char* deviceName, int size);
  int UpdateCaptureSettings(int channel, webrtc::VideoCodec& inst, bool def);
  int VideoCaptureRun();
  int VideoCaptureStop();

 protected:

 private:  // functions

  struct VideoCaptureMacName {
    VideoCaptureMacName();

    enum {
      kVideoCaptureMacNameMaxSize = 64
    };
    char _name[kVideoCaptureMacNameMaxSize];
    CFIndex _size;
  };

  // Timeout value [ms] if we want to create a new device list or not
  enum {
    kVideoCaptureDeviceListTimeout = 5000
  };
  // Temporary constant allowing this size from builtin iSight webcams.
  enum {
    kYuy2_1280_1024_length = 2621440
  };

 private:

  // Capture device callback
  static OSErr SendProcess(SGChannel sgChannel, Ptr p, long len, long* offset,
                           long chRefCon, TimeValue time, short writeType,
                           long refCon);
  int SendFrame(SGChannel sgChannel, char* data, long length, TimeValue time);

  // Capture device functions
  int CreateLocalGWorld(int width, int height);
  int RemoveLocalGWorld();
  int ConnectCaptureDevice();
  int DisconnectCaptureDevice();
  virtual bool IsCaptureDeviceSelected();

  // Process to make sure the capture device won't stop
  static bool GrabberUpdateThread(void*);
  bool GrabberUpdateProcess();

  // Starts and stops the capture
  int StartQuickTimeCapture();
  int StopQuickTimeCapture(bool* wasCapturing = NULL);

  static CFIndex PascalStringToCString(const unsigned char* pascalString,
                                       char* cString, CFIndex bufferSize);

 private :  // variables
  int32_t _id;
  bool _isCapturing;
  VideoCaptureCapability _captureCapability;
  CriticalSectionWrapper* _grabberCritsect;
  CriticalSectionWrapper* _videoMacCritsect;
  bool _terminated;
  webrtc::ThreadWrapper* _grabberUpdateThread;
  webrtc::EventWrapper* _grabberUpdateEvent;
  SeqGrabComponent _captureGrabber;
  Component _captureDevice;
  char _captureDeviceDisplayName[64];
  RawVideoType _captureVideoType;
  bool _captureIsInitialized;
  GWorldPtr _gWorld;
  SGChannel _captureChannel;
  ImageSequence _captureSequence;
  bool _sgPrepared;
  bool _sgStarted;
  int _trueCaptureWidth;
  int _trueCaptureHeight;
  ListWrapper _captureDeviceList;
  unsigned long _captureDeviceListTime;
  ListWrapper _captureCapabilityList;
};
}  // namespace webrtc
#endif  // WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_MAC_QUICKTIME_VIDEO_CAPTURE_QUICK_TIME_H_
