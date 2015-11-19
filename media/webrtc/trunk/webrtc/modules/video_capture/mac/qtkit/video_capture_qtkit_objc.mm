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
#define DEFAULT_FRAME_WIDTH             352
#define DEFAULT_FRAME_HEIGHT            288
#define ROTATE_CAPTURED_FRAME           1
#define LOW_QUALITY                     1

#import "webrtc/modules/video_capture/mac/qtkit/video_capture_qtkit_objc.h"

#include "webrtc/system_wrappers/interface/trace.h"

using namespace webrtc;
using namespace videocapturemodule;

@implementation VideoCaptureMacQTKitObjC

-(id)init {
  self = [super init];
  if (self) {
    [self initializeVariables];
  }
  return self;
}

- (void)dealloc {
  if (_captureSession)
    [_captureSession stopRunning];

  if (_captureVideoDeviceInput) {
    if ([[_captureVideoDeviceInput device] isOpen])
      [[_captureVideoDeviceInput device] close];

    [_captureVideoDeviceInput release];
  }

  [_captureDecompressedVideoOutput release];
  [_captureSession release];
  [_captureDevices release];
  [_lock release];

  [super dealloc];
}

#pragma mark Public methods

- (void)registerOwner:(VideoCaptureMacQTKit*)owner {
  [_lock lock];
  _owner = owner;
  [_lock unlock];
}

- (BOOL)setCaptureDeviceById:(char*)uniqueId {
  if (uniqueId == nil || !strcmp("", uniqueId)) {
    WEBRTC_TRACE(kTraceInfo, kTraceVideoCapture, 0,
                 "Incorrect capture id argument");
    return NO;
  }

  if (!strcmp(uniqueId, _captureDeviceNameUniqueID))
    return YES;

  QTCaptureDevice* captureDevice;
  for(int index = 0; index < _captureDeviceCount; index++) {
    captureDevice = (QTCaptureDevice*)[_captureDevices objectAtIndex:index];
    char captureDeviceId[1024] = "";
    [[captureDevice uniqueID] getCString:captureDeviceId
                               maxLength:1024
                                encoding:NSUTF8StringEncoding];
    if (strcmp(uniqueId, captureDeviceId) == 0) {
      WEBRTC_TRACE(kTraceInfo, kTraceVideoCapture, 0,
                   "%s:%d Found capture device id %s as index %d",
                   __FUNCTION__, __LINE__, captureDeviceId, index);
      [[captureDevice localizedDisplayName] getCString:_captureDeviceNameUTF8
                                             maxLength:1024
                                              encoding:NSUTF8StringEncoding];
      [[captureDevice uniqueID] getCString:_captureDeviceNameUniqueID
                                 maxLength:1024
                                  encoding:NSUTF8StringEncoding];
      break;
    }
    captureDevice = nil;
  }

  if (!captureDevice)
    return NO;

  NSError* error;
  if (![captureDevice open:&error]) {
    WEBRTC_TRACE(kTraceError, kTraceVideoCapture, 0,
                 "Failed to open capture device: %s", _captureDeviceNameUTF8);
    return NO;
  }

  if (_captureVideoDeviceInput) {
    [_captureVideoDeviceInput release];
  }
  _captureVideoDeviceInput =
      [[QTCaptureDeviceInput alloc] initWithDevice:captureDevice];

  if (![_captureSession addInput:_captureVideoDeviceInput error:&error]) {
    WEBRTC_TRACE(kTraceError, kTraceVideoCapture, 0,
                 "Failed to add input from %s to the capture session",
                 _captureDeviceNameUTF8);
    return NO;
  }

  WEBRTC_TRACE(kTraceInfo, kTraceVideoCapture, 0,
               "%s:%d successfully added capture device: %s", __FUNCTION__,
               __LINE__, _captureDeviceNameUTF8);
  return YES;
}

- (void)setCaptureHeight:(int)height width:(int)width frameRate:(int)frameRate {
  _frameWidth = width;
  _frameHeight = height;
  _frameRate = frameRate;

  NSDictionary* captureDictionary =
      [NSDictionary dictionaryWithObjectsAndKeys:
          [NSNumber numberWithDouble:_frameWidth],
          (id)kCVPixelBufferWidthKey,
          [NSNumber numberWithDouble:_frameHeight],
          (id)kCVPixelBufferHeightKey,
          [NSNumber numberWithUnsignedInt:kCVPixelFormatType_32ARGB],
          (id)kCVPixelBufferPixelFormatTypeKey,
          nil];
  [_captureDecompressedVideoOutput
      performSelectorOnMainThread:@selector(setPixelBufferAttributes:)
                       withObject:captureDictionary
                    waitUntilDone:YES];
}

- (void)startCapture {
  if (_capturing)
    return;

  [_captureSession startRunning];
  _capturing = YES;
}

- (void)stopCapture {
  if (!_capturing)
    return;

  // This method is often called on a secondary thread.  Which means
  // that the following can sometimes run "too early", causing crashes
  // and/or weird errors concerning initialization.  On OS X 10.7 and
  // 10.8, the CoreMediaIO method CMIOUninitializeGraph() is called from
  // -[QTCaptureSession stopRunning].  If this is called too early,
  // low-level session data gets uninitialized before low-level code
  // is finished trying to use it.  The solution is to make stopRunning
  // always run on the main thread.  See bug 837539.
  [_captureSession performSelectorOnMainThread:@selector(stopRunning)
                   withObject:nil
                   waitUntilDone:NO];
  _capturing = NO;
}

#pragma mark Private methods

- (BOOL)initializeVariables {
  if (NSClassFromString(@"QTCaptureSession") == nil)
    return NO;

  memset(_captureDeviceNameUTF8, 0, 1024);
  _framesDelivered = 0;
  _framesRendered = 0;
  _captureDeviceCount = 0;
  _capturing = NO;
  _captureInitialized = NO;
  _frameRate = DEFAULT_FRAME_RATE;
  _frameWidth = DEFAULT_FRAME_WIDTH;
  _frameHeight = DEFAULT_FRAME_HEIGHT;
  _lock = [[NSLock alloc] init];
  _captureSession = [[QTCaptureSession alloc] init];
  _captureDecompressedVideoOutput =
      [[QTCaptureDecompressedVideoOutput alloc] init];
  [_captureDecompressedVideoOutput setDelegate:self];

  [self getCaptureDevices];
  if (![self initializeVideoCapture])
    return NO;

  return NO;
}

- (void)getCaptureDevices {
  if (_captureDevices)
    [_captureDevices release];

  _captureDevices = [[NSArray alloc] initWithArray:
      [QTCaptureDevice inputDevicesWithMediaType:QTMediaTypeVideo]];

  _captureDeviceCount = _captureDevices.count;
}

- (BOOL)initializeVideoCapture{
  NSDictionary *captureDictionary =
      [NSDictionary dictionaryWithObjectsAndKeys:
          [NSNumber numberWithDouble:_frameWidth],
          (id)kCVPixelBufferWidthKey,
          [NSNumber numberWithDouble:_frameHeight],
          (id)kCVPixelBufferHeightKey,
          [NSNumber numberWithUnsignedInt:kCVPixelFormatType_32ARGB],
          (id)kCVPixelBufferPixelFormatTypeKey,
          nil];

  [_captureDecompressedVideoOutput setPixelBufferAttributes:captureDictionary];
  [_captureDecompressedVideoOutput setAutomaticallyDropsLateVideoFrames:YES];
  [_captureDecompressedVideoOutput
      setMinimumVideoFrameInterval:(NSTimeInterval)1/(float)_frameRate];

  NSError *error;
  if (![_captureSession addOutput:_captureDecompressedVideoOutput error:&error])
    return NO;

  return YES;
}

- (void)captureOutput:(QTCaptureOutput *)captureOutput
    didDropVideoFrameWithSampleBuffer:(QTSampleBuffer *)sampleBuffer
                       fromConnection:(QTCaptureConnection *)connection {
  // TODO(mflodman) Experiment more when this happens.
}

- (void)captureOutput:(QTCaptureOutput *)captureOutput
  didOutputVideoFrame:(CVImageBufferRef)videoFrame
     withSampleBuffer:(QTSampleBuffer *)sampleBuffer
       fromConnection:(QTCaptureConnection *)connection {

  [_lock lock];
  if (!_owner) {
    [_lock unlock];
    return;
  }

  const int kFlags = 0;
  if (CVPixelBufferLockBaseAddress(videoFrame, kFlags) == kCVReturnSuccess) {
    void *baseAddress = CVPixelBufferGetBaseAddress(videoFrame);
    size_t bytesPerRow = CVPixelBufferGetBytesPerRow(videoFrame);
    size_t frameHeight = CVPixelBufferGetHeight(videoFrame);
    size_t frameSize = bytesPerRow * frameHeight;

    VideoCaptureCapability tempCaptureCapability;
    tempCaptureCapability.width = _frameWidth;
    tempCaptureCapability.height = _frameHeight;
    tempCaptureCapability.maxFPS = _frameRate;
    // TODO(wu) : Update actual type and not hard-coded value.
    tempCaptureCapability.rawType = kVideoBGRA;

    _owner->IncomingFrame((unsigned char*)baseAddress, frameSize,
                          tempCaptureCapability, 0);
    CVPixelBufferUnlockBaseAddress(videoFrame, kFlags);
  }
  [_lock unlock];
  _framesDelivered++;
  _framesRendered++;
}

@end
