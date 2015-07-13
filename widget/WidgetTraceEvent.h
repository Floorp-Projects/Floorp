/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WIDGET_PUBLIC_WIDGETTRACEEVENT_H_
#define WIDGET_PUBLIC_WIDGETTRACEEVENT_H_

namespace mozilla {

// Perform any required initialization in the widget backend for
// event tracing. Return true if initialization was successful.
bool InitWidgetTracing();

// Perform any required cleanup in the widget backend for event tracing.
void CleanUpWidgetTracing();

// Fire a tracer event at the UI-thread event loop, and block until
// the event is processed. This should only be called by
// a thread that's not the UI thread.
bool FireAndWaitForTracerEvent();

// Signal that the event has been received by the event loop.
void SignalTracerThread();

} // namespace mozilla

#endif  // WIDGET_PUBLIC_WIDGETTRACEEVENT_H_
