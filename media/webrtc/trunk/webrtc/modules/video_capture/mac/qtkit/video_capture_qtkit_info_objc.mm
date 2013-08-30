/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#pragma mark **** imports/includes

#import "video_capture_qtkit_info_objc.h"

#include "trace.h"

using namespace webrtc;

#pragma mark **** hidden class interface

@implementation VideoCaptureMacQTKitInfoObjC

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

/// ***** Objective-C. Similar to C++ destructor
/// ***** Returns nothing
- (void)dealloc {

    [_captureDevicesInfo release];

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
    NSString* strMessage = [NSString stringWithFormat:@"Device %s is capturing", deviceUniqueIdUTF8];
    NSAlert* alert = [NSAlert alertWithMessageText:strTitle
                      defaultButton:strButton
                      alternateButton:nil otherButton:nil
                      informativeTextWithFormat:strMessage];
    [alert setAlertStyle:NSInformationalAlertStyle];
    [alert runModal];
    return [NSNumber numberWithInt:0];
}

- (NSNumber*)getCaptureDeviceCount{
    [self getCaptureDevices];
    return [NSNumber numberWithInt:_captureDeviceCountInfo];
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

    QTCaptureDevice* tempCaptureDevice =
        (QTCaptureDevice*)[_captureDevicesInfo objectAtIndex:index];
    if(!tempCaptureDevice)
    {
        return [NSNumber numberWithInt:-1];
    }

    memset(deviceName, 0, deviceNameLength);
    memset(deviceUniqueID, 0, deviceUniqueIDLength);

    bool successful = NO;

    NSString* tempString = [tempCaptureDevice localizedDisplayName];
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

    _poolInfo = [[NSAutoreleasePool alloc]init];
    _captureDeviceCountInfo = 0;
    [self getCaptureDevices];

    return [NSNumber numberWithInt:0];
}

// ***** Checks to see if the QTCaptureSession framework is available in the OS
// ***** If it is not, isOSSupprted = NO
// ***** Throughout the rest of the class isOSSupprted is checked and functions
// ***** are/aren't called depending
// ***** The user can use weak linking to the QTKit framework and run on older
// ***** versions of the OS
// ***** I.E. Backwards compaitibility
// ***** Returns nothing. Sets member variable
- (void)checkOSSupported
{
    Class osSupportedTest = NSClassFromString(@"QTCaptureSession");
    _OSSupportedInfo = NO;
    if(nil == osSupportedTest)
    {
    }
    _OSSupportedInfo = YES;
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
                            initWithArray:[QTCaptureDevice
                                           inputDevicesWithMediaType:QTMediaTypeVideo]];

    _captureDeviceCountInfo = _captureDevicesInfo.count;

    return [NSNumber numberWithInt:0];
}

@end
