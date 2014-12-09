/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CPR_WIN_STRING_H_
#define _CPR_WIN_STRING_H_

#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#define bcmp        memcmp
#define bzero(s, n) memset((s), 0, (n))
#define bcopy(src, dst, len) memcpy(dst, src, len); /* XXX Wrong */

int strncasecmp (const char *s1, const char *s2, size_t length);
int strcasecmp (const char *s1, const char *s2);
char *strcasestr (const char *s1, const char *s2);
void upper_string (char *str);

#endif
