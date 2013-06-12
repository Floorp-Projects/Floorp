/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 *  video_capture_quick_time_info.h
 *
 */

#ifndef WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_MAC_QUICKTIME_VIDEO_CAPTURE_QUICK_TIME_INFO_H_
#define WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_MAC_QUICKTIME_VIDEO_CAPTURE_QUICK_TIME_INFO_H_

#include <QuickTime/QuickTime.h>

#include "../../video_capture_impl.h"
#include "../../device_info_impl.h"
#include "list_wrapper.h"
#include "map_wrapper.h"

class VideoRenderCallback;

namespace webrtc
{
class CriticalSectionWrapper;
class EventWrapper;
class ThreadWrapper;
class Trace;

class VideoCaptureMacQuickTimeInfo: public DeviceInfoImpl
{
public:

    static DeviceInfo* Create(const int32_t id);
    static void Destroy(DeviceInfo* deviceInfo);

    VideoCaptureMacQuickTimeInfo(const int32_t id);
    virtual ~VideoCaptureMacQuickTimeInfo();

    int32_t Init();

    virtual uint32_t NumberOfDevices();

    /*
     * Returns the available capture devices.
     * deviceNumber   -[in] index of capture device
     * deviceNameUTF8 - friendly name of the capture device
     * deviceUniqueIdUTF8 - unique name of the capture device if it exist.
     *                      Otherwise same as deviceNameUTF8
     * productUniqueIdUTF8 - unique product id if it exist. Null terminated
     *                       otherwise.
     */
    virtual int32_t GetDeviceName(
        uint32_t deviceNumber, char* deviceNameUTF8,
        uint32_t deviceNameLength, char* deviceUniqueIdUTF8,
        uint32_t deviceUniqueIdUTF8Length,
        char* productUniqueIdUTF8 = 0,
        uint32_t productUniqueIdUTF8Length = 0);


    // ************** The remaining public functions are not supported on Mac

    /*
     *   Returns the number of capabilities for this device
     */
    virtual int32_t NumberOfCapabilities(const char* deviceUniqueIdUTF8);

    /*
     *   Gets the capabilities of the named device
     */
    virtual int32_t GetCapability(
        const char* deviceUniqueIdUTF8,
        const uint32_t deviceCapabilityNumber,
        VideoCaptureCapability& capability);

    /*
     *  Gets the capability that best matches the requested width, height and frame rate.
     *  Returns the deviceCapabilityNumber on success.
     */
    virtual int32_t GetBestMatchedCapability(
        const char* deviceUniqueIdUTF8,
        const VideoCaptureCapability& requested,
        VideoCaptureCapability& resulting);

    /*
     * Display OS /capture device specific settings dialog
     */
    virtual int32_t DisplayCaptureSettingsDialogBox(
        const char* deviceUniqueIdUTF8,
        const char* dialogTitleUTF8, void* parentWindow,
        uint32_t positionX, uint32_t positionY);

protected:
    virtual int32_t CreateCapabilityMap(
        const char* deviceUniqueIdUTF8);

private:

    struct VideoCaptureMacName
    {
        VideoCaptureMacName();

        enum
        {
            kVideoCaptureMacNameMaxSize = 64
        };
        char _name[kVideoCaptureMacNameMaxSize];
        CFIndex _size;
    };

    enum
    {
        kVideoCaptureMacDeviceListTimeout = 5000
    }; // Timeout value [ms] if we want to create a new device list or not
    enum
    {
        kYuy2_1280_1024_length = 2621440
    }; // Temporary constant allowing this size from built-in iSight webcams.

private:
    // private methods

    int GetCaptureDevices(uint32_t deviceNumber,
                          char* deviceNameUTF8,
                          uint32_t deviceNameUTF8Length,
                          char* deviceUniqueIdUTF8,
                          uint32_t deviceUniqueIdUTF8Length,
                          char* productUniqueIdUTF8,
                          uint32_t productUniqueIdUTF8Length,
                          int& numberOfDevices);

    static CFIndex PascalStringToCString(const unsigned char* pascalString,
                                         char* cString, CFIndex bufferSize);

private:
    // member vars
    int32_t _id;
    bool _terminated;
    CriticalSectionWrapper* _grabberCritsect;
    webrtc::Trace* _trace;
    webrtc::ThreadWrapper* _grabberUpdateThread;
    webrtc::EventWrapper* _grabberUpdateEvent;
    SeqGrabComponent _captureGrabber;
    Component _captureDevice;
    char _captureDeviceDisplayName[64];
    bool _captureIsInitialized;
    GWorldPtr _gWorld;
    SGChannel _captureChannel;
    ImageSequence _captureSequence;
    bool _sgPrepared;
    bool _sgStarted;
    int _codecWidth;
    int _codecHeight;
    int _trueCaptureWidth;
    int _trueCaptureHeight;
    ListWrapper _captureDeviceList;
    int64_t _captureDeviceListTime;
    ListWrapper _captureCapabilityList;
};
}  // namespace webrtc
#endif  // WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_MAC_QUICKTIME_VIDEO_CAPTURE_QUICK_TIME_INFO_H_
