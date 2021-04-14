/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef security_sandbox_loggingCallbacks_h__
#define security_sandbox_loggingCallbacks_h__

#include <sstream>

#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_security.h"
#include "mozilla/sandboxing/loggingTypes.h"
#include "nsContentUtils.h"

#include "mozilla/StackWalk.h"

namespace mozilla {

static LazyLogModule sSandboxTargetLog("SandboxTarget");

#define LOG_D(...) MOZ_LOG(sSandboxTargetLog, LogLevel::Debug, (__VA_ARGS__))

namespace sandboxing {

// NS_WalkStackCallback to write a formatted stack frame to an ostringstream.
static void
StackFrameToOStringStream(uint32_t aFrameNumber, void* aPC, void* aSP,
                          void* aClosure)
{
  std::ostringstream* stream = static_cast<std::ostringstream*>(aClosure);
  MozCodeAddressDetails details;
  char buf[1024];
  MozDescribeCodeAddress(aPC, &details);
  MozFormatCodeAddressDetails(buf, sizeof(buf), aFrameNumber, aPC, &details);
  *stream << std::endl << "--" << buf;
  stream->flush();
}

// Log to the browser console and, if DEBUG build, stderr.
static void
Log(const char* aMessageType,
    const char* aFunctionName,
    const char* aContext,
    const bool aShouldLogStackTrace = false,
    const void* aFirstFramePC = nullptr)
{
  std::ostringstream msgStream;
  msgStream << "Process Sandbox " << aMessageType << ": " << aFunctionName;
  if (aContext) {
    msgStream << " for : " << aContext;
  }

#if defined(MOZ_SANDBOX)
  // We can only log the stack trace on process types where we know that the
  // sandbox won't prevent it.
  if (XRE_IsContentProcess() && aShouldLogStackTrace) {
    auto stackTraceDepth =
        StaticPrefs::security_sandbox_windows_log_stackTraceDepth();
    if (stackTraceDepth) {
      msgStream << std::endl << "Stack Trace:";
      MozStackWalk(StackFrameToOStringStream, aFirstFramePC, stackTraceDepth,
                   &msgStream);
    }
  }
#endif
  std::string msg = msgStream.str();
#if defined(DEBUG)
  // Use NS_DebugBreak directly as we want child process prefix, but not source
  // file or line number.
  NS_DebugBreak(NS_DEBUG_WARNING, nullptr, msg.c_str(), nullptr, -1);
#endif

  if (nsContentUtils::IsInitialized()) {
    nsContentUtils::LogMessageToConsole(msg.c_str());
  }

  // As we don't always have the facility to log to console use MOZ_LOG as well.
  LOG_D("%s", msg.c_str());
}

// Initialize sandbox logging if required.
static void
InitLoggingIfRequired(ProvideLogFunctionCb aProvideLogFunctionCb)
{
  if (!aProvideLogFunctionCb) {
    return;
  }

  if (Preferences::GetBool("security.sandbox.logging.enabled") ||
      PR_GetEnv("MOZ_SANDBOX_LOGGING")) {
    aProvideLogFunctionCb(Log);
  }
}

} // sandboxing
} // mozilla

#endif // security_sandbox_loggingCallbacks_h__
