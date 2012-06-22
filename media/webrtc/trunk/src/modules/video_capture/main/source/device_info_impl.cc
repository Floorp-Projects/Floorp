/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "device_info_impl.h"
#include "video_capture_config.h"
#include "trace.h"
#include <stdlib.h>

#ifndef abs
#define abs(a) (a>=0?a:-a)
#endif

namespace webrtc
{
namespace videocapturemodule
{
DeviceInfoImpl::DeviceInfoImpl(const WebRtc_Word32 id)
    : _id(id), _apiLock(*RWLockWrapper::CreateRWLock()), _lastUsedDeviceName(NULL),
      _lastUsedDeviceNameLength(0)
{
}

DeviceInfoImpl::~DeviceInfoImpl(void)
{
    _apiLock.AcquireLockExclusive();
    // Reset old capability list
    MapItem* item = NULL;
    while ((item = _captureCapabilities.Last()))
    {
        delete (VideoCaptureCapability*) item->GetItem();
        _captureCapabilities.Erase(item);
    }
    free(_lastUsedDeviceName);
    _apiLock.ReleaseLockExclusive();

    delete &_apiLock;
}
WebRtc_Word32 DeviceInfoImpl::NumberOfCapabilities(
                                        const char* deviceUniqueIdUTF8)
{

    if (!deviceUniqueIdUTF8)
        return -1;

    _apiLock.AcquireLockShared();

    if (_lastUsedDeviceNameLength == strlen((char*) deviceUniqueIdUTF8))
    {
        // Is it the same device that is asked for again.
#if defined(WEBRTC_MAC_INTEL) || defined(WEBRTC_LINUX)
        if(strncasecmp((char*)_lastUsedDeviceName,
                       (char*) deviceUniqueIdUTF8,
                       _lastUsedDeviceNameLength)==0)
#else
        if (_strnicmp((char*) _lastUsedDeviceName,
                      (char*) deviceUniqueIdUTF8,
                      _lastUsedDeviceNameLength) == 0)
#endif
        {
            //yes
            _apiLock.ReleaseLockShared();
            return _captureCapabilities.Size();
        }
    }
    // Need to get exclusive rights to create the new capability map.
    _apiLock.ReleaseLockShared();
    WriteLockScoped cs2(_apiLock);

    WebRtc_Word32 ret = CreateCapabilityMap(deviceUniqueIdUTF8);
    return ret;
}

WebRtc_Word32 DeviceInfoImpl::GetCapability(const char* deviceUniqueIdUTF8,
                                            const WebRtc_UWord32 deviceCapabilityNumber,
                                            VideoCaptureCapability& capability)
{

    if (!deviceUniqueIdUTF8)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                   "deviceUniqueIdUTF8 parameter not set in call to GetCapability");
        return -1;
    }
    ReadLockScoped cs(_apiLock);

    if ((_lastUsedDeviceNameLength != strlen((char*) deviceUniqueIdUTF8))
#if defined(WEBRTC_MAC_INTEL) || defined(WEBRTC_LINUX)
        || (strncasecmp((char*)_lastUsedDeviceName,
                        (char*) deviceUniqueIdUTF8,
                        _lastUsedDeviceNameLength)!=0))
#else
        || (_strnicmp((char*) _lastUsedDeviceName,
                      (char*) deviceUniqueIdUTF8,
                      _lastUsedDeviceNameLength) != 0))
#endif

    {
        _apiLock.ReleaseLockShared();
        _apiLock.AcquireLockExclusive();
        if (-1 == CreateCapabilityMap(deviceUniqueIdUTF8))
        {
            _apiLock.ReleaseLockExclusive();
            _apiLock.AcquireLockShared();
            return -1;
        }
        _apiLock.ReleaseLockExclusive();
        _apiLock.AcquireLockShared();
    }

    // Make sure the number is valid
    if (deviceCapabilityNumber >= (unsigned int) _captureCapabilities.Size())
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                   "deviceCapabilityNumber %d is invalid in call to GetCapability",
                   deviceCapabilityNumber);
        return -1;
    }

    MapItem* item = _captureCapabilities.Find(deviceCapabilityNumber);
    if (!item)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                   "Failed to find capability number %d of %d possible",
                   deviceCapabilityNumber, _captureCapabilities.Size());
        return -1;
    }

    VideoCaptureCapability* capPointer =  static_cast<VideoCaptureCapability*>
                                          (item->GetItem());
    if (!capPointer)
    {
        return -1;
    }

    capability = *capPointer;
    return 0;
}

WebRtc_Word32 DeviceInfoImpl::GetBestMatchedCapability(
                                        const char*deviceUniqueIdUTF8,
                                        const VideoCaptureCapability& requested,
                                        VideoCaptureCapability& resulting)
{


    if (!deviceUniqueIdUTF8)
        return -1;

    ReadLockScoped cs(_apiLock);
    if ((_lastUsedDeviceNameLength != strlen((char*) deviceUniqueIdUTF8))
#if defined(WEBRTC_MAC_INTEL) || defined(WEBRTC_LINUX)
        || (strncasecmp((char*)_lastUsedDeviceName,
                        (char*) deviceUniqueIdUTF8,
                        _lastUsedDeviceNameLength)!=0))
#else
        || (_strnicmp((char*) _lastUsedDeviceName,
                      (char*) deviceUniqueIdUTF8,
                      _lastUsedDeviceNameLength) != 0))
#endif
    {
        _apiLock.ReleaseLockShared();
        _apiLock.AcquireLockExclusive();
        if (-1 == CreateCapabilityMap(deviceUniqueIdUTF8))
        {
            return -1;
        }
        _apiLock.ReleaseLockExclusive();
        _apiLock.AcquireLockShared();
    }

    WebRtc_Word32 bestformatIndex = -1;
    WebRtc_Word32 bestWidth = 0;
    WebRtc_Word32 bestHeight = 0;
    WebRtc_Word32 bestFrameRate = 0;
    RawVideoType bestRawType = kVideoUnknown;
    webrtc::VideoCodecType bestCodecType = webrtc::kVideoCodecUnknown;

    const WebRtc_Word32 numberOfCapabilies = _captureCapabilities.Size();

    for (WebRtc_Word32 tmp = 0; tmp < numberOfCapabilies; ++tmp) // Loop through all capabilities
    {
        MapItem* item = _captureCapabilities.Find(tmp);
        if (!item)
            return -1;

        VideoCaptureCapability& capability = *static_cast<VideoCaptureCapability*>
                                              (item->GetItem());

        const WebRtc_Word32 diffWidth = capability.width - requested.width;
        const WebRtc_Word32 diffHeight = capability.height - requested.height;
        const WebRtc_Word32 diffFrameRate = capability.maxFPS - requested.maxFPS;

        const WebRtc_Word32 currentbestDiffWith = bestWidth - requested.width;
        const WebRtc_Word32 currentbestDiffHeight = bestHeight - requested.height;
        const WebRtc_Word32 currentbestDiffFrameRate = bestFrameRate - requested.maxFPS;

        if ((diffHeight >= 0 && diffHeight <= abs(currentbestDiffHeight)) // Height better or equalt that previouse.
            || (currentbestDiffHeight < 0 && diffHeight >= currentbestDiffHeight))
        {

            if (diffHeight == currentbestDiffHeight) // Found best height. Care about the width)
            {
                if ((diffWidth >= 0 && diffWidth <= abs(currentbestDiffWith)) // Width better or equal
                    || (currentbestDiffWith < 0 && diffWidth >= currentbestDiffWith))
                {
                    if (diffWidth == currentbestDiffWith && diffHeight
                        == currentbestDiffHeight) // Same size as previously
                    {
                        //Also check the best frame rate if the diff is the same as previouse
                        if (((diffFrameRate >= 0 &&
                              diffFrameRate <= currentbestDiffFrameRate) // Frame rate to high but better match than previouse and we have not selected IUV
                            ||
                            (currentbestDiffFrameRate < 0 &&
                             diffFrameRate >= currentbestDiffFrameRate)) // Current frame rate is lower than requested. This is better.
                        )
                        {
                            if ((currentbestDiffFrameRate == diffFrameRate) // Same frame rate as previous  or frame rate allready good enough
                                || (currentbestDiffFrameRate >= 0))
                            {
                                if (bestRawType != requested.rawType
                                    && requested.rawType != kVideoUnknown
                                    && (capability.rawType == requested.rawType
                                        || capability.rawType == kVideoI420
                                        || capability.rawType == kVideoYUY2
                                        || capability.rawType == kVideoYV12))
                                {
                                    bestCodecType = capability.codecType;
                                    bestRawType = capability.rawType;
                                    bestformatIndex = tmp;
                                }
                                // If width height and frame rate is full filled we can use the camera for encoding if it is supported.
                                if (capability.height == requested.height
                                    && capability.width == requested.width
                                    && capability.maxFPS >= requested.maxFPS)
                                {
                                    if (capability.codecType == requested.codecType
                                        && bestCodecType != requested.codecType)
                                    {
                                        bestCodecType = capability.codecType;
                                        bestformatIndex = tmp;
                                    }
                                }
                            }
                            else // Better frame rate
                            {
                                if (requested.codecType == capability.codecType)
                                {

                                    bestWidth = capability.width;
                                    bestHeight = capability.height;
                                    bestFrameRate = capability.maxFPS;
                                    bestCodecType = capability.codecType;
                                    bestRawType = capability.rawType;
                                    bestformatIndex = tmp;
                                }
                            }
                        }
                    }
                    else // Better width than previously
                    {
                        if (requested.codecType == capability.codecType)
                        {
                            bestWidth = capability.width;
                            bestHeight = capability.height;
                            bestFrameRate = capability.maxFPS;
                            bestCodecType = capability.codecType;
                            bestRawType = capability.rawType;
                            bestformatIndex = tmp;
                        }
                    }
                }// else width no good
            }
            else // Better height
            {
                if (requested.codecType == capability.codecType)
                {
                    bestWidth = capability.width;
                    bestHeight = capability.height;
                    bestFrameRate = capability.maxFPS;
                    bestCodecType = capability.codecType;
                    bestRawType = capability.rawType;
                    bestformatIndex = tmp;
                }
            }
        }// else height not good
    }//end for

    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, _id,
               "Best camera format: Width %d, Height %d, Frame rate %d, Color format %d",
               bestWidth, bestHeight, bestFrameRate, bestRawType);

    // Copy the capability
    MapItem* item = _captureCapabilities.Find(bestformatIndex);
    if (!item)
        return -1;
    VideoCaptureCapability* capPointer =
        static_cast<VideoCaptureCapability*> (item->GetItem());
    if (!capPointer)
        return -1;

    resulting = *capPointer;

    return bestformatIndex;
}

/* Returns the expected Capture delay*/
WebRtc_Word32 DeviceInfoImpl::GetExpectedCaptureDelay(
                                          const DelayValues delayValues[],
                                          const WebRtc_UWord32 sizeOfDelayValues,
                                          const char* productId,
                                          const WebRtc_UWord32 width,
                                          const WebRtc_UWord32 height)
{
    WebRtc_Word32 bestDelay = kDefaultCaptureDelay;

    for (WebRtc_UWord32 device = 0; device < sizeOfDelayValues; ++device)
    {
        if (delayValues[device].productId && strncmp((char*) productId,
                                                     (char*) delayValues[device].productId,
                                                     kVideoCaptureProductIdLength) == 0)
        {
            // We have found the camera

            WebRtc_Word32 bestWidth = 0;
            WebRtc_Word32 bestHeight = 0;

            //Loop through all tested sizes and find one that seems fitting
            for (WebRtc_UWord32 delayIndex = 0; delayIndex < NoOfDelayValues; ++delayIndex)
            {
                const DelayValue& currentValue = delayValues[device].delayValues[delayIndex];

                const WebRtc_Word32 diffWidth = currentValue.width - width;
                const WebRtc_Word32 diffHeight = currentValue.height - height;

                const WebRtc_Word32 currentbestDiffWith = bestWidth - width;
                const WebRtc_Word32 currentbestDiffHeight = bestHeight - height;

                if ((diffHeight >= 0 && diffHeight <= abs(currentbestDiffHeight)) // Height better or equal than previous.
                    || (currentbestDiffHeight < 0 && diffHeight >= currentbestDiffHeight))
                {

                    if (diffHeight == currentbestDiffHeight) // Found best height. Care about the width)
                    {
                        if ((diffWidth >= 0 && diffWidth <= abs(currentbestDiffWith)) // Width better or equal
                            || (currentbestDiffWith < 0 && diffWidth >= currentbestDiffWith))
                        {
                            if (diffWidth == currentbestDiffWith && diffHeight
                                == currentbestDiffHeight) // Same size as previous
                            {
                            }
                            else // Better width than previously
                            {
                                bestWidth = currentValue.width;
                                bestHeight = currentValue.height;
                                bestDelay = currentValue.delay;
                            }
                        }// else width no good
                    }
                    else // Better height
                    {
                        bestWidth = currentValue.width;
                        bestHeight = currentValue.height;
                        bestDelay = currentValue.delay;
                    }
                }// else height not good
            }//end for
            break;
        }
    }
    if (bestDelay > kMaxCaptureDelay)
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideoCapture, _id,
                   "Expected capture delay too high. %dms, will use %d", bestDelay,
                   kMaxCaptureDelay);
        bestDelay = kMaxCaptureDelay;

    }

    return bestDelay;

}

//Default implementation. This should be overridden by Mobile implementations.
WebRtc_Word32 DeviceInfoImpl::GetOrientation(const char* deviceUniqueIdUTF8,
                                             VideoCaptureRotation& orientation)
{
    orientation = kCameraRotate0;
    return -1;
}
} //namespace videocapturemodule
} // namespace webrtc


