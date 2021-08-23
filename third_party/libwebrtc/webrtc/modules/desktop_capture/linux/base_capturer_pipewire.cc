/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/linux/base_capturer_pipewire.h"

#include <gio/gunixfdlist.h>
#include <glib-object.h>

#include <spa/param/format-utils.h>
#include <spa/param/props.h>

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>

#include <cstring>
#include <memory>
#include <utility>

#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

const char kDesktopBusName[] = "org.freedesktop.portal.Desktop";
const char kDesktopObjectPath[] = "/org/freedesktop/portal/desktop";
const char kDesktopRequestObjectPath[] =
    "/org/freedesktop/portal/desktop/request";
const char kSessionInterfaceName[] = "org.freedesktop.portal.Session";
const char kRequestInterfaceName[] = "org.freedesktop.portal.Request";
const char kScreenCastInterfaceName[] = "org.freedesktop.portal.ScreenCast";

// static
struct dma_buf_sync {
  uint64_t flags;
};
#define DMA_BUF_SYNC_READ      (1 << 0)
#define DMA_BUF_SYNC_START     (0 << 2)
#define DMA_BUF_SYNC_END       (1 << 2)
#define DMA_BUF_BASE           'b'
#define DMA_BUF_IOCTL_SYNC     _IOW(DMA_BUF_BASE, 0, struct dma_buf_sync)

static void SyncDmaBuf(int fd, uint64_t start_or_end) {
  struct dma_buf_sync sync = { 0 };

  sync.flags = start_or_end | DMA_BUF_SYNC_READ;

  while(true) {
    int ret;
    ret = ioctl (fd, DMA_BUF_IOCTL_SYNC, &sync);
    if (ret == -1 && errno == EINTR) {
      continue;
    } else if (ret == -1) {
      RTC_LOG(LS_ERROR) << "Failed to synchronize DMA buffer: " << g_strerror(errno);
      break;
    } else {
      break;
    }
  }
}

// static
void BaseCapturerPipeWire::OnCoreError(void *data,
                                       uint32_t id,
                                       int seq,
                                       int res,
                                       const char *message) {
  RTC_LOG(LS_ERROR) << "core error: " << message;
}

// static
void BaseCapturerPipeWire::OnStreamStateChanged(void* data,
                                                pw_stream_state old_state,
                                                pw_stream_state state,
                                                const char* error_message) {
  BaseCapturerPipeWire* that = static_cast<BaseCapturerPipeWire*>(data);
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
void BaseCapturerPipeWire::OnStreamParamChanged(void *data, uint32_t id,
                                                const struct spa_pod *format) {
  BaseCapturerPipeWire* that = static_cast<BaseCapturerPipeWire*>(data);
  RTC_DCHECK(that);

  RTC_LOG(LS_INFO) << "PipeWire stream param changed.";

  if (!format || id != SPA_PARAM_Format) {
    return;
  }

  spa_format_video_raw_parse(format, &that->spa_video_format_);

  auto width = that->spa_video_format_.size.width;
  auto height = that->spa_video_format_.size.height;
  // In order to be able to build in the non unified environment kBytesPerPixel
  // must be fully qualified, see Bug 1725145
  auto stride = SPA_ROUND_UP_N(width * BasicDesktopFrame::kBytesPerPixel, 4);
  auto size = height * stride;

  that->desktop_size_ = DesktopSize(width, height);

  uint8_t buffer[1024] = {};
  auto builder = spa_pod_builder{buffer, sizeof(buffer)};

  // Setup buffers and meta header for new format.
  const struct spa_pod* params[3];
  params[0] = reinterpret_cast<spa_pod *>(spa_pod_builder_add_object(&builder,
              SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
              SPA_PARAM_BUFFERS_dataType, SPA_POD_CHOICE_FLAGS_Int((1<<SPA_DATA_MemPtr) |
                                                                   (1<<SPA_DATA_MemFd) |
                                                                   (1<<SPA_DATA_DmaBuf)),
              SPA_PARAM_BUFFERS_size, SPA_POD_Int(size),
              SPA_PARAM_BUFFERS_stride, SPA_POD_Int(stride),
              SPA_PARAM_BUFFERS_buffers, SPA_POD_CHOICE_RANGE_Int(8, 1, 32)));
  params[1] = reinterpret_cast<spa_pod *>(spa_pod_builder_add_object(&builder,
              SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta,
              SPA_PARAM_META_type, SPA_POD_Id(SPA_META_Header),
              SPA_PARAM_META_size, SPA_POD_Int(sizeof(struct spa_meta_header))));
  params[2] = reinterpret_cast<spa_pod *>(spa_pod_builder_add_object(&builder,
              SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta,
              SPA_PARAM_META_type, SPA_POD_Id (SPA_META_VideoCrop),
              SPA_PARAM_META_size, SPA_POD_Int (sizeof(struct spa_meta_region))));
  pw_stream_update_params(that->pw_stream_, params, 3);
}

// static
void BaseCapturerPipeWire::OnStreamProcess(void* data) {
  BaseCapturerPipeWire* that = static_cast<BaseCapturerPipeWire*>(data);
  RTC_DCHECK(that);

  struct pw_buffer *next_buffer;
  struct pw_buffer *buffer = nullptr;

  next_buffer = pw_stream_dequeue_buffer(that->pw_stream_);
  while (next_buffer) {
    buffer = next_buffer;
    next_buffer = pw_stream_dequeue_buffer(that->pw_stream_);

    if (next_buffer) {
      pw_stream_queue_buffer (that->pw_stream_, buffer);
    }
  }

  if (!buffer) {
    return;
  }

  that->HandleBuffer(buffer);

  pw_stream_queue_buffer(that->pw_stream_, buffer);
}

BaseCapturerPipeWire::BaseCapturerPipeWire(CaptureSourceType source_type)
    : capture_source_type_(source_type) {}

BaseCapturerPipeWire::~BaseCapturerPipeWire() {
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

  if (start_request_signal_id_) {
    g_dbus_connection_signal_unsubscribe(connection_, start_request_signal_id_);
  }
  if (sources_request_signal_id_) {
    g_dbus_connection_signal_unsubscribe(connection_,
                                         sources_request_signal_id_);
  }
  if (session_request_signal_id_) {
    g_dbus_connection_signal_unsubscribe(connection_,
                                         session_request_signal_id_);
  }

  if (session_handle_) {
    GDBusMessage* message = g_dbus_message_new_method_call(
        kDesktopBusName, session_handle_, kSessionInterfaceName, "Close");
    if (message) {
      GError* error = nullptr;
      g_dbus_connection_send_message(connection_, message,
                                     G_DBUS_SEND_MESSAGE_FLAGS_NONE,
                                     /*out_serial=*/nullptr, &error);
      if (error) {
        RTC_LOG(LS_ERROR) << "Failed to close the session: " << error->message;
        g_error_free(error);
      }
      g_object_unref(message);
    }
  }

  g_free(start_handle_);
  g_free(sources_handle_);
  g_free(session_handle_);
  g_free(portal_handle_);

  if (cancellable_) {
    g_cancellable_cancel(cancellable_);
    g_object_unref(cancellable_);
    cancellable_ = nullptr;
  }

  if (proxy_) {
    g_object_unref(proxy_);
    proxy_ = nullptr;
  }
}

void BaseCapturerPipeWire::InitPortal() {
  cancellable_ = g_cancellable_new();
  g_dbus_proxy_new_for_bus(
      G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_NONE, /*info=*/nullptr,
      kDesktopBusName, kDesktopObjectPath, kScreenCastInterfaceName,
      cancellable_,
      reinterpret_cast<GAsyncReadyCallback>(OnProxyRequested), this);
}

void BaseCapturerPipeWire::InitPipeWire() {
  pw_init(/*argc=*/nullptr, /*argc=*/nullptr);

  pw_main_loop_ = pw_thread_loop_new("pipewire-main-loop", nullptr);
  pw_context_ = pw_context_new(pw_thread_loop_get_loop(pw_main_loop_), nullptr, 0);
  if (!pw_context_) {
    RTC_LOG(LS_ERROR) << "Failed to create PipeWire context";
    return;
  }

  pw_core_ = pw_context_connect_fd(pw_context_, pw_fd_, nullptr, 0);
  if (!pw_core_) {
    RTC_LOG(LS_ERROR) << "Failed to connect PipeWire context";
    return;
  }

  // Initialize event handlers, remote end and stream-related.
  pw_core_events_.version = PW_VERSION_CORE_EVENTS;
  pw_core_events_.error = &OnCoreError;

  pw_stream_events_.version = PW_VERSION_STREAM_EVENTS;
  pw_stream_events_.state_changed = &OnStreamStateChanged;
  pw_stream_events_.param_changed = &OnStreamParamChanged;
  pw_stream_events_.process = &OnStreamProcess;

  pw_core_add_listener(pw_core_, &spa_core_listener_, &pw_core_events_, this);

  pw_stream_ = CreateReceivingStream();
  if (!pw_stream_) {
    RTC_LOG(LS_ERROR) << "Failed to create PipeWire stream";
    return;
  }

  if (pw_thread_loop_start(pw_main_loop_) < 0) {
    RTC_LOG(LS_ERROR) << "Failed to start main PipeWire loop";
    portal_init_failed_ = true;
  }
}

pw_stream* BaseCapturerPipeWire::CreateReceivingStream() {
  spa_rectangle pwMinScreenBounds = spa_rectangle{1, 1};
  spa_rectangle pwMaxScreenBounds = spa_rectangle{UINT32_MAX, UINT32_MAX};

  auto stream = pw_stream_new(pw_core_, "webrtc-pipewire-stream", nullptr);

  if (!stream) {
    RTC_LOG(LS_ERROR) << "Could not create receiving stream.";
    return nullptr;
  }

  uint8_t buffer[1024] = {};
  const spa_pod* params[2];
  spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer, sizeof (buffer));

  params[0] = reinterpret_cast<spa_pod *>(spa_pod_builder_add_object(&builder,
              SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
              SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video),
              SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
              SPA_FORMAT_VIDEO_format, SPA_POD_CHOICE_ENUM_Id(5, SPA_VIDEO_FORMAT_BGRx, SPA_VIDEO_FORMAT_RGBx, SPA_VIDEO_FORMAT_RGBA,
                                                                 SPA_VIDEO_FORMAT_BGRx, SPA_VIDEO_FORMAT_BGRA),
              SPA_FORMAT_VIDEO_size, SPA_POD_CHOICE_RANGE_Rectangle(&pwMinScreenBounds,
                                                                    &pwMinScreenBounds,
                                                                    &pwMaxScreenBounds),
              0));
  pw_stream_add_listener(stream, &spa_stream_listener_, &pw_stream_events_, this);

  if (pw_stream_connect(stream, PW_DIRECTION_INPUT, pw_stream_node_id_,
      PW_STREAM_FLAG_AUTOCONNECT, params, 1) != 0) {
    RTC_LOG(LS_ERROR) << "Could not connect receiving stream.";
    portal_init_failed_ = true;
  }

  return stream;
}

static void SpaBufferUnmap(unsigned char *map, int map_size, bool IsDMABuf, int fd) {
  if (map) {
    if (IsDMABuf) {
      SyncDmaBuf(fd, DMA_BUF_SYNC_END);
    }
    munmap(map, map_size);
  }
}

void BaseCapturerPipeWire::HandleBuffer(pw_buffer* buffer) {
  spa_buffer* spaBuffer = buffer->buffer;
  uint8_t *map = nullptr;
  uint8_t* src = nullptr;

  if (spaBuffer->datas[0].chunk->size == 0) {
    RTC_LOG(LS_ERROR) << "Failed to get video stream: Zero size.";
    return;
  }

  switch (spaBuffer->datas[0].type) {
    case SPA_DATA_MemFd:
    case SPA_DATA_DmaBuf:
      map = static_cast<uint8_t*>(mmap(
          nullptr, spaBuffer->datas[0].maxsize + spaBuffer->datas[0].mapoffset,
          PROT_READ, MAP_PRIVATE, spaBuffer->datas[0].fd, 0));
      if (map == MAP_FAILED) {
        RTC_LOG(LS_ERROR) << "Failed to mmap memory: " << std::strerror(errno);
        return;
      }
      if (spaBuffer->datas[0].type == SPA_DATA_DmaBuf) {
        SyncDmaBuf(spaBuffer->datas[0].fd, DMA_BUF_SYNC_START);
      }
      src = SPA_MEMBER(map, spaBuffer->datas[0].mapoffset, uint8_t);
      break;
    case SPA_DATA_MemPtr:
      map = nullptr;
      src = static_cast<uint8_t*>(spaBuffer->datas[0].data);
      break;
    default:
      return;
  }

  if (!src) {
    RTC_LOG(LS_ERROR) << "Failed to get video stream: Wrong data after mmap()";
    SpaBufferUnmap(map,
      spaBuffer->datas[0].maxsize + spaBuffer->datas[0].mapoffset,
      spaBuffer->datas[0].type == SPA_DATA_DmaBuf, spaBuffer->datas[0].fd);
    return;
  }

  struct spa_meta_region* video_metadata =
    static_cast<struct spa_meta_region*>(
       spa_buffer_find_meta_data(spaBuffer, SPA_META_VideoCrop, sizeof(*video_metadata)));

  // Video size from metada is bigger than an actual video stream size.
  // The metadata are wrong or we should up-scale te video...in both cases
  // just quit now.
  if (video_metadata &&
      (video_metadata->region.size.width > (uint32_t)desktop_size_.width() ||
        video_metadata->region.size.height > (uint32_t)desktop_size_.height())) {
    RTC_LOG(LS_ERROR) << "Stream metadata sizes are wrong!";
    SpaBufferUnmap(map,
      spaBuffer->datas[0].maxsize + spaBuffer->datas[0].mapoffset,
      spaBuffer->datas[0].type == SPA_DATA_DmaBuf, spaBuffer->datas[0].fd);
    return;
  }

  // Use video metada when video size from metadata is set and smaller than
  // video stream size, so we need to adjust it.
  video_metadata_use_ = (video_metadata &&
      video_metadata->region.size.width != 0 &&
        video_metadata->region.size.height != 0 &&
          (video_metadata->region.size.width < (uint32_t)desktop_size_.width() ||
            video_metadata->region.size.height < (uint32_t)desktop_size_.height()));

  DesktopSize video_size_prev = video_size_;
    if (video_metadata_use_) {
    video_size_ = DesktopSize(video_metadata->region.size.width,
                              video_metadata->region.size.height);
  } else {
    video_size_ = desktop_size_;
  }

  rtc::CritScope lock(&current_frame_lock_);
  if (!current_frame_ || !video_size_.equals(video_size_prev)) {
    current_frame_ =
      std::make_unique<uint8_t[]>
        (video_size_.width() * video_size_.height() * BasicDesktopFrame::kBytesPerPixel);
  }

  const int32_t dstStride = video_size_.width() * BasicDesktopFrame::kBytesPerPixel;
  const int32_t srcStride = spaBuffer->datas[0].chunk->stride;

  // Adjust source content based on metadata video position
  if (video_metadata_use_ &&
      (video_metadata->region.position.y + video_size_.height() <= desktop_size_.height())) {
    src += srcStride * video_metadata->region.position.y;
  }
  const int xOffset =
      video_metadata_use_ &&
        (video_metadata->region.position.x + video_size_.width() <= desktop_size_.width())
          ? video_metadata->region.position.x * BasicDesktopFrame::kBytesPerPixel
          : 0;

  uint8_t* dst = current_frame_.get();
  for (int i = 0; i < video_size_.height(); ++i) {
    // Adjust source content based on crop video position if needed
    src += xOffset;
    std::memcpy(dst, src, dstStride);
    // If both sides decided to go with the RGBx format we need to convert it to
    // BGRx to match color format expected by WebRTC.
    if (spa_video_format_.format == SPA_VIDEO_FORMAT_RGBx ||
        spa_video_format_.format == SPA_VIDEO_FORMAT_RGBA) {
      ConvertRGBxToBGRx(dst, dstStride);
    }
    src += srcStride - xOffset;
    dst += dstStride;
  }

  SpaBufferUnmap(map,
    spaBuffer->datas[0].maxsize + spaBuffer->datas[0].mapoffset,
    spaBuffer->datas[0].type == SPA_DATA_DmaBuf, spaBuffer->datas[0].fd);
}

void BaseCapturerPipeWire::ConvertRGBxToBGRx(uint8_t* frame, uint32_t size) {
  // Change color format for KDE KWin which uses RGBx and not BGRx
  for (uint32_t i = 0; i < size; i += 4) {
    uint8_t tempR = frame[i];
    uint8_t tempB = frame[i + 2];
    frame[i] = tempB;
    frame[i + 2] = tempR;
  }
}

guint BaseCapturerPipeWire::SetupRequestResponseSignal(
    const gchar* object_path,
    GDBusSignalCallback callback) {
  return g_dbus_connection_signal_subscribe(
      connection_, kDesktopBusName, kRequestInterfaceName, "Response",
      object_path, /*arg0=*/nullptr, G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
      callback, this, /*user_data_free_func=*/nullptr);
}

// static
void BaseCapturerPipeWire::OnProxyRequested(GObject* /*object*/,
                                            GAsyncResult* result,
                                            gpointer user_data) {
  BaseCapturerPipeWire* that = static_cast<BaseCapturerPipeWire*>(user_data);
  RTC_DCHECK(that);

  GError* error = nullptr;
  GDBusProxy *proxy = g_dbus_proxy_new_finish(result, &error);
  if (!proxy) {
    if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      return;
    RTC_LOG(LS_ERROR) << "Failed to create a proxy for the screen cast portal: "
                      << error->message;
    g_error_free(error);
    that->portal_init_failed_ = true;
    return;
  }
  that->proxy_ = proxy;
  that->connection_ = g_dbus_proxy_get_connection(that->proxy_);

  RTC_LOG(LS_INFO) << "Created proxy for the screen cast portal.";
  that->SessionRequest();
}

// static
gchar* BaseCapturerPipeWire::PrepareSignalHandle(GDBusConnection* connection,
                                                 const gchar* token) {
  gchar* sender = g_strdup(g_dbus_connection_get_unique_name(connection) + 1);
  for (int i = 0; sender[i]; i++) {
    if (sender[i] == '.') {
      sender[i] = '_';
    }
  }

  gchar* handle = g_strconcat(kDesktopRequestObjectPath, "/", sender, "/",
                              token, /*end of varargs*/ nullptr);
  g_free(sender);

  return handle;
}

void BaseCapturerPipeWire::SessionRequest() {
  GVariantBuilder builder;
  gchar* variant_string;

  g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
  variant_string =
      g_strdup_printf("webrtc_session%d", g_random_int_range(0, G_MAXINT));
  g_variant_builder_add(&builder, "{sv}", "session_handle_token",
                        g_variant_new_string(variant_string));
  g_free(variant_string);
  variant_string = g_strdup_printf("webrtc%d", g_random_int_range(0, G_MAXINT));
  g_variant_builder_add(&builder, "{sv}", "handle_token",
                        g_variant_new_string(variant_string));

  portal_handle_ = PrepareSignalHandle(connection_, variant_string);
  session_request_signal_id_ = SetupRequestResponseSignal(
      portal_handle_, OnSessionRequestResponseSignal);
  g_free(variant_string);

  RTC_LOG(LS_INFO) << "Screen cast session requested.";
  g_dbus_proxy_call(
      proxy_, "CreateSession", g_variant_new("(a{sv})", &builder),
      G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, cancellable_,
      reinterpret_cast<GAsyncReadyCallback>(OnSessionRequested), this);
}

// static
void BaseCapturerPipeWire::OnSessionRequested(GDBusProxy *proxy,
                                              GAsyncResult* result,
                                              gpointer user_data) {
  BaseCapturerPipeWire* that = static_cast<BaseCapturerPipeWire*>(user_data);
  RTC_DCHECK(that);

  GError* error = nullptr;
  GVariant* variant = g_dbus_proxy_call_finish(proxy, result, &error);
  if (!variant) {
    if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      return;
    RTC_LOG(LS_ERROR) << "Failed to create a screen cast session: "
                      << error->message;
    g_error_free(error);
    that->portal_init_failed_ = true;
    return;
  }
  RTC_LOG(LS_INFO) << "Initializing the screen cast session.";

  gchar* handle = nullptr;
  g_variant_get_child(variant, 0, "o", &handle);
  g_variant_unref(variant);
  if (!handle) {
    RTC_LOG(LS_ERROR) << "Failed to initialize the screen cast session.";
    if (that->session_request_signal_id_) {
      g_dbus_connection_signal_unsubscribe(that->connection_,
                                           that->session_request_signal_id_);
      that->session_request_signal_id_ = 0;
    }
    that->portal_init_failed_ = true;
    return;
  }

  g_free(handle);

  RTC_LOG(LS_INFO) << "Subscribing to the screen cast session.";
}

// static
void BaseCapturerPipeWire::OnSessionRequestResponseSignal(
    GDBusConnection* connection,
    const gchar* sender_name,
    const gchar* object_path,
    const gchar* interface_name,
    const gchar* signal_name,
    GVariant* parameters,
    gpointer user_data) {
  BaseCapturerPipeWire* that = static_cast<BaseCapturerPipeWire*>(user_data);
  RTC_DCHECK(that);

  RTC_LOG(LS_INFO)
      << "Received response for the screen cast session subscription.";

  guint32 portal_response;
  GVariant* response_data;
  g_variant_get(parameters, "(u@a{sv})", &portal_response, &response_data);
  g_variant_lookup(response_data, "session_handle", "s",
                   &that->session_handle_);
  g_variant_unref(response_data);

  if (!that->session_handle_ || portal_response) {
    RTC_LOG(LS_ERROR)
        << "Failed to request the screen cast session subscription.";
    that->portal_init_failed_ = true;
    return;
  }

  that->SourcesRequest();
}

void BaseCapturerPipeWire::SourcesRequest() {
  GVariantBuilder builder;
  gchar* variant_string;

  g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
  // We want to record monitor content.
  g_variant_builder_add(&builder, "{sv}", "types",
                        g_variant_new_uint32(capture_source_type_));
  // We don't want to allow selection of multiple sources.
  g_variant_builder_add(&builder, "{sv}", "multiple",
                        g_variant_new_boolean(false));
  variant_string = g_strdup_printf("webrtc%d", g_random_int_range(0, G_MAXINT));
  g_variant_builder_add(&builder, "{sv}", "handle_token",
                        g_variant_new_string(variant_string));

  sources_handle_ = PrepareSignalHandle(connection_, variant_string);
  sources_request_signal_id_ = SetupRequestResponseSignal(
      sources_handle_, OnSourcesRequestResponseSignal);
  g_free(variant_string);

  RTC_LOG(LS_INFO) << "Requesting sources from the screen cast session.";
  g_dbus_proxy_call(
      proxy_, "SelectSources",
      g_variant_new("(oa{sv})", session_handle_, &builder),
      G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, cancellable_,
      reinterpret_cast<GAsyncReadyCallback>(OnSourcesRequested), this);
}

// static
void BaseCapturerPipeWire::OnSourcesRequested(GDBusProxy *proxy,
                                              GAsyncResult* result,
                                              gpointer user_data) {
  BaseCapturerPipeWire* that = static_cast<BaseCapturerPipeWire*>(user_data);
  RTC_DCHECK(that);

  GError* error = nullptr;
  GVariant* variant = g_dbus_proxy_call_finish(proxy, result, &error);
  if (!variant) {
    if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      return;
    RTC_LOG(LS_ERROR) << "Failed to request the sources: " << error->message;
    g_error_free(error);
    that->portal_init_failed_ = true;
    return;
  }

  RTC_LOG(LS_INFO) << "Sources requested from the screen cast session.";

  gchar* handle = nullptr;
  g_variant_get_child(variant, 0, "o", &handle);
  g_variant_unref(variant);
  if (!handle) {
    RTC_LOG(LS_ERROR) << "Failed to initialize the screen cast session.";
    if (that->sources_request_signal_id_) {
      g_dbus_connection_signal_unsubscribe(that->connection_,
                                           that->sources_request_signal_id_);
      that->sources_request_signal_id_ = 0;
    }
    that->portal_init_failed_ = true;
    return;
  }

  g_free(handle);

  RTC_LOG(LS_INFO) << "Subscribed to sources signal.";
}

// static
void BaseCapturerPipeWire::OnSourcesRequestResponseSignal(
    GDBusConnection* connection,
    const gchar* sender_name,
    const gchar* object_path,
    const gchar* interface_name,
    const gchar* signal_name,
    GVariant* parameters,
    gpointer user_data) {
  BaseCapturerPipeWire* that = static_cast<BaseCapturerPipeWire*>(user_data);
  RTC_DCHECK(that);

  RTC_LOG(LS_INFO) << "Received sources signal from session.";

  guint32 portal_response;
  g_variant_get(parameters, "(u@a{sv})", &portal_response, nullptr);
  if (portal_response) {
    RTC_LOG(LS_ERROR)
        << "Failed to select sources for the screen cast session.";
    that->portal_init_failed_ = true;
    return;
  }

  that->StartRequest();
}

void BaseCapturerPipeWire::StartRequest() {
  GVariantBuilder builder;
  gchar* variant_string;

  g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
  variant_string = g_strdup_printf("webrtc%d", g_random_int_range(0, G_MAXINT));
  g_variant_builder_add(&builder, "{sv}", "handle_token",
                        g_variant_new_string(variant_string));

  start_handle_ = PrepareSignalHandle(connection_, variant_string);
  start_request_signal_id_ =
      SetupRequestResponseSignal(start_handle_, OnStartRequestResponseSignal);
  g_free(variant_string);

  // "Identifier for the application window", this is Wayland, so not "x11:...".
  const gchar parent_window[] = "";

  RTC_LOG(LS_INFO) << "Starting the screen cast session.";
  g_dbus_proxy_call(
      proxy_, "Start",
      g_variant_new("(osa{sv})", session_handle_, parent_window, &builder),
      G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, cancellable_,
      reinterpret_cast<GAsyncReadyCallback>(OnStartRequested), this);
}

// static
void BaseCapturerPipeWire::OnStartRequested(GDBusProxy *proxy,
                                            GAsyncResult* result,
                                            gpointer user_data) {
  BaseCapturerPipeWire* that = static_cast<BaseCapturerPipeWire*>(user_data);
  RTC_DCHECK(that);

  GError* error = nullptr;
  GVariant* variant = g_dbus_proxy_call_finish(proxy, result, &error);
  if (!variant) {
    if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      return;
    RTC_LOG(LS_ERROR) << "Failed to start the screen cast session: "
                      << error->message;
    g_error_free(error);
    that->portal_init_failed_ = true;
    return;
  }

  RTC_LOG(LS_INFO) << "Initializing the start of the screen cast session.";

  gchar* handle = nullptr;
  g_variant_get_child(variant, 0, "o", &handle);
  g_variant_unref(variant);
  if (!handle) {
    RTC_LOG(LS_ERROR)
        << "Failed to initialize the start of the screen cast session.";
    if (that->start_request_signal_id_) {
      g_dbus_connection_signal_unsubscribe(that->connection_,
                                           that->start_request_signal_id_);
      that->start_request_signal_id_ = 0;
    }
    that->portal_init_failed_ = true;
    return;
  }

  g_free(handle);

  RTC_LOG(LS_INFO) << "Subscribed to the start signal.";
}

// static
void BaseCapturerPipeWire::OnStartRequestResponseSignal(
    GDBusConnection* connection,
    const gchar* sender_name,
    const gchar* object_path,
    const gchar* interface_name,
    const gchar* signal_name,
    GVariant* parameters,
    gpointer user_data) {
  BaseCapturerPipeWire* that = static_cast<BaseCapturerPipeWire*>(user_data);
  RTC_DCHECK(that);

  RTC_LOG(LS_INFO) << "Start signal received.";
  guint32 portal_response;
  GVariant* response_data;
  GVariantIter* iter = nullptr;
  g_variant_get(parameters, "(u@a{sv})", &portal_response, &response_data);
  if (portal_response || !response_data) {
    RTC_LOG(LS_ERROR) << "Failed to start the screen cast session.";
    that->portal_init_failed_ = true;
    return;
  }

  // Array of PipeWire streams. See
  // https://github.com/flatpak/xdg-desktop-portal/blob/master/data/org.freedesktop.portal.ScreenCast.xml
  // documentation for <method name="Start">.
  if (g_variant_lookup(response_data, "streams", "a(ua{sv})", &iter)) {
    GVariant* variant;

    while (g_variant_iter_next(iter, "@(ua{sv})", &variant)) {
      guint32 stream_id;
      GVariant* options;

      g_variant_get(variant, "(u@a{sv})", &stream_id, &options);
      RTC_DCHECK(options != nullptr);

      that->pw_stream_node_id_ = stream_id;
      g_variant_unref(options);
      g_variant_unref(variant);
    }
  }
  g_variant_iter_free(iter);
  g_variant_unref(response_data);

  that->OpenPipeWireRemote();
}

void BaseCapturerPipeWire::OpenPipeWireRemote() {
  GVariantBuilder builder;
  g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);

  RTC_LOG(LS_INFO) << "Opening the PipeWire remote.";

  g_dbus_proxy_call_with_unix_fd_list(
      proxy_, "OpenPipeWireRemote",
      g_variant_new("(oa{sv})", session_handle_, &builder),
      G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, /*fd_list=*/nullptr,
      cancellable_,
      reinterpret_cast<GAsyncReadyCallback>(OnOpenPipeWireRemoteRequested),
      this);
}

// static
void BaseCapturerPipeWire::OnOpenPipeWireRemoteRequested(
    GDBusProxy *proxy,
    GAsyncResult* result,
    gpointer user_data) {
  BaseCapturerPipeWire* that = static_cast<BaseCapturerPipeWire*>(user_data);
  RTC_DCHECK(that);

  GError* error = nullptr;
  GUnixFDList* outlist = nullptr;
  GVariant* variant = g_dbus_proxy_call_with_unix_fd_list_finish(
      proxy, &outlist, result, &error);
  if (!variant) {
    if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      return;
    RTC_LOG(LS_ERROR) << "Failed to open the PipeWire remote: "
                      << error->message;
    g_error_free(error);
    that->portal_init_failed_ = true;
    return;
  }

  gint32 index;
  g_variant_get(variant, "(h)", &index);

  if ((that->pw_fd_ = g_unix_fd_list_get(outlist, index, &error)) == -1) {
    RTC_LOG(LS_ERROR) << "Failed to get file descriptor from the list: "
                      << error->message;
    g_error_free(error);
    g_variant_unref(variant);
    that->portal_init_failed_ = true;
    return;
  }

  g_variant_unref(variant);
  g_object_unref(outlist);

  that->InitPipeWire();
  RTC_LOG(LS_INFO) << "PipeWire remote opened.";
}

void BaseCapturerPipeWire::Start(Callback* callback) {
  RTC_DCHECK(!callback_);
  RTC_DCHECK(callback);

  InitPortal();

  callback_ = callback;
}

void BaseCapturerPipeWire::CaptureFrame() {
  if (portal_init_failed_) {
    callback_->OnCaptureResult(Result::ERROR_PERMANENT, nullptr);
    return;
  }

  rtc::CritScope lock(&current_frame_lock_);
  if (!current_frame_) {
    callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
    return;
  }

  DesktopSize frame_size = desktop_size_;
  if (video_metadata_use_) {
    frame_size = video_size_;
  }

  std::unique_ptr<DesktopFrame> result(new BasicDesktopFrame(frame_size));
  result->CopyPixelsFrom(
      current_frame_.get(), (frame_size.width() * BasicDesktopFrame::kBytesPerPixel),
      DesktopRect::MakeWH(frame_size.width(), frame_size.height()));
  if (!result) {
    callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
    return;
  }
  callback_->OnCaptureResult(Result::SUCCESS, std::move(result));
}

// Keep in sync with defines at browser/actors/WebRTCParent.jsm
// With PipeWire we can't select which system resource is shared so
// we don't create a window/screen list. Instead we place these constants
// as window name/id so frontend code can identify PipeWire backend
// and does not try to create screen/window preview.

#define PIPEWIRE_ID   0xaffffff
#define PIPEWIRE_NAME "####_PIPEWIRE_PORTAL_####"

bool BaseCapturerPipeWire::GetSourceList(SourceList* sources) {
  sources->push_back({PIPEWIRE_ID, 0, PIPEWIRE_NAME});
  return true;
}

bool BaseCapturerPipeWire::SelectSource(SourceId id) {
  // Screen selection is handled by the xdg-desktop-portal.
  return id == PIPEWIRE_ID;
}

// static
std::unique_ptr<DesktopCapturer>
BaseCapturerPipeWire::CreateRawScreenCapturer(
    const DesktopCaptureOptions& options) {
  std::unique_ptr<BaseCapturerPipeWire> capturer =
      std::make_unique<BaseCapturerPipeWire>(BaseCapturerPipeWire::CaptureSourceType::kAny);
  return std::move(capturer);}

// static
std::unique_ptr<DesktopCapturer>
BaseCapturerPipeWire::CreateRawWindowCapturer(
    const DesktopCaptureOptions& options) {

  std::unique_ptr<BaseCapturerPipeWire> capturer =
      std::make_unique<BaseCapturerPipeWire>(BaseCapturerPipeWire::CaptureSourceType::kAny);
  return std::move(capturer);
}

}  // namespace webrtc
