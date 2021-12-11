/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEOSOURCEPROXY_H_
#define API_VIDEOSOURCEPROXY_H_

#include "api/proxy.h"
#include "api/mediastreaminterface.h"

namespace webrtc {

// Makes sure the real VideoTrackSourceInterface implementation is destroyed on
// the signaling thread and marshals all method calls to the signaling thread.
// TODO(deadbeef): Move this to .cc file and out of api/. What threads methods
// are called on is an implementation detail.
BEGIN_PROXY_MAP(VideoTrackSource)
  PROXY_SIGNALING_THREAD_DESTRUCTOR()
  PROXY_CONSTMETHOD0(SourceState, state)
  PROXY_CONSTMETHOD0(bool, remote)
  PROXY_CONSTMETHOD0(bool, is_screencast)
  PROXY_CONSTMETHOD0(rtc::Optional<bool>, needs_denoising)
  PROXY_METHOD1(bool, GetStats, Stats*)
  PROXY_WORKER_METHOD2(void,
                       AddOrUpdateSink,
                       rtc::VideoSinkInterface<VideoFrame>*,
                       const rtc::VideoSinkWants&)
  PROXY_WORKER_METHOD1(void, RemoveSink, rtc::VideoSinkInterface<VideoFrame>*)
  PROXY_METHOD1(void, RegisterObserver, ObserverInterface*)
  PROXY_METHOD1(void, UnregisterObserver, ObserverInterface*)
END_PROXY_MAP()

}  // namespace webrtc

#endif  // API_VIDEOSOURCEPROXY_H_
