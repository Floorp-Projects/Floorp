/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_recordreplay_ParentIPC_h
#define mozilla_recordreplay_ParentIPC_h

#include "mozilla/dom/ContentChild.h"
#include "mozilla/ipc/MessageChannel.h"
#include "mozilla/ipc/ProcessChild.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/ipc/ScopedXREEmbed.h"
#include "mozilla/ipc/Shmem.h"
#include "js/ReplayHooks.h"

namespace mozilla {
namespace recordreplay {
namespace parent {

// The middleman process is a content process that manages communication with
// one or more child recording or replaying processes. It performs IPC with the
// UI process in the normal fashion for a content process, using the normal
// IPDL protocols. Communication with a recording or replaying process is done
// via a special IPC channel (see Channel.h), and communication with a
// recording process can additionally be done via IPDL messages, usually by
// forwarding them from the UI process.

// UI process API

// Initialize state in a UI process.
void InitializeUIProcess(int aArgc, char** aArgv);

// Get any directory where content process recordings should be saved.
const char* SaveAllRecordingsDirectory();

// Middleman process API

// Save the recording up to the current point in execution.
void SaveRecording(const nsCString& aFilename);

// Get the message channel used to communicate with the UI process.
ipc::MessageChannel* ChannelToUIProcess();

// Initialize state in a middleman process.
void InitializeMiddleman(int aArgc, char* aArgv[], base::ProcessId aParentPid);

// Note the contents of the prefs shmem for use by the child process.
void NotePrefsShmemContents(char* aPrefs, size_t aPrefsLen);

} // namespace parent
} // namespace recordreplay
} // namespace mozilla

#endif // mozilla_recordreplay_ParentIPC_h
