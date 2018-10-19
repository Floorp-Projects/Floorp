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

namespace IPC { class Message; }

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

// Create a normal checkpoint, if execution has not diverged from the recording.
void CreateCheckpoint();

///////////////////////////////////////////////////////////////////////////////
// Painting Coordination
///////////////////////////////////////////////////////////////////////////////

// Tell the child code about any singleton vsync observer that currently
// exists. This is used to trigger artifical vsyncs that paint the current
// graphics when paused.
void SetVsyncObserver(VsyncObserver* aObserver);

// Called before processing incoming vsyncs from the UI process. Returns false
// if the vsync should be ignored.
bool OnVsync();

// Tell the child code about any ongoing painting activity. When a paint is
// about to happen, the main thread calls NotifyPaintStart, and when the
// compositor thread finishes the paint it calls NotifyPaintComplete.
void NotifyPaintStart();
void NotifyPaintComplete();

// Get a draw target which the compositor thread can paint to.
already_AddRefed<gfx::DrawTarget> DrawTargetForRemoteDrawing(LayoutDeviceIntSize aSize);

// Called to ignore IPDL messages sent after diverging from the recording,
// except for those needed for compositing.
bool SuppressMessageAfterDiverge(IPC::Message* aMsg);

} // namespace child
} // namespace recordreplay
} // namespace mozilla

#endif // mozilla_recordreplay_ChildIPC_h
