/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Debug.h"

#ifdef XP_WIN
#include <io.h>
#include <windows.h>
#endif

#ifdef ANDROID
#include <android/log.h>
#endif

void mozilla::PrintToDebugger(const nsAString& aStr, FILE* aStream,
                              LogOptions aOptions)
{
  nsString msg(aStr);
  if (aOptions & kPrintNewLine) {
    msg.AppendLiteral("\n");
  }

#ifdef XP_WIN
  if ((aOptions & kPrintToDebugger) && ::IsDebuggerPresent()) {
    ::OutputDebugStringW(msg.get());
  }

  if (!(aOptions & kPrintToStream)) {
    return;
  }

  int fd = _fileno(aStream);
  if (_isatty(fd)) {
    fflush(aStream);
    DWORD writtenCount;
    WriteConsoleW(reinterpret_cast<HANDLE>(_get_osfhandle(fd)),
                  msg.BeginReading(), msg.Length(),
                  &writtenCount, nullptr);
    return;
  }
#endif
  NS_ConvertUTF16toUTF8 cstr(msg);
#ifdef ANDROID
  if (aOptions & (kPrintInfoLog | kPrintErrorLog)) {
    __android_log_write(aOptions & kPrintErrorLog ? ANDROID_LOG_ERROR
                                                  : ANDROID_LOG_INFO,
                        "GeckoDump", cstr.get());
  }
#endif

#ifndef XP_WIN
  if (!(aOptions & kPrintToStream)) {
    return;
  }
#endif

#if defined(XP_MACOSX)
  // have to convert \r to \n so that printing to the console works
  cstr.ReplaceChar('\r', '\n');
#endif

  fputs(cstr.get(), aStream);
  fflush(aStream);
}
