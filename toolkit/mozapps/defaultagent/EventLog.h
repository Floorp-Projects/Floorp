/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __DEFAULT_BROWSER_AGENT_EVENT_LOG_H__
#define __DEFAULT_BROWSER_AGENT_EVENT_LOG_H__

#include <windows.h>

MOZ_BEGIN_EXTERN_C

extern MOZ_EXPORT const wchar_t* gWinEventLogSourceName;

MOZ_END_EXTERN_C

void WriteEventLogHresult(HRESULT hr, const char* sourceFile, int sourceLine);
void WriteEventLogErrorMessage(const wchar_t* messageFormat,
                               const char* sourceFile, int sourceLine, ...);

#define LOG_ERROR(hr) WriteEventLogHresult(hr, __FUNCTION__, __LINE__)
#define LOG_ERROR_MESSAGE(format, ...) \
  WriteEventLogErrorMessage(format, __FUNCTION__, __LINE__, ##__VA_ARGS__)

#endif  // __DEFAULT_BROWSER_AGENT_EVENT_LOG_H__
