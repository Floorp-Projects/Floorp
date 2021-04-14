/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "sandboxLogging.h"

#include "base/strings/utf_string_conversions.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "mozilla/Attributes.h"
#include "mozilla/StackWalk.h"

namespace mozilla {
namespace sandboxing {

static LogFunction sLogFunction = nullptr;

void
ProvideLogFunction(LogFunction aLogFunction)
{
  sLogFunction = aLogFunction;
}

static void
LogBlocked(const char* aFunctionName, const char* aContext, const void* aFirstFramePC)
{
  if (sLogFunction) {
    sLogFunction("BLOCKED", aFunctionName, aContext,
                 /* aShouldLogStackTrace */ true, aFirstFramePC);
  }
}

MOZ_NEVER_INLINE void
LogBlocked(const char* aFunctionName, const char* aContext)
{
  if (sLogFunction) {
    LogBlocked(aFunctionName, aContext, CallerPC());
  }
}

MOZ_NEVER_INLINE void
LogBlocked(const char* aFunctionName, const wchar_t* aContext)
{
  if (sLogFunction) {
    LogBlocked(aFunctionName, base::WideToUTF8(aContext).c_str(), CallerPC());
  }
}

MOZ_NEVER_INLINE void
LogBlocked(const char* aFunctionName, const wchar_t* aContext,
           uint16_t aLengthInBytes)
{
  if (sLogFunction) {
    LogBlocked(aFunctionName,
               base::WideToUTF8(std::wstring(aContext, aLengthInBytes / sizeof(wchar_t))).c_str(),
               CallerPC());
  }
}

void
LogAllowed(const char* aFunctionName, const char* aContext)
{
  if (sLogFunction) {
    sLogFunction("Broker ALLOWED", aFunctionName, aContext,
                 /* aShouldLogStackTrace */ false, nullptr);
  }
}

void
LogAllowed(const char* aFunctionName, const wchar_t* aContext)
{
  if (sLogFunction) {
    LogAllowed(aFunctionName, base::WideToUTF8(aContext).c_str());
  }
}

void
LogAllowed(const char* aFunctionName, const wchar_t* aContext,
           uint16_t aLengthInBytes)
{
  if (sLogFunction) {
    LogAllowed(aFunctionName,
               base::WideToUTF8(std::wstring(aContext, aLengthInBytes / sizeof(wchar_t))).c_str());
  }
}

} // sandboxing
} // mozilla
