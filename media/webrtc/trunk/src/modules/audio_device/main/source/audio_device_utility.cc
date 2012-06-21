/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cassert>

#include "audio_device_utility.h"

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

void AudioDeviceUtility::Sleep(WebRtc_UWord32 milliseconds)
{
    return ::Sleep(milliseconds);
}

void AudioDeviceUtility::WaitForKey()
{
	_getch();
}

WebRtc_UWord32 AudioDeviceUtility::GetTimeInMS()
{
	return timeGetTime();
}

bool AudioDeviceUtility::StringCompare(
    const char* str1 , const char* str2,
    const WebRtc_UWord32 length)
{
	return ((_strnicmp(str1, str2, length) == 0) ? true : false);
}

}  // namespace webrtc

#elif defined(WEBRTC_LINUX) || defined(WEBRTC_MAC)

// ============================================================================
//                                 Linux & Mac
// ============================================================================

#include <sys/time.h>   // gettimeofday
#include <time.h>       // nanosleep, gettimeofday
#include <string.h>     // strncasecmp
#include <stdio.h>      // getchar
#include <termios.h>    // tcgetattr

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

WebRtc_UWord32 AudioDeviceUtility::GetTimeInMS()
{
    struct timeval tv;
    struct timezone tz;
    WebRtc_UWord32 val;

    gettimeofday(&tv, &tz);
    val = (WebRtc_UWord32)(tv.tv_sec*1000 + tv.tv_usec/1000);
    return val;
}

void AudioDeviceUtility::Sleep(WebRtc_UWord32 milliseconds)
{
    timespec t;
    t.tv_sec = milliseconds/1000;
    t.tv_nsec = (milliseconds-(milliseconds/1000)*1000)*1000000;
    nanosleep(&t,NULL);
}

bool AudioDeviceUtility::StringCompare(
    const char* str1 , const char* str2, const WebRtc_UWord32 length)
{
    return (strncasecmp(str1, str2, length) == 0)?true: false;
}

}  // namespace webrtc

#endif  // defined(WEBRTC_LINUX) || defined(WEBRTC_MAC)


