/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_Debug_h__
#define mozilla_Debug_h__

namespace mozilla {

#ifdef XP_WIN

// Print aStr to a debugger if the debugger is attached.
void PrintToDebugger(const char* aStr);

#endif

} // namespace mozilla

#endif // mozilla_Debug_h__
