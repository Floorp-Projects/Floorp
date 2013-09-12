/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This is an implementation of stack unwinding according to a subset
 * of the ARM Exception Handling ABI; see the comment at the top of
 * the .cpp file for details.
 */

#ifndef mozilla_EHABIStackWalk_h__
#define mozilla_EHABIStackWalk_h__

#include <stddef.h>

#ifdef ANDROID
# include "android-signal-defs.h"
#else
# include <ucontext.h>
#endif

namespace mozilla {

void EHABIStackWalkInit();

size_t EHABIStackWalk(const mcontext_t &aContext, void *stackBase,
                      void **aSPs, void **aPCs, size_t aNumFrames);

}

#endif
