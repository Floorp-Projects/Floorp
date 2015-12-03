/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Set of helper methods to implement logging for Windows sandbox.
 */

#ifndef security_sandbox_sandboxLogging_h__
#define security_sandbox_sandboxLogging_h__

#include "loggingTypes.h"

#ifdef SANDBOX_EXPORTS
#define SANDBOX_EXPORT __declspec(dllexport)
#else
#define SANDBOX_EXPORT __declspec(dllimport)
#endif

namespace sandbox {
class TargetPolicy;
}

namespace mozilla {
namespace sandboxing {

// This is used to pass a LogCallback to the sandboxing code, as the logging
// requires code to which we cannot link directly.
void SANDBOX_EXPORT ProvideLogFunction(LogFunction aLogFunction);

// Set up dummy interceptions via the broker, so we can log calls.
void ApplyLoggingPolicy(sandbox::TargetPolicy& aPolicy);

// Log a "BLOCKED" msg to the browser console and, if DEBUG build, stderr.
// If the logging of a stack trace is enabled then the default aFramesToSkip
// will start from our caller's caller, which should normally be the function
// that triggered the interception.
void LogBlocked(const char* aFunctionName, const char* aContext = nullptr,
                uint32_t aFramesToSkip = 2);

// Convenience functions to convert to char*.
void LogBlocked(const char* aFunctionName, const wchar_t* aContext);
void LogBlocked(const char* aFunctionName, const wchar_t* aContext,
                uint16_t aLengthInBytes);

// Log a "ALLOWED" msg to the browser console and, if DEBUG build, stderr.
void LogAllowed(const char* aFunctionName, const char* aContext = nullptr);

// Convenience functions to convert to char*.
void LogAllowed(const char* aFunctionName, const wchar_t* aContext);
void LogAllowed(const char* aFunctionName, const wchar_t* aContext,
                uint16_t aLengthInBytes);


} // sandboxing
} // mozilla

#endif // security_sandbox_sandboxLogging_h__
