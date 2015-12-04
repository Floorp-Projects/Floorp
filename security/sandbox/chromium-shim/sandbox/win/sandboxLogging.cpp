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

void
ApplyLoggingPolicy(sandbox::TargetPolicy& aPolicy)
{
  // Add dummy rules, so that we can log in the interception code.
  // We already have a file interception set up for the client side of pipes.
  // Also, passing just "dummy" for file system policy causes win_utils.cc
  // IsReparsePoint() to loop.
  aPolicy.AddRule(sandbox::TargetPolicy::SUBSYS_NAMED_PIPES,
                  sandbox::TargetPolicy::NAMEDPIPES_ALLOW_ANY, L"dummy");
  aPolicy.AddRule(sandbox::TargetPolicy::SUBSYS_PROCESS,
                  sandbox::TargetPolicy::PROCESS_MIN_EXEC, L"dummy");
  aPolicy.AddRule(sandbox::TargetPolicy::SUBSYS_REGISTRY,
                  sandbox::TargetPolicy::REG_ALLOW_READONLY,
                  L"HKEY_CURRENT_USER\\dummy");
  aPolicy.AddRule(sandbox::TargetPolicy::SUBSYS_SYNC,
                  sandbox::TargetPolicy::EVENTS_ALLOW_READONLY, L"dummy");
  aPolicy.AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                  sandbox::TargetPolicy::HANDLES_DUP_BROKER, L"dummy");
}

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
