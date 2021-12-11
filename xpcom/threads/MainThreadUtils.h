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
bool NS_IsMainThreadTLSInitialized();
extern "C" {
bool NS_IsMainThread();
}

namespace mozilla {

#  ifdef DEBUG
void AssertIsOnMainThread();
#  else
inline void AssertIsOnMainThread() {}
#  endif

}  // namespace mozilla

#endif

#endif  // MainThreadUtils_h_
