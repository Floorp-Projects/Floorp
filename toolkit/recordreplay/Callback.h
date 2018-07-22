/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_recordreplay_Callback_h
#define mozilla_recordreplay_Callback_h

#include "mozilla/GuardObjects.h"

#include <functional>

namespace mozilla {
namespace recordreplay {

// Callbacks Overview.
//
// Record/replay callbacks are used to record and replay the use of callbacks
// within system libraries to reenter Gecko code. There are three challenges
// to replaying callbacks:
//
// 1. Invocations of the callbacks must be replayed so that they occur inside
//    the same system call and in the same order as during recording.
//
// 2. Data passed to the callback which originates in Gecko itself (e.g.
//    opaque data pointers) need to match up with the Gecko data which was
//    passed to the callback while recording.
//
// 3. Data passed to the callback which originates in the system library also
//    needs to match up with the data passed while recording.
//
// Each platform defines a CallbackEvent enum with the different callback
// signatures that the platform is able to redirect. Callback wrapper functions
// are then defined for each callback event.
//
// The following additional steps are taken to handle #1 above:
//
// A. System libraries which Gecko callbacks are passed to are redirected so
//    that they replace the Gecko callback with the callback wrapper for that
//    signature.
//
// B. When recording, system libraries which can invoke Gecko callbacks are
//    redirected to call the library API inside a call to
//    PassThroughThreadEventsAllowCallbacks.
//
// C. When a callback wrapper is invoked within the library, it calls
//    {Begin,End}Callback to stop passing through thread events while the
//    callback executes.
//
// D. {Begin,End}Callback additionally adds ExecuteCallback events for the
//    thread, and PassThroughThreadEventsAllowCallbacks adds a
//    CallbacksFinished event at the end. While replaying, calling
//    PassThroughThreadEventsAllowCallbacks will read these callback events
//    from the thread's events file and plas back calls to the wrappers which
//    executed while recording.
//
// #2 above is handled with the callback data API below. When a Gecko callback
// or opaque data pointer is passed to a system library API, that API is
// redirected so that it will call RegisterCallbackData on the Gecko pointer.
// Later, when the callback wrapper actually executes, it can use
// SaveOrRestoreCallbackData to record which Gecko pointer was used and later,
// during replay, restore the corresponding value in that execution.
//
// #3 above can be recorded and replayed using the standard
// RecordReplay{Value,Bytes} functions, in a similar manner to the handling of
// outputs of redirected functions.

// Note or remove a pointer passed to a system library API which might be a
// Gecko callback or a data pointer used by a Gecko callback.
void RegisterCallbackData(void* aData);
void RemoveCallbackData(void* aData);

// Record/replay a pointer that was passed to RegisterCallbackData earlier.
void SaveOrRestoreCallbackData(void** aData);

// If recording, call aFn with events passed through, allowing Gecko callbacks
// to execute within aFn. If replaying, execute only the Gecko callbacks which
// executed while recording.
void PassThroughThreadEventsAllowCallbacks(const std::function<void()>& aFn);

// Within a callback wrapper, bracket the execution of the code for the Gecko
// callback and record the callback as having executed. This stops passing
// through thread events so that behaviors in the Gecko callback are
// recorded/replayed.
void BeginCallback(size_t aCallbackId);
void EndCallback();

// During replay, invoke a callback with the specified id. This is platform
// specific and is defined in the various ProcessRedirect*.cpp files.
void ReplayInvokeCallback(size_t aCallbackId);

} // namespace recordreplay
} // namespace mozilla

#endif // mozilla_recordreplay_Callback_h
