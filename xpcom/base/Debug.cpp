/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Debug.h"

#ifdef XP_WIN
#include <windows.h>
#endif

#ifdef XP_WIN

void
mozilla::PrintToDebugger(const char* aStr)
{
  if (::IsDebuggerPresent()) {
    ::OutputDebugStringA(aStr);
  }
}

#endif
