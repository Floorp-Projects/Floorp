/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file provides implementations for the public IPC API on platforms where
// recording/replaying is not enabled.

#include "ChildIPC.h"
#include "ParentIPC.h"

namespace mozilla {
namespace recordreplay {

namespace child {

void
InitRecordingOrReplayingProcess(int* aArgc, char*** aArgv)
{
}

char*
PrefsShmemContents(size_t aPrefsLen)
{
  MOZ_CRASH();
}

base::ProcessId
MiddlemanProcessId()
{
  MOZ_CRASH();
}

base::ProcessId
ParentProcessId()
{
  MOZ_CRASH();
}

void
SetVsyncObserver(VsyncObserver* aObserver)
{
  MOZ_CRASH();
}

void
NotifyVsyncObserver()
{
  MOZ_CRASH();
}

void
NotifyPaint()
{
  MOZ_CRASH();
}

void
NotifyPaintStart()
{
  MOZ_CRASH();
}

void
NotifyPaintComplete()
{
  MOZ_CRASH();
}

void
WaitForPaintToComplete()
{
  MOZ_CRASH();
}

already_AddRefed<gfx::DrawTarget>
DrawTargetForRemoteDrawing(LayoutDeviceIntSize aSize)
{
  MOZ_CRASH();
}

void
NotifyFlushedRecording()
{
  MOZ_CRASH();
}

void
NotifyAlwaysMarkMajorCheckpoints()
{
  MOZ_CRASH();
}

void
ReportFatalError(const char* aFormat, ...)
{
  MOZ_CRASH();
}

void
BeginIdleTime()
{
  MOZ_CRASH();
}

void
EndIdleTime()
{
  MOZ_CRASH();
}

} // namespace child

namespace parent {

void
InitializeUIProcess(int aArgc, char** aArgv)
{
}

const char*
SaveAllRecordingsDirectory()
{
  return nullptr;
}

void
SaveRecording(const ipc::FileDescriptor& aFile)
{
  MOZ_CRASH();
}

ipc::MessageChannel*
ChannelToUIProcess()
{
  MOZ_CRASH();
}

void
InitializeMiddleman(int aArgc, char* aArgv[], base::ProcessId aParentPid)
{
  MOZ_CRASH();
}

void
NotePrefsShmemContents(char* aPrefs, size_t aPrefsLen)
{
  MOZ_CRASH();
}

void
OpenChannel(base::ProcessId aMiddlemanPid, uint32_t aChannelId, ipc::FileDescriptor* aConnection)
{
  MOZ_CRASH();
}

void
GetArgumentsForChildProcess(base::ProcessId aMiddlemanPid, uint32_t aChannelId,
                            const char* aRecordingFile, bool aRecording,
                            std::vector<std::string>& aExtraArgs)
{
  MOZ_CRASH();
}

} // namespace parent

} // namespace recordreplay
} // namespace mozilla
