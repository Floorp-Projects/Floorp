/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_recordreplay_ChildIPC_h
#define mozilla_recordreplay_ChildIPC_h

#include "base/process.h"
#include "mozilla/gfx/2D.h"
#include "Units.h"

namespace mozilla {

class VsyncObserver;

namespace recordreplay {
namespace child {

// This file has the public API for definitions used in facilitating IPC
// between a child recording/replaying process and the middleman process.

// Initialize replaying IPC state. This is called once during process startup,
// and is a no-op if the process is not recording/replaying.
void InitRecordingOrReplayingProcess(int* aArgc, char*** aArgv);

// Get the IDs of the middleman and parent processes.
base::ProcessId MiddlemanProcessId();
base::ProcessId ParentProcessId();

// Create a normal checkpoint, if no such checkpoint has been created yet.
void MaybeCreateInitialCheckpoint();

///////////////////////////////////////////////////////////////////////////////
// Painting Coordination
///////////////////////////////////////////////////////////////////////////////

// In child processes, paints do not occur in response to vsyncs from the UI
// process: when a page is updating rapidly these events occur sporadically and
// cause the tab's graphics to not accurately reflect the tab's state at that
// point in time. When viewing a normal tab this is no problem because the tab
// will be painted with the correct graphics momentarily, but when the tab can
// be rewound and paused this behavior is visible.
//
// This API is used to trigger artificial vsyncs whenever the page is updated.
// SetVsyncObserver is used to tell the child code about any singleton vsync
// observer that currently exists, and NotifyVsyncObserver is used to trigger
// a vsync on that observer at predictable points, e.g. the top of the main
// thread's event loop.
void SetVsyncObserver(VsyncObserver* aObserver);
void NotifyVsyncObserver();

// Similarly to the vsync handling above, in order to ensure that the tab's
// graphics accurately reflect its state, we want to perform paints
// synchronously after a vsync has occurred. When a paint is about to happen,
// the main thread calls NotifyPaintStart, and after the compositor thread has
// been informed about the update the main thread calls WaitForPaintToComplete
// to block until the compositor thread has finished painting and called
// NotifyPaintComplete.
void NotifyPaintStart();
void WaitForPaintToComplete();
void NotifyPaintComplete();

// Get a draw target which the compositor thread can paint to.
already_AddRefed<gfx::DrawTarget> DrawTargetForRemoteDrawing(LayoutDeviceIntSize aSize);

} // namespace child
} // namespace recordreplay
} // namespace mozilla

#endif // mozilla_recordreplay_ChildIPC_h
