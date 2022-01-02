/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_capture/linux/device_info_linux.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
// v4l includes
#if defined(__NetBSD__) || defined(__OpenBSD__) // WEBRTC_BSD
#include <sys/videoio.h>
#elif defined(__sun)
#include <sys/videodev2.h>
#else
#include <linux/videodev2.h>
#endif

#include <vector>

#include "modules/video_capture/video_capture.h"
#include "modules/video_capture/video_capture_defines.h"
#include "modules/video_capture/video_capture_impl.h"
#include "rtc_base/logging.h"

#ifdef WEBRTC_LINUX
#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )
#endif

namespace webrtc {
namespace videocapturemodule {
VideoCaptureModule::DeviceInfo* VideoCaptureImpl::CreateDeviceInfo() {
  return new videocapturemodule::DeviceInfoLinux();
}

#ifdef WEBRTC_LINUX
void DeviceInfoLinux::HandleEvent(inotify_event* event, int fd)
{
    if (event->mask & IN_CREATE) {
        if (fd == _fd_v4l) {
            DeviceChange();
        } else if ((event->mask & IN_ISDIR) && (fd == _fd_dev)) {
            if (_wd_v4l < 0) {
                // Sometimes inotify_add_watch failed if we call it immediately after receiving this event
                // Adding 5ms delay to let file system settle down
                usleep(5*1000);
                _wd_v4l = inotify_add_watch(_fd_v4l, "/dev/v4l/by-path/", IN_CREATE | IN_DELETE | IN_DELETE_SELF);
                if (_wd_v4l >= 0) {
                    DeviceChange();
                }
            }
        }
    } else if (event->mask & IN_DELETE) {
        if (fd == _fd_v4l) {
            DeviceChange();
        }
    } else if (event->mask & IN_DELETE_SELF) {
        if (fd == _fd_v4l) {
            inotify_rm_watch(_fd_v4l, _wd_v4l);
            _wd_v4l = -1;
        } else {
            assert(false);
        }
    }
}

int DeviceInfoLinux::EventCheck(int fd)
{
    struct pollfd fds = {
      .fd = fd,
      .events = POLLIN,
      .revents = 0
    };

    return poll(&fds, 1, 100);
}

int DeviceInfoLinux::HandleEvents(int fd)
{
    char buffer[BUF_LEN];

    ssize_t r = read(fd, buffer, BUF_LEN);

    if (r <= 0) {
        return r;
    }

    ssize_t buffer_i = 0;
    inotify_event* pevent;
    size_t eventSize;
    int count = 0;

    while (buffer_i < r)
    {
        pevent = (inotify_event *) (&buffer[buffer_i]);
        eventSize = sizeof(inotify_event) + pevent->len;
        char event[sizeof(inotify_event) + FILENAME_MAX + 1] // null-terminated
            __attribute__ ((aligned(__alignof__(struct inotify_event))));

        memcpy(event, pevent, eventSize);

        HandleEvent((inotify_event*)(event), fd);

        buffer_i += eventSize;
        count++;
    }

    return count;
}

int DeviceInfoLinux::ProcessInotifyEvents()
{
    while (!_isShutdown) {
        if (EventCheck(_fd_dev) > 0) {
            if (HandleEvents(_fd_dev) < 0) {
                break;
            }
        }
        if (EventCheck(_fd_v4l) > 0) {
            if (HandleEvents(_fd_v4l) < 0) {
                break;
            }
        }
    }
    return 0;
}

void DeviceInfoLinux::InotifyEventThread(void* obj)
{
    static_cast<DeviceInfoLinux*> (obj)->InotifyProcess();
}

void DeviceInfoLinux::InotifyProcess()
{
    _fd_v4l = inotify_init();
    _fd_dev = inotify_init();
    if (_fd_v4l >= 0 && _fd_dev >= 0) {
        _wd_v4l = inotify_add_watch(_fd_v4l, "/dev/v4l/by-path/", IN_CREATE | IN_DELETE | IN_DELETE_SELF);
        _wd_dev = inotify_add_watch(_fd_dev, "/dev/", IN_CREATE);
        ProcessInotifyEvents();

        if (_wd_v4l >= 0) {
          inotify_rm_watch(_fd_v4l, _wd_v4l);
        }

        if (_wd_dev >= 0) {
          inotify_rm_watch(_fd_dev, _wd_dev);
        }

        close(_fd_v4l);
        close(_fd_dev);
    }
}
#endif

DeviceInfoLinux::DeviceInfoLinux() : DeviceInfoImpl()
#ifdef WEBRTC_LINUX
    , _inotifyEventThread(new rtc::PlatformThread(
                            InotifyEventThread, this, "InotifyEventThread"))
    , _isShutdown(false)
#endif
{
#ifdef WEBRTC_LINUX
    if (_inotifyEventThread)
    {
        _inotifyEventThread->Start();
    }
#endif
}

int32_t DeviceInfoLinux::Init() {
  return 0;
}

DeviceInfoLinux::~DeviceInfoLinux() {
#ifdef WEBRTC_LINUX
    _isShutdown = true;

    if (_inotifyEventThread) {
        _inotifyEventThread->Stop();
        _inotifyEventThread = nullptr;
    }
#endif
}

uint32_t DeviceInfoLinux::NumberOfDevices() {
  RTC_LOG(LS_INFO) << __FUNCTION__;

  uint32_t count = 0;
  char device[20];
  int fd = -1;
  struct v4l2_capability cap;

  /* detect /dev/video [0-63]VideoCaptureModule entries */
  for (int n = 0; n < 64; n++) {
    sprintf(device, "/dev/video%d", n);
    if ((fd = open(device, O_RDONLY)) != -1) {
      // query device capabilities and make sure this is a video capture device
      if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0 || !IsVideoCaptureDevice(&cap)) {
        close(fd);
        continue;
      }

      close(fd);
      count++;
    }
  }

  return count;
}

int32_t DeviceInfoLinux::GetDeviceName(uint32_t deviceNumber,
                                       char* deviceNameUTF8,
                                       uint32_t deviceNameLength,
                                       char* deviceUniqueIdUTF8,
                                       uint32_t deviceUniqueIdUTF8Length,
                                       char* /*productUniqueIdUTF8*/,
                                       uint32_t /*productUniqueIdUTF8Length*/,
                                       pid_t* /*pid*/) {
  RTC_LOG(LS_INFO) << __FUNCTION__;

  // Travel through /dev/video [0-63]
  uint32_t count = 0;
  char device[20];
  int fd = -1;
  bool found = false;
  struct v4l2_capability cap;
  int device_index;
  for (device_index = 0; device_index < 64; device_index++) {
    sprintf(device, "/dev/video%d", device_index);
    if ((fd = open(device, O_RDONLY)) != -1) {
      // query device capabilities and make sure this is a video capture device
      if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0 || !IsVideoCaptureDevice(&cap)) {
        close(fd);
        continue;
      }
      if (count == deviceNumber) {
        // Found the device
        found = true;
        break;
      } else {
        close(fd);
        count++;
      }
    }
  }

  if (!found)
    return -1;

  // query device capabilities
  if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
    RTC_LOG(LS_INFO) << "error in querying the device capability for device "
                     << device << ". errno = " << errno;
    close(fd);
    return -1;
  }

  close(fd);

  char cameraName[64];
  memset(deviceNameUTF8, 0, deviceNameLength);
  memcpy(cameraName, cap.card, sizeof(cap.card));

  if (deviceNameLength >= strlen(cameraName)) {
    memcpy(deviceNameUTF8, cameraName, strlen(cameraName));
  } else {
    RTC_LOG(LS_INFO) << "buffer passed is too small";
    return -1;
  }

  if (cap.bus_info[0] != 0)  // may not available in all drivers
  {
    // copy device id
    if (deviceUniqueIdUTF8Length >= strlen((const char*)cap.bus_info)) {
      memset(deviceUniqueIdUTF8, 0, deviceUniqueIdUTF8Length);
      memcpy(deviceUniqueIdUTF8, cap.bus_info,
             strlen((const char*)cap.bus_info));
    } else {
      RTC_LOG(LS_INFO) << "buffer passed is too small";
      return -1;
    }
  } else {
    // if there's no bus info to use for uniqueId, invent one - and it has to be repeatable
    if (snprintf(deviceUniqueIdUTF8,
                 deviceUniqueIdUTF8Length, "fake_%u", device_index) >=
        (int) deviceUniqueIdUTF8Length)
    {
      return -1;
    }
  }
  return 0;
}

int32_t DeviceInfoLinux::CreateCapabilityMap(const char* deviceUniqueIdUTF8) {
  int fd;
  char device[32];
  bool found = false;

  const int32_t deviceUniqueIdUTF8Length =
      (int32_t)strlen((char*)deviceUniqueIdUTF8);
  if (deviceUniqueIdUTF8Length > kVideoCaptureUniqueNameLength) {
    RTC_LOG(LS_INFO) << "Device name too long";
    return -1;
  }
  RTC_LOG(LS_INFO) << "CreateCapabilityMap called for device "
                   << deviceUniqueIdUTF8;

  /* detect /dev/video [0-63] entries */
  for (int n = 0; n < 64; ++n) {
    sprintf(device, "/dev/video%d", n);
    fd = open(device, O_RDONLY);
    if (fd == -1)
      continue;

    // query device capabilities
    struct v4l2_capability cap;
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0) {
      // skip devices without video capture capability
      if (!IsVideoCaptureDevice(&cap)) {
        close(fd);
        continue;
      }

      if (cap.bus_info[0] != 0) {
        if (strncmp((const char*)cap.bus_info, (const char*)deviceUniqueIdUTF8,
                    strlen((const char*)deviceUniqueIdUTF8)) ==
            0)  // match with device id
        {
          found = true;
          break;  // fd matches with device unique id supplied
        }
      } else  // match for device name
      {
        if (IsDeviceNameMatches((const char*)cap.card,
                                (const char*)deviceUniqueIdUTF8)) {
          found = true;
          break;
        }
      }
    }
    close(fd);  // close since this is not the matching device
  }

  if (!found) {
    RTC_LOG(LS_INFO) << "no matching device found";
    return -1;
  }

  // now fd will point to the matching device
  // reset old capability list.
  _captureCapabilities.clear();

  int size = FillCapabilities(fd);
  close(fd);

  // Store the new used device name
  _lastUsedDeviceNameLength = deviceUniqueIdUTF8Length;
  _lastUsedDeviceName =
      (char*)realloc(_lastUsedDeviceName, _lastUsedDeviceNameLength + 1);
  memcpy(_lastUsedDeviceName, deviceUniqueIdUTF8,
         _lastUsedDeviceNameLength + 1);

  RTC_LOG(LS_INFO) << "CreateCapabilityMap " << _captureCapabilities.size();

  return size;
}

int32_t DeviceInfoLinux::DisplayCaptureSettingsDialogBox(
    const char* /*deviceUniqueIdUTF8*/,
    const char* /*dialogTitleUTF8*/,
    void* /*parentWindow*/,
    uint32_t /*positionX*/,
    uint32_t /*positionY*/) {
  return -1;
}

bool DeviceInfoLinux::IsDeviceNameMatches(const char* name,
                                          const char* deviceUniqueIdUTF8) {
  if (strncmp(deviceUniqueIdUTF8, name, strlen(name)) == 0)
    return true;
  return false;
}

bool DeviceInfoLinux::IsVideoCaptureDevice(struct v4l2_capability* cap)
{
  if (cap->capabilities & V4L2_CAP_DEVICE_CAPS) {
    return cap->device_caps & V4L2_CAP_VIDEO_CAPTURE;
  } else {
    return cap->capabilities & V4L2_CAP_VIDEO_CAPTURE;
  }
}

int32_t DeviceInfoLinux::FillCapabilities(int fd) {
  // set image format
  struct v4l2_format video_fmt;
  memset(&video_fmt, 0, sizeof(struct v4l2_format));

  video_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  video_fmt.fmt.pix.sizeimage = 0;

  int totalFmts = 4;
  unsigned int videoFormats[] = {V4L2_PIX_FMT_MJPEG, V4L2_PIX_FMT_YUV420,
                                 V4L2_PIX_FMT_YUYV, V4L2_PIX_FMT_UYVY};

  int sizes = 13;
  unsigned int size[][2] = {{128, 96},   {160, 120},  {176, 144},  {320, 240},
                            {352, 288},  {640, 480},  {704, 576},  {800, 600},
                            {960, 720},  {1280, 720}, {1024, 768}, {1440, 1080},
                            {1920, 1080}};

  for (int fmts = 0; fmts < totalFmts; fmts++) {
    for (int i = 0; i < sizes; i++) {
      video_fmt.fmt.pix.pixelformat = videoFormats[fmts];
      video_fmt.fmt.pix.width = size[i][0];
      video_fmt.fmt.pix.height = size[i][1];

      if (ioctl(fd, VIDIOC_TRY_FMT, &video_fmt) >= 0) {
        if ((video_fmt.fmt.pix.width == size[i][0]) &&
            (video_fmt.fmt.pix.height == size[i][1])) {
          VideoCaptureCapability cap;
          cap.width = video_fmt.fmt.pix.width;
          cap.height = video_fmt.fmt.pix.height;
          if (videoFormats[fmts] == V4L2_PIX_FMT_YUYV) {
            cap.videoType = VideoType::kYUY2;
          } else if (videoFormats[fmts] == V4L2_PIX_FMT_YUV420) {
            cap.videoType = VideoType::kI420;
          } else if (videoFormats[fmts] == V4L2_PIX_FMT_MJPEG) {
            cap.videoType = VideoType::kMJPEG;
          } else if (videoFormats[fmts] == V4L2_PIX_FMT_UYVY) {
            cap.videoType = VideoType::kUYVY;
          }

          // get fps of current camera mode
          // V4l2 does not have a stable method of knowing so we just guess.
          if (cap.width >= 800 && cap.videoType != VideoType::kMJPEG) {
            cap.maxFPS = 15;
          } else {
            cap.maxFPS = 30;
          }

          _captureCapabilities.push_back(cap);
          RTC_LOG(LS_VERBOSE) << "Camera capability, width:" << cap.width
                              << " height:" << cap.height
                              << " type:" << static_cast<int32_t>(cap.videoType)
                              << " fps:" << cap.maxFPS;
        }
      }
    }
  }

  RTC_LOG(LS_INFO) << "CreateCapabilityMap " << _captureCapabilities.size();
  return _captureCapabilities.size();
}

}  // namespace videocapturemodule
}  // namespace webrtc
