/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef security_sandbox_wosEnableCallbacks_h__
#define security_sandbox_wosEnableCallbacks_h__

#include "mozilla/warnonlysandbox/wosTypes.h"
#include "mozilla/warnonlysandbox/warnOnlySandbox.h"

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
namespace warnonlysandbox {

// We need to use a callback to work around the fact that sandbox_s lib is
// linked into plugin-container.exe directly and also via xul.dll via
// sandboxbroker.dll. This causes problems with holding the state required to
// work the warn only sandbox.
// So, we provide a callback from plugin-container.exe that the code in xul.dll
// can call to make sure we hit the right version of the code.
void TARGET_SANDBOX_EXPORT
SetProvideLogFunctionCb(ProvideLogFunctionCb aProvideLogFunctionCb);

// Initialize the warn only sandbox if required.
static void
PrepareForInit()
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
  *stream << "--" << buf << '\n';
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
  msgStream << std::endl;

#ifdef MOZ_STACKWALKING
  if (aShouldLogStackTrace) {
    if (sStackTraceDepth) {
      msgStream << "Stack Trace:" << std::endl;
      NS_StackWalk(StackFrameToOStringStream, aFramesToSkip, sStackTraceDepth,
                   &msgStream, 0, nullptr);
    }
  }
#endif

  std::string msg = msgStream.str();
#ifdef DEBUG
  std::cerr << msg;
#endif

  nsContentUtils::LogMessageToConsole(msg.c_str());
}

// Initialize the warn only sandbox if required.
static void
InitIfRequired()
{
  if (XRE_GetProcessType() == GeckoProcessType_Content
      && Preferences::GetString("browser.tabs.remote.sandbox").EqualsLiteral("warn")
      && sProvideLogFunctionCb) {
#ifdef MOZ_STACKWALKING
    Preferences::AddUintVarCache(&sStackTraceDepth,
      "browser.tabs.remote.sandbox.warnOnlyStackTraceDepth");
#endif
    sProvideLogFunctionCb(Log);
  }
}
#endif
} // warnonlysandbox
} // mozilla

#endif // security_sandbox_wosEnableCallbacks_h__
