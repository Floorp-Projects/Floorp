/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_windowscfgstatus_h
#define mozilla_windowscfgstatus_h

#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64) || defined(_M_ARM64))

#include <windows.h>
#include "mozilla/Attributes.h"
#include "mozilla/Types.h"

extern "C" {

/**
 * Tests for Microsoft's Control Flow Guard compiler security feature.
 * This function will crash if CFG is enabled.
 *
 * There is a dependency on the calling convention in
 * toolkit/xre/test/browser_checkcfgstatus.js so be sure to update that
 * if it changes.
 *
 * @returns false if CFG is not enabled. Crashes if CFG is enabled.
 * It will never return true.
 */
MFBT_API bool CFG_DisabledOrCrash();

}

#endif // defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
#endif // mozilla_windowscfgstatus_h
