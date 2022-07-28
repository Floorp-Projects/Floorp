/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file needs to be linked into libxul, so it can access the JS
// stack and the crash reporter.  Everything else in this directory
// should be able to be linked into its own shared library, in order
// to be able to isolate sandbox/chromium from ipc/chromium.

#include "SandboxInternal.h"
#include "SandboxLogging.h"

#include <unistd.h>
#include <sys/syscall.h>

#include "mozilla/StackWalk.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/Exceptions.h"
#include "nsContentUtils.h"
#include "nsExceptionHandler.h"
#include "nsIException.h"  // for nsIStackFrame
#include "nsString.h"
#include "nsThreadUtils.h"

namespace mozilla {

// Log JS stack info in the same place as the sandbox violation
// message.  Useful in case the responsible code is JS and all we have
// are logs and a minidump with the C++ stacks (e.g., on TBPL).
static void SandboxLogJSStack(void) {
  if (!NS_IsMainThread()) {
    // This might be a worker thread... or it might be a non-JS
    // thread, or a non-NSPR thread.  There's isn't a good API for
    // dealing with this, yet.
    return;
  }
  if (!nsContentUtils::XPConnect()) {
    // There is no content (e.g., the process is a media plugin), in
    // which case this will probably crash and definitely not work.
    return;
  }
  nsCOMPtr<nsIStackFrame> frame = dom::GetCurrentJSStack();
  // If we got a stack, we must have a current JSContext.  This is icky.  :(
  // Would be better if GetCurrentJSStack() handed out the JSContext it ended up
  // using or something.
  JSContext* cx = frame ? nsContentUtils::GetCurrentJSContext() : nullptr;
  for (int i = 0; frame != nullptr; ++i) {
    nsAutoString fileName, funName;
    int32_t lineNumber;

    // Don't stop unwinding if an attribute can't be read.
    fileName.SetIsVoid(true);
    frame->GetFilename(cx, fileName);
    lineNumber = frame->GetLineNumber(cx);
    funName.SetIsVoid(true);
    frame->GetName(cx, funName);

    if (!funName.IsVoid() || !fileName.IsVoid()) {
      SANDBOX_LOG("JS frame %d: %s %s line %d", i,
                  funName.IsVoid() ? "(anonymous)"
                                   : NS_ConvertUTF16toUTF8(funName).get(),
                  fileName.IsVoid() ? "(no file)"
                                    : NS_ConvertUTF16toUTF8(fileName).get(),
                  lineNumber);
    }

    frame = frame->GetCaller(cx);
  }
}

static void SandboxPrintStackFrame(uint32_t aFrameNumber, void* aPC, void* aSP,
                                   void* aClosure) {
  char buf[1024];
  MozCodeAddressDetails details;

  MozDescribeCodeAddress(aPC, &details);
  MozFormatCodeAddressDetails(buf, sizeof(buf), aFrameNumber, aPC, &details);
  SANDBOX_LOG("frame %s", buf);
}

static void SandboxLogCStack(const void* aFirstFramePC) {
  // Warning: this might not print any stack frames.  MozStackWalk
  // can't walk past the signal trampoline on ARM (bug 968531), and
  // x86 frame pointer walking may or may not work (bug 1082276).

  MozStackWalk(SandboxPrintStackFrame, aFirstFramePC, /* max */ 0, nullptr);
  SANDBOX_LOG("end of stack.");
}

static void SandboxCrash(int nr, siginfo_t* info, void* void_context,
                         const void* aFirstFramePC) {
  pid_t pid = getpid(), tid = syscall(__NR_gettid);
  bool dumped = CrashReporter::WriteMinidumpForSigInfo(nr, info, void_context);

  if (!dumped) {
    SANDBOX_LOG(
        "crash reporter is disabled (or failed);"
        " trying stack trace:");
    SandboxLogCStack(aFirstFramePC);
  }

  // Do this last, in case it crashes or deadlocks.
  SandboxLogJSStack();

  // Try to reraise, so the parent sees that this process crashed.
  // (If tgkill is forbidden, then seccomp will raise SIGSYS, which
  // also accomplishes that goal.)
  signal(SIGSYS, SIG_DFL);
  syscall(__NR_tgkill, pid, tid, nr);
}

static void __attribute__((constructor)) SandboxSetCrashFunc() {
  gSandboxCrashFunc = SandboxCrash;
}

}  // namespace mozilla
