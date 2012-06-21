/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * vie_defines.h
 */

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_DEFINES_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_DEFINES_H_

#include "engine_configurations.h"

#ifdef WEBRTC_MAC_INTEL
#include <stdio.h>
#include <unistd.h>
#endif

#ifdef WEBRTC_ANDROID
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/net.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#endif

namespace webrtc
{
// ===================================================
// ViE Defines
// ===================================================

// General
enum { kViEMinKeyRequestIntervalMs = 300};

// ViEBase
enum { kViEMaxNumberOfChannels = 32};
enum { kViEVersionMaxMessageSize = 1024 };
enum { kViEMaxModuleVersionSize = 960 };

// ViECapture
enum { kViEMaxCaptureDevices=10};
// Width used if no send codec has been set when a capture device is started
enum { kViECaptureDefaultWidth = 352};
// Height used if no send codec has been set when a capture device is started
enum { kViECaptureDefaultHeight = 288};
enum { kViECaptureDefaultFramerate = 30};
enum { kViECaptureMaxSnapshotWaitTimeMs = 500 };

// ViECodec
enum { kViEMaxCodecWidth = 1920};
enum { kViEMaxCodecHeight = 1200};
enum { kViEMaxCodecFramerate = 60};
enum { kViEMinCodecBitrate = 30};

// ViEEncryption
enum { kViEMaxSrtpKeyLength = 30};
enum { kViEMinSrtpEncryptLength = 16};
enum { kViEMaxSrtpEncryptLength = 256};
enum { kViEMaxSrtpAuthSh1Length = 20};
enum { kViEMaxSrtpTagAuthNullLength = 12};
enum { kViEMaxSrtpKeyAuthNullLength = 256};

// ViEExternalCodec

// ViEFile
enum { kViEMaxFilePlayers = 3};

// ViEImageProcess

// ViENetwork
enum { kViEMaxMtu = 1500};
enum { kViESocketThreads = 1};
enum { kViENumReceiveSocketBuffers = 500};

// ViERender
// Max valid time set in SetRenderTimeoutImage
enum { kViEMaxRenderTimeoutTimeMs  = 10000};
// Min valid time set in SetRenderTimeoutImage
enum { kViEMinRenderTimeoutTimeMs = 33};
enum { kViEDefaultRenderDelayMs = 10};

// ViERTP_RTCP
enum { kNackHistorySize = 400};

// Id definitions
enum {
    kViEChannelIdBase=0x0,
    kViEChannelIdMax=0xFF,
    kViECaptureIdBase=0x1001,
    kViECaptureIdMax=0x10FF,
    kViEFileIdBase=0x2000,
    kViEFileIdMax=0x200F,
    kViEDummyChannelId=0xFFFF
};

// Module id
// Create a unique id based on the ViE instance id and the
// channel id. ViE id > 0 and 0 <= channel id <= 255

inline int ViEId(const int vieId, const int channelId = -1)
{
    if (channelId == -1)
    {
        return (int) ((vieId << 16) + kViEDummyChannelId);
    }
    return (int) ((vieId << 16) + channelId);
}

inline int ViEModuleId(const int vieId, const int channelId = -1)
{
    if (channelId == -1)
    {
        return (int) ((vieId << 16) + kViEDummyChannelId);
    }
    return (int) ((vieId << 16) + channelId);
}

inline int ChannelId(const int moduleId)
{
    return (int) (moduleId & 0xffff);
}


// ============================================================================
// Platform specifics
// ============================================================================

//-------------------------------------
// Windows
//-------------------------------------

#if defined(_WIN32)
//  Build information macros
    #if defined(_DEBUG)
    #define BUILDMODE TEXT("d")
    #elif defined(DEBUG)
    #define BUILDMODE TEXT("d")
    #elif defined(NDEBUG)
    #define BUILDMODE TEXT("r")
    #else
    #define BUILDMODE TEXT("?")
    #endif

    #define BUILDTIME TEXT(__TIME__)
    #define BUILDDATE TEXT(__DATE__)

    // example: "Oct 10 2002 12:05:30 r"
    #define BUILDINFO BUILDDATE TEXT(" ") BUILDTIME TEXT(" ") BUILDMODE


    #define RENDER_MODULE_TYPE kRenderWindows
    // Warning pragmas
    // new behavior: elements of array 'XXX' will be default initialized
    #pragma warning(disable: 4351)
    // 'this' : used in base member initializer list
    #pragma warning(disable: 4355)
    // frame pointer register 'ebp' modified by inline assembly code
    #pragma warning(disable: 4731)

    // Include libraries
    #pragma comment( lib, "winmm.lib" )
#ifndef WEBRTC_EXTERNAL_TRANSPORT
    #pragma comment( lib, "ws2_32.lib" )
    #pragma comment( lib, "Iphlpapi.lib" )   // _GetAdaptersAddresses
#endif
#endif


//-------------------------------------
// Mac
//-------------------------------------

#ifdef WEBRTC_MAC_INTEL
	#define SLEEP(x) usleep(x * 1000)

	//  Build information macros
	#define TEXT(x) x
	#if defined(_DEBUG)
    #define BUILDMODE TEXT("d")
	#elif defined(DEBUG)
		#define BUILDMODE TEXT("d")
	#elif defined(NDEBUG)
		#define BUILDMODE TEXT("r")
	#else
		#define BUILDMODE TEXT("?")
	#endif

	#define BUILDTIME TEXT(__TIME__)
	#define BUILDDATE TEXT(__DATE__)

	// example: "Oct 10 2002 12:05:30 r"
	#define BUILDINFO BUILDDATE TEXT(" ") BUILDTIME TEXT(" ") BUILDMODE

	#define RENDER_MODULE_TYPE kRenderWindows
#endif

//-------------------------------------
// Linux
//-------------------------------------

#ifndef WEBRTC_ANDROID
#ifdef WEBRTC_LINUX

//  Build information macros
#if defined(_DEBUG)
    #define BUILDMODE "d"
#elif defined(DEBUG)
    #define BUILDMODE "d"
#elif defined(NDEBUG)
    #define BUILDMODE "r"
#else
    #define BUILDMODE "?"
#endif

#define BUILDTIME __TIME__
#define BUILDDATE __DATE__

// example: "Oct 10 2002 12:05:30 r"
#define BUILDINFO BUILDDATE " " BUILDTIME " " BUILDMODE

#endif  // ifdef WEBRTC_LINUX
#endif  // ifndef WEBRTC_ANDROID

#ifdef WEBRTC_ANDROID

    #define FAR
    #define __cdecl

    #if defined(_DEBUG)
        #define BUILDMODE "d"
    #elif defined(DEBUG)
        #define BUILDMODE "d"
    #elif defined(NDEBUG)
        #define BUILDMODE "r"
    #else
        #define BUILDMODE "?"
    #endif

    #define BUILDTIME __TIME__
    #define BUILDDATE __DATE__

    // example: "Oct 10 2002 12:05:30 r"
    #define BUILDINFO BUILDDATE " " BUILDTIME " " BUILDMODE

#endif  // #ifdef WEBRTC_ANDROID

} //namespace webrtc
#endif  // WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_DEFINES_H_
