/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Common.h"
#include "nsComponentManagerUtils.h"
#include "nsIConsoleService.h"
#include "nsIScriptError.h"
#include "nsServiceManagerUtils.h"

namespace mozilla::glean {

// This is copied from TelemetryCommons.cpp (and modified because consoleservice
// handles threading), but that one is not exported.
// There's _at least_ a third instance of `LogToBrowserConsole`,
// but that one is slightly different.
void LogToBrowserConsole(uint32_t aLogLevel, const nsAString& aMsg) {
  nsCOMPtr<nsIConsoleService> console(
      do_GetService("@mozilla.org/consoleservice;1"));
  if (!console) {
    NS_WARNING("Failed to log message to console.");
    return;
  }

  nsCOMPtr<nsIScriptError> error(do_CreateInstance(NS_SCRIPTERROR_CONTRACTID));
  error->Init(aMsg, u""_ns, u""_ns, 0, 0, aLogLevel, "chrome javascript"_ns,
              false /* from private window */, true /* from chrome context */);
  console->LogMessage(error);
}

}  // namespace mozilla::glean
