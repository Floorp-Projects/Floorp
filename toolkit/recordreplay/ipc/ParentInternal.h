/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_recordreplay_ParentInternal_h
#define mozilla_recordreplay_ParentInternal_h

#include "ParentIPC.h"

#include "Channel.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"

namespace mozilla {
namespace recordreplay {
namespace parent {

// This file has internal declarations for interaction between different
// components of middleman logic.

class ChildProcessInfo;

// Get the message loop for the main thread.
MessageLoop* MainThreadMessageLoop();

// Called when chrome JS can start running and initialization can finish.
void ChromeRegistered();

// Return whether replaying processes are allowed to save checkpoints and
// rewind. Can only be called after PreferencesLoaded().
bool CanRewind();

// Get the current active child process.
ChildProcessInfo* GetActiveChild();

// Get a child process by its ID.
ChildProcessInfo* GetChildProcess(size_t aId);

// Spawn a new replaying child process, returning its ID.
size_t SpawnReplayingChild();

// Specify the current active child.
void SetActiveChild(ChildProcessInfo* aChild);

// Return whether the middleman's main thread is blocked waiting on a
// synchronous IPDL reply from the recording child.
bool MainThreadIsWaitingForIPDLReply();

// If necessary, resume execution in the child before the main thread begins
// to block while waiting on an IPDL reply from the child.
void ResumeBeforeWaitingForIPDLReply();

// Initialize state which handles incoming IPDL messages from the UI and
// recording child processes.
void InitializeForwarding();

// Terminate all children and kill this process.
void Shutdown();

// Monitor used for synchronizing between the main and channel or message loop
// threads.
static Monitor* gMonitor;

///////////////////////////////////////////////////////////////////////////////
// Graphics
///////////////////////////////////////////////////////////////////////////////

extern void* gGraphicsMemory;

void InitializeGraphicsMemory();
void SendGraphicsMemoryToChild();

// Update the graphics painted in the UI process, per painting data received
// from a child process, or null if a repaint was triggered and failed due to
// an unhandled recording divergence.
void UpdateGraphicsInUIProcess(const PaintMessage* aMsg);

// ID for the mach message sent from a child process to the middleman to
// request a port for the graphics shmem.
static const int32_t GraphicsHandshakeMessageId = 42;

// ID for the mach message sent from the middleman to a child process with the
// requested memory for.
static const int32_t GraphicsMemoryMessageId = 43;

// Fixed size of the graphics shared memory buffer.
static const size_t GraphicsMemorySize = 4096 * 4096 * 4;

// Return whether the environment variable activating repaint stress mode is
// set. This makes various changes in both the middleman and child processes to
// trigger a child to diverge from the recording and repaint on every vsync,
// making sure that repainting can handle all the system interactions that
// occur while painting the current tab.
bool InRepaintStressMode();

///////////////////////////////////////////////////////////////////////////////
// Child Processes
///////////////////////////////////////////////////////////////////////////////

// Handle to the underlying recording process, if there is one. Recording
// processes are directly spawned by the middleman at startup, since they need
// to receive all the same IPC which the middleman receives from the UI process
// in order to initialize themselves. Replaying processes are all spawned by
// the UI process itself, due to sandboxing restrictions.
extern ipc::GeckoChildProcessHost* gRecordingProcess;

// Any information needed to spawn a recording child process, in addition to
// the contents of the introduction message.
struct RecordingProcessData {
  // File descriptors that will need to be remapped for the child process.
  const base::SharedMemoryHandle& mPrefsHandle;
  const ipc::FileDescriptor& mPrefMapHandle;

  RecordingProcessData(const base::SharedMemoryHandle& aPrefsHandle,
                       const ipc::FileDescriptor& aPrefMapHandle)
      : mPrefsHandle(aPrefsHandle), mPrefMapHandle(aPrefMapHandle) {}
};

// Information about a recording or replaying child process.
class ChildProcessInfo {
  // Channel for communicating with the process.
  Channel* mChannel;

  // The last time we sent or received a message from this process.
  TimeStamp mLastMessageTime;

  // Whether this process is recording.
  bool mRecording;

  // Whether the process is currently paused.
  bool mPaused;

  // Flags for whether we have received messages from the child indicating it
  // is crashing.
  bool mHasBegunFatalError;
  bool mHasFatalError;

  void OnIncomingMessage(const Message& aMsg);

  static void MaybeProcessPendingMessageRunnable();
  void ReceiveChildMessageOnMainThread(Message::UniquePtr aMsg);

  void OnCrash(const char* aWhy);
  void LaunchSubprocess(
      const Maybe<RecordingProcessData>& aRecordingProcessData);

 public:
  explicit ChildProcessInfo(
      const Maybe<RecordingProcessData>& aRecordingProcessData);
  ~ChildProcessInfo();

  size_t GetId() { return mChannel->GetId(); }
  bool IsRecording() { return mRecording; }
  bool IsPaused() { return mPaused; }

  // Send a message over the underlying channel.
  void SendMessage(Message&& aMessage);

  // Handle incoming messages from this process (and no others) until it pauses.
  // The return value is null if it is already paused, otherwise the message
  // which caused it to pause.
  void WaitUntilPaused();

  static void SetIntroductionMessage(IntroductionMessage* aMessage);
};

}  // namespace parent
}  // namespace recordreplay
}  // namespace mozilla

#endif  // mozilla_recordreplay_ParentInternal_h
