/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_VIE_DEFINES_H_
#define WEBRTC_VIDEO_ENGINE_VIE_DEFINES_H_

#include "webrtc/engine_configurations.h"

// TODO(mflodman) Remove.
#ifdef WEBRTC_ANDROID
#include <arpa/inet.h>  // NOLINT
#include <linux/net.h>  // NOLINT
#include <netinet/in.h>  // NOLINT
#include <pthread.h>  // NOLINT
#include <stdio.h>  // NOLINT
#include <stdlib.h>  // NOLINT
#include <string.h>  // NOLINT
#include <sys/socket.h>  // NOLINT
#include <sys/time.h>  // NOLINT
#include <sys/types.h>  // NOLINT
#include <time.h>  // NOLINT
#endif

namespace webrtc {

// General
enum { kViEMinKeyRequestIntervalMs = 300 };

// ViEBase
enum { kViEMaxNumberOfChannels = 64 };

// ViECapture
enum { kViEMaxCaptureDevices = 256 };
enum { kViECaptureDefaultWidth = 352 };
enum { kViECaptureDefaultHeight = 288 };
enum { kViECaptureDefaultFramerate = 30 };
enum { kViECaptureMaxSnapshotWaitTimeMs = 500 };

// ViECodec
enum { kViEMaxCodecWidth = 4096 };
enum { kViEMaxCodecHeight = 3072 };
enum { kViEMaxCodecFramerate = 60 };
enum { kViEMinCodecBitrate = 30 };

// ViENetwork
enum { kViEMaxMtu = 1500 };
enum { kViESocketThreads = 1 };
enum { kViENumReceiveSocketBuffers = 500 };

// ViERender
// Max valid time set in SetRenderTimeoutImage
enum { kViEMaxRenderTimeoutTimeMs  = 10000 };
// Min valid time set in SetRenderTimeoutImage
enum { kViEMinRenderTimeoutTimeMs = 33 };
enum { kViEDefaultRenderDelayMs = 10 };

// ViERTP_RTCP
enum { kSendSidePacketHistorySize = 600 };

// NACK
enum { kMaxPacketAgeToNack = 450 };  // In sequence numbers.
enum { kMaxNackListSize = 250 };

// Id definitions
enum {
  kViEChannelIdBase = 0x0,
  kViEChannelIdMax = 0xFF,
  kViECaptureIdBase = 0x1001,
  kViECaptureIdMax = 0x10FF,
  kViEDummyChannelId = 0xFFFF
};

// Module id
// Create a unique id based on the ViE instance id and the
// channel id. ViE id > 0 and 0 <= channel id <= 255

inline int ViEId(const int vieId, const int channelId = -1) {
  if (channelId == -1) {
    return static_cast<int>((vieId << 16) + kViEDummyChannelId);
  }
  return static_cast<int>((vieId << 16) + channelId);
}

inline int ViEModuleId(const int vieId, const int channelId = -1) {
  if (channelId == -1) {
    return static_cast<int>((vieId << 16) + kViEDummyChannelId);
  }
  return static_cast<int>((vieId << 16) + channelId);
}

inline int ChannelId(const int moduleId) {
  return static_cast<int>(moduleId & 0xffff);
}

// Windows specific.
#if defined(_WIN32)
  #define RENDER_MODULE_TYPE kRenderWindows

  // Include libraries.
  #pragma comment(lib, "winmm.lib")

  #ifndef WEBRTC_EXTERNAL_TRANSPORT
  #pragma comment(lib, "ws2_32.lib")
  #pragma comment(lib, "Iphlpapi.lib")   // _GetAdaptersAddresses
  #endif
#endif

// Mac specific.
#ifdef WEBRTC_MAC
  #define SLEEP(x) usleep(x * 1000)
  #define RENDER_MODULE_TYPE kRenderWindows
#endif

// Android specific.
#ifdef WEBRTC_ANDROID
  #define FAR
  #define __cdecl
#endif  // WEBRTC_ANDROID

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_VIE_DEFINES_H_
