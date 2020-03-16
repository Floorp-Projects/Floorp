/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EventLog.h"

#include <stdio.h>

void WriteEventLogError(HRESULT hr, const char* sourceFile, int sourceLine) {
  HANDLE source = RegisterEventSourceW(
      nullptr, L"" MOZ_APP_DISPLAYNAME " Default Browser Agent");
  if (!source) {
    // Not much we can do about this.
    return;
  }

  // The size of this buffer is arbitrary, but it should easily be enough
  // unless we come up with a really excessively long function name.
  wchar_t errorStr[MAX_PATH + 1] = L"";
  _snwprintf_s(errorStr, MAX_PATH + 1, _TRUNCATE, L"0x%X in %S:%d", hr,
               sourceFile, sourceLine);

  const wchar_t* stringsArray[] = {errorStr};
  ReportEventW(source, EVENTLOG_ERROR_TYPE, 0, hr, nullptr, 1, 0, stringsArray,
               nullptr);

  DeregisterEventSource(source);
}
