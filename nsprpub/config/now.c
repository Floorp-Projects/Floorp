/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>

#if defined(XP_UNIX) || defined(XP_OS2) || defined(XP_BEOS)
#include <sys/time.h>
#elif defined(_WIN32)
#include <windows.h>
#else
#error "Architecture not supported"
#endif


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
#elif defined(XP_UNIX) || defined(XP_OS2) || defined(XP_BEOS)
    long long now;
    struct timeval tv;
#ifdef HAVE_SVID_GETTOD
    gettimeofday(&tv);
#else
    gettimeofday(&tv, NULL);
#endif
    now = ((1000000LL) * tv.tv_sec) + (long long)tv.tv_usec;
#if defined(OSF1)
    fprintf(stdout, "%ld", now);
#elif defined(BEOS) && defined(__POWERPC__)
    fprintf(stdout, "%Ld", now);  /* Metroworks on BeOS PPC */
#else
    fprintf(stdout, "%lld", now);
#endif

#elif defined(_WIN32)
    __int64 now;
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    CopyMemory(&now, &ft, sizeof(now));
    /*
     * 116444736000000000 is the number of 100-nanosecond intervals
     * between Jan. 1, 1601 and Jan. 1, 1970.
     */
#ifdef __GNUC__
    now = (now - 116444736000000000LL) / 10LL;
    fprintf(stdout, "%lld", now);
#else
    now = (now - 116444736000000000i64) / 10i64;
    fprintf(stdout, "%I64d", now);
#endif

#else
#error "Architecture not supported"
#endif

    return 0;
}  /* main */

/* now.c */
