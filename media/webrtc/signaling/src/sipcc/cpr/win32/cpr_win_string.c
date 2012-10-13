/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cpr_types.h"
#include "cpr_string.h"
#include <stdio.h>


int
strncasecmp (const char *s1, const char *s2, size_t length)
{
    return (_strnicmp(s1, s2, length));
}

int
strcasecmp (const char *s1, const char *s2)
{

    if ((!s1 && s2) || (s1 && !s2)) {
        return (int) (s1 - s2);
    }

    return (_stricmp(s1, s2));
}


/*
 *  Function: strcasestr
 *
 *  Parameters:
 *
 *  Description: Works like strstr, but ignores case....
 *
 *  Returns:
 *
 */
char *
strcasestr (const char *s1, const char *s2)
{
    const char *cmp;
    const char *wpos;

    for (wpos = (char *) s1; *s1; wpos = ++s1) {
        cmp = s2;

        do {
            if (!*cmp) {
                return (char *) s1;
            }

            if (!*wpos) {
                return NULL;
            }

        } while (toupper(*wpos++) == toupper(*cmp++));
    }
    return NULL;
}


/*
 * upper_string   -  converts a string to all uppercase
 */
void
upper_string (char *str)
{
    while (*str) {
        *(str++) = toupper(*str);
    }
}
