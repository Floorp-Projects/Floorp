/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#pragma mark **** imports/includes

#import "webrtc/modules/video_capture/mac/avfoundation/video_capture_avfoundation_info_objc.h"

#include "webrtc/system_wrappers/include/trace.h"

using namespace webrtc;
using namespace videocapturemodule;

#pragma mark **** hidden class interface

@implementation VideoCaptureMacAVFoundationInfoObjC

// ****************** over-written OS methods ***********************
#pragma mark **** over-written OS methods

/// ***** Objective-C. Similar to C++ constructor, although invoked manually
/// ***** Potentially returns an instance of self
-(id)init{
    self = [super init];
    if(nil != self){
        [self checkOSSupported];
        [self initializeVariables];
    }
    else
    {
        return nil;
    }
    return self;
}

- (void)registerOwner:(VideoCaptureMacAVFoundationInfo*)owner {
    [_lock lock];
    _owner = owner;
    [_lock unlock];
}

/// ***** Objective-C. Similar to C++ destructor
/// ***** Returns nothing
- (void)dealloc {

    [_captureDevicesInfo release];

    // Remove Observers
    NSNotificationCenter* notificationCenter = [NSNotificationCenter defaultCenter];
    for (id observer in _observers)
        [notificationCenter removeObserver:observer];
    [_observers release];
    [_lock release];

    for (NSMutableArray* capabilityMap in _capabilityMaps) {
        [capabilityMap removeAllObjects];
        [capabilityMap release];
    }

    [_capabilityMaps removeAllObjects];
    [_capabilityMaps release];

    [super dealloc];
}

// ****************** public methods ******************
#pragma mark **** public method implementations

/// ***** Creates a message box with Cocoa framework
/// ***** Returns 0 on success, -1 otherwise.
- (NSNumber*)displayCaptureSettingsDialogBoxWithDevice:(const char*)deviceUniqueIdUTF8
                    AndTitle:(const char*)dialogTitleUTF8
                    AndParentWindow:(void*) parentWindow
                    AtX:(uint32_t)positionX
                    AndY:(uint32_t) positionY
{
    NSString* strTitle = [NSString stringWithFormat:@"%s", dialogTitleUTF8];
    NSString* strButton = @"Alright";
    NSAlert* alert = [NSAlert alertWithMessageText:strTitle
                      defaultButton:strButton
                      alternateButton:nil otherButton:nil
                      informativeTextWithFormat:@"Device %s is capturing", deviceUniqueIdUTF8];
    [alert setAlertStyle:NSInformationalAlertStyle];
    [alert runModal];
    return [NSNumber numberWithInt:0];
}

- (NSNumber*)getCaptureDeviceCount{
    [self getCaptureDevices];
    return [NSNumber numberWithInt:_captureDeviceCountInfo];
}

- (NSNumber*)getCaptureCapabilityCount:(const char*)uniqueId {

    AVCaptureDevice* captureDevice = nil;
    if (uniqueId == nil || !strcmp("", uniqueId)) {
        WEBRTC_TRACE(kTraceInfo, kTraceVideoCapture, 0,
            "Incorrect capture id argument");
        return [NSNumber numberWithInt:-1];
    }

    int deviceIndex;

    for (deviceIndex = 0; deviceIndex < _captureDeviceCountInfo; deviceIndex++) {
        captureDevice = (AVCaptureDevice*)[_captureDevicesInfo objectAtIndex:deviceIndex];
        char captureDeviceId[1024] = "";
        [[captureDevice uniqueID] getCString:captureDeviceId
                                   maxLength:1024
                                    encoding:NSUTF8StringEncoding];
        if (strcmp(uniqueId, captureDeviceId) == 0) {
            WEBRTC_TRACE(kTraceInfo, kTraceVideoCapture, 0,
                "%s:%d Found capture device id %s as index %d",
                __FUNCTION__, __LINE__, captureDeviceId, deviceIndex);
            break;
        }
        captureDevice = nil;
    }

    if (!captureDevice)
        return [NSNumber numberWithInt:-1];

    NSMutableArray* capabilityMap = (NSMutableArray*)[_capabilityMaps objectForKey:[NSNumber numberWithInt:deviceIndex]];

    if (capabilityMap != nil) {
        [capabilityMap removeAllObjects];
    } else {
        capabilityMap = [[NSMutableArray alloc] init];
        [_capabilityMaps setObject:capabilityMap forKey:[NSNumber numberWithInt:deviceIndex]];
    }

    int count = 0;

    for (int formatIndex = 0; formatIndex < (int)[captureDevice formats].count; formatIndex++) {
        AVCaptureDeviceFormat* format =
            (AVCaptureDeviceFormat*) [[captureDevice formats] objectAtIndex:formatIndex];

        count += format.videoSupportedFrameRateRanges.count;
        for (int frameRateIndex = 0;
                frameRateIndex < (int) format.videoSupportedFrameRateRanges.count;
                frameRateIndex++) {
            [capabilityMap addObject: [NSNumber numberWithInt:((formatIndex << 16) + (frameRateIndex & 0xffff))]];
        }
    }

    return [NSNumber numberWithInt:count];
}

- (NSNumber*)getCaptureCapability:(const char*)uniqueId
                     CapabilityId:(uint32_t)capabilityId
                 Capability_width:(int32_t*)width
                Capability_height:(int32_t*)height
                Capability_maxFPS:(int32_t*)maxFPS
                Capability_format:(webrtc::RawVideoType*)rawType
{
    AVCaptureDevice* captureDevice = nil;
    if (uniqueId == nil || !strcmp("", uniqueId)) {
        WEBRTC_TRACE(kTraceInfo, kTraceVideoCapture, 0,
            "Incorrect capture id argument");
        return [NSNumber numberWithInt:-1];
    }

    int deviceIndex;

    for (deviceIndex = 0; deviceIndex < _captureDeviceCountInfo; deviceIndex++) {
        captureDevice = (AVCaptureDevice*)[_captureDevicesInfo objectAtIndex:deviceIndex];
        char captureDeviceId[1024] = "";
        [[captureDevice uniqueID] getCString:captureDeviceId
                                   maxLength:1024
                                    encoding:NSUTF8StringEncoding];
        if (strcmp(uniqueId, captureDeviceId) == 0) {
            WEBRTC_TRACE(kTraceInfo, kTraceVideoCapture, 0,
                "%s:%d Found capture device id %s as index %d",
                __FUNCTION__, __LINE__, captureDeviceId, deviceIndex);
            break;
        }
        captureDevice = nil;
    }

    if (!captureDevice)
        return [NSNumber numberWithInt:-1];

    NSMutableArray* capabilityMap = [_capabilityMaps objectForKey:[NSNumber numberWithInt:deviceIndex]];
    NSNumber* indexNumber = [capabilityMap objectAtIndex:capabilityId];

    // protection for illegal capabilityId
    if (!indexNumber)
        return [NSNumber numberWithInt:-1];

    int indexInt = static_cast<int>([indexNumber integerValue]);
    int formatIndex = indexInt >> 16;
    int frameRateIndex = indexInt & 0xffff;

    AVCaptureDeviceFormat* format = (AVCaptureDeviceFormat*)[[captureDevice formats]objectAtIndex:formatIndex];
    CMVideoDimensions videoDimensions = CMVideoFormatDescriptionGetDimensions(format.formatDescription);
    AVFrameRateRange* frameRateRange = [format.videoSupportedFrameRateRanges objectAtIndex:frameRateIndex];

    *width = videoDimensions.width;
    *height = videoDimensions.height;

    // This is to fix setCaptureHeight() which fails for some webcams supporting non-integer framerates.
    // In setCaptureHeight(), we match the best framerate range by searching a range whose max framerate
    // is most close to (but smaller than or equal to) the target. Since maxFPS of capability is integer,
    // we fill in the capability maxFPS with the floor value (e.g., 29) of the real supported fps
    // (e.g., 29.97). If the target is set to 29, we failed to match the best format with max framerate
    // 29.97 since it is over the target. Therefore, we need to return a ceiling value as the maxFPS here.
    *maxFPS = static_cast<int32_t>(ceil(frameRateRange.maxFrameRate));
    *rawType = [VideoCaptureMacAVFoundationUtility fourCCToRawVideoType:CMFormatDescriptionGetMediaSubType(format.formatDescription)];

    return [NSNumber numberWithInt:0];
}

- (NSNumber*)getDeviceNamesFromIndex:(uint32_t)index
    DefaultName:(char*)deviceName
    WithLength:(uint32_t)deviceNameLength
    AndUniqueID:(char*)deviceUniqueID
    WithLength:(uint32_t)deviceUniqueIDLength
    AndProductID:(char*)deviceProductID
    WithLength:(uint32_t)deviceProductIDLength
{
    if(NO == _OSSupportedInfo)
    {
        return [NSNumber numberWithInt:0];
    }

    if(index >= (uint32_t)_captureDeviceCountInfo)
    {
        return [NSNumber numberWithInt:-1];
    }

    if ([_captureDevicesInfo count] <= index)
    {
      return [NSNumber numberWithInt:-1];
    }

    AVCaptureDevice* tempCaptureDevice = (AVCaptureDevice*)[_captureDevicesInfo objectAtIndex:index];
    if(!tempCaptureDevice)
    {
      return [NSNumber numberWithInt:-1];
    }

    memset(deviceName, 0, deviceNameLength);
    memset(deviceUniqueID, 0, deviceUniqueIDLength);

    bool successful = NO;

    NSString* tempString = [tempCaptureDevice localizedName];
    successful = [tempString getCString:(char*)deviceName
                  maxLength:deviceNameLength encoding:NSUTF8StringEncoding];
    if(NO == successful)
    {
        memset(deviceName, 0, deviceNameLength);
        return [NSNumber numberWithInt:-1];
    }

    tempString = [tempCaptureDevice uniqueID];
    successful = [tempString getCString:(char*)deviceUniqueID
                  maxLength:deviceUniqueIDLength encoding:NSUTF8StringEncoding];
    if(NO == successful)
    {
        memset(deviceUniqueID, 0, deviceNameLength);
        return [NSNumber numberWithInt:-1];
    }

    return [NSNumber numberWithInt:0];

}

// ****************** "private" category functions below here  ******************
#pragma mark **** "private" method implementations

- (NSNumber*)initializeVariables
{
    if(NO == _OSSupportedInfo)
    {
        return [NSNumber numberWithInt:0];
    }

    _captureDeviceCountInfo = 0;
    [self getCaptureDevices];

    _lock = [[NSLock alloc] init];

    //register device connected / disconnected event
    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];

    id deviceWasConnectedObserver = [notificationCenter addObserverForName:AVCaptureDeviceWasConnectedNotification
        object:nil
        queue:[NSOperationQueue mainQueue]
        usingBlock:^(NSNotification *note) {
            [_lock lock];
            if(_owner)
                _owner->DeviceChange();
            [_lock unlock];
        }];

    id deviceWasDisconnectedObserver = [notificationCenter addObserverForName:AVCaptureDeviceWasDisconnectedNotification
        object:nil
        queue:[NSOperationQueue mainQueue]
        usingBlock:^(NSNotification *note) {
            [_lock lock];
            if(_owner)
                _owner->DeviceChange();
            [_lock unlock];
        }];

    _observers = [[NSArray alloc] initWithObjects:deviceWasConnectedObserver, deviceWasDisconnectedObserver, nil];

    _capabilityMaps = [[NSMutableDictionary alloc] init];

    return [NSNumber numberWithInt:0];
}

// ***** Checks to see if the AVCaptureSession framework is available in the OS
// ***** If it is not, isOSSupprted = NO
// ***** Throughout the rest of the class isOSSupprted is checked and functions
// ***** are/aren't called depending
// ***** The user can use weak linking to the AVFoundation framework and run on older
// ***** versions of the OS
// ***** I.E. Backwards compaitibility
// ***** Returns nothing. Sets member variable
- (void)checkOSSupported
{
    Class osSupportedTest = NSClassFromString(@"AVCaptureSession");
    if(nil == osSupportedTest)
    {
      _OSSupportedInfo = NO;
    }
    else
    {
      _OSSupportedInfo = YES;
    }
}

/// ***** Retrieves the number of capture devices currently available
/// ***** Stores them in an NSArray instance
/// ***** Returns 0 on success, -1 otherwise.
- (NSNumber*)getCaptureDevices
{
    if(NO == _OSSupportedInfo)
    {
        return [NSNumber numberWithInt:0];
    }

    if(_captureDevicesInfo)
    {
        [_captureDevicesInfo release];
    }
    _captureDevicesInfo = [[NSArray alloc]
                            initWithArray:[AVCaptureDevice
                                           devicesWithMediaType:AVMediaTypeVideo]];

    _captureDeviceCountInfo = _captureDevicesInfo.count;

    return [NSNumber numberWithInt:0];
}

@end
