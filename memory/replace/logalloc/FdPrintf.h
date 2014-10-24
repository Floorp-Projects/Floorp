/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FdPrintf_h__
#define __FdPrintf_h__

/* We can't use libc's (f)printf because it would reenter in replace_malloc,
 * So use a custom and simplified version.
 * Only %p and %z are supported.
 * /!\ This function used a fixed-size internal buffer. The caller is
 * expected to not use a format string that may overflow.
 */
extern void FdPrintf(int aFd, const char* aFormat, ...)
#ifdef __GNUC__
__attribute__((format(printf, 2, 3)))
#endif
;

#endif /* __FdPrintf_h__ */
