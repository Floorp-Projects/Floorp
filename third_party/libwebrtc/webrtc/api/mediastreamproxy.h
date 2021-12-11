/*
 *  Copyright 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_MEDIASTREAMPROXY_H_
#define API_MEDIASTREAMPROXY_H_

#include <string>

#include "api/mediastreaminterface.h"
#include "api/proxy.h"

namespace webrtc {

// TODO(deadbeef): Move this to .cc file and out of api/. What threads methods
// are called on is an implementation detail.
BEGIN_SIGNALING_PROXY_MAP(MediaStream)
  PROXY_SIGNALING_THREAD_DESTRUCTOR()
  PROXY_CONSTMETHOD0(std::string, label)
  PROXY_METHOD0(AudioTrackVector, GetAudioTracks)
  PROXY_METHOD0(VideoTrackVector, GetVideoTracks)
  PROXY_METHOD1(rtc::scoped_refptr<AudioTrackInterface>,
                FindAudioTrack,
                const std::string&)
  PROXY_METHOD1(rtc::scoped_refptr<VideoTrackInterface>,
                FindVideoTrack,
                const std::string&)
  PROXY_METHOD1(bool, AddTrack, AudioTrackInterface*)
  PROXY_METHOD1(bool, AddTrack, VideoTrackInterface*)
  PROXY_METHOD1(bool, RemoveTrack, AudioTrackInterface*)
  PROXY_METHOD1(bool, RemoveTrack, VideoTrackInterface*)
  PROXY_METHOD1(void, RegisterObserver, ObserverInterface*)
  PROXY_METHOD1(void, UnregisterObserver, ObserverInterface*)
END_PROXY_MAP()

}  // namespace webrtc

#endif  // API_MEDIASTREAMPROXY_H_
