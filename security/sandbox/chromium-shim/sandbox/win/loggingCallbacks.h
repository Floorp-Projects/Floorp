/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef security_sandbox_loggingCallbacks_h__
#define security_sandbox_loggingCallbacks_h__

#include "mozilla/sandboxing/loggingTypes.h"
#include "mozilla/sandboxing/sandboxLogging.h"

#ifdef TARGET_SANDBOX_EXPORTS
#include <sstream>
#include <iostream>

#include "mozilla/Preferences.h"
#include "nsContentUtils.h"

#ifdef MOZ_STACKWALKING
#include "nsStackWalk.h"
#endif

#define TARGET_SANDBOX_EXPORT __declspec(dllexport)
#else
#define TARGET_SANDBOX_EXPORT __declspec(dllimport)
#endif

namespace mozilla {
namespace sandboxing {

// We need to use a callback to work around the fact that sandbox_s lib is
// linked into plugin-container.exe directly and also via xul.dll via
// sandboxbroker.dll. This causes problems with holding the state required to
// implement sandbox logging.
// So, we provide a callback from plugin-container.exe that the code in xul.dll
// can call to make sure we hit the right version of the code.
void TARGET_SANDBOX_EXPORT
SetProvideLogFunctionCb(ProvideLogFunctionCb aProvideLogFunctionCb);

// Provide a call back so a pointer to a logging function can be passed later.
static void
PrepareForLogging()
{
  SetProvideLogFunctionCb(ProvideLogFunction);
}

#ifdef TARGET_SANDBOX_EXPORTS
static ProvideLogFunctionCb sProvideLogFunctionCb = nullptr;

void
SetProvideLogFunctionCb(ProvideLogFunctionCb aProvideLogFunctionCb)
{
  sProvideLogFunctionCb = aProvideLogFunctionCb;
}

#ifdef MOZ_STACKWALKING
static uint32_t sStackTraceDepth = 0;

// NS_WalkStackCallback to write a formatted stack frame to an ostringstream.
static void
StackFrameToOStringStream(uint32_t aFrameNumber, void* aPC, void* aSP,
                          void* aClosure)
{
  std::ostringstream* stream = static_cast<std::ostringstream*>(aClosure);
  nsCodeAddressDetails details;
  char buf[1024];
  NS_DescribeCodeAddress(aPC, &details);
  NS_FormatCodeAddressDetails(buf, sizeof(buf), aFrameNumber, aPC, &details);
  *stream << std::endl << "--" << buf;
  stream->flush();
}
#endif

// Log to the browser console and, if DEBUG build, stderr.
static void
Log(const char* aMessageType,
    const char* aFunctionName,
    const char* aContext,
    const bool aShouldLogStackTrace = false,
    uint32_t aFramesToSkip = 0)
{
  std::ostringstream msgStream;
  msgStream << "Process Sandbox " << aMessageType << ": " << aFunctionName;
  if (aContext) {
    msgStream << " for : " << aContext;
  }

#ifdef MOZ_STACKWALKING
  if (aShouldLogStackTrace) {
    if (sStackTraceDepth) {
      msgStream << std::endl << "Stack Trace:";
      NS_StackWalk(StackFrameToOStringStream, aFramesToSkip, sStackTraceDepth,
                   &msgStream, 0, nullptr);
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
}

// Initialize sandbox logging if required.
static void
InitLoggingIfRequired()
{
  if (!sProvideLogFunctionCb) {
    return;
  }

  if (Preferences::GetBool("security.sandbox.windows.log") ||
      PR_GetEnv("MOZ_WIN_SANDBOX_LOGGING")) {
    sProvideLogFunctionCb(Log);

#if defined(MOZ_CONTENT_SANDBOX) && defined(MOZ_STACKWALKING)
    // We can only log the stack trace on process types where we know that the
    // sandbox won't prevent it.
    if (XRE_GetProcessType() == GeckoProcessType_Content) {
      Preferences::AddUintVarCache(&sStackTraceDepth,
        "security.sandbox.windows.log.stackTraceDepth");
    }
#endif
  }
}
#endif
} // sandboxing
} // mozilla

#endif // security_sandbox_loggingCallbacks_h__
