/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CPR_WIN_STDIO_H_
#define _CPR_WIN_STDIO_H_

#include "cpr_types.h"
#include <stdio.h>


#define snprintf cpr_win_snprintf
int cpr_win_snprintf(char *buffer, size_t n, const char *format, ...);

#endif
