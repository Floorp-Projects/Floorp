/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef security_sandbox_loggingTypes_h__
#define security_sandbox_loggingTypes_h__

#include <stdint.h>

namespace mozilla {
namespace sandboxing {

// We are using callbacks here that are passed in from the core code to prevent
// a circular dependency in the linking during the build.
typedef void (*LogFunction) (const char* aMessageType,
                             const char* aFunctionName,
                             const char* aContext,
                             const bool aShouldLogStackTrace,
                             uint32_t aFramesToSkip);
typedef void (*ProvideLogFunctionCb) (LogFunction aLogFunction);

} // sandboxing
} // mozilla

#endif // security_sandbox_loggingTypes_h__
