/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/*
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * This module converts keysym values into the corresponding ISO 10646-1
 * (UCS, Unicode) values.
 */

#ifdef MOZ_X11
#include <X11/X.h>
#else
#define KeySym unsigned int
#endif /* MOZ_X11 */

#ifdef __cplusplus
extern "C" { 
#endif

long keysym2ucs(KeySym keysym); 

#ifdef __cplusplus
} /* extern "C" */
#endif



