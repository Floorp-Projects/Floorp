/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PROGRESSUI_H__
#define PROGRESSUI_H__

#include "updatedefines.h"

#if defined(XP_WIN)
  typedef WCHAR NS_tchar;
  #define NS_main wmain
#else
  typedef char NS_tchar;
  #define NS_main main
#endif

// Called to perform any initialization of the widget toolkit
int InitProgressUI(int *argc, NS_tchar ***argv);

#if defined(XP_WIN)
  // Called on the main thread at startup
  int ShowProgressUI(bool indeterminate = false, bool initUIStrings = true);
  int InitProgressUIStrings();
#else
  // Called on the main thread at startup
  int ShowProgressUI();
#endif
// May be called from any thread
void QuitProgressUI();

// May be called from any thread: progress is a number between 0 and 100
void UpdateProgressUI(float progress);

#endif  // PROGRESSUI_H__
