/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_UTILITY_INCLUDE_HELPERS_IOS_H_
#define WEBRTC_MODULES_UTILITY_INCLUDE_HELPERS_IOS_H_

#if defined(WEBRTC_IOS)

#include <string>

namespace webrtc {
namespace ios {

bool CheckAndLogError(BOOL success, NSError* error);

std::string StdStringFromNSString(NSString* nsString);

// Return thread ID as a string.
std::string GetThreadId();

// Return thread ID as string suitable for debug logging.
std::string GetThreadInfo();

// Returns [NSThread currentThread] description as string.
// Example: <NSThread: 0x170066d80>{number = 1, name = main}
std::string GetCurrentThreadDescription();

std::string GetAudioSessionCategory();

// Returns the current name of the operating system.
std::string GetSystemName();

// Returns the current version of the operating system.
std::string GetSystemVersion();

// Returns the version of the operating system as a floating point value.
float GetSystemVersionAsFloat();

// Returns the device type.
// Examples: ”iPhone” and ”iPod touch”.
std::string GetDeviceType();

// Returns a more detailed device name.
// Examples: "iPhone 5s (GSM)" and "iPhone 6 Plus".
std::string GetDeviceName();

}  // namespace ios
}  // namespace webrtc

#endif  // defined(WEBRTC_IOS)

#endif  // WEBRTC_MODULES_UTILITY_INCLUDE_HELPERS_IOS_H_
