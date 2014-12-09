/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CPR_STDLIB_H_
#define _CPR_STDLIB_H_

#include <stdlib.h>
#include <string.h>

#ifdef SIP_OS_WINDOWS
#include <crtdbg.h>
#include <errno.h>
#endif

#include "mozilla/mozalloc.h"

#define cpr_malloc(a) moz_xmalloc(a)
#define cpr_calloc(a, b) moz_xcalloc(a, b)
#define cpr_realloc(a, b) moz_xrealloc(a, b)
#define cpr_free(a) moz_free(a)

#endif


