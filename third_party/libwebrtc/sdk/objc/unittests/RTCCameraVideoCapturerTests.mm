/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <OCMock/OCMock.h>
#import <XCTest/XCTest.h>

#if TARGET_OS_IPHONE
#import <UIKit/UIKit.h>
#endif

#import "base/RTCVideoFrame.h"
#import "components/capturer/RTCCameraVideoCapturer.h"
#import "helpers/AVCaptureSession+DevicePosition.h"
#import "helpers/RTCDispatcher.h"
#import "helpers/scoped_cftyperef.h"

#define WAIT(timeoutMs)                                                        \
  do {                                                                         \
    id expectation = [[XCTestExpectation alloc] initWithDescription:@"Dummy"]; \
    XCTWaiterResult res = [XCTWaiter waitForExpectations:@[ expectation ]      \
                                                 timeout:timeoutMs / 1000.0];  \
    XCTAssertEqual(XCTWaiterResultTimedOut, res);                              \
  } while (false);

#if TARGET_OS_IPHONE
// Helper method.
CMSampleBufferRef createTestSampleBufferRef() {

  // This image is already in the testing bundle.
  UIImage *image = [UIImage imageNamed:@"Default.png"];
  CGSize size = image.size;
  CGImageRef imageRef = [image CGImage];

  CVPixelBufferRef pixelBuffer = nullptr;
  CVPixelBufferCreate(kCFAllocatorDefault, size.width, size.height, kCVPixelFormatType_32ARGB, nil,
                      &pixelBuffer);

  CGColorSpaceRef rgbColorSpace = CGColorSpaceCreateDeviceRGB();
  // We don't care about bitsPerComponent and bytesPerRow so arbitrary value of 8 for both.
  CGContextRef context = CGBitmapContextCreate(nil, size.width, size.height, 8, 8 * size.width,
                                               rgbColorSpace, kCGImageAlphaPremultipliedFirst);

  CGContextDrawImage(
      context, CGRectMake(0, 0, CGImageGetWidth(imageRef), CGImageGetHeight(imageRef)), imageRef);

  CGColorSpaceRelease(rgbColorSpace);
  CGContextRelease(context);

  // We don't really care about the timing.
  CMSampleTimingInfo timing = {kCMTimeInvalid, kCMTimeInvalid, kCMTimeInvalid};
  CMVideoFormatDescriptionRef description = nullptr;
  CMVideoFormatDescriptionCreateForImageBuffer(NULL, pixelBuffer, &description);

  CMSampleBufferRef sampleBuffer = nullptr;
  CMSampleBufferCreateForImageBuffer(kCFAllocatorDefault, pixelBuffer, YES, NULL, NULL, description,
                                     &timing, &sampleBuffer);
  CFRelease(pixelBuffer);

  return sampleBuffer;

}
#endif
@interface RTC_OBJC_TYPE (RTCCameraVideoCapturer)
(Tests)<AVCaptureVideoDataOutputSampleBufferDelegate> -
    (instancetype)initWithDelegate
    : (__weak id<RTC_OBJC_TYPE(RTCVideoCapturerDelegate)>)delegate captureSession
    : (AVCaptureSession *)captureSession;
@end

@interface RTCCameraVideoCapturerTests : XCTestCase
@property(nonatomic, strong) id delegateMock;
@property(nonatomic, strong) id deviceMock;
@property(nonatomic, strong) id captureConnectionMock;
@property(nonatomic, strong) RTC_OBJC_TYPE(RTCCameraVideoCapturer) * capturer;
@end

@implementation RTCCameraVideoCapturerTests
@synthesize delegateMock = _delegateMock;
@synthesize deviceMock = _deviceMock;
@synthesize captureConnectionMock = _captureConnectionMock;
@synthesize capturer = _capturer;

- (void)setUp {
  self.delegateMock = OCMProtocolMock(@protocol(RTC_OBJC_TYPE(RTCVideoCapturerDelegate)));
  self.captureConnectionMock = OCMClassMock([AVCaptureConnection class]);
  self.capturer =
      [[RTC_OBJC_TYPE(RTCCameraVideoCapturer) alloc] initWithDelegate:self.delegateMock];
  self.deviceMock = [RTCCameraVideoCapturerTests createDeviceMock];
}

- (void)tearDown {
  [self.delegateMock stopMocking];
  [self.deviceMock stopMocking];
  self.delegateMock = nil;
  self.deviceMock = nil;
  self.capturer = nil;
}

#pragma mark - utils

+ (id)createDeviceMock {
  return OCMClassMock([AVCaptureDevice class]);
}

#pragma mark - test cases

- (void)testSetupSession {
  AVCaptureSession *session = self.capturer.captureSession;
  XCTAssertTrue(session != nil);

#if TARGET_OS_IPHONE
  XCTAssertEqual(session.sessionPreset, AVCaptureSessionPresetInputPriority);
  XCTAssertEqual(session.usesApplicationAudioSession, NO);
#endif
  XCTAssertEqual(session.outputs.count, 1u);
}

- (void)testSetupSessionOutput {
  AVCaptureVideoDataOutput *videoOutput = self.capturer.captureSession.outputs[0];
  XCTAssertEqual(videoOutput.alwaysDiscardsLateVideoFrames, NO);
  XCTAssertEqual(videoOutput.sampleBufferDelegate, self.capturer);
}

- (void)testSupportedFormatsForDevice {
  // given
  id validFormat1 = OCMClassMock([AVCaptureDeviceFormat class]);
  CMVideoFormatDescriptionRef format;

  // We don't care about width and heigth so arbitrary 123 and 456 values.
  int width = 123;
  int height = 456;
  CMVideoFormatDescriptionCreate(nil, kCVPixelFormatType_420YpCbCr8PlanarFullRange, width, height,
                                 nil, &format);
  OCMStub([validFormat1 formatDescription]).andReturn(format);

  id validFormat2 = OCMClassMock([AVCaptureDeviceFormat class]);
  CMVideoFormatDescriptionCreate(nil, kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange, width,
                                 height, nil, &format);
  OCMStub([validFormat2 formatDescription]).andReturn(format);

  id invalidFormat = OCMClassMock([AVCaptureDeviceFormat class]);
  CMVideoFormatDescriptionCreate(nil, kCVPixelFormatType_422YpCbCr8_yuvs, width, height, nil,
                                 &format);
  OCMStub([invalidFormat formatDescription]).andReturn(format);

  NSArray *formats = @[ validFormat1, validFormat2, invalidFormat ];
  OCMStub([self.deviceMock formats]).andReturn(formats);

  // when
  NSArray *supportedFormats =
      [RTC_OBJC_TYPE(RTCCameraVideoCapturer) supportedFormatsForDevice:self.deviceMock];

  // then
  XCTAssertEqual(supportedFormats.count, 3u);
  XCTAssertTrue([supportedFormats containsObject:validFormat1]);
  XCTAssertTrue([supportedFormats containsObject:validFormat2]);
  XCTAssertTrue([supportedFormats containsObject:invalidFormat]);

  // cleanup
  [validFormat1 stopMocking];
  [validFormat2 stopMocking];
  [invalidFormat stopMocking];
  validFormat1 = nil;
  validFormat2 = nil;
  invalidFormat = nil;
}

- (void)testDelegateCallbackNotCalledWhenInvalidBuffer {
  // given
  CMSampleBufferRef sampleBuffer = nullptr;
  [[self.delegateMock reject] capturer:[OCMArg any] didCaptureVideoFrame:[OCMArg any]];

  // when
  [self.capturer captureOutput:self.capturer.captureSession.outputs[0]
         didOutputSampleBuffer:sampleBuffer
                fromConnection:self.captureConnectionMock];

  // then
  [self.delegateMock verify];
}

- (void)testDelegateCallbackWithValidBufferAndOrientationUpdate {
#if TARGET_OS_IPHONE
  XCTExpectFailure(@"Setting orientation on UIDevice is not supported");
  [UIDevice.currentDevice setValue:@(UIDeviceOrientationPortraitUpsideDown) forKey:@"orientation"];
  CMSampleBufferRef sampleBuffer = createTestSampleBufferRef();

  // then
  [[self.delegateMock expect] capturer:self.capturer
                  didCaptureVideoFrame:[OCMArg checkWithBlock:^BOOL(RTC_OBJC_TYPE(RTCVideoFrame) *
                                                                    expectedFrame) {
                    XCTAssertEqual(expectedFrame.rotation, RTCVideoRotation_270);
                    return YES;
                  }]];

  // when
  NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
  [center postNotificationName:UIDeviceOrientationDidChangeNotification object:nil];

  // We need to wait for the dispatch to finish.
  WAIT(1000);

  [self.capturer captureOutput:self.capturer.captureSession.outputs[0]
         didOutputSampleBuffer:sampleBuffer
                fromConnection:self.captureConnectionMock];

  [self.delegateMock verify];
  CFRelease(sampleBuffer);
#endif
}

// The XCTest framework considers functions that don't take arguments tests. This is a helper.
- (void)testRotationCamera:(AVCaptureDevicePosition)camera
           withOrientation:(UIDeviceOrientation)deviceOrientation {
#if TARGET_OS_IPHONE
  // Mock the AVCaptureConnection as we will get the camera position from the connection's
  // input ports.
  AVCaptureDeviceInput *inputPortMock = OCMClassMock([AVCaptureDeviceInput class]);
  AVCaptureInputPort *captureInputPort = OCMClassMock([AVCaptureInputPort class]);
  NSArray *inputPortsArrayMock = @[captureInputPort];
  AVCaptureDevice *captureDeviceMock = OCMClassMock([AVCaptureDevice class]);
  OCMStub(((AVCaptureConnection *)self.captureConnectionMock).inputPorts).
      andReturn(inputPortsArrayMock);
  OCMStub(captureInputPort.input).andReturn(inputPortMock);
  OCMStub(inputPortMock.device).andReturn(captureDeviceMock);
  OCMStub(captureDeviceMock.position).andReturn(camera);

  XCTExpectFailure(@"Setting orientation on UIDevice is not supported");
  [UIDevice.currentDevice setValue:@(deviceOrientation) forKey:@"orientation"];

  CMSampleBufferRef sampleBuffer = createTestSampleBufferRef();

  [[self.delegateMock expect] capturer:self.capturer
                  didCaptureVideoFrame:[OCMArg checkWithBlock:^BOOL(RTC_OBJC_TYPE(RTCVideoFrame) *
                                                                    expectedFrame) {
                    if (camera == AVCaptureDevicePositionFront) {
                      if (deviceOrientation == UIDeviceOrientationLandscapeLeft) {
                        XCTAssertEqual(expectedFrame.rotation, RTCVideoRotation_180);
                      } else if (deviceOrientation == UIDeviceOrientationLandscapeRight) {
                        XCTAssertEqual(expectedFrame.rotation, RTCVideoRotation_0);
                      }
                    } else if (camera == AVCaptureDevicePositionBack) {
                      if (deviceOrientation == UIDeviceOrientationLandscapeLeft) {
                        XCTAssertEqual(expectedFrame.rotation, RTCVideoRotation_0);
                      } else if (deviceOrientation == UIDeviceOrientationLandscapeRight) {
                        XCTAssertEqual(expectedFrame.rotation, RTCVideoRotation_180);
                      }
                    }
                    return YES;
                  }]];

  NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
  [center postNotificationName:UIDeviceOrientationDidChangeNotification object:nil];

  // We need to wait for the dispatch to finish.
  WAIT(1000);

  [self.capturer captureOutput:self.capturer.captureSession.outputs[0]
         didOutputSampleBuffer:sampleBuffer
                fromConnection:self.captureConnectionMock];

  [self.delegateMock verify];

  CFRelease(sampleBuffer);
#endif
}

- (void)testRotationCameraBackLandscapeLeft {
  [self testRotationCamera:AVCaptureDevicePositionBack
           withOrientation:UIDeviceOrientationLandscapeLeft];
}

- (void)testRotationCameraFrontLandscapeLeft {
  [self testRotationCamera:AVCaptureDevicePositionFront
           withOrientation:UIDeviceOrientationLandscapeLeft];
}

- (void)testRotationCameraBackLandscapeRight {
  [self testRotationCamera:AVCaptureDevicePositionBack
           withOrientation:UIDeviceOrientationLandscapeRight];
}

- (void)testRotationCameraFrontLandscapeRight {
  [self testRotationCamera:AVCaptureDevicePositionFront
           withOrientation:UIDeviceOrientationLandscapeRight];
}

- (void)setExif:(CMSampleBufferRef)sampleBuffer {
  rtc::ScopedCFTypeRef<CFMutableDictionaryRef> exif(CFDictionaryCreateMutable(
      kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
  CFDictionarySetValue(exif.get(), CFSTR("LensModel"), CFSTR("iPhone SE back camera 4.15mm f/2.2"));
  CMSetAttachment(sampleBuffer, CFSTR("{Exif}"), exif.get(), kCMAttachmentMode_ShouldPropagate);
}

- (void)testRotationFrame {
#if TARGET_OS_IPHONE
  // Mock the AVCaptureConnection as we will get the camera position from the connection's
  // input ports.
  AVCaptureDeviceInput *inputPortMock = OCMClassMock([AVCaptureDeviceInput class]);
  AVCaptureInputPort *captureInputPort = OCMClassMock([AVCaptureInputPort class]);
  NSArray *inputPortsArrayMock = @[captureInputPort];
  AVCaptureDevice *captureDeviceMock = OCMClassMock([AVCaptureDevice class]);
  OCMStub(((AVCaptureConnection *)self.captureConnectionMock).inputPorts).
      andReturn(inputPortsArrayMock);
  OCMStub(captureInputPort.input).andReturn(inputPortMock);
  OCMStub(inputPortMock.device).andReturn(captureDeviceMock);
  OCMStub(captureDeviceMock.position).andReturn(AVCaptureDevicePositionFront);

  XCTExpectFailure(@"Setting orientation on UIDevice is not supported");
  [UIDevice.currentDevice setValue:@(UIDeviceOrientationLandscapeLeft) forKey:@"orientation"];

  CMSampleBufferRef sampleBuffer = createTestSampleBufferRef();

  [[self.delegateMock expect] capturer:self.capturer
                  didCaptureVideoFrame:[OCMArg checkWithBlock:^BOOL(RTC_OBJC_TYPE(RTCVideoFrame) *
                                                                    expectedFrame) {
                    // Front camera and landscape left should return 180. But the frame's exif
                    // we add below says its from the back camera, so rotation should be 0.
                    XCTAssertEqual(expectedFrame.rotation, RTCVideoRotation_0);
                    return YES;
                  }]];

  NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
  [center postNotificationName:UIDeviceOrientationDidChangeNotification object:nil];

  // We need to wait for the dispatch to finish.
  WAIT(1000);

  [self setExif:sampleBuffer];

  [self.capturer captureOutput:self.capturer.captureSession.outputs[0]
         didOutputSampleBuffer:sampleBuffer
                fromConnection:self.captureConnectionMock];

  [self.delegateMock verify];
  CFRelease(sampleBuffer);
#endif
}

- (void)testImageExif {
#if TARGET_OS_IPHONE
  CMSampleBufferRef sampleBuffer = createTestSampleBufferRef();
  [self setExif:sampleBuffer];

  AVCaptureDevicePosition cameraPosition = [AVCaptureSession
                                            devicePositionForSampleBuffer:sampleBuffer];
  XCTAssertEqual(cameraPosition, AVCaptureDevicePositionBack);
#endif
}

@end

@interface RTCCameraVideoCapturerTestsWithMockedCaptureSession : XCTestCase
@property(nonatomic, strong) id delegateMock;
@property(nonatomic, strong) id deviceMock;
@property(nonatomic, strong) id captureSessionMock;
@property(nonatomic, strong) RTC_OBJC_TYPE(RTCCameraVideoCapturer) * capturer;
@end

@implementation RTCCameraVideoCapturerTestsWithMockedCaptureSession
@synthesize delegateMock = _delegateMock;
@synthesize deviceMock = _deviceMock;
@synthesize captureSessionMock = _captureSessionMock;
@synthesize capturer = _capturer;

- (void)setUp {
  self.captureSessionMock = OCMStrictClassMock([AVCaptureSession class]);
  OCMStub([self.captureSessionMock setSessionPreset:[OCMArg any]]);
  OCMStub([self.captureSessionMock setUsesApplicationAudioSession:NO]);
  OCMStub([self.captureSessionMock canAddOutput:[OCMArg any]]).andReturn(YES);
  OCMStub([self.captureSessionMock addOutput:[OCMArg any]]);
  OCMStub([self.captureSessionMock beginConfiguration]);
  OCMStub([self.captureSessionMock commitConfiguration]);
  self.delegateMock = OCMProtocolMock(@protocol(RTC_OBJC_TYPE(RTCVideoCapturerDelegate)));
  self.capturer =
      [[RTC_OBJC_TYPE(RTCCameraVideoCapturer) alloc] initWithDelegate:self.delegateMock
                                                       captureSession:self.captureSessionMock];
  self.deviceMock = [RTCCameraVideoCapturerTests createDeviceMock];
}

- (void)tearDown {
  [self.delegateMock stopMocking];
  [self.deviceMock stopMocking];
  self.delegateMock = nil;
  self.deviceMock = nil;
  self.capturer = nil;
  self.captureSessionMock = nil;
}

#pragma mark - test cases

- (void)testStartingAndStoppingCapture {
  id expectedDeviceInputMock = OCMClassMock([AVCaptureDeviceInput class]);
  id captureDeviceInputMock = OCMClassMock([AVCaptureDeviceInput class]);
  OCMStub([captureDeviceInputMock deviceInputWithDevice:self.deviceMock error:[OCMArg setTo:nil]])
      .andReturn(expectedDeviceInputMock);

  OCMStub([self.deviceMock lockForConfiguration:[OCMArg setTo:nil]]).andReturn(YES);
  OCMStub([self.deviceMock unlockForConfiguration]);
  OCMStub([_captureSessionMock canAddInput:expectedDeviceInputMock]).andReturn(YES);
  OCMStub([_captureSessionMock inputs]).andReturn(@[ expectedDeviceInputMock ]);
  OCMStub([_captureSessionMock removeInput:expectedDeviceInputMock]);

  // Set expectation that the capture session should be started with correct device.
  OCMExpect([_captureSessionMock addInput:expectedDeviceInputMock]);
  OCMExpect([_captureSessionMock startRunning]);
  OCMExpect([_captureSessionMock stopRunning]);

  id format = OCMClassMock([AVCaptureDeviceFormat class]);
  [self.capturer startCaptureWithDevice:self.deviceMock format:format fps:30];
  [self.capturer stopCapture];

  // Start capture code is dispatched async.
  OCMVerifyAllWithDelay(_captureSessionMock, 15);
}

- (void)testStartCaptureFailingToLockForConfiguration {
  // The captureSessionMock is a strict mock, so this test will crash if the startCapture
  // method does not return when failing to lock for configuration.
  OCMExpect([self.deviceMock lockForConfiguration:[OCMArg setTo:nil]]).andReturn(NO);

  id format = OCMClassMock([AVCaptureDeviceFormat class]);
  [self.capturer startCaptureWithDevice:self.deviceMock format:format fps:30];

  // Start capture code is dispatched async.
  OCMVerifyAllWithDelay(self.deviceMock, 15);
}

- (void)testStartingAndStoppingCaptureWithCallbacks {
  id expectedDeviceInputMock = OCMClassMock([AVCaptureDeviceInput class]);
  id captureDeviceInputMock = OCMClassMock([AVCaptureDeviceInput class]);
  OCMStub([captureDeviceInputMock deviceInputWithDevice:self.deviceMock error:[OCMArg setTo:nil]])
      .andReturn(expectedDeviceInputMock);

  OCMStub([self.deviceMock lockForConfiguration:[OCMArg setTo:nil]]).andReturn(YES);
  OCMStub([self.deviceMock unlockForConfiguration]);
  OCMStub([_captureSessionMock canAddInput:expectedDeviceInputMock]).andReturn(YES);
  OCMStub([_captureSessionMock inputs]).andReturn(@[ expectedDeviceInputMock ]);
  OCMStub([_captureSessionMock removeInput:expectedDeviceInputMock]);

  // Set expectation that the capture session should be started with correct device.
  OCMExpect([_captureSessionMock addInput:expectedDeviceInputMock]);
  OCMExpect([_captureSessionMock startRunning]);
  OCMExpect([_captureSessionMock stopRunning]);

  dispatch_semaphore_t completedStopSemaphore = dispatch_semaphore_create(0);

  __block BOOL completedStart = NO;
  id format = OCMClassMock([AVCaptureDeviceFormat class]);
  [self.capturer startCaptureWithDevice:self.deviceMock
                                 format:format
                                    fps:30
                      completionHandler:^(NSError *error) {
                        XCTAssertEqual(error, nil);
                        completedStart = YES;
                      }];

  __block BOOL completedStop = NO;
  [self.capturer stopCaptureWithCompletionHandler:^{
    completedStop = YES;
    dispatch_semaphore_signal(completedStopSemaphore);
  }];

  dispatch_semaphore_wait(completedStopSemaphore,
                          dispatch_time(DISPATCH_TIME_NOW, 15.0 * NSEC_PER_SEC));
  OCMVerifyAllWithDelay(_captureSessionMock, 15);
  XCTAssertTrue(completedStart);
  XCTAssertTrue(completedStop);
}

- (void)testStartCaptureFailingToLockForConfigurationWithCallback {
  id expectedDeviceInputMock = OCMClassMock([AVCaptureDeviceInput class]);
  id captureDeviceInputMock = OCMClassMock([AVCaptureDeviceInput class]);
  OCMStub([captureDeviceInputMock deviceInputWithDevice:self.deviceMock error:[OCMArg setTo:nil]])
      .andReturn(expectedDeviceInputMock);

  id errorMock = OCMClassMock([NSError class]);

  OCMStub([self.deviceMock lockForConfiguration:[OCMArg setTo:errorMock]]).andReturn(NO);
  OCMStub([_captureSessionMock canAddInput:expectedDeviceInputMock]).andReturn(YES);
  OCMStub([self.deviceMock unlockForConfiguration]);

  OCMExpect([_captureSessionMock addInput:expectedDeviceInputMock]);

  dispatch_semaphore_t completedStartSemaphore = dispatch_semaphore_create(0);
  __block NSError *callbackError = nil;

  id format = OCMClassMock([AVCaptureDeviceFormat class]);
  [self.capturer startCaptureWithDevice:self.deviceMock
                                 format:format
                                    fps:30
                      completionHandler:^(NSError *error) {
                        callbackError = error;
                        dispatch_semaphore_signal(completedStartSemaphore);
                      }];

  long ret = dispatch_semaphore_wait(completedStartSemaphore,
                                     dispatch_time(DISPATCH_TIME_NOW, 15.0 * NSEC_PER_SEC));
  XCTAssertEqual(ret, 0);
  XCTAssertEqual(callbackError, errorMock);
}

// TODO(crbug.com/webrtc/14829): Test is disabled on iOS < 16 and broken on iOS 16.
- (void)DISABLED_testStartCaptureSetsOutputDimensionsInvalidPixelFormat {
  id expectedDeviceInputMock = OCMClassMock([AVCaptureDeviceInput class]);
  id captureDeviceInputMock = OCMClassMock([AVCaptureDeviceInput class]);
  OCMStub([captureDeviceInputMock deviceInputWithDevice:_deviceMock error:[OCMArg setTo:nil]])
      .andReturn(expectedDeviceInputMock);

  OCMStub([_deviceMock lockForConfiguration:[OCMArg setTo:nil]]).andReturn(YES);
  OCMStub([_deviceMock unlockForConfiguration]);
  OCMStub([_captureSessionMock canAddInput:expectedDeviceInputMock]).andReturn(YES);
  OCMStub([_captureSessionMock addInput:expectedDeviceInputMock]);
  OCMStub([_captureSessionMock inputs]).andReturn(@[ expectedDeviceInputMock ]);
  OCMStub([_captureSessionMock removeInput:expectedDeviceInputMock]);
  OCMStub([_captureSessionMock startRunning]);
  OCMStub([_captureSessionMock stopRunning]);

  id deviceFormatMock = OCMClassMock([AVCaptureDeviceFormat class]);
  CMVideoFormatDescriptionRef formatDescription;

  int width = 110;
  int height = 220;
  FourCharCode pixelFormat = 0x18000000;
  CMVideoFormatDescriptionCreate(nil, pixelFormat, width, height, nil, &formatDescription);
  OCMStub([deviceFormatMock formatDescription]).andReturn(formatDescription);

  [_capturer startCaptureWithDevice:_deviceMock format:deviceFormatMock fps:30];

  XCTestExpectation *expectation = [self expectationWithDescription:@"StopCompletion"];
  [_capturer stopCaptureWithCompletionHandler:^(void) {
    [expectation fulfill];
  }];

  [self waitForExpectationsWithTimeout:15 handler:nil];

  OCMVerify([_captureSessionMock
      addOutput:[OCMArg checkWithBlock:^BOOL(AVCaptureVideoDataOutput *output) {
        if (@available(iOS 16, *)) {
          XCTAssertEqual(width, [output.videoSettings[(id)kCVPixelBufferWidthKey] intValue]);
          XCTAssertEqual(height, [output.videoSettings[(id)kCVPixelBufferHeightKey] intValue]);
        } else {
          XCTAssertEqual(0, [output.videoSettings[(id)kCVPixelBufferWidthKey] intValue]);
          XCTAssertEqual(0, [output.videoSettings[(id)kCVPixelBufferHeightKey] intValue]);
        }
        XCTAssertEqual(
            (FourCharCode)kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange,
            [output.videoSettings[(id)kCVPixelBufferPixelFormatTypeKey] unsignedIntValue]);
        return YES;
      }]]);
}

@end
