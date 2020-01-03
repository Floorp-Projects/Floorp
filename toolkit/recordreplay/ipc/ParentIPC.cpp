/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file has the logic which the middleman process uses to communicate with
// the parent process and with the replayed process.

#include "ParentInternal.h"

#include "base/task.h"
#include "ipc/Channel.h"
#include "js/Proxy.h"
#include "mozilla/dom/ContentProcessMessageManager.h"
#include "InfallibleVector.h"
#include "JSControl.h"
#include "Monitor.h"
#include "ProcessRecordReplay.h"
#include "ProcessRedirect.h"

#include <algorithm>

using std::min;

namespace mozilla {
namespace recordreplay {
namespace parent {

///////////////////////////////////////////////////////////////////////////////
// UI Process State
///////////////////////////////////////////////////////////////////////////////

const char* gSaveAllRecordingsDirectory = nullptr;

void InitializeUIProcess(int aArgc, char** aArgv) {
  for (int i = 0; i < aArgc; i++) {
    if (!strcmp(aArgv[i], "--save-recordings") && i + 1 < aArgc) {
      gSaveAllRecordingsDirectory = strdup(aArgv[i + 1]);
    }
  }
}

const char* SaveAllRecordingsDirectory() {
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
  return gSaveAllRecordingsDirectory;
}

///////////////////////////////////////////////////////////////////////////////
// Child Processes
///////////////////////////////////////////////////////////////////////////////

// The single recording child process, or null.
static ChildProcessInfo* gRecordingChild;

// Any replaying child processes that have been spawned.
static StaticInfallibleVector<UniquePtr<ChildProcessInfo>> gReplayingChildren;

// The currently active child process.
static ChildProcessInfo* gActiveChild;

void Shutdown() {
  delete gRecordingChild;
  gReplayingChildren.clear();
  _exit(0);
}

bool IsMiddlemanWithRecordingChild() {
  return IsMiddleman() && gRecordingChild;
}

ChildProcessInfo* GetActiveChild() { return gActiveChild; }

ChildProcessInfo* GetChildProcess(size_t aId) {
  if (gRecordingChild && gRecordingChild->GetId() == aId) {
    return gRecordingChild;
  }
  for (const auto& child : gReplayingChildren) {
    if (child->GetId() == aId) {
      return child.get();
    }
  }
  return nullptr;
}

size_t SpawnReplayingChild() {
  ChildProcessInfo* child = new ChildProcessInfo(Nothing());
  gReplayingChildren.append(child);
  return child->GetId();
}

void ResumeBeforeWaitingForIPDLReply() {
  MOZ_RELEASE_ASSERT(gActiveChild->IsRecording());

  // The main thread is about to block while it waits for a sync reply from the
  // recording child process. If the child is paused, resume it immediately so
  // that we don't deadlock.
  if (gActiveChild->IsPaused()) {
    MOZ_CRASH("NYI");
  }
}

///////////////////////////////////////////////////////////////////////////////
// Preferences
///////////////////////////////////////////////////////////////////////////////

static bool gChromeRegistered;
static bool gRewindingEnabled;

void ChromeRegistered() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (gChromeRegistered) {
    return;
  }
  gChromeRegistered = true;

  gRewindingEnabled =
      Preferences::GetBool("devtools.recordreplay.enableRewinding");

  // Force-disable rewinding and saving checkpoints with an env var for testing.
  if (getenv("NO_REWIND")) {
    gRewindingEnabled = false;
  }

  Maybe<size_t> recordingChildId;

  if (gRecordingChild) {
    // Inform the recording child if we will be running devtools server code in
    // this process.
    if (DebuggerRunsInMiddleman()) {
      gRecordingChild->SendMessage(SetDebuggerRunsInMiddlemanMessage());
    }
    recordingChildId.emplace(gRecordingChild->GetId());
  }

  js::SetupMiddlemanControl(recordingChildId);
}

bool CanRewind() {
  MOZ_RELEASE_ASSERT(gChromeRegistered);
  return gRewindingEnabled;
}

bool DebuggerRunsInMiddleman() {
  if (IsRecordingOrReplaying()) {
    // This can be called in recording/replaying processes as well as the
    // middleman. Fetch the value which the middleman informed us of.
    return child::DebuggerRunsInMiddleman();
  }

  // Middleman processes which are recording and can't rewind do not run
  // developer tools server code. This will run in the recording process
  // instead.
  MOZ_RELEASE_ASSERT(IsMiddleman());
  return !gRecordingChild || CanRewind();
}

///////////////////////////////////////////////////////////////////////////////
// Saving Recordings
///////////////////////////////////////////////////////////////////////////////

// Handle to the recording file opened at startup.
static FileHandle gRecordingFd;

static void SaveRecordingInternal(const ipc::FileDescriptor& aFile) {
  // Make sure the recording file is up to date and ready for copying.
  js::BeforeSaveRecording();

  // Copy the file's contents to the new file.
  DirectSeekFile(gRecordingFd, 0);
  ipc::FileDescriptor::UniquePlatformHandle writefd =
      aFile.ClonePlatformHandle();
  char buf[4096];
  while (true) {
    size_t n = DirectRead(gRecordingFd, buf, sizeof(buf));
    if (!n) {
      break;
    }
    DirectWrite(writefd.get(), buf, n);
  }

  PrintSpew("Saved Recording Copy.\n");

  js::AfterSaveRecording();
}

void SaveRecording(const ipc::FileDescriptor& aFile) {
  MOZ_RELEASE_ASSERT(IsMiddleman());

  if (NS_IsMainThread()) {
    SaveRecordingInternal(aFile);
  } else {
    MainThreadMessageLoop()->PostTask(NewRunnableFunction(
        "SaveRecordingInternal", SaveRecordingInternal, aFile));
  }
}

///////////////////////////////////////////////////////////////////////////////
// Initialization
///////////////////////////////////////////////////////////////////////////////

// Message loop processed on the main thread.
static MessageLoop* gMainThreadMessageLoop;

MessageLoop* MainThreadMessageLoop() { return gMainThreadMessageLoop; }

static base::ProcessId gParentPid;

base::ProcessId ParentProcessId() { return gParentPid; }

void InitializeMiddleman(int aArgc, char* aArgv[], base::ProcessId aParentPid,
                         const base::SharedMemoryHandle& aPrefsHandle,
                         const ipc::FileDescriptor& aPrefMapHandle) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  CrashReporter::AnnotateCrashReport(CrashReporter::Annotation::RecordReplay,
                                     true);

  gParentPid = aParentPid;

  // Construct the message that will be sent to each child when starting up.
  IntroductionMessage* msg = IntroductionMessage::New(aParentPid, aArgc, aArgv);
  ChildProcessInfo::SetIntroductionMessage(msg);

  MOZ_RELEASE_ASSERT(gProcessKind == ProcessKind::MiddlemanRecording ||
                     gProcessKind == ProcessKind::MiddlemanReplaying);

  InitializeGraphicsMemory();

  gMonitor = new Monitor();

  gMainThreadMessageLoop = MessageLoop::current();

  if (gProcessKind == ProcessKind::MiddlemanRecording) {
    RecordingProcessData data(aPrefsHandle, aPrefMapHandle);
    gRecordingChild = new ChildProcessInfo(Some(data));

    // Set the active child to the recording child initially, so that message
    // forwarding works before the middleman control JS has been initialized.
    gActiveChild = gRecordingChild;
  }

  InitializeForwarding();

  // Open a file handle to the recording file we can use for saving recordings
  // later on.
  gRecordingFd = DirectOpenFile(gRecordingFilename, false);
}

}  // namespace parent
}  // namespace recordreplay
}  // namespace mozilla
