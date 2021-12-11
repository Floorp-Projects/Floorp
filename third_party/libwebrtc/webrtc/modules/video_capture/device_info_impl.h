/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CAPTURE_MAIN_SOURCE_DEVICE_INFO_IMPL_H_
#define MODULES_VIDEO_CAPTURE_MAIN_SOURCE_DEVICE_INFO_IMPL_H_

#include <vector>

#include "modules/video_capture/video_capture.h"
#include "system_wrappers/include/rw_lock_wrapper.h"

namespace webrtc
{
namespace videocapturemodule
{
class DeviceInfoImpl: public VideoCaptureModule::DeviceInfo
{
public:
    DeviceInfoImpl();
    virtual ~DeviceInfoImpl(void);
    virtual int32_t NumberOfCapabilities(const char* deviceUniqueIdUTF8);
    virtual int32_t GetCapability(
        const char* deviceUniqueIdUTF8,
        const uint32_t deviceCapabilityNumber,
        VideoCaptureCapability& capability);

    virtual int32_t GetBestMatchedCapability(
        const char* deviceUniqueIdUTF8,
        const VideoCaptureCapability& requested,
        VideoCaptureCapability& resulting);
    virtual int32_t GetOrientation(const char* deviceUniqueIdUTF8,
                                   VideoRotation& orientation);

protected:
    /* Initialize this object*/

    virtual int32_t Init()=0;
    virtual int32_t Refresh() { return 0; }
    /*
     * Fills the member variable _captureCapabilities with capabilities for the given device name.
     */
    virtual int32_t CreateCapabilityMap(const char* deviceUniqueIdUTF8)=0;

protected:
    // Data members
    typedef std::vector<VideoCaptureCapability> VideoCaptureCapabilities;
    VideoCaptureCapabilities _captureCapabilities;
    RWLockWrapper& _apiLock;
    char* _lastUsedDeviceName;
    uint32_t _lastUsedDeviceNameLength;
};
}  // namespace videocapturemodule
}  // namespace webrtc
#endif // MODULES_VIDEO_CAPTURE_MAIN_SOURCE_DEVICE_INFO_IMPL_H_
