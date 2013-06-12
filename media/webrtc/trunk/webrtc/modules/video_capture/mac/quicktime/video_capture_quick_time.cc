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
 *  video_capture_quick_time.cc
 *
 */


#include "video_capture_quick_time.h"

#include "CriticalSectionWrapper.h"
#include "event_wrapper.h"
#include "thread_wrapper.h"
#include "tick_util.h"
#include "trace.h"
#include <unistd.h>

namespace webrtc
{

VideoCaptureMacQuickTime::VideoCaptureMacQuickTime(int32_t iID) :
    VideoCaptureImpl(iID), // super class constructor
    _id(iID),
    _isCapturing(false),
    _captureCapability(),
    _grabberCritsect(CriticalSectionWrapper::CreateCriticalSection()),
    _videoMacCritsect(CriticalSectionWrapper::CreateCriticalSection()),
    _terminated(true), _grabberUpdateThread(NULL),
    _grabberUpdateEvent(NULL), _captureGrabber(NULL), _captureDevice(NULL),
    _captureVideoType(kVideoUnknown), _captureIsInitialized(false),
    _gWorld(NULL), _captureChannel(0), _captureSequence(NULL),
    _sgPrepared(false), _sgStarted(false), _trueCaptureWidth(0),
    _trueCaptureHeight(0), _captureDeviceList(),
    _captureDeviceListTime(0), _captureCapabilityList()

{
    _captureCapability.width = START_CODEC_WIDTH;
    _captureCapability.height = START_CODEC_HEIGHT;
    memset(_captureDeviceDisplayName, 0, sizeof(_captureDeviceDisplayName));
}

VideoCaptureMacQuickTime::~VideoCaptureMacQuickTime()
{


    VideoCaptureTerminate();

    if (_videoMacCritsect)
    {
        delete _videoMacCritsect;
    }
    if (_grabberCritsect)
    {
        delete _grabberCritsect;
    }

}

int32_t VideoCaptureMacQuickTime::Init(
    const int32_t id, const char* deviceUniqueIdUTF8)
{

    const int32_t nameLength =
        (int32_t) strlen((char*) deviceUniqueIdUTF8);
    if (nameLength > kVideoCaptureUniqueNameLength)
        return -1;

    // Store the device name
    _deviceUniqueId = new char[nameLength + 1];
    memset(_deviceUniqueId, 0, nameLength + 1);
    memcpy(_deviceUniqueId, deviceUniqueIdUTF8, nameLength + 1);

    // Check OSX version
    OSErr err = noErr;
    long version;

    _videoMacCritsect->Enter();
    if (!_terminated)
    {
        _videoMacCritsect->Leave();
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                     "%s:%d Already Initialized", __FUNCTION__, __LINE__);
        return -1;
    }

    err = Gestalt(gestaltSystemVersion, &version);
    if (err != noErr)
    {
        _videoMacCritsect->Leave();
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                     "%s:%d Could not retrieve OS version", __FUNCTION__,
                     __LINE__);
        return -1;
    }

    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, _id,
                 "%s:%d OS X version: %x,", __FUNCTION__, __LINE__, version);
    if (version < 0x00001040) // Older version than Mac OSX 10.4
    {
        _videoMacCritsect->Leave();
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                     "%s:%d OS version not supported", __FUNCTION__, __LINE__);
        return -1;
    }

    err = Gestalt(gestaltQuickTime, &version);
    if (err != noErr)
    {
        _videoMacCritsect->Leave();
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                     "%s:%d Could not retrieve QuickTime version",
                     __FUNCTION__, __LINE__);
        return -1;
    }

    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, _id,
                 "%s:%d QuickTime version: %x", __FUNCTION__, __LINE__,
                 version);
    if (version < 0x07000000) // QT v. 7.x or newer (QT 5.0.2 0x05020000)
    {
        _videoMacCritsect->Leave();
        return -1;
    }

    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCapture, _id,
                 "%s:%d EnterMovies()", __FUNCTION__, __LINE__);
    EnterMovies();

    if (VideoCaptureSetCaptureDevice((char*) deviceUniqueIdUTF8,
                                   kVideoCaptureProductIdLength) == -1)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                     "%s:%d failed to set capture device: %s", __FUNCTION__,
                     __LINE__, deviceUniqueIdUTF8);
        _videoMacCritsect->Leave();
        return -1;
    }

    _terminated = false;

    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, _id,
                 "%s:%d successful initialization", __FUNCTION__, __LINE__);
    _videoMacCritsect->Leave();

    return 0;
}

int32_t VideoCaptureMacQuickTime::StartCapture(
    const VideoCaptureCapability& capability)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, _id, "%s:%d "
        "capability.width=%d, capability.height=%d ,capability.maxFPS=%d "
        "capability.expectedCaptureDelay=%d, capability.interlaced=%d",
        __FUNCTION__, __LINE__, capability.width, capability.height,
        capability.maxFPS, capability.expectedCaptureDelay,
        capability.interlaced);

    _captureCapability.width = capability.width;
    _captureCapability.height = capability.height;
    _captureDelay = 120;

    if (VideoCaptureRun() == -1)
    {
        return -1;
    }

    return 0;
}

int32_t VideoCaptureMacQuickTime::StopCapture()
{

    if (VideoCaptureStop() == -1)
    {
        return -1;
    }

    return 0;
}

bool VideoCaptureMacQuickTime::CaptureStarted()
{
    return _isCapturing;
}

int32_t VideoCaptureMacQuickTime::CaptureSettings(
    VideoCaptureCapability& settings)
{
	settings.width = _captureCapability.width;
	settings.height = _captureCapability.height;
	settings.maxFPS = 0;
    return 0;
}

int VideoCaptureMacQuickTime::VideoCaptureTerminate()
{
    VideoCaptureStop();

    _videoMacCritsect->Enter();
    if (_terminated)
    {
        _videoMacCritsect->Leave();
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                     "%s:%d Already terminated", __FUNCTION__, __LINE__);
        return -1;
    }

    _grabberCritsect->Enter();

    // Stop the camera/sequence grabber
    // Resets: _captureSequence, _sgStarted
    StopQuickTimeCapture();

    // Remove local video settings
    // Resets: _gWorld, _captureCapability.width, _captureCapability.height
    RemoveLocalGWorld();
    DisconnectCaptureDevice();

    if (_grabberUpdateThread)
        _grabberUpdateThread->SetNotAlive();

    _grabberCritsect->Leave();

    if (_grabberUpdateEvent)
        _grabberUpdateEvent->Set();

    SLEEP(1);
    _grabberCritsect->Enter();

    if (_grabberUpdateThread)
    {
        _grabberUpdateThread->Stop();
        delete _grabberUpdateThread;
        _grabberUpdateThread = NULL;
    }
    if (_grabberUpdateEvent)
    {
        delete _grabberUpdateEvent;
        _grabberUpdateEvent = NULL;
    }

    // Close the sequence grabber
    if (_captureGrabber)
    {
        SGRelease(_captureGrabber);
        _captureGrabber = NULL;
        CloseComponent(_captureGrabber);
        _captureDevice = NULL;
    }
    _captureVideoType = kVideoUnknown;

    // Delete capture device list
    ListItem* item = _captureDeviceList.First();
    while (item)
    {
        delete static_cast<unsigned char*> (item->GetItem());
        _captureDeviceList.Erase(item);
        item = _captureDeviceList.First();
    }
    _captureDeviceListTime = 0;

    _terminated = true;

    _grabberCritsect->Leave();
    _videoMacCritsect->Leave();

    return 0;
}

int VideoCaptureMacQuickTime::UpdateCaptureSettings(int channel,
                                                    webrtc::VideoCodec& inst,
                                                    bool def)
{

    if (channel < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                     "%s:%d Invalid channel number: %d", __FUNCTION__,
                     __LINE__, channel);
        return -1;
    }

    // the size has changed, we need to change our setup
    _videoMacCritsect->Enter();

    // Stop capturing, if we are...
    _grabberCritsect->Enter();

    bool wasCapturing = false;
    StopQuickTimeCapture(&wasCapturing);

    // Create a new offline GWorld to receive captured frames
    RemoveLocalGWorld();

    if (CreateLocalGWorld(inst.width, inst.height) == -1)
    {
        _grabberCritsect->Leave();
        _videoMacCritsect->Leave();
        // Error already logged
        return -1;
    }
    _captureCapability.width = inst.width;
    _captureCapability.height = inst.height;

    // Connect the capture device to our offline GWorld
    // if we already have a capture device selected.
    if (_captureDevice)
    {
        DisconnectCaptureDevice();
        if (ConnectCaptureDevice() == -1)
        {
            // Error already logged
            _grabberCritsect->Leave();
            _videoMacCritsect->Leave();
            return -1;
        }
    }

    // Start capture if we did before
    if (wasCapturing)
    {
        if (StartQuickTimeCapture() == -1)
        {
            _grabberCritsect->Leave();
            _videoMacCritsect->Leave();
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                         "%s:%d Failed to start capturing", __FUNCTION__,
                         __LINE__);
            return -1;
        }
    }
    _grabberCritsect->Leave();
    _videoMacCritsect->Leave();

    return 0;
}

// Creates an off screen graphics world used for converting
// captured video frames if we can't get a format we want.
// Assumed protected by critsects
int VideoCaptureMacQuickTime::CreateLocalGWorld(int width, int height)
{
    if (_gWorld)
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideoCapture, _id,
                     "%s:%d GWorld already created", __FUNCTION__, __LINE__);
        return -1;
    }
    if (width == 0 || height == 0)
    {
        return -1;
    }

    Rect captureRect;
    captureRect.left = 0;
    captureRect.top = 0;
    captureRect.right = width;
    captureRect.bottom = height;

    // Create a GWorld in same size as we want to send to the codec
    if (QTNewGWorld(&(_gWorld), k2vuyPixelFormat, &captureRect, 0, NULL, 0)
        != noErr)
    {
        return -1;
    }
    _captureCapability.width = width;
    _captureCapability.height = height;

    if (!LockPixels(GetGWorldPixMap(_gWorld)))
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideoCapture, _id,
                     "%s:%d Could not lock pixmap. Continuing anyhow",
                     __FUNCTION__, __LINE__);
    }

    CGrafPtr theOldPort;
    GDHandle theOldDevice;
    GetGWorld(&theOldPort, &theOldDevice); // Gets the result from QTGetNewGWorld
    SetGWorld(_gWorld, NULL); // Sets the new GWorld
    BackColor( blackColor); // Changes the color on the graphic port
    ForeColor( whiteColor);
    EraseRect(&captureRect);
    SetGWorld(theOldPort, theOldDevice);

    return 0;
}

// Assumed critsect protected
int VideoCaptureMacQuickTime::RemoveLocalGWorld()
{
    if (!_gWorld)
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideoCapture, _id,
                     "%s:%d !gWorld", __FUNCTION__, __LINE__);
        return -1;
    }

    DisposeGWorld(_gWorld);
    _gWorld = NULL;
    _captureCapability.width = START_CODEC_WIDTH;
    _captureCapability.height = START_CODEC_HEIGHT;

    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, _id,
                 "%s:%d GWorld has been removed", __FUNCTION__, __LINE__);
    return 0;
}

// ConnectCaptureDevice
// This function prepares the capture device
// with the wanted settings, but the capture
// device isn't started.
//
// Assumed critsect protected
int VideoCaptureMacQuickTime::ConnectCaptureDevice()
{
    // Prepare the capture grabber if a capture device is already set
    if (!_captureGrabber)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                     "%s:%d No capture device is selected", __FUNCTION__,
                     __LINE__);
        return -1;
    }
    if (_captureIsInitialized)
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideoCapture, _id,
                     "%s:%d Capture device is already initialized",
                     __FUNCTION__, __LINE__);
        return -1;
    }
    if (!_gWorld)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                     "%s:%d No GWorld is created", __FUNCTION__, __LINE__);
        return -1;
    }

    OSErr err = noErr;
    long flags = 0;

    // Connect the camera to our offline GWorld
    // We won't use the GWorld if we get the format we want
    // from the camera.
    if (SGSetGWorld(_captureGrabber, _gWorld, NULL ) != noErr)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                     "%s:%d Could not connect capture device", __FUNCTION__,
                     __LINE__);
        return -1;
    }
    if (SGSetDataRef(_captureGrabber, 0, 0, seqGrabDontMakeMovie) != noErr)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                     "%s:%d Could not configure capture device", __FUNCTION__,
                     __LINE__);
        return -1;
    }

    // Set our capture callback
    if (SGSetDataProc(_captureGrabber, NewSGDataUPP(SendProcess), (long) this)
        != noErr)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                     "%s:%d Could not set capture callback. Unable to receive "
                     "frames", __FUNCTION__, __LINE__);
        return -1;
    }

    // Create a video channel to the sequence grabber
    if (SGNewChannel(_captureGrabber, VideoMediaType, &_captureChannel)
        != noErr) // Takes time!!!
    {
        return -1;
    }

    // Get a list with all capture devices to choose the one we want.
    SGDeviceList deviceList = NULL;
    if (SGGetChannelDeviceList(_captureChannel, sgDeviceListIncludeInputs,
                               &deviceList) != noErr)
    {

    }

    int numDevicesTypes = (*deviceList)->count;
    bool captureDeviceFound = false;
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCapture, _id,
                 "%s:%d Found %d channel devices", __FUNCTION__, __LINE__,
                 numDevicesTypes);

    // Loop through all devices to get the one we want.
    for (int i = 0; i < numDevicesTypes; i++)
    {
        SGDeviceName deviceTypeName = (*deviceList)->entry[i];
        WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCapture, _id,
                     "%s:%d Inspecting device number: %d", __FUNCTION__,
                     __LINE__, i);
        // Get the list with input devices
        if (deviceTypeName.inputs)
        {
            SGDeviceInputList inputList = deviceTypeName.inputs;
            int numInputDev = (*inputList)->count;
            WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCapture, _id,
                         "%s:%d Device has %d inputs", __FUNCTION__, __LINE__,
                         numInputDev);
            for (int inputDevIndex = 0;
                 inputDevIndex < numInputDev;
                 inputDevIndex++)
            {
                WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCapture,
                             _id, "%s:%d Inspecting input number: %d",
                             __FUNCTION__, __LINE__, inputDevIndex);
                SGDeviceInputName deviceInputName =
                    (*inputList)->entry[inputDevIndex];
                char devInName[64];
                memset(devInName, 0, 64);

                // SGDeviceInputName::name is a Str63, defined as a Pascal string.
                // (Refer to MacTypes.h)
                CFIndex devInNameLength =
                    PascalStringToCString(deviceInputName.name, devInName,
                                          sizeof(devInName));
                WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCapture,
                             _id,
                             "%s:%d Converted pascal string with length:%d  "
                             "to: %s", __FUNCTION__, __LINE__,
                             sizeof(devInName), devInName);
                if (devInNameLength < 0)
                {
                    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture,
                                 _id,
                                 "%s:%d Failed to convert device name from "
                                 "pascal string to c string", __FUNCTION__,
                                 __LINE__);
                    return -1;
                }

                if (!strcmp(devInName, _captureDeviceDisplayName))
                {
                    WEBRTC_TRACE(webrtc::kTraceDebug,
                                 webrtc::kTraceVideoCapture, _id,
                                 "%s:%d We have found our device: %s",
                                 __FUNCTION__, __LINE__,
                                 _captureDeviceDisplayName);

                    if (SGSetChannelDevice(_captureChannel, deviceTypeName.name)
                        != noErr)
                    {
                        WEBRTC_TRACE(webrtc::kTraceError,
                                     webrtc::kTraceVideoCapture, _id,
                                     "%s:%d Could not set capture device type: "
                                     "%s",__FUNCTION__, __LINE__,
                                     deviceTypeName.name);
                        return -1;
                    }

                    WEBRTC_TRACE(webrtc::kTraceInfo,
                                 webrtc::kTraceVideoCapture, _id,
                                 "%s:%d Capture device type is: %s",
                                 __FUNCTION__, __LINE__, deviceTypeName.name);
                    if (SGSetChannelDeviceInput(_captureChannel, inputDevIndex)
                        != noErr)
                    {
                        WEBRTC_TRACE(webrtc::kTraceError,
                                     webrtc::kTraceVideoCapture, _id,
                                     "%s:%d Could not set SG device",
                                     __FUNCTION__, __LINE__);
                        return -1;
                    }

                    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture,
                                 _id,
                                 "%s:%d Capture device: %s has successfully "
                                 "been set", __FUNCTION__, __LINE__,
                                 _captureDeviceDisplayName);
                    captureDeviceFound = true;
                    break;
                }
            }
            if (captureDeviceFound)
            {
                WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCapture,
                             _id,
                             "%s:%d Capture device found, breaking from loops",
                             __FUNCTION__, __LINE__);
                break;
            }
        }
    }
    err = SGDisposeDeviceList(_captureGrabber, deviceList);

    if (!captureDeviceFound)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                     "%s:%d Failed to find capture device: %s. Returning -1",
                     __FUNCTION__, __LINE__, _captureDeviceDisplayName);
        return -1;
    }

    // Set the size we want from the capture device
    Rect captureSize;
    captureSize.left = 0;
    captureSize.top = 0;
    captureSize.right = _captureCapability.width;
    captureSize.bottom = _captureCapability.height;

    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, _id,
                 "%s:%d Using capture rect: l:%d t:%d r:%d b:%d", __FUNCTION__,
                 __LINE__, captureSize.left, captureSize.top,
                 captureSize.right, captureSize.bottom);

    err = SGSetChannelBounds(_captureChannel, &captureSize);
    if (err == noErr)
    {
        err = SGSetChannelUsage(_captureChannel, flags | seqGrabRecord);
    }
    if (err != noErr)
    {
        SGDisposeChannel(_captureGrabber, _captureChannel);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                     "%s:%d Error setting SG channel to device", __FUNCTION__,
                     __LINE__);
        return -1;
    }

    // Find out what video format we'll get from the capture device.
    OSType compType;
    err = SGGetVideoCompressorType(_captureChannel, &compType);

    // Convert the Apple video format name to a VideoCapture name.
    if (compType == k2vuyPixelFormat)
    {
        _captureVideoType = kVideoUYVY;
        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, _id,
                     "%s:%d Device delivers UYUV formatted frames",
                     __FUNCTION__, __LINE__);
    }
    else if (compType == kYUVSPixelFormat)
    {
        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, _id,
                     "%s:%d Device delivers YUY2 formatted frames",
                     __FUNCTION__, __LINE__);
        _captureVideoType = kVideoYUY2;
    }
    else
    {
        WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCapture, _id,
                     "%s:%d Device delivers frames in an unknown format: 0x%x. "
                     "Consult QuickdrawTypes.h",
                     __FUNCTION__, __LINE__, compType);
        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, _id,
                     "%s:%d Device delivers frames in an unknown format.",
                     __FUNCTION__, __LINE__);
        _captureVideoType = kVideoUnknown;
    }

    if (SGPrepare(_captureGrabber, false, true) != noErr)
    {
        _grabberCritsect->Leave();
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                     "%s:%d Error starting sequence grabber", __FUNCTION__,
                     __LINE__);
        return -1;
    }

    // Try to set the codec size as capture size.
    err = SGSetChannelBounds(_captureChannel, &captureSize);

    // Check if we really will get the size we asked for.
    ImageDescriptionHandle imageDesc = (ImageDescriptionHandle) NewHandle(0);
    err = SGGetChannelSampleDescription(_captureChannel, (Handle) imageDesc);

    _trueCaptureWidth = (**imageDesc).width;
    _trueCaptureHeight = (**imageDesc).height;

    DisposeHandle((Handle) imageDesc);

    _captureIsInitialized = true;
    _sgPrepared = true;

    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, _id,
                 "%s:%d Success starting sequence grabber", __FUNCTION__,
                 __LINE__);

    return 0;
}

// Assumed critsect protected
int VideoCaptureMacQuickTime::DisconnectCaptureDevice()
{
    if (_sgStarted)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                     "%s:%d Capture device is still running. Returning -1",
                     __FUNCTION__, __LINE__);
        return -1;
    }
    if (!_sgPrepared)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                     "%s:%d No capture device connected", __FUNCTION__,
                     __LINE__);
        return -1;
    }

    // Close the capture channel
    SGStop(_captureGrabber);
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCapture, _id,
                 "%s:%d !!!! releasing sg stuff", __FUNCTION__, __LINE__);
    SGDisposeChannel(_captureGrabber, _captureChannel);
    SGRelease(_captureGrabber);
    CloseComponent(_captureGrabber);

    // Reset all values
    _captureChannel = NULL;
    _captureVideoType = kVideoUnknown;
    _trueCaptureWidth = 0;
    _trueCaptureHeight = 0;
    _captureIsInitialized = false;
    _sgPrepared = false;

    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, _id,
                 "%s:%d Sequence grabber removed", __FUNCTION__, __LINE__);

    return 0;
}

// StartQuickTimeCapture
//
// Actually starts the camera
// 
int VideoCaptureMacQuickTime::StartQuickTimeCapture()
{
    _grabberCritsect->Enter();
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, _id,
                 "%s:%d Attempting to start sequence grabber", __FUNCTION__,
                 __LINE__);

    if (_sgStarted)
    {
        _grabberCritsect->Leave();
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                     "%s:%d Sequence grabber already started", __FUNCTION__,
                     __LINE__);
        return 0;
    }
    if (!_sgPrepared)
    {
        _grabberCritsect->Leave();
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                     "%s:%d Sequence grabber not prepared properly",
                     __FUNCTION__, __LINE__);
        return 0;
    }

    if (SGStartRecord(_captureGrabber) != noErr)
    {
        _grabberCritsect->Leave();
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                     "%s:%d Error starting sequence grabber", __FUNCTION__,
                     __LINE__);
        return -1;
    }

    Rect captureRect = { 0, 0, 0, 0 };
    MatrixRecord scaleMatrix;
    ImageDescriptionHandle imageDesc = (ImageDescriptionHandle) NewHandle(0);

    // Get the sample description for the channel, which is the same as for the
    // capture device
    if (SGGetChannelSampleDescription(_captureChannel, (Handle) imageDesc)
        != noErr)
    {
        _grabberCritsect->Leave();
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                     "%s:%d Error accessing device properties", __FUNCTION__,
                     __LINE__);
        return -1;
    }

    // Create a scale matrix to scale the captured image
    // Needed if we don't get the size wanted from the camera
    captureRect.right = (**imageDesc).width;
    captureRect.bottom = (**imageDesc).height;

    Rect codecRect;
    codecRect.left = 0;
    codecRect.top = 0;
    codecRect.right = _captureCapability.width;
    codecRect.bottom = _captureCapability.height;
    RectMatrix(&scaleMatrix, &captureRect, &codecRect);

    // Start grabbing images from the capture device to _gWorld
    if (DecompressSequenceBegin(&_captureSequence, imageDesc, _gWorld, NULL,
                                NULL, &scaleMatrix, srcCopy, (RgnHandle) NULL,
                                NULL, codecNormalQuality, bestSpeedCodec)
        != noErr)
    {
        _grabberCritsect->Leave();
        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, _id,
                     "%s:%d Error starting decompress sequence", __FUNCTION__,
                     __LINE__);
        return -1;
    }
    DisposeHandle((Handle) imageDesc);
    _sgStarted = true;
    _grabberCritsect->Leave();
    return 0;
}

int VideoCaptureMacQuickTime::StopQuickTimeCapture(bool* wasCapturing)
{
    _grabberCritsect->Enter();
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, _id,
                 "%s:%d wasCapturing=%d", __FUNCTION__, __LINE__, wasCapturing);

    if (!_sgStarted)
    {
        if (wasCapturing)
            *wasCapturing = false;

        _grabberCritsect->Leave();
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideoCapture, _id,
                     "%s:%d Sequence grabber was never started", __FUNCTION__,
                     __LINE__);
        return 0;
    }

    if (wasCapturing)
        *wasCapturing = true;

    OSErr error = noErr;
    error = SGStop(_captureGrabber);
    CDSequenceEnd(_captureSequence);
    _captureSequence = NULL;
    _sgStarted = false;

    _grabberCritsect->Leave();
    if (error != noErr)
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideoCapture, _id,
                     "%s:%d Could not stop sequence grabber", __FUNCTION__,
                     __LINE__);
        return -1;
    }

    return 0;
}

//-------------------------------------------------
//
//  Thread/function to keep capture device working
//
//-------------------------------------------------


//
// GrabberUpdateThread / GrabberUpdateProcess
//
// Called at a certain time interval to tell
// the capture device / SequenceGrabber to
// actually work.
bool VideoCaptureMacQuickTime::GrabberUpdateThread(void* obj)
{
    return static_cast<VideoCaptureMacQuickTime*> (obj)->GrabberUpdateProcess();
}

bool VideoCaptureMacQuickTime::GrabberUpdateProcess()
{
    _grabberUpdateEvent->Wait(30);

    if (_isCapturing == false)
        return false;

    _grabberCritsect->Enter();
    if (_captureGrabber)
    {
        if (SGIdle(_captureGrabber) != noErr)
        {
        }
    }
    _grabberCritsect->Leave();
    return true;
}

//
// VideoCaptureStop
//
// Stops the capture device
//
int VideoCaptureMacQuickTime::VideoCaptureStop()
{
    if (_grabberUpdateThread)
    {
        _grabberUpdateThread->Stop();
    }

    _videoMacCritsect->Enter();
    _grabberCritsect->Enter();
    int retVal = StopQuickTimeCapture();
    _grabberCritsect->Leave();
    _videoMacCritsect->Leave();
    if (retVal == -1)
    {
        return -1;
    }

    _isCapturing = false;
    return 0;
}

//
// VideoCaptureRun
//
// Starts the capture device and creates
// the update thread.
//
int VideoCaptureMacQuickTime::VideoCaptureRun()
{
    _videoMacCritsect->Enter();
    _grabberCritsect->Enter();

    int res = StartQuickTimeCapture();

    // Create the thread for updating sequence grabber if not created earlier
    if (!_grabberUpdateThread)
    {
        _grabberUpdateEvent = EventWrapper::Create();
        _grabberUpdateThread = ThreadWrapper::CreateThread(
            VideoCaptureMacQuickTime::GrabberUpdateThread, this, kHighPriority);
        unsigned int id;
        _grabberUpdateThread->Start(id);
    }
    else
    {
        unsigned int id;
        _grabberUpdateThread->Start(id);
    }

    _grabberCritsect->Leave();
    _videoMacCritsect->Leave();

    _isCapturing = true;
    return res;
}

// ---------------------------------------------------------------------- 
//
// SendProcess
// sequence grabber data procedure
//
// This function is called by the capture device as soon as a new
// frame is available.
//
//
// SendFrame
//
// The non-static function used by the capture device callback
//
// Input:
//        sgChannel: the capture device channel generating the callback
//        data:      the video frame
//        length:    the data length in bytes
//        grabTime:  time stamp generated by the capture device / sequece grabber
//
// ----------------------------------------------------------------------

OSErr VideoCaptureMacQuickTime::SendProcess(SGChannel sgChannel, Ptr p,
                                            long len, long* /*offset*/,
                                            long /*chRefCon*/, TimeValue time,
                                            short /*writeType*/, long refCon)
{
    VideoCaptureMacQuickTime* videoEngine =
        reinterpret_cast<VideoCaptureMacQuickTime*> (refCon);
    return videoEngine->SendFrame(sgChannel, (char*) p, len, time);
}

int VideoCaptureMacQuickTime::SendFrame(SGChannel /*sgChannel*/, char* data,
                                        long length, TimeValue /*grabTime*/)
{
    if (!_sgPrepared)
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideoCapture, _id,
                     "%s:%d Sequence Grabber is not initialized", __FUNCTION__,
                     __LINE__);
        return 0;
    }

    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCapture, _id,
                 "%s:%d Frame has been delivered\n", __FUNCTION__, __LINE__);

    CodecFlags ignore;
    _grabberCritsect->Enter();
    if (_gWorld)
    {
        // Will be set to true if we don't recognize the size and/or video
        // format.
        bool convertFrame = false;
        int32_t width = 352;
        int32_t height = 288;
        int32_t frameSize = 0;

        VideoCaptureCapability captureCapability;
        captureCapability.width = width;
        captureCapability.height = height;
        captureCapability.maxFPS = 30;

        switch (_captureVideoType)
        {
            case kVideoUYVY:
                captureCapability.rawType = kVideoUYVY;
                break;
            case kVideoYUY2:
                captureCapability.rawType = kVideoYUY2;
                break;
            case kVideoI420:
                captureCapability.rawType = kVideoI420;
                break;
            default:
                captureCapability.rawType = kVideoI420;
                WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCapture,
                             _id, "%s:%d raw = I420 by default\n",
                             __FUNCTION__, __LINE__);
                break;
        }

        // Convert the camera video type to something VideoEngine can work with
        // Check if we need to downsample the incomming frame.
        switch (_captureVideoType)
        {
            case kVideoUYVY:
            case kVideoYUY2:
                frameSize = (width * height * 16) >> 3; // 16 is for YUY2 format
                if (width == _captureCapability.width || height
                    == _captureCapability.height)
                {
                    // Ok format and size, send the frame to super class
                    IncomingFrame((uint8_t*) data,
                                  (int32_t) frameSize, captureCapability,
                                  TickTime::MillisecondTimestamp());

                }
                else if (width == _trueCaptureWidth && height
                    == _trueCaptureHeight)
                {
                    // We need to scale the picture to correct size...
                    // This happens for cameras not supporting all sizes.
                    // E.g. older built-in iSight doesn't support QCIF.

                    // Convert the incoming frame into our GWorld.
                    int res =
                        DecompressSequenceFrameS(_captureSequence, data,
                                                 length, 0, &ignore, NULL);
                    if (res != noErr && res != -8976) // 8796 ==  black frame
                    {
                        WEBRTC_TRACE(webrtc::kTraceWarning,
                                     webrtc::kTraceVideoCapture, _id,
                                     "%s:%d Captured black frame. Not "
                                     "processing it", __FUNCTION__, __LINE__);
                        _grabberCritsect->Leave();
                        return 0;
                    }

                    // Copy the frame from the PixMap to our video buffer
                    PixMapHandle pixMap = GetGWorldPixMap(_gWorld);

                    // Lock the image data in the GWorld.
                    LockPixels(pixMap);

                    // Get a pointer to the pixel data.
                    Ptr capturedFrame = GetPixBaseAddr(pixMap);

                    // Send the converted frame out to super class
                    IncomingFrame((uint8_t*) data,
                                  (int32_t) frameSize, captureCapability,
                                  TickTime::MillisecondTimestamp());

                    // Unlock the image data to get ready for the next frame.
                    UnlockPixels(pixMap);
                }
                else
                {
                    // Not a size we recognize, use the Mac internal scaling...
                    convertFrame = true;
                    WEBRTC_TRACE(webrtc::kTraceDebug,
                                 webrtc::kTraceVideoCapture, _id,
                                 "%s:%d Not correct incoming stream size for "
                                 "the format and configured size",
                                 __FUNCTION__, __LINE__);
                }
                break;
            default:

                // Not a video format we recognize, use the Mac internal scaling
                WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideoCapture,
                             _id, "%s:%d Unknown video frame format (default)",
                             __FUNCTION__, __LINE__);
                convertFrame = true;
                break;
        }

        if (convertFrame)
        {
            WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCapture, _id,
                         "%s:%d Unrecognized frame format. Converting frame",
                         __FUNCTION__, __LINE__);

            // We don't recognise the input format. Convert to UYVY, I420 is not
            // supported on osx. Decompress the grabbed frame into the GWorld,
            // i.e from webcam format to ARGB (RGB24), and extract the frame.
            int res = DecompressSequenceFrameS(_captureSequence, data, length,
                                               0, &ignore, NULL);
            if (res != noErr && res != -8976) // 8796 means a black frame
            {
                _grabberCritsect->Leave();
                return 0;
            }

            // Copy the frame from the PixMap to our video buffer
            PixMapHandle rgbPixMap = GetGWorldPixMap(_gWorld);
            LockPixels(rgbPixMap);
            Ptr capturedFrame = GetPixBaseAddr(rgbPixMap);

            // Get the picture size
            int width = (*rgbPixMap)->bounds.right;
            int height = (*rgbPixMap)->bounds.bottom;

            // 16 is for YUY2 format.
            int32_t frameSize = (width * height * 16) >> 3;

            // Ok format and size, send the frame to super class
            IncomingFrame((uint8_t*) data, (int32_t) frameSize,
                          captureCapability, TickTime::MillisecondTimestamp());

            UnlockPixels(rgbPixMap);
        }

        // Tell the capture device it's ok to update.
        SGUpdate(_captureGrabber, NULL);
    }
    else
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideoCapture, _id,
                     "%s:%d No GWorld created, but frames are being delivered",
                     __FUNCTION__, __LINE__);
    }

    _grabberCritsect->Leave();
    return 0;
}

int VideoCaptureMacQuickTime::VideoCaptureInitThreadContext()
{
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCapture, _id,
                 "%s:%d ", __FUNCTION__, __LINE__);
    _videoMacCritsect->Enter();
    EnterMoviesOnThread( kQTEnterMoviesFlagDontSetComponentsThreadMode);
    _videoMacCritsect->Leave();
    return 0;
}

//
//
//  Functions for handling capture devices
//
//

VideoCaptureMacQuickTime::VideoCaptureMacName::VideoCaptureMacName() :
    _size(0)
{
    memset(_name, 0, kVideoCaptureMacNameMaxSize);
}

int VideoCaptureMacQuickTime::VideoCaptureSetCaptureDevice(
    const char* deviceName, int size)
{


    _videoMacCritsect->Enter();
    bool wasCapturing = false;

    _grabberCritsect->Enter();
    if (_captureGrabber)
    {
        // Stop grabbing, disconnect and close the old capture device
        StopQuickTimeCapture(&wasCapturing);
        DisconnectCaptureDevice();
        CloseComponent(_captureGrabber);
        _captureDevice = NULL;
        _captureGrabber = NULL;
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                     "%s:%d Old capture device removed", __FUNCTION__,
                     __LINE__);
    }

    if (deviceName == NULL || size == 0)
    {
        _grabberCritsect->Leave();
        _videoMacCritsect->Leave();
        return 0;
    }

    if (size < 0)
    {
        _grabberCritsect->Leave();
        _videoMacCritsect->Leave();
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                     "%s:%d 'size' is not valid", __FUNCTION__, __LINE__);
        return 0;
    }

    ComponentDescription compCaptureType;

    // Define the component we want to open
    compCaptureType.componentType = SeqGrabComponentType;
    compCaptureType.componentSubType = 0;
    compCaptureType.componentManufacturer = 0;
    compCaptureType.componentFlags = 0;
    compCaptureType.componentFlagsMask = 0;

    long numSequenceGrabbers = CountComponents(&compCaptureType);

    // loop through the available grabbers and open the first possible
    for (int i = 0; i < numSequenceGrabbers; i++)
    {
        _captureDevice = FindNextComponent(0, &compCaptureType);
        _captureGrabber = OpenComponent(_captureDevice);
        if (_captureGrabber != NULL)
        {
            // We've found a sequencegrabber that we could open
            if (SGInitialize(_captureGrabber) != noErr)
            {
                _grabberCritsect->Leave();
                _videoMacCritsect->Leave();
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture,
                             _id,
                             "%s:%d Could not initialize sequence grabber",
                             __FUNCTION__, __LINE__);
                return -1;
            }
            break;
        }
        if (i == numSequenceGrabbers - 1)
        {
            // Couldn't open a sequence grabber
            _grabberCritsect->Leave();
            _videoMacCritsect->Leave();
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                         "%s:%d Could not open a sequence grabber",
                         __FUNCTION__, __LINE__);
            return -1;
        }
    }

    if (!_gWorld)
    {
        // We don't have a GWorld. Create one to enable early preview
        // without calling SetSendCodec
        if (CreateLocalGWorld(_captureCapability.width,
                              _captureCapability.height) == -1)
        {
            // Error already logged
            _grabberCritsect->Leave();
            _videoMacCritsect->Leave();
            return -1;
        }
    }
    // Connect the camera with our GWorld
    int cpySize = size;
    if ((unsigned int) size > sizeof(_captureDeviceDisplayName))
    {
        cpySize = sizeof(_captureDeviceDisplayName);
    }
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCapture, _id,
                 "%s:%d Copying %d chars from deviceName to "
                 "_captureDeviceDisplayName (size=%d)\n",
                 __FUNCTION__, __LINE__, cpySize, size);
    memcpy(_captureDeviceDisplayName, deviceName, cpySize);
    if (ConnectCaptureDevice() == -1)
    {
        // Error already logged
        _grabberCritsect->Leave();
        _videoMacCritsect->Leave();
        return -1;
    }

    if (StartQuickTimeCapture() == -1)
    {
        // Error already logged
        _grabberCritsect->Leave();
        _videoMacCritsect->Leave();
        return -1;
    }
    _grabberCritsect->Leave();
    _videoMacCritsect->Leave();
    return 0;
}

bool VideoCaptureMacQuickTime::IsCaptureDeviceSelected()
{
    _grabberCritsect->Leave();
    return (_captureIsInitialized) ? true : false;
    _grabberCritsect->Leave();
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
CFIndex VideoCaptureMacQuickTime::PascalStringToCString(
    const unsigned char* pascalString, char* cString, CFIndex bufferSize)
{

    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCapture, 0,
                 "%s:%d Converting pascal string to c string", __FUNCTION__,
                 __LINE__);
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
        WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCapture, 0,
                     "%s:%d Error in CFStringCreateWithPascalString()",
                     __FUNCTION__, __LINE__);
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
        WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCapture, 0,
                     "%s:%d Error in CFStringGetCString()", __FUNCTION__,
                     __LINE__);
        CFRelease(cfString);
        return -1;
    }

    CFRelease(cfString);
    return cStringLength;
}
}  // namespace webrtc
