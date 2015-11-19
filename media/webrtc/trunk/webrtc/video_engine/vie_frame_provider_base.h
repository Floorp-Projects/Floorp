/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_VIE_FRAME_PROVIDER_BASE_H_
#define WEBRTC_VIDEO_ENGINE_VIE_FRAME_PROVIDER_BASE_H_

#include <vector>

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/common_types.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class CriticalSectionWrapper;
class VideoEncoder;
class I420VideoFrame;

// ViEFrameCallback shall be implemented by all classes receiving frames from a
// frame provider.
class ViEFrameCallback {
 public:
  virtual void DeliverFrame(int id,
                            I420VideoFrame* video_frame,
                            const std::vector<uint32_t>& csrcs) = 0;

  // The capture delay has changed from the provider. |frame_delay| is given in
  // ms.
  virtual void DelayChanged(int id, int frame_delay) = 0;

  // Get the width, height and frame rate preferred by this observer.
  virtual int GetPreferedFrameSettings(int* width,
                                       int* height,
                                       int* frame_rate) = 0;

  // ProviderDestroyed is called when the frame is about to be destroyed. There
  // must not be any more calls to the frame provider after this.
  virtual void ProviderDestroyed(int id) = 0;

  virtual ~ViEFrameCallback() {}
};

// ViEFrameProviderBase is a base class that will deliver frames to all
// registered ViEFrameCallbacks.
class ViEFrameProviderBase {
 public:
  ViEFrameProviderBase(int Id, int engine_id);
  virtual ~ViEFrameProviderBase();

  // Returns the frame provider id.
  int Id() const;

  // Register frame callbacks, i.e. a receiver of the captured frame.
  // Must be called on the same thread as the provider was constructed on.
  int RegisterFrameCallback(int observer_id, ViEFrameCallback* callback);

  // Unregisters a previously registered callback.  Returns -1 if the callback
  // object hasn't been registered.
  // Must be called on the same thread as the provider was constructed on.
  int DeregisterFrameCallback(const ViEFrameCallback* callback);

  // Determines if a callback is currently registered.
  // Must be called on the same thread as the provider was constructed on.
  bool IsFrameCallbackRegistered(const ViEFrameCallback* callback);

  // FrameCallbackChanged
  // Inherited classes should check for new frame_settings and reconfigure
  // output if possible.
  virtual int FrameCallbackChanged() = 0;

 protected:
  void DeliverFrame(I420VideoFrame* video_frame,
                    const std::vector<uint32_t>& csrcs);
  void SetFrameDelay(int frame_delay);
  int FrameDelay();
  int GetBestFormat(int* best_width,
                    int* best_height,
                    int* best_frame_rate);

  rtc::ThreadChecker thread_checker_;
  rtc::ThreadChecker frame_delivery_thread_checker_;

  const int id_;
  const int engine_id_;

  // Frame callbacks.
  typedef std::vector<ViEFrameCallback*> FrameCallbacks;
  FrameCallbacks frame_callbacks_;
  const rtc::scoped_ptr<CriticalSectionWrapper> provider_cs_;

 private:
  rtc::scoped_ptr<I420VideoFrame> extra_frame_;
  int frame_delay_;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_VIE_FRAME_PROVIDER_BASE_H_
