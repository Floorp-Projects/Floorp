/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_capture/mac/avfoundation/video_capture_avfoundation.h"
#import "webrtc/modules/video_capture/mac/avfoundation/video_capture_avfoundation_info_objc.h"
#import "webrtc/modules/video_capture/mac/avfoundation/video_capture_avfoundation_objc.h"
#include "webrtc/modules/video_capture/video_capture_config.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"
#include "webrtc/system_wrappers/include/trace.h"

class nsAutoreleasePool {
public:
    nsAutoreleasePool()
    {
        mLocalPool = [[NSAutoreleasePool alloc] init];
    }
    ~nsAutoreleasePool()
    {
        [mLocalPool release];
    }
private:
    NSAutoreleasePool *mLocalPool;
};

namespace webrtc
{

namespace videocapturemodule
{

VideoCaptureMacAVFoundation::VideoCaptureMacAVFoundation(const int32_t id) :
    VideoCaptureImpl(id),
    _captureDevice(NULL),
    _captureInfo(NULL),
    _isCapturing(false),
    _id(id),
    _captureWidth(AVFOUNDATION_DEFAULT_WIDTH),
    _captureHeight(AVFOUNDATION_DEFAULT_HEIGHT),
    _captureFrameRate(AVFOUNDATION_DEFAULT_FRAME_RATE),
    _captureRawType(kVideoUnknown),
    _frameCount(0)
{

    memset(_currentDeviceNameUTF8, 0, MAX_NAME_LENGTH);
    memset(_currentDeviceUniqueIdUTF8, 0, MAX_NAME_LENGTH);
    memset(_currentDeviceProductUniqueIDUTF8, 0, MAX_NAME_LENGTH);
}

VideoCaptureMacAVFoundation::~VideoCaptureMacAVFoundation()
{

    nsAutoreleasePool localPool;
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCapture, _id,
                 "~VideoCaptureMacAVFoundation() called");
    if(_captureDevice)
    {
        [_captureDevice registerOwner:nil];
        [_captureDevice performSelectorOnMainThread:@selector(stopCapture)
                        withObject:nil
                        waitUntilDone:NO];
        [_captureDevice performSelectorOnMainThread:@selector(release)
                        withObject:nil
                        waitUntilDone:NO];
    }

    if(_captureInfo)
    {
        [_captureInfo release];
    }
}

int32_t VideoCaptureMacAVFoundation::Init(
    const int32_t id, const char* iDeviceUniqueIdUTF8)
{
    CriticalSectionScoped cs(&_apiCs);


    const int32_t nameLength =
        (int32_t) strlen((char*)iDeviceUniqueIdUTF8);
    if(nameLength>kVideoCaptureUniqueNameLength)
        return -1;

    // Store the device name
    _deviceUniqueId = new char[nameLength+1];
    memcpy(_deviceUniqueId, iDeviceUniqueIdUTF8,nameLength+1);

    nsAutoreleasePool localPool;

    _captureDevice = [[VideoCaptureMacAVFoundationObjC alloc] init];
    if(NULL == _captureDevice)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, id,
                     "Failed to create an instance of "
                     "VideoCaptureMacAVFounationObjC");
        return -1;
    }

    [_captureDevice registerOwner:this];

    if(0 == strcmp((char*)iDeviceUniqueIdUTF8, ""))
    {
        // the user doesn't want to set a capture device at this time
        return 0;
    }

    _captureInfo = [[VideoCaptureMacAVFoundationInfoObjC alloc]init];
    if(nil == _captureInfo)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, id,
        "Failed to create an instance of VideoCaptureMacAVFoundationInfoObjC");
        return -1;
    }

    int captureDeviceCount = [[_captureInfo getCaptureDeviceCount]intValue];
    if(captureDeviceCount < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, id,
                     "No Capture Devices Present");
        return -1;
    }

    const int NAME_LENGTH = 1024;
    char deviceNameUTF8[1024] = "";
    char deviceUniqueIdUTF8[1024] = "";
    char deviceProductUniqueIDUTF8[1024] = "";

    bool captureDeviceFound = false;
    for(int index = 0; index < captureDeviceCount; index++){

        memset(deviceNameUTF8, 0, NAME_LENGTH);
        memset(deviceUniqueIdUTF8, 0, NAME_LENGTH);
        memset(deviceProductUniqueIDUTF8, 0, NAME_LENGTH);
        if(-1 == [[_captureInfo getDeviceNamesFromIndex:index
                   DefaultName:deviceNameUTF8 WithLength:NAME_LENGTH
                   AndUniqueID:deviceUniqueIdUTF8 WithLength:NAME_LENGTH
                   AndProductID:deviceProductUniqueIDUTF8
                   WithLength:NAME_LENGTH]intValue])
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                         "GetDeviceName returned -1 for index %d", index);
            return -1;
        }
        if(0 == strcmp((const char*)iDeviceUniqueIdUTF8,
                       (char*)deviceUniqueIdUTF8))
        {
            // we have a match
            captureDeviceFound = true;
            break;
        }
    }

    if(false == captureDeviceFound)
    {
        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, _id,
                     "Failed to find capture device unique ID %s",
                     iDeviceUniqueIdUTF8);
        return -1;
    }

    // at this point we know that the user has passed in a valid camera. Let's
    // set it as the current.
    if(![_captureDevice setCaptureDeviceById:(char*)deviceUniqueIdUTF8])
    {
        strcpy((char*)_deviceUniqueId, (char*)deviceUniqueIdUTF8);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                     "Failed to set capture device %s (unique ID %s) even "
                     "though it was a valid return from "
                     "VideoCaptureMacAVFoundationInfo", deviceNameUTF8,
                     iDeviceUniqueIdUTF8);
        return -1;
    }

    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, _id,
                 "successfully Init VideoCaptureMacAVFoundation" );
    return 0;
}

int32_t VideoCaptureMacAVFoundation::StartCapture(
    const VideoCaptureCapability& capability)
{

    nsAutoreleasePool localPool;
    _captureWidth = capability.width;
    _captureHeight = capability.height;
    _captureFrameRate = capability.maxFPS;
    _captureRawType = capability.rawType;
    _captureDelay = 120;

    [_captureDevice setCaptureHeight:_captureHeight
                               width:_captureWidth
                           frameRate:_captureFrameRate
                             rawType:&_captureRawType];

    [_captureDevice startCapture];
    _isCapturing = true;
    return 0;
}

int32_t VideoCaptureMacAVFoundation::StopCapture()
{
    nsAutoreleasePool localPool;
    [_captureDevice stopCapture];
    _isCapturing = false;
    return 0;
}

bool VideoCaptureMacAVFoundation::CaptureStarted()
{
    return _isCapturing;
}

int32_t VideoCaptureMacAVFoundation::CaptureSettings(VideoCaptureCapability& settings)
{
    settings.width = _captureWidth;
    settings.height = _captureHeight;
    settings.maxFPS = _captureFrameRate;
    settings.rawType = _captureRawType;
    return 0;
}


// ********** begin functions inherited from DeviceInfoImpl **********

struct VideoCaptureCapabilityMacAVFoundation:public VideoCaptureCapability
{
    VideoCaptureCapabilityMacAVFoundation()
    {
    }
};
}  // namespace videocapturemodule
}  // namespace webrtc
