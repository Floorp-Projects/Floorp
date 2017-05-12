/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
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

#import "webrtc/modules/video_capture/mac/avfoundation/video_capture_avfoundation_objc.h"

#include "webrtc/system_wrappers/include/trace.h"

using namespace webrtc;
using namespace videocapturemodule;

@implementation VideoCaptureMacAVFoundationObjC

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

  [_captureVideoDataOutput release];

  if (_videoDataOutputQueue)
    dispatch_release(_videoDataOutputQueue);

  [_captureSession release];
  [_captureDevices release];
  [_lock release];

  [super dealloc];
}

#pragma mark Public methods

- (void)registerOwner:(VideoCaptureMacAVFoundation*)owner {
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

  for(int index = 0; index < _captureDeviceCount; index++) {
    _captureDevice = (AVCaptureDevice*)[_captureDevices objectAtIndex:index];
    char captureDeviceId[1024] = "";
    [[_captureDevice uniqueID] getCString:captureDeviceId
                               maxLength:1024
                                encoding:NSUTF8StringEncoding];
    if (strcmp(uniqueId, captureDeviceId) == 0) {
      WEBRTC_TRACE(kTraceInfo, kTraceVideoCapture, 0,
                   "%s:%d Found capture device id %s as index %d",
                   __FUNCTION__, __LINE__, captureDeviceId, index);
      [[_captureDevice localizedName] getCString:_captureDeviceNameUTF8
                                             maxLength:1024
                                              encoding:NSUTF8StringEncoding];
      [[_captureDevice uniqueID] getCString:_captureDeviceNameUniqueID
                                 maxLength:1024
                                  encoding:NSUTF8StringEncoding];
      break;
    }
    _captureDevice = nil;
  }

  if (!_captureDevice)
    return NO;

  NSError* error;
  AVCaptureDeviceInput *deviceInput =
    [AVCaptureDeviceInput deviceInputWithDevice:_captureDevice error:&error];

  if (deviceInput &&
    [_captureSession canAddInput:deviceInput]) {
    [_captureSession addInput:deviceInput];
  } else {
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

- (void)setCaptureHeight:(int)height width:(int)width frameRate:(int)frameRate rawType:(webrtc::RawVideoType*)rawType {
  _frameWidth = width;
  _frameHeight = height;
  _frameRate = frameRate;
  _rawType = *rawType;

  AVCaptureDeviceFormat *bestFormat = nil;
  AVFrameRateRange *bestFrameRateRange = nil;

  for (AVCaptureDeviceFormat* format in [_captureDevice formats]) {
    CMVideoDimensions videoDimensions = CMVideoFormatDescriptionGetDimensions(format.formatDescription);
    if (videoDimensions.width == _frameWidth &&
        videoDimensions.height == _frameHeight &&
        [VideoCaptureMacAVFoundationUtility fourCCToRawVideoType:CMFormatDescriptionGetMediaSubType(format.formatDescription)] == _rawType)
    {
      bestFormat = format;
      for (AVFrameRateRange* range in format.videoSupportedFrameRateRanges) {
        if ( range.maxFrameRate <= _frameRate && range.maxFrameRate > bestFrameRateRange.maxFrameRate) {
          bestFrameRateRange = range;
        }
      }
      break;
    }
  }

  if (bestFormat && bestFrameRateRange) {
    NSDictionary* newSettings = [NSDictionary dictionaryWithObjectsAndKeys:
      [NSNumber numberWithDouble:_frameWidth], (id)kCVPixelBufferWidthKey,
      [NSNumber numberWithDouble:_frameHeight], (id)kCVPixelBufferHeightKey,
      [NSNumber numberWithUnsignedInt:CMFormatDescriptionGetMediaSubType(bestFormat.formatDescription)], (id)kCVPixelBufferPixelFormatTypeKey,
      nil];

    _captureVideoDataOutput.videoSettings = newSettings;

    AVCaptureConnection* captureConnection =
      [_captureVideoDataOutput connectionWithMediaType:AVMediaTypeVideo];

    if ([captureConnection isVideoMinFrameDurationSupported]) {
      [captureConnection setVideoMinFrameDuration:bestFrameRateRange.minFrameDuration];
    }

    if ([captureConnection isVideoMaxFrameDurationSupported]) {
      [captureConnection setVideoMaxFrameDuration:bestFrameRateRange.maxFrameDuration];
    }
  }
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

  [_captureSession performSelectorOnMainThread:@selector(stopRunning)
                   withObject:nil
                   waitUntilDone:NO];
  _capturing = NO;
}

#pragma mark Private methods

- (BOOL)initializeVariables {
  if (NSClassFromString(@"AVCaptureSession") == nil)
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
  _captureSession = [[AVCaptureSession alloc] init];

  _captureVideoDataOutput = [AVCaptureVideoDataOutput new];

  [_captureVideoDataOutput setAlwaysDiscardsLateVideoFrames:YES];

  _videoDataOutputQueue = dispatch_queue_create("VideoDataOutputQueue", DISPATCH_QUEUE_SERIAL);
  [_captureVideoDataOutput setSampleBufferDelegate:self queue:_videoDataOutputQueue];

  [self getCaptureDevices];
  if (![self initializeVideoCapture])
    return NO;

  return NO;
}

- (void)getCaptureDevices {
  if (_captureDevices)
    [_captureDevices release];

  _captureDevices = [[NSArray alloc] initWithArray:
      [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo]];

  _captureDeviceCount = _captureDevices.count;
}

- (BOOL)initializeVideoCapture{

  if ([_captureSession canAddOutput:_captureVideoDataOutput]) {
    [_captureSession addOutput:_captureVideoDataOutput];
    return YES;
  } else {
    return NO;
  }
}

- (void)captureOutput:(AVCaptureOutput *)captureOutput
  didDropSampleBuffer:(CMSampleBufferRef)sampleBuffer
       fromConnection:(AVCaptureConnection *)connection {
  // TODO(mflodman) Experiment more when this happens.
}

- (void)captureOutput:(AVCaptureOutput *)captureOutput
  didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
         fromConnection:(AVCaptureConnection *)connection {

  [_lock lock];
  if (!_owner) {
    [_lock unlock];
    return;
  }

  CMFormatDescriptionRef formatDescription =
      CMSampleBufferGetFormatDescription(sampleBuffer);
  webrtc::RawVideoType rawType =
      [VideoCaptureMacAVFoundationUtility fourCCToRawVideoType:CMFormatDescriptionGetMediaSubType(formatDescription)];
  CMVideoDimensions dimensions =
      CMVideoFormatDescriptionGetDimensions(formatDescription);

  VideoCaptureCapability tempCaptureCapability;
  tempCaptureCapability.width = dimensions.width;
  tempCaptureCapability.height = dimensions.height;
  tempCaptureCapability.maxFPS = _frameRate;
  tempCaptureCapability.rawType = rawType;

  CMBlockBufferRef blockBuffer = CMSampleBufferGetDataBuffer(sampleBuffer);

  if (blockBuffer) {
    char* baseAddress;
    size_t frameSize;
    size_t lengthAtOffset;
    CMBlockBufferGetDataPointer(blockBuffer, 0, &lengthAtOffset, &frameSize, &baseAddress);

    NSAssert(lengthAtOffset == frameSize, @"lengthAtOffset != frameSize)");

    _owner->IncomingFrame((unsigned char*)baseAddress, frameSize,
                            tempCaptureCapability, 0);
  } else {
    // Get a CMSampleBuffer's Core Video image buffer for the media data
    CVImageBufferRef videoFrame = CMSampleBufferGetImageBuffer(sampleBuffer);

    if (CVPixelBufferLockBaseAddress(videoFrame, kCVPixelBufferLock_ReadOnly) == kCVReturnSuccess) {
      void* baseAddress = CVPixelBufferGetBaseAddress(videoFrame);
      size_t bytesPerRow = CVPixelBufferGetBytesPerRow(videoFrame);
      size_t frameHeight = CVPixelBufferGetHeight(videoFrame);
      size_t frameSize = bytesPerRow * frameHeight;

      _owner->IncomingFrame((unsigned char*)baseAddress, frameSize,
                            tempCaptureCapability, 0);
      CVPixelBufferUnlockBaseAddress(videoFrame, kCVPixelBufferLock_ReadOnly);
    }
  }
  [_lock unlock];
  _framesDelivered++;
  _framesRendered++;
}

@end
