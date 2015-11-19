/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_TEST_LIBVIETEST_INCLUDE_VIE_TO_FILE_RENDERER_H_
#define WEBRTC_VIDEO_ENGINE_TEST_LIBVIETEST_INCLUDE_VIE_TO_FILE_RENDERER_H_

#include <stdio.h>
#include <string.h>

#include <list>
#include <string>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/video_engine/include/vie_render.h"

namespace webrtc {
class CriticalSectionWrapper;
class EventWrapper;
class ThreadWrapper;
};  // namespace webrtc

namespace test {
struct Frame;
};  // namespace test

class ViEToFileRenderer: public webrtc::ExternalRenderer {
 public:
  ViEToFileRenderer();
  virtual ~ViEToFileRenderer();

  // Returns false if we fail opening the output filename for writing.
  bool PrepareForRendering(const std::string& output_path,
                           const std::string& output_filename);

  // Closes the output file.
  void StopRendering();

  // Deletes the closed output file from the file system. This is one option
  // after calling StopRendering, the other being KeepOutputFile. This file
  // renderer will forget about the file after this call and can be used again.
  bool DeleteOutputFile();

  // Renames the closed output file to its previous name with the provided
  // prefix prepended. This file renderer will forget about the file after this
  // call and can be used again.
  bool SaveOutputFile(const std::string& prefix);

  // Implementation of ExternalRenderer:
  int FrameSizeChange(unsigned int width,
                      unsigned int height,
                      unsigned int number_of_streams) override;

  int DeliverFrame(unsigned char* buffer,
                   size_t buffer_size,
                   uint32_t time_stamp,
                   int64_t ntp_time_ms,
                   int64_t render_time,
                   void* handle) override;

  int DeliverI420Frame(const webrtc::I420VideoFrame& webrtc_frame) override;

  bool IsTextureSupported() override;

  const std::string GetFullOutputPath() const;

 private:
  typedef std::list<test::Frame*> FrameQueue;

  // Returns a frame with the specified |buffer_size|. Tries to avoid allocating
  // new frames by reusing frames from |free_frame_queue_|.
  test::Frame* NewFrame(size_t buffer_size);
  static bool RunRenderThread(void* obj);
  void ForgetOutputFile();
  bool ProcessRenderQueue();

  FILE* output_file_;
  std::string output_path_;
  std::string output_filename_;
  rtc::scoped_ptr<webrtc::ThreadWrapper> thread_;
  rtc::scoped_ptr<webrtc::CriticalSectionWrapper> frame_queue_cs_;
  rtc::scoped_ptr<webrtc::EventWrapper> frame_render_event_;
  FrameQueue render_queue_;
  FrameQueue free_frame_queue_;
};

#endif  // WEBRTC_VIDEO_ENGINE_TEST_LIBVIETEST_INCLUDE_VIE_TO_FILE_RENDERER_H_
