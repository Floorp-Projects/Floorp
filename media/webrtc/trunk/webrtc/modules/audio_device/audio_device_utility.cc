/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>

#include "webrtc/modules/audio_device/audio_device_utility.h"

#if defined(_WIN32)

// ============================================================================
//                                     Windows
// ============================================================================

#include <windows.h>
#include <conio.h>
#include <ctype.h>
#include <stdio.h>
#include <mmsystem.h>

namespace webrtc
{

void AudioDeviceUtility::WaitForKey()
{
	_getch();
}

uint32_t AudioDeviceUtility::GetTimeInMS()
{
	return timeGetTime();
}

bool AudioDeviceUtility::StringCompare(
    const char* str1 , const char* str2,
    const uint32_t length)
{
	return ((_strnicmp(str1, str2, length) == 0) ? true : false);
}

}  // namespace webrtc

#elif defined(WEBRTC_LINUX) || defined(WEBRTC_MAC)

// ============================================================================
//                                 Linux & Mac
// ============================================================================

#include <stdio.h>      // getchar
#include <string.h>     // strncasecmp
#include <sys/time.h>   // gettimeofday
#include <termios.h>    // tcgetattr
#include <time.h>       // gettimeofday

#include <unistd.h>

namespace webrtc
{

void AudioDeviceUtility::WaitForKey()
{

    struct termios oldt, newt;

    tcgetattr( STDIN_FILENO, &oldt );

    // we don't want getchar to echo!

    newt = oldt;
    newt.c_lflag &= ~( ICANON | ECHO );
    tcsetattr( STDIN_FILENO, TCSANOW, &newt );

    // catch any newline that's hanging around...

    // you'll have to hit enter twice if you

    // choose enter out of all available keys

    if (getchar() == '\n')
    {
        getchar();
    }

    tcsetattr( STDIN_FILENO, TCSANOW, &oldt );
}

uint32_t AudioDeviceUtility::GetTimeInMS()
{
    struct timeval tv;
    struct timezone tz;
    uint32_t val;

    gettimeofday(&tv, &tz);
    val = (uint32_t)(tv.tv_sec*1000 + tv.tv_usec/1000);
    return val;
}

bool AudioDeviceUtility::StringCompare(
    const char* str1 , const char* str2, const uint32_t length)
{
    return (strncasecmp(str1, str2, length) == 0)?true: false;
}

}  // namespace webrtc

#endif  // defined(WEBRTC_LINUX) || defined(WEBRTC_MAC)
