/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "warnOnlySandbox.h"

#include "base/strings/utf_string_conversions.h"
#include "sandbox/win/src/sandbox_policy.h"

namespace mozilla {
namespace warnonlysandbox {

void
ApplyWarnOnlyPolicy(sandbox::TargetPolicy& aPolicy)
{
  // Add rules to allow everything that we can, so that we can add logging to
  // warn when we would be blocked by the sandbox.
  aPolicy.AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                  sandbox::TargetPolicy::FILES_ALLOW_ANY, L"*");
  aPolicy.AddRule(sandbox::TargetPolicy::SUBSYS_NAMED_PIPES,
                  sandbox::TargetPolicy::NAMEDPIPES_ALLOW_ANY, L"*");
  aPolicy.AddRule(sandbox::TargetPolicy::SUBSYS_PROCESS,
                  sandbox::TargetPolicy::PROCESS_ALL_EXEC, L"*");
  aPolicy.AddRule(sandbox::TargetPolicy::SUBSYS_REGISTRY,
                  sandbox::TargetPolicy::REG_ALLOW_ANY,
                  L"HKEY_CLASSES_ROOT\\*");
  aPolicy.AddRule(sandbox::TargetPolicy::SUBSYS_REGISTRY,
                  sandbox::TargetPolicy::REG_ALLOW_ANY,
                  L"HKEY_CURRENT_USER\\*");
  aPolicy.AddRule(sandbox::TargetPolicy::SUBSYS_REGISTRY,
                  sandbox::TargetPolicy::REG_ALLOW_ANY,
                  L"HKEY_LOCAL_MACHINE\\*");
  aPolicy.AddRule(sandbox::TargetPolicy::SUBSYS_REGISTRY,
                  sandbox::TargetPolicy::REG_ALLOW_ANY,
                  L"HKEY_USERS\\*");
  aPolicy.AddRule(sandbox::TargetPolicy::SUBSYS_REGISTRY,
                  sandbox::TargetPolicy::REG_ALLOW_ANY,
                  L"HKEY_PERFORMANCE_DATA\\*");
  aPolicy.AddRule(sandbox::TargetPolicy::SUBSYS_REGISTRY,
                  sandbox::TargetPolicy::REG_ALLOW_ANY,
                  L"HKEY_PERFORMANCE_TEXT\\*");
  aPolicy.AddRule(sandbox::TargetPolicy::SUBSYS_REGISTRY,
                  sandbox::TargetPolicy::REG_ALLOW_ANY,
                  L"HKEY_PERFORMANCE_NLSTEXT\\*");
  aPolicy.AddRule(sandbox::TargetPolicy::SUBSYS_REGISTRY,
                  sandbox::TargetPolicy::REG_ALLOW_ANY,
                  L"HKEY_CURRENT_CONFIG\\*");
  aPolicy.AddRule(sandbox::TargetPolicy::SUBSYS_REGISTRY,
                  sandbox::TargetPolicy::REG_ALLOW_ANY,
                  L"HKEY_DYN_DATA\\*");
  aPolicy.AddRule(sandbox::TargetPolicy::SUBSYS_SYNC,
                  sandbox::TargetPolicy::EVENTS_ALLOW_ANY, L"*");
  aPolicy.AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                  sandbox::TargetPolicy::HANDLES_DUP_ANY, L"*");
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
    LogBlocked(aFunctionName,  WideToUTF8(aContext).c_str(), /* aFramesToSkip */ 3);
  }
}

void
LogBlocked(const char* aFunctionName, const wchar_t* aContext,
           uint16_t aLength)
{
  if (sLogFunction) {
    // Skip an extra frame to allow for this function.
    LogBlocked(aFunctionName, WideToUTF8(std::wstring(aContext, aLength)).c_str(),
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
    LogAllowed(aFunctionName, WideToUTF8(aContext).c_str());
  }
}

void
LogAllowed(const char* aFunctionName, const wchar_t* aContext,
           uint16_t aLength)
{
  if (sLogFunction) {
    LogAllowed(aFunctionName, WideToUTF8(std::wstring(aContext, aLength)).c_str());
  }
}

} // warnonlysandbox
} // mozilla
