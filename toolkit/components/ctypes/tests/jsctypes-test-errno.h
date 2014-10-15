/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Attributes.h"
#include "mozilla/Types.h"

#define EXPORT_CDECL(type)   MOZ_EXPORT type

MOZ_BEGIN_EXTERN_C

  EXPORT_CDECL(void) set_errno(int status);
  EXPORT_CDECL(int) get_errno();

#if defined(XP_WIN)
  EXPORT_CDECL(void) set_last_error(int status);
  EXPORT_CDECL(int) get_last_error();
#endif // defined(XP_WIN)

MOZ_END_EXTERN_C
