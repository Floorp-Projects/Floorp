/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MainThreadUtils_h_
#define MainThreadUtils_h_

#include "nscore.h"

class nsIThread;

/**
 * Get a reference to the main thread.
 *
 * @param aResult
 *   The resulting nsIThread object.
 */
extern nsresult NS_GetMainThread(nsIThread** aResult);

#ifdef MOZILLA_INTERNAL_API
// Fast access to the current thread.  Do not release the returned pointer!  If
// you want to use this pointer from some other thread, then you will need to
// AddRef it.  Otherwise, you should only consider this pointer valid from code
// running on the current thread.
extern nsIThread* NS_GetCurrentThread();
#endif

#ifdef MOZILLA_INTERNAL_API
bool NS_IsMainThread();
#endif

#endif // MainThreadUtils_h_
