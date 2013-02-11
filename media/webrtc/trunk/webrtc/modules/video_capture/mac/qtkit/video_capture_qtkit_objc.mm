/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#define DEFAULT_CAPTURE_DEVICE_INDEX    1
#define DEFAULT_FRAME_RATE              30
#define DEFAULT_FRAME_WIDTH                352
#define DEFAULT_FRAME_HEIGHT            288
#define ROTATE_CAPTURED_FRAME           1
#define LOW_QUALITY                     1

#import "video_capture_qtkit_objc.h"
#include "video_capture_qtkit_utility.h"
#include "trace.h"

using namespace webrtc;
using namespace videocapturemodule;

@implementation VideoCaptureMacQTKitObjC

#pragma mark **** over-written OS methods

/// ***** Objective-C. Similar to C++ constructor, although must be invoked
///       manually.
/// ***** Potentially returns an instance of self
-(id)init{
    self = [super init];
    if(nil != self)
    {
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
    if(_captureSession)
    {
        [_captureSession stopRunning];
        [_captureSession release];
    }
    [super dealloc];
}

#pragma mark **** public methods



/// ***** Registers the class's owner, which is where the delivered frames are
///       sent
/// ***** Returns 0 on success, -1 otherwise.
- (NSNumber*)registerOwner:(VideoCaptureMacQTKit*)owner{
    if(!owner){
        return [NSNumber numberWithInt:-1];
    }
    _owner = owner;
    return [NSNumber numberWithInt:0];
}

/// ***** Sets the QTCaptureSession's input device from a char*
/// ***** Sets several member variables. Can signal the error system if one has
///       occurred
/// ***** Returns 0 on success, -1 otherwise.
- (NSNumber*)setCaptureDeviceById:(char*)uniqueId{
    if(NO == _OSSupported)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceVideoCapture, 0,
                     "%s:%d OS version does not support necessary APIs",
                     __FUNCTION__, __LINE__);
        return [NSNumber numberWithInt:0];
    }

    if(!uniqueId || (0 == strcmp("", uniqueId)))
    {
        WEBRTC_TRACE(kTraceInfo, kTraceVideoCapture, 0,
                     "%s:%d  \"\" was passed in for capture device name",
                     __FUNCTION__, __LINE__);
        memset(_captureDeviceNameUTF8, 0, 1024);
        return [NSNumber numberWithInt:0];
    }

    if(0 == strcmp(uniqueId, _captureDeviceNameUniqueID))
    {
        // camera already set
        WEBRTC_TRACE(kTraceInfo, kTraceVideoCapture, 0,
                     "%s:%d Capture device is already set to %s", __FUNCTION__,
                     __LINE__, _captureDeviceNameUTF8);
        return [NSNumber numberWithInt:0];
    }

    bool success = NO;
    QTCaptureDevice* tempCaptureDevice;
    for(int index = 0; index < _captureDeviceCount; index++)
    {
        tempCaptureDevice = (QTCaptureDevice*)[_captureDevices
                                               objectAtIndex:index];
        char tempCaptureDeviceId[1024] = "";
        [[tempCaptureDevice uniqueID]
          getCString:tempCaptureDeviceId maxLength:1024
          encoding:NSUTF8StringEncoding];
        if(0 == strcmp(uniqueId, tempCaptureDeviceId))
        {
            WEBRTC_TRACE(kTraceInfo, kTraceVideoCapture, 0,
                         "%s:%d Found capture device id %s as index %d",
                         __FUNCTION__, __LINE__, tempCaptureDeviceId, index);
            success = YES;
          [[tempCaptureDevice localizedDisplayName]
              getCString:_captureDeviceNameUTF8
               maxLength:1024
                encoding:NSUTF8StringEncoding];
          [[tempCaptureDevice uniqueID]
              getCString:_captureDeviceNameUniqueID
               maxLength:1024
                encoding:NSUTF8StringEncoding];
            break;
        }

    }

    if(NO == success)
    {
        // camera not found
        // nothing has been changed yet, so capture device will stay in it's
        // state
        WEBRTC_TRACE(kTraceInfo, kTraceVideoCapture, 0,
                     "%s:%d Capture device id %s was not found in list of "
                     "available devices.", __FUNCTION__, __LINE__, uniqueId);
        return [NSNumber numberWithInt:0];
    }

    NSError* error;
    success = [tempCaptureDevice open:&error];
    if(!success)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideoCapture, 0,
                     "%s:%d Failed to open capture device: %s",
                     __FUNCTION__, __LINE__, _captureDeviceNameUTF8);
        return [NSNumber numberWithInt:-1];
    }

    if(_captureVideoDeviceInput)
    {
        [_captureVideoDeviceInput release];
    }
    _captureVideoDeviceInput = [[QTCaptureDeviceInput alloc]
                                 initWithDevice:tempCaptureDevice];

    success = [_captureSession addInput:_captureVideoDeviceInput error:&error];
    if(!success)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideoCapture, 0,
                     "%s:%d Failed to add input from %s to the capture session",
                     __FUNCTION__, __LINE__, _captureDeviceNameUTF8);
        return [NSNumber numberWithInt:-1];
    }

    WEBRTC_TRACE(kTraceInfo, kTraceVideoCapture, 0,
                 "%s:%d successfully added capture device: %s", __FUNCTION__,
                 __LINE__, _captureDeviceNameUTF8);
    return [NSNumber numberWithInt:0];
}


/// ***** Updates the capture devices size and frequency
/// ***** Sets member variables _frame* and _captureDecompressedVideoOutput
/// ***** Returns 0 on success, -1 otherwise.
- (NSNumber*)setCaptureHeight:(int)height AndWidth:(int)width
             AndFrameRate:(int)frameRate{
    if(NO == _OSSupported)
    {
        return [NSNumber numberWithInt:0];
    }

    _frameWidth = width;
    _frameHeight = height;
    _frameRate = frameRate;

    // TODO(mflodman) Check fps settings.
    // [_captureDecompressedVideoOutput
    //     setMinimumVideoFrameInterval:(NSTimeInterval)1/(float)_frameRate];
    NSDictionary* captureDictionary = [NSDictionary dictionaryWithObjectsAndKeys:
                                       [NSNumber numberWithDouble:_frameWidth], (id)kCVPixelBufferWidthKey,
                                       [NSNumber numberWithDouble:_frameHeight], (id)kCVPixelBufferHeightKey,
                                       [NSNumber numberWithUnsignedInt:kCVPixelFormatType_32ARGB],
                                       (id)kCVPixelBufferPixelFormatTypeKey, nil]; 
    [_captureDecompressedVideoOutput performSelectorOnMainThread:@selector(setPixelBufferAttributes:) withObject:captureDictionary waitUntilDone:NO];
//    [_captureDecompressedVideoOutput setPixelBufferAttributes:captureDictionary];

        
    // these methods return type void so there isn't much we can do about
    // checking success
    return [NSNumber numberWithInt:0];
}

/// ***** Starts the QTCaptureSession, assuming correct state. Also ensures that
///       an NSRunLoop is running
/// ***** Without and NSRunLoop to process events, the OS doesn't check for a
///       new frame.
/// ***** Sets member variables _capturing
/// ***** Returns 0 on success, -1 otherwise.
- (NSNumber*)startCapture{
    if(NO == _OSSupported)
    {
        return [NSNumber numberWithInt:0];
    }

    if(YES == _capturing)
    {
        return [NSNumber numberWithInt:0];
    }
  
//    NSLog(@"--------------- before ---------------");
    [[NSRunLoop mainRunLoop] runUntilDate:[NSDate distantFuture]];
//    NSLog(@"--------------- after ---------------");

    if(NO == _captureInitialized)
    {
        // this should never be called..... it is initialized on class init
        [self initializeVideoCapture];
    }    
    [_captureSession startRunning];

    
    _capturing = YES;

    return [NSNumber numberWithInt:0];
}

/// ***** Stops the QTCaptureSession, assuming correct state
/// ***** Sets member variables _capturing
/// ***** Returns 0 on success, -1 otherwise.
- (NSNumber*)stopCapture{

    if(NO == _OSSupported)
    {
        return [NSNumber numberWithInt:0];
    }

    if(nil == _captureSession)
    {
        return [NSNumber numberWithInt:0];
    }

    if(NO == _capturing)
    {
        return [NSNumber numberWithInt:0];
    }

    if(YES == _capturing)
    {
        [_captureSession stopRunning];
    }

    _capturing = NO;
    return [NSNumber numberWithInt:0];
}

// ********** "private" functions below here **********
#pragma mark **** "private" methods

/// ***** Class member variables are initialized here
/// ***** Returns 0 on success, -1 otherwise.
- (NSNumber*)initializeVariables{

    if(NO == _OSSupported)
    {
        return [NSNumber numberWithInt:0];
    }

    _pool = [[NSAutoreleasePool alloc]init];

    memset(_captureDeviceNameUTF8, 0, 1024);
    _counter = 0;
    _framesDelivered = 0;
    _framesRendered = 0;
    _captureDeviceCount = 0;
    _capturing = NO;
    _captureInitialized = NO;
    _frameRate = DEFAULT_FRAME_RATE;
    _frameWidth = DEFAULT_FRAME_WIDTH;
    _frameHeight = DEFAULT_FRAME_HEIGHT;
    _captureDeviceName = [[NSString alloc] initWithFormat:@""];
    _rLock = [[VideoCaptureRecursiveLock alloc] init];
    _captureSession = [[QTCaptureSession alloc] init];
    _captureDecompressedVideoOutput = [[QTCaptureDecompressedVideoOutput alloc]
                                        init];
    [_captureDecompressedVideoOutput setDelegate:self];

    [self getCaptureDevices];
    [self initializeVideoCapture];

    return [NSNumber numberWithInt:0];

}

// Checks to see if the QTCaptureSession framework is available in the OS
// If it is not, isOSSupprted = NO.
// Throughout the rest of the class isOSSupprted is checked and functions
// are/aren't called depending
// The user can use weak linking to the QTKit framework and run on older
// versions of the OS. I.E. Backwards compaitibility
// Returns nothing. Sets member variable
- (void)checkOSSupported{

    Class osSupportedTest = NSClassFromString(@"QTCaptureSession");
    _OSSupported = NO;
    if(nil == osSupportedTest)
    {
    }
    _OSSupported = YES;
}

/// ***** Retrieves the number of capture devices currently available
/// ***** Stores them in an NSArray instance
/// ***** Returns 0 on success, -1 otherwise.
- (NSNumber*)getCaptureDevices{

    if(NO == _OSSupported)
    {
        return [NSNumber numberWithInt:0];
    }

    if(_captureDevices)
    {
        [_captureDevices release];
    }
    _captureDevices = [[NSArray alloc] initWithArray:
        [QTCaptureDevice inputDevicesWithMediaType:QTMediaTypeVideo]];

    _captureDeviceCount = _captureDevices.count;
    if(_captureDeviceCount < 1)
    {
        return [NSNumber numberWithInt:0];
    }
    return [NSNumber numberWithInt:0];
}

// Initializes a QTCaptureSession (member variable) to deliver frames via
// callback
// QTCapture* member variables affected
// The image format and frequency are setup here
// Returns 0 on success, -1 otherwise.
- (NSNumber*)initializeVideoCapture{

    if(YES == _captureInitialized)
    {
        return [NSNumber numberWithInt:-1];
    }

    QTCaptureDevice* videoDevice =
        (QTCaptureDevice*)[_captureDevices objectAtIndex:0];

    bool success = NO;
    NSError*    error;

    success = [videoDevice open:&error];
    if(!success)
    {
        return [NSNumber numberWithInt:-1];
    }

    [_captureDecompressedVideoOutput setPixelBufferAttributes:
        [NSDictionary dictionaryWithObjectsAndKeys:
            [NSNumber numberWithDouble:_frameWidth], (id)kCVPixelBufferWidthKey,
            [NSNumber numberWithDouble:_frameHeight], (id)kCVPixelBufferHeightKey,
            [NSNumber numberWithUnsignedInt:kCVPixelFormatType_32ARGB],
            (id)kCVPixelBufferPixelFormatTypeKey, nil]];

    // TODO(mflodman) Check fps settings.
    //[_captureDecompressedVideoOutput setMinimumVideoFrameInterval:
    //    (NSTimeInterval)1/(float)_frameRate];
    //[_captureDecompressedVideoOutput setAutomaticallyDropsLateVideoFrames:YES];

    success = [_captureSession addOutput:_captureDecompressedVideoOutput
               error:&error];

    if(!success)
    {
        return [NSNumber numberWithInt:-1];
    }

    _captureInitialized = YES;

    return [NSNumber numberWithInt:0];
}

// This is the callback that is called when the OS has a frame to deliver to us.
// Starts being called when [_captureSession startRunning] is called. Stopped
// similarly.
// Parameter videoFrame contains the image. The format, size, and frequency
// were setup earlier.
// Returns 0 on success, -1 otherwise.
- (void)captureOutput:(QTCaptureOutput *)captureOutput
    didOutputVideoFrame:(CVImageBufferRef)videoFrame
     withSampleBuffer:(QTSampleBuffer *)sampleBuffer
     fromConnection:(QTCaptureConnection *)connection{

    if(YES == [_rLock tryLock])
    {
        [_rLock lock];
    }
    else
    {
        return;
    }

    if(NO == _OSSupported)
    {
        return;
    }

    const int LOCK_FLAGS = 0; // documentation says to pass 0

    // get size of the frame
    CVPixelBufferLockBaseAddress(videoFrame, LOCK_FLAGS);
    void* baseAddress = CVPixelBufferGetBaseAddress(videoFrame);
    size_t bytesPerRow = CVPixelBufferGetBytesPerRow(videoFrame);
    int frameHeight = CVPixelBufferGetHeight(videoFrame);
    CVPixelBufferUnlockBaseAddress(videoFrame, LOCK_FLAGS);

    if(_owner)
    {

        int frameSize = bytesPerRow * frameHeight;    // 32 bit ARGB format
        CVBufferRetain(videoFrame);
        VideoCaptureCapability tempCaptureCapability;
        tempCaptureCapability.width = _frameWidth;
        tempCaptureCapability.height = _frameHeight;
        tempCaptureCapability.maxFPS = _frameRate;
        // TODO(wu) : Update actual type and not hard-coded value. 
        tempCaptureCapability.rawType = kVideoBGRA;

        _owner->IncomingFrame((unsigned char*)baseAddress,
                              frameSize,
                              tempCaptureCapability,
                              0);

        CVBufferRelease(videoFrame);
    }

    _framesDelivered++;
    _framesRendered++;

    if(YES == [_rLock locked])
    {
        [_rLock unlock];
    }
}

@end
