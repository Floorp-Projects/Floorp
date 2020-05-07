/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import <AVFoundation/AVFoundation.h>

#import "modules/video_capture/objc/device_info_objc.h"
#include "modules/video_capture/video_capture_config.h"

@implementation DeviceInfoIosObjC

- (id)init {
  self = [super init];

  if (nil != self) {
    [self configureObservers];
  }

  return self;
}


- (void)dealloc {
   NSNotificationCenter* notificationCenter = [NSNotificationCenter defaultCenter];
   for (id observer in _observers)
       [notificationCenter removeObserver:observer];
}

- (void)registerOwner:(DeviceInfoIos*)owner {
  [_lock lock];
  _owner = owner;
  [_lock unlock];
}

+ (int)captureDeviceCount {
  int cnt = 0;
  for (AVCaptureDevice* device in
       [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo]) {
    if ([device isSuspended]) {
      continue;
    }
    cnt++;
  }
  return cnt;
}

+ (AVCaptureDevice*)captureDeviceForIndex:(int)index {
  int cnt = 0;
  for (AVCaptureDevice* device in
       [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo]) {
    if ([device isSuspended]) {
      continue;
    }
    if (cnt == index) {
      return device;
    }
    cnt++;
  }

  return nil;
}

+ (AVCaptureDevice*)captureDeviceForUniqueId:(NSString*)uniqueId {
  for (AVCaptureDevice* device in
       [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo]) {
    if ([device isSuspended]) {
      continue;
    }
    if ([uniqueId isEqual:device.uniqueID]) {
      return device;
    }
  }

  return nil;
}

+ (NSString*)deviceNameForIndex:(int)index {
  return [DeviceInfoIosObjC captureDeviceForIndex:index].localizedName;
}

+ (NSString*)deviceUniqueIdForIndex:(int)index {
  return [DeviceInfoIosObjC captureDeviceForIndex:index].uniqueID;
}

+ (NSString*)deviceNameForUniqueId:(NSString*)uniqueId {
  return [[AVCaptureDevice deviceWithUniqueID:uniqueId] localizedName];
}

+ (webrtc::VideoCaptureCapability)capabilityForPreset:(NSString*)preset {
  webrtc::VideoCaptureCapability capability;

  // TODO(tkchin): Maybe query AVCaptureDevice for supported formats, and
  // then get the dimensions / frame rate from each supported format
  if ([preset isEqualToString:AVCaptureSessionPreset352x288]) {
    capability.width = 352;
    capability.height = 288;
    capability.maxFPS = 30;
    capability.videoType = webrtc::VideoType::kNV12;
    capability.interlaced = false;
  } else if ([preset isEqualToString:AVCaptureSessionPreset640x480]) {
    capability.width = 640;
    capability.height = 480;
    capability.maxFPS = 30;
    capability.videoType = webrtc::VideoType::kNV12;
    capability.interlaced = false;
  } else if ([preset isEqualToString:AVCaptureSessionPreset1280x720]) {
    capability.width = 1280;
    capability.height = 720;
    capability.maxFPS = 30;
    capability.videoType = webrtc::VideoType::kNV12;
    capability.interlaced = false;
  }

  return capability;
}

- (void)configureObservers {
  //register device connected / disconnected event
  _lock = [[NSLock alloc] init];

  NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];

  id deviceWasConnectedObserver = [notificationCenter addObserverForName:AVCaptureDeviceWasConnectedNotification
      object:nil
      queue:[NSOperationQueue mainQueue]
      usingBlock:^(NSNotification *note) {
          [_lock lock];
          AVCaptureDevice *device = [note object];
          BOOL isVideoDevice = [device hasMediaType: AVMediaTypeVideo];
          if(isVideoDevice && _owner)
              _owner->DeviceChange();
          [_lock unlock];
      }];

  id deviceWasDisconnectedObserver = [notificationCenter addObserverForName:AVCaptureDeviceWasDisconnectedNotification
      object:nil
      queue:[NSOperationQueue mainQueue]
      usingBlock:^(NSNotification *note) {
          [_lock lock];
          AVCaptureDevice *device = [note object];
          BOOL isVideoDevice = [device hasMediaType: AVMediaTypeVideo];
          if(isVideoDevice && _owner)
              _owner->DeviceChange();
          [_lock unlock];
      }];

  _observers = [[NSArray alloc] initWithObjects:deviceWasConnectedObserver, deviceWasDisconnectedObserver, nil];
}

@end
