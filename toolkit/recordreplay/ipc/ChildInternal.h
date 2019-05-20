/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_recordreplay_ChildInternal_h
#define mozilla_recordreplay_ChildInternal_h

#include "Channel.h"
#include "ChildIPC.h"
#include "JSControl.h"
#include "MiddlemanCall.h"
#include "Monitor.h"

// This file has internal definitions for communication between the main
// record/replay infrastructure and child side IPC code.

namespace mozilla {
namespace recordreplay {
namespace child {

// Optional information about a crash that occurred. If not provided to
// ReportFatalError, the current thread will be treated as crashed.
struct MinidumpInfo {
  int mExceptionType;
  int mCode;
  int mSubcode;
  mach_port_t mThread;

  MinidumpInfo(int aExceptionType, int aCode, int aSubcode, mach_port_t aThread)
      : mExceptionType(aExceptionType),
        mCode(aCode),
        mSubcode(aSubcode),
        mThread(aThread) {}
};

// Generate a minidump and report a fatal error to the middleman process.
void ReportFatalError(const Maybe<MinidumpInfo>& aMinidumpInfo,
                      const char* aFormat, ...);

// Monitor used for various synchronization tasks.
extern Monitor* gMonitor;

// Whether the middleman runs developer tools server code.
bool DebuggerRunsInMiddleman();

// Notify the middleman that the last manifest was finished.
void ManifestFinished(const js::CharBuffer& aResponse);

// Send messages operating on middleman calls.
void SendMiddlemanCallRequest(const char* aInputData, size_t aInputSize,
                              InfallibleVector<char>* aOutputData);
void SendResetMiddlemanCalls();

// Return whether a repaint is in progress and is not allowed to trigger an
// unhandled recording divergence per preferences.
bool CurrentRepaintCannotFail();

}  // namespace child
}  // namespace recordreplay
}  // namespace mozilla

#endif  // mozilla_recordreplay_ChildInternal_h
