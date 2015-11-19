/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SOUND_PULSEAUDIOSYMBOLTABLE_H_
#define WEBRTC_SOUND_PULSEAUDIOSYMBOLTABLE_H_

#include <pulse/context.h>
#include <pulse/def.h>
#include <pulse/error.h>
#include <pulse/introspect.h>
#include <pulse/stream.h>
#include <pulse/thread-mainloop.h>

#include "webrtc/base/latebindingsymboltable.h"

namespace rtc {

#define PULSE_AUDIO_SYMBOLS_CLASS_NAME PulseAudioSymbolTable
// The PulseAudio symbols we need, as an X-Macro list.
// This list must contain precisely every libpulse function that is used in
// pulseaudiosoundsystem.cc.
#define PULSE_AUDIO_SYMBOLS_LIST \
  X(pa_bytes_per_second) \
  X(pa_context_connect) \
  X(pa_context_disconnect) \
  X(pa_context_errno) \
  X(pa_context_get_protocol_version) \
  X(pa_context_get_server_info) \
  X(pa_context_get_sink_info_list) \
  X(pa_context_get_sink_input_info) \
  X(pa_context_get_source_info_by_index) \
  X(pa_context_get_source_info_list) \
  X(pa_context_get_state) \
  X(pa_context_new) \
  X(pa_context_set_sink_input_volume) \
  X(pa_context_set_source_volume_by_index) \
  X(pa_context_set_state_callback) \
  X(pa_context_unref) \
  X(pa_cvolume_set) \
  X(pa_operation_get_state) \
  X(pa_operation_unref) \
  X(pa_stream_connect_playback) \
  X(pa_stream_connect_record) \
  X(pa_stream_disconnect) \
  X(pa_stream_drop) \
  X(pa_stream_get_device_index) \
  X(pa_stream_get_index) \
  X(pa_stream_get_latency) \
  X(pa_stream_get_sample_spec) \
  X(pa_stream_get_state) \
  X(pa_stream_new) \
  X(pa_stream_peek) \
  X(pa_stream_readable_size) \
  X(pa_stream_set_buffer_attr) \
  X(pa_stream_set_overflow_callback) \
  X(pa_stream_set_read_callback) \
  X(pa_stream_set_state_callback) \
  X(pa_stream_set_underflow_callback) \
  X(pa_stream_set_write_callback) \
  X(pa_stream_unref) \
  X(pa_stream_writable_size) \
  X(pa_stream_write) \
  X(pa_strerror) \
  X(pa_threaded_mainloop_free) \
  X(pa_threaded_mainloop_get_api) \
  X(pa_threaded_mainloop_lock) \
  X(pa_threaded_mainloop_new) \
  X(pa_threaded_mainloop_signal) \
  X(pa_threaded_mainloop_start) \
  X(pa_threaded_mainloop_stop) \
  X(pa_threaded_mainloop_unlock) \
  X(pa_threaded_mainloop_wait)

#define LATE_BINDING_SYMBOL_TABLE_CLASS_NAME PULSE_AUDIO_SYMBOLS_CLASS_NAME
#define LATE_BINDING_SYMBOL_TABLE_SYMBOLS_LIST PULSE_AUDIO_SYMBOLS_LIST
#include "webrtc/base/latebindingsymboltable.h.def"

}  // namespace rtc

#endif  // WEBRTC_SOUND_PULSEAUDIOSYMBOLTABLE_H_
