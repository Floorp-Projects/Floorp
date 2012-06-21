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
 *  video_capture_quick_time_info.cc
 *
 */

#include "../../video_capture_config.h"
#include "video_capture_quick_time_info.h"

#include "critical_section_wrapper.h"
#include "event_wrapper.h"
#include "thread_wrapper.h"
#include "trace.h"
#include "video_capture.h"

namespace webrtc
{

VideoCaptureMacQuickTimeInfo::VideoCaptureMacQuickTimeInfo(
    const WebRtc_Word32 iID) :
    DeviceInfoImpl(iID), _id(iID),
    _grabberCritsect(CriticalSectionWrapper::CreateCriticalSection())
{
}

VideoCaptureMacQuickTimeInfo::~VideoCaptureMacQuickTimeInfo()
{
}

WebRtc_Word32 VideoCaptureMacQuickTimeInfo::Init()
{

    return 0;
}

WebRtc_UWord32 VideoCaptureMacQuickTimeInfo::NumberOfDevices()
{
    int numOfDevices = 0;

    // don't care about these variables... dummy vars to call GetCaptureDevices
    const int kNameLength = 1024;
    char deviceNameUTF8[kNameLength] = "";
    char deviceUniqueIdUTF8[kNameLength] = "";
    char productUniqueIdUTF8[kNameLength] = "";

    if (GetCaptureDevices(0, deviceNameUTF8, kNameLength, deviceUniqueIdUTF8,
                          kNameLength, productUniqueIdUTF8, kNameLength,
                          numOfDevices) != 0)
    {
        return 0;
    }

    return numOfDevices;
}

WebRtc_Word32 VideoCaptureMacQuickTimeInfo::GetDeviceName(
    WebRtc_UWord32 deviceNumber, char* deviceNameUTF8,
    WebRtc_UWord32 deviceNameUTF8Length, char* deviceUniqueIdUTF8,
    WebRtc_UWord32 deviceUniqueIdUTF8Length, char* productUniqueIdUTF8,
    WebRtc_UWord32 productUniqueIdUTF8Length)
{

    int numOfDevices = 0; // not needed for this function
    return GetCaptureDevices(deviceNumber, deviceNameUTF8,
                             deviceNameUTF8Length, deviceUniqueIdUTF8,
                             deviceUniqueIdUTF8Length, productUniqueIdUTF8,
                             productUniqueIdUTF8Length, numOfDevices);
}

WebRtc_Word32 VideoCaptureMacQuickTimeInfo::NumberOfCapabilities(
    const char* deviceUniqueIdUTF8)
{
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                 "NumberOfCapabilities is not supported on the Mac platform.");
    return -1;
}

WebRtc_Word32 VideoCaptureMacQuickTimeInfo::GetCapability(
    const char* deviceUniqueIdUTF8,
    const WebRtc_UWord32 deviceCapabilityNumber,
    VideoCaptureCapability& capability)
{
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                 "NumberOfCapabilities is not supported on the Mac platform.");
    return -1;
}

WebRtc_Word32 VideoCaptureMacQuickTimeInfo::GetBestMatchedCapability(
    const char*deviceUniqueIdUTF8,
    const VideoCaptureCapability& requested, VideoCaptureCapability& resulting)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, _id,
                 "NumberOfCapabilities is not supported on the Mac platform.");
    return -1;
}

WebRtc_Word32 VideoCaptureMacQuickTimeInfo::DisplayCaptureSettingsDialogBox(
    const char* deviceUniqueIdUTF8,
    const char* dialogTitleUTF8, void* parentWindow,
    WebRtc_UWord32 positionX, WebRtc_UWord32 positionY)
{
     return -1;
}

WebRtc_Word32 VideoCaptureMacQuickTimeInfo::CreateCapabilityMap(
    const char* deviceUniqueIdUTF8)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, _id,
                 "NumberOfCapabilities is not supported on the Mac platform.");
    return -1;
}

int VideoCaptureMacQuickTimeInfo::GetCaptureDevices(
    WebRtc_UWord32 deviceNumber, char* deviceNameUTF8,
    WebRtc_UWord32 deviceNameUTF8Length, char* deviceUniqueIdUTF8,
    WebRtc_UWord32 deviceUniqueIdUTF8Length, char* productUniqueIdUTF8,
    WebRtc_UWord32 productUniqueIdUTF8Length, int& numberOfDevices)
{


    numberOfDevices = 0;
    memset(deviceNameUTF8, 0, deviceNameUTF8Length);
    memset(deviceUniqueIdUTF8, 0, deviceUniqueIdUTF8Length);
    memset(productUniqueIdUTF8, 0, productUniqueIdUTF8Length);

    if (deviceNumber < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                     "%s:%d Invalid deviceNumber", __FUNCTION__, __LINE__);
        return -1;
    }

    Component captureDevice = NULL;
    SeqGrabComponent captureGrabber = NULL;
    SGChannel captureChannel = NULL;
    bool closeChannel = false;

    ComponentDescription compCaptureType;

    compCaptureType.componentType = SeqGrabComponentType;
    compCaptureType.componentSubType = 0;
    compCaptureType.componentManufacturer = 0;
    compCaptureType.componentFlags = 0;
    compCaptureType.componentFlagsMask = 0;

    // Get the number of sequence grabbers
    long numSequenceGrabbers = CountComponents(&compCaptureType);

    if (deviceNumber > numSequenceGrabbers)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                     "%s:%d Invalid deviceNumber", __FUNCTION__, __LINE__);
        return -1;
    }

    if (numSequenceGrabbers <= 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                     "%s:%d No sequence grabbers available", __FUNCTION__,
                     __LINE__);
        return -1;
    }

    // Open a sequence grabber
    for (int seqGrabberIndex = 0;
         seqGrabberIndex < numSequenceGrabbers;
         seqGrabberIndex++)
    {
        captureDevice = FindNextComponent(0, &compCaptureType);
        captureGrabber = OpenComponent(captureDevice);
        if (captureGrabber != NULL)
        {
            // We've found a sequencegrabber
            if (SGInitialize(captureGrabber) != noErr)
            {
                CloseComponent(captureGrabber);
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture,
                             _id, "%s:%d Could not init the sequence grabber",
                             __FUNCTION__, __LINE__);
                return -1;
            }
            break;
        }
        if (seqGrabberIndex == numSequenceGrabbers - 1)
        {
            // Couldn't open a sequence grabber
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                         "%s:%d Could not open a sequence grabber",
                         __FUNCTION__, __LINE__);
            return -1;
        }
    }

    // Create a temporary channel to get the names of the capture devices.
    // Takes time, make this in a nother way...
    if (SGNewChannel(captureGrabber, VideoMediaType, &captureChannel) != noErr)
    {
        // Could not create a video channel...
        SGRelease(captureGrabber);
        CloseComponent(captureGrabber);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                     "%s:%d Could not create a sequence grabber video channel",
                     __FUNCTION__, __LINE__);
        return -1;
    }
    closeChannel = true;

    // Find the type of capture devices, e.g. USB-devices, Firewire, DV, ...
    SGDeviceList deviceList = NULL;
    if (SGGetChannelDeviceList(captureChannel, sgDeviceListIncludeInputs,
                               &deviceList) != noErr)
    {
        if (closeChannel)
            SGDisposeChannel(captureGrabber, captureChannel);
        if (captureGrabber)
        {
            SGRelease(captureGrabber);
            CloseComponent(captureGrabber);
        }
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                     "%s:%d Could not create a device list", __FUNCTION__,
                     __LINE__);
        return -1;
    }

    // Loop through all device types and all devices for each type
    // and store in a list.
    int numDevices = (*deviceList)->count;
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCapture, _id,
                 "%s:%d Found %d devices", __FUNCTION__, __LINE__, numDevices);

    for (int i = 0; i < numDevices; i++)
    {

        SGDeviceName sgDeviceName = (*deviceList)->entry[i];
        // Get the list with input devices for this type of device
        if (sgDeviceName.inputs)
        {
            SGDeviceInputList inputList = sgDeviceName.inputs;
            int numInputDev = (*inputList)->count;

            for (int inputDevIndex = 0;
                 inputDevIndex < numInputDev;
                 inputDevIndex++)
            {
                // Get the name for this capture device
                SGDeviceInputName deviceInputName =
                    (*inputList)->entry[inputDevIndex];

                VideoCaptureMacName* deviceName = new VideoCaptureMacName();

                deviceName->_size = PascalStringToCString(
                    deviceInputName.name, deviceName->_name,
                    sizeof(deviceName->_name));

                if (deviceName->_size > 0)
                {
                    WEBRTC_TRACE(webrtc::kTraceDebug,webrtc::kTraceVideoCapture,
                                 _id,
                                 "%s:%d Capture device %d: %s was successfully "
                                 "set", __FUNCTION__, __LINE__, numberOfDevices,
                                 deviceName->_name);

                    if (numberOfDevices == deviceNumber)
                    {
                        strcpy((char*) deviceNameUTF8, deviceName->_name);
                        strcpy((char*) deviceUniqueIdUTF8, deviceName->_name);
                    }
                    numberOfDevices++;
                }
                else
                {
                    delete deviceName;

                    if (deviceName->_size < 0)
                    {
                        WEBRTC_TRACE(webrtc::kTraceError,
                                     webrtc::kTraceVideoCapture, _id,
                                     "%s:%d Error in PascalStringToCString",
                                     __FUNCTION__, __LINE__);
                        return -1;
                    }
                }
            }
        }
    }

    // clean up
    SGDisposeDeviceList(captureGrabber, deviceList);
    if (closeChannel)
    {
        SGDisposeChannel(captureGrabber, captureChannel);
    }
    if (captureGrabber)
    {
        SGRelease(captureGrabber);
        CloseComponent(captureGrabber);
    }

    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCapture, _id,
                 "%s:%d End function successfully", __FUNCTION__, __LINE__);
    return 0;
}

/**
 Convert a Pascal string to a C string.
 
 \param[in]  pascalString
 Pascal string to convert. Pascal strings contain the number of 
 characters in the first byte and are not null-terminated.
 
 \param[out] cString
 The C string buffer into which to copy the converted string.
 
 \param[in]  bufferSize
 The size of the C string buffer in bytes.
 
 \return The number of characters in the string on success and -1 on failure.
 */
CFIndex VideoCaptureMacQuickTimeInfo::PascalStringToCString(
    const unsigned char* pascalString, char* cString, CFIndex bufferSize)
{
    if (pascalString == NULL)
    {
        return -1;
    }

    if (cString == NULL)
    {
        return -1;
    }

    if (bufferSize == 0)
    {
        return -1;
    }

    CFIndex cStringLength = 0;
    CFIndex maxStringLength = bufferSize - 1;

    CFStringRef cfString = CFStringCreateWithPascalString(
        NULL, pascalString, kCFStringEncodingMacRoman);
    if (cfString == NULL)
    {
        CFRelease(cfString);
        return -1;
    }

    CFIndex cfLength = CFStringGetLength(cfString);
    cStringLength = cfLength;
    if (cfLength > maxStringLength)
    {
        cStringLength = maxStringLength;
    }

    Boolean success = CFStringGetCString(cfString, cString, bufferSize,
                                         kCFStringEncodingMacRoman);

    // Ensure the problem isn't insufficient buffer length.
    // This is fine; we will return a partial string.
    if (success == false && cfLength <= maxStringLength)
    {
        CFRelease(cfString);
        return -1;
    }

    CFRelease(cfString);
    return cStringLength;
}

//
//
//  Functions for handling capture devices
//
//

VideoCaptureMacQuickTimeInfo::VideoCaptureMacName::VideoCaptureMacName() :
    _size(0)
{
    memset(_name, 0, kVideoCaptureMacNameMaxSize);
}
}  // namespace webrtc
