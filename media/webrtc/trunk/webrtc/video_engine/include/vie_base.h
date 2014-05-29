/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This sub-API supports the following functionalities:
//
//  - Creating and deleting VideoEngine instances.
//  - Creating and deleting channels.
//  - Connect a video channel with a corresponding voice channel for audio/video
//    synchronization.
//  - Start and stop sending and receiving.

#ifndef WEBRTC_VIDEO_ENGINE_INCLUDE_VIE_BASE_H_
#define WEBRTC_VIDEO_ENGINE_INCLUDE_VIE_BASE_H_

#include "webrtc/common_types.h"

#if defined(ANDROID) && !defined(WEBRTC_CHROMIUM_BUILD)
#include <jni.h>
#endif

namespace webrtc {

class Config;
class VoiceEngine;

// CpuOveruseObserver is called when a system overuse is detected and
// VideoEngine cannot keep up the encoding frequency.
class CpuOveruseObserver {
 public:
  // Called as soon as an overuse is detected.
  virtual void OveruseDetected() = 0;
  // Called periodically when the system is not overused any longer.
  virtual void NormalUsage() = 0;

 protected:
  virtual ~CpuOveruseObserver() {}
};


class WEBRTC_DLLEXPORT VideoEngine {
 public:
  // Creates a VideoEngine object, which can then be used to acquire sub‐APIs.
  static VideoEngine* Create();
  static VideoEngine* Create(const Config& config);

  // Deletes a VideoEngine instance.
  static bool Delete(VideoEngine*& video_engine);

  // Specifies the amount and type of trace information, which will be created
  // by the VideoEngine.
  static int SetTraceFilter(const unsigned int filter);

  // Sets the name of the trace file and enables non‐encrypted trace messages.
  static int SetTraceFile(const char* file_nameUTF8,
                          const bool add_file_counter = false);

  // Installs the TraceCallback implementation to ensure that the VideoEngine
  // user receives callbacks for generated trace messages.
  static int SetTraceCallback(TraceCallback* callback);

#if defined(ANDROID) && !defined(WEBRTC_CHROMIUM_BUILD)
  // Android specific.
  static int SetAndroidObjects(JavaVM* java_vm);
#endif

 protected:
  VideoEngine() {}
  virtual ~VideoEngine() {}
};

class WEBRTC_DLLEXPORT ViEBase {
 public:
  // Factory for the ViEBase sub‐API and increases an internal reference
  // counter if successful. Returns NULL if the API is not supported or if
  // construction fails.
  static ViEBase* GetInterface(VideoEngine* video_engine);

  // Releases the ViEBase sub-API and decreases an internal reference counter.
  // Returns the new reference count. This value should be zero
  // for all sub-API:s before the VideoEngine object can be safely deleted.
  virtual int Release() = 0;

  // Initiates all common parts of the VideoEngine.
  virtual int Init() = 0;

  // Connects a VideoEngine instance to a VoiceEngine instance for audio video
  // synchronization.
  virtual int SetVoiceEngine(VoiceEngine* voice_engine) = 0;

  // Creates a new channel.
  virtual int CreateChannel(int& video_channel) = 0;

  // Creates a new channel grouped together with |original_channel|. The channel
  // can both send and receive video. It is assumed the channel is sending
  // and/or receiving video to the same end-point.
  // Note: |CreateReceiveChannel| will give better performance and network
  // properties for receive only channels.
  virtual int CreateChannel(int& video_channel,
                            int original_channel) = 0;

  // Creates a new channel grouped together with |original_channel|. The channel
  // can only receive video and it is assumed the remote end-point is the same
  // as for |original_channel|.
  virtual int CreateReceiveChannel(int& video_channel,
                                   int original_channel) = 0;

  // Deletes an existing channel and releases the utilized resources.
  virtual int DeleteChannel(const int video_channel) = 0;

  // Registers an observer to be called when an overuse is detected, see
  // 'CpuOveruseObserver' for details.
  // NOTE: This is still very experimental functionality.
  virtual int RegisterCpuOveruseObserver(int channel,
                                         CpuOveruseObserver* observer) = 0;

  // Gets cpu overuse measures.
  // capture_jitter_ms: The current estimated jitter in ms based on incoming
  //                    captured frames.
  // avg_encode_time_ms: The average encode time in ms.
  // encode_usage_percent: The average encode time divided by the average time
  //                       difference between incoming captured frames.
  // capture_queue_delay_ms_per_s: The current time delay between an incoming
  //                               captured frame until the frame is being
  //                               processed. The delay is expressed in ms
  //                               delay per second.
  // TODO(asapersson): Remove default implementation.
  virtual int CpuOveruseMeasures(int channel,
                                 int* capture_jitter_ms,
                                 int* avg_encode_time_ms,
                                 int* encode_usage_percent,
                                 int* capture_queue_delay_ms_per_s) {
    return -1;
  }

  // Specifies the VoiceEngine and VideoEngine channel pair to use for
  // audio/video synchronization.
  virtual int ConnectAudioChannel(const int video_channel,
                                  const int audio_channel) = 0;

  // Disconnects a previously paired VideoEngine and VoiceEngine channel pair.
  virtual int DisconnectAudioChannel(const int video_channel) = 0;

  // Starts sending packets to an already specified IP address and port number
  // for a specified channel.
  virtual int StartSend(const int video_channel) = 0;

  // Stops packets from being sent for a specified channel.
  virtual int StopSend(const int video_channel) = 0;

  // Prepares VideoEngine for receiving packets on the specified channel.
  virtual int StartReceive(const int video_channel) = 0;

  // Stops receiving incoming RTP and RTCP packets on the specified channel.
  virtual int StopReceive(const int video_channel) = 0;

  // Retrieves the version information for VideoEngine and its components.
  virtual int GetVersion(char version[1024]) = 0;

  // Returns the last VideoEngine error code.
  virtual int LastError() = 0;

 protected:
  ViEBase() {}
  virtual ~ViEBase() {}
};

}  // namespace webrtc

#endif  // #define WEBRTC_VIDEO_ENGINE_MAIN_INTERFACE_VIE_BASE_H_
