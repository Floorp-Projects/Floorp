/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file defines the interface for event tracing in WebRTC.
//
// Event log handlers are set through SetupEventTracer(). User of this API will
// provide two function pointers to handle event tracing calls.
//
// * GetCategoryEnabledPtr
//   Event tracing system calls this function to determine if a particular
//   event category is enabled.
//
// * AddTraceEventPtr
//   Adds a tracing event. It is the user's responsibility to log the data
//   provided.
//
// Parameters for the above two functions are described in trace_event.h.

#ifndef WEBRTC_SYSTEM_WRAPPERS_INTERFACE_EVENT_TRACER_H_
#define WEBRTC_SYSTEM_WRAPPERS_INTERFACE_EVENT_TRACER_H_

// This file has moved.
// TODO(tommi): Delete after removing dependencies and updating Chromium.
#include "webrtc/base/event_tracer.h"

#endif  // WEBRTC_SYSTEM_WRAPPERS_INTERFACE_EVENT_TRACER_H_
