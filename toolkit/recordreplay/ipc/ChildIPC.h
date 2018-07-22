/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_recordreplay_ChildIPC_h
#define mozilla_recordreplay_ChildIPC_h

#include "base/process.h"

namespace mozilla {
namespace recordreplay {
namespace child {

// Naively replaying a child process execution will not perform any IPC. When
// the replaying process attempts to make system calls that communicate with
// the parent, function redirections are invoked that simply replay the values
// which those calls produced in the original recording.
//
// The replayed process needs to be able to communicate with the parent in some
// ways, however. IPDL messages need to be sent to the compositor in the parent
// to render graphics, and the parent needs to send messages to the client to
// control and debug the replay.
//
// This file manages the real IPC which occurs in a replaying process. New
// threads --- which did not existing while recording --- are spawned to manage
// IPC with the middleman process, and IPDL actors are created up front for use
// in communicating with the middleman using the PReplay protocol.

///////////////////////////////////////////////////////////////////////////////
// Public API
///////////////////////////////////////////////////////////////////////////////

// Initialize replaying IPC state. This is called once during process startup,
// and is a no-op if the process is not replaying.
void InitRecordingOrReplayingProcess(base::ProcessId aParentPid,
                                     int* aArgc, char*** aArgv);

// Get the contents of the prefs shmem as conveyed to the middleman process.
char* PrefsShmemContents(size_t aPrefsLen);

base::ProcessId MiddlemanProcessId();
base::ProcessId ParentProcessId();

// Notify the middleman that the recording was flushed.
void NotifyFlushedRecording();

// Notify the middleman about an AlwaysMarkMajorCheckpoints directive.
void NotifyAlwaysMarkMajorCheckpoints();

// Report a fatal error to the middleman process.
void ReportFatalError(const char* aFormat, ...);

// Mark a time span when the main thread is idle.
void BeginIdleTime();
void EndIdleTime();

} // namespace child
} // namespace recordreplay
} // namespace mozilla

#endif // mozilla_recordreplay_ChildIPC_h
