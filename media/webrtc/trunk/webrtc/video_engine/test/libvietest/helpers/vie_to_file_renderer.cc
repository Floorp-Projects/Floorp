/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/test/libvietest/include/vie_to_file_renderer.h"

#include <assert.h>

#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/thread_wrapper.h"

namespace test {
struct Frame {
 public:
  Frame(unsigned char* buffer,
        int buffer_size,
        uint32_t timestamp,
        int64_t render_time)
      : buffer(new unsigned char[buffer_size]),
        buffer_size(buffer_size),
        timestamp(timestamp),
        render_time(render_time) {
    memcpy(this->buffer.get(), buffer, buffer_size);
  }

  webrtc::scoped_array<unsigned char> buffer;
  int buffer_size;
  uint32_t timestamp;
  int64_t render_time;

 private:
  DISALLOW_COPY_AND_ASSIGN(Frame);
};
};  // namespace test

ViEToFileRenderer::ViEToFileRenderer()
    : output_file_(NULL),
      output_path_(),
      output_filename_(),
      thread_(webrtc::ThreadWrapper::CreateThread(
          ViEToFileRenderer::RunRenderThread,
          this, webrtc::kNormalPriority, "ViEToFileRendererThread")),
      frame_queue_cs_(webrtc::CriticalSectionWrapper::CreateCriticalSection()),
      frame_render_event_(webrtc::EventWrapper::Create()),
      render_queue_(),
      free_frame_queue_() {
}

ViEToFileRenderer::~ViEToFileRenderer() {
  while (!free_frame_queue_.empty()) {
    delete free_frame_queue_.front();
    free_frame_queue_.pop_front();
  }
}

bool ViEToFileRenderer::PrepareForRendering(
    const std::string& output_path,
    const std::string& output_filename) {

  assert(output_file_ == NULL);

  output_file_ = fopen((output_path + output_filename).c_str(), "wb");
  if (output_file_ == NULL) {
    return false;
  }

  output_filename_ = output_filename;
  output_path_ = output_path;
  unsigned int tid;
  return thread_->Start(tid);
}

void ViEToFileRenderer::StopRendering() {
  assert(output_file_ != NULL);
  if (thread_.get() != NULL) {
    thread_->SetNotAlive();
    // Signal that a frame is ready to be written to file.
    frame_render_event_->Set();
    // Call Stop() repeatedly, waiting for ProcessRenderQueue() to finish.
    while (!thread_->Stop()) continue;
  }
  fclose(output_file_);
  output_file_ = NULL;
}

bool ViEToFileRenderer::SaveOutputFile(const std::string& prefix) {
  assert(output_file_ == NULL && output_filename_ != "");
  if (rename((output_path_ + output_filename_).c_str(),
                  (output_path_ + prefix + output_filename_).c_str()) != 0) {
    perror("Failed to rename output file");
    return false;
  }
  ForgetOutputFile();
  return true;
}

bool ViEToFileRenderer::DeleteOutputFile() {
  assert(output_file_ == NULL && output_filename_ != "");
  if (remove((output_path_ + output_filename_).c_str()) != 0) {
    perror("Failed to delete output file");
    return false;
  }
  ForgetOutputFile();
  return true;
}

const std::string ViEToFileRenderer::GetFullOutputPath() const {
  return output_path_ + output_filename_;
}

void ViEToFileRenderer::ForgetOutputFile() {
  output_filename_ = "";
  output_path_ = "";
}

int ViEToFileRenderer::DeliverFrame(unsigned char *buffer,
                                    int buffer_size,
                                    uint32_t time_stamp,
                                    int64_t render_time,
                                    void* /*handle*/) {
  webrtc::CriticalSectionScoped lock(frame_queue_cs_.get());
  test::Frame* frame;
  if (free_frame_queue_.empty()) {
    frame = new test::Frame(buffer, buffer_size, time_stamp, render_time);
  } else {
    // Reuse an already allocated frame.
    frame = free_frame_queue_.front();
    free_frame_queue_.pop_front();
    if (frame->buffer_size < buffer_size) {
      frame->buffer.reset(new unsigned char[buffer_size]);
    }
    memcpy(frame->buffer.get(), buffer, buffer_size);
    frame->buffer_size = buffer_size;
    frame->timestamp = time_stamp;
    frame->render_time = render_time;
  }
  render_queue_.push_back(frame);
  // Signal that a frame is ready to be written to file.
  frame_render_event_->Set();
  return 0;
}

bool ViEToFileRenderer::IsTextureSupported() { return false; }

int ViEToFileRenderer::FrameSizeChange(unsigned int width,
                                       unsigned int height,
                                       unsigned int number_of_streams) {
  return 0;
}

bool ViEToFileRenderer::RunRenderThread(void* obj) {
  assert(obj);
  ViEToFileRenderer* renderer = static_cast<ViEToFileRenderer*>(obj);
  return renderer->ProcessRenderQueue();
}

bool ViEToFileRenderer::ProcessRenderQueue() {
  // Wait for a frame to be rendered.
  frame_render_event_->Wait(WEBRTC_EVENT_INFINITE);
  frame_queue_cs_->Enter();
  // Render all frames in the queue.
  while (!render_queue_.empty()) {
    test::Frame* frame = render_queue_.front();
    render_queue_.pop_front();
    // Leave the critical section before writing to file to not block calls to
    // the renderer.
    frame_queue_cs_->Leave();
    assert(output_file_);
    int written = fwrite(frame->buffer.get(), sizeof(unsigned char),
                              frame->buffer_size, output_file_);
    frame_queue_cs_->Enter();
    // Return the frame.
    free_frame_queue_.push_front(frame);
    if (written != frame->buffer_size) {
      frame_queue_cs_->Leave();
      return false;
    }
  }
  frame_queue_cs_->Leave();
  return true;
}
