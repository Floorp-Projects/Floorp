/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "webrtc/modules/video_capture/ios/device_info_ios_objc.h"
#import "webrtc/modules/video_capture/ios/video_capture_ios_objc.h"

#include "webrtc/system_wrappers/interface/trace.h"

using namespace webrtc;
using namespace webrtc::videocapturemodule;

@interface VideoCaptureIosObjC (hidden)
- (int)changeCaptureInputWithName:(NSString*)captureDeviceName;

@end

@implementation VideoCaptureIosObjC

@synthesize frameRotation = _framRotation;

- (id)initWithOwner:(VideoCaptureIos*)owner captureId:(int)captureId {
  if (self == [super init]) {
    _owner = owner;
    _captureId = captureId;
    _captureSession = [[AVCaptureSession alloc] init];

    if (!_captureSession) {
      return nil;
    }

    // create and configure a new output (using callbacks)
    AVCaptureVideoDataOutput* captureOutput =
        [[AVCaptureVideoDataOutput alloc] init];
    [captureOutput setSampleBufferDelegate:self
        queue:dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0)];
    NSString* key = (NSString*)kCVPixelBufferPixelFormatTypeKey;

    NSNumber* val = [NSNumber
        numberWithUnsignedInt:kCVPixelFormatType_420YpCbCr8BiPlanarFullRange];
    NSDictionary* videoSettings =
        [NSDictionary dictionaryWithObject:val forKey:key];
    captureOutput.videoSettings = videoSettings;

    // add new output
    if ([_captureSession canAddOutput:captureOutput]) {
      [_captureSession addOutput:captureOutput];
    } else {
      WEBRTC_TRACE(kTraceError,
                   kTraceVideoCapture,
                   _captureId,
                   "%s:%s:%d Could not add output to AVCaptureSession ",
                   __FILE__,
                   __FUNCTION__,
                   __LINE__);
    }

    NSNotificationCenter* notify = [NSNotificationCenter defaultCenter];
    [notify addObserver:self
               selector:@selector(onVideoError:)
                   name:AVCaptureSessionRuntimeErrorNotification
                 object:_captureSession];
  }

  return self;
}

- (BOOL)setCaptureDeviceByUniqueId:(NSString*)uniqueId {
  // check to see if the camera is already set
  if (_captureSession) {
    NSArray* currentInputs = [NSArray arrayWithArray:[_captureSession inputs]];
    if ([currentInputs count] > 0) {
      AVCaptureDeviceInput* currentInput = [currentInputs objectAtIndex:0];
      if ([uniqueId isEqualToString:[currentInput.device localizedName]]) {
        return YES;
      }
    }
  }

  return [self changeCaptureInputByUniqueId:uniqueId];
}

- (BOOL)startCaptureWithCapability:(const VideoCaptureCapability&)capability {
  if (!_captureSession) {
    return NO;
  }

  // check limits of the resolution
  if (capability.maxFPS < 0 || capability.maxFPS > 60) {
    return NO;
  }

  if ([_captureSession canSetSessionPreset:AVCaptureSessionPreset1920x1080]) {
    if (capability.width > 1920 || capability.height > 1080) {
      return NO;
    }
  } else if ([_captureSession
                 canSetSessionPreset:AVCaptureSessionPreset1280x720]) {
    if (capability.width > 1280 || capability.height > 720) {
      return NO;
    }
  } else if ([_captureSession
                 canSetSessionPreset:AVCaptureSessionPreset640x480]) {
    if (capability.width > 640 || capability.height > 480) {
      return NO;
    }
  } else if ([_captureSession
                 canSetSessionPreset:AVCaptureSessionPreset352x288]) {
    if (capability.width > 352 || capability.height > 288) {
      return NO;
    }
  } else if (capability.width < 0 || capability.height < 0) {
    return NO;
  }

  _capability = capability;

  NSArray* currentOutputs = [_captureSession outputs];
  if ([currentOutputs count] == 0) {
    return NO;
  }

  NSString* captureQuality =
      [NSString stringWithString:AVCaptureSessionPresetLow];
  if (_capability.width >= 1920 || _capability.height >= 1080) {
    captureQuality =
        [NSString stringWithString:AVCaptureSessionPreset1920x1080];
  } else if (_capability.width >= 1280 || _capability.height >= 720) {
    captureQuality = [NSString stringWithString:AVCaptureSessionPreset1280x720];
  } else if (_capability.width >= 640 || _capability.height >= 480) {
    captureQuality = [NSString stringWithString:AVCaptureSessionPreset640x480];
  } else if (_capability.width >= 352 || _capability.height >= 288) {
    captureQuality = [NSString stringWithString:AVCaptureSessionPreset352x288];
  }

  AVCaptureVideoDataOutput* currentOutput =
      (AVCaptureVideoDataOutput*)[currentOutputs objectAtIndex:0];

  // begin configuration for the AVCaptureSession
  [_captureSession beginConfiguration];

  // picture resolution
  [_captureSession setSessionPreset:captureQuality];

  // take care of capture framerate now
  AVCaptureConnection* connection =
      [currentOutput connectionWithMediaType:AVMediaTypeVideo];

  CMTime cm_time = {1, _capability.maxFPS, kCMTimeFlags_Valid, 0};

  [connection setVideoMinFrameDuration:cm_time];
  [connection setVideoMaxFrameDuration:cm_time];

  // finished configuring, commit settings to AVCaptureSession.
  [_captureSession commitConfiguration];

  [_captureSession startRunning];

  [captureQuality release];

  return YES;
}

- (void)onVideoError {
  // TODO(sjlee): make the specific error handling with this notification.
  WEBRTC_TRACE(kTraceError,
               kTraceVideoCapture,
               _captureId,
               "%s:%s:%d [AVCaptureSession startRunning] error.",
               __FILE__,
               __FUNCTION__,
               __LINE__);
}

- (BOOL)stopCapture {
  if (!_captureSession) {
    return NO;
  }

  [_captureSession stopRunning];

  return YES;
}

- (BOOL)changeCaptureInputByUniqueId:(NSString*)uniqueId {
  NSArray* currentInputs = [_captureSession inputs];
  // remove current input
  if ([currentInputs count] > 0) {
    AVCaptureInput* currentInput =
        (AVCaptureInput*)[currentInputs objectAtIndex:0];

    [_captureSession removeInput:currentInput];
  }

  // Look for input device with the name requested (as our input param)
  // get list of available capture devices
  int captureDeviceCount = [DeviceInfoIosObjC captureDeviceCount];
  if (captureDeviceCount <= 0) {
    return NO;
  }

  AVCaptureDevice* captureDevice =
      [DeviceInfoIosObjC captureDeviceForUniqueId:uniqueId];

  if (!captureDevice) {
    return NO;
  }

  // now create capture session input out of AVCaptureDevice
  NSError* deviceError = nil;
  AVCaptureDeviceInput* newCaptureInput =
      [AVCaptureDeviceInput deviceInputWithDevice:captureDevice
                                            error:&deviceError];

  if (!newCaptureInput) {
    const char* errorMessage = [[deviceError localizedDescription] UTF8String];

    WEBRTC_TRACE(kTraceError,
                 kTraceVideoCapture,
                 _captureId,
                 "%s:%s:%d deviceInputWithDevice error:%s",
                 __FILE__,
                 __FUNCTION__,
                 __LINE__,
                 errorMessage);

    return NO;
  }

  // try to add our new capture device to the capture session
  [_captureSession beginConfiguration];

  BOOL addedCaptureInput = NO;
  if ([_captureSession canAddInput:newCaptureInput]) {
    [_captureSession addInput:newCaptureInput];
    addedCaptureInput = YES;
  } else {
    addedCaptureInput = NO;
  }

  [_captureSession commitConfiguration];

  return addedCaptureInput;
}

- (void)captureOutput:(AVCaptureOutput*)captureOutput
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
           fromConnection:(AVCaptureConnection*)connection {
  const int kFlags = 0;
  CVImageBufferRef videoFrame = CMSampleBufferGetImageBuffer(sampleBuffer);

  if (CVPixelBufferLockBaseAddress(videoFrame, kFlags) != kCVReturnSuccess) {
    return;
  }

  const int kYPlaneIndex = 0;
  const int kUVPlaneIndex = 1;

  uint8_t* baseAddress =
      (uint8_t*)CVPixelBufferGetBaseAddressOfPlane(videoFrame, kYPlaneIndex);
  int yPlaneBytesPerRow =
      CVPixelBufferGetBytesPerRowOfPlane(videoFrame, kYPlaneIndex);
  int yPlaneHeight = CVPixelBufferGetHeightOfPlane(videoFrame, kYPlaneIndex);
  int uvPlaneBytesPerRow =
      CVPixelBufferGetBytesPerRowOfPlane(videoFrame, kUVPlaneIndex);
  int uvPlaneHeight = CVPixelBufferGetHeightOfPlane(videoFrame, kUVPlaneIndex);
  int frameSize =
      yPlaneBytesPerRow * yPlaneHeight + uvPlaneBytesPerRow * uvPlaneHeight;

  VideoCaptureCapability tempCaptureCapability;
  tempCaptureCapability.width = CVPixelBufferGetWidth(videoFrame);
  tempCaptureCapability.height = CVPixelBufferGetHeight(videoFrame);
  tempCaptureCapability.maxFPS = _capability.maxFPS;
  tempCaptureCapability.rawType = kVideoNV12;

  _owner->IncomingFrame(baseAddress, frameSize, tempCaptureCapability, 0);

  CVPixelBufferUnlockBaseAddress(videoFrame, kFlags);
}

@end
