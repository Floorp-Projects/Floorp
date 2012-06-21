/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "event_wrapper.h"

#if defined(_WIN32)
    #include <windows.h>
    #include "event_win.h"
#elif defined(WEBRTC_MAC_INTEL)
    #include <ApplicationServices/ApplicationServices.h>
    #include <pthread.h>
    #include "event_posix.h"
#else
    #include <pthread.h>
    #include "event_posix.h"
#endif

namespace webrtc {
EventWrapper* EventWrapper::Create()
{
#if defined(_WIN32)
    return new EventWindows();
#else
    return EventPosix::Create();
#endif
}

int EventWrapper::KeyPressed()
{
#if defined(_WIN32)
    int keyDown = 0;
    for(int key = 0x20; key < 0x90; key++)
    {
        short res = GetAsyncKeyState(key);
        keyDown |= res%2;  // Get the LSB
    }
    if(keyDown)
    {
        return 1;
    }
    else
    {
        return 0;
    }
#elif defined(WEBRTC_MAC_INTEL)
    bool keyDown = false;
    // loop through all Mac virtual key constant values
    for(int keyIndex = 0; keyIndex <= 0x5C; keyIndex++) 
    {
        keyDown |= CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, keyIndex);
    }
    if(keyDown)
    {
        return 1;
    }
    else
    {
        return 0;
    } 
#else
    return -1;
#endif
}
} // namespace webrtc
