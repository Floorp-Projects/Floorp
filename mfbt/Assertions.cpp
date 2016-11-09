/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Types.h"

/*
 * The crash reason is defined as a global variable here rather than in the
 * crash reporter itself to make it available to all code, even libraries like
 * JS that don't link with the crash reporter directly. This value will only
 * be consumed if the crash reporter is used by the target application.
 */

MOZ_BEGIN_EXTERN_C
MOZ_EXPORT const char* gMozCrashReason = nullptr;
MOZ_END_EXTERN_C
