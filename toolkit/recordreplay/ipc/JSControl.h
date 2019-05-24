/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_recordreplay_JSControl_h
#define mozilla_recordreplay_JSControl_h

#include "jsapi.h"

#include "InfallibleVector.h"
#include "ProcessRewind.h"

#include "mozilla/DefineEnum.h"
#include "nsString.h"

namespace mozilla {
namespace recordreplay {

struct Message;

namespace parent {
  class ChildProcessInfo;
}

namespace js {

// This file manages interactions between the record/replay infrastructure and
// JS code. This interaction can occur in two ways:
//
// - In the middleman process, devtools server code can use the
//   RecordReplayControl object to send requests to the recording/replaying
//   child process and control its behavior.
//
// - In the recording/replaying process, a JS sandbox is created before the
//   first checkpoint is reached, which responds to the middleman's requests.
//   The RecordReplayControl object is also provided here, but has a different
//   interface which allows the JS to query the current process.

// Buffer type used for encoding object data.
typedef InfallibleVector<char16_t> CharBuffer;

// Set up the JS sandbox in the current recording/replaying process and load
// its target script.
void SetupDevtoolsSandbox();

// JS state is initialized when the first checkpoint is reached.
bool IsInitialized();

// Start processing a manifest received from the middleman.
void ManifestStart(const CharBuffer& aContents);

// The following hooks are used in the middleman process to call methods defined
// by the middleman control logic.

// Setup the middleman control state.
void SetupMiddlemanControl(const Maybe<size_t>& aRecordingChildId);

// Handle an incoming message from a child process.
void ForwardManifestFinished(parent::ChildProcessInfo* aChild,
                             const Message& aMsg);

// Prepare the child processes so that the recording file can be safely copied.
void BeforeSaveRecording();
void AfterSaveRecording();

// The following hooks are used in the recording/replaying process to
// call methods defined by the JS sandbox.

// Called when running forward, immediately before hitting a normal or
// temporary checkpoint.
void BeforeCheckpoint();

// Called immediately after hitting a normal or temporary checkpoint, either
// when running forward or immediately after rewinding. aRestoredCheckpoint is
// true if we just rewound.
void AfterCheckpoint(size_t aCheckpoint, bool aRestoredCheckpoint);

// Accessors for state which can be accessed from JS.

// Mark a time span when the main thread is idle.
void BeginIdleTime();
void EndIdleTime();

}  // namespace js
}  // namespace recordreplay
}  // namespace mozilla

#endif  // mozilla_recordreplay_JSControl_h
