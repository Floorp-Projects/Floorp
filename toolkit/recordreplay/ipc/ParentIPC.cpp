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
#include "ChildInternal.h"
#include "InfallibleVector.h"
#include "JSControl.h"
#include "Monitor.h"
#include "ProcessRecordReplay.h"
#include "ProcessRedirect.h"
#include "rrIConnection.h"

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

void SpawnReplayingChild(size_t aChannelId) {
  ChildProcessInfo* child = new ChildProcessInfo(aChannelId, Nothing());
  gReplayingChildren.append(child);
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

StaticInfallibleVector<char> gRecordingContents;

static void SaveRecordingInternal(const ipc::FileDescriptor& aFile) {
  // Make sure the recording file is up to date and ready for copying.
  js::BeforeSaveRecording();

  // Copy the recording's contents to the new file.
  ipc::FileDescriptor::UniquePlatformHandle writefd =
      aFile.ClonePlatformHandle();
  DirectWrite(writefd.get(), gRecordingContents.begin(),
              gRecordingContents.length());

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
// Cloud Processes
///////////////////////////////////////////////////////////////////////////////

bool UseCloudForReplayingProcesses() {
  nsAutoString cloudServer;
  Preferences::GetString("devtools.recordreplay.cloudServer", cloudServer);
  return cloudServer.Length() != 0;
}

static StaticRefPtr<rrIConnection> gConnection;
static StaticInfallibleVector<Channel*> gConnectionChannels;

class SendMessageToCloudRunnable : public Runnable {
 public:
  int32_t mConnectionId;
  Message::UniquePtr mMsg;

  SendMessageToCloudRunnable(int32_t aConnectionId, Message::UniquePtr aMsg)
      : Runnable("SendMessageToCloudRunnable"),
        mConnectionId(aConnectionId), mMsg(std::move(aMsg)) {}

  NS_IMETHODIMP Run() {
    AutoSafeJSContext cx;
    JSAutoRealm ar(cx, xpc::PrivilegedJunkScope());

    JS::RootedObject data(cx, JS::NewArrayBuffer(cx, mMsg->mSize));
    MOZ_RELEASE_ASSERT(data);

    {
      JS::AutoCheckCannotGC nogc;

      bool isSharedMemory;
      uint8_t* ptr = JS::GetArrayBufferData(data, &isSharedMemory, nogc);
      MOZ_RELEASE_ASSERT(ptr);

      memcpy(ptr, mMsg.get(), mMsg->mSize);
    }

    JS::RootedValue dataValue(cx, JS::ObjectValue(*data));
    if (NS_FAILED(gConnection->SendMessage(mConnectionId, dataValue))) {
      MOZ_CRASH("SendMessageToCloud");
    }

    return NS_OK;
  }
};

static bool ConnectionCallback(JSContext* aCx, unsigned aArgc, JS::Value* aVp) {
  JS::CallArgs args = CallArgsFromVp(aArgc, aVp);

  if (!args.get(0).isNumber()) {
    JS_ReportErrorASCII(aCx, "Expected number");
    return false;
  }
  size_t id = args.get(0).toNumber();
  if (id >= gConnectionChannels.length() || !gConnectionChannels[id]) {
    JS_ReportErrorASCII(aCx, "Bad connection channel ID");
    return false;
  }

  if (!args.get(1).isObject()) {
    JS_ReportErrorASCII(aCx, "Expected object");
    return false;
  }

  bool sentData = false;
  {
    JS::AutoCheckCannotGC nogc;

    uint32_t length;
    uint8_t* ptr;
    bool isSharedMemory;
    JS::GetArrayBufferLengthAndData(&args.get(1).toObject(), &length,
                                    &isSharedMemory, &ptr);

    if (ptr) {
      Channel* channel = gConnectionChannels[id];
      channel->SendMessageData((const char*) ptr, length);
      sentData = true;
    }
  }
  if (!sentData) {
    JS_ReportErrorASCII(aCx, "Expected array buffer");
    return false;
  }

  args.rval().setUndefined();
  return true;
}

void CreateReplayingCloudProcess(base::ProcessId aProcessId,
                                 uint32_t aChannelId) {
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());

  if (!gConnection) {
    nsCOMPtr<rrIConnection> connection =
        do_ImportModule("resource://devtools/server/actors/replay/connection.js");
    gConnection = connection.forget();
    ClearOnShutdown(&gConnection);

    AutoSafeJSContext cx;
    JSAutoRealm ar(cx, xpc::PrivilegedJunkScope());

    JSFunction* fun = JS_NewFunction(cx, ConnectionCallback, 2, 0,
                                     "ConnectionCallback");
    MOZ_RELEASE_ASSERT(fun);
    JS::RootedValue callback(cx, JS::ObjectValue(*(JSObject*)fun));
    if (NS_FAILED(gConnection->Initialize(callback))) {
      MOZ_CRASH("CreateReplayingCloudProcess");
    }
  }

  AutoSafeJSContext cx;
  JSAutoRealm ar(cx, xpc::PrivilegedJunkScope());

  nsAutoString cloudServer;
  Preferences::GetString("devtools.recordreplay.cloudServer", cloudServer);
  MOZ_RELEASE_ASSERT(cloudServer.Length() != 0);

  int32_t connectionId;
  if (NS_FAILED(gConnection->Connect(aChannelId, cloudServer, &connectionId))) {
    MOZ_CRASH("CreateReplayingCloudProcess");
  }

  Channel* channel = new Channel(
      aChannelId, Channel::Kind::ParentCloud,
      [=](Message::UniquePtr aMsg) {
        RefPtr<SendMessageToCloudRunnable> runnable =
          new SendMessageToCloudRunnable(connectionId, std::move(aMsg));
        NS_DispatchToMainThread(runnable);
      }, aProcessId);
  while ((size_t)connectionId >= gConnectionChannels.length()) {
    gConnectionChannels.append(nullptr);
  }
  gConnectionChannels[connectionId] = channel;
}

///////////////////////////////////////////////////////////////////////////////
// Initialization
///////////////////////////////////////////////////////////////////////////////

// Message loop processed on the main thread.
static MessageLoop* gMainThreadMessageLoop;

MessageLoop* MainThreadMessageLoop() { return gMainThreadMessageLoop; }

static base::ProcessId gParentPid;

base::ProcessId ParentProcessId() { return gParentPid; }

Monitor* gMonitor;

void InitializeMiddleman(int aArgc, char* aArgv[], base::ProcessId aParentPid,
                         const base::SharedMemoryHandle& aPrefsHandle,
                         const ipc::FileDescriptor& aPrefMapHandle) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  CrashReporter::AnnotateCrashReport(CrashReporter::Annotation::RecordReplay,
                                     true);

  gParentPid = aParentPid;

  // Construct the message that will be sent to each child when starting up.
  IntroductionMessage* msg = IntroductionMessage::New(aParentPid, aArgc, aArgv);
  GetCurrentBuildId(&msg->mBuildId);

  ChildProcessInfo::SetIntroductionMessage(msg);

  MOZ_RELEASE_ASSERT(gProcessKind == ProcessKind::MiddlemanRecording ||
                     gProcessKind == ProcessKind::MiddlemanReplaying);

  InitializeGraphicsMemory();

  gMonitor = new Monitor();

  gMainThreadMessageLoop = MessageLoop::current();

  if (gProcessKind == ProcessKind::MiddlemanRecording) {
    RecordingProcessData data(aPrefsHandle, aPrefMapHandle);
    gRecordingChild = new ChildProcessInfo(0, Some(data));

    // Set the active child to the recording child initially, so that message
    // forwarding works before the middleman control JS has been initialized.
    gActiveChild = gRecordingChild;
  }

  InitializeForwarding();

  if (gProcessKind == ProcessKind::MiddlemanReplaying) {
    // Load the entire recording into memory.
    FileHandle fd = DirectOpenFile(gRecordingFilename, false);

    char buf[4096];
    while (true) {
      size_t n = DirectRead(fd, buf, sizeof(buf));
      if (!n) {
        break;
      }
      gRecordingContents.append(buf, n);
    }

    DirectCloseFile(fd);

    // Update the build ID in the introduction message according to what we
    // find in the recording. The introduction message is sent first to each
    // replaying process, and when replaying in the cloud its contents will be
    // analyzed to determine what binaries to use for the replay.
    Recording::ExtractBuildId(gRecordingContents.begin(),
                              gRecordingContents.length(), &msg->mBuildId);
  }
}

}  // namespace parent
}  // namespace recordreplay
}  // namespace mozilla
