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

#include "mozilla/NullPtr.h"
#include "mozilla/unused.h"
#include "mozilla/dom/Exceptions.h"
#include "nsString.h"
#include "nsThreadUtils.h"

#ifdef MOZ_CRASHREPORTER
#include "nsExceptionHandler.h"
#endif

namespace mozilla {

// Log JS stack info in the same place as the sandbox violation
// message.  Useful in case the responsible code is JS and all we have
// are logs and a minidump with the C++ stacks (e.g., on TBPL).
static void
SandboxLogJSStack(void)
{
  if (!NS_IsMainThread()) {
    // This might be a worker thread... or it might be a non-JS
    // thread, or a non-NSPR thread.  There's isn't a good API for
    // dealing with this, yet.
    return;
  }
  nsCOMPtr<nsIStackFrame> frame = dom::GetCurrentJSStack();
  for (int i = 0; frame != nullptr; ++i) {
    nsAutoString fileName, funName;
    int32_t lineNumber;

    // Don't stop unwinding if an attribute can't be read.
    fileName.SetIsVoid(true);
    unused << frame->GetFilename(fileName);
    lineNumber = 0;
    unused << frame->GetLineNumber(&lineNumber);
    funName.SetIsVoid(true);
    unused << frame->GetName(funName);

    if (!funName.IsVoid() || !fileName.IsVoid()) {
      SANDBOX_LOG_ERROR("JS frame %d: %s %s line %d", i,
                        funName.IsVoid() ?
                        "(anonymous)" : NS_ConvertUTF16toUTF8(funName).get(),
                        fileName.IsVoid() ?
                        "(no file)" : NS_ConvertUTF16toUTF8(fileName).get(),
                        lineNumber);
    }

    nsCOMPtr<nsIStackFrame> nextFrame;
    nsresult rv = frame->GetCaller(getter_AddRefs(nextFrame));
    NS_ENSURE_SUCCESS_VOID(rv);
    frame = nextFrame;
  }
}

static void
SandboxCrash(int nr, siginfo_t *info, void *void_context)
{
  pid_t pid = getpid(), tid = syscall(__NR_gettid);

#ifdef MOZ_CRASHREPORTER
  bool dumped = CrashReporter::WriteMinidumpForSigInfo(nr, info, void_context);
  if (!dumped) {
    SANDBOX_LOG_ERROR("Failed to write minidump");
  }
#endif

  // Do this last, in case it crashes or deadlocks.
  SandboxLogJSStack();

  // Try to reraise, so the parent sees that this process crashed.
  // (If tgkill is forbidden, then seccomp will raise SIGSYS, which
  // also accomplishes that goal.)
  signal(SIGSYS, SIG_DFL);
  syscall(__NR_tgkill, pid, tid, nr);
}

static void __attribute__((constructor))
SandboxSetCrashFunc()
{
  gSandboxCrashFunc = SandboxCrash;
}

} // namespace mozilla
