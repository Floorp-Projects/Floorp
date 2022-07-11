/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/linux/wayland/shared_screencast_stream.h"

#include <libdrm/drm_fourcc.h>
#include <pipewire/pipewire.h>
#include <spa/param/format-utils.h>
#include <spa/param/props.h>
#include <spa/param/video/format-utils.h>
#include <spa/utils/result.h>
#include <sys/mman.h>

#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "modules/desktop_capture/linux/wayland/egl_dmabuf.h"
#include "modules/desktop_capture/screen_capture_frame_queue.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/sanitizer.h"
#include "rtc_base/string_encode.h"
#include "rtc_base/string_to_number.h"
#include "rtc_base/synchronization/mutex.h"

#if defined(WEBRTC_DLOPEN_PIPEWIRE)
#include "modules/desktop_capture/linux/wayland/pipewire_stubs.h"
using modules_desktop_capture_linux_wayland::InitializeStubs;
using modules_desktop_capture_linux_wayland::kModuleDrm;
using modules_desktop_capture_linux_wayland::kModulePipewire;
using modules_desktop_capture_linux_wayland::StubPathMap;
#endif  // defined(WEBRTC_DLOPEN_PIPEWIRE)

namespace webrtc {

const int kBytesPerPixel = 4;

#if defined(WEBRTC_DLOPEN_PIPEWIRE)
const char kPipeWireLib[] = "libpipewire-0.3.so.0";
const char kDrmLib[] = "libdrm.so.2";
#endif

#if !PW_CHECK_VERSION(0, 3, 29)
#define SPA_POD_PROP_FLAG_MANDATORY (1u << 3)
#endif
#if !PW_CHECK_VERSION(0, 3, 33)
#define SPA_POD_PROP_FLAG_DONT_FIXATE (1u << 4)
#endif

struct PipeWireVersion {
  int major = 0;
  int minor = 0;
  int micro = 0;
};

constexpr PipeWireVersion kDmaBufMinVersion = {0, 3, 24};
constexpr PipeWireVersion kDmaBufModifierMinVersion = {0, 3, 33};

PipeWireVersion ParsePipeWireVersion(const char* version) {
  std::vector<std::string> parsed_version;
  rtc::split(version, '.', &parsed_version);

  if (parsed_version.size() != 3) {
    return {};
  }

  absl::optional<int> major = rtc::StringToNumber<int>(parsed_version.at(0));
  absl::optional<int> minor = rtc::StringToNumber<int>(parsed_version.at(1));
  absl::optional<int> micro = rtc::StringToNumber<int>(parsed_version.at(2));

  // Return invalid version if we failed to parse it
  if (!major || !minor || !micro) {
    return {0, 0, 0};
  }

  return {major.value(), micro.value(), micro.value()};
}

spa_pod* BuildFormat(spa_pod_builder* builder,
                     uint32_t format,
                     const std::vector<uint64_t>& modifiers) {
  bool first = true;
  spa_pod_frame frames[2];
  spa_rectangle pw_min_screen_bounds = spa_rectangle{1, 1};
  spa_rectangle pw_max_screen_bounds = spa_rectangle{UINT32_MAX, UINT32_MAX};

  spa_pod_builder_push_object(builder, &frames[0], SPA_TYPE_OBJECT_Format,
                              SPA_PARAM_EnumFormat);
  spa_pod_builder_add(builder, SPA_FORMAT_mediaType,
                      SPA_POD_Id(SPA_MEDIA_TYPE_video), 0);
  spa_pod_builder_add(builder, SPA_FORMAT_mediaSubtype,
                      SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw), 0);
  spa_pod_builder_add(builder, SPA_FORMAT_VIDEO_format, SPA_POD_Id(format), 0);

  if (modifiers.size()) {
    spa_pod_builder_prop(
        builder, SPA_FORMAT_VIDEO_modifier,
        SPA_POD_PROP_FLAG_MANDATORY | SPA_POD_PROP_FLAG_DONT_FIXATE);
    spa_pod_builder_push_choice(builder, &frames[1], SPA_CHOICE_Enum, 0);
    // modifiers from the array
    for (int64_t val : modifiers) {
      spa_pod_builder_long(builder, val);
      // Add the first modifier twice as the very first value is the default
      // option
      if (first) {
        spa_pod_builder_long(builder, val);
        first = false;
      }
    }
    spa_pod_builder_pop(builder, &frames[1]);
  }

  spa_pod_builder_add(
      builder, SPA_FORMAT_VIDEO_size,
      SPA_POD_CHOICE_RANGE_Rectangle(
          &pw_min_screen_bounds, &pw_min_screen_bounds, &pw_max_screen_bounds),
      0);

  return static_cast<spa_pod*>(spa_pod_builder_pop(builder, &frames[0]));
}

class PipeWireThreadLoopLock {
 public:
  explicit PipeWireThreadLoopLock(pw_thread_loop* loop) : loop_(loop) {
    pw_thread_loop_lock(loop_);
  }
  ~PipeWireThreadLoopLock() { pw_thread_loop_unlock(loop_); }

 private:
  pw_thread_loop* const loop_;
};

class ScopedBuf {
 public:
  ScopedBuf() {}
  ScopedBuf(uint8_t* map, int map_size, int fd)
      : map_(map), map_size_(map_size), fd_(fd) {}
  ~ScopedBuf() {
    if (map_ != MAP_FAILED) {
      munmap(map_, map_size_);
    }
  }

  operator bool() { return map_ != MAP_FAILED; }

  void initialize(uint8_t* map, int map_size, int fd) {
    map_ = map;
    map_size_ = map_size;
    fd_ = fd;
  }

  uint8_t* get() { return map_; }

 protected:
  uint8_t* map_ = static_cast<uint8_t*>(MAP_FAILED);
  int map_size_;
  int fd_;
};

class SharedScreenCastStreamPrivate {
 public:
  SharedScreenCastStreamPrivate();
  ~SharedScreenCastStreamPrivate();

  bool StartScreenCastStream(uint32_t stream_node_id, int fd);
  void StopScreenCastStream();
  std::unique_ptr<DesktopFrame> CaptureFrame();

 private:
  uint32_t pw_stream_node_id_ = 0;
  int pw_fd_ = -1;

  DesktopSize desktop_size_ = {};
  DesktopSize video_size_;

  webrtc::Mutex queue_lock_;
  ScreenCaptureFrameQueue<SharedDesktopFrame> queue_
      RTC_GUARDED_BY(&queue_lock_);

  int64_t modifier_;
  std::unique_ptr<EglDmaBuf> egl_dmabuf_;

  // PipeWire types
  struct pw_context* pw_context_ = nullptr;
  struct pw_core* pw_core_ = nullptr;
  struct pw_stream* pw_stream_ = nullptr;
  struct pw_thread_loop* pw_main_loop_ = nullptr;

  spa_hook spa_core_listener_;
  spa_hook spa_stream_listener_;

  // A number used to verify all previous methods and the resulting
  // events have been handled.
  int server_version_sync_ = 0;
  // Version of the running PipeWire server we communicate with
  PipeWireVersion pw_server_version_;
  // Version of the library used to run our code
  PipeWireVersion pw_client_version_;

  // event handlers
  pw_core_events pw_core_events_ = {};
  pw_stream_events pw_stream_events_ = {};

  struct spa_video_info_raw spa_video_format_;

  void ProcessBuffer(pw_buffer* buffer);
  void ConvertRGBxToBGRx(uint8_t* frame, uint32_t size);

  // PipeWire callbacks
  static void OnCoreError(void* data,
                          uint32_t id,
                          int seq,
                          int res,
                          const char* message);
  static void OnCoreDone(void* user_data, uint32_t id, int seq);
  static void OnCoreInfo(void* user_data, const pw_core_info* info);
  static void OnStreamParamChanged(void* data,
                                   uint32_t id,
                                   const struct spa_pod* format);
  static void OnStreamStateChanged(void* data,
                                   pw_stream_state old_state,
                                   pw_stream_state state,
                                   const char* error_message);
  static void OnStreamProcess(void* data);
};

bool operator>=(const PipeWireVersion& current_pw_version,
                const PipeWireVersion& required_pw_version) {
  if (!current_pw_version.major && !current_pw_version.minor &&
      !current_pw_version.micro) {
    return false;
  }

  return std::tie(current_pw_version.major, current_pw_version.minor,
                  current_pw_version.micro) >=
         std::tie(required_pw_version.major, required_pw_version.minor,
                  required_pw_version.micro);
}

bool operator<=(const PipeWireVersion& current_pw_version,
                const PipeWireVersion& required_pw_version) {
  if (!current_pw_version.major && !current_pw_version.minor &&
      !current_pw_version.micro) {
    return false;
  }

  return std::tie(current_pw_version.major, current_pw_version.minor,
                  current_pw_version.micro) <=
         std::tie(required_pw_version.major, required_pw_version.minor,
                  required_pw_version.micro);
}

void SharedScreenCastStreamPrivate::OnCoreError(void* data,
                                                uint32_t id,
                                                int seq,
                                                int res,
                                                const char* message) {
  SharedScreenCastStreamPrivate* that =
      static_cast<SharedScreenCastStreamPrivate*>(data);
  RTC_DCHECK(that);

  RTC_LOG(LS_ERROR) << "PipeWire remote error: " << message;
}

void SharedScreenCastStreamPrivate::OnCoreInfo(void* data,
                                               const pw_core_info* info) {
  SharedScreenCastStreamPrivate* stream =
      static_cast<SharedScreenCastStreamPrivate*>(data);
  RTC_DCHECK(stream);

  stream->pw_server_version_ = ParsePipeWireVersion(info->version);
}

void SharedScreenCastStreamPrivate::OnCoreDone(void* data,
                                               uint32_t id,
                                               int seq) {
  const SharedScreenCastStreamPrivate* stream =
      static_cast<SharedScreenCastStreamPrivate*>(data);
  RTC_DCHECK(stream);

  if (id == PW_ID_CORE && stream->server_version_sync_ == seq) {
    pw_thread_loop_signal(stream->pw_main_loop_, false);
  }
}

// static
void SharedScreenCastStreamPrivate::OnStreamStateChanged(
    void* data,
    pw_stream_state old_state,
    pw_stream_state state,
    const char* error_message) {
  SharedScreenCastStreamPrivate* that =
      static_cast<SharedScreenCastStreamPrivate*>(data);
  RTC_DCHECK(that);

  switch (state) {
    case PW_STREAM_STATE_ERROR:
      RTC_LOG(LS_ERROR) << "PipeWire stream state error: " << error_message;
      break;
    case PW_STREAM_STATE_PAUSED:
    case PW_STREAM_STATE_STREAMING:
    case PW_STREAM_STATE_UNCONNECTED:
    case PW_STREAM_STATE_CONNECTING:
      break;
  }
}

// static
void SharedScreenCastStreamPrivate::OnStreamParamChanged(
    void* data,
    uint32_t id,
    const struct spa_pod* format) {
  SharedScreenCastStreamPrivate* that =
      static_cast<SharedScreenCastStreamPrivate*>(data);
  RTC_DCHECK(that);

  RTC_LOG(LS_INFO) << "PipeWire stream format changed.";
  if (!format || id != SPA_PARAM_Format) {
    return;
  }

  spa_format_video_raw_parse(format, &that->spa_video_format_);

  auto width = that->spa_video_format_.size.width;
  auto height = that->spa_video_format_.size.height;
  auto stride = SPA_ROUND_UP_N(width * kBytesPerPixel, 4);
  auto size = height * stride;

  that->desktop_size_ = DesktopSize(width, height);

  uint8_t buffer[1024] = {};
  auto builder = spa_pod_builder{buffer, sizeof(buffer)};

  // Setup buffers and meta header for new format.

  // When SPA_FORMAT_VIDEO_modifier is present we can use DMA-BUFs as
  // the server announces support for it.
  // See https://github.com/PipeWire/pipewire/blob/master/doc/dma-buf.dox
  const bool has_modifier =
      spa_pod_find_prop(format, nullptr, SPA_FORMAT_VIDEO_modifier);
  that->modifier_ =
      has_modifier ? that->spa_video_format_.modifier : DRM_FORMAT_MOD_INVALID;
  std::vector<const spa_pod*> params;
  const int buffer_types =
      has_modifier || (that->pw_server_version_ >= kDmaBufMinVersion)
          ? (1 << SPA_DATA_DmaBuf) | (1 << SPA_DATA_MemFd) |
                (1 << SPA_DATA_MemPtr)
          : (1 << SPA_DATA_MemFd) | (1 << SPA_DATA_MemPtr);

  params.push_back(reinterpret_cast<spa_pod*>(spa_pod_builder_add_object(
      &builder, SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
      SPA_PARAM_BUFFERS_size, SPA_POD_Int(size), SPA_PARAM_BUFFERS_stride,
      SPA_POD_Int(stride), SPA_PARAM_BUFFERS_buffers,
      SPA_POD_CHOICE_RANGE_Int(8, 1, 32), SPA_PARAM_BUFFERS_dataType,
      SPA_POD_CHOICE_FLAGS_Int(buffer_types))));
  params.push_back(reinterpret_cast<spa_pod*>(spa_pod_builder_add_object(
      &builder, SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta, SPA_PARAM_META_type,
      SPA_POD_Id(SPA_META_Header), SPA_PARAM_META_size,
      SPA_POD_Int(sizeof(struct spa_meta_header)))));
  params.push_back(reinterpret_cast<spa_pod*>(spa_pod_builder_add_object(
      &builder, SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta, SPA_PARAM_META_type,
      SPA_POD_Id(SPA_META_VideoCrop), SPA_PARAM_META_size,
      SPA_POD_Int(sizeof(struct spa_meta_region)))));
  pw_stream_update_params(that->pw_stream_, params.data(), params.size());
}

// static
void SharedScreenCastStreamPrivate::OnStreamProcess(void* data) {
  SharedScreenCastStreamPrivate* that =
      static_cast<SharedScreenCastStreamPrivate*>(data);
  RTC_DCHECK(that);

  struct pw_buffer* next_buffer;
  struct pw_buffer* buffer = nullptr;

  next_buffer = pw_stream_dequeue_buffer(that->pw_stream_);
  while (next_buffer) {
    buffer = next_buffer;
    next_buffer = pw_stream_dequeue_buffer(that->pw_stream_);

    if (next_buffer) {
      pw_stream_queue_buffer(that->pw_stream_, buffer);
    }
  }

  if (!buffer) {
    return;
  }

  that->ProcessBuffer(buffer);

  pw_stream_queue_buffer(that->pw_stream_, buffer);
}

SharedScreenCastStreamPrivate::SharedScreenCastStreamPrivate() {}

SharedScreenCastStreamPrivate::~SharedScreenCastStreamPrivate() {
  if (pw_main_loop_) {
    pw_thread_loop_stop(pw_main_loop_);
  }

  if (pw_stream_) {
    pw_stream_destroy(pw_stream_);
  }

  if (pw_core_) {
    pw_core_disconnect(pw_core_);
  }

  if (pw_context_) {
    pw_context_destroy(pw_context_);
  }

  if (pw_main_loop_) {
    pw_thread_loop_destroy(pw_main_loop_);
  }
}

RTC_NO_SANITIZE("cfi-icall")
bool SharedScreenCastStreamPrivate::StartScreenCastStream(
    uint32_t stream_node_id,
    int fd) {
#if defined(WEBRTC_DLOPEN_PIPEWIRE)
  StubPathMap paths;

  // Check if the PipeWire and DRM libraries are available.
  paths[kModulePipewire].push_back(kPipeWireLib);
  paths[kModuleDrm].push_back(kDrmLib);
  if (!InitializeStubs(paths)) {
    RTC_LOG(LS_ERROR) << "Failed to load the PipeWire library and symbols.";
    return false;
  }
#endif  // defined(WEBRTC_DLOPEN_PIPEWIRE)
  egl_dmabuf_ = std::make_unique<EglDmaBuf>();

  pw_stream_node_id_ = stream_node_id;
  pw_fd_ = fd;

  pw_init(/*argc=*/nullptr, /*argc=*/nullptr);

  pw_main_loop_ = pw_thread_loop_new("pipewire-main-loop", nullptr);

  pw_context_ =
      pw_context_new(pw_thread_loop_get_loop(pw_main_loop_), nullptr, 0);
  if (!pw_context_) {
    RTC_LOG(LS_ERROR) << "Failed to create PipeWire context";
    return false;
  }

  if (pw_thread_loop_start(pw_main_loop_) < 0) {
    RTC_LOG(LS_ERROR) << "Failed to start main PipeWire loop";
    return false;
  }

  pw_client_version_ = ParsePipeWireVersion(pw_get_library_version());

  // Initialize event handlers, remote end and stream-related.
  pw_core_events_.version = PW_VERSION_CORE_EVENTS;
  pw_core_events_.info = &OnCoreInfo;
  pw_core_events_.done = &OnCoreDone;
  pw_core_events_.error = &OnCoreError;

  pw_stream_events_.version = PW_VERSION_STREAM_EVENTS;
  pw_stream_events_.state_changed = &OnStreamStateChanged;
  pw_stream_events_.param_changed = &OnStreamParamChanged;
  pw_stream_events_.process = &OnStreamProcess;

  {
    PipeWireThreadLoopLock thread_loop_lock(pw_main_loop_);

    pw_core_ = pw_context_connect_fd(pw_context_, pw_fd_, nullptr, 0);
    if (!pw_core_) {
      RTC_LOG(LS_ERROR) << "Failed to connect PipeWire context";
      return false;
    }

    pw_core_add_listener(pw_core_, &spa_core_listener_, &pw_core_events_, this);

    server_version_sync_ =
        pw_core_sync(pw_core_, PW_ID_CORE, server_version_sync_);

    pw_thread_loop_wait(pw_main_loop_);

    pw_properties* reuseProps =
        pw_properties_new_string("pipewire.client.reuse=1");
    pw_stream_ = pw_stream_new(pw_core_, "webrtc-consume-stream", reuseProps);

    if (!pw_stream_) {
      RTC_LOG(LS_ERROR) << "Failed to create PipeWire stream";
      return false;
    }

    pw_stream_add_listener(pw_stream_, &spa_stream_listener_,
                           &pw_stream_events_, this);
    uint8_t buffer[2048] = {};
    std::vector<uint64_t> modifiers;

    spa_pod_builder builder = spa_pod_builder{buffer, sizeof(buffer)};

    std::vector<const spa_pod*> params;
    const bool has_required_pw_client_version =
        pw_client_version_ >= kDmaBufModifierMinVersion;
    const bool has_required_pw_server_version =
        pw_server_version_ >= kDmaBufModifierMinVersion;
    for (uint32_t format : {SPA_VIDEO_FORMAT_BGRA, SPA_VIDEO_FORMAT_RGBA,
                            SPA_VIDEO_FORMAT_BGRx, SPA_VIDEO_FORMAT_RGBx}) {
      // Modifiers can be used with PipeWire >= 0.3.33
      if (has_required_pw_client_version && has_required_pw_server_version) {
        modifiers = egl_dmabuf_->QueryDmaBufModifiers(format);

        if (!modifiers.empty()) {
          params.push_back(BuildFormat(&builder, format, modifiers));
        }
      }

      params.push_back(BuildFormat(&builder, format, /*modifiers=*/{}));
    }

    if (pw_stream_connect(pw_stream_, PW_DIRECTION_INPUT, pw_stream_node_id_,
                          PW_STREAM_FLAG_AUTOCONNECT, params.data(),
                          params.size()) != 0) {
      RTC_LOG(LS_ERROR) << "Could not connect receiving stream.";
      return false;
    }

    RTC_LOG(LS_INFO) << "PipeWire remote opened.";
  }
  return true;
}

void SharedScreenCastStreamPrivate::StopScreenCastStream() {
  if (pw_stream_) {
    pw_stream_disconnect(pw_stream_);
  }
}

std::unique_ptr<DesktopFrame> SharedScreenCastStreamPrivate::CaptureFrame() {
  webrtc::MutexLock lock(&queue_lock_);

  if (!queue_.current_frame()) {
    return std::unique_ptr<DesktopFrame>{};
  }

  std::unique_ptr<SharedDesktopFrame> frame = queue_.current_frame()->Share();
  return std::move(frame);
}

void SharedScreenCastStreamPrivate::ProcessBuffer(pw_buffer* buffer) {
  spa_buffer* spa_buffer = buffer->buffer;
  ScopedBuf map;
  std::unique_ptr<uint8_t[]> src_unique_ptr;
  uint8_t* src = nullptr;

  if (spa_buffer->datas[0].chunk->size == 0) {
    RTC_LOG(LS_ERROR) << "Failed to get video stream: Zero size.";
    return;
  }

  if (spa_buffer->datas[0].type == SPA_DATA_MemFd) {
    map.initialize(
        static_cast<uint8_t*>(
            mmap(nullptr,
                 spa_buffer->datas[0].maxsize + spa_buffer->datas[0].mapoffset,
                 PROT_READ, MAP_PRIVATE, spa_buffer->datas[0].fd, 0)),
        spa_buffer->datas[0].maxsize + spa_buffer->datas[0].mapoffset,
        spa_buffer->datas[0].fd);

    if (!map) {
      RTC_LOG(LS_ERROR) << "Failed to mmap the memory: "
                        << std::strerror(errno);
      return;
    }

    src = SPA_MEMBER(map.get(), spa_buffer->datas[0].mapoffset, uint8_t);
  } else if (spa_buffer->datas[0].type == SPA_DATA_DmaBuf) {
    const uint n_planes = spa_buffer->n_datas;

    if (!n_planes) {
      return;
    }

    std::vector<EglDmaBuf::PlaneData> plane_datas;
    for (uint32_t i = 0; i < n_planes; ++i) {
      EglDmaBuf::PlaneData data = {
          static_cast<int32_t>(spa_buffer->datas[i].fd),
          static_cast<uint32_t>(spa_buffer->datas[i].chunk->stride),
          static_cast<uint32_t>(spa_buffer->datas[i].chunk->offset)};
      plane_datas.push_back(data);
    }

    src_unique_ptr = egl_dmabuf_->ImageFromDmaBuf(
        desktop_size_, spa_video_format_.format, plane_datas, modifier_);
    src = src_unique_ptr.get();
  } else if (spa_buffer->datas[0].type == SPA_DATA_MemPtr) {
    src = static_cast<uint8_t*>(spa_buffer->datas[0].data);
  }

  if (!src) {
    return;
  }

  struct spa_meta_region* video_metadata =
      static_cast<struct spa_meta_region*>(spa_buffer_find_meta_data(
          spa_buffer, SPA_META_VideoCrop, sizeof(*video_metadata)));

  // Video size from metadata is bigger than an actual video stream size.
  // The metadata are wrong or we should up-scale the video...in both cases
  // just quit now.
  if (video_metadata && (video_metadata->region.size.width >
                             static_cast<uint32_t>(desktop_size_.width()) ||
                         video_metadata->region.size.height >
                             static_cast<uint32_t>(desktop_size_.height()))) {
    RTC_LOG(LS_ERROR) << "Stream metadata sizes are wrong!";
    return;
  }

  // Use video metadata when video size from metadata is set and smaller than
  // video stream size, so we need to adjust it.
  bool video_metadata_use = false;
  const struct spa_rectangle* video_metadata_size =
      video_metadata ? &video_metadata->region.size : nullptr;

  if (video_metadata_size && video_metadata_size->width != 0 &&
      video_metadata_size->height != 0 &&
      (static_cast<int>(video_metadata_size->width) < desktop_size_.width() ||
       static_cast<int>(video_metadata_size->height) <
           desktop_size_.height())) {
    video_metadata_use = true;
  }

  if (video_metadata_use) {
    video_size_ =
        DesktopSize(video_metadata_size->width, video_metadata_size->height);
  } else {
    video_size_ = desktop_size_;
  }

  uint32_t y_offset = video_metadata_use && (video_metadata->region.position.y +
                                                 video_size_.height() <=
                                             desktop_size_.height())
                          ? video_metadata->region.position.y
                          : 0;
  uint32_t x_offset = video_metadata_use && (video_metadata->region.position.x +
                                                 video_size_.width() <=
                                             desktop_size_.width())
                          ? video_metadata->region.position.x
                          : 0;

  uint8_t* updated_src = src + (spa_buffer->datas[0].chunk->stride * y_offset) +
                         (kBytesPerPixel * x_offset);

  webrtc::MutexLock lock(&queue_lock_);

  // Move to the next frame if the current one is being used and shared
  if (queue_.current_frame() && queue_.current_frame()->IsShared()) {
    queue_.MoveToNextFrame();
    if (queue_.current_frame() && queue_.current_frame()->IsShared()) {
      RTC_LOG(LS_WARNING)
          << "Failed to process PipeWire buffer: no available frame";
      return;
    }
  }

  if (!queue_.current_frame() ||
      !queue_.current_frame()->size().equals(video_size_)) {
    std::unique_ptr<DesktopFrame> frame(new BasicDesktopFrame(
        DesktopSize(video_size_.width(), video_size_.height())));
    queue_.ReplaceCurrentFrame(SharedDesktopFrame::Wrap(std::move(frame)));
  }

  queue_.current_frame()->CopyPixelsFrom(
      updated_src,
      (spa_buffer->datas[0].chunk->stride - (kBytesPerPixel * x_offset)),
      DesktopRect::MakeWH(video_size_.width(), video_size_.height()));

  if (spa_video_format_.format == SPA_VIDEO_FORMAT_RGBx ||
      spa_video_format_.format == SPA_VIDEO_FORMAT_RGBA) {
    uint8_t* tmp_src = queue_.current_frame()->data();
    for (int i = 0; i < video_size_.height(); ++i) {
      // If both sides decided to go with the RGBx format we need to convert it
      // to BGRx to match color format expected by WebRTC.
      ConvertRGBxToBGRx(tmp_src, queue_.current_frame()->stride());
      tmp_src += queue_.current_frame()->stride();
    }
  }
}

void SharedScreenCastStreamPrivate::ConvertRGBxToBGRx(uint8_t* frame,
                                                      uint32_t size) {
  for (uint32_t i = 0; i < size; i += 4) {
    uint8_t tempR = frame[i];
    uint8_t tempB = frame[i + 2];
    frame[i] = tempB;
    frame[i + 2] = tempR;
  }
}

SharedScreenCastStream::SharedScreenCastStream()
    : private_(std::make_unique<SharedScreenCastStreamPrivate>()) {}

SharedScreenCastStream::~SharedScreenCastStream() {}

rtc::scoped_refptr<SharedScreenCastStream>
SharedScreenCastStream::CreateDefault() {
  // Explicit new, to access non-public constructor.
  return rtc::scoped_refptr(new SharedScreenCastStream());
}

bool SharedScreenCastStream::StartScreenCastStream(uint32_t stream_node_id,
                                                   int fd) {
  return private_->StartScreenCastStream(stream_node_id, fd);
}

void SharedScreenCastStream::StopScreenCastStream() {
  private_->StopScreenCastStream();
}

std::unique_ptr<DesktopFrame> SharedScreenCastStream::CaptureFrame() {
  return private_->CaptureFrame();
}

}  // namespace webrtc
