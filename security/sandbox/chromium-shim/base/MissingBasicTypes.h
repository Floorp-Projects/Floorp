/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef security_sandbox_MissingBasicTypes_h__
#define security_sandbox_MissingBasicTypes_h__

#include <stdint.h>

// These types are still used by the Chromium sandbox code. When referencing
// Chromium sandbox code from Gecko we can't use the normal base/basictypes.h as
// it clashes with the one from ipc/chromium/src/base/. These types have been
// removed from the one in ipc/chromium/src/base/.
typedef int8_t int8;
typedef uint8_t uint8;
typedef int16_t int16;
typedef uint16_t uint16;
typedef int32_t int32;
typedef uint32_t uint32;
typedef int64_t int64;
typedef uint64_t uint64;

#endif  // security_sandbox_MissingBasicTypes_h__
