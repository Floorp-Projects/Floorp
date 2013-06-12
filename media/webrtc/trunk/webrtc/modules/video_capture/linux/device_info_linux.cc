/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "device_info_linux.h"

#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

//v4l includes
#include <linux/videodev2.h>

#include "ref_count.h"
#include "trace.h"


namespace webrtc
{
namespace videocapturemodule
{
VideoCaptureModule::DeviceInfo*
VideoCaptureImpl::CreateDeviceInfo(const WebRtc_Word32 id)
{
    videocapturemodule::DeviceInfoLinux *deviceInfo =
                    new videocapturemodule::DeviceInfoLinux(id);
    if (!deviceInfo)
    {
        deviceInfo = NULL;
    }

    return deviceInfo;
}

DeviceInfoLinux::DeviceInfoLinux(const WebRtc_Word32 id)
    : DeviceInfoImpl(id)
{
}

WebRtc_Word32 DeviceInfoLinux::Init()
{
    return 0;
}

DeviceInfoLinux::~DeviceInfoLinux()
{
}

WebRtc_UWord32 DeviceInfoLinux::NumberOfDevices()
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideoCapture, _id, "%s", __FUNCTION__);

    WebRtc_UWord32 count = 0;
    char device[20];
    int fd = -1;

    /* detect /dev/video [0-63]VideoCaptureModule entries */
    for (int n = 0; n < 64; n++)
    {
        sprintf(device, "/dev/video%d", n);
        if ((fd = open(device, O_RDONLY)) != -1)
        {
            close(fd);
            count++;
        }
    }

    return count;
}

WebRtc_Word32 DeviceInfoLinux::GetDeviceName(
                                         WebRtc_UWord32 deviceNumber,
                                         char* deviceNameUTF8,
                                         WebRtc_UWord32 deviceNameLength,
                                         char* deviceUniqueIdUTF8,
                                         WebRtc_UWord32 deviceUniqueIdUTF8Length,
                                         char* /*productUniqueIdUTF8*/,
                                         WebRtc_UWord32 /*productUniqueIdUTF8Length*/)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideoCapture, _id, "%s", __FUNCTION__);

    // Travel through /dev/video [0-63]
    WebRtc_UWord32 count = 0;
    char device[20];
    int fd = -1;
    bool found = false;
    int device_index;
    for (device_index = 0; device_index < 64; device_index++)
    {
        sprintf(device, "/dev/video%d", device_index);
        if ((fd = open(device, O_RDONLY)) != -1)
        {
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
    struct v4l2_capability cap;
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                   "error in querying the device capability for device %s. errno = %d",
                   device, errno);
        close(fd);
        return -1;
    }

    close(fd);

    char cameraName[64];
    memset(deviceNameUTF8, 0, deviceNameLength);
    memcpy(cameraName, cap.card, sizeof(cap.card));

    if (deviceNameLength >= strlen(cameraName))
    {
        memcpy(deviceNameUTF8, cameraName, strlen(cameraName));
    }
    else
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id, "buffer passed is too small");
        return -1;
    }

    if (cap.bus_info[0] != 0) // may not available in all drivers
    {
        // copy device id
        if (deviceUniqueIdUTF8Length >= strlen((const char*) cap.bus_info))
        {
            memset(deviceUniqueIdUTF8, 0, deviceUniqueIdUTF8Length);
            memcpy(deviceUniqueIdUTF8, cap.bus_info,
                   strlen((const char*) cap.bus_info));
        }
        else
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                       "buffer passed is too small");
            return -1;
        }
    } else {
        // if there's no bus info to use for uniqueId, invent one - and it has to be repeatable
        if (snprintf(deviceUniqueIdUTF8, deviceUniqueIdUTF8Length, "fake_%u", device_index) >=
            deviceUniqueIdUTF8Length)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                       "buffer passed is too small");
            return -1;
        }
    }

    return 0;
}

WebRtc_Word32 DeviceInfoLinux::CreateCapabilityMap(
                                        const char* deviceUniqueIdUTF8)
{
    int fd;
    char device[32];
    bool found = false;
    int device_index;

    const WebRtc_Word32 deviceUniqueIdUTF8Length =
                            (WebRtc_Word32) strlen((char*) deviceUniqueIdUTF8);
    if (deviceUniqueIdUTF8Length > kVideoCaptureUniqueNameLength)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id, "Device name too long");
        return -1;
    }
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, _id,
               "CreateCapabilityMap called for device %s", deviceUniqueIdUTF8);

    if (sscanf(deviceUniqueIdUTF8,"fake_%d",&device_index) == 1)
    {
        sprintf(device, "/dev/video%d", device_index);
        fd = open(device, O_RDONLY);
        if (fd != -1) {
            found = true;
        }
    } else {
        /* detect /dev/video [0-63] entries */
        for (int n = 0; n < 64; ++n)
        {
            sprintf(device, "/dev/video%d", n);
            fd = open(device, O_RDONLY);
            if (fd == -1)
                continue;

            // query device capabilities
            struct v4l2_capability cap;
            if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0)
            {
                if (cap.bus_info[0] != 0)
                {
                    if (strncmp((const char*) cap.bus_info,
                                (const char*) deviceUniqueIdUTF8,
                                strlen((const char*) deviceUniqueIdUTF8)) == 0) //match with device id
                    {
                        found = true;
                        break; // fd matches with device unique id supplied
                    }
                }
                // else can't be a match as the test for fake_* above would have matched it
            }
            close(fd); // close since this is not the matching device
        }
    }
    if (!found)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id, "no matching device found");
        return -1;
    }

    // now fd will point to the matching device
    // reset old capability map
    MapItem* item = NULL;
    while ((item = _captureCapabilities.Last()))
    {
        delete static_cast<VideoCaptureCapability*> (item->GetItem());
        _captureCapabilities.Erase(item);
    }

    int size = FillCapabilityMap(fd);
    close(fd);

    // Store the new used device name
    _lastUsedDeviceNameLength = deviceUniqueIdUTF8Length;
    _lastUsedDeviceName = (char*) realloc(_lastUsedDeviceName,
                                                   _lastUsedDeviceNameLength + 1);
    memcpy(_lastUsedDeviceName, deviceUniqueIdUTF8, _lastUsedDeviceNameLength + 1);

    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, _id, "CreateCapabilityMap %d",
               _captureCapabilities.Size());

    return size;
}

bool DeviceInfoLinux::IsDeviceNameMatches(const char* name,
                                          const char* deviceUniqueIdUTF8)
{
    if (strncmp(deviceUniqueIdUTF8, name, strlen(name)) == 0)
            return true;
    return false;
}

WebRtc_Word32 DeviceInfoLinux::FillCapabilityMap(int fd)
{

    // set image format
    struct v4l2_format video_fmt;
    memset(&video_fmt, 0, sizeof(struct v4l2_format));

    video_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    video_fmt.fmt.pix.sizeimage = 0;

    int totalFmts = 3;
    unsigned int videoFormats[] = {
        V4L2_PIX_FMT_MJPEG,
        V4L2_PIX_FMT_YUV420,
        V4L2_PIX_FMT_YUYV };

    int sizes = 13;
    unsigned int size[][2] = { { 128, 96 }, { 160, 120 }, { 176, 144 },
                               { 320, 240 }, { 352, 288 }, { 640, 480 },
                               { 704, 576 }, { 800, 600 }, { 960, 720 },
                               { 1280, 720 }, { 1024, 768 }, { 1440, 1080 },
                               { 1920, 1080 } };

    int index = 0;
    for (int fmts = 0; fmts < totalFmts; fmts++)
    {
        for (int i = 0; i < sizes; i++)
        {
            video_fmt.fmt.pix.pixelformat = videoFormats[fmts];
            video_fmt.fmt.pix.width = size[i][0];
            video_fmt.fmt.pix.height = size[i][1];

            if (ioctl(fd, VIDIOC_TRY_FMT, &video_fmt) >= 0)
            {
                if ((video_fmt.fmt.pix.width == size[i][0])
                    && (video_fmt.fmt.pix.height == size[i][1]))
                {
                    VideoCaptureCapability *cap = new VideoCaptureCapability();
                    cap->width = video_fmt.fmt.pix.width;
                    cap->height = video_fmt.fmt.pix.height;
                    cap->expectedCaptureDelay = 120;
                    if (videoFormats[fmts] == V4L2_PIX_FMT_YUYV)
                    {
                        cap->rawType = kVideoYUY2;
                    }
                    else if (videoFormats[fmts] == V4L2_PIX_FMT_MJPEG)
                    {
                        cap->rawType = kVideoMJPEG;
                    }

                    // get fps of current camera mode
                    // V4l2 does not have a stable method of knowing so we just guess.
                    if(cap->width >= 800 && cap->rawType != kVideoMJPEG)
                    {
                        cap->maxFPS = 15;
                    }
                    else
                    {
                        cap->maxFPS = 30;
                    }

                    _captureCapabilities.Insert(index, cap);
                    index++;
                    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, _id,
                               "Camera capability, width:%d height:%d type:%d fps:%d",
                               cap->width, cap->height, cap->rawType, cap->maxFPS);
                }
            }
        }
    }

    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, _id, "CreateCapabilityMap %d",
               _captureCapabilities.Size());
    return _captureCapabilities.Size();
}

} // namespace videocapturemodule
} // namespace webrtc
