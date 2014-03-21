/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <time.h>

int main(int argc, char **argv)
{
#if defined(OMIT_LIB_BUILD_TIME)
    /*
     * Some platforms don't have any 64-bit integer type
     * such as 'long long'.  Because we can't use NSPR's
     * PR_snprintf in this program, it is difficult to
     * print a static initializer for PRInt64 (a struct).
     * So we print nothing.  The makefiles that build the
     * shared libraries will detect the empty output string
     * of this program and omit the library build time
     * in PRVersionDescription.
     */
#elif defined(_MSC_VER)
    __int64 now;
    time_t sec;

    sec = time(NULL);
    now = (1000000i64) * sec;
    fprintf(stdout, "%I64d", now);
#else
    long long now;
    time_t sec;

    sec = time(NULL);
    now = (1000000LL) * sec;
    fprintf(stdout, "%lld", now);
#endif

    return 0;
}  /* main */

/* now.c */
