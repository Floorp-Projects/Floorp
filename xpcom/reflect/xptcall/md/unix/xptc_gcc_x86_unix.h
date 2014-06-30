/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Special include file for xptc*_gcc_x86_unix.cpp */

// 
// this may improve the static function calls, but may not.
//

#ifdef MOZ_NEED_LEADING_UNDERSCORE
#define SYMBOL_UNDERSCORE "_"
#else
#define SYMBOL_UNDERSCORE
#endif

