/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#if defined(WEBRTC_IOS)

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>
#import <sys/sysctl.h>
#import <UIKit/UIKit.h>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/utility/include/helpers_ios.h"

namespace webrtc {
namespace ios {

// TODO(henrika): move to shared location.
// See https://code.google.com/p/webrtc/issues/detail?id=4773 for details.
NSString* NSStringFromStdString(const std::string& stdString) {
  // std::string may contain null termination character so we construct
  // using length.
  return [[NSString alloc] initWithBytes:stdString.data()
                                  length:stdString.length()
                                encoding:NSUTF8StringEncoding];
}

std::string StdStringFromNSString(NSString* nsString) {
  NSData* charData = [nsString dataUsingEncoding:NSUTF8StringEncoding];
  return std::string(reinterpret_cast<const char*>([charData bytes]),
                     [charData length]);
}

bool CheckAndLogError(BOOL success, NSError* error) {
  if (!success) {
    NSString* msg =
        [NSString stringWithFormat:@"Error: %ld, %@, %@", (long)error.code,
                                   error.localizedDescription,
                                   error.localizedFailureReason];
    LOG(LS_ERROR) << StdStringFromNSString(msg);
    return false;
  }
  return true;
}

// TODO(henrika): see if it is possible to move to GetThreadName in
// platform_thread.h and base it on pthread methods instead.
std::string GetCurrentThreadDescription() {
  NSString* name = [NSString stringWithFormat:@"%@", [NSThread currentThread]];
  return StdStringFromNSString(name);
}

std::string GetAudioSessionCategory() {
  NSString* category = [[AVAudioSession sharedInstance] category];
  return StdStringFromNSString(category);
}

std::string GetSystemName() {
  NSString* osName = [[UIDevice currentDevice] systemName];
  return StdStringFromNSString(osName);
}

std::string GetSystemVersion() {
  NSString* osVersion = [[UIDevice currentDevice] systemVersion];
  return StdStringFromNSString(osVersion);
}

float GetSystemVersionAsFloat() {
  NSString* osVersion = [[UIDevice currentDevice] systemVersion];
  return osVersion.floatValue;
}

std::string GetDeviceType() {
  NSString* deviceModel = [[UIDevice currentDevice] model];
  return StdStringFromNSString(deviceModel);
}

std::string GetDeviceName() {
  size_t size;
  sysctlbyname("hw.machine", NULL, &size, NULL, 0);
  rtc::scoped_ptr<char[]> machine;
  machine.reset(new char[size]);
  sysctlbyname("hw.machine", machine.get(), &size, NULL, 0);
  std::string raw_name(machine.get());
  if (!raw_name.compare("iPhone1,1"))
    return std::string("iPhone 1G");
  if (!raw_name.compare("iPhone1,2"))
    return std::string("iPhone 3G");
  if (!raw_name.compare("iPhone2,1"))
    return std::string("iPhone 3GS");
  if (!raw_name.compare("iPhone3,1"))
    return std::string("iPhone 4");
  if (!raw_name.compare("iPhone3,3"))
    return std::string("Verizon iPhone 4");
  if (!raw_name.compare("iPhone4,1"))
    return std::string("iPhone 4S");
  if (!raw_name.compare("iPhone5,1"))
    return std::string("iPhone 5 (GSM)");
  if (!raw_name.compare("iPhone5,2"))
    return std::string("iPhone 5 (GSM+CDMA)");
  if (!raw_name.compare("iPhone5,3"))
    return std::string("iPhone 5c (GSM)");
  if (!raw_name.compare("iPhone5,4"))
    return std::string("iPhone 5c (GSM+CDMA)");
  if (!raw_name.compare("iPhone6,1"))
    return std::string("iPhone 5s (GSM)");
  if (!raw_name.compare("iPhone6,2"))
    return std::string("iPhone 5s (GSM+CDMA)");
  if (!raw_name.compare("iPhone7,1"))
    return std::string("iPhone 6 Plus");
  if (!raw_name.compare("iPhone7,2"))
    return std::string("iPhone 6");
  if (!raw_name.compare("iPhone8,1"))
    return std::string("iPhone 6s");
  if (!raw_name.compare("iPhone8,2"))
    return std::string("iPhone 6s Plus");
  if (!raw_name.compare("iPod1,1"))
    return std::string("iPod Touch 1G");
  if (!raw_name.compare("iPod2,1"))
    return std::string("iPod Touch 2G");
  if (!raw_name.compare("iPod3,1"))
    return std::string("iPod Touch 3G");
  if (!raw_name.compare("iPod4,1"))
    return std::string("iPod Touch 4G");
  if (!raw_name.compare("iPod5,1"))
    return std::string("iPod Touch 5G");
  if (!raw_name.compare("iPad1,1"))
    return std::string("iPad");
  if (!raw_name.compare("iPad2,1"))
    return std::string("iPad 2 (WiFi)");
  if (!raw_name.compare("iPad2,2"))
    return std::string("iPad 2 (GSM)");
  if (!raw_name.compare("iPad2,3"))
    return std::string("iPad 2 (CDMA)");
  if (!raw_name.compare("iPad2,4"))
    return std::string("iPad 2 (WiFi)");
  if (!raw_name.compare("iPad2,5"))
    return std::string("iPad Mini (WiFi)");
  if (!raw_name.compare("iPad2,6"))
    return std::string("iPad Mini (GSM)");
  if (!raw_name.compare("iPad2,7"))
    return std::string("iPad Mini (GSM+CDMA)");
  if (!raw_name.compare("iPad3,1"))
    return std::string("iPad 3 (WiFi)");
  if (!raw_name.compare("iPad3,2"))
    return std::string("iPad 3 (GSM+CDMA)");
  if (!raw_name.compare("iPad3,3"))
    return std::string("iPad 3 (GSM)");
  if (!raw_name.compare("iPad3,4"))
    return std::string("iPad 4 (WiFi)");
  if (!raw_name.compare("iPad3,5"))
    return std::string("iPad 4 (GSM)");
  if (!raw_name.compare("iPad3,6"))
    return std::string("iPad 4 (GSM+CDMA)");
  if (!raw_name.compare("iPad4,1"))
    return std::string("iPad Air (WiFi)");
  if (!raw_name.compare("iPad4,2"))
    return std::string("iPad Air (Cellular)");
  if (!raw_name.compare("iPad4,4"))
    return std::string("iPad mini 2G (WiFi)");
  if (!raw_name.compare("iPad4,5"))
    return std::string("iPad mini 2G (Cellular)");
  if (!raw_name.compare("i386"))
    return std::string("Simulator");
  if (!raw_name.compare("x86_64"))
    return std::string("Simulator");
  LOG(LS_WARNING) << "Failed to find device name (" << raw_name << ")";
  return raw_name;
}

}  // namespace ios
}  // namespace webrtc

#endif  // defined(WEBRTC_IOS)
