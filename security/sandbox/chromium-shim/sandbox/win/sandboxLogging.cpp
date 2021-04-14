/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "sandboxLogging.h"

#include "base/strings/utf_string_conversions.h"
#include "sandbox/win/src/sandbox_policy.h"

namespace mozilla {
namespace sandboxing {

static LogFunction sLogFunction = nullptr;

void
ProvideLogFunction(LogFunction aLogFunction)
{
  sLogFunction = aLogFunction;
}

void
LogBlocked(const char* aFunctionName, const char* aContext, uint32_t aFramesToSkip)
{
  if (sLogFunction) {
    sLogFunction("BLOCKED", aFunctionName, aContext,
                 /* aShouldLogStackTrace */ true, aFramesToSkip);
  }
}

void
LogBlocked(const char* aFunctionName, const wchar_t* aContext)
{
  if (sLogFunction) {
    // Skip an extra frame to allow for this function.
    LogBlocked(aFunctionName, base::WideToUTF8(aContext).c_str(),
               /* aFramesToSkip */ 3);
  }
}

void
LogBlocked(const char* aFunctionName, const wchar_t* aContext,
           uint16_t aLengthInBytes)
{
  if (sLogFunction) {
    // Skip an extra frame to allow for this function.
    LogBlocked(aFunctionName,
               base::WideToUTF8(std::wstring(aContext, aLengthInBytes / sizeof(wchar_t))).c_str(),
               /* aFramesToSkip */ 3);
  }
}

void
LogAllowed(const char* aFunctionName, const char* aContext)
{
  if (sLogFunction) {
    sLogFunction("Broker ALLOWED", aFunctionName, aContext,
                 /* aShouldLogStackTrace */ false, /* aFramesToSkip */ 0);
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
