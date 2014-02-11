/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_Debug_h
#define mozilla_Debug_h

#include "nsString.h"
#include <stdio.h>

namespace mozilla {

typedef uint32_t LogOptions;

// Print a message to the specified stream.
const LogOptions kPrintToStream   = 1 << 0;

// Print a message to a debugger if the debugger is attached.
// At the moment, only meaningful on Windows.
const LogOptions kPrintToDebugger = 1 << 1;

// Print an info-level message to the log.
// At the moment, only meaningful on Andriod.
const LogOptions kPrintInfoLog    = 1 << 2;

// Print an error-level message to the log.
// At the moment, only meaningful on Andriod.
const LogOptions kPrintErrorLog   = 1 << 3;

// Print a new line after the message.
const LogOptions kPrintNewLine    = 1 << 4;

// Print aStr to a debugger if the debugger is attached.
void PrintToDebugger(const nsAString& aStr, FILE* aStream,
                     LogOptions aOptions = kPrintToStream
                                         | kPrintToDebugger
                                         | kPrintInfoLog);

} // namespace mozilla

#endif // mozilla_Debug_h
