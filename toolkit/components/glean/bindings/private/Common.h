/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_glean_Common_h
#define mozilla_glean_Common_h

#include "nsIScriptError.h"

namespace mozilla::glean {

/**
 * Dumps a log message to the Browser Console using the provided level.
 *
 * @param aLogLevel The level to use when displaying the message in the browser
 * console (e.g. nsIScriptError::warningFlag, ...).
 * @param aMsg The text message to print to the console.
 */
void LogToBrowserConsole(uint32_t aLogLevel, const nsAString& aMsg);

}  // namespace mozilla::glean

#endif /* mozilla_glean_Common_h */
