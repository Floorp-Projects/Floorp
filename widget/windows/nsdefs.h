/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSDEFS_H
#define NSDEFS_H

#include <windows.h>

#ifdef _DEBUG
  #define BREAK_TO_DEBUGGER           DebugBreak()
#else   
  #define BREAK_TO_DEBUGGER
#endif  

#ifdef _DEBUG
  #define VERIFY(exp)                 if (!(exp)) { GetLastError(); BREAK_TO_DEBUGGER; }
#else   // !_DEBUG
  #define VERIFY(exp)                 (exp)
#endif  // !_DEBUG

// Win32 logging modules:
// nsWindow, nsSound, and nsClipboard
//
// Logging can be changed at runtime without recompiling in the General
// property page of Visual Studio under the "Environment" property.
//
// Two variables are of importance to be set: 
// MOZ_LOG and MOZ_LOG_FILE
//
// MOZ_LOG:
// MOZ_LOG=all:5                 (To log everything completely)
// MOZ_LOG=nsWindow:5,nsSound:5,nsClipboard:5
//                               (To log windows widget stuff)
// MOZ_LOG=                      (To turn off logging)
//
// MOZ_LOG_FILE:
// MOZ_LOG_FILE=C:\log.txt       (To a file on disk)
// MOZ_LOG_FILE=WinDebug         (To the debug window)
// MOZ_LOG_FILE=                 (To stdout/stderr)

#endif  // NSDEFS_H


