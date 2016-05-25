/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

//
//  video_capture_avfoundation_objc.h
//
//

#ifndef WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_MAC_AVFOUNDATION_VIDEO_CAPTURE_AVFOUNDATION_OBJC_H_
#define WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_MAC_AVFOUNDATION_VIDEO_CAPTURE_AVFOUNDATION_OBJC_H_

#import <AppKit/AppKit.h>
#import <CoreData/CoreData.h>
#import <CoreFoundation/CoreFoundation.h>
#import <CoreVideo/CoreVideo.h>
#import <Foundation/Foundation.h>

#include "webrtc/modules/video_capture/mac/avfoundation/video_capture_avfoundation.h"

@interface VideoCaptureMacAVFoundationObjC : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate> {
  bool _capturing;
  int _frameRate;
  int _frameWidth;
  int _frameHeight;
  webrtc::RawVideoType _rawType;
  int _framesDelivered;
  int _framesRendered;
  bool _captureInitialized;

  webrtc::videocapturemodule::VideoCaptureMacAVFoundation* _owner;
  NSLock* _lock;

  AVCaptureDevice* _captureDevice;
  AVCaptureSession* _captureSession;
  AVCaptureVideoDataOutput* _captureVideoDataOutput;
  dispatch_queue_t _videoDataOutputQueue;
  NSArray* _captureDevices;
  int _captureDeviceCount;
  char _captureDeviceNameUTF8[1024];
  char _captureDeviceNameUniqueID[1024];
}

- (void)getCaptureDevices;
- (BOOL)initializeVideoCapture;
- (BOOL)initializeVariables;

- (void)registerOwner:(webrtc::videocapturemodule::VideoCaptureMacAVFoundation*)owner;
- (BOOL)setCaptureDeviceById:(char*)uniqueId;
- (void)setCaptureHeight:(int)height width:(int)width frameRate:(int)frameRate rawType:(webrtc::RawVideoType*)rawType;
- (void)startCapture;
- (void)stopCapture;

@end

#endif  // WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_MAC_AVFOUNDATION_VIDEO_CAPTURE_AVFOUNDATION_OBJC_H_
