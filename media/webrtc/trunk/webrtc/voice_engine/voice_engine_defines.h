/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 *  This file contains common constants for VoiceEngine, as well as
 *  platform specific settings and include files.
 */

#ifndef WEBRTC_VOICE_ENGINE_VOICE_ENGINE_DEFINES_H
#define WEBRTC_VOICE_ENGINE_VOICE_ENGINE_DEFINES_H

#include "common_types.h"
#include "engine_configurations.h"

// ----------------------------------------------------------------------------
//  Enumerators
// ----------------------------------------------------------------------------

namespace webrtc
{

// VolumeControl
enum { kMinVolumeLevel = 0 };
enum { kMaxVolumeLevel = 255 };
// Min scale factor for per-channel volume scaling
const float kMinOutputVolumeScaling = 0.0f;
// Max scale factor for per-channel volume scaling
const float kMaxOutputVolumeScaling = 10.0f;
// Min scale factor for output volume panning
const float kMinOutputVolumePanning = 0.0f;
// Max scale factor for output volume panning
const float kMaxOutputVolumePanning = 1.0f;

// DTMF
enum { kMinDtmfEventCode = 0 };                 // DTMF digit "0"
enum { kMaxDtmfEventCode = 15 };                // DTMF digit "D"
enum { kMinTelephoneEventCode = 0 };            // RFC4733 (Section 2.3.1)
enum { kMaxTelephoneEventCode = 255 };          // RFC4733 (Section 2.3.1)
enum { kMinTelephoneEventDuration = 100 };
enum { kMaxTelephoneEventDuration = 60000 };    // Actual limit is 2^16
enum { kMinTelephoneEventAttenuation = 0 };     // 0 dBm0
enum { kMaxTelephoneEventAttenuation = 36 };    // -36 dBm0
enum { kMinTelephoneEventSeparationMs = 100 };  // Min delta time between two
                                                // telephone events
enum { kVoiceEngineMaxIpPacketSizeBytes = 1500 };       // assumes Ethernet

enum { kVoiceEngineMaxModuleVersionSize = 960 };

// Base
enum { kVoiceEngineVersionMaxMessageSize = 1024 };

// Encryption
// SRTP uses 30 bytes key length
enum { kVoiceEngineMaxSrtpKeyLength = 30 };
// SRTP minimum key/tag length for encryption level
enum { kVoiceEngineMinSrtpEncryptLength = 16 };
// SRTP maximum key/tag length for encryption level
enum { kVoiceEngineMaxSrtpEncryptLength = 256 };
// SRTP maximum key/tag length for authentication level,
// HMAC SHA1 authentication type
enum { kVoiceEngineMaxSrtpAuthSha1Length = 20 };
// SRTP maximum tag length for authentication level,
// null authentication type
enum { kVoiceEngineMaxSrtpTagAuthNullLength = 12 };
// SRTP maximum key length for authentication level,
// null authentication type
enum { kVoiceEngineMaxSrtpKeyAuthNullLength = 256 };

// Audio processing
enum { kVoiceEngineAudioProcessingDeviceSampleRateHz = 48000 };

// Codec
// Min init target rate for iSAC-wb
enum { kVoiceEngineMinIsacInitTargetRateBpsWb = 10000 };
// Max init target rate for iSAC-wb
enum { kVoiceEngineMaxIsacInitTargetRateBpsWb = 32000 };
// Min init target rate for iSAC-swb
enum { kVoiceEngineMinIsacInitTargetRateBpsSwb = 10000 };
// Max init target rate for iSAC-swb
enum { kVoiceEngineMaxIsacInitTargetRateBpsSwb = 56000 };
// Lowest max rate for iSAC-wb
enum { kVoiceEngineMinIsacMaxRateBpsWb = 32000 };
// Highest max rate for iSAC-wb
enum { kVoiceEngineMaxIsacMaxRateBpsWb = 53400 };
// Lowest max rate for iSAC-swb
enum { kVoiceEngineMinIsacMaxRateBpsSwb = 32000 };
// Highest max rate for iSAC-swb
enum { kVoiceEngineMaxIsacMaxRateBpsSwb = 107000 };
// Lowest max payload size for iSAC-wb
enum { kVoiceEngineMinIsacMaxPayloadSizeBytesWb = 120 };
// Highest max payload size for iSAC-wb
enum { kVoiceEngineMaxIsacMaxPayloadSizeBytesWb = 400 };
// Lowest max payload size for iSAC-swb
enum { kVoiceEngineMinIsacMaxPayloadSizeBytesSwb = 120 };
// Highest max payload size for iSAC-swb
enum { kVoiceEngineMaxIsacMaxPayloadSizeBytesSwb = 600 };

// VideoSync
// Lowest minimum playout delay
enum { kVoiceEngineMinMinPlayoutDelayMs = 0 };
// Highest minimum playout delay
enum { kVoiceEngineMaxMinPlayoutDelayMs = 1000 };

// Network
// Min packet-timeout time for received RTP packets
enum { kVoiceEngineMinPacketTimeoutSec = 1 };
// Max packet-timeout time for received RTP packets
enum { kVoiceEngineMaxPacketTimeoutSec = 150 };
// Min sample time for dead-or-alive detection
enum { kVoiceEngineMinSampleTimeSec = 1 };
// Max sample time for dead-or-alive detection
enum { kVoiceEngineMaxSampleTimeSec = 150 };

// RTP/RTCP
// Min 4-bit ID for RTP extension (see section 4.2 in RFC 5285)
enum { kVoiceEngineMinRtpExtensionId = 1 };
// Max 4-bit ID for RTP extension
enum { kVoiceEngineMaxRtpExtensionId = 14 };

} // namespace webrtc

// TODO(andrew): we shouldn't be using the precompiler for this.
// Use enums or bools as appropriate.
#define WEBRTC_AUDIO_PROCESSING_OFF false

#define WEBRTC_VOICE_ENGINE_HP_DEFAULT_STATE true
    // AudioProcessing HP is ON
#define WEBRTC_VOICE_ENGINE_NS_DEFAULT_STATE  WEBRTC_AUDIO_PROCESSING_OFF
    // AudioProcessing NS off
#define WEBRTC_VOICE_ENGINE_AGC_DEFAULT_STATE true
    // AudioProcessing AGC on
#define WEBRTC_VOICE_ENGINE_EC_DEFAULT_STATE  WEBRTC_AUDIO_PROCESSING_OFF
    // AudioProcessing EC off
#define WEBRTC_VOICE_ENGINE_VAD_DEFAULT_STATE WEBRTC_AUDIO_PROCESSING_OFF
    // AudioProcessing off
#define WEBRTC_VOICE_ENGINE_RX_AGC_DEFAULT_STATE WEBRTC_AUDIO_PROCESSING_OFF
    // AudioProcessing RX AGC off
#define WEBRTC_VOICE_ENGINE_RX_NS_DEFAULT_STATE WEBRTC_AUDIO_PROCESSING_OFF
    // AudioProcessing RX NS off
#define WEBRTC_VOICE_ENGINE_RX_HP_DEFAULT_STATE WEBRTC_AUDIO_PROCESSING_OFF
    // AudioProcessing RX High Pass Filter off

#define WEBRTC_VOICE_ENGINE_NS_DEFAULT_MODE NoiseSuppression::kModerate
    // AudioProcessing NS moderate suppression
#define WEBRTC_VOICE_ENGINE_AGC_DEFAULT_MODE GainControl::kAdaptiveAnalog
    // AudioProcessing AGC analog digital combined
#define WEBRTC_VOICE_ENGINE_RX_AGC_DEFAULT_MODE GainControl::kAdaptiveDigital
    // AudioProcessing AGC mode
#define WEBRTC_VOICE_ENGINE_RX_NS_DEFAULT_MODE NoiseSuppression::kModerate
    // AudioProcessing RX NS mode

// Macros
// Comparison of two strings without regard to case
#define STR_CASE_CMP(x,y) ::_stricmp(x,y)
// Compares characters of two strings without regard to case
#define STR_NCASE_CMP(x,y,n) ::_strnicmp(x,y,n)

// ----------------------------------------------------------------------------
//  Build information macros
// ----------------------------------------------------------------------------

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

// Example: "Oct 10 2002 12:05:30 r"
#define BUILDINFO BUILDDATE " " BUILDTIME " " BUILDMODE

// ----------------------------------------------------------------------------
//  Macros
// ----------------------------------------------------------------------------

#if (defined(_DEBUG) && defined(_WIN32) && (_MSC_VER >= 1400))
  #include <windows.h>
  #include <stdio.h>
  #define DEBUG_PRINT(...)      \
  {                             \
    char msg[256];              \
    sprintf(msg, __VA_ARGS__);  \
    OutputDebugStringA(msg);    \
  }
#else
  // special fix for visual 2003
  #define DEBUG_PRINT(exp)      ((void)0)
#endif  // defined(_DEBUG) && defined(_WIN32)

#define CHECK_CHANNEL(channel)  if (CheckChannel(channel) == -1) return -1;

// ----------------------------------------------------------------------------
//  Default Trace filter
// ----------------------------------------------------------------------------

#define WEBRTC_VOICE_ENGINE_DEFAULT_TRACE_FILTER \
    kTraceStateInfo | kTraceWarning | kTraceError | kTraceCritical | \
    kTraceApiCall

// ----------------------------------------------------------------------------
//  Inline functions
// ----------------------------------------------------------------------------

namespace webrtc
{

inline int VoEId(const int veId, const int chId)
{
    if (chId == -1)
    {
        const int dummyChannel(99);
        return (int) ((veId << 16) + dummyChannel);
    }
    return (int) ((veId << 16) + chId);
}

inline int VoEModuleId(const int veId, const int chId)
{
    return (int) ((veId << 16) + chId);
}

// Convert module ID to internal VoE channel ID
inline int VoEChannelId(const int moduleId)
{
    return (int) (moduleId & 0xffff);
}

} // namespace webrtc

// ----------------------------------------------------------------------------
//  Platform settings
// ----------------------------------------------------------------------------

// *** WINDOWS ***

#if defined(_WIN32)

  #pragma comment( lib, "winmm.lib" )

  #ifndef WEBRTC_EXTERNAL_TRANSPORT
    #pragma comment( lib, "ws2_32.lib" )
  #endif

// ----------------------------------------------------------------------------
//  Enumerators
// ----------------------------------------------------------------------------

namespace webrtc
{
// Max number of supported channels
enum { kVoiceEngineMaxNumOfChannels = 32 };
// Max number of channels which can be played out simultaneously
enum { kVoiceEngineMaxNumOfActiveChannels = 16 };
} // namespace webrtc

// ----------------------------------------------------------------------------
//  Defines
// ----------------------------------------------------------------------------

  #include <windows.h>

  // Comparison of two strings without regard to case
  #define STR_CASE_CMP(x,y) ::_stricmp(x,y)
  // Compares characters of two strings without regard to case
  #define STR_NCASE_CMP(x,y,n) ::_strnicmp(x,y,n)

// Default device for Windows PC
  #define WEBRTC_VOICE_ENGINE_DEFAULT_DEVICE \
    AudioDeviceModule::kDefaultCommunicationDevice

#endif  // #if (defined(_WIN32)

// *** LINUX ***

#ifdef WEBRTC_LINUX

#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#ifndef QNX
  #include <linux/net.h>
#ifndef ANDROID
  #include <sys/soundcard.h>
#endif // ANDROID
#endif // QNX
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>
#include <time.h>
#include <sys/time.h>

#define DWORD unsigned long int
#define WINAPI
#define LPVOID void *
#define FALSE 0
#define TRUE 1
#define UINT unsigned int
#define UCHAR unsigned char
#define TCHAR char
#ifdef QNX
#define _stricmp stricmp
#else
#define _stricmp strcasecmp
#endif
#define GetLastError() errno
#define WSAGetLastError() errno
#define LPCTSTR const char*
#define LPCSTR const char*
#define wsprintf sprintf
#define TEXT(a) a
#define _ftprintf fprintf
#define _tcslen strlen
#define FAR
#define __cdecl
#define LPSOCKADDR struct sockaddr *

// Default device for Linux and Android
#define WEBRTC_VOICE_ENGINE_DEFAULT_DEVICE 0

#ifdef ANDROID

// ----------------------------------------------------------------------------
//  Enumerators
// ----------------------------------------------------------------------------

namespace webrtc
{
  // Max number of supported channels
  enum { kVoiceEngineMaxNumOfChannels = 32 };
  // Max number of channels which can be played out simultaneously
  enum { kVoiceEngineMaxNumOfActiveChannels = 16 };
} // namespace webrtc

// ----------------------------------------------------------------------------
//  Defines
// ----------------------------------------------------------------------------

  // Always excluded for Android builds
  #undef WEBRTC_CODEC_ISAC
  // We need WEBRTC_VOE_EXTERNAL_REC_AND_PLAYOUT to make things work on Android.  
  // Motivation for the commented-out undef below is unclear.
  //
  // #undef WEBRTC_VOE_EXTERNAL_REC_AND_PLAYOUT
  #undef WEBRTC_CONFERENCING
  #undef WEBRTC_TYPING_DETECTION

  // Default audio processing states
  #undef  WEBRTC_VOICE_ENGINE_NS_DEFAULT_STATE
  #undef  WEBRTC_VOICE_ENGINE_AGC_DEFAULT_STATE
  #undef  WEBRTC_VOICE_ENGINE_EC_DEFAULT_STATE
  #define WEBRTC_VOICE_ENGINE_NS_DEFAULT_STATE  WEBRTC_AUDIO_PROCESSING_OFF
  #define WEBRTC_VOICE_ENGINE_AGC_DEFAULT_STATE WEBRTC_AUDIO_PROCESSING_OFF
  #define WEBRTC_VOICE_ENGINE_EC_DEFAULT_STATE  WEBRTC_AUDIO_PROCESSING_OFF

  // Default audio processing modes
  #undef  WEBRTC_VOICE_ENGINE_NS_DEFAULT_MODE
  #undef  WEBRTC_VOICE_ENGINE_AGC_DEFAULT_MODE
  #define WEBRTC_VOICE_ENGINE_NS_DEFAULT_MODE  \
      NoiseSuppression::kModerate
  #define WEBRTC_VOICE_ENGINE_AGC_DEFAULT_MODE \
      GainControl::kAdaptiveDigital

  // This macro used to cause the calling function to set an error code and return.
  // However, not doing that seems to cause the unit tests to pass / behave reasonably,
  // so it's disabled for now; see bug 819856.
  #define ANDROID_NOT_SUPPORTED(stat)

#else // LINUX PC
// ----------------------------------------------------------------------------
//  Enumerators
// ----------------------------------------------------------------------------

namespace webrtc
{
  // Max number of supported channels
  enum { kVoiceEngineMaxNumOfChannels = 32 };
  // Max number of channels which can be played out simultaneously
  enum { kVoiceEngineMaxNumOfActiveChannels = 16 };
} // namespace webrtc

// ----------------------------------------------------------------------------
//  Defines
// ----------------------------------------------------------------------------

  #define ANDROID_NOT_SUPPORTED(stat)

#endif // ANDROID - LINUX PC

#else
#define ANDROID_NOT_SUPPORTED(stat)
#endif  // #ifdef WEBRTC_LINUX

// *** WEBRTC_MAC ***
// including iPhone

#ifdef WEBRTC_MAC

#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/time.h>
#include <time.h>
#include <AudioUnit/AudioUnit.h>
#if !defined(WEBRTC_IOS)
  #include <CoreServices/CoreServices.h>
  #include <CoreAudio/CoreAudio.h>
  #include <AudioToolbox/DefaultAudioOutput.h>
  #include <AudioToolbox/AudioConverter.h>
  #include <CoreAudio/HostTime.h>
#endif

#define DWORD unsigned long int
#define WINAPI
#define LPVOID void *
#define FALSE 0
#define TRUE 1
#define SOCKADDR_IN struct sockaddr_in
#define UINT unsigned int
#define UCHAR unsigned char
#define TCHAR char
#define _stricmp strcasecmp
#define GetLastError() errno
#define WSAGetLastError() errno
#define LPCTSTR const char*
#define wsprintf sprintf
#define TEXT(a) a
#define _ftprintf fprintf
#define _tcslen strlen
#define FAR
#define __cdecl
#define LPSOCKADDR struct sockaddr *
#define LPCSTR const char*
#define ULONG unsigned long

// Default device for Mac and iPhone
#define WEBRTC_VOICE_ENGINE_DEFAULT_DEVICE 0

// iPhone specific
#if defined(WEBRTC_IOS)

// ----------------------------------------------------------------------------
//  Enumerators
// ----------------------------------------------------------------------------

namespace webrtc
{
  // Max number of supported channels
  enum { kVoiceEngineMaxNumOfChannels = 2 };
  // Max number of channels which can be played out simultaneously
  enum { kVoiceEngineMaxNumOfActiveChannels = 2 };
} // namespace webrtc

// ----------------------------------------------------------------------------
//  Defines
// ----------------------------------------------------------------------------

  // Always excluded for iPhone builds
  #undef WEBRTC_CODEC_ISAC
  #undef WEBRTC_VOE_EXTERNAL_REC_AND_PLAYOUT

  #undef  WEBRTC_VOICE_ENGINE_NS_DEFAULT_STATE
  #undef  WEBRTC_VOICE_ENGINE_AGC_DEFAULT_STATE
  #undef  WEBRTC_VOICE_ENGINE_EC_DEFAULT_STATE
  #define WEBRTC_VOICE_ENGINE_NS_DEFAULT_STATE  WEBRTC_AUDIO_PROCESSING_OFF
  #define WEBRTC_VOICE_ENGINE_AGC_DEFAULT_STATE WEBRTC_AUDIO_PROCESSING_OFF
  #define WEBRTC_VOICE_ENGINE_EC_DEFAULT_STATE  WEBRTC_AUDIO_PROCESSING_OFF

  #undef  WEBRTC_VOICE_ENGINE_NS_DEFAULT_MODE
  #undef  WEBRTC_VOICE_ENGINE_AGC_DEFAULT_MODE
  #define WEBRTC_VOICE_ENGINE_NS_DEFAULT_MODE \
      NoiseSuppression::kModerate
  #define WEBRTC_VOICE_ENGINE_AGC_DEFAULT_MODE \
      GainControl::kAdaptiveDigital

  #define IPHONE_NOT_SUPPORTED(stat) \
    stat.SetLastError(VE_FUNC_NOT_SUPPORTED, kTraceError, \
                      "API call not supported"); \
    return -1;

#else // Non-iPhone

// ----------------------------------------------------------------------------
//  Enumerators
// ----------------------------------------------------------------------------

namespace webrtc
{
  // Max number of supported channels
  enum { kVoiceEngineMaxNumOfChannels = 32 };
  // Max number of channels which can be played out simultaneously
  enum { kVoiceEngineMaxNumOfActiveChannels = 16 };
} // namespace webrtc

// ----------------------------------------------------------------------------
//  Defines
// ----------------------------------------------------------------------------

  #define IPHONE_NOT_SUPPORTED(stat)
#endif

#else
#define IPHONE_NOT_SUPPORTED(stat)
#endif  // #ifdef WEBRTC_MAC



#endif // WEBRTC_VOICE_ENGINE_VOICE_ENGINE_DEFINES_H
