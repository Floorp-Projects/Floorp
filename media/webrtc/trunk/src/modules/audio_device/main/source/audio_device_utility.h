/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_UTILITY_H
#define WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_UTILITY_H

#include "typedefs.h"

namespace webrtc
{

class AudioDeviceUtility
{
public:
    static WebRtc_UWord32 GetTimeInMS();
	static void Sleep(WebRtc_UWord32 milliseconds);
	static void WaitForKey();
    static bool StringCompare(const char* str1,
                              const char* str2,
                              const WebRtc_UWord32 length);
	virtual WebRtc_Word32 Init() = 0;

	virtual ~AudioDeviceUtility() {}
};

}  // namespace webrtc

#endif  // WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_UTILITY_H

